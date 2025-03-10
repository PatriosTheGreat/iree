// Copyright 2023 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "iree/compiler/InputConversion/Common/PassDetail.h"
#include "iree/compiler/InputConversion/Common/Passes.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Tosa/IR/TosaOps.h"
#include "mlir/IR/BuiltinDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"

// Dialect specific
#ifdef IREE_HAVE_MHLO_INPUT
#include "iree/compiler/InputConversion/MHLO/Passes.h"
#include "iree/compiler/InputConversion/StableHLO/Passes.h"
#include "mhlo/IR/hlo_ops.h"
#include "stablehlo/dialect/StablehloOps.h"
#endif  // IREE_HAVE_MHLO_INPUT
#ifdef IREE_HAVE_TOSA_INPUT
#include "iree/compiler/InputConversion/TOSA/Passes.h"
#endif  // IREE_HAVE_TOSA_INPUT
#ifdef IREE_HAVE_TORCH_INPUT
#include "iree/compiler/InputConversion/TMTensor/Passes.h"
#include "torch-mlir-dialects/Dialect/TMTensor/IR/TMTensorDialect.h"
#endif  // IREE_HAVE_TORCH_INPUT

namespace mlir::iree_compiler {
namespace {
struct AutoInputConversionPipelinePass final
    : AutoInputConversionPipelineBase<AutoInputConversionPipelinePass> {
  void runOnOperation() override;
  void getDependentDialects(DialectRegistry& registry) const override;
};

// All the features seen that should be handled during input conversion.
struct InputFeatures {
  // HLO features.
  bool hasStableHLO = false;
  bool hasMHLO = false;
  // - XLA import features.
  bool hasTuples = false;

  // TOSA features.
  bool hasTOSA = false;

  // tm_tensor
  bool hasTmTensor = false;
};

static void populateHloFeatures(Operation* op, InputFeatures& features) {
  if (features.hasTuples) {
    return;
  }

  if (auto funcOp = dyn_cast<func::FuncOp>(op)) {
    FunctionType type = dyn_cast<FunctionType>(funcOp.getFunctionType());
    for (auto t : type.getResults()) {
      if (isa<TupleType>(t)) {
        features.hasTuples = true;
        return;
      }
    }
    for (auto t : type.getInputs()) {
      if (isa<TupleType>(t)) {
        features.hasTuples = true;
        return;
      }
    }
  }

  // Check for tuple operands or results.
  for (auto t : op->getOperandTypes()) {
    if (isa<TupleType>(t)) {
      features.hasTuples = true;
      return;
    }
  }
  for (auto t : op->getResultTypes()) {
    if (isa<TupleType>(t)) {
      features.hasTuples = true;
      return;
    }
  }
}

static void populateFeatures(Operation* op, const Dialect* stablehloDialect,
                             const Dialect* mhloDialect,
                             const Dialect* tmTensorDialect,
                             const Dialect* tosaDialect,
                             InputFeatures& features) {
  Dialect* d = op->getDialect();
  if (d == stablehloDialect) {
    features.hasStableHLO = true;
    return populateHloFeatures(op, features);
  }
  if (d == mhloDialect) {
    features.hasMHLO = true;
    return populateHloFeatures(op, features);
  }
  if (d == tosaDialect) {
    features.hasTOSA = true;
    return;
  }
  if (d == tmTensorDialect) {
    features.hasTmTensor = true;
    return;
  }
}

void AutoInputConversionPipelinePass::runOnOperation() {
  ModuleOp module = getOperation();
  MLIRContext* ctxt = &getContext();

  InputFeatures features;
  const Dialect* stablehloDialect = ctxt->getLoadedDialect("stablehlo");
  const Dialect* mhloDialect = ctxt->getLoadedDialect("mhlo");
  const Dialect* tosaDialect = ctxt->getLoadedDialect("tosa");
  const Dialect* tmTensorDialect = ctxt->getLoadedDialect("tm_tensor");
  if (!stablehloDialect && !mhloDialect && !tosaDialect && !tmTensorDialect) {
    return;
  }

  auto res = module.walk([&](Operation* op) {
    populateFeatures(op, stablehloDialect, mhloDialect, tmTensorDialect,
                     tosaDialect, features);
    bool hasAnyHLO = features.hasStableHLO || features.hasMHLO;
    if (hasAnyHLO && features.hasTOSA) {
      module.emitError("not yet implemented mixture of *HLO and TOSA");
      return WalkResult::interrupt();
    }
    if (hasAnyHLO && features.hasTmTensor) {
      module.emitError("not yet implemented mixture of *HLO and TM Tensor");
      return WalkResult::interrupt();
    }
    if (features.hasTOSA && features.hasTmTensor) {
      module.emitError("not yet implemented mixture of TOSA and TM Tensor");
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  if (res.wasInterrupted()) {
    return signalPassFailure();
  }
  if (!features.hasStableHLO && !features.hasMHLO && !features.hasTOSA &&
      !features.hasTmTensor) {
    return;
  }

  OpPassManager pm(ModuleOp::getOperationName(),
                   OpPassManager::Nesting::Explicit);
#ifdef IREE_HAVE_MHLO_INPUT
  if (features.hasStableHLO && !features.hasMHLO) {
    if (features.hasTuples) {
      stablehlo::buildStableHLOXLAInputConversionPassPipeline(pm);
    } else {
      stablehlo::buildStableHLOInputConversionPassPipeline(pm);
    }
  }
  if (features.hasMHLO) {
    if (features.hasTuples) {
      MHLO::buildXLAInputConversionPassPipeline(pm);
    } else {
      MHLO::buildMHLOInputConversionPassPipeline(pm);
    }
  }
#endif  // IREE_HAVE_MHLO_INPUT
#ifdef IREE_HAVE_TOSA_INPUT
  if (features.hasTOSA) {
    buildTOSAInputConversionPassPipeline(pm);
  }
#endif  // IREE_HAVE_TOSA_INPUT
#ifdef IREE_HAVE_TORCH_INPUT
  if (features.hasTmTensor) {
    pm.addNestedPass<func::FuncOp>(
        TMTensor::createConvertTMTensorToLinalgExtPass());
  }
#endif  // IREE_HAVE_TORCH_INPUT

  if (failed(runPipeline(pm, module))) {
    signalPassFailure();
  }
}

void AutoInputConversionPipelinePass::getDependentDialects(
    DialectRegistry& registry) const {
  // Register dialects from all possible pipelines, as we do not statically know
  // which pipeline will be selected, while dialect registration happens before
  // we run any detection on the input.
  //
  // TODO(kuhar): Find a better registration mechanism so that we do not have to
  // build pipelines just to query dialects and discard them immediately after.
  auto appendPipelineDialects =
      [&registry](function_ref<void(OpPassManager&)> buildFn) {
        OpPassManager pm;
        buildFn(pm);
        pm.getDependentDialects(registry);
      };

#ifdef IREE_HAVE_MHLO_INPUT
  appendPipelineDialects(stablehlo::buildStableHLOInputConversionPassPipeline);
  appendPipelineDialects(
      stablehlo::buildStableHLOXLAInputConversionPassPipeline);

  appendPipelineDialects(MHLO::buildMHLOInputConversionPassPipeline);
  appendPipelineDialects(MHLO::buildXLAInputConversionPassPipeline);
#endif  // IREE_HAVE_MHLO_INPUT

#ifdef IREE_HAVE_TOSA_INPUT
  appendPipelineDialects(buildTOSAInputConversionPassPipeline);
#endif  // IREE_HAVE_TOSA_INPUT

#ifdef IREE_HAVE_TORCH_INPUT
  appendPipelineDialects([](OpPassManager& pm) {
    pm.addNestedPass<func::FuncOp>(
        TMTensor::createConvertTMTensorToLinalgExtPass());
  });
#endif  // IREE_HAVE_TORCH_INPUT
}
}  // namespace

std::unique_ptr<OperationPass<ModuleOp>>
createAutoInputConversionPipelinePass() {
  return std::make_unique<AutoInputConversionPipelinePass>();
}

}  // namespace mlir::iree_compiler
