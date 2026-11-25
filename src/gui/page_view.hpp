#pragma once

#include <QWidget>
#include <functional>

#include <nlohmann/json.hpp>

namespace moji {

class Repository;
struct Event;

// OneNote 风格页面：Markdown 文字层 + 可移动/缩放/旋转/调层的图片 + 墨迹层
class PageEditor : public QWidget {
public:
    explicit PageEditor(Repository& repo, QWidget* parent = nullptr);
    ~PageEditor() override;

    void set_event(Event* event);
    void set_note(const QString& markdown);
    void set_dark(bool dark);
    void set_read_only(bool read_only);
    void set_text_font(int px);
    void set_ink_mode(bool enabled);
    bool ink_mode() const;
    void refresh_images(Event* event);

    std::function<void(const QString&)> on_layout_changed;
    std::function<void()> on_add_image;
    std::function<void()> on_remove_image;

private:
    class Impl;
    Impl* impl_;
};

}  // namespace moji
