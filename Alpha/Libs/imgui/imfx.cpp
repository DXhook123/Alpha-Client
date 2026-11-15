#include "imfx.h"

#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"
#include "imgui_internal.h"

#include <dxgi.h>
#include <d3d11.h>
#include <d3d12.h>
#include <d2d1_3.h>

#include <vector>

static ID2D1Factory3* d2dFactory = nullptr;
static ID2D1Device2* d2dDevice = nullptr;
static ID2D1DeviceContext2* d2dDeviceContext = nullptr;
static ID2D1Effect* blurEffect = nullptr;
static ID2D1Effect* displacementMapEffect = nullptr;
static ID2D1Effect* turbulenceEffect = nullptr;
static ID2D1Effect* shadowEffect = nullptr;
static ID2D1Effect* compositeEffect = nullptr;
static IDXGISurface* backBuffer = nullptr;
static ID2D1Bitmap1* sourceBitmap = nullptr;
static ID2D1Bitmap1* shadowBitmap = nullptr;
static bool d2dInit = false;
static float dpi = 0.0f;
static ImFX::FXMode mode = ImFX::FXMode::QUALITY;

static std::vector<ID2D1Bitmap*> ghostBitmaps;
static bool gGhostRequested = false;
static int gGhostMaxFrames = 1;
static float gGhostStrength = 0.9f;


void ImFX::SetFXMode(ImFX::FXMode fxmode) {
    mode = fxmode;
}

ImFX::FXMode ImFX::GetFXMode() {
    return mode;
}

bool ImFX::NewFrame(class ::ID3D11Device* d3d11Device, IDXGISurface* dxgiBackBuffer, float fxdpi) {
    if (d3d11Device == nullptr || dxgiBackBuffer == nullptr) {
        return false;
    }

    if (d2dInit == false && SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory))) {
        //Yay?
        //Create d2d & dxgi device(s)
        IDXGIDevice* dxgiDevice = nullptr;
        if (FAILED(d3d11Device->QueryInterface(IID_PPV_ARGS(&dxgiDevice))) || dxgiDevice == nullptr) {
            return false;
        }
        d2dFactory->CreateDevice(dxgiDevice, &d2dDevice);
        dxgiDevice->Release();
        if (d2dDevice == nullptr) {
            return false;
        }
        d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2dDeviceContext);
        if (d2dDeviceContext == nullptr) {
            return false;
        }

        //Create blur
        d2dDeviceContext->CreateEffect(CLSID_D2D1GaussianBlur, &blurEffect);
        if (blurEffect == nullptr) {
            return false;
        }
        //Create DisplacementMap effect
        d2dDeviceContext->CreateEffect(CLSID_D2D1DisplacementMap, &displacementMapEffect);
        if (displacementMapEffect == nullptr) {
            return false;
        }

        d2dInit = true;
    }

    if (!d2dInit || d2dDeviceContext == nullptr || blurEffect == nullptr) {
        return false;
    }

    // 交换链会轮换 backbuffer，不能只在 D2D 初始化时绑定第一帧的 surface。
    // Cryo 在每帧 beginRender 中重新创建 bitmap，这里保持相同的生命周期。
    if (sourceBitmap != nullptr) {
        d2dDeviceContext->SetTarget(nullptr);
        sourceBitmap->Release();
        sourceBitmap = nullptr;
    }

    if (backBuffer != nullptr) {
        backBuffer->Release();
        backBuffer = nullptr;
    }
    dxgiBackBuffer->AddRef();
    backBuffer = dxgiBackBuffer;
    dpi = fxdpi > 0.0f ? fxdpi : 96.0f;

    D2D1_BITMAP_PROPERTIES1 bitmapProperties
        = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);
    HRESULT hr = d2dDeviceContext->CreateBitmapFromDxgiSurface(dxgiBackBuffer, &bitmapProperties, &sourceBitmap);
    if (FAILED(hr)) {
        backBuffer->Release();
        backBuffer = nullptr;
        return false;
    }

    return true;
}

bool ImFX::CleanupFX() {
    if (!d2dInit)
        return false;
    if (shadowBitmap != nullptr) {
        shadowBitmap->Release();
        shadowBitmap = nullptr;
    }

    for (ID2D1Bitmap* gb : ghostBitmaps) {
        if (gb) gb->Release();
    }
    ghostBitmaps.clear();
    gGhostRequested = false;

    if (sourceBitmap != nullptr) {
        sourceBitmap->Release();
        sourceBitmap = nullptr;
    }
    if (backBuffer) {
        backBuffer->Release();
        backBuffer = nullptr;
    }
    if (displacementMapEffect) {
        displacementMapEffect->Release();
        displacementMapEffect = nullptr;
    }
    d2dFactory->Release();
    d2dDevice->Release();
    d2dDeviceContext->Release();
    blurEffect->Release();
    d2dInit = false;
    return true;
}

bool ImFX::EndFrame() {
    if (!d2dInit) {
        return false;
    }

    if (sourceBitmap != nullptr) {
        d2dDeviceContext->SetTarget(nullptr);
        sourceBitmap->Release();
        sourceBitmap = nullptr;
    }
    if (backBuffer != nullptr) {
        backBuffer->Release();
        backBuffer = nullptr;
    }
    return true;
}

struct EffectsContext {
    ID2D1Bitmap* targetBitmap = nullptr;
};
static std::vector<EffectsContext> currentContext;
struct BeginData {
    bool dontCopy = false;
};

void BeginFX(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
    BeginData* data = (BeginData*)cmd->UserCallbackData;
    if (!d2dInit || d2dDeviceContext == nullptr || sourceBitmap == nullptr) {
        delete data;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    d2dDeviceContext->SetTarget(sourceBitmap);
    d2dDeviceContext->BeginDraw();

    //Copy the bitmap
    ID2D1Bitmap* targetBitmap = nullptr;
    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(sourceBitmap->GetPixelFormat());
    if (FAILED(d2dDeviceContext->CreateBitmap(sourceBitmap->GetPixelSize(), props, &targetBitmap)) ||
        targetBitmap == nullptr) {
        d2dDeviceContext->EndDraw();
        d2dDeviceContext->SetTarget(nullptr);
        delete data;
        return;
    }
    auto destPoint = D2D1::Point2U(0, 0);
    auto size = sourceBitmap->GetPixelSize();
    auto rect = D2D1::RectU(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    targetBitmap->CopyFromBitmap(&destPoint, sourceBitmap, &rect);

    currentContext.emplace_back();
    currentContext.back().targetBitmap = targetBitmap;
    delete data;
}

struct EndData {
    bool composite = false;
};

void EndFX(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
    EndData* data = (EndData*)cmd->UserCallbackData;
    ImGuiIO& io = ImGui::GetIO();

    if (currentContext.empty() || d2dDeviceContext == nullptr || sourceBitmap == nullptr) {
        delete data;
        return;
    }

    if (data != nullptr && data->composite && compositeEffect != nullptr) {
        compositeEffect->SetInput(0, sourceBitmap);
        compositeEffect->SetInput(1, currentContext.back().targetBitmap);

        ID2D1Image* outImage = nullptr;
        compositeEffect->GetOutput(&outImage);
        d2dDeviceContext->SetTarget(sourceBitmap);
        d2dDeviceContext->DrawImage(outImage);
        outImage->Release();
    }

    d2dDeviceContext->Flush();
    d2dDeviceContext->EndDraw();
    d2dDeviceContext->SetTarget(nullptr);

    currentContext.back().targetBitmap->Release();
    currentContext.pop_back();
    delete data;
}

static std::vector<ImDrawList*> currentDrawlist;

bool ImFX::Begin(ImDrawList* drawList, bool dontCopy) {
    if (!d2dInit || sourceBitmap == nullptr || drawList == nullptr) {
        return false;
    }

    drawList->AddCallback(BeginFX, new BeginData{ dontCopy });
    currentDrawlist.push_back(drawList);
    return true;
}

bool ImFX::End(bool composite) {
    if (!d2dInit || currentDrawlist.empty()) {
        return false;
    }

    currentDrawlist.back()->AddCallback(EndFX, new EndData{ composite });
    currentDrawlist.pop_back();
    return true;
}

bool ImFX::ApplyBlur(float strength, std::optional<ImVec4> clipRect, float rounding) {
    if (!d2dInit || d2dDeviceContext == nullptr || d2dFactory == nullptr ||
        blurEffect == nullptr || sourceBitmap == nullptr || strength <= 0.0f) {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    const ImVec4 rect = clipRect.has_value() ? *clipRect :
        ImVec4(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);
    const D2D1_RECT_F screenRect = D2D1::RectF(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);
    const D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(
        D2D1::RectF(rect.x, rect.y, rect.z, rect.w), rounding, rounding);

    d2dDeviceContext->SetTarget(sourceBitmap);
    // ID2D1DeviceContext::BeginDraw 只切换绘制状态，不返回 HRESULT。
    d2dDeviceContext->BeginDraw();

    ID2D1Bitmap* targetBitmap = nullptr;
    const D2D1_BITMAP_PROPERTIES properties = D2D1::BitmapProperties(sourceBitmap->GetPixelFormat());
    if (FAILED(d2dDeviceContext->CreateBitmap(sourceBitmap->GetPixelSize(), properties, &targetBitmap)) ||
        targetBitmap == nullptr) {
        d2dDeviceContext->EndDraw();
        d2dDeviceContext->SetTarget(nullptr);
        return false;
    }

    const D2D1_POINT_2U destination = D2D1::Point2U(0, 0);
    const D2D1_SIZE_U size = sourceBitmap->GetPixelSize();
    const D2D1_RECT_U copyRect = D2D1::RectU(0, 0, size.width, size.height);
    if (FAILED(targetBitmap->CopyFromBitmap(&destination, sourceBitmap, &copyRect))) {
        targetBitmap->Release();
        d2dDeviceContext->EndDraw();
        d2dDeviceContext->SetTarget(nullptr);
        return false;
    }

    blurEffect->SetInput(0, targetBitmap);
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, strength);
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION,
        mode == ImFX::FXMode::QUALITY ? D2D1_GAUSSIANBLUR_OPTIMIZATION_QUALITY
        : D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED);

    ID2D1Image* output = nullptr;
    // ID2D1Effect::GetOutput 只写入输出指针，不返回 HRESULT。
    blurEffect->GetOutput(&output);
    if (output == nullptr) {
        targetBitmap->Release();
        d2dDeviceContext->EndDraw();
        d2dDeviceContext->SetTarget(nullptr);
        return false;
    }

    ID2D1ImageBrush* brush = nullptr;
    const D2D1_IMAGE_BRUSH_PROPERTIES brushProperties = D2D1::ImageBrushProperties(screenRect);
    if (FAILED(d2dDeviceContext->CreateImageBrush(output, brushProperties, &brush)) || brush == nullptr) {
        output->Release();
        targetBitmap->Release();
        d2dDeviceContext->EndDraw();
        d2dDeviceContext->SetTarget(nullptr);
        return false;
    }

    ID2D1RoundedRectangleGeometry* geometry = nullptr;
    const bool geometryReady = SUCCEEDED(d2dFactory->CreateRoundedRectangleGeometry(roundedRect, &geometry)) &&
        geometry != nullptr;
    if (geometryReady)
        d2dDeviceContext->FillGeometry(geometry, brush);

    if (geometry != nullptr)
        geometry->Release();
    brush->Release();
    output->Release();
    targetBitmap->Release();

    d2dDeviceContext->Flush();
    const HRESULT endResult = d2dDeviceContext->EndDraw();
    d2dDeviceContext->SetTarget(nullptr);
    return geometryReady && SUCCEEDED(endResult);
}

struct PushLayerData {
    float rounding = 0.0f;
    std::optional<ImVec4> clipRect;

    PushLayerData(std::optional<ImVec4> clipRect) {
        this->clipRect = clipRect;
    }
};

static std::vector<ID2D1Layer*> layers;

void PushLayerCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
    ID2D1Layer* layer;
    d2dDeviceContext->CreateLayer(&layer);

    ImGuiIO& io = ImGui::GetIO();
    //Create rects and rounded rects
    PushLayerData* data = (PushLayerData*)cmd->UserCallbackData;
    ImVec4 clipRect = data->clipRect.has_value() ? *data->clipRect : cmd->ClipRect;

    //Create rects and rounded rects
    D2D1_RECT_F clipRectD2D = D2D1::RectF(
        clipRect.x,
        clipRect.y,
        clipRect.z,
        clipRect.w
    );

    D2D1_LAYER_PARAMETERS parameters;
    parameters.contentBounds = clipRectD2D;
    parameters.geometricMask = nullptr;
    parameters.maskAntialiasMode = D2D1_ANTIALIAS_MODE_ALIASED;
    parameters.maskTransform = D2D1::Matrix3x2F::Identity();
    parameters.opacity = 1.0f;
    parameters.opacityBrush = nullptr;
    parameters.layerOptions = D2D1_LAYER_OPTIONS_NONE;
    d2dDeviceContext->PushLayer(parameters, layer);
    layers.push_back(layer);
}

void PopLayerCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
    ID2D1Layer* layer = layers.back();
    d2dDeviceContext->PopLayer();
    layers.pop_back();
    layer->Release();
}

bool ImFX::PushLayer(std::optional<ImVec4> clipRect) {
    if (!d2dInit) {
        return false;
    }

    currentDrawlist.back()->AddCallback(PushLayerCallback, new PushLayerData(clipRect));
    return true;
}

bool ImFX::PopLayer() {
    if (!d2dInit) {
        return false;
    }

    float rounding = ImGui::GetStyle().FrameRounding;
    currentDrawlist.back()->AddCallback(PopLayerCallback, nullptr);
    return true;
}

struct BlurCallbackData {
    float strength = 0.0f;
    float rounding = 0.0f;
    std::optional<ImVec4> clipRect;

    BlurCallbackData(float strength, float rounding, std::optional<ImVec4> clipRect = std::optional<ImVec4>()) {
        this->strength = strength;
        this->rounding = rounding;
        this->clipRect = clipRect;
    }
};

void BlurCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
    //Do D2D rendering stuff

    BlurCallbackData* data = (BlurCallbackData*)cmd->UserCallbackData;
    if (!d2dInit || d2dDeviceContext == nullptr || sourceBitmap == nullptr || blurEffect == nullptr ||
        data == nullptr) {
        delete data;
        return;
    }

    if (currentContext.size() <= 0) {
        // draw list 被重建或回调被跳过时，安全放弃本次效果，不能让游戏进程崩溃。
        delete data;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec4 clipRect = data->clipRect.has_value() ? *data->clipRect : cmd->ClipRect;

    //Create rects and rounded rects
    D2D1_RECT_F screenRectF = D2D1::RectF(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    D2D1_RECT_F clipRectD2D = D2D1::RectF(
        clipRect.x,
        clipRect.y,
        clipRect.z,
        clipRect.w
    );
    D2D1_ROUNDED_RECT clipRectRounded = D2D1::RoundedRect(clipRectD2D, data->rounding, data->rounding);

    //Apply blur effect
    blurEffect->SetInput(0, currentContext.back().targetBitmap);
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, data->strength);
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
    D2D1_GAUSSIANBLUR_OPTIMIZATION opt = mode == ImFX::FXMode::QUALITY ? D2D1_GAUSSIANBLUR_OPTIMIZATION_QUALITY
        : D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED;
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, opt);

    //Get the output image
    ID2D1Image* outImage = nullptr;
    // ID2D1Effect::GetOutput 只写入输出指针，不返回 HRESULT。
    blurEffect->GetOutput(&outImage);
    if (outImage == nullptr) {
        delete data;
        return;
    }

    //Create bitmap brush for the clipping
    ID2D1ImageBrush* outImageBrush = nullptr;
    D2D1_IMAGE_BRUSH_PROPERTIES outImage_props = D2D1::ImageBrushProperties(screenRectF);
    if (FAILED(d2dDeviceContext->CreateImageBrush(
        outImage,
        outImage_props,
        &outImageBrush
    )) || outImageBrush == nullptr) {
        outImage->Release();
        delete data;
        return;
    }

    //Draw the blur
    ID2D1RoundedRectangleGeometry* clipRectGeo = nullptr;
    if (d2dFactory != nullptr && SUCCEEDED(d2dFactory->CreateRoundedRectangleGeometry(clipRectRounded, &clipRectGeo)) &&
        clipRectGeo != nullptr) {
        d2dDeviceContext->FillGeometry(clipRectGeo, outImageBrush);
        clipRectGeo->Release();
    }

    //Release interfaces
    outImageBrush->Release();
    outImage->Release();

    //Free data
    delete data;
}

bool ImFX::AddBlur(float strength, std::optional<ImVec4> clipRect, float rounding) {
    if (!d2dInit || sourceBitmap == nullptr || currentDrawlist.empty()) {
        return false;
    }

    if (mode == FXMode::PERFORMANCE) {
        return true;
    }

    BlurCallbackData* data = new BlurCallbackData(strength, rounding, clipRect);
    currentDrawlist.back()->AddCallback(BlurCallback, data);
    return true;
}

//https://stackoverflow.com/a/73769914
ID2D1PathGeometry*
CreateRoundRect(int x, int y, int width, int height, int leftTop, int rightTop, int rightBottom, int leftBottom) {
    ID2D1GeometrySink* sink = nullptr;
    ID2D1PathGeometry* path = nullptr;

    d2dFactory->CreatePathGeometry(&path);
    path->Open(&sink);

    D2D1_POINT_2F p[2];

    p[0].x = x + leftTop;
    p[0].y = y;
    sink->BeginFigure(p[0], D2D1_FIGURE_BEGIN::D2D1_FIGURE_BEGIN_FILLED);
    p[1].x = x + width - rightTop;
    p[1].y = y;
    sink->AddLines(p, 2);

    p[0].x = x + width;
    p[0].y = y + rightTop;

    if (rightTop) {
        D2D1_POINT_2F p2 = D2D1::Matrix3x2F::Rotation(0, p[1]).TransformPoint(p[0]);
        sink->AddArc(D2D1::ArcSegment(p2, D2D1::SizeF(rightTop, rightTop), 0, D2D1_SWEEP_DIRECTION_CLOCKWISE,
            D2D1_ARC_SIZE_SMALL));
    }

    p[1].x = x + width;
    p[1].y = y + height - rightBottom;
    sink->AddLines(p, 2);

    p[0].x = x + width - rightBottom;
    p[0].y = y + height;

    if (rightBottom) {
        D2D1_POINT_2F p2 = D2D1::Matrix3x2F::Rotation(0, p[1]).TransformPoint(p[0]);
        sink->AddArc(D2D1::ArcSegment(p2, D2D1::SizeF(rightBottom, rightBottom), 0, D2D1_SWEEP_DIRECTION_CLOCKWISE,
            D2D1_ARC_SIZE_SMALL));
    }

    p[1].x = x + leftBottom;
    p[1].y = y + height;
    sink->AddLines(p, 2);

    p[0].x = x;
    p[0].y = y + height - leftBottom;
    if (leftBottom) {
        D2D1_POINT_2F p2 = D2D1::Matrix3x2F::Rotation(0, p[1]).TransformPoint(p[0]);
        sink->AddArc(D2D1::ArcSegment(p2, D2D1::SizeF(leftBottom, leftBottom), 0, D2D1_SWEEP_DIRECTION_CLOCKWISE,
            D2D1_ARC_SIZE_SMALL));
    }


    p[1].x = x;
    p[1].y = y + leftTop;
    sink->AddLines(p, 2);
    p[0].x = x + leftTop;
    p[0].y = y;
    if (leftTop) {
        D2D1_POINT_2F p2 = D2D1::Matrix3x2F::Rotation(0, p[1]).TransformPoint(p[0]);
        sink->AddArc(D2D1::ArcSegment(p2, D2D1::SizeF(leftTop, leftTop), 0, D2D1_SWEEP_DIRECTION_CLOCKWISE,
            D2D1_ARC_SIZE_SMALL));
    }

    sink->EndFigure(D2D1_FIGURE_END::D2D1_FIGURE_END_CLOSED);
    sink->Close();
    sink->Release();

    return path;
}

struct ColorCallbackData {
    ImVec4 color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    std::optional<ImVec4> rounding;
    std::optional<ImVec4> clipRect;

    ColorCallbackData(ImVec4 color, std::optional<ImVec4> rounding,
        std::optional<ImVec4> clipRect = std::optional<ImVec4>()) {
        this->color = color;
        this->rounding = rounding;
        this->clipRect = clipRect;
    }
};

void ColorCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
    if (!d2dInit) {
        return;
    }

    if (backBuffer == nullptr) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ColorCallbackData* data = (ColorCallbackData*)cmd->UserCallbackData;
    ImVec4 clipRect = data->clipRect.has_value() ? *data->clipRect : cmd->ClipRect;
    ImVec4 rounding = data->rounding.has_value() ? *data->rounding : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    //Create rects and rounded rects
    D2D1_RECT_F screenRectF = D2D1::RectF(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    D2D1_RECT_F clipRectD2D = D2D1::RectF(
        clipRect.x,
        clipRect.y,
        clipRect.z,
        clipRect.w
    );
    //This uses width and height so we have to subtract the positions too
    ID2D1PathGeometry* roundedRectGeo = CreateRoundRect(clipRect.x, clipRect.y, clipRect.z - clipRect.x,
        clipRect.w - clipRect.y, rounding.x, rounding.y, rounding.z,
        rounding.w);

    ID2D1SolidColorBrush* colorBrush = nullptr;
    d2dDeviceContext->CreateSolidColorBrush(D2D1::ColorF(data->color.x, data->color.y, data->color.z, data->color.w),
        &colorBrush);
    d2dDeviceContext->FillGeometry(roundedRectGeo, colorBrush);

    //Release interfaces
    colorBrush->Release();
    roundedRectGeo->Release();

    //Free data
    delete data;
}

bool ImFX::AddColor(ImVec4 color, std::optional<ImVec4> rounding, std::optional<ImVec4> clipRect) {
    if (!d2dInit) {
        return false;
    }

    ColorCallbackData* data = new ColorCallbackData(color, rounding, clipRect);
    currentDrawlist.back()->AddCallback(ColorCallback, data);
    return true;
}

bool ImFX::AddColor(ImVec4 color, float rounding, std::optional<ImVec4> clipRect) {
    return AddColor(color, ImVec4(rounding, rounding, rounding, rounding), clipRect);
}

bool ImFX::AddColor(ImVec4 color, std::optional<ImVec4> clipRect) {
    if (!d2dInit) {
        return false;
    }

    float rounding = ImGui::GetStyle().FrameRounding;
    return AddColor(color, ImVec4(rounding, rounding, rounding, rounding), clipRect);
}

bool ImFX::AddColor(ImGuiCol color, std::optional<ImVec4> clipRect) {
    return AddColor(ImGui::GetStyleColorVec4(color), clipRect);
}

struct ShadowCallbackData {
    float strength = 0.0f;
    float rounding = 0.0f;
    ImVec4 color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    std::optional<ImVec4> clipRect;

    ShadowCallbackData(float strength, float rounding, ImVec4 color,
        std::optional<ImVec4> clipRect = std::optional<ImVec4>()) {
        this->strength = strength;
        this->rounding = rounding;
        this->color = color;
        this->clipRect = clipRect;
    }
};

void ShadowCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
    //Do D2D rendering stuff

    if (!d2dInit) {
        return;
    }

    if (currentContext.size() <= 0) {
        throw std::exception("You must call ImFX::Begin before using effects!");
    }

    ImGuiIO& io = ImGui::GetIO();
    ShadowCallbackData* data = (ShadowCallbackData*)cmd->UserCallbackData;
    ImVec4 clipRect = data->clipRect.has_value() ? *data->clipRect : cmd->ClipRect;

    //Create rects and rounded rects
    D2D1_RECT_F screenRectF = D2D1::RectF(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    D2D1_RECT_F clipRectD2D = D2D1::RectF(
        clipRect.x,
        clipRect.y,
        clipRect.z,
        clipRect.w
    );
    D2D1_ROUNDED_RECT clipRectRounded = D2D1::RoundedRect(clipRectD2D, data->rounding, data->rounding);

    if (shadowBitmap == nullptr) {
        //Create a new blank bitmap
        D2D1_BITMAP_PROPERTIES1 newLayerProps = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
            sourceBitmap->GetPixelFormat());
        d2dDeviceContext->CreateBitmap(sourceBitmap->GetPixelSize(), nullptr, 0, newLayerProps, &shadowBitmap);
    }

    //Draw the color
    d2dDeviceContext->SetTarget(shadowBitmap);
    d2dDeviceContext->Clear(D2D1::ColorF(0, 0, 0, 0));
    ID2D1SolidColorBrush* colorBrush = nullptr;
    d2dDeviceContext->CreateSolidColorBrush(D2D1::ColorF(data->color.x, data->color.y, data->color.z, data->color.w),
        &colorBrush);
    d2dDeviceContext->FillRoundedRectangle(clipRectRounded, colorBrush);

    //Apply blur effect
    blurEffect->SetInput(0, shadowBitmap);
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, data->strength);
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
    D2D1_GAUSSIANBLUR_OPTIMIZATION opt = mode == ImFX::FXMode::QUALITY ? D2D1_GAUSSIANBLUR_OPTIMIZATION_QUALITY
        : D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED;
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, opt);

    d2dDeviceContext->SetTarget(sourceBitmap);

    //Release interfaces
    colorBrush->Release();

    //Get the output image
    ID2D1Image* outImage = nullptr;
    blurEffect->GetOutput(&outImage);

    //Create bitmap brush for the clipping
    ID2D1ImageBrush* outImageBrush = nullptr;
    D2D1_IMAGE_BRUSH_PROPERTIES outImage_props = D2D1::ImageBrushProperties(screenRectF);
    d2dDeviceContext->CreateImageBrush(
        outImage,
        outImage_props,
        &outImageBrush
    );

    //Draw the shadow
    ID2D1RectangleGeometry* screenRectGeo = nullptr;
    d2dFactory->CreateRectangleGeometry(screenRectF, &screenRectGeo);
    d2dDeviceContext->FillGeometry(screenRectGeo, outImageBrush);
    screenRectGeo->Release();

    //Release interfaces
    outImageBrush->Release();
    outImage->Release();

    //Free data
    delete data;
}

bool ImFX::QueueGhostFrame(int maxFrames, float strength) {
    gGhostRequested = true;
    gGhostMaxFrames = maxFrames > 0 ? maxFrames : 1;
    gGhostStrength = strength;
    return true;
}

bool ImFX::HasQueuedGhostFrame() {
    return gGhostRequested;
}

bool ImFX::RenderQueuedGhostFrame(ID3D11Device* d3d11Device, IDXGISurface* surface, float fxdpi) {
    if (!gGhostRequested) {
        return false;
    }
    gGhostRequested = false;

    if (d3d11Device == nullptr || surface == nullptr) {
        return false;
    }

    // 借助 NewFrame 为当前 backbuffer 建立 D2D sourceBitmap（并首次初始化 D2D）。
    if (!NewFrame(d3d11Device, surface, fxdpi) || d2dDeviceContext == nullptr || sourceBitmap == nullptr) {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    D2D1_SIZE_U size = sourceBitmap->GetPixelSize();
    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(sourceBitmap->GetPixelFormat());
    D2D1_POINT_2U destPoint = D2D1::Point2U(0, 0);
    D2D1_RECT_U copyRect = D2D1::RectU(0, 0, size.width, size.height);

    d2dDeviceContext->SetTarget(sourceBitmap);
    d2dDeviceContext->BeginDraw();

    // 记录当前帧画面作为残影。
    int maxFrames = gGhostMaxFrames;
    while ((int)ghostBitmaps.size() >= maxFrames) {
        ID2D1Bitmap* stale = ghostBitmaps.front();
        ghostBitmaps.erase(ghostBitmaps.begin());
        if (stale) stale->Release();
    }

    ID2D1Bitmap* ghostBitmap = nullptr;
    d2dDeviceContext->CreateBitmap(size, props, &ghostBitmap);
    if (ghostBitmap != nullptr) {
        ghostBitmap->CopyFromBitmap(&destPoint, sourceBitmap, &copyRect);
        ghostBitmaps.push_back(ghostBitmap);
    }

    // 把历史残影按强度递减叠加到当前画面。
    float alpha = 0.3f * gGhostStrength;
    D2D1_RECT_F destRect = D2D1::RectF(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    for (ID2D1Bitmap* gb : ghostBitmaps) {
        if (gb != nullptr) {
            d2dDeviceContext->DrawBitmap(gb, destRect, alpha,
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        }
        alpha *= gGhostStrength;
    }

    d2dDeviceContext->EndDraw();
    d2dDeviceContext->SetTarget(nullptr);
    EndFrame();
    return true;
}
