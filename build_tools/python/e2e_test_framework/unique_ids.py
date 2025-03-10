# Copyright 2022 The IREE Authors
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""List of unique random IDs in the framework and id utilities.

Each ID should be generated from uuid.uuid4().
"""

import hashlib
from typing import Sequence

# Special id which will be ignored when calculating the composite id.
#
# It should only be used when adding a new field to a composite object while
# we want to maintain the same id on the existing composite objects.
#
# In such case, you need to create a "default config" for the new field with
# this id and populate that config to the fields of the existing objects. The
# composite id computing function will ignore this id and keep the output
# unchanged.
TRANSPARENT_ID = "00000000-0000-0000-0000-000000000000"


def hash_composite_id(keys: Sequence[str]) -> str:
  """Computes the composite hash id from string keys.

  String keys are the component ids that compose this composite object. We hash
  the composite id since the id isn't designed to be inspected and insufficient
  to reconstruct the original composite object.

  Note that the output is sensitive to the order of the keys, and any key ==
  TRANSPARENT_ID will be skipped. When adding a new key to the keys, the new key
  should be always appended to the end. In this way, the composite id can be
  unchanged for the existing composite object if they use TRANSPARENT_ID on the
  new keyed field.

  The composite id is computed in the following steps:
  1. Index each key with its position in the list from 0.
  2. Remove any key == TRANSPARENT_ID
  3. Get the SHA256 hex digest of "0-key_0:1-key_1:..."

  Step 1 is needed to avoid the ambiguity between:
  ["key_abc", TRANSPARENT_ID] and [TRANSPARENT_ID, "key_abc"]
  since after removing TRANSPARENT_ID, they both become ["key_abc"] without the
  position index.

  Args:
    keys: list of string keys.

  Returns:
    Unique composite id.
  """
  trimmed_indexed_key = [
      f"{index}-{key}" for index, key in enumerate(keys)
      if key != TRANSPARENT_ID
  ]
  return hashlib.sha256(
      ":".join(trimmed_indexed_key).encode("utf-8")).hexdigest()


# To generate an id, run `uuid.uuid4()`.

# Models.
#    TFLite.
MODEL_DEEPLABV3_FP32 = "c36c63b0-220a-4d78-8ade-c45ce47d89d3"
MODEL_MOBILESSD_FP32 = "0e466f69-91d6-4e50-b62b-a82b6213a231"
MODEL_POSENET_FP32 = "5afc3014-d29d-4e88-a840-fbaf678acf2b"
MODEL_MOBILEBERT_FP32 = "cc69d69f-6d1f-4a1a-a31e-e021888d0d28"
MODEL_MOBILEBERT_INT8 = "e3997104-a3d2-46b4-9fbf-39069906d123"
MODEL_MOBILEBERT_FP16 = "73a0402e-271b-4aa8-a6a5-ac05839ca569"
MODEL_MOBILENET_V1 = "78eab9e5-9ff1-4769-9b55-933c81cc9a0f"
MODEL_MOBILENET_V2 = "7d45f8e5-bb5e-48d0-928d-8f125104578f"
MODEL_MOBILENET_V3SMALL = "58855e40-eba9-4a71-b878-6b35e3460244"
MODEL_PERSON_DETECT_INT8 = "bc1338be-e3df-44fd-82e4-40ba9560a073"
MODEL_EFFICIENTNET_INT8 = "4a6f545e-1b4e-41a5-9236-792aa578184b"

#    Tensorflow.
MODEL_MINILM_L12_H384_UNCASED_INT32_SEQLEN128 = "ecf5c970-ee97-49f0-a4ed-df1f34e9d493"
MODEL_BERT_FOR_MASKED_LM_FP32_SEQLEN512_TF = "39d157ad-f0ec-4a76-963b-d783beaed60f"
MODEL_EFFICIENTNET_V2_S_FP32_TF = "ebe7897f-5613-435b-a330-3cb967704e5e"
MODEL_BERT_LARGE_TF_FP32_SEQLEN384 = "8871f602-571c-4eb8-b94d-554cc8ceec5a"
MODEL_MOBILENET_V2_INT8 = "3dd5a95e-92a9-4486-9062-9a33224f28db"

MODEL_RESNET50_3X224X224_FP32_TF = "9a5a8b8c-6e7a-4b51-bb4f-84e738957238"
MODEL_BERT_LARGE_384_FP32_TF = "5f3de3b3-fd00-4582-a97e-b70ff5edab07"
MODEL_T5_LARGE_512_FP32_TF = "587e595d-2adf-4e41-9617-43178a133725"

#    PyTorch.
MODEL_CLIP_TEXT_SEQLEN64_FP32_TORCH = "9a9515c7-cb68-4c34-b1d2-0e8c0a3620b8"
MODEL_UNET_2D_FP32_TORCH = "340553d1-e6fe-41b6-b2c7-687c74ccec56"
MODEL_EFFICIENTNET_V2_S_FP32_TORCH = "cc474102-7d2f-4ec1-92ae-84e83ba0f390"
MODEL_EFFICIENTNET_V2_S_FP16_TORCH = "271ea7a0-68e7-45b6-91f4-f39d5ce9e29c"
MODEL_EFFICIENTNET_B7_FP32_TORCH = "68caa96e-b8bb-48a2-bb08-a3044981a370"

MODEL_BERT_LARGE_384_FP32_TORCH = "cbc5e400-7c93-4844-aca8-bce8f1bf9948"
MODEL_BERT_LARGE_384_FP16_TORCH = "c1be9a9d-64c3-4d88-8551-89a8b4f305ba"
MODEL_RESNET50_3X224X224_FP32_TORCH = "fd05da43-5e37-4fa0-88f8-3ceec1682345"
MODEL_RESNET50_3X224X224_FP16_TORCH = "5e779dd2-f115-4a1c-9aab-87b7ae4a568d"

#    JAX. We use same ids as what is defined in the OpenXLA model library.
MODEL_RESNET50 = "aff75509-4420-40a8-844e-dbfc48494fe6-MODEL_RESNET50"
MODEL_RESNET50_FP32 = f"{MODEL_RESNET50}-fp32"
MODEL_RESNET50_FP32_JAX = f"{MODEL_RESNET50_FP32}-JAX"
MODEL_RESNET50_FP32_JAX_3X224X224XF32 = f"{MODEL_RESNET50_FP32_JAX}-3x224x224xf32"

MODEL_BERT_LARGE = "47cb0d3a-5eb7-41c7-9d7c-97aae7023ecf-MODEL_BERT_LARGE"
MODEL_BERT_LARGE_FP32 = f"{MODEL_BERT_LARGE}-fp32"
MODEL_BERT_LARGE_FP32_JAX = f"{MODEL_BERT_LARGE_FP32}-JAX"
MODEL_BERT_LARGE_FP32_JAX_384XI32 = f"{MODEL_BERT_LARGE_FP32_JAX}-384xi32"

MODEL_T5_LARGE = "173c7180-bad4-4b91-8423-4beeb13d2b0a-MODEL_T5_LARGE"
MODEL_T5_LARGE_FP32 = f"{MODEL_T5_LARGE}-fp32"
MODEL_T5_LARGE_FP32_JAX = f"{MODEL_T5_LARGE_FP32}-JAX"
MODEL_T5_LARGE_FP32_JAX_512XI32 = f"{MODEL_T5_LARGE_FP32_JAX}-512xi32"

# Microbenchmarks. UB is shorthand for microbenchmark.
MICRO_MATMUL_3456X1024X2048_FP16_MLIR = "50a7aece-73f9-47f4-a93a-4a1178f45407"
MICRO_MATMUL_3456X1024X2048_FP32_MLIR = "a55afe1c-9410-47a6-b417-04b0d75ee5f4"
MICRO_MATMUL_2560X2560X2560_FP16_MLIR = "81cebaaf-e23d-4a32-89dc-9fc7adc37a8f"
MICRO_MATMUL_2560X2560X2560_FP32_MLIR = "579b77ea-76bd-4eb3-bd85-067c25a89eff"
MICRO_MATMUL_128X256X8192_FP16_MLIR = "699fd533-02a9-49f0-bf26-1902d8dbb5af"
MICRO_MATMUL_128X256X8192_FP32_MLIR = "a6c2b812-0a71-45e7-9ea5-f3d8529213ef"
MICRO_MATMUL_2564x2564x2564_FP32_MLIR = "4e75ff72-f807-49f6-b740-febca1794334"
MICRO_MATMUL_2562x2564x2562_FP32_MLIR = "8d6be288-9b88-41bd-bc5a-5644df0481bb"
MICRO_MATMUL_2562x2561x2561_FP32_MLIR = "0a3d952b-41ca-43d2-9ec3-ccb8dde20ce3"

# Model input data
MODEL_INPUT_DATA_ZEROS = "8d4a034e-944d-4725-8402-d6f6e61be93c"

# Devices
DEVICE_SPEC_GCP_C2_STANDARD_16 = "9a4804f1-b1b9-46cd-b251-7f16a655f782"
DEVICE_SPEC_GCP_A2_HIGHGPU_1G = "78c56b95-2d7d-44b5-b5fd-8e47aa961108"
DEVICE_SPEC_MOBILE_PIXEL_4 = "fc901efc-ddf8-44c0-b009-8eecb8286521"
DEVICE_SPEC_MOBILE_PIXEL_6_PRO = "0f624778-dc50-43eb-8060-7f4aea9634e1"
DEVICE_SPEC_MOBILE_MOTO_EDGE_X30 = "dae12e6a-4169-419d-adc9-4f63a2ff1997"
DEVICE_SPEC_MOBILE_SM_G980F = "0247f709-a300-4d12-b986-bdb0c15c2653"

# IREE benchmarks
IREE_COMPILE_CONFIG_VMVX_GENERIC_EXPERIMENTAL = "75336abd-8108-462c-9ce3-15443e3f32f4"
IREE_COMPILE_CONFIG_LINUX_CASCADELAKE = "e7e18b0f-c72d-4f1c-89b1-5afee70df6e9"
IREE_COMPILE_CONFIG_LINUX_CASCADELAKE_FUSE_PADDING = "6d0d5716-5525-44ad-b71d-8075ee1583a6"
IREE_COMPILE_CONFIG_LINUX_RV64_GENERIC_DEFAULTS = "cdf579a9-5446-403b-a991-802a6c702e65"
IREE_COMPILE_CONFIG_LINUX_RV32_GENERIC_DEFAULTS = "6d9ce240-ec14-4d8f-a8e4-1b20aa17b4e4"
IREE_COMPILE_CONFIG_LINUX_CUDA_SM80_DEFAULTS = "09cb5300-7f73-45cf-9f68-e114c77ca030"
IREE_COMPILE_CONFIG_LINUX_CUDA_SM80_MATMUL_UBENCH = "3f66ba98-5716-4d30-9a87-50bc78e5f714"
IREE_COMPILE_CONFIG_LINUX_CUDA_SM80_MATMUL_SPLITK_UBENCH = "54cf2ec3-d073-4281-9561-b6c1280bd0eb"
IREE_COMPILE_CONFIG_LINUX_VULKAN_SD_SIMT = "da0ea6e6-719b-43ee-bfec-72eb3b1173bf"
IREE_COMPILE_CONFIG_LINUX_VULKAN_SD_TENSORCORE = "97790694-4f0f-4d83-bc52-d74e019c1df9"
IREE_COMPILE_CONFIG_ANDROID_ARM_VALHALL_DEFAULTS = "8da35f2b-a042-4b7d-9dcf-5ebbc1728765"
IREE_COMPILE_CONFIG_ANDROID_ARM_VALHALL_EXPERIMENTAL = "32a56c8d-cc6c-41b8-8620-1f8eda0b8223"
IREE_COMPILE_CONFIG_ANDROID_ARM_VALHALL_EXPERIMENTAL_REPEATED_KERNEL = "6b601a8d-4824-42e0-bcc6-500c0c3fa346"
IREE_COMPILE_CONFIG_ANDROID_ARMV8_2_A_GENERIC_DEFAULTS = "1f2adf49-282e-4aff-9d4f-e63b1621f1e8"
IREE_COMPILE_CONFIG_ANDROID_ARMV8_2_A_GENERIC_MMT4D = "d463322c-24e6-4685-85ca-d541b41a405f"
IREE_COMPILE_CONFIG_ANDROID_ARMV8_2_A_GENERIC_MMT4D_DOTPROD = "f672a6b9-99fc-47ce-8b1b-8e5f44a541a1"
IREE_COMPILE_CONFIG_ANDROID_QUALCOMM_ADRENO_DEFAULTS = "c7eea358-d8d2-4199-9d75-bb741c399b1b"
IREE_COMPILE_CONFIG_ANDROID_QUALCOMM_ADRENO_FUSE_PADDING = "d3038b95-c889-456a-bff6-5cbabd10f1ad"
IREE_COMPILE_CONFIG_ANDROID_QUALCOMM_ADRENO_FUSE_PADDING_REPEATED_KERNEL = "70b823ca-2807-4531-8c00-e02af7d70466"
IREE_MODULE_EXECUTION_CONFIG_LOCAL_SYNC = "13fc65a9-e5dc-4cbb-9c09-25b0b08f4c03"
IREE_MODULE_EXECUTION_CONFIG_LOCAL_TASK_BASE = "c7c4a15e-b20c-4898-bb4a-864f34ff34b2"
IREE_MODULE_EXECUTION_CONFIG_SYS_SCHED_LOCAL_TASK_BASE = "0dfb6b03-bd15-45a9-b82a-345c03f1fea6"
IREE_MODULE_EXECUTION_CONFIG_CUDA = "f7c0ec98-f028-436a-b05a-7d35cf18ce2d"
IREE_MODULE_EXECUTION_CONFIG_CUDA_BATCH_SIZE_100 = "ce15c338-b1d1-4ee3-b876-22d3cc5a831d"
IREE_MODULE_EXECUTION_CONFIG_VULKAN = "34ae13f0-d6d9-43f7-befb-15d024e88e89"
IREE_MODULE_EXECUTION_CONFIG_VULKAN_BATCH_SIZE_16 = "b10737a8-5da4-4052-9b7a-5b07f21e02d0"
IREE_MODULE_EXECUTION_CONFIG_VULKAN_BATCH_SIZE_32 = "c59f6ed8-ef78-4ddd-93ea-f173c5e4d6b8"
IREE_MODULE_EXECUTION_CONFIG_VMVX_LOCAL_TASK_BASE = "953183e2-1e84-4a51-a43c-9b869bdc2218"
IREE_MODULE_EXECUTION_CONFIG_VMVX_SYS_SCHED_LOCAL_TASK_BASE = "a1a9795e-2fc5-4d95-abc0-b0fb41b07557"
IREE_MODEL_IMPORT_STABLEHLO_MLIR_DEFAULT = "8b2df698-f3ba-4207-8696-6c909776eac4"
IREE_MODEL_IMPORT_TFLITE_DEFAULT = "16280d67-7ce0-4807-ab4b-0cb3c771d206"
IREE_MODEL_IMPORT_LINALG_MLIR_DEFAULT = "8afc4561-e84d-4a91-af55-2b1917465fcc"
