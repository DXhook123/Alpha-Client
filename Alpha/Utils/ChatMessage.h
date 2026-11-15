#pragma once

#include <algorithm>
#include <string>

namespace ChatMessage {
	struct Style {
		bool showPrefix = true;
		std::string prefix;
		std::string tag;
		int prefixColor = 11;
		int tagColor = 14;
		int bracketColor = 8;
		int textColor = 7;
	};

	inline std::string colorCode(int color) {
		constexpr char codes[] = "0123456789abcdef";
		return std::string("\xC2\xA7") + codes[(std::clamp)(color, 0, 15)];
	}

	inline std::string cleanLabel(const std::string& label) {
		std::string result;
		for (size_t i = 0; i < label.size(); ++i) {
			if (label.compare(i, 2, "\xC2\xA7") == 0) {
				i += (i + 2 < label.size()) ? 2 : 1;
				continue;
			}
			if (static_cast<unsigned char>(label[i]) >= 0x20)
				result += label[i];
		}
		return result;
	}

	inline std::string format(const std::string& message, const Style& style) {
		std::string result = "\xC2\xA7r";
		if (style.showPrefix) {
			const auto addBracket = [&](const std::string& label, int color) {
				const std::string text = cleanLabel(label);
				if (!text.empty())
					result += colorCode(style.bracketColor) + "[" + colorCode(color) + text
						+ colorCode(style.bracketColor) + "]";
			};
			addBracket(style.prefix, style.prefixColor);
			addBracket(style.tag, style.tagColor);
			if (result.size() > 3) result += ' ';
		}
		return result + "\xC2\xA7r" + colorCode(style.textColor) + message + "\xC2\xA7r";
	}
}
