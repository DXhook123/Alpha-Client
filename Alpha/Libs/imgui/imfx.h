#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"

#include <optional>

class ID3D11Device;

class IDXGISurface;

struct ID2D1Bitmap1;

namespace ImFX {
    enum class FXMode {
        QUALITY,
        NORMAL,
        PERFORMANCE
    };

    void SetFXMode(FXMode mode);

    FXMode GetFXMode();

    bool NewFrame(ID3D11Device* d3d11Device, IDXGISurface* backBuffer, float dpi);

    bool EndFrame();

    bool CleanupFX();

    bool Begin(ImDrawList* drawList, bool dontCopy = false);

    bool End(bool composite = false);

    // 在没有 ImGui 回调的渲染路径（例如 D3D12）上直接处理当前 backbuffer。
    bool ApplyBlur(float strength, std::optional<ImVec4> clipRect = std::optional<ImVec4>(),
        float rounding = 0.0f);

    bool PushLayer(std::optional<ImVec4> clipRect = std::optional<ImVec4>());

    bool PopLayer();

    bool AddBlur(float strength, std::optional<ImVec4> clipRect = std::optional<ImVec4>(),
        float rounding = ImGui::GetStyle().FrameRounding);

    bool AddColor(ImVec4 color, float rounding, std::optional<ImVec4> clipRect);

    bool AddColor(ImVec4 color, std::optional<ImVec4> rounding, std::optional<ImVec4> clipRect);

    bool AddColor(ImVec4 color, std::optional<ImVec4> clipRect = std::optional<ImVec4>());

    bool AddColor(ImGuiCol color, std::optional<ImVec4> clipRect = std::optional<ImVec4>());

    bool AddDisplacementMap();

    // 动态模糊残影：模块每帧调用 QueueGhostFrame 登记本帧需求，
    // PresentHook 在拿到当前 backbuffer 时调用 RenderQueuedGhostFrame 立即执行叠加。
    bool QueueGhostFrame(int maxFrames, float strength);
    // PresentHook 用它判断是否真的存在残影请求，避免每帧无谓地获取
    // D3D11-on-12 包装资源并 Flush。
    bool HasQueuedGhostFrame();
    bool RenderQueuedGhostFrame(ID3D11Device* d3d11Device, IDXGISurface* surface, float dpi);
}
