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

#include "velox/functions/sparksql/aggregates/CentralMomentsAggregate.h"
#include "velox/functions/lib/aggregates/CentralMomentsAggregatesBase.h"

namespace facebook::velox::functions::aggregate::sparksql {

namespace {
struct SkewnessResultAccessor {
  static bool hasResult(const CentralMomentsAccumulator& accumulator) {
    return accumulator.count() >= 1 && accumulator.m2() != 0;
  }

  static double result(const CentralMomentsAccumulator& accumulator) {
    return std::sqrt(accumulator.count()) * accumulator.m3() /
        std::pow(accumulator.m2(), 1.5);
  }
};

struct KurtosisResultAccessor {
  static bool hasResult(const CentralMomentsAccumulator& accumulator) {
    return accumulator.count() >= 1 && accumulator.m2() != 0;
  }

  static double result(const CentralMomentsAccumulator& accumulator) {
    double count = accumulator.count();
    double m2 = accumulator.m2();
    double m4 = accumulator.m4();
    return count * m4 / (m2 * m2) - 3.0;
  }
};

template <typename TResultAccessor>
exec::AggregateRegistrationResult registerCentralMoments(
    const std::string& name,
    bool withCompanionFunctions,
    bool overwrite) {
  std::vector<std::shared_ptr<exec::AggregateFunctionSignature>> signatures;
  signatures.push_back(
      exec::AggregateFunctionSignatureBuilder()
          .returnType("double")
          .intermediateType(CentralMomentsIntermediateResult::type())
          .argumentType("double")
          .build());

  return exec::registerAggregateFunction(
      name,
      std::move(signatures),
      [name](
          core::AggregationNode::Step step,
          const std::vector<TypePtr>& argTypes,
          const TypePtr& resultType,
          const core::QueryConfig& /*config*/)
          -> std::unique_ptr<exec::Aggregate> {
        VELOX_CHECK_EQ(argTypes.size(), 1, "{} takes only one argument", name);
        const auto& inputType = argTypes[0];
        if (exec::isRawInput(step)) {
          if (inputType->kind() == TypeKind::DOUBLE) {
            return std::make_unique<
                CentralMomentsAggregatesBase<double, TResultAccessor>>(
                resultType);
          }
          VELOX_UNSUPPORTED(
              "Unsupported input type: {}. "
              "Expected DOUBLE.",
              inputType->toString());
        } else {
          checkAccumulatorRowType(
              inputType,
              "Input type for final aggregation must be "
              "(count:bigint, m1:double, m2:double, m3:double, m4:double) struct");
          return std::make_unique<CentralMomentsAggregatesBase<
              int64_t /*unused*/,
              TResultAccessor>>(resultType);
        }
      },
      withCompanionFunctions,
      overwrite);
}
} // namespace

void registerCentralMomentsAggregate(
    const std::string& prefix,
    bool withCompanionFunctions,
    bool overwrite) {
  registerCentralMoments<SkewnessResultAccessor>(
      prefix + "skewness", withCompanionFunctions, overwrite);
  registerCentralMoments<KurtosisResultAccessor>(
      prefix + "kurtosis", withCompanionFunctions, overwrite);
}

} // namespace facebook::velox::functions::aggregate::sparksql
