#pragma once
#include <atomic>
#include <cstdint>

namespace RenderFrameCounter {
	inline std::atomic_uint64_t serial = 0;
}
