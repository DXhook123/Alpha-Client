#pragma once
#include "../../Utils/MemoryUtil.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

class HashedString {
public:
	CLASS_MEMBER(uint64_t, mStrHash, 0x0);
	CLASS_MEMBER(std::string, mStr, 0x8);
	CLASS_MEMBER(const HashedString*, mLastMatch, 0x28);

	HashedString() {
		memset(this, 0, sizeof(HashedString));
	}

	HashedString(const std::string& text) {
		memset(this, 0, sizeof(HashedString));
		mStr = text;
		computeHash();
	}

	HashedString(uint64_t hash, const std::string& text) {
		memset(this, 0, sizeof(HashedString));
		mStrHash = hash;
		mStr = text;
	}

	[[nodiscard]] static constexpr uint64_t computeHash(std::string_view str) {
		if (str.empty()) return 0;
		uint64_t hash = 0xCBF29CE484222325ULL;
		for (char c : str) {
			hash = static_cast<uint64_t>(c) ^ (0x100000001B3ULL * hash);
		}
		return hash;
	}

	void computeHash() {
		mStrHash = 0xCBF29CE484222325ULL;
		if (mStr.empty()) return;
		for (char c : mStr) {
			mStrHash = static_cast<uint64_t>(c) ^ (0x100000001B3ULL * mStrHash);
		}
	}

	void setText(const std::string& text) {
		mStr = text;
		computeHash();
	}

	bool operator==(const HashedString& rhs) const {
		const std::string& thisStr = *reinterpret_cast<const std::string*>(reinterpret_cast<uintptr_t>(this) + 0x8);
		const std::string& rhsStr = *reinterpret_cast<const std::string*>(reinterpret_cast<uintptr_t>(&rhs) + 0x8);
		return thisStr == rhsStr;
	}

	bool operator!=(const HashedString& rhs) const {
		return !(*this == rhs);
	}

	bool operator<(const HashedString& rhs) const {
		const std::string& thisStr = *reinterpret_cast<const std::string*>(reinterpret_cast<uintptr_t>(this) + 0x8);
		const std::string& rhsStr = *reinterpret_cast<const std::string*>(reinterpret_cast<uintptr_t>(&rhs) + 0x8);
		uint64_t thisHash = *reinterpret_cast<const uint64_t*>(reinterpret_cast<uintptr_t>(this));
		uint64_t rhsHash = *reinterpret_cast<const uint64_t*>(reinterpret_cast<uintptr_t>(&rhs));
		if (thisHash < rhsHash) return true;
		if (thisHash == rhsHash) return thisStr < rhsStr;
		return false;
	}
private:
	char pad[0x30];
};
