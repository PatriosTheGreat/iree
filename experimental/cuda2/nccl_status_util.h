// Copyright 2023 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef IREE_EXPERIMENTAL_CUDA2_NCCL_STATUS_UTIL_H_
#define IREE_EXPERIMENTAL_CUDA2_NCCL_STATUS_UTIL_H_

#include <stdint.h>

#include "experimental/cuda2/nccl_dynamic_symbols.h"
#include "iree/base/api.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Converts a ncclResult_t to an iree_status_t.
//
// Usage:
//   iree_status_t status = IREE_NCCL_RESULT_TO_STATUS(nccl_symbols,
//                                                     ncclDoThing(...));
#define IREE_NCCL_RESULT_TO_STATUS(syms, expr, ...) \
  iree_hal_nccl2_result_to_status((syms), ((syms)->expr), __FILE__, __LINE__)

// IREE_RETURN_IF_ERROR but implicitly converts the ncclResult_t return value to
// an iree_status_t.
//
// Usage:
//   IREE_NCCL_RETURN_IF_ERROR(nccl_symbols, ncclDoThing(...), "message");
#define IREE_NCCL_RETURN_IF_ERROR(syms, expr, ...)                             \
  IREE_RETURN_IF_ERROR(iree_hal_nccl2_result_to_status((syms), ((syms)->expr), \
                                                       __FILE__, __LINE__),    \
                       __VA_ARGS__)

// IREE_IGNORE_ERROR but implicitly converts the ncclResult_t return value to
// an iree_status_t.
//
// Usage:
//   IREE_NCCL_IGNORE_ERROR(nccl_symbols, ncclDoThing(...));
#define IREE_NCCL_IGNORE_ERROR(syms, expr)                                  \
  IREE_IGNORE_ERROR(iree_hal_nccl2_result_to_status((syms), ((syms)->expr), \
                                                    __FILE__, __LINE__))

// Converts a ncclResult_t to an iree_status_t object.
iree_status_t iree_hal_nccl2_result_to_status(
    const iree_hal_cuda2_nccl_dynamic_symbols_t* syms, ncclResult_t result,
    const char* file, uint32_t line);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // IREE_EXPERIMENTAL_CUDA2_NCCL_STATUS_UTIL_H_
