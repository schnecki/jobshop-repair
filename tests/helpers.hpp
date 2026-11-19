#pragma once

#include <sstream>
#include <string>

#include "jsr/instance.hpp"

namespace jsr::test {

inline Instance fromString(const std::string& text) {
  std::istringstream in(text);
  return parseInstance(in, "inline");
}

inline Instance ft06() {
  return readInstance(JSR_INSTANCE_DIR "/ft06.txt");
}

// J0: m0(3) -> m1(2)
// J1: m0(2) -> m1(4)
inline Instance tiny() {
  return fromString(
      "2 2\n"
      "0 3 1 2\n"
      "0 2 1 4\n");
}

}  // namespace jsr::test
