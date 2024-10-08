/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "velox/experimental/wave/exec/Wave.h"

namespace facebook::velox::wave {

std::string rowTypeString(const Type& type) {
  return "";
}

void AbstractAggregation::reserveState(InstructionStatus& reservedState) {
  instructionStatus = reservedState;
  // A group by produces 8 bytes of grid level state and uses the main main
  // BlockStatus for lane status.
  reservedState.gridState += 8;
}

AdvanceResult AbstractAggregation::canAdvance(
    WaveStream& stream,
    LaunchControl* control,
    OperatorState* state,
    int32_t programIdx) const {
  return {};
}

AdvanceResult AbstractReadAggregation::canAdvance(
    WaveStream& stream,
    LaunchControl* control,
    OperatorState* state,
    int32_t programIdx) const {
  auto* aggState = reinterpret_cast<AggregateOperatorState*>(state);
  if (aggState->isNew) {
    aggState->isNew = false;
    return {.numRows = 1};
  }
  return {};
}

} // namespace facebook::velox::wave
