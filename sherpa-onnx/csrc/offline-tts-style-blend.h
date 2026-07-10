// sherpa-onnx/csrc/offline-tts-style-blend.h
//
// Copyright (c)  2026  Xiaomi Corporation

#ifndef SHERPA_ONNX_CSRC_OFFLINE_TTS_STYLE_BLEND_H_
#define SHERPA_ONNX_CSRC_OFFLINE_TTS_STYLE_BLEND_H_

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "sherpa-onnx/csrc/macros.h"

namespace sherpa_onnx {

// Parse a style-blend spec "sid[:weight][,sid[:weight]]..." into
// (sid, weight) pairs, with the weights normalized to sum to 1.
//
// A missing weight means 1, so "5,3" is an equal blend of speakers 5
// and 3, and "5:60,3:40" is 60% speaker 5, 40% speaker 3. The weight
// scale is arbitrary; only the ratios matter.
//
// This is meant for models whose speaker is a style embedding that the
// caller looks up in a table (e.g., Kokoro, Kitten): a weighted average
// of style rows is itself a valid style. Models that consume an integer
// sid inside the ONNX graph (e.g., vits, matcha) cannot blend this way.
//
// On invalid input it logs an error and returns an empty vector; the
// caller should treat that as a failed generation, not a fatal error.
inline std::vector<std::pair<int32_t, float>> ParseStyleBlend(
    const std::string &spec, int32_t num_speakers) {
  // Single-speaker models report num_speakers == 0 but accept sid 0.
  int32_t max_sid = num_speakers > 0 ? num_speakers : 1;

  std::vector<std::pair<int32_t, float>> ans;
  float total = 0;

  std::istringstream iss(spec);
  std::string entry;
  while (std::getline(iss, entry, ',')) {
    std::string sid_str = entry;
    std::string weight_str;
    auto colon = entry.find(':');
    if (colon != std::string::npos) {
      sid_str = entry.substr(0, colon);
      weight_str = entry.substr(colon + 1);
    }

    const char *s = sid_str.c_str();
    char *end = nullptr;
    int32_t sid = static_cast<int32_t>(strtol(s, &end, 10));
    if (end == s || *end != '\0' || sid < 0 || sid >= max_sid) {
      SHERPA_ONNX_LOGE(
          "Invalid sid '%s' in style blend '%s'. sid should be in the range "
          "[%d, %d]",
          sid_str.c_str(), spec.c_str(), 0, max_sid - 1);
      return {};
    }

    float weight = 1.0f;
    if (colon != std::string::npos) {
      s = weight_str.c_str();
      weight = strtof(s, &end);
      if (end == s || *end != '\0' || !std::isfinite(weight) || weight < 0) {
        SHERPA_ONNX_LOGE(
            "Invalid weight '%s' in style blend '%s'. Weights must be finite "
            "and >= 0",
            weight_str.c_str(), spec.c_str());
        return {};
      }
    }

    ans.emplace_back(sid, weight);
    total += weight;
  }

  if (ans.empty()) {
    SHERPA_ONNX_LOGE("Empty style blend spec");
    return {};
  }

  if (total <= 0) {
    SHERPA_ONNX_LOGE("Weights in style blend '%s' sum to 0", spec.c_str());
    return {};
  }

  for (auto &sw : ans) {
    sw.second /= total;
  }

  return ans;
}

}  // namespace sherpa_onnx

#endif  // SHERPA_ONNX_CSRC_OFFLINE_TTS_STYLE_BLEND_H_
