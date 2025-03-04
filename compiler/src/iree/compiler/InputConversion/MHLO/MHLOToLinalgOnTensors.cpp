// Copyright 2020 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

//===- XLAToLinalgOnTensors.cpp - Pass to convert XLA to Linalg on tensors-===//
//
// Pass to convert from XLA to linalg on tensers. Uses the patterns from
// tensorflow/compiler/mlir/xla/transforms/legalize_to_linalg.cc along with
// some IREE specific patterns.
//
//===----------------------------------------------------------------------===//
#include <memory>

#include "iree-dialects/Dialect/LinalgExt/IR/LinalgExtDialect.h"
#include "iree/compiler/Dialect/Flow/IR/FlowOps.h"
#include "iree/compiler/Dialect/Util/IR/UtilDialect.h"
#include "iree/compiler/Dialect/Util/IR/UtilOps.h"
#include "iree/compiler/InputConversion/MHLO/ConvertMHLOToFlow.h"
#include "iree/compiler/InputConversion/MHLO/PassDetail.h"
#include "iree/compiler/InputConversion/MHLO/Passes.h"
#include "iree/compiler/InputConversion/MHLO/Rewriters.h"
#include "iree/compiler/Utils/ConversionUtils.h"
#include "mhlo/IR/hlo_ops.h"
#include "mhlo/transforms/rewriters.h"
#include "mhlo/utils/legalize_to_linalg_utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Complex/IR/Complex.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/Linalg/Transforms/Transforms.h"
#include "mlir/Dialect/MLProgram/IR/MLProgram.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Matchers.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "mlir/Transforms/Passes.h"
#include "stablehlo/dialect/ChloOps.h"

namespace mlir {
namespace iree_compiler {
namespace MHLO {

//===----------------------------------------------------------------------===//
// mhlo.concatenate conversion patterns.
//===----------------------------------------------------------------------===//

namespace {
/// Converts mhlo.concatenate operation to extract_slice ops + insert_slice ops.
struct ConcatenateOpConversion
    : public OpConversionPattern<mhlo::ConcatenateOp> {
  using OpConversionPattern<mhlo::ConcatenateOp>::OpConversionPattern;

  LogicalResult matchAndRewrite(
      mhlo::ConcatenateOp op, OpAdaptor adaptor,
      ConversionPatternRewriter &rewriter) const override {
    auto resultType = llvm::dyn_cast<RankedTensorType>(
        this->typeConverter->convertType(op.getResult().getType()));
    if (!resultType || !resultType.hasStaticShape()) {
      return rewriter.notifyMatchFailure(op,
                                         "expected static shape for output");
    }

    Location loc = op.getLoc();
    int dim = op.getDimension();
    int rank = resultType.getRank();
    SmallVector<Value, 3> offsets, sizes, strides;
    for (int i = 0; i < rank; ++i) {
      offsets.push_back(rewriter.create<arith::ConstantIndexOp>(loc, 0));
      sizes.push_back(rewriter.createOrFold<tensor::DimOp>(
          loc, adaptor.getOperands()[0], i));
      strides.push_back(rewriter.create<arith::ConstantIndexOp>(loc, 1));
    }
    Value resultDimSize = rewriter.create<arith::ConstantIndexOp>(loc, 0);
    for (auto arg : adaptor.getOperands()) {
      auto size = rewriter.createOrFold<tensor::DimOp>(loc, arg, dim);
      resultDimSize =
          rewriter.createOrFold<arith::AddIOp>(loc, resultDimSize, size);
    }
    sizes[dim] = resultDimSize;
    Value result = rewriter.create<tensor::EmptyOp>(
        loc, resultType.getShape(), resultType.getElementType());

    auto toOpFoldResult = [](Value v) -> OpFoldResult {
      auto op = v.getDefiningOp<arith::ConstantIndexOp>();
      if (!op) return v;
      return op.getValue();
    };

    Value accBound = rewriter.create<arith::ConstantIndexOp>(loc, 0);
    for (auto arg : adaptor.getOperands()) {
      offsets[dim] = accBound;
      sizes[dim] = rewriter.createOrFold<tensor::DimOp>(loc, arg, dim);
      result = rewriter.create<tensor::InsertSliceOp>(
          loc, arg, result, llvm::map_to_vector(offsets, toOpFoldResult),
          llvm::map_to_vector(sizes, toOpFoldResult),
          llvm::map_to_vector(strides, toOpFoldResult));
      accBound = rewriter.create<arith::AddIOp>(loc, accBound, sizes[dim]);
    }
    rewriter.replaceOp(op, result);
    return success();
  }
};

//===----------------------------------------------------------------------===//
// mhlo.fft conversion patterns.
//===----------------------------------------------------------------------===//

/// Creats coefficients based on DFT definition, see
/// https://en.wikipedia.org/wiki/Discrete_Fourier_transform
Value getDFTMatmulCoeff(OpBuilder b, Location loc, RankedTensorType matrixType,
                        bool isRealPart) {
  // scale = 2 * pi / N
  double scale = 2 * M_PI / matrixType.getDimSize(0);

  SmallVector<Attribute> values;
  assert(matrixType.getRank() == 2 && "expected 2D matrix");
  for (auto i : llvm::seq<unsigned>(0, matrixType.getDimSize(0))) {
    for (auto j : llvm::seq<unsigned>(0, matrixType.getDimSize(1))) {
      double v = scale * i * j;
      if (isRealPart) {
        v = cos(v);
      } else {
        v = -sin(v);
      }
      values.push_back(b.getF32FloatAttr(v));
    }
  }
  return b.create<arith::ConstantOp>(
      loc, matrixType, DenseFPElementsAttr::get(matrixType, values));
}

Value createLinalgMatmulOnTensors(OpBuilder b, Location loc,
                                  RankedTensorType resultType, Value lhs,
                                  Value rhs) {
  Value zero = b.create<arith::ConstantOp>(
      loc, b.getZeroAttr(resultType.getElementType()));
  Value emptyTensor = b.create<mlir::tensor::EmptyOp>(
      loc, resultType.getShape(), resultType.getElementType(),
      /*dyn_size=*/ValueRange{});
  Value zeroTensor =
      b.create<linalg::FillOp>(loc, zero, emptyTensor).getResult(0);

  switch (llvm::cast<RankedTensorType>(lhs.getType()).getRank()) {
    case 1:
      return b
          .create<linalg::VecmatOp>(loc, TypeRange{resultType},
                                    ValueRange{lhs, rhs},
                                    ValueRange{zeroTensor})
          .getResult(0);
    case 2:
      return b
          .create<linalg::MatmulOp>(loc, TypeRange{resultType},
                                    ValueRange{lhs, rhs},
                                    ValueRange{zeroTensor})
          .getResult(0);
    default:
      assert(false && "unhandled matmul type");
      return Value();
  }
}

/// Converts mhlo.fft operation to Linalg ops.
struct FftOpConversion : public OpConversionPattern<mhlo::FftOp> {
  using OpConversionPattern<mhlo::FftOp>::OpConversionPattern;

  LogicalResult matchAndRewrite(
      mhlo::FftOp op, OpAdaptor adaptor,
      ConversionPatternRewriter &rewriter) const override {
    if (op.getFftType() != mhlo::FftType::RFFT) {
      return rewriter.notifyMatchFailure(op,
                                         "non RFFT types are supported yet");
    }

    auto inputType =
        llvm::dyn_cast<RankedTensorType>(adaptor.getOperand().getType());
    if (!inputType || !inputType.hasStaticShape() || inputType.getRank() > 2) {
      return rewriter.notifyMatchFailure(op, "only static 1D or 2D dft ops");
    }

    int rank = inputType.getRank();
    int n = inputType.getDimSize(rank - 1);
    int fftLength =
        op.getFftLength().getSplatValue<IntegerAttr>().getInt() / 2 + 1;

    Location loc = op.getLoc();
    auto matrixType =
        RankedTensorType::get({n, fftLength}, inputType.getElementType());
    auto resultType = RankedTensorType::get(
        llvm::cast<RankedTensorType>(op.getType()).getShape(),
        inputType.getElementType());

    auto realMatrix =
        getDFTMatmulCoeff(rewriter, loc, matrixType, /*isRealPart=*/true);
    auto real = createLinalgMatmulOnTensors(rewriter, loc, resultType,
                                            adaptor.getOperand(), realMatrix);

    auto imagMatrix =
        getDFTMatmulCoeff(rewriter, loc, matrixType, /*isRealPart=*/false);
    auto imag = createLinalgMatmulOnTensors(rewriter, loc, resultType,
                                            adaptor.getOperand(), imagMatrix);

    // Pack the results back to mhlo::ComplexOp.
    rewriter.replaceOpWithNewOp<mhlo::ComplexOp>(op, op.getType(), real, imag);
    return success();
  }
};

struct OptimizationBarrierOpConversion
    : public OpConversionPattern<mhlo::OptimizationBarrierOp> {
  using OpConversionPattern<mhlo::OptimizationBarrierOp>::OpConversionPattern;

  LogicalResult matchAndRewrite(
      mhlo::OptimizationBarrierOp op, OpAdaptor adaptor,
      ConversionPatternRewriter &rewriter) const override {
    SmallVector<Value> outputs;
    for (auto operand : adaptor.getOperands()) {
      outputs.push_back(
          rewriter
              .create<IREE::Util::OptimizationBarrierOp>(op.getLoc(), operand)
              .getResult(0));
    }
    rewriter.replaceOp(op, outputs);
    return success();
  }
};

// Returns true if all attributes in the given dictionary are valid for IREE
// input dialects.
static bool isValidFuncAttr(DictionaryAttr attrs) {
  // TODO: switch to using a dialect-based exclusion list or some other way that
  // is not a big string table.
  for (auto attr : attrs) {
    if (attr.getName() == "tf.aliasing_output") return false;
  }
  return true;
}

// Adds iree.abi.encoding attributes for arguments and results when they have
// had their type changed during conversion.
static void setFuncEncodings(func::FuncOp funcOp, FunctionType oldFuncType,
                             FunctionType newFuncType) {
  auto encodingName = StringAttr::get(funcOp.getContext(), "iree.abi.encoding");
  for (auto [i, oldType, newType] :
       llvm::enumerate(oldFuncType.getInputs(), newFuncType.getInputs())) {
    if (oldType != newType)
      funcOp.setArgAttr(i, encodingName, TypeAttr::get(oldType));
  }
  for (auto [i, oldType, newType] :
       llvm::enumerate(oldFuncType.getResults(), newFuncType.getResults())) {
    if (oldType != newType)
      funcOp.setResultAttr(i, encodingName, TypeAttr::get(oldType));
  }
}

// Rewrites attributes on the function from ones coming from HLO-based frontends
// to the IREE supported versions.
static void rewriteFuncAttrs(func::FuncOp funcOp) {
  auto *context = funcOp.getContext();
  auto indexType = IndexType::get(context);
  auto abiOutputName = StringAttr::get(context, "iree.abi.output");
  auto aliasingOutputName = StringAttr::get(context, "tf.aliasing_output");
  auto rewriteAttrs = [&](DictionaryAttr &allAttrs) {
    SmallVector<NamedAttribute> newAttrs;
    newAttrs.reserve(allAttrs.size());
    for (auto attr : allAttrs) {
      if (attr.getName() == aliasingOutputName) {
        newAttrs.push_back({
            abiOutputName,
            IntegerAttr::get(indexType,
                             llvm::cast<IntegerAttr>(attr.getValue()).getInt()),
        });
      } else {
        newAttrs.push_back(attr);
      }
    }
    allAttrs = DictionaryAttr::get(context, newAttrs);
  };
  SmallVector<DictionaryAttr> argAttrs;
  funcOp.getAllArgAttrs(argAttrs);
  llvm::for_each(argAttrs, rewriteAttrs);
  funcOp.setAllArgAttrs(argAttrs);
  SmallVector<DictionaryAttr> resultAttrs;
  funcOp.getAllResultAttrs(resultAttrs);
  llvm::for_each(resultAttrs, rewriteAttrs);
  funcOp.setAllResultAttrs(resultAttrs);
}

// We need to convert func ops in order to convert types.
class BuiltinFuncOpPattern : public OpConversionPattern<func::FuncOp> {
  using OpConversionPattern<func::FuncOp>::OpConversionPattern;
  LogicalResult matchAndRewrite(
      func::FuncOp srcOp, OpAdaptor adaptor,
      ConversionPatternRewriter &rewriter) const override {
    FunctionType srcFuncType = srcOp.getFunctionType();
    TypeConverter::SignatureConversion signatureConversion(
        srcOp.getNumArguments());

    // Convert function arguments.
    for (unsigned i = 0, e = srcFuncType.getNumInputs(); i < e; ++i) {
      if (failed(getTypeConverter()->convertSignatureArg(
              i, srcFuncType.getInput(i), signatureConversion))) {
        return rewriter.notifyMatchFailure(srcOp, "argument failed to convert");
      }
    }

    // Convert function results.
    SmallVector<Type> convertedResultTypes;
    if (failed(getTypeConverter()->convertTypes(srcFuncType.getResults(),
                                                convertedResultTypes))) {
      return rewriter.notifyMatchFailure(srcOp, "results failed to convert");
    }

    // Create new function with converted argument and result types.
    auto oldFuncType = srcOp.getFunctionType();
    auto newFuncType = mlir::FunctionType::get(
        srcOp.getContext(), signatureConversion.getConvertedTypes(),
        convertedResultTypes);

    // Update the function in place.
    rewriter.startRootUpdate(srcOp);
    srcOp.setType(newFuncType);
    rewriteFuncAttrs(srcOp);
    setFuncEncodings(srcOp, oldFuncType, newFuncType);

    // Tell the rewriter to convert the region signature.
    TypeConverter &typeConverter = *getTypeConverter();
    if (failed(rewriter.convertRegionTypes(&srcOp.getBody(), typeConverter,
                                           &signatureConversion))) {
      return failure();
    }

    rewriter.finalizeRootUpdate(srcOp);
    return success();
  }
};

class GlobalOpPattern : public OpConversionPattern<ml_program::GlobalOp> {
 public:
  using OpConversionPattern<ml_program::GlobalOp>::OpConversionPattern;
  LogicalResult matchAndRewrite(
      ml_program::GlobalOp globalOp, OpAdaptor adaptor,
      ConversionPatternRewriter &rewriter) const override {
    auto oldType = globalOp.getType();
    auto newType = getTypeConverter()->convertType(oldType);
    if (newType == oldType) return failure();
    if (!newType) {
      return rewriter.notifyMatchFailure(globalOp,
                                         "result type conversion failed");
    }
    rewriter.updateRootInPlace(globalOp, [&]() {
      globalOp.setType(newType);
      if (auto oldValue = globalOp.getValueAttr()) {
        globalOp.setValueAttr(
            convertAttribute(globalOp.getLoc(), oldValue, *getTypeConverter()));
      }
    });
    return success();
  }
};

class GenericTypeConvert : public ConversionPattern {
 public:
  GenericTypeConvert(StringRef rootName, TypeConverter &converter,
                     MLIRContext *context, PatternBenefit benefit = 0)
      : ConversionPattern(converter, rootName, benefit, context) {}
  LogicalResult matchAndRewrite(
      Operation *op, ArrayRef<Value> operands,
      ConversionPatternRewriter &rewriter) const override {
    llvm::SmallVector<NamedAttribute, 4> newAttr;
    llvm::append_range(newAttr, op->getAttrs());
    llvm::SmallVector<Type, 4> newResults;
    if (failed(getTypeConverter()->convertTypes(op->getResultTypes(),
                                                newResults))) {
      return rewriter.notifyMatchFailure(op, "result type conversion failed");
    }
    OperationState state(op->getLoc(), op->getName().getStringRef(), operands,
                         newResults, newAttr, op->getSuccessors());
    for (Region &r : op->getRegions()) {
      Region *newRegion = state.addRegion();
      rewriter.inlineRegionBefore(r, *newRegion, newRegion->begin());
      TypeConverter::SignatureConversion result(newRegion->getNumArguments());
      if (failed(getTypeConverter()->convertSignatureArgs(
              newRegion->getArgumentTypes(), result))) {
        return rewriter.notifyMatchFailure(op,
                                           "argument type conversion failed");
      }
      rewriter.applySignatureConversion(newRegion, result);
    }
    Operation *newOp = rewriter.create(state);
    rewriter.replaceOp(op, newOp->getResults());
    return success();
  }
};

std::optional<Value> scalarToTensor(OpBuilder &builder, Type /*type*/,
                                    ValueRange inputs, Location loc) {
  assert(inputs.size() == 1);
  if (llvm::isa<ShapedType>(inputs.front().getType())) {
    return std::nullopt;
  }
  return builder
      .create<tensor::FromElementsOp>(
          loc, RankedTensorType::get({}, inputs.front().getType()),
          inputs.front())
      .getResult();
}

std::optional<Value> materializeCastFromIllegal(OpBuilder &builder, Type type,
                                                ValueRange inputs,
                                                Location loc) {
  Type fromType = getElementTypeOrSelf(inputs[0].getType());
  Type toType = getElementTypeOrSelf(type);
  if ((!fromType.isSignedInteger() && !fromType.isUnsignedInteger()) ||
      !toType.isSignlessInteger())
    return std::nullopt;
  // Use bitcast to do signless->signful conversions.
  return builder.create<tensor::BitcastOp>(loc, type, inputs[0])->getResult(0);
}

std::optional<Value> materializeCastToIllegal(OpBuilder &builder, Type type,
                                              ValueRange inputs, Location loc) {
  Type fromType = getElementTypeOrSelf(inputs[0].getType());
  Type toType = getElementTypeOrSelf(type);
  if (!fromType.isSignlessInteger() ||
      (!toType.isSignedInteger() && !toType.isUnsignedInteger()))
    return std::nullopt;
  // Use bitcast to do signless->signful conversions.
  return builder.create<tensor::BitcastOp>(loc, type, inputs[0])->getResult(0);
}

struct ConvertMHLOToLinalgOnTensorsPass
    : public ConvertMHLOToLinalgOnTensorsBase<
          ConvertMHLOToLinalgOnTensorsPass> {
  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<
        IREE::Flow::FlowDialect, IREE::Util::UtilDialect, linalg::LinalgDialect,
        mhlo::MhloDialect, shape::ShapeDialect, tensor::TensorDialect,
        math::MathDialect, memref::MemRefDialect, complex::ComplexDialect>();
  }

  void runOnOperation() override {
    RewritePatternSet patterns(&getContext());
    MLIRContext *context = &getContext();

    auto typeConverter = mhlo::createHloToLinalgTypeConverter();
    typeConverter->addArgumentMaterialization(scalarToTensor);
    typeConverter->addArgumentMaterialization(materializeCastFromIllegal);
    typeConverter->addTargetMaterialization(materializeCastFromIllegal);
    typeConverter->addSourceMaterialization(materializeCastToIllegal);
    // NOTE: not using corresponding setupMHLOToFlowPatterns because the entire
    // MHLO dialects are marked illegal by this pass.
    // TODO: Collapse/rework all of these patterns once the consolidation
    // lands. There is little reason to have these so spread out.
    populateMHLOToFlowPatterns(context, patterns);

    chlo::populateDecomposeChloPatterns(context, &patterns);
    populateMHLOBroadcastingToLinalgPatterns(context, *typeConverter, patterns);
    mhlo::populateScalarHloToArithmeticConversionPatterns(
        context, *typeConverter, &patterns,
        [](Operation *op) { return mhlo::isInBodyOfLinalgOps(op); });
    populateMHLOToLinalgOnTensorsConversionPatterns(context, *typeConverter,
                                                    patterns);
    populateMHLOComplexToRealPatterns(context, *typeConverter, patterns);

    populateMHLOCollectiveOpsConversionPatterns(context, *typeConverter,
                                                patterns);
    // TODO(*): expose patterns that do this much better from
    // iree/compiler/Dialect/Util/Transforms/ConvertPrimitiveType.cpp

    // Structural patterns (functions, cfg, terminators).
    patterns.insert<BuiltinFuncOpPattern>(*typeConverter, context);
    patterns.insert<GenericTypeConvert>(func::ReturnOp::getOperationName(),
                                        *typeConverter, context);
    patterns.insert<GenericTypeConvert>(func::CallOp::getOperationName(),
                                        *typeConverter, context);
    patterns.insert<GenericTypeConvert>(cf::CondBranchOp::getOperationName(),
                                        *typeConverter, context);
    patterns.insert<GenericTypeConvert>(cf::BranchOp::getOperationName(),
                                        *typeConverter, context);
    patterns.insert<GlobalOpPattern>(*typeConverter, context);
    patterns.insert<GenericTypeConvert>(
        ml_program::GlobalLoadOp::getOperationName(), *typeConverter, context);
    patterns.insert<GenericTypeConvert>(
        ml_program::GlobalLoadConstOp::getOperationName(), *typeConverter,
        context);
    patterns.insert<GenericTypeConvert>(
        ml_program::GlobalStoreOp::getOperationName(), *typeConverter, context);
    // This is needed when converting mhlo::ReplicaIDOp.
    patterns.insert<GenericTypeConvert>(
        tensor::FromElementsOp::getOperationName(), *typeConverter, context);
    patterns.insert<GenericTypeConvert>(
        arith::IndexCastUIOp::getOperationName(), *typeConverter, context);
    ConversionTarget target(getContext());

    auto isIllegalType = [&](Type t) { return !typeConverter->isLegal(t); };
    auto isLegallyTypedOp = [&](Operation *op) -> bool {
      for (Type type : op->getResultTypes()) {
        if (isIllegalType(type)) return false;
      }
      for (Type type : op->getOperandTypes()) {
        if (isIllegalType(type)) return false;
      }
      return true;
    };

    target.addIllegalDialect<chlo::ChloDialect>();
    target.addIllegalDialect<mhlo::MhloDialect>();

    // Functions must have legal types.
    target.addDynamicallyLegalOp<func::FuncOp>([&](func::FuncOp funcOp) {
      if (auto attrs = funcOp.getAllArgAttrs()) {
        if (!llvm::all_of(attrs.getAsRange<DictionaryAttr>(),
                          isValidFuncAttr)) {
          return false;
        }
      }
      if (auto attrs = funcOp.getAllResultAttrs()) {
        if (!llvm::all_of(attrs.getAsRange<DictionaryAttr>(),
                          isValidFuncAttr)) {
          return false;
        }
      }
      for (Type type : funcOp.getFunctionType().getInputs()) {
        if (isIllegalType(type)) return false;
      }
      for (Type type : funcOp.getFunctionType().getResults()) {
        if (isIllegalType(type)) return false;
      }
      for (Block &block : funcOp.getFunctionBody()) {
        for (Type type : block.getArgumentTypes()) {
          if (isIllegalType(type)) return false;
        }
      }
      return true;
    });
    target.addDynamicallyLegalOp<func::ReturnOp>([&](func::ReturnOp op) {
      return llvm::all_of(op.getOperandTypes(),
                          [&](Type type) { return !isIllegalType(type); });
    });
    target.addDynamicallyLegalOp<func::CallOp>([&](func::CallOp op) {
      return llvm::all_of(op.getOperandTypes(),
                          [&](Type type) { return !isIllegalType(type); });
    });
    target.addDynamicallyLegalOp<cf::CondBranchOp>([&](cf::CondBranchOp op) {
      return llvm::all_of(op.getOperandTypes(),
                          [&](Type type) { return !isIllegalType(type); });
    });
    target.addDynamicallyLegalOp<cf::BranchOp>([&](cf::BranchOp op) {
      return llvm::all_of(op.getOperandTypes(),
                          [&](Type type) { return !isIllegalType(type); });
    });
    target.addDynamicallyLegalOp<ml_program::GlobalOp>(
        [&](ml_program::GlobalOp op) {
          return typeConverter->isLegal(op.getType());
        });

    // Let the rest fall through.
    target.addLegalDialect<BuiltinDialect>();
    target.addLegalDialect<IREE::LinalgExt::IREELinalgExtDialect>();
    target.addLegalOp<tensor::BitcastOp>();
    target.markUnknownOpDynamicallyLegal(isLegallyTypedOp);

    if (failed(applyPartialConversion(getOperation(), target,
                                      std::move(patterns)))) {
      return signalPassFailure();
    }

    {
      // Apply the patterns to remove unused operands and results.
      RewritePatternSet removeUnusedOperandsResultsPatterns(&getContext());
      linalg::populateEraseUnusedOperandsAndResultsPatterns(
          removeUnusedOperandsResultsPatterns);
      if (failed(applyPatternsAndFoldGreedily(
              getOperation(),
              std::move(removeUnusedOperandsResultsPatterns)))) {
        return signalPassFailure();
      }
    }
  }
};

}  // namespace

void populateMHLOToLinalgOnTensorsConversionPatterns(
    MLIRContext *context, TypeConverter &typeConverter,
    RewritePatternSet &patterns) {
  mhlo::populateHloToLinalgConversionPattern(context, typeConverter, &patterns);
  // TODO(#5809): Drop ConcatenateOp lowering in favor of the upstream version
  //              then remove the PatternBenefit here
  patterns.insert<ConcatenateOpConversion, FftOpConversion,
                  OptimizationBarrierOpConversion>(typeConverter, context,
                                                   PatternBenefit(1000));
}

std::unique_ptr<OperationPass<ModuleOp>> createMHLOToLinalgOnTensorsPass() {
  return std::make_unique<ConvertMHLOToLinalgOnTensorsPass>();
}

}  // namespace MHLO
}  // namespace iree_compiler
}  // namespace mlir
