#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <string>
#include <sstream>
#include <type_traits>
#include <memory>
#include <functional>

// Type trait to check if a type has a toString() method
template <typename T, typename = void>
struct has_to_string : std::false_type
{
};

template <typename T>
// This specialization uses SFINAE (Substitution Failure Is Not An Error) to detect if T has a toString() method
// std::void_t is used to create a substitution context
// decltype(std::declval<T>().toString()) attempts to call toString() on a value of type T
// If T has a toString() method, this specialization is selected, inheriting from std::true_type
// If T doesn't have a toString() method, this specialization is discarded due to SFINAE, falling back to the primary template
struct has_to_string<T, std::void_t<decltype(std::declval<T>().toString())>> : std::true_type
{
};

class Logger
{
private:
  std::ofstream file;
  std::queue<std::function<std::string()>> logQueue;
  std::mutex queueMutex;
  std::condition_variable condition;
  std::thread loggerThread;
  bool running;

  void processLogs()
  {
    while (true)
    {
      std::unique_lock<std::mutex> lock(queueMutex);
      // Wait until there are logs to process or the logger is shutting down
      // This prevents busy-waiting and allows the thread to sleep when there's no work
      // The lambda function is the predicate that determines when to wake up:
      // - If the queue is not empty, there are logs to process
      // - If running is false, the logger is shutting down and should exit
      condition.wait(lock, [this]
                     { return !logQueue.empty() || !running; });
      flushQueue();

      if (!running)
      {
        return;
      }
    }
  }

  void flushQueue()
  {
    while (!logQueue.empty())
    {
      std::string logMessage = logQueue.front()(); // Call the lambda to serialize
      // Add the timestamp to the front of the message
      auto now = std::chrono::system_clock::now();
      auto now_c = std::chrono::system_clock::to_time_t(now);
      std::stringstream timestamp;
      timestamp << std::put_time(std::localtime(&now_c), "[%Y-%m-%d %H:%M:%S] ");
      file << timestamp.str();

      file << logMessage << std::endl;
      logQueue.pop();
    }
    file.flush();
  }

  // Helper function to convert data to string
  template <typename T>
  static std::string dataToString(const T &data)
  {
    if constexpr (has_to_string<T>::value)
    {
      return data.toString();
    }
    else if constexpr (std::is_convertible_v<T, std::string>)
    {
      return std::string(data);
    }
    else
    {
      std::ostringstream oss;
      oss << data;
      return oss.str();
    }
  }

public:
  Logger(const std::string &filename) : file(filename), running(true)
  {
    loggerThread = std::thread(&Logger::processLogs, this);
  }

  ~Logger()
  {
    running = false;
    condition.notify_one();
    if (loggerThread.joinable())
    {
      loggerThread.join();
    }
    file.close();
  }

  template <typename T>
  void log(T &&data)
  {
    auto ptr = std::make_shared<std::decay_t<T>>(std::forward<T>(data));
    {
      std::lock_guard<std::mutex> lock(queueMutex);
      logQueue.push([ptr]()
                    { return dataToString(*ptr); });
    }
    condition.notify_one();
  }
};

#define LOG(data) logger.log(data)

// Example usage
class ExampleData
{
public:
  std::string toString() const
  {
    return "Example data with toString()";
  }
};

int main()
{
  Logger logger("log.txt");

  // Test with different types
  {
    ExampleData exampleData;
    LOG(exampleData);
  }
  LOG("Direct string");
  LOG(42);
  LOG(3.14);

  return 0;
}