#ifndef LOGGER_H_
#define LOGGER_H_

#include <cstdint>
#include <cstring>
#include <hal/systick.h>

struct InternedString {
  const char* str;
};

enum class LogLevel {
  DEBUG,
  INFO,
  WARNING,
  ERROR,
  OFF
};

template<class Derived>
class Logger {
 public:
  template<typename ... T>
  inline void log(LogLevel level, T ...args) {
    if (level < m_level) return;

    m_derived.startMessage(m_systick.getCoarseTickCount());
    sendRemainingArguments(args...);
    m_derived.finishMessage();
  }

  inline void printfFmtValidator([[maybe_unused]] const char* fmt, ...)
    __attribute__((format(printf, 2, 3))) { }

  void setLevel(LogLevel level) { m_level = level; }

 private:
  LogLevel m_level = LogLevel::DEBUG;
  Derived& m_derived = static_cast<Derived&>(*this);
  SysTick& m_systick = SysTick::getInstance();

  template<typename T>
  inline void sendArgument(const T argument) {
    m_derived.appendData(reinterpret_cast<const uint8_t*>(&argument), sizeof(T));
  }

  inline void sendArgument(const char* argument) {
    m_derived.appendString(argument);
  }

  template<typename T>
  inline void sendRemainingArguments(const T& first_arg) {
    sendArgument(first_arg);
  }

  template<typename T, typename ... Types>
  inline void sendRemainingArguments(const T& first_arg, Types... args) {
    sendArgument(first_arg);
    sendRemainingArguments(args...);
  }
};

template<char... N>
struct InternedDebugString {
  __attribute__((section(".interned_strings.debug"))) static constexpr char string[] { N... };
};

template<char... N>
constexpr char InternedDebugString<N...>::string[];

template<char... N>
struct InternedInfoString {
  __attribute__((section(".interned_strings.info"))) static constexpr char string[] { N... };
};

template<char... N>
constexpr char InternedInfoString<N...>::string[];

template<char... N>
struct InternedWarningString {
  __attribute__((section(".interned_strings.warning"))) static constexpr char string[] { N... };
};

template<char... N>
constexpr char InternedWarningString<N...>::string[];

template<char... N>
struct InternedErrorString {
  __attribute__((section(".interned_strings.error"))) static constexpr char string[] { N... };
};

template<char... N>
constexpr char InternedErrorString<N...>::string[];

template<typename T, T... C>
InternedString operator ""_intern_debug() {
  return InternedString { decltype(InternedDebugString<C..., T{}>{})::string };
}

template<typename T, T... C>
InternedString operator ""_intern_info() {
  return InternedString { decltype(InternedInfoString<C..., T{}>{})::string };
}

template<typename T, T... C>
InternedString operator ""_intern_warning() {
  return InternedString { decltype(InternedWarningString<C..., T{}>{})::string };
}

template<typename T, T... C>
InternedString operator ""_intern_error() {
  return InternedString { decltype(InternedErrorString<C..., T{}>{})::string };
}

#define LOG_DEBUG(logger, fmt, ...) \
  { \
    (logger)->printfFmtValidator(fmt, ## __VA_ARGS__); \
    (logger)->log(LogLevel::DEBUG, fmt ## _intern_debug, ## __VA_ARGS__); \
  }

#define LOG_INFO(logger, fmt, ...) \
  { \
    (logger)->printfFmtValidator(fmt, ## __VA_ARGS__); \
    (logger)->log(LogLevel::INFO, fmt ## _intern_info, ## __VA_ARGS__); \
  }

#define LOG_WARNING(logger, fmt, ...) \
  { \
    (logger)->printfFmtValidator(fmt, ## __VA_ARGS__); \
    (logger)->log(LogLevel::WARNING, fmt ## _intern_warning, ## __VA_ARGS__); \
  }

#define LOG_ERROR(logger, fmt, ...) \
  { \
    (logger)->printfFmtValidator(fmt, ## __VA_ARGS__); \
    (logger)->log(LogLevel::ERROR, fmt ## _intern_error, ## __VA_ARGS__); \
  }

#endif  // LOGGER_H_
