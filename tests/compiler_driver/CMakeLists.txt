################################################################################
# Autogenerated by build_tools/bazel_to_cmake/bazel_to_cmake.py from           #
# tests/compiler_driver/BUILD.bazel                                            #
#                                                                              #
# Use iree_cmake_extra_content from iree/build_defs.oss.bzl to add arbitrary   #
# CMake-only content.                                                          #
#                                                                              #
# To disable autogeneration for this file entirely, delete this header.        #
################################################################################

iree_add_all_subdirs()

iree_lit_test_suite(
  NAME
    lit
  SRCS
    "executable_benchmarks.mlir"
    "hal_executable.mlir"
    "inline_dynamic_hal_executable.mlir"
    "inline_static_hal_executable.mlir"
    "preprocessing_flags.mlir"
    "smoketest.mlir"
    "streams.mlir"
  TOOLS
    FileCheck
    iree-compile
)

### BAZEL_TO_CMAKE_PRESERVES_ALL_CONTENT_BELOW_THIS_LINE ###
