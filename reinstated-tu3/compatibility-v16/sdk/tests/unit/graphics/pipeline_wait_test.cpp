/**
 * @file        graphics/pipeline_wait_test.cpp
 * @brief       Tests for the bounded wait for pipelines created in the background
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <chrono>

#include <catch2/catch_test_macros.hpp>

#include <rex/graphics/util/pipeline_wait.h>

using rex::graphics::WaitForPipelineCreation;

TEST_CASE("A ready pipeline is taken without waiting", "[graphics][pipeline_wait]") {
  int calls = 0;
  CHECK(WaitForPipelineCreation(
      [&]() {
        ++calls;
        return true;
      },
      50));
  CHECK(calls == 1);
}

TEST_CASE("A budget of 0 checks once and never waits", "[graphics][pipeline_wait]") {
  int calls = 0;
  CHECK_FALSE(WaitForPipelineCreation(
      [&]() {
        ++calls;
        return false;
      },
      0));
  CHECK(calls == 1);
}

TEST_CASE("A pipeline that appears while waiting is taken", "[graphics][pipeline_wait]") {
  int calls = 0;
  CHECK(WaitForPipelineCreation([&]() { return ++calls == 3; }, 1000));
  CHECK(calls == 3);
}

TEST_CASE("A pipeline still missing after the budget is given up", "[graphics][pipeline_wait]") {
  const auto start = std::chrono::steady_clock::now();
  CHECK_FALSE(WaitForPipelineCreation([]() { return false; }, 2));
  CHECK(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(2));
}
