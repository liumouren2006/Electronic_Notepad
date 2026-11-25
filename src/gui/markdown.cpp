#include "markdown.hpp"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

namespace moji {

namespace {

QString escape_html(QString text) {
    text.replace("&", "&amp;");
    text.replace("<", "&lt;");
    text.replace(">", "&gt;");
    text.replace("\"", "&quot;");
    return text;
}

QString inline_format(const QString& raw) {
    QString text = escape_html(raw);
    // 行内代码先占位，避免内部 ** 被处理
    text.replace(QRegularExpression("`([^`]+)`"), "<code>\\1</code>");
    text.replace(QRegularExpression("\\*\\*(.+?)\\*\\*"), "<b>\\1</b>");
    text.replace(QRegularExpression("__(.+?)__"), "<b>\\1</b>");
    text.replace(QRegularExpression("~~(.+?)~~"), "<s>\\1</s>");
    text.replace(QRegularExpression("\\+\\+(.+?)\\+\\+"), "<u>\\1</u>");
    text.replace(QRegularExpression("==(.+?)=="),
                 "<span style=\"background-color:#fff3b0;color:#1c2130;\">\\1</span>");
    text.replace(QRegularExpression("\\{color:(#[0-9a-fA-F]{3,8}|[a-zA-Z]+)\\}(.+?)\\{/color\\}"),
                 "<span style=\"color:\\1\">\\2</span>");
    text.replace(QRegularExpression("\\*(?!\\*)(.+?)(?<!\\*)\\*"), "<i>\\1</i>");
    text.replace(QRegularExpression("\\[([^\\]]+)\\]\\((https?://[^)\\s]+)\\)"),
                 "<a href=\"\\2\">\\1</a>");
    return text;
}

bool is_table_separator(const QString& line) {
    static const QRegularExpression re("^\\s*\\|?\\s*:?-{2,}:?\\s*(\\|\\s*:?-{2,}:?\\s*)*\\|?\\s*$");
    return re.match(line).hasMatch();
}

QStringList table_cells(const QString& line) {
    QString trimmed = line.trimmed();
    if (trimmed.startsWith('|')) trimmed.remove(0, 1);
    if (trimmed.endsWith('|')) trimmed.chop(1);
    QStringList cells;
    for (const QString& cell : trimmed.split('|')) {
        cells << cell.trimmed();
    }
    return cells;
}

}  // namespace

QString markdown_to_html(const QString& markdown, bool dark, int font_px) {
    QString text = markdown;
    text.replace("\r\n", "\n");
    text.replace('\r', '\n');
    const QStringList lines = text.split('\n');

    QString body;
    QString paragraph;
    auto flush_paragraph = [&]() {
        if (!paragraph.trimmed().isEmpty()) {
            body += "<p>" + inline_format(paragraph) + "</p>\n";
        }
        paragraph.clear();
    };

    bool in_code = false;
    QString code;
    bool in_ul = false, in_ol = false, in_quote = false;
    auto close_lists = [&]() {
        if (in_ul) { body += "</ul>\n"; in_ul = false; }
        if (in_ol) { body += "</ol>\n"; in_ol = false; }
    };
    auto close_quote = [&]() {
        if (in_quote) { body += "</blockquote>\n"; in_quote = false; }
    };

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        QString trimmed = line.trimmed();

        if (in_code) {
            if (trimmed.startsWith("```")) {
                body += "<pre><code>" + escape_html(code) + "</code></pre>\n";
                code.clear();
                in_code = false;
            } else {
                code += line + "\n";
            }
            continue;
        }
        if (trimmed.startsWith("```")) {
            flush_paragraph();
            close_lists();
            close_quote();
            in_code = true;
            continue;
        }
        if (trimmed.isEmpty()) {
            flush_paragraph();
            close_lists();
            close_quote();
            continue;
        }

        // 表格：当前行含 | 且下一行是分隔行
        if (line.contains('|') && i + 1 < lines.size() &&
            is_table_separator(lines[i + 1])) {
            flush_paragraph();
            close_lists();
            close_quote();
            QStringList headers = table_cells(line);
            body += "<table border=\"1\" cellspacing=\"0\" cellpadding=\"6\" "
                    "style=\"border-collapse:collapse;\">\n<thead><tr>";
            for (const QString& h : headers) {
                body += "<th>" + inline_format(h) + "</th>";
            }
            body += "</tr></thead>\n<tbody>\n";
            i += 1;
            while (i + 1 < lines.size() && lines[i + 1].contains('|') &&
                   !lines[i + 1].trimmed().isEmpty()) {
                ++i;
                QStringList cells = table_cells(lines[i]);
                body += "<tr>";
                for (int c = 0; c < headers.size(); ++c) {
                    body += "<td>" + inline_format(c < cells.size() ? cells[c] : "") + "</td>";
                }
                body += "</tr>\n";
            }
            body += "</tbody></table>\n";
            continue;
        }

        // 标题
        static const QRegularExpression heading("^(#{1,6})\\s+(.*)$");
        auto hm = heading.match(trimmed);
        if (hm.hasMatch()) {
            flush_paragraph();
            close_lists();
            close_quote();
            int level = hm.captured(1).size();
            body += QString("<h%1>%2</h%1>\n").arg(level).arg(inline_format(hm.captured(2)));
            continue;
        }

        // 分隔线
        if (trimmed == "---" || trimmed == "***" || trimmed == "___") {
            flush_paragraph();
            close_lists();
            close_quote();
            body += "<hr/>\n";
            continue;
        }

        // 引用
        if (trimmed.startsWith(">")) {
            flush_paragraph();
            close_lists();
            if (!in_quote) {
                body += "<blockquote>\n";
                in_quote = true;
            }
            QString q = trimmed.mid(1).trimmed();
            body += "<p>" + inline_format(q) + "</p>\n";
            continue;
        }
        close_quote();

        // 无序列表 / 任务列表
        static const QRegularExpression ul("^[-*+]\\s+(.*)$");
        auto um = ul.match(trimmed);
        if (um.hasMatch()) {
            flush_paragraph();
            if (in_ol) { body += "</ol>\n"; in_ol = false; }
            if (!in_ul) { body += "<ul>\n"; in_ul = true; }
            QString item = um.captured(1);
            if (item.startsWith("[x] ") || item.startsWith("[X] ")) {
                body += "<li>☑ " + inline_format(item.mid(4)) + "</li>\n";
            } else if (item.startsWith("[ ] ")) {
                body += "<li>☐ " + inline_format(item.mid(4)) + "</li>\n";
            } else {
                body += "<li>" + inline_format(item) + "</li>\n";
            }
            continue;
        }

        // 有序列表
        static const QRegularExpression ol("^\\d+[.)]\\s+(.*)$");
        auto om = ol.match(trimmed);
        if (om.hasMatch()) {
            flush_paragraph();
            if (in_ul) { body += "</ul>\n"; in_ul = false; }
            if (!in_ol) { body += "<ol>\n"; in_ol = true; }
            body += "<li>" + inline_format(om.captured(1)) + "</li>\n";
            continue;
        }
        close_lists();

        // 普通段落
        if (!paragraph.isEmpty()) paragraph += "<br>";
        paragraph += trimmed;
    }
    if (in_code) {
        body += "<pre><code>" + escape_html(code) + "</code></pre>\n";
    }
    flush_paragraph();
    close_lists();
    close_quote();

    const QString bg = dark ? "#161a23" : "#ffffff";
    const QString fg = dark ? "#e8eaf2" : "#1c2130";
    const QString muted = dark ? "#939cb3" : "#68708a";
    const QString border = dark ? "#2a3142" : "#dfe4f0";
    const QString thbg = dark ? "#1d2230" : "#eef1f8";
    const QString codebg = dark ? "#11151d" : "#f4f6fb";

    return QString(
               "<html><head><style>"
               "body{background:%1;color:%2;font-family:'Microsoft YaHei UI',sans-serif;"
               "font-size:%8px;line-height:1.6;}"
               "h1{color:%2;font-size:1.65em;margin:16px 0 10px;}"
               "h2{color:%2;font-size:1.38em;margin:14px 0 8px;}"
               "h3{color:%2;font-size:1.18em;margin:12px 0 6px;}"
               "h4,h5,h6{color:%2;font-size:1.05em;margin:10px 0 6px;}"
               "del{color:%4;}"
               "img{border-radius:8px;margin:6px 0;}"
               "mark{border-radius:3px;padding:0 2px;}"
               "p{margin:6px 0;}"
               "a{color:#4f6ef7;}"
               "code{background:%5;color:%2;font-family:Consolas,monospace;padding:1px 4px;}"
               "pre{background:%5;padding:8px;border-radius:6px;}"
               "blockquote{border-left:3px solid %3;margin:8px 0;padding:2px 12px;color:%4;}"
               "table{border-collapse:collapse;margin:8px 0;}"
               "th{background:%6;color:%2;}"
               "th,td{border:1px solid %3;padding:6px 9px;}"
               "ul,ol{margin:6px 0 6px 22px;}"
               "hr{border:none;border-top:1px solid %3;}"
               "</style></head><body>%7</body></html>")
        .arg(bg, fg, border, muted, codebg, thbg, body)
        .arg(std::max(9, font_px));
}

}  // namespace moji
