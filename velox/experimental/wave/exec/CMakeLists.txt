# Copyright (c) Facebook, Inc. and its affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

add_library(
  velox_wave_exec
  Aggregation.cpp
  AggregationInstructions.cu
  ExprKernel.cu
  OperandSet.cpp
  ToWave.cpp
  WaveOperator.cpp
  Vectors.cpp
  Values.cpp
  WaveDriver.cpp
  Wave.cpp
  Project.cpp)

set_target_properties(velox_wave_exec PROPERTIES CUDA_ARCHITECTURES native)

target_link_libraries(velox_wave_exec velox_wave_vector velox_wave_common
                      velox_exception velox_common_base velox_exec)

if(${VELOX_BUILD_TESTING})
  add_subdirectory(tests)
endif()