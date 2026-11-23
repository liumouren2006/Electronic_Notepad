#include "imaging.hpp"

#include <algorithm>
#include <cstring>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include "core/util.hpp"

using namespace Gdiplus;

namespace moji {

namespace {
ULONG_PTR g_token = 0;
bool g_ready = false;

bool png_encoder(CLSID& clsid) {
    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return false;
    std::vector<BYTE> buffer(size);
    auto* codecs = reinterpret_cast<ImageCodecInfo*>(buffer.data());
    if (GetImageEncoders(num, size, codecs) != Ok) return false;
    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(codecs[i].MimeType, L"image/png") == 0) {
            clsid = codecs[i].Clsid;
            return true;
        }
    }
    return false;
}

}  // namespace

bool imaging_init() {
    if (g_ready) return true;
    GdiplusStartupInput input;
    if (GdiplusStartup(&g_token, &input, nullptr) != Ok) return false;
    g_ready = true;
    return true;
}

void imaging_shutdown() {
    if (g_ready) {
        GdiplusShutdown(g_token);
        g_ready = false;
    }
}

std::optional<ImageData> load_image_rgba(const fs::path& path) {
    if (!g_ready) return std::nullopt;
    Bitmap bitmap(path.wstring().c_str(), false);
    if (bitmap.GetLastStatus() != Ok) return std::nullopt;
    int w = (int)bitmap.GetWidth();
    int h = (int)bitmap.GetHeight();
    if (w <= 0 || h <= 0) return std::nullopt;
    // 限制尺寸，避免超大图撑爆
    if (w > 4096 || h > 4096) return std::nullopt;
    Rect rect(0, 0, w, h);
    BitmapData data;
    if (bitmap.LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok) {
        return std::nullopt;
    }
    ImageData out;
    out.width = w;
    out.height = h;
    out.rgba.resize((size_t)w * h * 4);
    for (int y = 0; y < h; ++y) {
        const uint8_t* src = (const uint8_t*)data.Scan0 + (size_t)y * data.Stride;
        uint8_t* dst = out.rgba.data() + (size_t)y * w * 4;
        for (int x = 0; x < w; ++x) {
            dst[x * 4 + 0] = src[x * 4 + 2];
            dst[x * 4 + 1] = src[x * 4 + 1];
            dst[x * 4 + 2] = src[x * 4 + 0];
            dst[x * 4 + 3] = src[x * 4 + 3];
        }
    }
    bitmap.UnlockBits(&data);
    return out;
}

bool save_rgba_png(const fs::path& path, int width, int height,
                   const uint8_t* rgba) {
    if (!g_ready || width <= 0 || height <= 0) return false;
    std::vector<uint8_t> bgra((size_t)width * height * 4);
    for (size_t i = 0; i + 3 < bgra.size(); i += 4) {
        bgra[i + 0] = rgba[i + 2];
        bgra[i + 1] = rgba[i + 1];
        bgra[i + 2] = rgba[i + 0];
        bgra[i + 3] = rgba[i + 3];
    }
    Bitmap bitmap(width, height, width * 4, PixelFormat32bppARGB, bgra.data());
    if (bitmap.GetLastStatus() != Ok) return false;
    CLSID clsid;
    if (!png_encoder(clsid)) return false;
    return bitmap.Save(path.wstring().c_str(), &clsid, nullptr) == Ok;
}

bool save_clipboard_png(const fs::path& path) {
    if (!g_ready) return false;
    if (!OpenClipboard(nullptr)) return false;
    bool ok = false;
    if (HBITMAP hbmp = (HBITMAP)GetClipboardData(CF_BITMAP)) {
        Bitmap* bitmap = Bitmap::FromHBITMAP(hbmp, nullptr);
        if (bitmap && bitmap->GetLastStatus() == Ok) {
            CLSID clsid;
            if (png_encoder(clsid)) {
                ok = bitmap->Save(path.wstring().c_str(), &clsid, nullptr) == Ok;
            }
        }
        delete bitmap;
    }
    CloseClipboard();
    return ok;
}

bool save_sketch_png(const fs::path& path, int width, int height,
                     const std::vector<Stroke>& strokes) {
    if (!g_ready) return false;
    Bitmap bitmap(width, height, PixelFormat32bppARGB);
    Graphics graphics(&bitmap);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.Clear(Color(255, 255, 255, 255));
    for (const auto& stroke : strokes) {
        if (stroke.points.empty()) continue;
        uint32_t c = stroke.color;
        // c 为 RGBA：低字节是 alpha
        Color color((BYTE)(c & 0xFF), (BYTE)((c >> 24) & 0xFF),
                    (BYTE)((c >> 16) & 0xFF), (BYTE)((c >> 8) & 0xFF));
        Pen pen(color, stroke.width);
        pen.SetStartCap(LineCapRound);
        pen.SetEndCap(LineCapRound);
        pen.SetLineJoin(LineJoinRound);
        if (stroke.points.size() == 1) {
            const auto& p = stroke.points[0];
            SolidBrush brush(color);
            graphics.FillEllipse(&brush, p.x - stroke.width / 2,
                                 p.y - stroke.width / 2, stroke.width, stroke.width);
            continue;
        }
        std::vector<PointF> pts;
        pts.reserve(stroke.points.size());
        for (const auto& p : stroke.points) pts.emplace_back(p.x, p.y);
        graphics.DrawLines(&pen, pts.data(), (INT)pts.size());
    }
    CLSID clsid;
    if (!png_encoder(clsid)) return false;
    return bitmap.Save(path.wstring().c_str(), &clsid, nullptr) == Ok;
}

}  // namespace moji
