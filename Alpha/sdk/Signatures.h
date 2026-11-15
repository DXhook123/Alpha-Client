#pragma once
#include <string_view>
#include "../Utils/MemoryUtil.h"
#define findSig(sig) MemoryUtil::findSignature(sig)

namespace Sigs {
    namespace actor {
        inline std::string_view getNameTag = "56 48 83 EC ? 48 8B 81 ? ? ? ? 48 85 C0 74 ? 8B 50 ? 48 8B 08 29 CA 81 E2 ? ? ? ? 48 8D 05";
        inline std::string_view setPos = "41 56 56 57 53 48 83 EC 48 44 89 C3 48 89 D7 48 89 CE 66 0F 6E 02 66 0F 7E C0 25 FF FF FF 7F";
    }
    namespace packet {
        inline std::string_view minecraftPacketsCreatePacket = "56 48 83 EC ?? 48 89 CE 81 FA";
    }
    namespace hook {
        inline std::string_view clientInstanceUpdate = "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC ? ? ? ? 48 8D AC 24 ? ? ? ? 48 C7 85 ? ? ? ? ? ? ? ? 89 D3 48 89 CE 48 8B 01";
        inline std::string_view keyMouse = "41 57 41 56 41 55 41 54 56 57 55 53 48 83 EC ? 44 89 CF 44 89 C3 89 D5 48 89 CE 44 0F B7 A4 24";
        inline std::string_view getGamma = "48 83 EC 38 48 8B 05 B5 27 82 10 48 31 E0 48 89 44 24 30 48 8B 01 48 8B";
        inline std::string_view actorSetRot = "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC D8 00 00 00 44 0F 29 94 24 C0 00 00 00 44 0F 29 8C 24 B0 00 00 00 44 0F 29 84 24 A0 00 00 00 0F 29 BC 24 90 00 00 00 0F 29 B4 24 80 00 00 00 4C 89 C6 49 8B 40 10 41 8B 78 18";
        inline std::string_view rakPeerGetAveragePing = "48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 31 E0 48 89 84 24 ? ? ? ? 4C 8B 02 4C 3B 05 ? ? ? ? 0F 85 ? ? ? ? 0F B7 42 ? 44 0F B7 82 ? ? ? ? 44 0F B7 8A ? ? ? ? 66 89 44 24 ? 0F 10 42 ? 0F 10 4A ? 0F 10 52 ? 0F 10 5A ? 0F 11 44 24 ? 0F 11 4C 24 ? 0F 11 54 24 ? 0F 11 5C 24 ? 0F 10 42 ? 0F 11 44 24 ? 0F 10 42 ? 0F 11 44 24 ? 0F 10 42 ? 0F 11 84 24 ? ? ? ? 0F 10 82 ? ? ? ? 0F 11 84 24 ? ? ? ? 66 44 89 8C 24 ? ? ? ? 66 44 89 84 24 ? ? ? ? 48 8D 54 24 ? 45 31 C0 45 31 C9 E8 ? ? ? ? BA";
    }
    namespace vtable {
        inline std::string_view ActorVatble = "48 8D 05 ?? ?? ?? ?? 48 89 07 66 0F EF C0 F3 0F 7F 87";
        inline std::string_view gameMode2 = "48 8D 05 ?? ?? ?? ?? 48 89 01 48 89 51 ?? 48 C7 41 ?? ?? ?? ?? ?? C7 41 ?? ?? ?? ?? ?? C6 41";
    }
    namespace clientInstance {
        inline std::string_view getScreenName = "55 56 57 48 83 EC ? 48 8D 6C 24 ? 48 C7 45 ? ? ? ? ? 48 89 D6 48 8B 01 48 8B 80 ? ? ? ? 48 8D 55 ? FF 15 ? ? ? ? 48 83 7D ? ? 75 ? 48 8D 05 ? ? ? ? 48 89 44 24 ? 48 8D 0D ? ? ? ? 48 8D 15 ? ? ? ? 4C 8D 0D ? ? ? ? 41 B8 ? ? ? ? E8 ? ? ? ? 84 C0 74 ? C7 04 25 ? ? ? ? ? ? ? ? 48 8B 45 ? 80 38 ? 75 ? 48 8D 05 ? ? ? ? 48 89 44 24 ? 48 8D 0D ? ? ? ? 48 8D 15 ? ? ? ? 4C 8D 0D ? ? ? ? 41 B8 ? ? ? ? E8 ? ? ? ? 84 C0 74 ? C7 04 25 ? ? ? ? ? ? ? ? 48 8B 4D ? 48 8B 01 48 8B 80 ? ? ? ? 48 89 F2 FF 15 ? ? ? ? 0F 57 C0 48 8B 7D ? 0F 11 45 ? 48 85 FF 74 ? F0 FF 4F ? 75 ? 48 8B 07 48 8B 00 48 89 F9 FF 15 ? ? ? ? F0 FF 4F ? 75 ? 48 8B 07 48 8B 40 ? 48 89 F9 FF 15 ? ? ? ? 48 8B 7D ? 48 C7 45 ? ? ? ? ? 48 85 FF 74 ? F0 FF 4F ? 75 ? 48 8B 07 48 8B 00 48 89 F9 FF 15 ? ? ? ? F0 FF 4F ? 75 ? 48 8B 07 48 8B 40 ? 48 89 F9 FF 15 ? ? ? ? 48 89 F0 48 83 C4 ? 5F 5E 5D C3 66 0F 1F 44 00 ? 48 89 54 24 ? 55 56 57 48 83 EC ? 48 8D 6A ? 48 8D 4D ? E8 ? ? ? ? 90 48 83 C4 ? 5F 5E 5D C3 CC CC CC CC CC CC CC CC CC CC CC CC CC CC 55 56 57 48 83 EC ? 48 8D 6C 24 ? 48 C7 45 ? ? ? ? ? 48 89 D6";
        inline std::string_view grabMouse = "56 48 83 EC ? 48 89 CE 48 8B 01 48 8B 80 ? ? ? ? FF 15 ? ? ? ? 84 C0 74 ? 48 8B 8E ? ? ? ? 48 8B 01 48 8B 80 ? ? ? ? 48 8B 15 ? ? ? ? 48 83 C4 ? 5E 48 FF E2 90 48 83 C4 ? 5E C3 CC CC CC CC CC CC CC CC CC CC CC CC CC 56 48 83 EC";
        inline std::string_view releaseMouse = "56 48 83 EC ? 48 89 CE 48 8B 01 48 8B 80 ? ? ? ? FF 15 ? ? ? ? 84 C0 74 ? 48 8B 8E ? ? ? ? 48 8B 01 48 8B 80 ? ? ? ? 48 8B 15 ? ? ? ? 48 83 C4 ? 5E 48 FF E2 90 48 83 C4 ? 5E C3 CC CC CC CC CC CC CC CC CC CC CC CC CC 56 53";
    }
    namespace gui {
        inline std::string_view displayMessage = "55 56 53 48 81 EC ?? ?? ?? ?? 48 8D AC 24 ?? ?? ?? ?? 48 C7 85 ?? ?? ?? ?? ?? ?? ?? ?? 44 88 CB";
    }
    namespace blockSource {
        inline std::string_view getSeenPercent = "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC ? ? ? ? 44 0F 29 BC 24 ? ? ? ? 44 0F 29 B4 24 ? ? ? ? 44 0F 29 AC 24 ? ? ? ? 44 0F 29 A4 24 ? ? ? ? 44 0F 29 9C 24 ? ? ? ? 44 0F 29 94 24 ? ? ? ? 44 0F 29 8C 24 ? ? ? ? 44 0F 29 84 24 ? ? ? ? 0F 29 BC 24 ? ? ? ? 0F 29 B4 24 ? ? ? ? 4C 89 C6 48 89 D7";
    }
    namespace render {
        inline std::string_view levelRendererRenderLevel = "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC ?? ?? ?? ?? 48 8D AC 24 ?? ?? ?? ?? 44 0F 29 85 ?? ?? ?? ?? 0F 29 BD ?? ?? ?? ?? 0F 29 B5 ?? ?? ?? ?? 48 C7 85 ?? ?? ?? ?? FE FF FF FF 0F 28 F2 49 89 D7 48 89 CF 48 83 B9 38 01 00 00 00";
        inline std::string_view screenViewSetupAndRender = "E8 ? ? ? ? 48 8B 4B ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 89 FA FF 15 ? ? ? ? 48 8D 4D";
    }
}
