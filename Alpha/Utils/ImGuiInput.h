#pragma once
#include <imgui.h>
#include <Windows.h>
#include <cstdint>
#include <string>

namespace ImGuiInput {
	void setGameWindow(HWND hwnd);
	HWND getGameWindow();
	void feedMouse();
	void feedKeyboard();
	void queueMouseWheel(float wheelX, float wheelY);
	void beginOverlayInput();
	void endOverlayInput();
	void resetMouseState();

	void setTextInputActive(HWND hwnd, bool active);
	void shutdownTextInput();
	bool isTextInputActive();
	bool isTextInputComposing();
	bool getCursorClientPos(float& x, float& y);
	bool shouldUseNativeTextInput();
	bool handleTextInputMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void drainTextInput();
	void syncTextInputMirror(const std::string& text);

	inline constexpr UINT WM_ALPHA_TEXT_INPUT = WM_APP + 0x0149;

	struct TextSyncTask {
		bool focusLeave = false;
		int32_t oldLength = 0;
		int32_t newLength = 0;
	};
	inline constexpr UINT WM_ALPHA_TEXT_SYNC = WM_APP + 0x0150;
	void runTextSyncTask(TextSyncTask* task);
}
