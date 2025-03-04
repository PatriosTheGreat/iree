# Copyright 2019 The IREE Authors
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Doesn't use bazel_to_cmake because of various special logic throughout.

# Enable compiler targets based on options.
set(IREE_COMPILER_TARGETS "")
set(IREE_COMPILER_TARGET_COPTS "")
if(IREE_TARGET_BACKEND_LLVM_CPU)
  list(APPEND IREE_COMPILER_TARGETS iree::compiler::Dialect::HAL::Target::LLVMCPU)
  list(APPEND IREE_COMPILER_TARGET_COPTS "-DIREE_HAVE_LLVM_CPU_TARGET")
endif()
if(IREE_TARGET_BACKEND_METAL_SPIRV)
  list(APPEND IREE_COMPILER_TARGETS iree::compiler::Dialect::HAL::Target::MetalSPIRV)
  list(APPEND IREE_COMPILER_TARGET_COPTS "-DIREE_HAVE_METALSPIRV_TARGET")
endif()
if(IREE_TARGET_BACKEND_VMVX)
  list(APPEND IREE_COMPILER_TARGETS iree::compiler::Dialect::HAL::Target::VMVX)
  list(APPEND IREE_COMPILER_TARGET_COPTS "-DIREE_HAVE_VMVX_TARGET")
endif()
if(IREE_TARGET_BACKEND_VULKAN_SPIRV)
  list(APPEND IREE_COMPILER_TARGETS iree::compiler::Dialect::HAL::Target::VulkanSPIRV)
  list(APPEND IREE_COMPILER_TARGET_COPTS "-DIREE_HAVE_VULKANSPIRV_TARGET")
endif()
if(IREE_TARGET_BACKEND_WEBGPU)
  list(APPEND IREE_COMPILER_TARGETS iree::compiler::Dialect::HAL::Target::WebGPU)
  list(APPEND IREE_COMPILER_TARGET_COPTS "-DIREE_HAVE_WEBGPU_TARGET")
endif()
if(IREE_TARGET_BACKEND_ROCM)
  list(APPEND IREE_COMPILER_TARGETS iree::compiler::Dialect::HAL::Target::ROCM)
  list(APPEND IREE_COMPILER_TARGET_COPTS "-DIREE_HAVE_ROCM_TARGET")
endif()

set(IREE_VERSION_TARGET_COPTS "")
if(IREE_EMBEDDED_RELEASE_INFO)
  list(APPEND IREE_VERSION_TARGET_COPTS "-DIREE_RELEASE_REVISION=\"${IREE_RELEASE_REVISION}\"")
  list(APPEND IREE_VERSION_TARGET_COPTS "-DIREE_RELEASE_VERSION=\"${IREE_RELEASE_VERSION}\"")
endif()

# Enable input dialects based on options.
set(IREE_INPUT_DEPS "")
if(IREE_INPUT_MHLO)
  list(APPEND IREE_INPUT_DEPS iree::compiler::InputConversion::MHLO)
  list(APPEND IREE_INPUT_DEPS iree::compiler::InputConversion::StableHLO)
  list(APPEND IREE_INPUT_DEPS tensorflow::external_mhlo_includes)
  list(APPEND IREE_INPUT_DEPS ChloOps)
  list(APPEND IREE_INPUT_DEPS MhloDialect)
  list(APPEND IREE_INPUT_DEPS StablehloOps)
endif()
if(IREE_INPUT_TORCH)
  list(APPEND IREE_INPUT_DEPS iree::compiler::InputConversion::TMTensor)
  list(APPEND IREE_INPUT_DEPS TorchMLIRTMTensorDialect)
endif()
if(IREE_INPUT_TOSA)
  list(APPEND IREE_INPUT_DEPS iree::compiler::InputConversion::TOSA)
  list(APPEND IREE_INPUT_DEPS MLIRTosaDialect)
  list(APPEND IREE_INPUT_DEPS MLIRTosaTransforms)
endif()

iree_cc_library(
  NAME
    init_input_passes_and_dialects
  HDRS
    "init_input_dialects.h"
    "init_input_passes.h"
  SRCS
    "init_input_dialects.cc"
    "init_input_passes.cc"
  DEPS
    ${IREE_INPUT_DEPS}
    iree::compiler::InputConversion::Common
  PUBLIC
)

iree_cc_library(
  NAME
    init_iree_passes_and_dialects
  HDRS
    "init_iree_dialects.h"
    "init_iree_passes.h"
  DEPS
    IREEInputDialect
    IREELinalgExtDialect
    IREELinalgExtTransformOps
    IREELinalgExtTransforms
    IREELinalgTransformDialect
    IREELinalgTransformDialectPasses
    MLIRIR
    iree::compiler::Bindings::Native::Transforms
    iree::compiler::Bindings::TFLite::Transforms
    iree::compiler::Codegen::Dialect::IREECodegenDialect
    iree::compiler::Codegen::Interfaces::Interfaces
    iree::compiler::ConstEval
    iree::compiler::Dialect::Flow::IR
    iree::compiler::Dialect::Flow::Transforms
    iree::compiler::Dialect::HAL::IR::HALDialect
    iree::compiler::Dialect::HAL::Transforms
    iree::compiler::Dialect::Stream::IR
    iree::compiler::Dialect::Stream::Transforms
    iree::compiler::Dialect::Util::IR
    iree::compiler::Dialect::Util::IR::ExternalModels
    iree::compiler::Dialect::Util::Transforms
    iree::compiler::Dialect::VM::Analysis
    iree::compiler::Dialect::VM::IR
    iree::compiler::Dialect::VM::Target::init_targets
    iree::compiler::Dialect::VM::Transforms
    iree::compiler::Dialect::VMVX::IR::VMVXDialect
    iree::compiler::Dialect::VMVX::Transforms
    iree::compiler::Dialect::Vulkan::IR
    iree::compiler::Modules::HAL::Inline::IR::HALInlineDialect
    iree::compiler::Modules::HAL::Inline::Transforms
    iree::compiler::Modules::HAL::Loader::IR::HALLoaderDialect
    iree::compiler::Modules::HAL::Loader::Transforms
    iree::compiler::Pipelines
    iree::compiler::Preprocessing::Passes
  PUBLIC
)

iree_cc_library(
  NAME
    init_mlir_passes_and_dialects
  HDRS
    "init_mlir_dialects.h"
    "init_mlir_passes.h"
  DEPS
    MLIRAffineDialect
    MLIRAffineTransforms
    MLIRArmNeonDialect
    MLIRArmNeon2dToIntr
    MLIRBufferizationDialect
    MLIRComplexDialect
    MLIRControlFlowDialect
    MLIRGPUDialect
    MLIRGPUToSPIRV
    MLIRIR
    MLIRLLVMDialect
    MLIRLinalgDialect
    MLIRLinalgToLLVM
    MLIRLinalgTransforms
    MLIRMLProgramDialect
    MLIRQuantDialect
    MLIRSCFDialect
    MLIRSCFToGPU
    MLIRSCFTransforms
    MLIRSPIRVDialect
    MLIRSPIRVTransforms
    MLIRShapeDialect
    MLIRFuncDialect
    MLIRFuncToSPIRV
    MLIRTensorInferTypeOpInterfaceImpl
    MLIRTransforms
    MLIRVectorDialect
    iree::compiler::Dialect::VM::Target::init_targets
  PUBLIC
)

iree_cc_library(
  NAME
    init_passes_and_dialects
  HDRS
    "init_dialects.h"
    "init_passes.h"
  DEPS
    ::init_compiler_modules
    ::init_input_passes_and_dialects
    ::init_iree_passes_and_dialects
    ::init_mlir_passes_and_dialects
    iree::compiler::Codegen::Codegen
    iree::compiler::Dialect::HAL::Conversion::Passes
  PUBLIC
)

iree_cc_library(
  NAME
    init_llvmir_translations
  HDRS
    "init_llvmir_translations.h"
  DEPS
    MLIRArmNeonToLLVMIRTranslation
    MLIRLLVMToLLVMIRTranslation
  PUBLIC
)

iree_cc_library(
  NAME
    init_compiler_modules
  HDRS
    "init_compiler_modules.h"
  DEPS
    iree::compiler::Modules::Check::IR::CheckDialect
)

iree_cc_library(
  NAME
    init_targets
  HDRS
    "init_targets.h"
  SRCS
    "init_targets.cc"
  DEPS
    ${IREE_COMPILER_TARGETS}
  COPTS
    ${IREE_COMPILER_TARGET_COPTS}
  PUBLIC
)

iree_cc_library(
  NAME
    iree_compile_lib
  HDRS
    "iree_compile_lib.h"
  SRCS
    "iree_compile_lib.cc"
  DEPS
    LLVMSupport
    iree::compiler::API::Internal::Embed
    iree::compiler::Pipelines
    iree::compiler::bindings::c::headers
  PUBLIC
)

iree_cc_library(
  NAME
    version
  HDRS
    "version.h"
  SRCS
    "version.cc"
  COPTS
    ${IREE_VERSION_TARGET_COPTS}
)

