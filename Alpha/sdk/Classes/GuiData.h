#pragma once
#include "../../Utils/MemoryUtil.h"
#include "../../Utils/Maths.h"
#include "../../Utils/Spdlog.h"
#include "../Signatures.h"
#include <atomic>
#include <optional>
#include <string>

class GuiData {
private:
	static bool hasPageAccess(const void* address, size_t size, bool executable) {
		if (!address || size == 0) return false;
		MEMORY_BASIC_INFORMATION info{};
		if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
			(info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
		const auto start = reinterpret_cast<uintptr_t>(address);
		const auto end = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
		if (start > end || size > end - start) return false;
		const DWORD protection = info.Protect & 0xff;
		if (executable)
			return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
				protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
		return protection == PAGE_READONLY || protection == PAGE_READWRITE ||
			protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READ ||
			protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
	}

public:
	CLASS_MEMBER(Vec2<float>, screenSize, 0x40);
	CLASS_MEMBER(Vec2<float>, guiSize, 0x50);
	CLASS_MEMBER(float, guiScale, 0x5C);
	CLASS_MEMBER(float, invGuiScale, 0x60);
	CLASS_MEMBER(short, pointerX, 0x7A);
	CLASS_MEMBER(short, pointerY, 0x7C);

	Vec2<float> getMousePos() {
		return Vec2<float>((float)pointerX, (float)pointerY);
	}
	void displayMessage(const std::string& msg, bool validateBackend = false) {
		if (validateBackend) {
			const auto backendSlot = reinterpret_cast<const uintptr_t*>(reinterpret_cast<uintptr_t>(this) + 0x98);
			uintptr_t backend = 0;
			uintptr_t vtable = 0;
			uintptr_t virtualFunction = 0;
			if (hasPageAccess(backendSlot, sizeof(uintptr_t), false))
				backend = *backendSlot;
			if (hasPageAccess(reinterpret_cast<const void*>(backend), sizeof(uintptr_t), false))
				vtable = *reinterpret_cast<const uintptr_t*>(backend);
			if (vtable <= UINTPTR_MAX - 0x2F0 &&
				hasPageAccess(reinterpret_cast<const void*>(vtable + 0x2F0), sizeof(uintptr_t), false))
				virtualFunction = *reinterpret_cast<const uintptr_t*>(vtable + 0x2F0);
			if (!hasPageAccess(reinterpret_cast<const void*>(virtualFunction), 1, true)) {
				static std::atomic_bool warned{false};
				if (!warned.exchange(true)) {
					char diagnostic[256];
					sprintf_s(diagnostic, "[Alpha] GuiData chat backend invalid: gui=%p backend=%p vtable=%p target=%p\n",
						static_cast<void*>(this), reinterpret_cast<void*>(backend),
						reinterpret_cast<void*>(vtable), reinterpret_cast<void*>(virtualFunction));
					OutputDebugStringA(diagnostic);
				}
				return;
			}
		}
		static uintptr_t address = 0;
		if (address == 0) {
			address = MemoryUtil::findSignature(Sigs::gui::displayMessage);
			if (address == 0) {
				LOG_WARN("[GuiData] displayMessage sig NOT FOUND");
				return;
			}
			LOG_INFO("[GuiData] displayMessage sig resolved @ 0x%llX", (unsigned long long)address);
		}
		using display_message_t = void(__fastcall*)(GuiData*, const std::string&, std::optional<std::string>, bool);
		reinterpret_cast<display_message_t>(address)(this, msg, std::nullopt, false);
	}
};
