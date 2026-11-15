#pragma once
#include "../Module.h"
#include "Position.h"
#include "FPS.h"
#include "SpeedHUD.h"
#include "PingHUD.h"
#include "TPSHUD.h"
#include "ArrayListHUD.h"
#include "WatermarkHUD.h"
#include "WelcomeHUD.h"
#include "PotionHUD.h"
#include <atomic>
#include <array>
#include <string>
#include <vector>

class MinecraftUIRenderContext;

class HudEditor : public Module {
public:
	HudEditor();
	Vec2<float> categoryPosition = Vec2<float>(24.f, 60.f);

	bool runOnBackground() override { return true; }
	void onEnable() override;
	void onDisable() override;
	static bool saveHudConfig();
	static void loadHudConfig();
	std::array<HudModule*, 9> getElements() { return { &pos, &fps, &ping, &speed, &tps, &arrayList, &watermark, &welcome, &potions }; }
	void onImGuiRender(ImDrawList* drawlist) override;
	void onNormalTick(LocalPlayer* localPlayer) override;
	std::string serializeAll() const;
	void loadFromLines(const std::vector<std::string>& lines);
	void handleScreenView(void* screenView);
	void renderNative(MinecraftUIRenderContext* context);

	Position* getPos() { return &pos; }
	FPS*      getFps() { return &fps; }
	SpeedHUD* getSpeed() { return &speed; }
	PingHUD*  getPing() { return &ping; }
	ArrayListHUD* getArrayList() { return &arrayList; }
	WatermarkHUD* getWatermark() { return &watermark; }
	WelcomeHUD* getWelcome() { return &welcome; }
	PotionHUD* getPotions() { return &potions; }

private:
	int fontMode = 1;
	bool fontLayoutInitialized = false;
	int lastLayoutFontType = -1;
	int lastLayoutFontStyle = -1;
	int lastLayoutFontMode = -1;
	float lastLayoutFontScale = 0.f;
	std::atomic_bool useMinecraftFont{ true };
	std::atomic_bool nativePositionShown{ true };
	std::atomic_bool nativeFpsShown{ true };
	std::atomic_bool nativeSpeedShown{ true };
	std::atomic_bool nativePingShown{ true };
	std::atomic_bool nativeTpsShown{ true };
	Position pos;
	FPS      fps;
	SpeedHUD speed;
	PingHUD  ping;
	TPSHUD tps;
	ArrayListHUD arrayList;
	WatermarkHUD watermark;
	WelcomeHUD welcome;
	PotionHUD potions;
};
