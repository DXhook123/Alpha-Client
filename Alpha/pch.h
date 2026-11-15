#pragma once

#if defined(_M_X64) && defined(_DEBUG)
#error Alpha x64 requires /MD without _DEBUG; use ALPHA_DEBUG for project debug code.
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#ifdef _M_X64
static_assert(_ITERATOR_DEBUG_LEVEL == 0, "Alpha requires release-compatible STL layout, including Debug builds");
static_assert(sizeof(std::string) == 0x20, "Alpha requires the game's x64 std::string layout");
static_assert(sizeof(std::vector<int>) == 0x18, "Alpha requires the game's x64 std::vector layout");
#endif
