#include "lockfree-stack.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <algorithm>

class LockfreeStackTest : public testing::Test
{
protected:
  LockfreeStack<int> stack;
};

// Basic functionality tests
TEST_F(LockfreeStackTest, InitiallyEmpty)
{
  EXPECT_TRUE(stack.empty());
}

TEST_F(LockfreeStackTest, PushSingleElement)
{
  stack.push(42);
  EXPECT_FALSE(stack.empty());
}

TEST_F(LockfreeStackTest, PushAndPop)
{
  stack.push(42);
  auto result = stack.pop();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 42);
  EXPECT_TRUE(stack.empty());
}

TEST_F(LockfreeStackTest, PopEmptyStack)
{
  auto result = stack.pop();
  EXPECT_FALSE(result.has_value());
}

TEST_F(LockfreeStackTest, MultipleElements)
{
  std::vector<int> values = {1, 2, 3, 4, 5};
  for (auto val : values)
  {
    stack.push(val);
  }

  // Since it's a stack, we expect LIFO order
  std::vector<int> popped;
  while (auto val = stack.pop())
  {
    popped.push_back(val.value());
  }

  std::reverse(values.begin(), values.end());
  EXPECT_EQ(values, popped);
}

// Thread safety tests
TEST_F(LockfreeStackTest, ConcurrentPush)
{
  const int num_threads = 4;
  const int pushes_per_thread = 1000;
  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back([this, i, pushes_per_thread]()
                         {
            for (int j = 0; j < pushes_per_thread; ++j) {
                stack.push(i * pushes_per_thread + j);
            } });
  }

  for (auto &thread : threads)
  {
    thread.join();
  }

  int count = 0;
  while (stack.pop())
  {
    count++;
  }

  EXPECT_EQ(count, num_threads * pushes_per_thread);
}

TEST_F(LockfreeStackTest, ConcurrentPushPop)
{
  const int num_threads = 4;
  const int operations_per_thread = 100;
  std::vector<std::thread> threads;
  std::atomic<int> total_pushes{0};
  std::atomic<int> successful_pops{0};

  // Producer threads
  for (int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back([this, &total_pushes, operations_per_thread]()
                         {
            for (int j = 0; j < operations_per_thread; ++j) {
                stack.push(j);
                total_pushes++;
            } });
  }

  // Consumer threads
  for (int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back([this, &successful_pops, operations_per_thread]()
                         {
            while (successful_pops < operations_per_thread * num_threads) {
                if (auto val = stack.pop()) {
                    successful_pops++;
                }
            } });
  }

  for (auto &thread : threads)
  {
    thread.join();
  }

  EXPECT_EQ(total_pushes.load(), successful_pops.load());
  EXPECT_TRUE(stack.empty());
}

TEST_F(LockfreeStackTest, StressTest)
{
  const int num_threads = 8;
  const int iterations = 10000;
  std::vector<std::thread> threads;
  std::atomic<int> push_count{0};
  std::atomic<int> pop_count{0};

  for (int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back([this, &push_count, &pop_count, iterations]()
                         {
            for (int j = 0; j < iterations; ++j) {
                if (j % 2 == 0) {
                    stack.push(j);
                    push_count++;
                } else {
                    if (stack.pop()) {
                        pop_count++;
                    }
                }
            } });
  }

  for (auto &thread : threads)
  {
    thread.join();
  }

  // Pop remaining elements
  while (stack.pop())
  {
    pop_count++;
  }

  EXPECT_EQ(push_count.load(), pop_count.load());
  EXPECT_TRUE(stack.empty());
}

int main()
{
  testing::InitGoogleTest();
  return RUN_ALL_TESTS();
}