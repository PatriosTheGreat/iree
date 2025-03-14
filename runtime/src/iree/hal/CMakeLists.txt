################################################################################
# Autogenerated by build_tools/bazel_to_cmake/bazel_to_cmake.py from           #
# runtime/src/iree/hal/BUILD.bazel                                             #
#                                                                              #
# Use iree_cmake_extra_content from iree/build_defs.oss.bzl to add arbitrary   #
# CMake-only content.                                                          #
#                                                                              #
# To disable autogeneration for this file entirely, delete this header.        #
################################################################################

iree_add_all_subdirs()

iree_cc_library(
  NAME
    hal
  HDRS
    "api.h"
  SRCS
    "allocator.c"
    "allocator.h"
    "allocator_heap.c"
    "buffer.c"
    "buffer.h"
    "buffer_heap.c"
    "buffer_heap_impl.h"
    "buffer_view.c"
    "buffer_view.h"
    "buffer_view_util.c"
    "buffer_view_util.h"
    "channel.c"
    "channel.h"
    "channel_provider.c"
    "channel_provider.h"
    "command_buffer.c"
    "command_buffer.h"
    "command_buffer_validation.c"
    "command_buffer_validation.h"
    "detail.h"
    "device.c"
    "device.h"
    "driver.c"
    "driver.h"
    "driver_registry.c"
    "driver_registry.h"
    "event.c"
    "event.h"
    "executable.c"
    "executable.h"
    "executable_cache.c"
    "executable_cache.h"
    "fence.c"
    "fence.h"
    "pipeline_layout.c"
    "pipeline_layout.h"
    "resource.h"
    "semaphore.c"
    "semaphore.h"
    "string_util.c"
    "string_util.h"
  DEPS
    iree::base
    iree::base::core_headers
    iree::base::internal
    iree::base::internal::path
    iree::base::internal::synchronization
    iree::base::tracing
  PUBLIC
)

iree_cc_test(
  NAME
    string_util_test
  SRCS
    "string_util_test.cc"
  DEPS
    ::hal
    iree::base
    iree::base::internal::span
    iree::testing::gtest
    iree::testing::gtest_main
)

### BAZEL_TO_CMAKE_PRESERVES_ALL_CONTENT_BELOW_THIS_LINE ###
