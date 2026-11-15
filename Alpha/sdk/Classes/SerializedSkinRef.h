#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class SkinImageBlob {
public:
	const unsigned char* data() const { return dataPtr; }
	size_t size() const { return dataSize; }

private:
	void* deleter = nullptr;
	unsigned char* dataPtr = nullptr;
	size_t dataSize = 0;
};

class SkinImage {
public:
	bool hasRgbaBytes() const {
		if (!bytes.data() || width == 0 || height == 0 || width > 4096 || height > 4096)
			return false;

		const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
		if (pixelCount / height != width)
			return false;

		const size_t expectedSize = pixelCount * 4u;
		if (expectedSize / 4u != pixelCount)
			return false;
		return bytes.size() >= expectedSize;
	}

	int imageFormat = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t depth = 0;
	uint8_t usage = 0;
	uint8_t pad[7]{};
	SkinImageBlob bytes;
};

static_assert(sizeof(SkinImageBlob) == 0x18, "SkinImageBlob layout must remain 0x18 bytes");
static_assert(sizeof(SkinImage) == 0x30, "SkinImage layout must remain 0x30 bytes");

class SerializedSkinRef {
public:
	const std::string* getId() const {
		const uintptr_t wrapper = *reinterpret_cast<const uintptr_t*>(this);
		if (!wrapper)
			return nullptr;
		return reinterpret_cast<const std::string*>(wrapper);
	}

	const std::string* getName() const {
		const uintptr_t wrapper = *reinterpret_cast<const uintptr_t*>(this);
		if (!wrapper)
			return nullptr;
		return reinterpret_cast<const std::string*>(wrapper + 0x40);
	}

	const SkinImage* getSkinImage() const {
		const uintptr_t wrapper = *reinterpret_cast<const uintptr_t*>(this);
		if (!wrapper)
			return nullptr;
		return reinterpret_cast<const SkinImage*>(wrapper + 0xA0);
	}

	bool isValid() const {
		const std::string* name = getName();
		return name && !name->empty();
	}

private:
	void* skinImpl = nullptr;
	void* refCount = nullptr;
};

static_assert(sizeof(SerializedSkinRef) == 0x10, "SerializedSkinRef layout must remain 0x10 bytes");
