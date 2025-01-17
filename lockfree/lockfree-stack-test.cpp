#include "lockfree-stack.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <algorithm>
#include <deque>
#include <random>

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

    constexpr auto num_threads = 4;
    constexpr auto pushes_per_thread = 1000;
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

// TEST_F(LockfreeStackTest, ConcurrentPushPop)
// {
//   const int num_threads = 4;
//   const int operations_per_thread = 100;
//   std::vector<std::thread> threads;
//   std::atomic<int> total_pushes{0};
//   std::atomic<int> successful_pops{0};
//
//   // Producer threads
//   for (int i = 0; i < num_threads; ++i)
//   {
//     threads.emplace_back([this, &total_pushes, operations_per_thread]()
//                          {
//             for (int j = 0; j < operations_per_thread; ++j) {
//                 stack.push(j);
//                 total_pushes++;
//             } });
//   }
//
//   // Consumer threads
//   for (int i = 0; i < num_threads; ++i)
//   {
//     threads.emplace_back([this, &successful_pops, operations_per_thread]()
//                          {
//             while (successful_pops < operations_per_thread * num_threads) {
//                 if (auto val = stack.pop()) {
//                     successful_pops++;
//                 }
//             } });
//   }
//
//   for (auto &thread : threads)
//   {
//     thread.join();
//   }
//
//   EXPECT_EQ(total_pushes.load(), successful_pops.load());
//   EXPECT_TRUE(stack.empty());
// }

// TEST_F(LockfreeStackTest, StressTest)
// {
//   const int num_threads = 8;
//   const int iterations = 10000;
//   std::vector<std::thread> threads;
//   std::atomic<int> push_count{0};
//   std::atomic<int> pop_count{0};
//
//   for (int i = 0; i < num_threads; ++i)
//   {
//     threads.emplace_back([this, &push_count, &pop_count, iterations]()
//                          {
//             for (int j = 0; j < iterations; ++j) {
//                 if (j % 2 == 0) {
//                     stack.push(j);
//                     push_count++;
//                 } else {
//                     if (stack.pop()) {
//                         pop_count++;
//                     }
//                 }
//             } });
//   }
//
//   for (auto &thread : threads)
//   {
//     thread.join();
//   }
//
//   // Pop remaining elements
//   while (stack.pop())
//   {
//     pop_count++;
//   }
//
//   EXPECT_EQ(push_count.load(), pop_count.load());
//   EXPECT_TRUE(stack.empty());
// }

class MultiWriterSingleReaderTest : public testing::Test {
protected:
    LockfreeStack<int> stack;
    const int NUM_WRITERS = 4;
    const int ITEMS_PER_WRITER = 10000;
    const std::chrono::milliseconds OPERATION_TIME{100};  // Time to run operations
};

TEST_F(MultiWriterSingleReaderTest, MultipleWritersSingleReader) {
    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    std::atomic<int> total_pushed{0};
    std::atomic<int> total_popped{0};
    std::set<int> unique_values;
    std::mutex set_mutex;

    // Create writer threads
    std::vector<std::thread> writers;
    for (int i = 0; i < NUM_WRITERS; ++i) {
        writers.emplace_back([&, writer_id=i]() {
            // Wait for start signal
            while (!start.load()) {
                std::this_thread::yield();
            }

            int base = writer_id * ITEMS_PER_WRITER;
            for (int j = 0; j < ITEMS_PER_WRITER && !stop.load(); ++j) {
                int value = base + j;
                stack.push(value);
                total_pushed.fetch_add(1);
            }
        });
    }

    // Create reader thread
    std::thread reader([&]() {
        // Wait for start signal
        while (!start.load()) {
            std::this_thread::yield();
        }

        while (!stop.load() || !stack.empty()) {
            if (auto value = stack.pop()) {
                {
                    std::lock_guard<std::mutex> lock(set_mutex);
                    unique_values.insert(*value);
                }
                total_popped.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });

    // Start the test
    start.store(true);

    // Let the test run for a set duration
    std::this_thread::sleep_for(OPERATION_TIME);
    stop.store(true);

    // Wait for all threads to complete
    for (auto& writer : writers) {
        writer.join();
    }
    reader.join();

    // Verify results
    EXPECT_EQ(total_pushed.load(), total_popped.load())
        << "Items pushed: " << total_pushed.load()
        << ", Items popped: " << total_popped.load();

    // Verify no duplicates were processed
    EXPECT_EQ(unique_values.size(), total_popped.load())
        << "Unique values: " << unique_values.size()
        << ", Total popped: " << total_popped.load();

    // Verify stack is empty
    EXPECT_TRUE(stack.empty()) << "Stack should be empty after test";
}

TEST_F(MultiWriterSingleReaderTest, BurstyWriters) {
    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    std::atomic<int> total_pushed{0};
    std::atomic<int> total_popped{0};
    std::set<int> unique_values;
    std::mutex set_mutex;

    // Create bursty writer threads
    std::vector<std::thread> writers;
    for (int i = 0; i < NUM_WRITERS; ++i) {
        writers.emplace_back([&, writer_id=i]() {
            while (!start.load()) {
                std::this_thread::yield();
            }

            // Create bursts of pushes with random pauses
            int base = writer_id * ITEMS_PER_WRITER;
            int items_remaining = ITEMS_PER_WRITER;

            while (items_remaining > 0 && !stop.load()) {
                // Push a burst of items
                int burst_size = std::min(100, items_remaining);
                for (int j = 0; j < burst_size; ++j) {
                    stack.push(base + (ITEMS_PER_WRITER - items_remaining) + j);
                    total_pushed.fetch_add(1);
                }
                items_remaining -= burst_size;

                // Random pause between bursts
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(rand() % 10)
                );
            }
        });
    }

    // Create reader thread
    std::thread reader([&]() {
        while (!start.load()) {
            std::this_thread::yield();
        }

        while (!stop.load() || !stack.empty()) {
            if (auto value = stack.pop()) {
                {
                    std::lock_guard<std::mutex> lock(set_mutex);
                    unique_values.insert(*value);
                }
                total_popped.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });

    // Start the test
    start.store(true);

    // Let the test run for a set duration
    std::this_thread::sleep_for(OPERATION_TIME);
    stop.store(true);

    // Wait for all threads to complete
    for (auto& writer : writers) {
        writer.join();
    }
    reader.join();

    // Verify results
    EXPECT_EQ(total_pushed.load(), total_popped.load());
    EXPECT_EQ(unique_values.size(), total_popped.load());
    EXPECT_TRUE(stack.empty());
}

class SingleWriterSingleReaderTest : public testing::Test {
protected:
    LockfreeStack<int> stack;
    const int NUM_ITEMS = 10000;
    const std::chrono::milliseconds OPERATION_TIME{100};
};

TEST_F(SingleWriterSingleReaderTest, OrderPreservation) {
    std::vector<int> written_values;
    std::vector<int> read_values;
    std::atomic<bool> writer_finished{false};

    // Writer thread - push all values first
    std::thread writer([&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            stack.push(i);
            written_values.push_back(i);
        }
        writer_finished.store(true);
    });

    // Reader thread - start only after writer is done
    std::thread reader([&]() {
        // Wait for writer to finish
        while (!writer_finished.load()) {
            std::this_thread::yield();
        }

        // Now pop everything
        while (!stack.empty()) {
            if (auto value = stack.pop()) {
                read_values.push_back(*value);
            }
        }
    });

    writer.join();
    reader.join();

    // Verify results
    ASSERT_EQ(NUM_ITEMS, written_values.size()) << "Not all items were written";
    ASSERT_EQ(NUM_ITEMS, read_values.size()) << "Not all items were read";

    // For LIFO order, the read values should be the reverse of written values
    for (size_t i = 0; i < NUM_ITEMS; ++i) {
        EXPECT_EQ(written_values[NUM_ITEMS - 1 - i], read_values[i])
            << "Order mismatch at index " << i;
    }

    // Verify stack is empty
    EXPECT_TRUE(stack.empty()) << "Stack should be empty after test";
}

TEST_F(SingleWriterSingleReaderTest, BurstyWriter) {
    std::vector<int> written_values;
    std::vector<int> read_values;
    std::atomic<bool> writer_finished{false};

    // Writer thread with bursts
    std::thread writer([&]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> burst_size_dist(10, 100);  // Random burst sizes

        int items_written = 0;
        while (items_written < NUM_ITEMS) {
            // Determine burst size
            int burst_size = std::min(burst_size_dist(gen), NUM_ITEMS - items_written);

            // Write burst
            for (int i = 0; i < burst_size; ++i) {
                int value = items_written + i;
                stack.push(value);
                written_values.push_back(value);
            }

            items_written += burst_size;

            // Random pause between bursts (for more realistic burst behavior)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        writer_finished.store(true);
    });

    // Reader thread - starts only after writer is done
    std::thread reader([&]() {
        // Wait for writer to finish
        while (!writer_finished.load()) {
            std::this_thread::yield();
        }

        // Now pop everything
        while (!stack.empty()) {
            if (auto value = stack.pop()) {
                read_values.push_back(*value);
            }
        }
    });

    writer.join();
    reader.join();

    // Verify results
    ASSERT_EQ(NUM_ITEMS, written_values.size())
        << "Writer didn't write all items. Written: " << written_values.size();
    ASSERT_EQ(NUM_ITEMS, read_values.size())
        << "Reader didn't read all items. Read: " << read_values.size();

    // For LIFO order, the read values should be the exact reverse of written values
    for (size_t i = 0; i < NUM_ITEMS; ++i) {
        EXPECT_EQ(written_values[NUM_ITEMS - 1 - i], read_values[i])
            << "Order mismatch at index " << i
            << ". Expected " << written_values[NUM_ITEMS - 1 - i]
            << " but got " << read_values[i];
    }

    // Additional verification: stack should be empty
    EXPECT_TRUE(stack.empty()) << "Stack should be empty after test";

    // Print first mismatch for debugging if test fails
    if (testing::Test::HasFailure()) {
        for (size_t i = 0; i < NUM_ITEMS; ++i) {
            if (written_values[NUM_ITEMS - 1 - i] != read_values[i]) {
                std::cout << "First mismatch at index " << i << ":\n";
                std::cout << "Expected: " << written_values[NUM_ITEMS - 1 - i] << "\n";
                std::cout << "Got: " << read_values[i] << "\n";
                // Print a few values around the mismatch
                size_t start = (i > 2) ? i - 2 : 0;
                size_t end = std::min(i + 3, read_values.size());
                std::cout << "Values around mismatch:\n";
                for (size_t j = start; j < end; ++j) {
                    std::cout << "Index " << j
                             << ": written[" << (NUM_ITEMS - 1 - j) << "]="
                             << written_values[NUM_ITEMS - 1 - j]
                             << ", read[" << j << "]=" << read_values[j] << "\n";
                }
                break;
            }
        }
    }
}

int main()
{
  testing::InitGoogleTest();
  return RUN_ALL_TESTS();
}