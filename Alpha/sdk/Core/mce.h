#pragma once
#include <cstdint>
#include <algorithm>
#include <memory>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include "../../Utils/MemoryUtil.h"
#include "../Signatures.h"
#include "HashedString.h"

namespace mce {

class Blob {
public:
	using value_type = unsigned char;
	using delete_function = void (*)(value_type*);

	struct Deleter {
		delete_function fn = &Blob::defaultDeleter;
		void operator()(value_type* ptr) const noexcept {
			if (ptr && fn) fn(ptr);
		}
	};

	struct NativePtr {
		Deleter mDeleter{};
		value_type* mData = nullptr;

		NativePtr() = default;
		NativePtr(value_type* data, Deleter deleter) : mDeleter(deleter), mData(data) {}
		value_type* get() const { return mData; }
		Deleter get_deleter() const { return mDeleter; }
	};

	NativePtr mBlob;
	std::size_t mSize = 0;

	Blob() = default;
	Blob(Blob&&) noexcept = default;
	Blob& operator=(Blob&&) noexcept = default;
	Blob(const Blob&) = delete;
	Blob& operator=(const Blob&) = delete;

	static Blob fromVector(const std::vector<value_type>& bytes) {
		Blob blob;
		if (!bytes.empty()) {
			blob.mBlob = NativePtr(new value_type[bytes.size()], Deleter{});
			std::copy(bytes.begin(), bytes.end(), blob.mBlob.get());
			blob.mSize = bytes.size();
		}
		return blob;
	}

	value_type* data() { return mBlob.mData; }
	const value_type* data() const { return mBlob.mData; }
	std::size_t size() const { return mSize; }

private:
	static void defaultDeleter(value_type* ptr) noexcept { delete[] ptr; }
};

static_assert(sizeof(Blob) == 0x18, "mce::Blob layout must remain 0x18 bytes");

enum class ImageFormat : int {
	None = 0,
	R8Unorm = 1,
	RGB8Unorm = 2,
	RGBA8Unorm = 3,
};

enum class ImageUsage : unsigned char {
	Unknown = 0,
	sRGB = 1,
	Data = 2,
};

struct Image {
	ImageFormat imageFormat = ImageFormat::None;
	uint32_t mWidth = 0;
	uint32_t mHeight = 0;
	uint32_t mDepth = 0;
	ImageUsage mUsage = ImageUsage::Unknown;
	unsigned char pad[7]{};
	Blob mImageBytes;
};

static_assert(sizeof(Image) == 0x30, "mce::Image layout must remain 0x30 bytes");

struct Color {
	union {
		struct {
			float r, g, b, a;
		};
		float arr[4];
	};

	Color(float red = 1.f, float green = 1.f, float blue = 1.f, float alpha = 1.f)
		: r(red), g(green), b(blue), a(alpha) {}

	static const Color White;
	static const Color Black;
	static const Color Red;
	static const Color Green;
	static const Color Blue;
	static const Color Transparent;
};

inline const Color Color::White(1.f, 1.f, 1.f, 1.f);
inline const Color Color::Black(0.f, 0.f, 0.f, 1.f);
inline const Color Color::Red(1.f, 0.f, 0.f, 1.f);
inline const Color Color::Green(0.f, 1.f, 0.f, 1.f);
inline const Color Color::Blue(0.f, 0.f, 1.f, 1.f);
inline const Color Color::Transparent(0.f, 0.f, 0.f, 0.f);

struct UUID {
	uint64_t mLow;
	uint64_t mHigh;

	static UUID generate() {
		UUID uuid = {};
		uuid.mLow = rand();
		uuid.mHigh = rand();
		return uuid;
	}

	[[nodiscard]] std::string toString() const {
		uint32_t timeLow = (mLow & 0xFFFFFFFF);
		uint16_t timeMid = (mLow >> 32) & 0xFFFF;
		uint16_t timeHiAndVersion = ((mLow >> 48) & 0x0FFF) | (4 << 12);
		uint16_t clkSeq = (mHigh & 0x3FFF) | 0x8000;
		uint16_t nodeHi = (mHigh >> 16) & 0xFFFF;
		uint32_t nodeLow = (mHigh >> 32);

		std::stringstream ss;
		ss << std::hex << std::setfill('0')
			<< std::setw(8) << timeLow << '-'
			<< std::setw(4) << timeMid << '-'
			<< std::setw(4) << timeHiAndVersion << '-'
			<< std::setw(4) << clkSeq << '-'
			<< std::setw(4) << nodeHi
			<< std::setw(8) << nodeLow;
		return ss.str();
	}

	bool operator==(const UUID& other) const {
		return mLow == other.mLow && mHigh == other.mHigh;
	}

	bool operator!=(const UUID& other) const {
		return !(*this == other);
	}

	explicit operator bool() const {
		return mLow != 0 || mHigh != 0;
	}
};

struct MaterialPtr {
	char pad[0x20];
};

}
