#pragma once
#include <cstddef>

namespace SdkVersion {
	struct Layout {
		int getRegion, getLocalPlayer, getLevel, getGuiData;
		int getPacketSender, getMouseGrabbed, getScreenName, refocusMouse;
		std::size_t guiData, loopbackPacketSender;
		int fillRectangle, drawDebugText, flushText;
		std::size_t transactionPacketSize, transaction, clientSide;
		std::size_t mcGame, minecraft, levelRenderer;
		int getGameRenderer;
		std::size_t gameRenderer, viewMatrix, projectionMatrix, renderPlayer, cameraPos;
		int actorSwing;
		std::size_t emoteRuntimeId, emotePieceId, emoteTicks, emoteXuid, emotePlatformId, emoteFlags;
		std::size_t screenVisualTree;
		std::size_t useTransactionItem, useTransactionPlayerPos, useTransactionClickPos;
		std::size_t useTransactionResult, useTransactionCooldown;
		std::size_t gameSession, simTimer, realTimer, minecraftPacketSender;
		int getPlayerXuid;
		std::size_t tessBegin, tessColor, tessVertex, tessRender, meshImmediately;
		std::size_t attributeInstanceSize;
	};

	inline constexpr Layout active{
		.getRegion = 30,
		.getLocalPlayer = 31,
		.getLevel = 168,
		.getGuiData = 218,
		.getPacketSender = 293,
		.getMouseGrabbed = 147,
		.getScreenName = 246,
		.refocusMouse = 312,
		.guiData = 0x650,
		.loopbackPacketSender = 0x1D0,
		.fillRectangle = 15,
		.drawDebugText = 4,
		.flushText = 6,
		.transactionPacketSize = 0x188,
		.transaction = 0x170,
		.clientSide = 0x178,
		.mcGame = 0x1A8,
		.minecraft = 0x1B0,
		.levelRenderer = 0x1C0,
		.getGameRenderer = 186,
		.gameRenderer = 0x0,
		.viewMatrix = 0x388,
		.projectionMatrix = 0x408,
		.renderPlayer = 0x468,
		.cameraPos = 0x654,
		.actorSwing = 109,
		.emoteRuntimeId = 0x30,
		.emotePieceId = 0x38,
		.emoteTicks = 0x58,
		.emoteXuid = 0x60,
		.emotePlatformId = 0x80,
		.emoteFlags = 0xA0,
		.screenVisualTree = 0x50,
		.useTransactionItem = 0x90,
		.useTransactionPlayerPos = 0xF0,
		.useTransactionClickPos = 0xFC,
		.useTransactionResult = 0x108,
		.useTransactionCooldown = 0x109,
		.gameSession = 0xC8,
		.simTimer = 0xE0,
		.realTimer = 0xE8,
		.minecraftPacketSender = 0x100,
		.getPlayerXuid = 231,
		.tessBegin = 0x59CA210,
		.tessColor = 0x59CA610,
		.tessVertex = 0x59CA6D0,
		.tessRender = 0x59CBC80,
		.meshImmediately = 0x467ED80,
		.attributeInstanceSize = 0x88,
	};

	constexpr bool is12651() { return true; }
	inline const Layout& current() {
		return active;
	}
}
