#pragma once

// Force-included into every C++ translation unit of this project. Nitro's and
// this package's C++ sources rely on transitive standard includes that libc++
// (iOS/Android) provides but the MSVC STL does not.
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
