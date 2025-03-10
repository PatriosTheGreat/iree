################################################################################
# Autogenerated by build_tools/bazel_to_cmake/bazel_to_cmake.py from           #
# compiler/src/iree/compiler/InputConversion/TMTensor/BUILD.bazel              #
#                                                                              #
# Use iree_cmake_extra_content from iree/build_defs.oss.bzl to add arbitrary   #
# CMake-only content.                                                          #
#                                                                              #
# To disable autogeneration for this file entirely, delete this header.        #
################################################################################

iree_add_all_subdirs()

iree_tablegen_library(
  NAME
    PassesIncGen
  TD_FILE
    "Passes.td"
  OUTS
    --gen-pass-decls Passes.h.inc
)

iree_cc_library(
  NAME
    PassHeaders
  HDRS
    "PassDetail.h"
    "Passes.h"
    "Passes.h.inc"
  DEPS
    ::PassesIncGen
    MLIRPass
    MLIRTransforms
  PUBLIC
)

iree_cc_library(
  NAME
    TMTensor
  HDRS
    "Passes.h"
  SRCS
    "ConvertTMTensorToLinalgExt.cpp"
    "Passes.cpp"
  DEPS
    ::PassHeaders
    ::PassesIncGen
    IREELinalgExtDialect
    MLIRFuncDialect
    MLIRIR
    MLIRPass
    MLIRTransforms
    TorchMLIRTMTensorDialect
  DEFINES
    "IREE_HAVE_TORCH_INPUT"
  PUBLIC
)

### BAZEL_TO_CMAKE_PRESERVES_ALL_CONTENT_BELOW_THIS_LINE ###
