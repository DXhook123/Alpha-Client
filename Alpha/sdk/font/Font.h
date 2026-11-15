#pragma once

#include "../Core/mce.h"
#include <cstddef>
#include <cstdint>

class Font {
public:
	static constexpr std::size_t Colors = 0x18;
	static constexpr std::size_t ScalarOverride = 0x218;
	static constexpr std::size_t ScaleOverrideCharacter = 0x220;
	static constexpr std::size_t TextureGroup = 0x238;
	static constexpr std::size_t StringCache = 0x248;
	static constexpr std::size_t ObfuscatedIndex = 0x258;
	static constexpr std::size_t ObfuscatedTextTime = 0x25C;
	static constexpr std::size_t CaretRenderPosition = 0x260;
	static constexpr std::size_t CaretRenderSize = 0x268;
	static constexpr std::size_t AlwaysUnicode = 0x270;
	static constexpr std::size_t AutoResetFormat = 0x271;
	static constexpr std::size_t UseCache = 0x272;
	static constexpr std::size_t CurrentColor = 0x274;
	static constexpr std::size_t FontTexture = 0x284;
	static constexpr std::size_t CaretColor = 0x288;
	static constexpr std::size_t Italic = 0x298;
	static constexpr std::size_t Bold = 0x299;
	static constexpr std::size_t Strikethrough = 0x29A;
	static constexpr std::size_t Underlined = 0x29B;
	static constexpr std::size_t Obfuscated = 0x29C;
	static constexpr std::size_t FontMaterial = 0x2A0;

	[[nodiscard]] void* const* getVTable() const {
		return *reinterpret_cast<void* const* const*>(this);
	}

	[[nodiscard]] const mce::Color& getColor(std::size_t index) const {
		return read<mce::Color>(Colors + index * sizeof(mce::Color));
	}

	[[nodiscard]] float getScalarOverride() const {
		return read<float>(ScalarOverride);
	}

	[[nodiscard]] bool isAlwaysUnicode() const {
		return read<bool>(AlwaysUnicode);
	}

	[[nodiscard]] bool isAutoResetFormat() const {
		return read<bool>(AutoResetFormat);
	}

	[[nodiscard]] bool isUsingCache() const {
		return read<bool>(UseCache);
	}

	[[nodiscard]] bool isItalic() const {
		return read<bool>(Italic);
	}

	[[nodiscard]] bool isBold() const {
		return read<bool>(Bold);
	}

	[[nodiscard]] bool isStrikethrough() const {
		return read<bool>(Strikethrough);
	}

	[[nodiscard]] bool isUnderlined() const {
		return read<bool>(Underlined);
	}

	[[nodiscard]] bool isObfuscated() const {
		return read<bool>(Obfuscated);
	}

private:
	template <typename T>
	[[nodiscard]] const T& read(std::size_t offset) const {
		return *reinterpret_cast<const T*>(reinterpret_cast<const std::byte*>(this) + offset);
	}
};

static_assert(Font::Colors == 0x18);
static_assert(Font::ScalarOverride == 0x218);
static_assert(Font::FontMaterial == 0x2A0);
