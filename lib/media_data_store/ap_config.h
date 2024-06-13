#ifndef AP_CONFIG_H
#define AP_CONFIG_H
#pragma once

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

//#include "ap_export.h"

#if _WIN32
#pragma warning(disable : 4251)
#endif

#define DECLARE_STRING_PROPERTY(n)                                                                                     \
private:                                                                                                               \
  std::string n##_;                                                                                                    \
                                                                                                                       \
public:                                                                                                                \
  const std::string &n() const { return n##_; }                                                                        \
                                                                                                                       \
public:                                                                                                                \
  void n(const std::string &value) { n##_ = value; }

#define DECLARE_BOOL_PROPERTY(n)                                                                                       \
private:                                                                                                               \
  bool n##_;                                                                                                           \
                                                                                                                       \
public:                                                                                                                \
  const bool &n() const { return n##_; }                                                                               \
                                                                                                                       \
public:                                                                                                                \
  void n(const bool &value) { n##_ = value; }

#define DECLARE_INTEGER32_PROPERTY(n)                                                                                  \
private:                                                                                                               \
  std::int32_t n##_;                                                                                                   \
                                                                                                                       \
public:                                                                                                                \
  const std::int32_t &n() const { return n##_; }                                                                       \
                                                                                                                       \
public:                                                                                                                \
  void n(const std::int32_t &value) { n##_ = value; }

#define DECLARE_INTEGER64_PROPERTY(n)                                                                                  \
private:                                                                                                               \
  std::int64_t n##_;                                                                                                   \
                                                                                                                       \
public:                                                                                                                \
  const std::int64_t &n() const { return n##_; }                                                                       \
                                                                                                                       \
public:                                                                                                                \
  void n(const std::int64_t &value) { n##_ = value; }

#define DECLARE_FLOAT_PROPERTY(n)                                                                                      \
private:                                                                                                               \
  std::float_t n##_;                                                                                                   \
                                                                                                                       \
public:                                                                                                                \
  const std::float_t &n() const { return n##_; }                                                                       \
                                                                                                                       \
public:                                                                                                                \
  void n(const std::float_t &value) { n##_ = value; }

#define DECLARE_OBJECT_PROPERTY(n, t)                                                                                  \
private:                                                                                                               \
  t n##_;                                                                                                              \
                                                                                                                       \
public:                                                                                                                \
  const t &n() const { return n##_; }                                                                                  \
                                                                                                                       \
public:                                                                                                                \
  void n(const t &value) { n##_ = value; }


#endif // AP_CONFIG_H
