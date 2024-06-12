#ifndef UTILS_H_
#define UTILS_H_
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

#if __ANDROID__
#include <jni.h>
#endif


inline uint16_t swap_bytes(const uint16_t v) { return ((v & 0x00ff) << 8) | ((v & 0xff00) >> 8); }

inline uint32_t swap_bytes(const uint32_t v) {
  uint32_t r = 0;
  uint8_t *pr = (uint8_t *)&r;
  uint8_t *pv = (uint8_t *)&v;
  pr[0] = pv[3];
  pr[1] = pv[2];
  pr[2] = pv[1];
  pr[3] = pv[0];
  return r;
}

inline uint64_t swap_bytes(const uint64_t v) {
  uint64_t r = 0;
  uint8_t *pr = (uint8_t *)&r;
  uint8_t *pv = (uint8_t *)&v;
  pr[0] = pv[7];
  pr[1] = pv[6];
  pr[2] = pv[5];
  pr[3] = pv[4];
  pr[4] = pv[3];
  pr[5] = pv[2];
  pr[6] = pv[1];
  pr[7] = pv[0];
  return r;
}


/// <summary>
/// Sets the name for the thread.
/// </summary>
/// <param name="t">For windows platofmr this is the thread handle.</param>
/// <param name="name">The thread name.</param>
void set_thread_name(void *t, const char *name);

/// <summary>
///
/// </summary>
/// <param name="name"></param>
void set_current_thread_name(const char *name);

/// <summary>
///
/// </summary>
std::string string_replace(const std::string &str, const std::string &pattern, const std::string &with);

std::string generate_file_name();

/// <summary>
///
/// </summary>
int compare_string_no_case(const char *str1, const char *str2);

/// <summary>
///
/// </summary>
bool get_youtube_url(const char *data, uint32_t length, std::string &url);



#endif // !UTILS_H_
