#pragma once
#include <Mojangles.h>
#include <MiSans.h>
#include <NotoSans.h>
#include <Windows.h>
#include <imfx.h>
#include <impl/imgui_impl_dx11.h>
#include <impl/imgui_impl_dx12.h>
#include <impl/imgui_impl_win32.h>

#include "Client/Client.h"
#include "../FuncHook.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include "Renderer/ImGuiUtils.h"
#include "Renderer/DrawUtil.h"
#include "sdk/Render/Camera.h"
#include "sdk/Game.h"
#include "Client/Managers/ModuleManager/ModuleManager.h"
#include "Client/Managers/CommandManager/CommandManager.h"
#include "Client/Managers/ModuleManager/Modules/Client/CustomFont.h"
#include "Client/Managers/ModuleManager/Modules/Client/ClickGui.h"
#include "Client/Managers/ModuleManager/Modules/Client/HudEditor.h"
#include "Client/Managers/ModuleManager/Modules/Client/TabList.h"
#include "Client/Managers/ModuleManager/Modules/ModuleTranslations.hpp"
#include "RenderFrameCounter.h"
#include "Utils/ImGuiInput.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
    LPARAM lParam);

class PresentHook : public FuncHook {
public:
	class CallbackGuard {
	public:
		CallbackGuard() : allowed(enterCallback()) {}
		~CallbackGuard() { leaveCallback(); }
		CallbackGuard(const CallbackGuard&) = delete;
		CallbackGuard& operator=(const CallbackGuard&) = delete;
		explicit operator bool() const { return allowed; }
	private:
		bool allowed;
	};

	static void beginShutdown() {
		{
			std::lock_guard<std::mutex> lock(callbackMutex);
			callbacksShuttingDown = true;
		}
		if (originalWndProc && hookedWindow) {
			if (const UINT message = windowTitleMessage.load(std::memory_order_acquire)) {
				DWORD_PTR ignored = 0;
				SendMessageTimeoutW(hookedWindow, message, kRestoreWindowTitle, 0,
					SMTO_ABORTIFHUNG | SMTO_BLOCK | SMTO_ERRORONEXIT, 250, &ignored);
			}
			SetWindowLongPtr(hookedWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalWndProc));
			DWORD_PTR ignored = 0;
			SendMessageTimeout(hookedWindow, WM_NULL, 0, 0,
				SMTO_ABORTIFHUNG | SMTO_BLOCK, 1000, &ignored);
		}
	}

	static void waitForCallbacks() {
		std::unique_lock<std::mutex> lock(callbackMutex);
		callbackCv.wait(lock, [] { return activeCallbacks == 0; });
	}

private:
    using present_t = HRESULT(__thiscall*)(IDXGISwapChain3*, UINT, UINT);
	static inline present_t oPresent;
	static inline std::mutex callbackMutex;
	static inline std::condition_variable callbackCv;
	static inline bool callbacksShuttingDown = false;
	static inline size_t activeCallbacks = 0;

	static bool enterCallback() {
		std::lock_guard<std::mutex> lock(callbackMutex);
		++activeCallbacks;
		return !callbacksShuttingDown;
	}

	static void leaveCallback() {
		{
			std::lock_guard<std::mutex> lock(callbackMutex);
			if (activeCallbacks > 0)
				--activeCallbacks;
		}
		callbackCv.notify_all();
	}

    static inline WNDPROC originalWndProc = nullptr;
	static inline HWND hookedWindow = nullptr;
    static constexpr wchar_t kClientWindowTitle[] = L"Alpha 26.51";
    static constexpr WPARAM kApplyWindowTitle = 1;
	static constexpr WPARAM kRestoreWindowTitle = 2;
	static inline std::atomic<UINT> windowTitleMessage{ 0 };
	static inline std::atomic<HWND> titleRequestWindow{ nullptr };
	static inline HWND titledWindow = nullptr;
	static inline std::wstring originalWindowTitle;

	static bool canAccessWindowTitle(HWND window) {
		DWORD process = 0;
		const DWORD thread = GetWindowThreadProcessId(window, &process);
		return thread != 0 && (thread == GetCurrentThreadId() || process != GetCurrentProcessId());
	}

	static std::wstring readWindowTitleOnWindowThread(HWND window) {
		std::wstring title(1024, L'\0');
		const int copied = GetWindowTextW(window, title.data(), static_cast<int>(title.size()));
		title.resize(static_cast<size_t>(copied));
		return title;
	}

	static bool setWindowTitleOnWindowThread(HWND window, const wchar_t* title) {
		if (GetWindowThreadProcessId(window, nullptr) == GetCurrentThreadId())
			return SetWindowTextW(window, title) != FALSE;
		DWORD_PTR result = 0;
		return SendMessageTimeoutW(window, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(title),
			SMTO_ABORTIFHUNG | SMTO_BLOCK | SMTO_ERRORONEXIT, 100, &result) != 0 && result != 0;
	}

	static void applyWindowTitleOnWindowThread(HWND window) {
		if (!window || !IsWindow(window)) return;
		HWND root = GetAncestor(window, GA_ROOT);
		if (!root) root = window;
		if (!canAccessWindowTitle(root)) return;
		const wchar_t* desired = kClientWindowTitle;
		if (root == titledWindow) {
			if (readWindowTitleOnWindowThread(root) != desired)
				setWindowTitleOnWindowThread(root, desired);
			return;
		}
		const std::wstring previousTitle = readWindowTitleOnWindowThread(root);
		if (setWindowTitleOnWindowThread(root, desired)) {
			originalWindowTitle = previousTitle;
			titledWindow = root;
		}
	}

	static void restoreWindowTitleOnWindowThread() {
		if (titledWindow && IsWindow(titledWindow) && canAccessWindowTitle(titledWindow)) {
			const std::wstring current = readWindowTitleOnWindowThread(titledWindow);
			if (current == kClientWindowTitle)
				setWindowTitleOnWindowThread(titledWindow, originalWindowTitle.c_str());
		}
	}

	static void queueWindowTitle() {
		const HWND window = hookedWindow;
		if (!originalWndProc || !window || !IsWindow(window)) return;
		UINT message = windowTitleMessage.load(std::memory_order_acquire);
		if (!message) {
			message = RegisterWindowMessageW(L"Alpha.Client.WindowTitle.v1");
			if (!message) return;
			windowTitleMessage.store(message, std::memory_order_release);
		}
		if (titleRequestWindow.exchange(window, std::memory_order_acq_rel) == window) return;
		if (!PostMessageW(window, message, kApplyWindowTitle, 0))
			titleRequestWindow.store(nullptr, std::memory_order_release);
	}
	static constexpr UINT D3D12_MAX_FRAMES = 3;
	static constexpr UINT D3D12_MAX_SRV_DESCRIPTORS = 512;
	static constexpr UINT D3D12_TRANSIENT_SRV_DESCRIPTORS = 256;
	static inline ID3D12CommandQueue* d3d12CommandQueue = nullptr;
	static inline ID3D12Device* d3d12Device = nullptr;
	static inline ID3D12DescriptorHeap* d3d12SrvHeap = nullptr;
	static inline ID3D12DescriptorHeap* d3d12RtvHeap = nullptr;
	static inline ID3D12CommandAllocator* d3d12CommandAllocators[D3D12_MAX_FRAMES] = {};
	static inline ID3D12GraphicsCommandList* d3d12CommandList = nullptr;
	static inline ID3D12Resource* d3d12BackBuffers[D3D12_MAX_FRAMES] = {};
	static inline D3D12_CPU_DESCRIPTOR_HANDLE d3d12RtvHandles[D3D12_MAX_FRAMES] = {};
	static inline ID3D12Fence* d3d12Fence = nullptr;
	static inline UINT64 d3d12FenceValue = 0;
	static inline UINT64 d3d12FrameFenceValues[D3D12_MAX_FRAMES] = {};
	static inline HANDLE d3d12FenceEvent = nullptr;
	static inline UINT d3d12FrameCount = 0;
	static inline UINT d3d12SrvHeapIndex = 0;
	static inline UINT d3d12PersistentSrvIndex = D3D12_TRANSIENT_SRV_DESCRIPTORS;
	static inline std::vector<UINT> d3d12FreePersistentSrvSlots;
	static inline bool d3d12Initialized = false;
	static inline bool d3d12Initializing = false;
	static inline bool d3d11Initialized = false;
	static inline bool d3d11ContextInitialized = false;
	static inline ID3D11Device* d3d11DeviceRef = nullptr;
	static inline ID3D11DeviceContext* d3d11Context = nullptr;
	static inline ID3D11Device* d3d12D3D11Device = nullptr;
	static inline ID3D11DeviceContext* d3d12D3D11Context = nullptr;
	static inline ID3D11On12Device* d3d12D3D11On12 = nullptr;
	static inline ID3D11Resource* d3d12WrappedBackBuffers[D3D12_MAX_FRAMES] = {};

	enum class UserTextureBackend : uint8_t {
		D3D11,
		D3D12
	};

	struct UserTextureResource {
		UserTextureBackend backend = UserTextureBackend::D3D11;
		ImTextureID textureId = ImTextureID_Invalid;
		ID3D11Texture2D* d3d11Texture = nullptr;
		ID3D11ShaderResourceView* d3d11View = nullptr;
		ID3D12Resource* d3d12Texture = nullptr;
		ID3D12Resource* d3d12Upload = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE d3d12CpuHandle{};
		D3D12_GPU_DESCRIPTOR_HANDLE d3d12GpuHandle{};
		UINT d3d12Slot = 0;
		uint64_t generation = 0;
	};

	static inline std::unordered_map<uint64_t, UserTextureResource> userTextures;
	static inline uint64_t nextUserTextureHandle = 1;
	static inline uint64_t userTextureGeneration = 1;

    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		CallbackGuard callback;
		const UINT titleMessage = windowTitleMessage.load(std::memory_order_acquire);
		if (titleMessage && msg == titleMessage) {
			if (wParam == kRestoreWindowTitle) restoreWindowTitleOnWindowThread();
			else if (callback && wParam == kApplyWindowTitle) applyWindowTitleOnWindowThread(hWnd);
			return 0;
		}
		if (msg == ImGuiInput::WM_ALPHA_TEXT_INPUT) {
			if (auto* text = reinterpret_cast<std::string*>(lParam)) {
				if (callback) {
					if (ClickGui* clickGui = ModuleManager::getModule<ClickGui>())
						clickGui->appendTextInput(*text);
				}
				delete text;
			}
			return 0;
		}
		if (msg == ImGuiInput::WM_ALPHA_TEXT_SYNC) {
			ImGuiInput::runTextSyncTask(reinterpret_cast<ImGuiInput::TextSyncTask*>(lParam));
			return 0;
		}
		if (!callback)
			return originalWndProc ? CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam)
				: DefWindowProc(hWnd, msg, wParam, lParam);
        static bool suppressTextInputKeyUp = false;
        if (msg == WM_KILLFOCUS || (msg == WM_ACTIVATEAPP && wParam == FALSE)) {
            Game::setTextInputActive(false);
            suppressTextInputKeyUp = false;
        }

        const auto isOverlayOpen = [] {
            ClickGui* cg = ModuleManager::getModule<ClickGui>();
            HudEditor* he = ModuleManager::getModule<HudEditor>();
            return (cg && cg->isEnabled()) || (he && he->isEnabled());
        };
        const bool overlayOpen = isOverlayOpen();

        switch (msg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (!(lParam & 0x40000000)) {
                const int key = (int)wParam;
                const bool opensTextInput = !overlayOpen && !Game::isTextInputActive() &&
                    (key == 'T' || key == VK_RETURN || key == VK_OEM_2 || key == VK_DIVIDE);
                if (opensTextInput) {
                    Game::setTextInputActive(true);
                    CommandManager::resetChatInput();
                    suppressTextInputKeyUp = true;
                    break;
                }
                if (!overlayOpen && Game::isTextInputActive()) {
                    if (key == VK_ESCAPE || key == VK_RETURN) {
                        Game::setTextInputActive(false);
                        CommandManager::resetChatInput();
                        suppressTextInputKeyUp = true;
                    }
                    break;
                }
                ModuleManager::onKeyUpdate(key, true);
            }
            break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (suppressTextInputKeyUp &&
                ((int)wParam == 'T' || (int)wParam == VK_RETURN ||
                 (int)wParam == VK_OEM_2 || (int)wParam == VK_DIVIDE ||
                 (int)wParam == VK_ESCAPE)) {
                suppressTextInputKeyUp = false;
                break;
            }
            if (overlayOpen || !Game::isTextInputActive())
                ModuleManager::onKeyUpdate((int)wParam, false);
            break;
        case WM_RBUTTONDOWN:
            if (overlayOpen || !Game::isTextInputActive())
                ModuleManager::onKeyUpdate(VK_RBUTTON, true);
            break;
        case WM_RBUTTONUP:
            if (overlayOpen || !Game::isTextInputActive())
                ModuleManager::onKeyUpdate(VK_RBUTTON, false);
            break;
        }

		if (!overlayOpen && Game::isTextInputActive())
			CommandManager::handleChatInputMessage(msg, wParam);

        if (isOverlayOpen()) {
            const bool textMessage = ImGuiInput::handleTextInputMessage(hWnd, msg, wParam, lParam);
            const bool imeWindowMessage = msg == WM_CHAR || msg == WM_IME_CHAR
                || msg == WM_IME_SETCONTEXT || msg == WM_IME_STARTCOMPOSITION
                || msg == WM_IME_COMPOSITION || msg == WM_IME_ENDCOMPOSITION
                || msg == WM_IME_NOTIFY || msg == WM_IME_REQUEST;
            if (imeWindowMessage)
                return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
            if (!textMessage)
                ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
            switch (msg) {
            case WM_KEYDOWN: case WM_KEYUP:
            case WM_SYSKEYDOWN: case WM_SYSKEYUP: case WM_SYSCHAR:
            case WM_MOUSEMOVE: case WM_MOUSELEAVE:
            case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
            case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
            case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
                return 0;
            case WM_INPUT:
                return DefWindowProc(hWnd, msg, wParam, lParam);
            default:
                return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
            }
        }

        const bool tabListOpen = [] {
            TabList* tabList = ModuleManager::getModule<TabList>();
            return tabList && tabList->isEnabled() && tabList->getKeybind() == VK_TAB;
        }();
        if (tabListOpen && (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && wParam == VK_TAB)
            return true;

        if (msg < WM_MOUSEFIRST || msg > WM_MOUSELAST)
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
        return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
    }

	static void applyCustomFont() {
		ImGuiIO& io = ImGui::GetIO();
		CustomFont* cFont = ModuleManager::getModule<CustomFont>();
		ImGuiUtils::setFontStyle(cFont ? cFont->fontStyle : 0);
		float presetScale = 1.f;
		if (cFont) {
			switch (std::clamp(cFont->sizePreset, 0, 3)) {
			case 0: presetScale = 0.92f; break;
			case 1: presetScale = 1.00f; break;
			case 2: presetScale = 1.16f; break;
			case 3: presetScale = 0.82f; break;
			}
		}
		ImGuiUtils::setFontScale(cFont
			? presetScale * (std::clamp(cFont->fontSize, 0.8f, 2.f) / 1.3f)
			: 1.f);
		if (!cFont) {
			ImGuiUtils::setFontSize(25.f);
			ImGuiUtils::setShadowOffset(1.f);
			return;
		}
		const int fontIndex = std::clamp(cFont->fontType, 0, 3);
		if (io.Fonts->Fonts.Size > fontIndex)
			io.FontDefault = io.Fonts->Fonts[fontIndex];
		switch (fontIndex) {
		case 0: {
			ImGuiUtils::setFontSize(22.f);
			ImGuiUtils::setShadowOffset(2.f);
			break;
		}
		case 1: {
			ImGuiUtils::setFontSize(25.f);
			ImGuiUtils::setShadowOffset(2.f);
			break;
		}
		case 2: {
			ImGuiUtils::setFontSize(25.f);
			ImGuiUtils::setShadowOffset(1.f);
			break;
		}
		case 3: {
			ImGuiUtils::setFontSize(25.f);
			ImGuiUtils::setShadowOffset(1.f);
			break;
		}
		}
	}

	static void mergeCjkFont(ImGuiIO& io, float sizePixels) {
		const ImWchar* cjkRanges = reinterpret_cast<const ImWchar*>(getTranslationGlyphRanges());
		const float cjkSizePixels = sizePixels * 1.448f;
		ImFontConfig cjkCfg;
		cjkCfg.MergeMode = true;
		cjkCfg.RasterizerDensity = 1.f;
		cjkCfg.OversampleH = 1;
		cjkCfg.OversampleV = 1;
		cjkCfg.PixelSnapH = true;
		cjkCfg.PixelSnapV = true;
		cjkCfg.RasterizerMultiply = 1.15f;
		const int resourceId = 102;
		HMODULE module = nullptr;
		GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCWSTR)&mergeCjkFont, &module);
		HRSRC resource = module ? FindResourceW(module, MAKEINTRESOURCEW(resourceId), MAKEINTRESOURCEW(10)) : nullptr;
		HGLOBAL loaded = resource ? LoadResource(module, resource) : nullptr;
		void* data = loaded ? LockResource(loaded) : nullptr;
		const int dataSize = resource ? (int)SizeofResource(module, resource) : 0;
		if (!data || dataSize <= 0) return;

		cjkCfg.FontDataOwnedByAtlas = false;
		io.Fonts->AddFontFromMemoryTTF(data, dataSize, cjkSizePixels, &cjkCfg, cjkRanges);
	}

	static void addCryoCategoryIconFont(ImGuiIO& io) {
		HMODULE module = nullptr;
		GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCWSTR)&addCryoCategoryIconFont, &module);
		HRSRC resource = module ? FindResourceW(module, MAKEINTRESOURCEW(103), MAKEINTRESOURCEW(10)) : nullptr;
		HGLOBAL loaded = resource ? LoadResource(module, resource) : nullptr;
		void* data = loaded ? LockResource(loaded) : nullptr;
		const int dataSize = resource ? (int)SizeofResource(module, resource) : 0;
		if (!data || dataSize <= 0) {
			ClickGui::setCategoryIconFont(nullptr);
			return;
		}

		static const ImWchar iconRanges[] = { 'B', 'B', 'a', 'g', 0 };
		ImFontConfig iconConfig;
		iconConfig.FontDataOwnedByAtlas = false;
		iconConfig.RasterizerDensity = 3.f;
		iconConfig.OversampleH = 2;
		iconConfig.OversampleV = 2;
		iconConfig.PixelSnapH = false;
		iconConfig.PixelSnapV = false;
		ImFont* iconFont = io.Fonts->AddFontFromMemoryTTF(data, dataSize, 100.f,
			&iconConfig, iconRanges);
		ClickGui::setCategoryIconFont(iconFont);
	}

	static void addFontCombinations(ImGuiIO& io) {
		ImFontConfig fontConfig;
		fontConfig.FontDataOwnedByAtlas = false;
		fontConfig.OversampleH = 1;
		fontConfig.OversampleV = 1;
		fontConfig.PixelSnapH = true;
		fontConfig.PixelSnapV = true;
		fontConfig.RasterizerDensity = 5.f;
		fontConfig.RasterizerMultiply = 1.28f;
		io.Fonts->AddFontFromMemoryTTF((void*)Mojangles, sizeof(Mojangles), 100.f, &fontConfig);
		mergeCjkFont(io, 100.f);

		fontConfig.RasterizerDensity = 1.f;
		fontConfig.OversampleH = 2;
		fontConfig.OversampleV = 2;
		fontConfig.PixelSnapH = false;
		fontConfig.PixelSnapV = false;
		fontConfig.RasterizerMultiply = 1.15f;
		io.Fonts->AddFontFromMemoryTTF((void*)NotoSans, sizeof(NotoSans), 50.f, &fontConfig);
		mergeCjkFont(io, 50.f);

		HMODULE module = nullptr;
		GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCWSTR)&addFontCombinations, &module);
		HRSRC resource = module ? FindResourceW(module, MAKEINTRESOURCEW(106), MAKEINTRESOURCEW(10)) : nullptr;
		HGLOBAL loaded = resource ? LoadResource(module, resource) : nullptr;
		void* wenKaiData = loaded ? LockResource(loaded) : nullptr;
		const int wenKaiSize = resource ? (int)SizeofResource(module, resource) : 0;
		fontConfig.RasterizerDensity = 2.f;
		fontConfig.OversampleH = 2;
		fontConfig.OversampleV = 2;
		fontConfig.PixelSnapH = false;
		fontConfig.PixelSnapV = false;
		fontConfig.RasterizerMultiply = 1.12f;
		if (wenKaiData && wenKaiSize > 0) {
			io.Fonts->AddFontFromMemoryTTF(wenKaiData, wenKaiSize, 50.f, &fontConfig);
			ImFontConfig wenKaiCjkConfig = fontConfig;
			wenKaiCjkConfig.MergeMode = true;
			io.Fonts->AddFontFromMemoryTTF(wenKaiData, wenKaiSize, 50.f,
				&wenKaiCjkConfig,
				reinterpret_cast<const ImWchar*>(getTranslationGlyphRanges()));
		}
		else {
			io.Fonts->AddFontFromMemoryTTF((void*)Mojangles, sizeof(Mojangles), 100.f, &fontConfig);
			mergeCjkFont(io, 100.f);
		}

		fontConfig.RasterizerDensity = 1.f;
		fontConfig.OversampleH = 2;
		fontConfig.OversampleV = 2;
		fontConfig.PixelSnapH = false;
		fontConfig.PixelSnapV = false;
		fontConfig.RasterizerMultiply = 1.15f;
		io.Fonts->AddFontFromMemoryCompressedTTF(
			misans_ttf_compressed_data, misans_ttf_compressed_size, 50.f, &fontConfig);
		mergeCjkFont(io, 50.f);
		addCryoCategoryIconFont(io);
	}

	static void prepareStableFontBakes(ImGuiIO& io) {
		static constexpr float stableSizes[] = {
			6.f, 7.f, 8.f, 9.f, 10.f, 11.f, 12.f, 13.f, 14.f,
			15.f, 16.f, 17.f, 18.f, 19.f, 20.f, 21.f, 22.f, 23.f,
			24.f, 25.f, 26.f, 27.f, 28.f, 29.f, 30.f, 31.f, 32.f,
			48.f, 64.f, 96.f, 128.f
		};
		for (int i = 0; i < io.Fonts->Fonts.Size; ++i)
			for (float size : stableSizes)
				io.Fonts->Fonts[i]->GetFontBaked(size, 1.f);
		for (int i = 0; i < io.Fonts->Fonts.Size; ++i)
			io.Fonts->Fonts[i]->Flags |= ImFontFlags_LockBakedSizes;
	}

	static DXGI_FORMAT getD3D12RenderFormat(DXGI_FORMAT format) {
		return format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ? DXGI_FORMAT_R8G8B8A8_UNORM : format;
	}

	static void allocateD3D12SrvDescriptor(ImGui_ImplDX12_InitInfo* initInfo, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle) {
		if (!d3d12SrvHeap) {
			*outCpuHandle = {};
			*outGpuHandle = {};
			return;
		}
		const UINT increment = initInfo->Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		const UINT slot = (d3d12SrvHeapIndex++) % D3D12_TRANSIENT_SRV_DESCRIPTORS;
		*outCpuHandle = d3d12SrvHeap->GetCPUDescriptorHandleForHeapStart();
		*outGpuHandle = d3d12SrvHeap->GetGPUDescriptorHandleForHeapStart();
		outCpuHandle->ptr += (SIZE_T)slot * increment;
		outGpuHandle->ptr += (SIZE_T)slot * increment;
	}

	static void freeD3D12SrvDescriptor(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {
	}

	static void freePersistentD3D12SrvDescriptor(UINT slot) {
		if (slot >= D3D12_TRANSIENT_SRV_DESCRIPTORS && slot < D3D12_MAX_SRV_DESCRIPTORS)
			d3d12FreePersistentSrvSlots.push_back(slot);
	}

	static bool allocatePersistentD3D12SrvDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle, UINT& outSlot) {
		if (!d3d12Device || !d3d12SrvHeap || !outCpuHandle || !outGpuHandle)
			return false;

		if (!d3d12FreePersistentSrvSlots.empty()) {
			outSlot = d3d12FreePersistentSrvSlots.back();
			d3d12FreePersistentSrvSlots.pop_back();
		}
		else {
			if (d3d12PersistentSrvIndex >= D3D12_MAX_SRV_DESCRIPTORS)
				return false;
			outSlot = d3d12PersistentSrvIndex++;
		}

		const UINT increment = d3d12Device->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		*outCpuHandle = d3d12SrvHeap->GetCPUDescriptorHandleForHeapStart();
		*outGpuHandle = d3d12SrvHeap->GetGPUDescriptorHandleForHeapStart();
		outCpuHandle->ptr += static_cast<SIZE_T>(outSlot) * increment;
		outGpuHandle->ptr += static_cast<UINT64>(outSlot) * increment;
		if (outCpuHandle->ptr == 0 || outGpuHandle->ptr == 0) {
			freePersistentD3D12SrvDescriptor(outSlot);
			*outCpuHandle = {};
			*outGpuHandle = {};
			return false;
		}
		return true;
	}

	static void releaseUserTextureResource(UserTextureResource& texture) {
		if (texture.backend == UserTextureBackend::D3D11) {
			if (texture.d3d11View) {
				texture.d3d11View->Release();
				texture.d3d11View = nullptr;
			}
			if (texture.d3d11Texture) {
				texture.d3d11Texture->Release();
				texture.d3d11Texture = nullptr;
			}
		}
		else {
			if (texture.d3d12Upload) {
				texture.d3d12Upload->Release();
				texture.d3d12Upload = nullptr;
			}
			if (texture.d3d12Texture) {
				texture.d3d12Texture->Release();
				texture.d3d12Texture = nullptr;
			}
			if (texture.d3d12CpuHandle.ptr != 0)
				freePersistentD3D12SrvDescriptor(texture.d3d12Slot);
			texture.d3d12CpuHandle = {};
			texture.d3d12GpuHandle = {};
		}
		texture.textureId = ImTextureID_Invalid;
	}

	static void releaseAllUserTextures() {
		for (auto& entry : userTextures)
			releaseUserTextureResource(entry.second);
		userTextures.clear();

		++userTextureGeneration;
		if (userTextureGeneration == 0)
			userTextureGeneration = 1;
	}

	static bool createD3D11UserTexture(const unsigned char* rgba, UINT width, UINT height,
		UserTextureResource& out) {
		if (!d3d11DeviceRef || !rgba || width == 0 || height == 0)
			return false;

		D3D11_TEXTURE2D_DESC textureDesc{};
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA subresource{};
		subresource.pSysMem = rgba;
		subresource.SysMemPitch = width * 4u;
		if (FAILED(d3d11DeviceRef->CreateTexture2D(&textureDesc, &subresource,
			&out.d3d11Texture)) || !out.d3d11Texture)
			return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = textureDesc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		if (FAILED(d3d11DeviceRef->CreateShaderResourceView(out.d3d11Texture, &viewDesc,
			&out.d3d11View)) || !out.d3d11View) {
			out.d3d11Texture->Release();
			out.d3d11Texture = nullptr;
			return false;
		}

		out.backend = UserTextureBackend::D3D11;
		out.textureId = static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(out.d3d11View));
		out.generation = userTextureGeneration;
		return true;
	}

	static bool createD3D12UserTexture(const unsigned char* rgba, UINT width, UINT height,
		UserTextureResource& out) {
		if (!d3d12Device || !d3d12CommandList || !d3d12SrvHeap || !rgba || width == 0 || height == 0)
			return false;
		out.backend = UserTextureBackend::D3D12;

		if (!allocatePersistentD3D12SrvDescriptor(&out.d3d12CpuHandle, &out.d3d12GpuHandle,
			out.d3d12Slot))
			return false;

		D3D12_RESOURCE_DESC textureDesc{};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

		D3D12_HEAP_PROPERTIES defaultHeap{};
		defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
		if (FAILED(d3d12Device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE,
			&textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&out.d3d12Texture))) ||
			!out.d3d12Texture) {
			freePersistentD3D12SrvDescriptor(out.d3d12Slot);
			out.d3d12CpuHandle = {};
			out.d3d12GpuHandle = {};
			return false;
		}

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
		UINT rowCount = 0;
		UINT64 rowSize = 0;
		UINT64 uploadSize = 0;
		d3d12Device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &rowCount,
			&rowSize, &uploadSize);
		if (uploadSize == 0 || rowCount < height || footprint.Footprint.RowPitch < width * 4u) {
			releaseUserTextureResource(out);
			return false;
		}

		D3D12_RESOURCE_DESC uploadDesc{};
		uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		uploadDesc.Width = uploadSize;
		uploadDesc.Height = 1;
		uploadDesc.DepthOrArraySize = 1;
		uploadDesc.MipLevels = 1;
		uploadDesc.SampleDesc.Count = 1;
		uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		D3D12_HEAP_PROPERTIES uploadHeap{};
		uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
		if (FAILED(d3d12Device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE,
			&uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&out.d3d12Upload))) ||
			!out.d3d12Upload) {
			releaseUserTextureResource(out);
			return false;
		}

		void* mapped = nullptr;
		D3D12_RANGE readRange{ 0, 0 };
		if (FAILED(out.d3d12Upload->Map(0, &readRange, &mapped)) || !mapped) {
			releaseUserTextureResource(out);
			return false;
		}
		for (UINT y = 0; y < height; ++y) {
			std::memcpy(static_cast<unsigned char*>(mapped) + footprint.Offset +
				static_cast<size_t>(y) * footprint.Footprint.RowPitch,
				rgba + static_cast<size_t>(y) * width * 4u, width * 4u);
		}
		D3D12_RANGE writtenRange{ 0, static_cast<SIZE_T>(uploadSize) };
		out.d3d12Upload->Unmap(0, &writtenRange);

		D3D12_TEXTURE_COPY_LOCATION source{};
		source.pResource = out.d3d12Upload;
		source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		source.PlacedFootprint = footprint;
		D3D12_TEXTURE_COPY_LOCATION destination{};
		destination.pResource = out.d3d12Texture;
		destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		destination.SubresourceIndex = 0;
		d3d12CommandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = out.d3d12Texture;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		d3d12CommandList->ResourceBarrier(1, &barrier);

		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		d3d12Device->CreateShaderResourceView(out.d3d12Texture, &viewDesc, out.d3d12CpuHandle);

		out.textureId = static_cast<ImTextureID>(out.d3d12GpuHandle.ptr);
		out.generation = userTextureGeneration;
		if (out.textureId == ImTextureID_Invalid) {
			releaseUserTextureResource(out);
			return false;
		}
		return true;
	}

	static bool waitForD3D12Value(UINT64 value) {
		if (!d3d12Fence || !d3d12CommandQueue)
			return false;

		if (d3d12Fence->GetCompletedValue() >= value)
			return true;

		if (!d3d12FenceEvent) {
			d3d12FenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
			if (!d3d12FenceEvent)
				return false;
		}

		if (FAILED(d3d12Fence->SetEventOnCompletion(value, d3d12FenceEvent)))
			return false;

		WaitForSingleObject(d3d12FenceEvent, INFINITE);
		return true;
	}

	static bool waitForD3D12Gpu() {
		return waitForD3D12Value(d3d12FenceValue);
	}

	static bool waitForD3D12FrameSlot(UINT frameIndex) {
		if (frameIndex >= D3D12_MAX_FRAMES)
			return false;
		return waitForD3D12Value(d3d12FrameFenceValues[frameIndex]);
	}

	static bool signalD3D12Fence() {
		if (!d3d12Fence || !d3d12CommandQueue)
			return false;

		++d3d12FenceValue;
		return SUCCEEDED(d3d12CommandQueue->Signal(d3d12Fence, d3d12FenceValue));
	}

	static bool createD3D12BlurBridge() {
		if (d3d12D3D11Device && d3d12D3D11Context && d3d12D3D11On12)
			return true;
		if (!d3d12Device || !d3d12CommandQueue || d3d12FrameCount == 0)
			return false;

		IUnknown* queues[] = { d3d12CommandQueue };
		ID3D11Device* device11 = nullptr;
		ID3D11DeviceContext* context11 = nullptr;
		ID3D11On12Device* on12 = nullptr;
		const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED;
		HRESULT hr = D3D11On12CreateDevice(d3d12Device, flags, nullptr, 0, queues, 1, 0,
			&device11, &context11, nullptr);
		if (FAILED(hr) || !device11 || !context11) {
			if (context11) context11->Release();
			if (device11) device11->Release();
			return false;
		}
		hr = device11->QueryInterface(IID_PPV_ARGS(&on12));
		if (FAILED(hr) || !on12) {
			context11->Release();
			device11->Release();
			return false;
		}

		d3d12D3D11Device = device11;
		d3d12D3D11Context = context11;
		d3d12D3D11On12 = on12;

		D3D11_RESOURCE_FLAGS resourceFlags = {};
		resourceFlags.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		for (UINT i = 0; i < d3d12FrameCount; ++i) {
			if (!d3d12BackBuffers[i] || FAILED(d3d12D3D11On12->CreateWrappedResource(
				d3d12BackBuffers[i], &resourceFlags,
				D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT,
				IID_PPV_ARGS(&d3d12WrappedBackBuffers[i])))) {
				for (UINT j = 0; j <= i && j < D3D12_MAX_FRAMES; ++j) {
					if (d3d12WrappedBackBuffers[j]) {
						d3d12WrappedBackBuffers[j]->Release();
						d3d12WrappedBackBuffers[j] = nullptr;
					}
				}
				d3d12D3D11On12->Release();
				d3d12D3D11On12 = nullptr;
				d3d12D3D11Context->Release();
				d3d12D3D11Context = nullptr;
				d3d12D3D11Device->Release();
				d3d12D3D11Device = nullptr;
				return false;
			}
		}
		return true;
	}

	static void releaseD3D12BlurBridge() {
		ImFX::CleanupFX();
		if (d3d12D3D11Context)
			d3d12D3D11Context->Flush();
		for (UINT i = 0; i < D3D12_MAX_FRAMES; ++i) {
			if (d3d12WrappedBackBuffers[i]) {
				d3d12WrappedBackBuffers[i]->Release();
				d3d12WrappedBackBuffers[i] = nullptr;
			}
		}
		if (d3d12D3D11On12) {
			d3d12D3D11On12->Release();
			d3d12D3D11On12 = nullptr;
		}
		if (d3d12D3D11Context) {
			d3d12D3D11Context->Release();
			d3d12D3D11Context = nullptr;
		}
		if (d3d12D3D11Device) {
			d3d12D3D11Device->Release();
			d3d12D3D11Device = nullptr;
		}
	}

	static bool renderD3D12Blur(UINT frameIndex, HWND window) {
		ClickGui* clickGui = ModuleManager::getModule<ClickGui>();
		if (!clickGui || !clickGui->isEnabled())
			return false;
		const float strength = std::clamp(clickGui->getBgBlur(), 0.0f, 20.0f);
		if (strength <= 0.01f || frameIndex >= D3D12_MAX_FRAMES ||
			!d3d12WrappedBackBuffers[frameIndex] || !d3d12D3D11Device ||
			!d3d12D3D11Context || !d3d12D3D11On12)
			return false;

		IDXGISurface* surface = nullptr;
		if (FAILED(d3d12WrappedBackBuffers[frameIndex]->QueryInterface(IID_PPV_ARGS(&surface))) || !surface)
			return false;

		ID3D11Resource* resource = d3d12WrappedBackBuffers[frameIndex];
		d3d12D3D11On12->AcquireWrappedResources(&resource, 1);
		const float dpi = static_cast<float>(GetDpiForWindow(window));
		const bool prepared = ImFX::NewFrame(d3d12D3D11Device, surface, dpi);
		const bool blurred = prepared && ImFX::ApplyBlur(strength,
			ImVec4(0.0f, 0.0f, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y), 0.0f);
		ImFX::EndFrame();
		surface->Release();
		d3d12D3D11On12->ReleaseWrappedResources(&resource, 1);
		d3d12D3D11Context->Flush();
		return blurred;
	}

	static bool renderD3D12GhostFrame(UINT frameIndex, HWND window) {
		if (frameIndex >= D3D12_MAX_FRAMES ||
			!d3d12WrappedBackBuffers[frameIndex] || !d3d12D3D11Device ||
			!d3d12D3D11Context || !d3d12D3D11On12)
			return false;

		IDXGISurface* surface = nullptr;
		if (FAILED(d3d12WrappedBackBuffers[frameIndex]->QueryInterface(IID_PPV_ARGS(&surface))) || !surface)
			return false;

		ID3D11Resource* resource = d3d12WrappedBackBuffers[frameIndex];
		d3d12D3D11On12->AcquireWrappedResources(&resource, 1);
		const float dpi = static_cast<float>(GetDpiForWindow(window));
		const bool done = ImFX::RenderQueuedGhostFrame(d3d12D3D11Device, surface, dpi);
		surface->Release();
		d3d12D3D11On12->ReleaseWrappedResources(&resource, 1);
		d3d12D3D11Context->Flush();
		return done;
	}

	static void releaseD3D12RenderResources() {
		if (d3d12Initialized)
			waitForD3D12Gpu();
		releaseD3D12BlurBridge();
		releaseAllUserTextures();

		if (d3d12Initialized && ImGui::GetCurrentContext()) {
			ImGui_ImplDX12_InvalidateDeviceObjects();
			ImGui_ImplDX12_Shutdown();
			ImGui_ImplWin32_Shutdown();
		}

		if (d3d12CommandList) {
			d3d12CommandList->Release();
			d3d12CommandList = nullptr;
		}

		for (UINT i = 0; i < D3D12_MAX_FRAMES; ++i) {
			if (d3d12BackBuffers[i]) {
				d3d12BackBuffers[i]->Release();
				d3d12BackBuffers[i] = nullptr;
			}
			if (d3d12CommandAllocators[i]) {
				d3d12CommandAllocators[i]->Release();
				d3d12CommandAllocators[i] = nullptr;
			}
		}

		if (d3d12RtvHeap) {
			d3d12RtvHeap->Release();
			d3d12RtvHeap = nullptr;
		}
		if (d3d12SrvHeap) {
			d3d12SrvHeap->Release();
			d3d12SrvHeap = nullptr;
		}
		if (d3d12Fence) {
			d3d12Fence->Release();
			d3d12Fence = nullptr;
		}
		if (d3d12FenceEvent) {
			CloseHandle(d3d12FenceEvent);
			d3d12FenceEvent = nullptr;
		}
		if (d3d12Device) {
			d3d12Device->Release();
			d3d12Device = nullptr;
		}

		d3d12FrameCount = 0;
		d3d12FenceValue = 0;
		for (UINT i = 0; i < D3D12_MAX_FRAMES; ++i)
			d3d12FrameFenceValues[i] = 0;
		d3d12SrvHeapIndex = 0;
		d3d12PersistentSrvIndex = D3D12_TRANSIENT_SRV_DESCRIPTORS;
		d3d12FreePersistentSrvSlots.clear();
		d3d12Initialized = false;
	}

	static bool createD3D12RenderResources(IDXGISwapChain3* swapChain, ID3D12Device* device, HWND window) {
		DXGI_SWAP_CHAIN_DESC swapDesc = {};
		if (FAILED(swapChain->GetDesc(&swapDesc)))
			return false;

		d3d12Device = device;
		d3d12Device->AddRef();

		d3d12FrameCount = swapDesc.BufferCount;
		if (d3d12FrameCount == 0 || d3d12FrameCount > D3D12_MAX_FRAMES)
			d3d12FrameCount = D3D12_MAX_FRAMES;

		D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
		srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvDesc.NumDescriptors = D3D12_MAX_SRV_DESCRIPTORS;
		srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		if (FAILED(device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&d3d12SrvHeap))))
			return false;

		D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
		rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvDesc.NumDescriptors = d3d12FrameCount;
		if (FAILED(device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&d3d12RtvHeap)))) {
			releaseD3D12RenderResources();
			return false;
		}

		UINT rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtvStart = d3d12RtvHeap->GetCPUDescriptorHandleForHeapStart();
		for (UINT i = 0; i < d3d12FrameCount; ++i) {
			if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&d3d12BackBuffers[i])))) {
				releaseD3D12RenderResources();
				return false;
			}

			d3d12RtvHandles[i] = rtvStart;
			d3d12RtvHandles[i].ptr += (SIZE_T)i * rtvSize;
			D3D12_RENDER_TARGET_VIEW_DESC rtvView = {};
			rtvView.Format = getD3D12RenderFormat(d3d12BackBuffers[i]->GetDesc().Format);
			rtvView.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			device->CreateRenderTargetView(d3d12BackBuffers[i], &rtvView, d3d12RtvHandles[i]);

			if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d12CommandAllocators[i])))) {
				releaseD3D12RenderResources();
				return false;
			}
		}

		createD3D12BlurBridge();

		if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12CommandAllocators[0], nullptr, IID_PPV_ARGS(&d3d12CommandList)))) {
			releaseD3D12RenderResources();
			return false;
		}
		d3d12CommandList->Close();

		if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12Fence)))) {
			releaseD3D12RenderResources();
			return false;
		}

		if (!ImGui::GetCurrentContext()) {
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
		}
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = nullptr;
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
		if (io.Fonts->Fonts.Size == 0) {
		addFontCombinations(io);
		prepareStableFontBakes(io);
	}

		HWND initWindow = swapDesc.OutputWindow ? swapDesc.OutputWindow : window;
		ImGui_ImplWin32_Init(initWindow);
		ImGuiInput::setGameWindow(initWindow);

		ImGui_ImplDX12_InitInfo initInfo;
		initInfo.Device = device;
		initInfo.CommandQueue = d3d12CommandQueue;
		initInfo.NumFramesInFlight = (int)d3d12FrameCount;
		initInfo.RTVFormat = getD3D12RenderFormat(swapDesc.BufferDesc.Format);
		initInfo.SrvDescriptorHeap = d3d12SrvHeap;
		initInfo.SrvDescriptorAllocFn = allocateD3D12SrvDescriptor;
		initInfo.SrvDescriptorFreeFn = freeD3D12SrvDescriptor;
		ImGui_ImplDX12_Init(&initInfo);
		ImGui_ImplDX12_CreateDeviceObjects();

		if (initWindow && !originalWndProc) {
			originalWndProc = (WNDPROC)SetWindowLongPtr(initWindow, GWLP_WNDPROC, (LONG_PTR)WndProc);
			hookedWindow = initWindow;
		}

		d3d12Initialized = true;
		return true;
	}

	static bool renderD3D12(IDXGISwapChain3* swapChain, ID3D12Device* device, HWND window) {
		if (!d3d12Initialized && !d3d12Initializing) {
			if (!d3d12CommandQueue) return false;

			d3d12Initializing = true;
			bool initialized = createD3D12RenderResources(swapChain, device, window);
			d3d12Initializing = false;
			if (!initialized) return false;
		}

		if (!d3d12Initialized)
			return false;

		UINT frameIndex = swapChain->GetCurrentBackBufferIndex() % d3d12FrameCount;
		if (!waitForD3D12FrameSlot(frameIndex))
			return false;
		if (FAILED(d3d12CommandAllocators[frameIndex]->Reset()))
			return false;
		if (FAILED(d3d12CommandList->Reset(d3d12CommandAllocators[frameIndex], nullptr)))
			return false;

		Camera::captureFrame();
		const D3D12_RESOURCE_DESC backBufferDesc = d3d12BackBuffers[frameIndex]->GetDesc();

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = d3d12BackBuffers[frameIndex];
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		d3d12CommandList->ResourceBarrier(1, &barrier);

		d3d12CommandList->OMSetRenderTargets(1, &d3d12RtvHandles[frameIndex], FALSE, nullptr);
		ID3D12DescriptorHeap* heaps[] = { d3d12SrvHeap };
		d3d12CommandList->SetDescriptorHeaps(1, heaps);

		ClickGui* clickGui = ModuleManager::getModule<ClickGui>();
		const bool blurRequested = clickGui && clickGui->isEnabled()
			&& clickGui->getBgBlur() > 0.01f
			&& d3d12WrappedBackBuffers[frameIndex]
			&& d3d12D3D11Device && d3d12D3D11Context && d3d12D3D11On12;
		if (blurRequested) {
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			d3d12CommandList->ResourceBarrier(1, &barrier);
			if (FAILED(d3d12CommandList->Close()))
				return false;
			ID3D12CommandList* backgroundLists[] = { d3d12CommandList };
			d3d12CommandQueue->ExecuteCommandLists(1, backgroundLists);
			if (!signalD3D12Fence() || !waitForD3D12Gpu())
				return false;
			if (FAILED(d3d12CommandAllocators[frameIndex]->Reset()) ||
				FAILED(d3d12CommandList->Reset(d3d12CommandAllocators[frameIndex], nullptr)))
				return false;

			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			d3d12CommandList->ResourceBarrier(1, &barrier);
			d3d12CommandList->OMSetRenderTargets(1, &d3d12RtvHandles[frameIndex], FALSE, nullptr);
			d3d12CommandList->SetDescriptorHeaps(1, heaps);
		}

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		renderD3D12Blur(frameIndex, window);
		if (ImFX::HasQueuedGhostFrame())
			renderD3D12GhostFrame(frameIndex, window);

		const bool wantsImGuiInput = [] {
			ClickGui* cg = ModuleManager::getModule<ClickGui>();
			HudEditor* he = ModuleManager::getModule<HudEditor>();
			return (cg && cg->isEnabled()) || (he && he->isEnabled());
		}();
        if (wantsImGuiInput) {
            ImGuiInput::beginOverlayInput();
            ImGuiInput::feedMouse();
            ImGuiInput::feedKeyboard();
            ImGuiInput::drainTextInput();
        }
		else {
			ImGuiInput::endOverlayInput();
		}

		applyCustomFont();
		ImGui::NewFrame();
		{
			ImGuiUtils::setDrawList(ImGui::GetBackgroundDrawList());
			DrawUtil::onImGuiWorldRender();
			ModuleManager::onImGuiRender(ImGui::GetBackgroundDrawList(),
				Game::getLocalPlayer() == nullptr);
			CommandManager::renderSuggestions(ImGui::GetForegroundDrawList());
			CommandManager::checkAutoSave();
		}

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3d12CommandList);

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		d3d12CommandList->ResourceBarrier(1, &barrier);
		d3d12CommandList->Close();

		ID3D12CommandList* commandLists[] = { d3d12CommandList };
		d3d12CommandQueue->ExecuteCommandLists(1, commandLists);
		if (!signalD3D12Fence())
			return false;
		d3d12FrameFenceValues[frameIndex] = d3d12FenceValue;
		return true;
	}

    static HRESULT presentCallback(IDXGISwapChain3* swapChain, UINT syncInterval, UINT flags) {
		CallbackGuard callback;
		if (!callback)
			return oPresent(swapChain, syncInterval, flags);
		RenderFrameCounter::serial.fetch_add(1, std::memory_order_release);
        if (!Client::isInitialized())
            return oPresent(swapChain, syncInterval, flags);

		static HWND window = nullptr;
		if (!window || !IsWindow(window)) {
			window = FindWindowW(nullptr, L"Minecraft");
			if (!window) window = FindWindowW(nullptr, kClientWindowTitle);
		}
		queueWindowTitle();

        ID3D12Device* d3d12Device = nullptr;
        ID3D11Device* d3d11Device = nullptr;

        if (SUCCEEDED(swapChain->GetDevice(IID_PPV_ARGS(&d3d12Device)))) {
            renderD3D12(swapChain, d3d12Device, window);
            d3d12Device->Release();
            return oPresent(swapChain, syncInterval, flags);
        }
		else if (SUCCEEDED(swapChain->GetDevice(IID_PPV_ARGS(&d3d11Device)))) {
			if (!d3d11ContextInitialized) {
				d3d11Device->AddRef();
				d3d11DeviceRef = d3d11Device;
				d3d11Device->GetImmediateContext(&d3d11Context);
				if (!d3d11DeviceRef || !d3d11Context) {
					if (d3d11Context) {
						d3d11Context->Release();
						d3d11Context = nullptr;
					}
					if (d3d11DeviceRef) {
						d3d11DeviceRef->Release();
						d3d11DeviceRef = nullptr;
					}
					d3d11Device->Release();
					return oPresent(swapChain, syncInterval, flags);
				}

				ImGui::CreateContext();
				d3d11Initialized = true;
                ImGuiIO& io = ImGui::GetIO();
                io.IniFilename = nullptr;

				addFontCombinations(io);
				prepareStableFontBakes(io);

				ImGui_ImplWin32_Init(window);
				ImGui_ImplDX11_Init(d3d11DeviceRef, d3d11Context);

                if (window && !originalWndProc) {
                    originalWndProc =
                        (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
                    hookedWindow = window;
                }

				d3d11ContextInitialized = true;
			}

            ID3D11Texture2D* pBackBuffer = nullptr;
            swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

            ID3D11RenderTargetView* mainRenderTargetView = nullptr;
            if (pBackBuffer) {
                d3d11Device->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
                pBackBuffer->Release();
            }

            IDXGISurface* dxgiBackBuffer = nullptr;
            if (SUCCEEDED(swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer)))) {
                ImFX::RenderQueuedGhostFrame(d3d11Device, dxgiBackBuffer, (float)GetDpiForWindow(window));
                ImFX::NewFrame(d3d11Device, dxgiBackBuffer, (float)GetDpiForWindow(window));
                dxgiBackBuffer->Release();
            }

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();

            const bool wantsImGuiInput = [] {
                ClickGui* cg = ModuleManager::getModule<ClickGui>();
                HudEditor* he = ModuleManager::getModule<HudEditor>();
                return (cg && cg->isEnabled()) || (he && he->isEnabled());
            }();
            if (wantsImGuiInput) {
                ImGuiInput::beginOverlayInput();
                ImGuiInput::feedMouse();
                ImGuiInput::feedKeyboard();
                ImGuiInput::drainTextInput();
            }
            else {
                ImGuiInput::endOverlayInput();
            }

			applyCustomFont();
			ImGui::NewFrame();

			{
				ImGuiUtils::setDrawList(ImGui::GetBackgroundDrawList());
				Camera::captureFrame();
				DrawUtil::onImGuiWorldRender();
				ModuleManager::onImGuiRender(ImGui::GetBackgroundDrawList(),
					Game::getLocalPlayer() == nullptr);
				CommandManager::renderSuggestions(ImGui::GetForegroundDrawList());
				CommandManager::checkAutoSave();
			}

            ImGui::EndFrame();
            ImGui::Render();

            if (mainRenderTargetView) {
				d3d11Context->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

                ImFX::EndFrame();

                mainRenderTargetView->Release();
            }

            d3d11Device->Release();
        }

        return oPresent(swapChain, syncInterval, flags);
    }

public:
    static bool createUserTexture(const unsigned char* rgba, UINT width, UINT height,
		uint64_t& handle, ImTextureID& textureId, uint64_t& generation) {
		if (!rgba || width == 0 || height == 0 || width > 1024 || height > 1024)
			return false;

		UserTextureResource resource{};
		bool created = false;
		if (d3d12Initialized)
			created = createD3D12UserTexture(rgba, width, height, resource);
		else if (d3d11Initialized)
			created = createD3D11UserTexture(rgba, width, height, resource);
		if (!created)
			return false;

		uint64_t newHandle = nextUserTextureHandle++;
		if (newHandle == 0)
			newHandle = nextUserTextureHandle++;
		resource.generation = userTextureGeneration;
		const auto [it, inserted] = userTextures.emplace(newHandle, resource);
		if (!inserted) {
			releaseUserTextureResource(resource);
			return false;
		}

		handle = newHandle;
		textureId = it->second.textureId;
		generation = it->second.generation;
		return textureId != ImTextureID_Invalid;
	}

    static void releaseUserTexture(uint64_t& handle, ImTextureID& textureId, uint64_t& generation) {
		if (handle != 0) {
			auto it = userTextures.find(handle);
			if (it != userTextures.end()) {
				releaseUserTextureResource(it->second);
				userTextures.erase(it);
			}
		}
		handle = 0;
		textureId = ImTextureID_Invalid;
		generation = 0;
	}

    static bool isUserTextureValid(uint64_t handle, ImTextureID textureId, uint64_t generation) {
		if (handle == 0 || textureId == ImTextureID_Invalid || generation != userTextureGeneration)
			return false;
		auto it = userTextures.find(handle);
		return it != userTextures.end() && it->second.generation == generation &&
			it->second.textureId == textureId;
	}

    static void setD3D12CommandQueue(ID3D12CommandQueue* queue) {
		if (!queue || d3d12CommandQueue)
			return;

		d3d12CommandQueue = queue;
		d3d12CommandQueue->AddRef();
	}

    static void invalidateD3D12() {
		releaseD3D12RenderResources();
	}

    static void invalidateD3D11() {
		if (!d3d11Initialized)
			return;
		releaseAllUserTextures();
	}

	static void shutdown() {
		beginShutdown();
		waitForCallbacks();
		titledWindow = nullptr;
		originalWindowTitle.clear();
		titleRequestWindow.store(nullptr, std::memory_order_release);
		ImGuiInput::shutdownTextInput();
		originalWndProc = nullptr;
		hookedWindow = nullptr;

		releaseD3D12RenderResources();

		if (d3d11Initialized && ImGui::GetCurrentContext()) {
			ImGui_ImplDX11_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImFX::CleanupFX();
			d3d11Initialized = false;
		}
		if (d3d11Context) {
			d3d11Context->Release();
			d3d11Context = nullptr;
		}
		if (d3d11DeviceRef) {
			d3d11DeviceRef->Release();
			d3d11DeviceRef = nullptr;
		}
		d3d11ContextInitialized = false;
		if (d3d12CommandQueue) {
			d3d12CommandQueue->Release();
			d3d12CommandQueue = nullptr;
		}

		ClickGui::setCategoryIconFont(nullptr);
		if (ImGui::GetCurrentContext())
			ImGui::DestroyContext();
	}

    PresentHook() {
        OriginFunc = (void*)&oPresent;
        func = (void*)&presentCallback;
    }
};
