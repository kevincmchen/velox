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

#include <gtest/gtest.h>

#include <folly/Random.h>
#include <folly/init/Init.h>

#include "velox/row/UnsafeRowDeserializers.h"
#include "velox/row/UnsafeRowFast.h"
#include "velox/vector/fuzzer/VectorFuzzer.h"
#include "velox/vector/tests/utils/VectorTestBase.h"

namespace facebook::velox::row {
namespace {

using namespace facebook::velox::test;

class UnsafeRowFuzzTests : public ::testing::Test {
 public:
  static void SetUpTestCase() {
    memory::MemoryManager::testingSetInstance({});
  }

  UnsafeRowFuzzTests() {
    clearBuffers();
  }

  void clearBuffers() {
    for (auto& buffer : buffers_) {
      std::memset(buffer, 0, kBufferSize);
    }
  }

  template <typename T>
  void doTest(
      const RowTypePtr& rowType,
      std::function<std::vector<T>(const RowVectorPtr& data)> serializeFunc) {
    VectorFuzzer::Options opts;
    opts.vectorSize = kNumBuffers;
    opts.nullRatio = 0.1;
    opts.dictionaryHasNulls = false;
    opts.stringVariableLength = true;
    opts.stringLength = 20;
    opts.containerVariableLength = true;
    opts.complexElementsMaxSize = 10'000;

    // Spark uses microseconds to store timestamp
    opts.timestampPrecision =
        VectorFuzzer::Options::TimestampPrecision::kMicroSeconds,
    opts.containerLength = 10;

    VectorFuzzer fuzzer(opts, pool_.get());

    const auto iterations = 200;
    for (size_t i = 0; i < iterations; ++i) {
      clearBuffers();

      auto seed = folly::Random::rand32();

      LOG(INFO) << "seed: " << seed;
      SCOPED_TRACE(fmt::format("seed: {}", seed));

      fuzzer.reSeed(seed);
      const auto& inputVector = fuzzer.fuzzInputRow(rowType);

      // Serialize rowVector into bytes.
      auto serialized = serializeFunc(inputVector);
      if constexpr (std::is_same_v<T, std::optional<std::string_view>>) {
        // Deserialize previous bytes back to row vector
        VectorPtr outputVector = UnsafeRowDeserializer::deserialize(
            serialized, rowType, pool_.get());

        assertEqualVectors(inputVector, outputVector);
      } else {
        VectorPtr outputVector =
            UnsafeRowFast::deserialize(serialized, rowType, pool_.get());

        assertEqualVectors(inputVector, outputVector);
      }
    }
  }

  static constexpr uint64_t kBufferSize = 70 << 10; // 70kb
  static constexpr uint64_t kNumBuffers = 100;

  std::array<char[kBufferSize], kNumBuffers> buffers_{};

  std::shared_ptr<memory::MemoryPool> pool_ =
      memory::memoryManager()->addLeafPool();
};

TEST_F(UnsafeRowFuzzTests, fast) {
  auto rowType = ROW({
      BOOLEAN(),
      TINYINT(),
      SMALLINT(),
      INTEGER(),
      VARCHAR(),
      BIGINT(),
      REAL(),
      DOUBLE(),
      VARCHAR(),
      VARBINARY(),
      UNKNOWN(),
      DECIMAL(20, 2),
      DECIMAL(12, 4),
      // Arrays.
      ARRAY(BOOLEAN()),
      ARRAY(TINYINT()),
      ARRAY(SMALLINT()),
      ARRAY(INTEGER()),
      ARRAY(BIGINT()),
      ARRAY(REAL()),
      ARRAY(DOUBLE()),
      ARRAY(VARCHAR()),
      ARRAY(VARBINARY()),
      ARRAY(UNKNOWN()),
      ARRAY(DECIMAL(20, 2)),
      ARRAY(DECIMAL(12, 4)),
      // Nested arrays.
      ARRAY(ARRAY(INTEGER())),
      ARRAY(ARRAY(BIGINT())),
      ARRAY(ARRAY(VARCHAR())),
      ARRAY(ARRAY(UNKNOWN())),
      // Maps.
      MAP(BIGINT(), REAL()),
      MAP(BIGINT(), BIGINT()),
      MAP(BIGINT(), VARCHAR()),
      MAP(BIGINT(), DECIMAL(20, 2)),
      MAP(BIGINT(), DECIMAL(12, 4)),
      MAP(INTEGER(), MAP(BIGINT(), DOUBLE())),
      MAP(VARCHAR(), BOOLEAN()),
      MAP(INTEGER(), MAP(BIGINT(), ARRAY(REAL()))),
      // Timestamp and date types.
      TIMESTAMP(),
      DATE(),
      ARRAY(TIMESTAMP()),
      ARRAY(DATE()),
      MAP(DATE(), ARRAY(TIMESTAMP())),
      // Structs.
      ROW(
          {BOOLEAN(),
           INTEGER(),
           TIMESTAMP(),
           DECIMAL(20, 2),
           VARCHAR(),
           ARRAY(BIGINT())}),
      ROW(
          {BOOLEAN(),
           ROW({INTEGER(), TIMESTAMP()}),
           VARCHAR(),
           ARRAY(BIGINT())}),
      ARRAY({ROW({BIGINT(), VARCHAR()})}),
      MAP(BIGINT(), ROW({BOOLEAN(), TINYINT(), REAL()})),
  });

  doTest<char*>(rowType, [&](const RowVectorPtr& data) {
    const auto numRows = data->size();
    std::vector<char*> serialized;
    serialized.reserve(numRows);

    UnsafeRowFast fast(data);

    std::vector<vector_size_t> rows(numRows);
    std::iota(rows.begin(), rows.end(), 0);
    std::vector<vector_size_t> serializedRowSizes(numRows);
    std::vector<vector_size_t*> serializedRowSizesPtr(numRows);
    for (auto i = 0; i < numRows; ++i) {
      serializedRowSizesPtr[i] = &serializedRowSizes[i];
    }
    fast.serializedRowSizes(
        folly::Range(rows.data(), numRows), serializedRowSizesPtr.data());
    for (auto i = 0; i < numRows; ++i) {
      // The serialized row includes the size of the row.
      VELOX_CHECK_EQ(serializedRowSizes[i], fast.rowSize(i) + sizeof(uint32_t));
    }

    for (auto i = 0; i < data->size(); ++i) {
      auto rowSize = fast.serialize(i, buffers_[i]);
      VELOX_CHECK_LE(rowSize, kBufferSize);

      EXPECT_EQ(rowSize, fast.rowSize(i)) << i << ", " << data->toString(i);

      serialized.push_back(buffers_[i]);
    }
    return serialized;
  });

  doTest<std::optional<std::string_view>>(
      rowType, [&](const RowVectorPtr& data) {
        const auto numRows = data->size();
        std::vector<std::optional<std::string_view>> serialized;
        serialized.reserve(numRows);

        UnsafeRowFast fast(data);

        std::vector<vector_size_t> rows(numRows);
        std::iota(rows.begin(), rows.end(), 0);
        std::vector<vector_size_t> serializedRowSizes(numRows);
        std::vector<vector_size_t*> serializedRowSizesPtr(numRows);
        for (auto i = 0; i < numRows; ++i) {
          serializedRowSizesPtr[i] = &serializedRowSizes[i];
        }
        fast.serializedRowSizes(
            folly::Range(rows.data(), numRows), serializedRowSizesPtr.data());
        for (auto i = 0; i < numRows; ++i) {
          // The serialized row includes the size of the row.
          VELOX_CHECK_EQ(
              serializedRowSizes[i], fast.rowSize(i) + sizeof(uint32_t));
        }

        for (auto i = 0; i < data->size(); ++i) {
          auto rowSize = fast.serialize(i, buffers_[i]);
          VELOX_CHECK_LE(rowSize, kBufferSize);

          EXPECT_EQ(rowSize, fast.rowSize(i)) << i << ", " << data->toString(i);

          serialized.push_back(std::string_view(buffers_[i], rowSize));
        }
        return serialized;
      });
}

} // namespace
} // namespace facebook::velox::row
