// 墨记 MoJi · Dear ImGui + DirectX11 图形界面
#include "gui.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <d3d11.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "core/importer.hpp"
#include "core/model.hpp"
#include "core/util.hpp"
#include "imaging.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

namespace moji {

namespace {

// ---------------------------------------------------------------- 调色板
struct Pal {
    ImU32 bg, surface, surfaceAlt, card, cardHover, selected;
    ImU32 text, muted, primary, primaryHover, primarySoft;
    ImU32 border, input, success, successSoft, warning, warningSoft;
    ImU32 error, errorSoft, violet, teal, white, black;
};

ImU32 rgb(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }

Pal light_pal() {
    Pal p{};
    p.bg = rgb(244, 246, 251);
    p.surface = rgb(255, 255, 255);
    p.surfaceAlt = rgb(238, 241, 248);
    p.card = rgb(255, 255, 255);
    p.cardHover = rgb(230, 235, 247);
    p.selected = rgb(231, 236, 255);
    p.text = rgb(28, 33, 48);
    p.muted = rgb(104, 112, 138);
    p.primary = rgb(79, 110, 247);
    p.primaryHover = rgb(63, 92, 224);
    p.primarySoft = rgb(231, 236, 255);
    p.border = rgb(223, 228, 240);
    p.input = rgb(248, 249, 253);
    p.success = rgb(22, 163, 74);
    p.successSoft = rgb(231, 247, 238);
    p.warning = rgb(217, 119, 6);
    p.warningSoft = rgb(254, 243, 225);
    p.error = rgb(220, 38, 38);
    p.errorSoft = rgb(253, 236, 236);
    p.violet = rgb(124, 92, 255);
    p.teal = rgb(14, 148, 136);
    p.white = rgb(255, 255, 255);
    p.black = rgb(0, 0, 0);
    return p;
}

Pal dark_pal() {
    Pal p{};
    p.bg = rgb(14, 17, 23);
    p.surface = rgb(22, 26, 35);
    p.surfaceAlt = rgb(29, 34, 48);
    p.card = rgb(22, 26, 35);
    p.cardHover = rgb(36, 42, 58);
    p.selected = rgb(35, 47, 85);
    p.text = rgb(232, 234, 242);
    p.muted = rgb(147, 156, 179);
    p.primary = rgb(124, 156, 255);
    p.primaryHover = rgb(148, 175, 255);
    p.primarySoft = rgb(35, 47, 85);
    p.border = rgb(42, 49, 66);
    p.input = rgb(17, 21, 29);
    p.success = rgb(74, 222, 128);
    p.successSoft = rgb(22, 50, 31);
    p.warning = rgb(251, 191, 36);
    p.warningSoft = rgb(58, 44, 18);
    p.error = rgb(248, 113, 113);
    p.errorSoft = rgb(58, 28, 31);
    p.violet = rgb(167, 139, 250);
    p.teal = rgb(45, 212, 191);
    p.white = rgb(255, 255, 255);
    p.black = rgb(0, 0, 0);
    return p;
}

ImVec4 v4(ImU32 c) {
    return ImVec4(((c >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
                  ((c >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
                  ((c >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
                  ((c >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f);
}

void apply_style(const Pal& p) {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0;
    s.ChildRounding = 12;
    s.FrameRounding = 10;
    s.PopupRounding = 12;
    s.ScrollbarRounding = 8;
    s.GrabRounding = 8;
    s.TabRounding = 8;
    s.WindowPadding = ImVec2(0, 0);
    s.FramePadding = ImVec2(10, 7);
    s.ItemSpacing = ImVec2(8, 8);
    s.ChildBorderSize = 1;
    s.WindowBorderSize = 0;
    s.ScrollbarSize = 10;
    s.Colors[ImGuiCol_Text] = v4(p.text);
    s.Colors[ImGuiCol_TextDisabled] = v4(p.muted);
    s.Colors[ImGuiCol_WindowBg] = v4(p.bg);
    s.Colors[ImGuiCol_ChildBg] = v4(p.surface);
    s.Colors[ImGuiCol_PopupBg] = v4(p.surface);
    s.Colors[ImGuiCol_Border] = v4(p.border);
    s.Colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    s.Colors[ImGuiCol_FrameBg] = v4(p.input);
    s.Colors[ImGuiCol_FrameBgHovered] = v4(p.surfaceAlt);
    s.Colors[ImGuiCol_FrameBgActive] = v4(p.primarySoft);
    s.Colors[ImGuiCol_TitleBg] = v4(p.surface);
    s.Colors[ImGuiCol_TitleBgActive] = v4(p.surface);
    s.Colors[ImGuiCol_MenuBarBg] = v4(p.surface);
    s.Colors[ImGuiCol_ScrollbarBg] = v4(p.bg);
    s.Colors[ImGuiCol_ScrollbarGrab] = v4(p.border);
    s.Colors[ImGuiCol_ScrollbarGrabHovered] = v4(p.muted);
    s.Colors[ImGuiCol_ScrollbarGrabActive] = v4(p.primary);
    s.Colors[ImGuiCol_CheckMark] = v4(p.success);
    s.Colors[ImGuiCol_SliderGrab] = v4(p.primary);
    s.Colors[ImGuiCol_SliderGrabActive] = v4(p.primaryHover);
    s.Colors[ImGuiCol_Button] = v4(p.surfaceAlt);
    s.Colors[ImGuiCol_ButtonHovered] = v4(p.cardHover);
    s.Colors[ImGuiCol_ButtonActive] = v4(p.border);
    s.Colors[ImGuiCol_Header] = v4(p.primarySoft);
    s.Colors[ImGuiCol_HeaderHovered] = v4(p.cardHover);
    s.Colors[ImGuiCol_HeaderActive] = v4(p.selected);
    s.Colors[ImGuiCol_Separator] = v4(p.border);
    s.Colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
    s.Colors[ImGuiCol_Tab] = v4(p.surfaceAlt);
    s.Colors[ImGuiCol_TabHovered] = v4(p.cardHover);
    s.Colors[ImGuiCol_TabActive] = v4(p.primarySoft);
    s.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.45f);
}

// ---------------------------------------------------------------- 工具
struct TexEntry {
    ID3D11ShaderResourceView* srv = nullptr;
    int width = 0;
    int height = 0;
    fs::file_time_type mtime{};
    bool valid = false;
};

struct GuiState {
    explicit GuiState(Repository& r) : repo(r) {}
    Repository& repo;

    ImFont* body = nullptr;
    ImFont* head = nullptr;
    ImFont* small = nullptr;
    Pal pal = light_pal();
    bool dark = false;

    std::string mode = "all";
    std::string tag_filter;
    char search[256] = {0};
    std::string current_id;

    char title[512] = {0};
    char due[64] = {0};
    bool due_enabled = false;
    int remind = 0;
    char tags[256] = {0};
    char note[16384] = {0};

    bool loading = false;
    bool dirty = false;
    double last_edit = 0;
    bool focus_title = false;
    bool focus_note = false;
    bool focus_search = false;

    std::string status;
    double status_until = 0;
    bool confirm_delete = false;
    bool show_help = false;
    bool show_about = false;
    bool show_sketch = false;

    std::vector<Stroke> strokes;
    Stroke current_stroke;
    bool drawing = false;
    uint32_t pen_color = 0x1F2430FF;
    float pen_width = 4.0f;

    std::vector<std::string> dropped;
    std::map<std::string, TexEntry> textures;

    bool reminder_shown = false;
    std::string reminder_text;
    double last_reminder_check = 0;

    bool screenshot_mode = false;
    std::string screenshot_path;
    int frame_count = 0;
    bool quit = false;

    HWND hwnd = nullptr;
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    IDXGISwapChain* swap = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    UINT resize_w = 0;
    UINT resize_h = 0;

    void set_status(const std::string& text, double seconds = 4.0) {
        status = text;
        status_until = ImGui::GetTime() + seconds;
    }

    std::string search_text() const { return trim(search); }

    std::vector<Event*> visible() {
        std::vector<Event*> base;
        if (mode == "today") base = repo.due_on(now_time());
        else if (mode == "pending") base = repo.pending();
        else if (mode == "done") base = repo.finished();
        else if (mode == "overdue") base = repo.overdue();
        else base = repo.all();
        if (!tag_filter.empty()) {
            std::vector<Event*> filtered;
            for (Event* e : base) {
                if (std::find(e->tags.begin(), e->tags.end(), tag_filter) != e->tags.end()) {
                    filtered.push_back(e);
                }
            }
            base = filtered;
        }
        std::string key = lower_ascii(search_text());
        if (key.empty()) return base;
        std::vector<Event*> out;
        for (Event* e : base) {
            std::string hay = lower_ascii(e->title + " " + e->note + " " + join(e->tags, " "));
            for (const auto& a : e->attachments) hay += " " + lower_ascii(a.name);
            if (hay.find(key) != std::string::npos) out.push_back(e);
        }
        return out;
    }

    Event* current() { return current_id.empty() ? nullptr : repo.find(current_id); }

    void load_event(Event* e) {
        loading = true;
        if (!e) {
            current_id.clear();
            title[0] = due[0] = tags[0] = note[0] = 0;
            due_enabled = false;
            remind = 0;
            loading = false;
            return;
        }
        current_id = e->id;
        std::snprintf(title, sizeof(title), "%s", e->title.c_str());
        std::snprintf(tags, sizeof(tags), "%s", join(e->tags, ", ").c_str());
        std::snprintf(note, sizeof(note), "%s", e->note.c_str());
        due_enabled = e->due.has_value();
        if (e->due.has_value()) {
            std::string s = iso_from_time(*e->due);
            std::replace(s.begin(), s.end(), 'T', ' ');
            if (s.size() > 16) s.resize(16);
            std::snprintf(due, sizeof(due), "%s", s.c_str());
        } else {
            due[0] = 0;
        }
        remind = e->remind_before;
        dirty = false;
        loading = false;
    }

    void select(const std::string& id) {
        if (id == current_id) return;
        save_current();
        Event* e = repo.find(id);
        load_event(e);
    }

    bool save_current() {
        if (loading || current_id.empty()) return true;
        Event* e = repo.find(current_id);
        if (!e) return true;
        std::optional<std::time_t> due_value;
        if (due_enabled) {
            std::string text = trim(due);
            if (text.empty()) {
                set_status("已开启截止时间，请填写时间", 5);
                return false;
            }
            due_value = parse_datetime(text);
            if (!due_value.has_value()) {
                set_status("时间格式应为 YYYY-MM-DD HH:MM", 5);
                return false;
            }
        }
        std::string new_title = trim(title);
        if (new_title.empty()) {
            set_status("标题不能为空", 4);
            return false;
        }
        try {
            repo.update(current_id, new_title, note, due_value, split_tags(tags), remind);
            dirty = false;
            set_status("已保存 · " + clock_str(now_time()), 2);
            return true;
        } catch (const std::exception& ex) {
            set_status(std::string("保存失败：") + ex.what(), 6);
            return false;
        }
    }

    void new_event() {
        save_current();
        try {
            Event& e = repo.add("新记事");
            mode = "all";
            tag_filter.clear();
            search[0] = 0;
            load_event(&e);
            focus_title = true;
            set_status("已新建记事", 2);
        } catch (const std::exception& ex) {
            set_status(std::string("新建失败：") + ex.what(), 5);
        }
    }

    void toggle_done() {
        Event* e = current();
        if (!e) return;
        std::string id = e->id;
        try {
            Event& updated = repo.toggle_done(id);
            set_status(updated.done ? "已完成：" + updated.title : "恢复待办：" + updated.title, 2);
            load_event(&updated);
        } catch (const std::exception& ex) {
            set_status(std::string("操作失败：") + ex.what(), 5);
        }
    }

    void remove_current() {
        if (current_id.empty()) return;
        std::string id = current_id;
        std::vector<Event*> vis = visible();
        int index = -1;
        for (size_t i = 0; i < vis.size(); ++i) {
            if (vis[i]->id == id) index = (int)i;
        }
        if (!repo.remove(id)) return;
        std::vector<Event*> after = visible();
        if (after.empty()) {
            load_event(nullptr);
        } else {
            int next = index >= 0 ? std::min(index, (int)after.size() - 1) : 0;
            load_event(after[(size_t)next]);
        }
        set_status("已删除（Ctrl+Alt+Z 可撤销）", 3);
    }

    void move_current(int delta) {
        Event* e = current();
        if (!e) return;
        try {
            repo.move_by(e->id, delta);
            Event* moved = repo.find(e->id);
            set_status("优先级 → P" + std::to_string(moved ? moved->priority : 0), 2);
        } catch (const std::exception& ex) {
            set_status(std::string("调整失败：") + ex.what(), 5);
        }
    }

    void duplicate_current() {
        Event* e = current();
        if (!e) return;
        try {
            Event& copy = repo.duplicate(e->id);
            load_event(&copy);
            set_status("已复制：" + copy.title, 3);
        } catch (const std::exception& ex) {
            set_status(std::string("复制失败：") + ex.what(), 5);
        }
    }

    void undo(bool redo) {
        bool ok = redo ? repo.redo() : repo.undo();
        Event* e = current();
        if (!e && !repo.events.empty()) {
            load_event(&*repo.events.begin());
        }
        set_status(redo ? (ok ? "已重做" : "没有可重做的操作")
                        : (ok ? "已撤销" : "没有可撤销的操作"),
                   2);
    }

    void attach_files(const std::vector<std::string>& paths) {
        if (current_id.empty()) return;
        int added = 0;
        for (const auto& p : paths) {
#ifdef _WIN32
            fs::path path(to_wide(p));
#else
            fs::path path(p);
#endif
            try {
                repo.add_attachment(current_id, path);
                ++added;
            } catch (const std::exception& ex) {
                set_status(std::string("附件失败：") + ex.what(), 5);
            }
        }
        if (added) set_status("已添加 " + std::to_string(added) + " 个附件", 2);
    }

    void paste_image() {
        if (current_id.empty()) return;
        fs::path tmp = repo.store.dir / "_clipboard.png";
        if (!save_clipboard_png(tmp)) {
            set_status("剪贴板里没有图片", 3);
            return;
        }
        try {
            repo.add_attachment(current_id, tmp);
            set_status("图片已存为附件", 2);
        } catch (const std::exception& ex) {
            set_status(std::string("粘贴失败：") + ex.what(), 5);
        }
        std::error_code ec;
        fs::remove(tmp, ec);
    }

    void check_reminders() {
        double now = ImGui::GetTime();
        if (last_reminder_check == 0) {
            last_reminder_check = now;
        } else if (now - last_reminder_check < 30.0) {
            return;
        }
        last_reminder_check = now;
        auto due = repo.due_reminders(now_time());
        if (due.empty()) return;
        MessageBeep(MB_ICONEXCLAMATION);
        std::string text;
        for (size_t i = 0; i < due.size() && i < 5; ++i) {
            if (i) text += "\n";
            text += "• " + due[i]->title + "（" + humanize_due(due[i]->due) + "）";
        }
        if (due.size() > 5) text += "\n……以及另外 " + std::to_string(due.size() - 5) + " 项";
        reminder_text = text;
        reminder_shown = true;
        repo.mark_reminded(due, now_time());
    }
};

GuiState* g_app = nullptr;

// ---------------------------------------------------------------- D3D11
bool create_device(GuiState& st, HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    UINT flags = 0;
    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, 2,
        D3D11_SDK_VERSION, &sd, &st.swap, &st.dev, &level, &st.ctx);
    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, levels, 2,
            D3D11_SDK_VERSION, &sd, &st.swap, &st.dev, &level, &st.ctx);
    }
    if (FAILED(hr)) return false;
    ID3D11Texture2D* back = nullptr;
    if (FAILED(st.swap->GetBuffer(0, IID_PPV_ARGS(&back)))) return false;
    st.dev->CreateRenderTargetView(back, nullptr, &st.rtv);
    back->Release();
    return st.rtv != nullptr;
}

void cleanup_rtv(GuiState& st) {
    if (st.rtv) {
        st.rtv->Release();
        st.rtv = nullptr;
    }
}

void cleanup_device(GuiState& st) {
    cleanup_rtv(st);
    if (st.swap) {
        st.swap->Release();
        st.swap = nullptr;
    }
    if (st.ctx) {
        st.ctx->Release();
        st.ctx = nullptr;
    }
    if (st.dev) {
        st.dev->Release();
        st.dev = nullptr;
    }
}

void release_textures(GuiState& st) {
    for (auto& kv : st.textures) {
        if (kv.second.srv) kv.second.srv->Release();
    }
    st.textures.clear();
}

ID3D11ShaderResourceView* texture_for(GuiState& st, const Attachment& att) {
    if (!att.is_image()) return nullptr;
    fs::path path;
    try {
        path = st.repo.store.resolve(att);
    } catch (...) {
        return nullptr;
    }
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) return nullptr;
    auto mtime = fs::last_write_time(path, ec);
    auto it = st.textures.find(att.path);
    if (it != st.textures.end() && it->second.valid && !ec && it->second.mtime == mtime) {
        return it->second.srv;
    }
    auto img = load_image_rgba(path);
    if (!img.has_value()) return nullptr;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = (UINT)img->width;
    desc.Height = (UINT)img->height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = img->rgba.data();
    init.SysMemPitch = (UINT)img->width * 4;
    ID3D11Texture2D* tex = nullptr;
    if (FAILED(st.dev->CreateTexture2D(&desc, &init, &tex))) return nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    st.dev->CreateShaderResourceView(tex, nullptr, &srv);
    tex->Release();
    if (!srv) return nullptr;
    if (it != st.textures.end() && it->second.srv) it->second.srv->Release();
    TexEntry entry;
    entry.srv = srv;
    entry.width = img->width;
    entry.height = img->height;
    entry.mtime = mtime;
    entry.valid = true;
    st.textures[att.path] = entry;
    return srv;
}

bool capture_backbuffer(GuiState& st, const fs::path& path) {
    ID3D11Texture2D* back = nullptr;
    if (FAILED(st.swap->GetBuffer(0, IID_PPV_ARGS(&back)))) return false;
    D3D11_TEXTURE2D_DESC desc{};
    back->GetDesc(&desc);
    D3D11_TEXTURE2D_DESC sdesc = desc;
    sdesc.Usage = D3D11_USAGE_STAGING;
    sdesc.BindFlags = 0;
    sdesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    sdesc.MiscFlags = 0;
    ID3D11Texture2D* staging = nullptr;
    if (FAILED(st.dev->CreateTexture2D(&sdesc, nullptr, &staging))) {
        back->Release();
        return false;
    }
    st.ctx->CopyResource(staging, back);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    bool ok = false;
    if (SUCCEEDED(st.ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
        std::vector<uint8_t> rgba((size_t)desc.Width * desc.Height * 4);
        for (UINT y = 0; y < desc.Height; ++y) {
            const uint8_t* src = (const uint8_t*)mapped.pData + (size_t)y * mapped.RowPitch;
            uint8_t* dst = rgba.data() + (size_t)y * desc.Width * 4;
            for (UINT x = 0; x < desc.Width; ++x) {
                dst[x * 4 + 0] = src[x * 4 + 0];
                dst[x * 4 + 1] = src[x * 4 + 1];
                dst[x * 4 + 2] = src[x * 4 + 2];
                dst[x * 4 + 3] = 255;
            }
        }
        st.ctx->Unmap(staging, 0);
        ok = save_rgba_png(path, (int)desc.Width, (int)desc.Height, rgba.data());
    }
    staging->Release();
    back->Release();
    return ok;
}

// ---------------------------------------------------------------- 小工具
std::string ellipsize(const std::string& text, float max_width) {
    if (ImGui::CalcTextSize(text.c_str()).x <= max_width) return text;
    std::string out;
    size_t i = 0;
    while (i < text.size()) {
        unsigned char c = (unsigned char)text[i];
        size_t len = 1;
        if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        if (i + len > text.size()) break;
        std::string candidate = out + text.substr(i, len);
        if (ImGui::CalcTextSize((candidate + "…").c_str()).x > max_width) break;
        out = candidate;
        i += len;
    }
    return out + "…";
}

float chip(ImDrawList* dl, ImVec2 pos, const std::string& text, ImU32 fg, ImU32 bg) {
    ImVec2 ts = ImGui::CalcTextSize(text.c_str());
    float w = ts.x + 14.0f;
    float h = ts.y + 4.0f;
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), bg, 7.0f);
    dl->AddText(ImVec2(pos.x + 7, pos.y + 2), fg, text.c_str());
    return w;
}

ImU32 hex_color(const std::string& text, ImU32 fallback) {
    std::string s = text;
    if (!s.empty() && s[0] == '#') s.erase(s.begin());
    if (s.size() != 6) return fallback;
    try {
        int v = std::stoi(s, nullptr, 16);
        return IM_COL32((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF, 255);
    } catch (...) {
        return fallback;
    }
}

ImU32 im32(uint32_t rgba) {
    return IM_COL32((rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF,
                    rgba & 0xFF);
}

ImU32 priority_color(int p) {
    static const ImU32 colors[] = {
        IM_COL32(239, 68, 68, 255), IM_COL32(249, 115, 22, 255),
        IM_COL32(234, 179, 8, 255), IM_COL32(34, 197, 94, 255),
        IM_COL32(6, 182, 212, 255), IM_COL32(99, 102, 241, 255)};
    return colors[(std::max(1, p) - 1) % 6];
}

ImU32 priority_soft(int p) {
    static const ImU32 colors[] = {
        IM_COL32(254, 226, 226, 255), IM_COL32(255, 237, 213, 255),
        IM_COL32(254, 249, 195, 255), IM_COL32(220, 252, 231, 255),
        IM_COL32(207, 250, 254, 255), IM_COL32(224, 231, 255, 255)};
    return colors[(std::max(1, p) - 1) % 6];
}

bool primary_button(const char* label, ImVec2 size = ImVec2(0, 0)) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(IM_COL32(79, 110, 247, 255)));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImGui::GetColorU32(IM_COL32(63, 92, 224, 255)));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(IM_COL32(255, 255, 255, 255)));
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return clicked;
}

void open_path(const fs::path& path) {
    ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

std::optional<fs::path> open_dialog(HWND owner, const wchar_t* filter) {
    wchar_t buffer[1024] = {0};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = 1024;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return std::nullopt;
    return fs::path(buffer);
}

std::optional<fs::path> save_dialog(HWND owner, const wchar_t* filter,
                                    const wchar_t* def_ext,
                                    const std::wstring& default_name) {
    wchar_t buffer[1024] = {0};
    wcsncpy(buffer, default_name.c_str(), 1023);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = 1024;
    ofn.lpstrDefExt = def_ext;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&ofn)) return std::nullopt;
    return fs::path(buffer);
}

// ---------------------------------------------------------------- 字体 / 样式
void load_fonts(GuiState& st) {
    ImGuiIO& io = ImGui::GetIO();
    static ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    builder.AddText("…·「」【】✓✎←→↑↓●○逾期编辑");
    builder.BuildRanges(&ranges);

    const char* candidates[] = {"C:\\Windows\\Fonts\\msyh.ttc", "C:\\Windows\\Fonts\\simsun.ttc",
                                "C:\\Windows\\Fonts\\msyh.ttf"};
    for (const char* file : candidates) {
        if (GetFileAttributesA(file) == INVALID_FILE_ATTRIBUTES) continue;
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.PixelSnapH = true;
        cfg.FontNo = 0;
        st.body = io.Fonts->AddFontFromFileTTF(file, 18.0f, &cfg, ranges.Data);
        cfg.FontNo = 0;
        st.head = io.Fonts->AddFontFromFileTTF(file, 26.0f, &cfg, ranges.Data);
        st.small = io.Fonts->AddFontFromFileTTF(file, 14.0f, &cfg, ranges.Data);
        if (st.body) break;
    }
    if (!st.body) {
        st.body = io.Fonts->AddFontDefault();
        st.head = st.body;
        st.small = st.body;
    }
    io.FontDefault = st.body;
}

// ---------------------------------------------------------------- 主界面
void draw_sidebar(GuiState& st) {
    Pal& p = st.pal;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, p.surface);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 18));
    ImGui::BeginChild("##sidebar", ImVec2(252, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    // 品牌
    ImVec2 mark = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(mark, mark + ImVec2(50, 50), p.primary, 16);
    ImGui::PushFont(st.head);
    ImGui::GetWindowDrawList()->AddText(mark + ImVec2(12, 10), p.white, "墨");
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(mark + ImVec2(62, 2));
    ImGui::PushFont(st.head);
    ImGui::TextUnformatted("墨记 MoJi");
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(mark + ImVec2(62, 32));
    ImGui::PushStyleColor(ImGuiCol_Text, p.muted);
    ImGui::TextUnformatted("电子记事本");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 12));

    Stats stats = st.repo.stats();
    float card_w = (ImGui::GetContentRegionAvail().x - 8) / 2;
    auto stat_card = [&](const char* id, const std::string& value, const char* caption,
                         ImU32 color) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, p.surfaceAlt);
        ImGui::BeginChild(id, ImVec2(card_w, 58), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar);
        ImGui::SetCursorPos(ImVec2(12, 8));
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::PushFont(st.head);
        ImGui::TextUnformatted(value.c_str());
        ImGui::PopFont();
        ImGui::PopStyleColor();
        ImGui::SetCursorPos(ImVec2(12, 36));
        ImGui::PushStyleColor(ImGuiCol_Text, p.muted);
        ImGui::TextUnformatted(caption);
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    };
    stat_card("##s1", std::to_string(stats.today), "今日安排", p.primary);
    ImGui::SameLine(0, 8);
    stat_card("##s2", std::to_string(stats.pending), "待办", p.text);
    stat_card("##s3", std::to_string(stats.overdue), "逾期",
              stats.overdue ? p.error : p.muted);
    ImGui::SameLine(0, 8);
    stat_card("##s4", std::to_string(stats.rate) + "%", "完成率", p.success);
    ImGui::Dummy(ImVec2(0, 10));

    auto nav = [&](const char* label, const std::string& mode, int count) {
        bool selected = st.mode == mode && st.tag_filter.empty();
        ImGui::PushStyleColor(ImGuiCol_Text, selected ? p.primary : p.muted);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Header, p.primarySoft);
        std::string text = std::string(label) + "##nav_" + mode;
        if (ImGui::Selectable(text.c_str(), selected, ImGuiSelectableFlags_None,
                              ImVec2(0, 32))) {
            st.mode = mode;
            st.tag_filter.clear();
        }
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        std::string num = std::to_string(count);
        float tw = ImGui::CalcTextSize(num.c_str()).x;
        ImGui::GetWindowDrawList()->AddText(ImVec2(max.x - tw - 10, min.y + 6),
                                            selected ? p.primary : p.muted, num.c_str());
        if (selected) ImGui::PopStyleColor();
        ImGui::PopStyleColor();
    };
    ImGui::PushStyleColor(ImGuiCol_Text, p.muted);
    ImGui::TextUnformatted("视图");
    ImGui::PopStyleColor();
    nav("全部", "all", stats.total);
    nav("今天", "today", stats.today);
    nav("待办", "pending", stats.pending);
    nav("已完成", "done", stats.done);
    nav("逾期", "overdue", stats.overdue);
    ImGui::Dummy(ImVec2(0, 8));

    ImGui::PushStyleColor(ImGuiCol_Text, p.muted);
    ImGui::TextUnformatted("标签");
    ImGui::PopStyleColor();
    auto tags = st.repo.tags();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, p.surface);
    ImGui::BeginChild("##tags", ImVec2(0, 150), ImGuiChildFlags_None);
    if (tags.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, p.muted);
        ImGui::TextUnformatted("暂无标签");
        ImGui::PopStyleColor();
    } else {
        int shown = 0;
        for (const auto& kv : tags) {
            if (++shown > 20) break;
            bool selected = st.tag_filter == kv.first;
            ImGui::PushStyleColor(ImGuiCol_Text, selected ? p.primary : p.muted);
            if (selected) ImGui::PushStyleColor(ImGuiCol_Header, p.primarySoft);
            std::string label = "#" + kv.first;
            if (ImGui::Selectable((label + "##tag_" + kv.first).c_str(), selected,
                                  ImGuiSelectableFlags_None, ImVec2(0, 26))) {
                st.tag_filter = selected ? "" : kv.first;
                st.mode = "all";
                st.save_current();
            }
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            std::string num = std::to_string(kv.second);
            float tw = ImGui::CalcTextSize(num.c_str()).x;
            ImGui::GetWindowDrawList()->AddText(ImVec2(max.x - tw - 8, min.y + 3),
                                                selected ? p.primary : p.muted, num.c_str());
            if (selected) ImGui::PopStyleColor();
            ImGui::PopStyleColor();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // 底部
    float bottom_h = 118.0f;
    float spacer = ImGui::GetContentRegionAvail().y - bottom_h;
    if (spacer > 0) ImGui::Dummy(ImVec2(0, spacer));
    if (ImGui::Button(st.dark ? "浅色主题" : "深色主题", ImVec2(100, 30))) {
        st.dark = !st.dark;
        st.pal = st.dark ? dark_pal() : light_pal();
        apply_style(st.pal);
        json cfg = st.repo.store.load_config();
        cfg["theme"] = st.dark ? "dark" : "light";
        st.repo.store.save_config(cfg);
    }
    ImGui::SameLine();
    if (ImGui::Button("数据目录", ImVec2(100, 30))) {
        open_path(st.repo.store.dir);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", st.repo.display_dir().c_str());
    }
    ImGui::PushStyleColor(ImGuiCol_Text, p.muted);
    ImGui::PushFont(st.small);
    ImGui::TextUnformatted("数据保存在本地");
    ImGui::TextUnformatted("v1.0.0 · C++17 / ImGui");
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void draw_card(GuiState& st, Event* e, float width) {
    ImGui::PushID(e->id.c_str());
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = 86.0f;
    ImGui::InvisibleButton("##card", ImVec2(width, h));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked(0);
    bool dbl = hovered && ImGui::IsMouseDoubleClicked(0);
    bool selected = st.current_id == e->id;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 bg = selected ? st.pal.selected : (hovered ? st.pal.cardHover : st.pal.card);
    dl->AddRectFilled(p, p + ImVec2(width, h), bg, 12);
    if (selected) dl->AddRect(p, p + ImVec2(width, h), st.pal.primary, 12, 0, 2);

    ImU32 accent = e->done ? st.pal.muted
                           : (e->is_overdue() ? st.pal.error
                                              : (e->color.empty() ? priority_color(e->priority)
                                                                  : hex_color(e->color, priority_color(e->priority))));
    dl->AddRectFilled(p + ImVec2(12, 18), p + ImVec2(16, 68), accent, 2);

    ImVec2 cb = p + ImVec2(26, 20);
    dl->AddRectFilled(cb, cb + ImVec2(18, 18), e->done ? st.pal.success : st.pal.input, 5);
    if (e->done) {
        dl->AddText(cb + ImVec2(4, 0), st.pal.white, "✓");
    } else {
        dl->AddRect(cb, cb + ImVec2(18, 18), st.pal.border, 5);
    }

    ImGui::PushClipRect(p, p + ImVec2(width - 74, h), true);
    dl->AddText(p + ImVec2(52, 12), e->done ? st.pal.muted : st.pal.text,
                ellipsize(e->title, width - 130).c_str());
    ImGui::PopClipRect();

    std::string badge = "P" + std::to_string(e->priority);
    ImU32 pcol = priority_color(e->priority);
    float badge_w = ImGui::CalcTextSize(badge.c_str()).x + 12;
    dl->AddRectFilled(ImVec2(p.x + width - badge_w - 14, p.y + 12),
                      ImVec2(p.x + width - 14, p.y + 34), priority_soft(e->priority), 7);
    dl->AddText(ImVec2(p.x + width - badge_w - 8, p.y + 16), pcol, badge.c_str());

    float x = 52;
    ImU32 due_fg = e->done ? st.pal.muted
                           : (e->is_overdue() ? st.pal.error
                                              : (e->is_due_today() ? st.pal.warning : st.pal.muted));
    ImU32 due_bg = e->done ? st.pal.surfaceAlt
                           : (e->is_overdue() ? st.pal.errorSoft
                                              : (e->is_due_today() ? st.pal.warningSoft
                                                                   : st.pal.surfaceAlt));
    x += chip(dl, ImVec2(p.x + x, p.y + 45), humanize_due(e->due), due_fg, due_bg) + 6;
    for (size_t i = 0; i < e->tags.size() && i < 2 && x < width - 90; ++i) {
        float cw = chip(dl, ImVec2(p.x + x, p.y + 45), "#" + e->tags[i], st.pal.violet,
                        st.pal.surfaceAlt);
        x += cw + 6;
    }
    if (!e->attachments.empty() && x < width - 40) {
        chip(dl, ImVec2(p.x + x, p.y + 45),
             "附件 " + std::to_string(e->attachments.size()), st.pal.teal, st.pal.surfaceAlt);
    }

    ImVec2 mouse = ImGui::GetIO().MousePos;
    bool on_checkbox = mouse.x >= cb.x && mouse.x <= cb.x + 18 && mouse.y >= cb.y &&
                       mouse.y <= cb.y + 18;
    if (clicked) {
        if (on_checkbox) {
            try {
                Event& updated = st.repo.toggle_done(e->id);
                st.set_status(updated.done ? "已完成：" + updated.title
                                           : "恢复待办：" + updated.title,
                              2);
            } catch (const std::exception& ex) {
                st.set_status(std::string("操作失败：") + ex.what(), 4);
            }
        } else {
            st.select(e->id);
            if (dbl) st.focus_note = true;
        }
    }
    ImGui::Dummy(ImVec2(0, 8));
    ImGui::PopID();
}

void draw_center(GuiState& st) {
    Pal& p = st.pal;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, p.bg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 18));
    ImGui::BeginChild("##center", ImVec2(420, 0), ImGuiChildFlags_None);
    ImGui::PopStyleVar();

    ImGui::PushFont(st.head);
    ImGui::TextUnformatted("记事");
    ImGui::PopFont();
    float new_w = 92;
    ImGui::SameLine(ImGui::GetWindowWidth() - new_w - 24);
    if (primary_button("+ 新建", ImVec2(new_w, 34))) {
        st.new_event();
    }
    ImGui::Dummy(ImVec2(0, 4));

    ImGui::SetNextItemWidth(-1);
    if (st.focus_search) {
        ImGui::SetKeyboardFocusHere();
        st.focus_search = false;
    }
    ImGui::InputTextWithHint("##search", "搜索标题、标签、正文…", st.search,
                             sizeof(st.search));
    ImGui::PushStyleColor(ImGuiCol_Text, p.muted);
    ImGui::PushFont(st.small);
    auto vis = st.visible();
    std::string info = "共 " + std::to_string(vis.size()) + " 条 · 按优先级排序";
    if (!st.tag_filter.empty()) info += " · 标签 #" + st.tag_filter;
    if (st.search_text().size()) info += " · 搜索 “" + st.search_text() + "”";
    ImGui::TextUnformatted(info.c_str());
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 2));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, p.bg);
    ImGui::BeginChild("##cards", ImVec2(0, 0), ImGuiChildFlags_None);
    if (vis.empty()) {
        ImGui::Dummy(ImVec2(0, 60));
        ImGui::PushStyleColor(ImGuiCol_Text, p.muted);
        ImGui::TextUnformatted("没有符合条件的记事");
        ImGui::TextUnformatted("点击右上角「+ 新建」开始记录");
        ImGui::PopStyleColor();
    }
    for (Event* e : vis) {
        draw_card(st, e, ImGui::GetContentRegionAvail().x);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void draw_attachments(GuiState& st, Event* e, float height) {
    Pal& p = st.pal;
    ImGui::PushStyleColor(ImGuiCol_Text, p.muted);
    ImGui::TextUnformatted("附件");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, p.muted);
    ImGui::PushFont(st.small);
    ImGui::TextUnformatted("拖入文件 · 双击打开 · 右键移除");
    ImGui::PopFont();
    ImGui::PopStyleColor();
    float bx = ImGui::GetWindowWidth() - 220;
    ImGui::SameLine(bx);
    if (ImGui::Button("添加文件", ImVec2(70, 24))) {
        auto path = open_dialog(st.hwnd, L"所有文件\0*.*\0图片\0*.png;*.jpg;*.jpeg;*.gif;*.bmp\0");
        if (path) st.attach_files({path->u8string().empty() ? "" : ""});
    }
    ImGui::SameLine();
    if (ImGui::Button("粘贴图片", ImVec2(70, 24))) st.paste_image();
    ImGui::SameLine();
    if (ImGui::Button("手绘", ImVec2(52, 24))) {
        st.strokes.clear();
        st.show_sketch = true;
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, p.input);
    ImGui::BeginChild("##attgrid", ImVec2(0, height), ImGuiChildFlags_None);
    if (!e || e->attachments.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, p.muted);
        ImGui::TextUnformatted("暂无附件。可拖入图片、音频或任意文件。");
        ImGui::PopStyleColor();
    } else {
        float avail = ImGui::GetContentRegionAvail().x;
        float used = 0;
        bool removed = false;
        for (size_t i = 0; i < e->attachments.size(); ++i) {
            const Attachment& att = e->attachments[i];
            ImGui::PushID((int)i);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, p.surfaceAlt);
            ImGui::BeginChild("##att", ImVec2(104, 112), ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoScrollbar);
            ID3D11ShaderResourceView* srv = texture_for(st, att);
            if (srv) {
                ImGui::Image((ImTextureID)(intptr_t)srv, ImVec2(88, 60));
            } else {
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(pos, pos + ImVec2(88, 60),
                                                          st.pal.primarySoft, 8);
                std::string ext = att.name;
                auto dot = ext.find_last_of('.');
                if (dot != std::string::npos) ext = ext.substr(dot + 1);
                for (char& c : ext) c = (char)std::toupper((unsigned char)c);
                ImGui::PushFont(st.head);
                ImGui::SetCursorScreenPos(pos + ImVec2(18, 14));
                ImGui::TextUnformatted(ext.c_str());
                ImGui::PopFont();
                ImGui::SetCursorScreenPos(pos + ImVec2(0, 64));
            }
            ImGui::PushFont(st.small);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 96);
            ImGui::TextUnformatted(ellipsize(att.name, 96).c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopFont();
            bool dbl = ImGui::IsWindowHovered() && ImGui::IsMouseDoubleClicked(0);
            if (ImGui::BeginPopupContextItem("##ctx")) {
                if (ImGui::MenuItem("打开")) {
                    try {
                        open_path(st.repo.store.resolve(att));
                    } catch (...) {
                    }
                }
                if (ImGui::MenuItem("移除")) {
                    try {
                        st.repo.remove_attachment(e->id, (int)i);
                        st.set_status("已移除附件", 2);
                        removed = true;
                    } catch (const std::exception& ex) {
                        st.set_status(std::string("移除失败：") + ex.what(), 4);
                    }
                }
                ImGui::EndPopup();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            if (dbl) {
                try {
                    open_path(st.repo.store.resolve(att));
                } catch (...) {
                }
            }
            ImGui::PopID();
            if (removed) break;
            used += 112;
            if (used + 104 < avail) ImGui::SameLine();
            else used = 0;
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void draw_editor(GuiState& st) {
    Pal& p = st.pal;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, p.surface);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26, 20));
    ImGui::BeginChild("##editor", ImVec2(0, 0), ImGuiChildFlags_None);
    ImGui::PopStyleVar();

    Event* e = st.current();
    if (!e) {
        ImGui::Dummy(ImVec2(0, 40));
        ImGui::PushStyleColor(ImGuiCol_Text, p.muted);
        ImGui::TextUnformatted("选择左侧记事，或点击「+ 新建」。");
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    ImGui::PushFont(st.head);
    if (st.focus_title) {
        ImGui::SetKeyboardFocusHere();
        st.focus_title = false;
    }
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##title", st.title, sizeof(st.title),
                         ImGuiInputTextFlags_AutoSelectAll)) {
        st.dirty = true;
        st.last_edit = ImGui::GetTime();
    }
    ImGui::PopFont();

    ImGui::PushStyleColor(ImGuiCol_Text, p.muted);
    ImGui::PushFont(st.small);
    std::string meta = "P" + std::to_string(e->priority) + "  ·  截止 " +
                       humanize_due(e->due) + "  ·  创建于 " +
                       (e->created_at.has_value() ? date_str(*e->created_at) : "");
    if (!e->attachments.empty()) meta += "  ·  附件 " + std::to_string(e->attachments.size());
    ImGui::TextUnformatted(meta.c_str());
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 2));

    // 截止与提醒
    bool due_changed = ImGui::Checkbox("截止", &st.due_enabled);
    if (due_changed && st.due_enabled && trim(st.due).empty()) {
        std::string s = iso_from_time(now_time() + 3600);
        std::replace(s.begin(), s.end(), 'T', ' ');
        if (s.size() > 16) s.resize(16);
        std::snprintf(st.due, sizeof(st.due), "%s", s.c_str());
        st.dirty = true;
        st.last_edit = ImGui::GetTime();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(176);
    if (ImGui::InputText("##due", st.due, sizeof(st.due))) {
        st.dirty = true;
        st.last_edit = ImGui::GetTime();
    }
    if (st.due_enabled && !trim(st.due).empty() && !parse_datetime(st.due).has_value()) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, p.error);
        ImGui::TextUnformatted("格式应为 YYYY-MM-DD HH:MM");
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("提前");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    if (ImGui::InputInt("##remind", &st.remind, 0, 0)) {
        if (st.remind < 0) st.remind = 0;
        st.dirty = true;
        st.last_edit = ImGui::GetTime();
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("分钟");

    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##tags", "标签：课程, 截止（逗号分隔）", st.tags,
                                 sizeof(st.tags))) {
        st.dirty = true;
        st.last_edit = ImGui::GetTime();
    }

    // 工具栏
    float bw = 64;
    if (primary_button("保存", ImVec2(bw, 32))) {
        st.save_current();
    }
    ImGui::SameLine();
    if (ImGui::Button(e->done ? "取消完成" : "标记完成", ImVec2(84, 32))) {
        st.toggle_done();
    }
    ImGui::SameLine();
    if (ImGui::Button("复制", ImVec2(56, 32))) st.duplicate_current();
    ImGui::SameLine();
    if (ImGui::Button("↑", ImVec2(32, 32))) st.move_current(-1);
    ImGui::SameLine();
    if (ImGui::Button("↓", ImVec2(32, 32))) st.move_current(1);
    ImGui::SameLine();
    if (ImGui::Button("撤销", ImVec2(56, 32))) st.undo(false);
    ImGui::SameLine();
    if (ImGui::Button("重做", ImVec2(56, 32))) st.undo(true);
    ImGui::SameLine(ImGui::GetWindowWidth() - 90);
    ImGui::PushStyleColor(ImGuiCol_Button, p.errorSoft);
    ImGui::PushStyleColor(ImGuiCol_Text, p.error);
    if (ImGui::Button("删除", ImVec2(64, 32))) st.confirm_delete = true;
    ImGui::PopStyleColor(2);

    ImGui::PushStyleColor(ImGuiCol_Text, p.muted);
    ImGui::TextUnformatted("备注（支持 Markdown）");
    ImGui::PopStyleColor();
    float avail_y = ImGui::GetContentRegionAvail().y;
    float note_h = std::max(120.0f, avail_y - 250.0f);
    if (st.focus_note) {
        ImGui::SetKeyboardFocusHere();
        st.focus_note = false;
    }
    if (ImGui::InputTextMultiline("##note", st.note, sizeof(st.note), ImVec2(-1, note_h))) {
        st.dirty = true;
        st.last_edit = ImGui::GetTime();
    }
    draw_attachments(st, e, 120);

    ImGui::PushFont(st.small);
    if (!st.status.empty() && ImGui::GetTime() < st.status_until) {
        ImGui::PushStyleColor(ImGuiCol_Text, p.primary);
        ImGui::TextUnformatted(st.status.c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, p.muted);
        ImGui::TextUnformatted("修改会自动保存 · Ctrl+S 立即保存");
        ImGui::PopStyleColor();
    }
    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------- 弹窗
void draw_sketch(GuiState& st) {
    if (!st.show_sketch) return;
    ImGui::SetNextWindowSize(ImVec2(820, 590), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("手绘笔迹", &st.show_sketch, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    static const uint32_t colors[] = {0x1F2430FF, 0xEF4444FF, 0x4F6EF7FF,
                                      0x16A34AFF, 0xF97316FF, 0x7C5CFFFF};
    for (int i = 0; i < 6; ++i) {
        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button, v4(im32(colors[i])));
        if (ImGui::Button("##sw", ImVec2(24, 24))) st.pen_color = colors[i];
        ImGui::PopStyleColor();
        ImGui::PopID();
        ImGui::SameLine();
    }
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("粗细", &st.pen_width, 1.0f, 18.0f, "%.0f");
    ImGui::SameLine();
    if (ImGui::Button("撤销")) {
        if (!st.strokes.empty()) st.strokes.pop_back();
    }
    ImGui::SameLine();
    if (ImGui::Button("清空")) st.strokes.clear();

    ImVec2 canvas_size(760, 400);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##canvas", canvas_size);
    bool active = ImGui::IsItemActive();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, p + canvas_size, st.pal.white, 8);
    dl->PushClipRect(p, p + canvas_size, true);
    auto draw_stroke = [&](const Stroke& s) {
        if (s.points.size() < 2) {
            if (!s.points.empty()) {
                dl->AddCircleFilled(ImVec2(p.x + s.points[0].x, p.y + s.points[0].y),
                                    s.width / 2, im32(s.color));
            }
            return;
        }
        std::vector<ImVec2> pts;
        pts.reserve(s.points.size());
        for (const auto& pt : s.points) pts.emplace_back(p.x + pt.x, p.y + pt.y);
        dl->AddPolyline(pts.data(), (int)pts.size(), im32(s.color), ImDrawFlags_None,
                        s.width);
    };
    for (const auto& s : st.strokes) draw_stroke(s);
    if (active) draw_stroke(st.current_stroke);
    dl->PopClipRect();

    if (ImGui::IsItemActivated()) {
        st.current_stroke = Stroke();
        st.current_stroke.color = st.pen_color;
        st.current_stroke.width = st.pen_width;
        st.drawing = true;
    }
    if (active && ImGui::IsMouseDown(0)) {
        ImVec2 m = ImGui::GetIO().MousePos;
        ImVec2 local(m.x - p.x, m.y - p.y);
        if (local.x >= 0 && local.y >= 0 && local.x <= canvas_size.x &&
            local.y <= canvas_size.y) {
            st.current_stroke.points.push_back({local.x, local.y});
        }
    }
    if (!ImGui::IsMouseDown(0) && st.drawing) {
        if (!st.current_stroke.points.empty()) st.strokes.push_back(st.current_stroke);
        st.current_stroke = Stroke();
        st.drawing = false;
    }

    ImGui::Dummy(ImVec2(0, 4));
    if (primary_button("保存为附件", ImVec2(120, 32))) {
        fs::path tmp = st.repo.store.dir / "_sketch.png";
        if (save_sketch_png(tmp, 760, 400, st.strokes)) {
            try {
                st.repo.add_attachment(st.current_id, tmp);
                st.set_status("笔迹已保存为附件", 3);
                st.show_sketch = false;
            } catch (const std::exception& ex) {
                st.set_status(std::string("保存失败：") + ex.what(), 5);
            }
        }
        std::error_code ec;
        fs::remove(tmp, ec);
    }
    ImGui::SameLine();
    if (ImGui::Button("取消", ImVec2(80, 32))) st.show_sketch = false;
    ImGui::End();
}

void draw_modals(GuiState& st) {
    if (st.confirm_delete) {
        ImGui::OpenPopup("删除记事");
        st.confirm_delete = false;
    }
    if (ImGui::BeginPopupModal("删除记事", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        Event* e = st.current();
        ImGui::TextUnformatted(e ? ("确定删除「" + e->title + "」？").c_str()
                                 : "确定删除这条记事？");
        ImGui::TextDisabled("删除后可用 Ctrl+Alt+Z 撤销");
        ImGui::Dummy(ImVec2(0, 8));
        if (ImGui::Button("取消", ImVec2(90, 30))) ImGui::CloseCurrentPopup();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, v4(st.pal.error));
        ImGui::PushStyleColor(ImGuiCol_Text, v4(st.pal.white));
        if (ImGui::Button("删除", ImVec2(90, 30))) {
            st.remove_current();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::EndPopup();
    }

    if (st.reminder_shown) {
        ImGui::OpenPopup("待办提醒");
        st.reminder_shown = false;
    }
    if (ImGui::BeginPopupModal("待办提醒", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("到点的记事：");
        ImGui::TextWrapped("%s", st.reminder_text.c_str());
        ImGui::Dummy(ImVec2(0, 8));
        if (primary_button("知道了", ImVec2(120, 32))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (st.show_help) {
        ImGui::SetNextWindowSize(ImVec2(560, 420), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("帮助 · 快捷键", &st.show_help)) {
            ImGui::TextUnformatted("Ctrl+N 新建，Ctrl+S 保存，Ctrl+F 搜索");
            ImGui::TextUnformatted("Ctrl+Alt+Z / Ctrl+Alt+Y 撤销 / 重做");
            ImGui::TextUnformatted("F1 帮助，F5 刷新");
            ImGui::Separator();
            ImGui::TextWrapped(
                "附件：把文件拖进窗口即可添加到当前记事；"
                "双击缩略图用系统程序打开；右键移除；"
                "「粘贴图片」读取剪贴板；「手绘」可画笔迹。");
            ImGui::TextWrapped(
                "数据保存在本地 ~/.moji：events.json 为 UTF-8 文本，附件按事件分目录，"
                "自动备份保留最近 20 份。");
            ImGui::Separator();
            ImGui::TextWrapped("优先级：在第 N 位插入新事件后，原第 N 位及之后的事件自动 +1。");
        }
        ImGui::End();
    }
    if (st.show_about) {
        ImGui::SetNextWindowSize(ImVec2(460, 260), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("关于墨记", &st.show_about)) {
            ImGui::TextUnformatted("墨记 MoJi 1.0.0");
            ImGui::TextWrapped(
                "课程设计《电子记事本的实现》C++17 版本："
                "单向优先级链表、UTF-8 文本存储、TUI + ImGui GUI、附件与手绘、"
                "四种导入导出格式。");
            ImGui::TextWrapped("数据目录：%s", st.repo.display_dir().c_str());
        }
        ImGui::End();
    }
}

void draw_menus(GuiState& st) {
    if (!ImGui::BeginMenuBar()) return;
    if (ImGui::BeginMenu("文件")) {
        if (ImGui::MenuItem("新建记事", "Ctrl+N")) st.new_event();
        if (ImGui::MenuItem("导入…")) {
            auto path = open_dialog(st.hwnd, L"墨记数据\0*.mjnote;*.json;*.txt\0所有文件\0*.*\0");
            if (path) {
                try {
                    ImportReport r = import_auto(st.repo, *path, true);
                    st.set_status("导入完成：新增 " + std::to_string(r.added) + " 条，跳过 " +
                                      std::to_string(r.skipped) + " 条",
                                  6);
                    if (!st.repo.events.empty()) st.load_event(&*st.repo.events.begin());
                } catch (const std::exception& ex) {
                    st.set_status(std::string("导入失败：") + ex.what(), 6);
                }
            }
        }
        if (ImGui::MenuItem("导出…")) {
            auto path = save_dialog(
                st.hwnd,
                L"墨记打包 (*.mjnote)\0*.mjnote\0JSON (*.json)\0*.json\0todo.txt (*.txt)\0*.txt\0Markdown (*.md)\0*.md\0",
                L"mjnote", L"moji-export.mjnote");
            if (path) {
                try {
                    export_auto(st.repo, *path);
                    st.set_status("已导出到 " + path->u8string(), 6);
                } catch (const std::exception& ex) {
                    st.set_status(std::string("导出失败：") + ex.what(), 6);
                }
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("退出")) st.quit = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("编辑")) {
        if (ImGui::MenuItem("撤销（应用操作）", "Ctrl+Alt+Z")) st.undo(false);
        if (ImGui::MenuItem("重做", "Ctrl+Alt+Y")) st.undo(true);
        ImGui::Separator();
        if (ImGui::MenuItem("插入表格")) {
            std::string snippet = "\n| 项目 | 内容 |\n| --- | --- |\n|  |  |\n";
            size_t len = strlen(st.note);
            size_t add = std::min(snippet.size(), sizeof(st.note) - len - 1);
            memcpy(st.note + len, snippet.c_str(), add);
            st.note[len + add] = 0;
            st.dirty = true;
            st.last_edit = ImGui::GetTime();
        }
        if (ImGui::MenuItem("手绘笔迹…")) {
            st.strokes.clear();
            st.show_sketch = true;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("视图")) {
        if (ImGui::MenuItem("聚焦搜索", "Ctrl+F")) st.focus_search = true;
        if (ImGui::MenuItem("刷新", "F5")) {
            st.repo.reload();
            Event* e = st.current();
            if (!e) st.load_event(nullptr);
            st.set_status("已刷新", 2);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("浅色主题", nullptr, !st.dark)) {
            st.dark = false;
            st.pal = light_pal();
            apply_style(st.pal);
        }
        if (ImGui::MenuItem("深色主题", nullptr, st.dark)) {
            st.dark = true;
            st.pal = dark_pal();
            apply_style(st.pal);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("帮助")) {
        if (ImGui::MenuItem("快捷键", "F1")) st.show_help = true;
        if (ImGui::MenuItem("关于墨记")) st.show_about = true;
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}

void draw_ui(GuiState& st) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);

    draw_menus(st);
    draw_sidebar(st);
    ImGui::SameLine(0, 0);
    draw_center(st);
    ImGui::SameLine(0, 0);
    draw_editor(st);
    ImGui::End();

    draw_sketch(st);
    draw_modals(st);
}

// ---------------------------------------------------------------- 窗口
LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;
    switch (msg) {
        case WM_SIZE:
            if (g_app && wParam != SIZE_MINIMIZED) {
                g_app->resize_w = LOWORD(lParam);
                g_app->resize_h = HIWORD(lParam);
            }
            return 0;
        case WM_DROPFILES: {
            HDROP drop = (HDROP)wParam;
            UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < count; ++i) {
                UINT len = DragQueryFileW(drop, i, nullptr, 0);
                std::wstring path(len + 1, L'\0');
                DragQueryFileW(drop, i, &path[0], len + 1);
                path.resize(len);
                if (g_app) g_app->dropped.push_back(to_utf8(path));
            }
            DragFinish(drop);
            return 0;
        }
        case WM_CLOSE:
            if (g_app) {
                g_app->save_current();
                g_app->quit = true;
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int run_gui(Repository& repo, const std::string& screenshot_path) {
    if (!imaging_init()) {
        return 2;
    }
    ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpszClassName = L"MoJiGuiClass";
    RegisterClassExW(&wc);

    RECT rect{0, 0, 1280, 820};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    int win_w = rect.right - rect.left;
    int win_h = rect.bottom - rect.top;
    bool shot_mode = !screenshot_path.empty();
    int x = shot_mode ? -32000 : CW_USEDEFAULT;
    int y = shot_mode ? 0 : CW_USEDEFAULT;
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"墨记 MoJi · 电子记事本",
                                WS_OVERLAPPEDWINDOW, x, y, win_w, win_h, nullptr,
                                nullptr, wc.hInstance, nullptr);
    if (!hwnd) return 3;
    DragAcceptFiles(hwnd, TRUE);

    GuiState state(repo);
    g_app = &state;
    state.hwnd = hwnd;
    state.screenshot_mode = shot_mode;
    state.screenshot_path = screenshot_path;
    json cfg = repo.store.load_config();
    state.dark = cfg.value("theme", std::string("light")) == "dark";
    state.pal = state.dark ? dark_pal() : light_pal();

    if (!create_device(state, hwnd)) {
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 4;
    }
    ShowWindow(hwnd, shot_mode ? SW_SHOWNOACTIVATE : SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(state.dev, state.ctx);
    load_fonts(state);
    apply_style(state.pal);
    ImGui_ImplDX11_CreateDeviceObjects();

    if (!repo.events.empty()) {
        Event* first = &*repo.events.begin();
        state.load_event(first);
    }
    if (shot_mode) {
        state.last_reminder_check = ImGui::GetTime();
        state.frame_count = 0;
    }

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;
        if (state.quit) {
            DestroyWindow(hwnd);
            break;
        }
        if (state.resize_w && state.resize_h) {
            cleanup_rtv(state);
            state.swap->ResizeBuffers(0, state.resize_w, state.resize_h,
                                      DXGI_FORMAT_UNKNOWN, 0);
            ID3D11Texture2D* back = nullptr;
            if (SUCCEEDED(state.swap->GetBuffer(0, IID_PPV_ARGS(&back)))) {
                state.dev->CreateRenderTargetView(back, nullptr, &state.rtv);
                back->Release();
            }
            state.resize_w = state.resize_h = 0;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 快捷键
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N)) state.new_event();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) state.save_current();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) state.focus_search = true;
        if (io.KeyCtrl && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Z)) state.undo(false);
        if (io.KeyCtrl && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Y)) state.undo(true);
        if (ImGui::IsKeyPressed(ImGuiKey_F1)) state.show_help = true;
        if (ImGui::IsKeyPressed(ImGuiKey_F5)) state.repo.reload();

        if (!shot_mode) state.check_reminders();
        if (!state.dropped.empty()) {
            state.attach_files(state.dropped);
            state.dropped.clear();
        }
        if (state.dirty && ImGui::GetTime() - state.last_edit > 0.7) {
            state.save_current();
        }

        draw_ui(state);

        ImGui::Render();
        const float clear[4] = {0.05f, 0.06f, 0.09f, 1.0f};
        state.ctx->OMSetRenderTargets(1, &state.rtv, nullptr);
        state.ctx->ClearRenderTargetView(state.rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        if (shot_mode && ++state.frame_count >= 20) {
            if (!capture_backbuffer(state, fs::path(to_wide(screenshot_path)))) {
                std::fprintf(stderr, "screenshot failed\n");
            }
            done = true;
        }

        state.swap->Present(shot_mode ? 0 : 1, 0);
    }

    state.save_current();
    json out_cfg = repo.store.load_config();
    out_cfg["theme"] = state.dark ? "dark" : "light";
    repo.store.save_config(out_cfg);

    release_textures(state);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanup_device(state);
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    g_app = nullptr;
    imaging_shutdown();
    return 0;
}

}  // namespace moji
