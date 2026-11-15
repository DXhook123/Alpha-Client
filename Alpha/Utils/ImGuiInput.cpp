#include "ImGuiInput.h"
#include <cmath>

#include <imm.h>
#pragma comment(lib, "imm32.lib")

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Text.Core.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#ifndef ALPHA_BOOTSTRAP
#include "Client/Managers/ModuleManager/ModuleManager.h"
#include "Client/Managers/ModuleManager/Modules/Client/ClickGui.h"
#endif

namespace {
	HWND g_gameWindow = nullptr;
	float g_pendingWheelX = 0.f;
	float g_pendingWheelY = 0.f;
	bool g_prevKeyState[256] = {};
	bool g_overlayInputActive = false;

	using winrt::Windows::UI::Text::Core::CoreTextCompositionCompletedEventArgs;
	using winrt::Windows::UI::Text::Core::CoreTextCompositionStartedEventArgs;
	using winrt::Windows::UI::Text::Core::CoreTextEditContext;
	using winrt::Windows::UI::Text::Core::CoreTextLayoutRequestedEventArgs;
	using winrt::Windows::UI::Text::Core::CoreTextRange;
	using winrt::Windows::UI::Text::Core::CoreTextSelectionRequestedEventArgs;
	using winrt::Windows::UI::Text::Core::CoreTextTextRequestedEventArgs;
	using winrt::Windows::UI::Text::Core::CoreTextTextUpdatingEventArgs;

	HWND g_textInputWindow = nullptr;
	std::atomic<bool> g_textInputActive{ false };
	std::atomic<bool> g_textInputComposing{ false };
	std::atomic<bool> g_textInputBridgeEngaged{ false };
	std::atomic<bool> g_textBridgeDelivered{ false };
	std::atomic<bool> g_textLegacyDelivered{ false };
	CoreTextEditContext g_textContext{ nullptr };
	CoreTextEditContext::TextUpdating_revoker g_textUpdating;
	CoreTextEditContext::CompositionStarted_revoker g_compositionStarted;
	CoreTextEditContext::CompositionCompleted_revoker g_compositionCompleted;
	CoreTextEditContext::TextRequested_revoker g_textRequested;
	CoreTextEditContext::LayoutRequested_revoker g_layoutRequested;
	CoreTextEditContext::SelectionRequested_revoker g_selectionRequested;
	CoreTextEditContext::FocusRemoved_revoker g_focusRemoved;
	std::atomic<bool> g_textCallbacksClosing{false};
	std::atomic<uint32_t> g_textCallbacks{0};
	struct TextCallbackGuard {
		bool allowed;
		TextCallbackGuard() {
			g_textCallbacks.fetch_add(1, std::memory_order_acquire);
			allowed = !g_textCallbacksClosing.load(std::memory_order_acquire);
		}
		~TextCallbackGuard() {
			if (g_textCallbacks.fetch_sub(1, std::memory_order_release) == 1) g_textCallbacks.notify_all();
		}
	};

	void revokeTextCallbacks() {
		g_textUpdating.revoke(); g_compositionStarted.revoke(); g_compositionCompleted.revoke();
		g_textRequested.revoke(); g_layoutRequested.revoke(); g_selectionRequested.revoke(); g_focusRemoved.revoke();
	}
	std::mutex g_textMirrorMutex;
	std::wstring g_textMirror;
	CoreTextRange g_textCompositionRange{ -1, -1 };
	RECT g_textCaretRect{};
	std::atomic<bool> g_skipNextComposition{ false };
	std::atomic<DWORD> g_windowThreadId{ 0 };
	std::mutex g_pendingTextMutex;
	std::vector<std::string> g_pendingText;

	bool onWindowThread() {
		const DWORD id = g_windowThreadId.load(std::memory_order_relaxed);
		return id != 0 && id == GetCurrentThreadId();
	}

	void postTextSyncTask(bool focusLeave, int32_t oldLength, int32_t newLength) {
		HWND window = g_textInputWindow;
		if (!window) window = g_gameWindow;
		if (!window) return;
		auto* task = new ImGuiInput::TextSyncTask{ focusLeave, oldLength, newLength };
		if (!PostMessageW(window, ImGuiInput::WM_ALPHA_TEXT_SYNC, 0, reinterpret_cast<LPARAM>(task)))
			delete task;
	}

	std::wstring utf8ToWide(const std::string& text) {
		if (text.empty()) return {};
		const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
			text.data(), static_cast<int>(text.size()), nullptr, 0);
		if (length <= 0) return {};
		std::wstring result(static_cast<size_t>(length), L'\0');
		MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
			static_cast<int>(text.size()), result.data(), length);
		return result;
	}

	std::string wideToUtf8(const std::wstring& text) {
		if (text.empty()) return {};
		const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(),
			static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
		if (length <= 0) return {};
		std::string result(static_cast<size_t>(length), '\0');
		WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
			result.data(), length, nullptr, nullptr);
		return result;
	}

	void queueText(const std::wstring& text, bool fromTextBridge) {
		if (text.empty()) return;
		if (!g_textInputActive.load(std::memory_order_acquire)) return;
		if (fromTextBridge) {
			if (g_textLegacyDelivered.load(std::memory_order_acquire)) return;
			g_textBridgeDelivered.store(true, std::memory_order_release);
		} else {
			if (g_textBridgeDelivered.load(std::memory_order_acquire)) return;
			g_textLegacyDelivered.store(true, std::memory_order_release);
		}
		std::string utf8 = wideToUtf8(text);
		if (utf8.empty()) return;
		std::lock_guard lock(g_pendingTextMutex);
		g_pendingText.push_back(std::move(utf8));
	}

	bool isImeKeyboardLayout() {
		return ImmIsIME(GetKeyboardLayout(0)) != FALSE;
	}

	void ensureTextContext() {
		if (g_textContext || g_textCallbacksClosing.load(std::memory_order_acquire)) return;
		try {
			using namespace winrt::Windows::UI::Text::Core;
			g_textContext = CoreTextServicesManager::GetForCurrentView().CreateEditContext();
			if (!g_textContext) return;
			g_textContext.InputPaneDisplayPolicy(CoreTextInputPaneDisplayPolicy::Manual);

			g_textUpdating = g_textContext.TextUpdating(winrt::auto_revoke, [](CoreTextEditContext const&, CoreTextTextUpdatingEventArgs const& args) {
				TextCallbackGuard callback;
				if (!callback.allowed) return;
				const std::wstring text(args.Text());
				const CoreTextRange range = args.Range();
				const bool composing = g_textInputComposing.load(std::memory_order_relaxed);
				std::wstring committed;
				bool feedAll = false;

				{
					std::lock_guard lock(g_textMirrorMutex);
					if (!composing) {
						feedAll = isImeKeyboardLayout();
						g_skipNextComposition.store(feedAll, std::memory_order_relaxed);
					} else if (g_textCompositionRange.StartCaretPosition >= 0
						&& range.StartCaretPosition > g_textCompositionRange.StartCaretPosition
						&& range.StartCaretPosition <= static_cast<int32_t>(g_textMirror.size())) {
						committed = g_textMirror.substr(g_textCompositionRange.StartCaretPosition,
							range.StartCaretPosition - g_textCompositionRange.StartCaretPosition);
					}

					if (range.StartCaretPosition >= 0
						&& range.EndCaretPosition >= range.StartCaretPosition
						&& range.EndCaretPosition <= static_cast<int32_t>(g_textMirror.size())) {
						g_textMirror.replace(range.StartCaretPosition,
							range.EndCaretPosition - range.StartCaretPosition, text);
					}
					if (!composing) {
						g_textCompositionRange = CoreTextRange{ -1, -1 };
					} else {
						g_textCompositionRange = CoreTextRange{ range.StartCaretPosition,
							range.StartCaretPosition + static_cast<int32_t>(text.size()) };
					}
				}

				args.Result(CoreTextTextUpdatingResult::Succeeded);
				if (feedAll) queueText(text, true);
				else if (!committed.empty()) queueText(committed, true);
			});

			g_compositionStarted = g_textContext.CompositionStarted(winrt::auto_revoke, [](CoreTextEditContext const&, CoreTextCompositionStartedEventArgs const&) {
				TextCallbackGuard callback;
				if (!callback.allowed) return;
				g_textInputComposing.store(true, std::memory_order_relaxed);
				g_skipNextComposition.store(false, std::memory_order_relaxed);
			});

			g_compositionCompleted = g_textContext.CompositionCompleted(winrt::auto_revoke, [](CoreTextEditContext const&, CoreTextCompositionCompletedEventArgs const& args) {
				TextCallbackGuard callback;
				if (!callback.allowed) return;
				const bool skip = g_skipNextComposition.exchange(false, std::memory_order_relaxed);
				std::wstring committed;
				{
					std::lock_guard lock(g_textMirrorMutex);
					if (!skip) {
						bool fromSegments = false;
						try {
							for (auto const& segment : args.CompositionSegments()) {
								const CoreTextRange range = segment.Range();
								if (range.StartCaretPosition >= 0
									&& range.EndCaretPosition >= range.StartCaretPosition
									&& range.EndCaretPosition <= static_cast<int32_t>(g_textMirror.size())) {
									committed += g_textMirror.substr(range.StartCaretPosition,
										range.EndCaretPosition - range.StartCaretPosition);
									fromSegments = true;
								}
							}
						} catch (...) {}
						if (!fromSegments && g_textCompositionRange.StartCaretPosition >= 0
							&& g_textCompositionRange.EndCaretPosition <= static_cast<int32_t>(g_textMirror.size())) {
							committed = g_textMirror.substr(g_textCompositionRange.StartCaretPosition,
								g_textCompositionRange.EndCaretPosition - g_textCompositionRange.StartCaretPosition);
						}
					}
					g_textCompositionRange = CoreTextRange{ -1, -1 };
				}
				g_textInputComposing.store(false, std::memory_order_relaxed);
				if (!committed.empty()) queueText(committed, true);
			});

			g_textRequested = g_textContext.TextRequested(winrt::auto_revoke, [](CoreTextEditContext const&, CoreTextTextRequestedEventArgs const& args) {
				TextCallbackGuard callback;
				if (!callback.allowed) return;
				const CoreTextRange range = args.Request().Range();
				std::lock_guard lock(g_textMirrorMutex);
				if (range.StartCaretPosition >= 0 && range.EndCaretPosition >= range.StartCaretPosition
					&& range.EndCaretPosition <= static_cast<int32_t>(g_textMirror.size())) {
					args.Request().Text(g_textMirror.substr(range.StartCaretPosition,
						range.EndCaretPosition - range.StartCaretPosition));
				} else {
					args.Request().Text(L"");
				}
			});

			g_layoutRequested = g_textContext.LayoutRequested(winrt::auto_revoke, [](CoreTextEditContext const&, CoreTextLayoutRequestedEventArgs const& args) {
				TextCallbackGuard callback;
				if (!callback.allowed) return;
				winrt::Windows::Foundation::Rect rect{
					static_cast<float>(g_textCaretRect.left), static_cast<float>(g_textCaretRect.top),
					static_cast<float>(g_textCaretRect.right), static_cast<float>(g_textCaretRect.bottom) };
				auto bounds = args.Request().LayoutBounds();
				bounds.TextBounds(rect);
				bounds.ControlBounds(rect);
			});

			g_selectionRequested = g_textContext.SelectionRequested(winrt::auto_revoke, [](CoreTextEditContext const&, CoreTextSelectionRequestedEventArgs const& args) {
				TextCallbackGuard callback;
				if (!callback.allowed) return;
				std::lock_guard lock(g_textMirrorMutex);
				const int32_t caret = static_cast<int32_t>(g_textMirror.size());
				args.Request().Selection(CoreTextRange{ caret, caret });
			});

			g_focusRemoved = g_textContext.FocusRemoved(winrt::auto_revoke, [](CoreTextEditContext const&, winrt::Windows::Foundation::IInspectable const&) {
				TextCallbackGuard callback;
				if (!callback.allowed) return;
				g_textInputBridgeEngaged.store(false, std::memory_order_relaxed);
				g_textInputComposing.store(false, std::memory_order_relaxed);
			});
		} catch (...) {
			revokeTextCallbacks();
			g_textContext = nullptr;
		}
	}

	void engageTextContext() {
		ensureTextContext();
		if (!g_textContext || g_textInputBridgeEngaged.load(std::memory_order_acquire)) return;
		try {
			g_textContext.NotifyFocusEnter();
			g_textInputBridgeEngaged.store(true, std::memory_order_release);
			g_textInputComposing.store(false, std::memory_order_release);
			g_skipNextComposition.store(false, std::memory_order_release);
			g_textBridgeDelivered.store(false, std::memory_order_release);
			g_textLegacyDelivered.store(false, std::memory_order_release);
		} catch (...) {
			g_textInputBridgeEngaged.store(false, std::memory_order_release);
		}
	}

	void handleImm32Message(HWND hwnd, UINT msg, LPARAM lParam) {
		switch (msg) {
		case WM_IME_SETCONTEXT:
			if (hwnd) {
				if (HIMC context = ImmGetContext(hwnd)) {
					ImmReleaseContext(hwnd, context);
				} else if (HIMC created = ImmCreateContext()) {
					ImmAssociateContext(hwnd, created);
				}
			}
			break;
		case WM_IME_STARTCOMPOSITION:
			g_textInputComposing.store(true, std::memory_order_relaxed);
			g_textBridgeDelivered.store(false, std::memory_order_release);
			g_textLegacyDelivered.store(false, std::memory_order_release);
			break;
		case WM_IME_COMPOSITION:
			if (lParam & GCS_COMPSTR)
				g_textInputComposing.store(true, std::memory_order_relaxed);
			if (lParam & GCS_RESULTSTR) {
				if (HIMC context = ImmGetContext(hwnd)) {
					const LONG byteLength = ImmGetCompositionStringW(context, GCS_RESULTSTR, nullptr, 0);
					if (byteLength > 0) {
						std::wstring result(static_cast<size_t>(byteLength / sizeof(wchar_t)), L'\0');
						ImmGetCompositionStringW(context, GCS_RESULTSTR, result.data(), byteLength);
						queueText(result, false);
					}
					ImmReleaseContext(hwnd, context);
				}
			}
			break;
		case WM_IME_ENDCOMPOSITION:
			g_textInputComposing.store(false, std::memory_order_relaxed);
			break;
		}
	}

	bool isKeyDown(int vk) {
		return (GetAsyncKeyState(vk) & 0x8000) != 0;
	}

	bool wasKeyPressed(int vk) {
		const bool down = isKeyDown(vk);
		const bool pressed = down && !g_prevKeyState[vk];
		g_prevKeyState[vk] = down;
		return pressed;
	}

	ImGuiKey vkToImGuiKey(int vk) {
		if (vk >= '0' && vk <= '9') {
			return static_cast<ImGuiKey>(ImGuiKey_0 + (vk - '0'));
		}
		if (vk >= 'A' && vk <= 'Z') {
			return static_cast<ImGuiKey>(ImGuiKey_A + (vk - 'A'));
		}

		switch (vk) {
		case VK_TAB: return ImGuiKey_Tab;
		case VK_LEFT: return ImGuiKey_LeftArrow;
		case VK_RIGHT: return ImGuiKey_RightArrow;
		case VK_UP: return ImGuiKey_UpArrow;
		case VK_DOWN: return ImGuiKey_DownArrow;
		case VK_PRIOR: return ImGuiKey_PageUp;
		case VK_NEXT: return ImGuiKey_PageDown;
		case VK_HOME: return ImGuiKey_Home;
		case VK_END: return ImGuiKey_End;
		case VK_INSERT: return ImGuiKey_Insert;
		case VK_DELETE: return ImGuiKey_Delete;
		case VK_BACK: return ImGuiKey_Backspace;
		case VK_SPACE: return ImGuiKey_Space;
		case VK_RETURN: return ImGuiKey_Enter;
		case VK_ESCAPE: return ImGuiKey_Escape;
		case VK_CONTROL: return ImGuiKey_LeftCtrl;
		case VK_SHIFT: return ImGuiKey_LeftShift;
		case VK_MENU: return ImGuiKey_LeftAlt;
		default: return ImGuiKey_None;
		}
	}

	void submitTextCharacter(int vk) {
		ImGuiIO& io = ImGui::GetIO();
		if (!io.WantTextInput) {
			return;
		}

		if (vk >= '0' && vk <= '9') {
			io.AddInputCharacter(static_cast<unsigned int>(vk));
			return;
		}

		if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
			io.AddInputCharacter(static_cast<unsigned int>('0' + (vk - VK_NUMPAD0)));
			return;
		}

		if (vk >= 'A' && vk <= 'Z') {
			const bool shift = isKeyDown(VK_SHIFT);
			io.AddInputCharacter(static_cast<unsigned int>(shift ? vk : vk + 32));
			return;
		}

		switch (vk) {
		case VK_OEM_PERIOD:
		case VK_DECIMAL:
			io.AddInputCharacter('.');
			break;
		case VK_OEM_COMMA:
			io.AddInputCharacter(',');
			break;
		case VK_OEM_MINUS:
		case VK_SUBTRACT:
			io.AddInputCharacter('-');
			break;
		}
	}
	bool resolveCursorPosition(ImGuiIO& io, float& outX, float& outY) {
		HWND hwnd = g_gameWindow;
		if (hwnd == nullptr) {
			hwnd = GetForegroundWindow();
		}
		if (hwnd == nullptr) {
			return false;
		}

		POINT cursorPos = {};
		if (!GetCursorPos(&cursorPos) || !ScreenToClient(hwnd, &cursorPos)) {
			return false;
		}

		RECT clientRect = {};
		if (!GetClientRect(hwnd, &clientRect)) {
			outX = static_cast<float>(cursorPos.x);
			outY = static_cast<float>(cursorPos.y);
			return true;
		}

		const float clientW = static_cast<float>(clientRect.right - clientRect.left);
		const float clientH = static_cast<float>(clientRect.bottom - clientRect.top);
		const float displayW = io.DisplaySize.x;
		const float displayH = io.DisplaySize.y;

		if (clientW > 0.f && clientH > 0.f && displayW > 0.f && displayH > 0.f) {
			const float widthDiff = std::abs(displayW - clientW);
			const float heightDiff = std::abs(displayH - clientH);
			if (widthDiff > 1.f || heightDiff > 1.f) {
				outX = static_cast<float>(cursorPos.x) * (displayW / clientW);
				outY = static_cast<float>(cursorPos.y) * (displayH / clientH);
			}
			else {
				outX = static_cast<float>(cursorPos.x);
				outY = static_cast<float>(cursorPos.y);
			}
		}
		else {
			outX = static_cast<float>(cursorPos.x);
			outY = static_cast<float>(cursorPos.y);
		}

		return true;
	}
}

namespace ImGuiInput {
	void setGameWindow(HWND hwnd) {
		g_gameWindow = hwnd;
	}
	HWND getGameWindow() {
		return g_gameWindow;
	}

	void queueMouseWheel(float wheelX, float wheelY) {
		g_pendingWheelX += wheelX;
		g_pendingWheelY += wheelY;
	}

	void feedMouse() {
		if (ImGui::GetCurrentContext() == nullptr) {
			return;
		}

		ImGuiIO& io = ImGui::GetIO();
		io.MouseDrawCursor = true;

		float mouseX = 0.f;
		float mouseY = 0.f;
		if (resolveCursorPosition(io, mouseX, mouseY)) {
			io.AddMousePosEvent(mouseX, mouseY);
		}
		io.AddMouseButtonEvent(0, isKeyDown(VK_LBUTTON));
		io.AddMouseButtonEvent(1, isKeyDown(VK_RBUTTON));
		io.AddMouseButtonEvent(2, isKeyDown(VK_MBUTTON));

		if (g_pendingWheelX != 0.f || g_pendingWheelY != 0.f) {
			io.AddMouseWheelEvent(g_pendingWheelX, g_pendingWheelY);
			g_pendingWheelX = 0.f;
			g_pendingWheelY = 0.f;
		}
	}

	void feedKeyboard() {
		if (ImGui::GetCurrentContext() == nullptr) {
			return;
		}

		ImGuiIO& io = ImGui::GetIO();
		for (int vk = 0x08; vk < 0xFE; vk++) {
			const ImGuiKey imguiKey = vkToImGuiKey(vk);
			if (imguiKey != ImGuiKey_None) {
				io.AddKeyEvent(imguiKey, isKeyDown(vk));
			}

			if (wasKeyPressed(vk)) {
				submitTextCharacter(vk);
			}
		}
	}

	void resetMouseState() {
		if (ImGui::GetCurrentContext() == nullptr) {
			ClipCursor(nullptr);
			return;
		}

		ImGuiIO& io = ImGui::GetIO();
		io.AddMouseButtonEvent(0, false);
		io.AddMouseButtonEvent(1, false);
		io.AddMouseButtonEvent(2, false);
		io.MouseDown[0] = false;
		io.MouseDown[1] = false;
		io.MouseDown[2] = false;
		io.MouseDrawCursor = false;
		ClipCursor(nullptr);
	}

	void beginOverlayInput() {
		ClipCursor(nullptr);
		if (!g_overlayInputActive) {
			if (HWND captured = GetCapture(); captured && captured == g_gameWindow)
				ReleaseCapture();
			g_overlayInputActive = true;
		}

		if (ImGui::GetCurrentContext() == nullptr) {
			return;
		}

		ImGui::GetIO().MouseDrawCursor = true;
	}

	void endOverlayInput() {
		g_overlayInputActive = false;
		if (ImGui::GetCurrentContext() != nullptr) {
			ImGui::GetIO().MouseDrawCursor = false;
		}
	}

	void setTextInputActive(HWND hwnd, bool active) {
		if (active) {
			if (!hwnd) hwnd = GetFocus();
			if (!hwnd) hwnd = g_gameWindow ? g_gameWindow : GetForegroundWindow();
			if (!hwnd) return;
			g_textInputWindow = hwnd;
		}

		const HWND target = active ? hwnd : g_textInputWindow;
		if (!active) {
			const bool wasEngaged = g_textInputBridgeEngaged.load(std::memory_order_acquire);
			g_textInputActive.store(false, std::memory_order_release);
			g_textInputComposing.store(false, std::memory_order_release);
			g_textBridgeDelivered.store(false, std::memory_order_release);
			g_textLegacyDelivered.store(false, std::memory_order_release);
			if (g_textContext && wasEngaged && onWindowThread()) {
				try { g_textContext.NotifyFocusLeave(); } catch (...) {}
			} else if (g_textContext && wasEngaged) {
				postTextSyncTask(true, 0, 0);
			}
			g_textInputBridgeEngaged.store(false, std::memory_order_release);
			{
				std::lock_guard lock(g_textMirrorMutex);
				g_textMirror.clear();
				g_textCompositionRange = CoreTextRange{ -1, -1 };
			}
			{
				std::lock_guard lock(g_pendingTextMutex);
				g_pendingText.clear();
			}
			g_textInputWindow = nullptr;
		}

		if (!target) return;
		HIMC context = ImmGetContext(target);
		if (!context) {
			context = ImmCreateContext();
			if (context) ImmAssociateContext(target, context);
		}
		if (context) {
			ImmSetOpenStatus(context, active ? TRUE : FALSE);
			ImmReleaseContext(target, context);
		}

		if (!active) return;
		g_textInputActive.store(true, std::memory_order_release);

		POINT cursor = {};
		if (GetCursorPos(&cursor))
			g_textCaretRect = { cursor.x, cursor.y, cursor.x + 8, cursor.y + 24 };
	}

	void shutdownTextInput() {
		g_textCallbacksClosing.store(true, std::memory_order_release);
		setTextInputActive(nullptr, false);
		revokeTextCallbacks();
		auto count = g_textCallbacks.load(std::memory_order_acquire);
		while (count) {
			g_textCallbacks.wait(count, std::memory_order_acquire);
			count = g_textCallbacks.load(std::memory_order_acquire);
		}
		g_textContext = nullptr;
		std::lock_guard lock(g_pendingTextMutex);
		g_pendingText.clear();
	}

	bool isTextInputActive() {
		return g_textInputActive.load(std::memory_order_acquire);
	}

	bool isTextInputComposing() {
		return g_textInputComposing.load(std::memory_order_acquire);
	}

	bool getCursorClientPos(float& x, float& y) {
		HWND hwnd = g_gameWindow;
		if (!hwnd) hwnd = GetForegroundWindow();
		if (!hwnd) return false;
		POINT cursor{};
		if (!GetCursorPos(&cursor) || !ScreenToClient(hwnd, &cursor)) return false;
		RECT client{};
		if (!GetClientRect(hwnd, &client)) {
			x = static_cast<float>(cursor.x);
			y = static_cast<float>(cursor.y);
			return true;
		}
		const float clientW = static_cast<float>(client.right - client.left);
		const float clientH = static_cast<float>(client.bottom - client.top);
		const ImGuiIO& io = ImGui::GetIO();
		const float displayW = io.DisplaySize.x;
		const float displayH = io.DisplaySize.y;
		if (clientW > 1.f && clientH > 1.f && displayW > 1.f && displayH > 1.f) {
			if (std::abs(displayW - clientW) > 1.f || std::abs(displayH - clientH) > 1.f) {
				x = static_cast<float>(cursor.x) * (displayW / clientW);
				y = static_cast<float>(cursor.y) * (displayH / clientH);
				return true;
			}
		}
		x = static_cast<float>(cursor.x);
		y = static_cast<float>(cursor.y);
		return true;
	}

	bool shouldUseNativeTextInput() {
		return isTextInputActive() && (isImeKeyboardLayout() || isTextInputComposing());
	}

	bool handleTextInputMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		if (!isTextInputActive()) return false;
		g_windowThreadId.store(GetCurrentThreadId(), std::memory_order_relaxed);
		if (hwnd) g_textInputWindow = hwnd;
		engageTextContext();
		if (msg == WM_IME_SETCONTEXT || msg == WM_IME_STARTCOMPOSITION
			|| msg == WM_IME_COMPOSITION || msg == WM_IME_ENDCOMPOSITION) {
			handleImm32Message(hwnd, msg, lParam);
			return true;
		}
		if (msg == WM_CHAR) {
			if (isImeKeyboardLayout()
				&& !g_textInputBridgeEngaged.load(std::memory_order_acquire)
				&& !isTextInputComposing()) {
				const wchar_t character = static_cast<wchar_t>(wParam);
				if (character >= 0x20 && character != 0x7F)
					queueText(std::wstring(1, character), false);
			}
			return true;
		}
		if (msg == WM_IME_CHAR) {
			if (!g_textInputBridgeEngaged.load(std::memory_order_acquire)) {
				const wchar_t character = static_cast<wchar_t>(wParam);
				if (character >= 0x20 && character != 0x7F)
					queueText(std::wstring(1, character), false);
			}
			return true;
		}
		return false;
	}

	void drainTextInput() {
		std::vector<std::string> batch;
		{
			std::lock_guard lock(g_pendingTextMutex);
			batch.swap(g_pendingText);
		}
		if (batch.empty()) return;
#ifdef ALPHA_BOOTSTRAP
		if (!ImGui::GetCurrentContext()) return;
		for (const std::string& text : batch) ImGui::GetIO().AddInputCharactersUTF8(text.c_str());
#else
		ClickGui* clickGui = ModuleManager::getModule<ClickGui>();
		if (!clickGui) return;
		HWND deliveryWindow = g_textInputWindow;
		if (!deliveryWindow) deliveryWindow = g_gameWindow;
		for (const std::string& text : batch) {
			if (!deliveryWindow) {
				clickGui->appendTextInput(text);
				continue;
			}
			auto* pending = new std::string(text);
			if (!PostMessageW(deliveryWindow, WM_ALPHA_TEXT_INPUT, 0,
					reinterpret_cast<LPARAM>(pending))) {
				delete pending;
				clickGui->appendTextInput(text);
			}
		}
#endif
	}

	void syncTextInputMirror(const std::string& text) {
		if (!isTextInputActive()) return;
		const std::wstring replacement = utf8ToWide(text);
		int32_t oldLength = 0;
		{
			std::lock_guard lock(g_textMirrorMutex);
			oldLength = static_cast<int32_t>(g_textMirror.size());
			g_textMirror = replacement;
			g_textCompositionRange = CoreTextRange{ -1, -1 };
		}
		if (g_textContext && g_textInputBridgeEngaged.load(std::memory_order_acquire)) {
			const int32_t newLength = static_cast<int32_t>(replacement.size());
			if (onWindowThread()) {
				try {
					g_textContext.NotifyTextChanged(CoreTextRange{ 0, oldLength }, 0,
						CoreTextRange{ 0, newLength });
				} catch (...) {}
			} else {
				postTextSyncTask(false, oldLength, newLength);
			}
		}
	}

	void runTextSyncTask(TextSyncTask* task) {
		if (!task) return;
		struct TaskCleaner {
			TextSyncTask* pointer;
			~TaskCleaner() { delete pointer; }
		} cleaner{ task };
		if (!g_textContext || !g_textInputBridgeEngaged.load(std::memory_order_acquire)) return;
		try {
			if (task->focusLeave) {
				g_textContext.NotifyFocusLeave();
				g_textInputBridgeEngaged.store(false, std::memory_order_release);
			} else {
				g_textContext.NotifyTextChanged(CoreTextRange{ 0, task->oldLength }, 0,
					CoreTextRange{ 0, task->newLength });
			}
		} catch (...) {}
	}
}
