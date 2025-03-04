################################################################################
# Autogenerated by build_tools/bazel_to_cmake/bazel_to_cmake.py from           #
# runtime/src/iree/builtins/ukernel/arch/arm_64/BUILD.bazel                    #
#                                                                              #
# Use iree_cmake_extra_content from iree/build_defs.oss.bzl to add arbitrary   #
# CMake-only content.                                                          #
#                                                                              #
# To disable autogeneration for this file entirely, delete this header.        #
################################################################################

iree_add_all_subdirs()

if(IREE_BUILD_COMPILER AND IREE_TARGET_BACKEND_LLVM_CPU)

iree_bitcode_library(
  NAME
    ukernel_bitcode_arm_64_base
  ARCH
    arm_64
  SRCS
    "mmt4d_arm_64.c"
    "pack_arm_64.c"
    "query_tile_sizes_arm_64.c"
    "unpack_arm_64.c"
)

iree_bitcode_library(
  NAME
    ukernel_bitcode_arm_64_dotprod
  ARCH
    arm_64
  SRCS
    "mmt4d_arm_64_dotprod.c"
  COPTS
    "-march=armv8.2-a+dotprod"
)

iree_bitcode_library(
  NAME
    ukernel_bitcode_arm_64_i8mm
  ARCH
    arm_64
  SRCS
    "mmt4d_arm_64_i8mm.c"
  COPTS
    "-march=armv8.2-a+i8mm"
)

iree_link_bitcode(
  NAME
    ukernel_bitcode_arm_64
  SRCS
    "ukernel_bitcode_arm_64_base.bc"
    "ukernel_bitcode_arm_64_dotprod.bc"
    "ukernel_bitcode_arm_64_i8mm.bc"

)

endif()  # IREE_BUILD_COMPILER AND IREE_TARGET_BACKEND_LLVM_CPU

### BAZEL_TO_CMAKE_PRESERVES_ALL_CONTENT_BELOW_THIS_LINE ###

if (NOT (IREE_ARCH STREQUAL "arm_64"))
  return()
endif()

iree_select_compiler_opts(IREE_UK_COPTS_ARM_64_DOTPROD
  CLANG_OR_GCC
    "-march=armv8.2-a+dotprod"
)

iree_select_compiler_opts(IREE_UK_COPTS_ARM_64_I8MM
  CLANG_OR_GCC
    "-march=armv8.2-a+i8mm"
)

iree_cc_library(
  NAME
    common_arm_64
  HDRS
    "common_arm_64.h"
  DEPS
    iree::builtins::ukernel::internal_headers
    iree::schemas::cpu_data
)

iree_cc_library(
  NAME
    arm_64_dotprod
  SRCS
    "mmt4d_arm_64_dotprod.c"
  COPTS
    "${IREE_UK_COPTS_ARM_64_DOTPROD}"
  DEPS
    iree::builtins::ukernel::internal_headers
)

iree_cc_library(
  NAME
    arm_64_i8mm
  SRCS
    "mmt4d_arm_64_i8mm.c"
  COPTS
    "${IREE_UK_COPTS_ARM_64_I8MM}"
  DEPS
    iree::builtins::ukernel::internal_headers
)

iree_cc_library(
  NAME
    arm_64
  SRCS
    "mmt4d_arm_64.c"
    "pack_arm_64.c"
    "query_tile_sizes_arm_64.c"
    "unpack_arm_64.c"
  DEPS
    ::common_arm_64
    ::arm_64_dotprod
    ::arm_64_i8mm
    iree::base::core_headers
    iree::schemas::cpu_data
    iree::builtins::ukernel::internal_headers
    ${IREE_UK_ARM_64_DEPS}
  PUBLIC
)

set(IREE_UK_ARCH_DEPS "iree::builtins::ukernel::arch::arm_64" PARENT_SCOPE)
