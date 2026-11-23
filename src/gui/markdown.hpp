#pragma once

#include <QString>

namespace moji {

// 轻量 Markdown -> HTML：标题、列表、表格、引用、粗体/斜体/删除线/下划线/颜色、链接、代码
QString markdown_to_html(const QString& markdown, bool dark = false);

}  // namespace moji
