// 墨记 MoJi · Qt Widgets 图形界面
#include "qt_gui.hpp"

#include <QAction>
#include <QActionGroup>
#include <QAbstractItemView>
#include <QAbstractAnimation>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QDragMoveEvent>
#include <QHash>
#include <QLayoutItem>
#include <QList>
#include <QSize>
#include <QVariant>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDesktopServices>
#include <QDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFile>
#include <QColorDialog>
#include <QTabWidget>
#include <QTextBrowser>
#include <QSet>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QKeySequence>
#include <QPaintEvent>
#include <QStyle>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QSystemTrayIcon>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "core/importer.hpp"
#include "core/model.hpp"
#include "core/repository.hpp"
#include "core/util.hpp"
#include "core/sound.hpp"
#include "gui/markdown.hpp"
#include "gui/page_view.hpp"

namespace moji {

namespace {

using namespace std::placeholders;

// ---------------------------------------------------------------- 基础工具
QString qs(const std::string& s) { return QString::fromStdString(s); }
QString qpath(const fs::path& p) {
#ifdef _WIN32
    return QString::fromStdWString(p.wstring());
#else
    return QString::fromStdString(p.string());
#endif
}
fs::path fspath(const QString& s) {
#ifdef _WIN32
    return fs::path(s.toStdWString());
#else
    return fs::path(s.toStdString());
#endif
}
QString clock_text(std::time_t t) { return qs(clock_str(t)); }
// ---------------------------------------------------------------- 配色与 QSS
struct Pal {
    QString bg, surface, surfaceAlt, surfaceHover;
    QString text, muted, primary, primaryHover, primarySoft;
    QString border, input, success, successSoft;
    QString warning, warningSoft, error, errorSoft;
    QString violet, teal, toastBg, toastText, toastMuted;
};

Pal make_light(const QColor& accent = QColor("#4f6ef7")) {
    Pal p;
    p.bg = "#f4f6fb";
    p.surface = "#ffffff";
    p.surfaceAlt = "#eef1f8";
    p.surfaceHover = "#e6ebf7";
    p.text = "#1c2130";
    p.muted = "#68708a";
    p.primary = "#4f6ef7";
    p.primaryHover = "#3f5ce0";
    p.primarySoft = "#e7ecff";
    p.border = "#dfe4f0";
    p.input = "#f8f9fd";
    p.success = "#16a34a";
    p.successSoft = "#e7f7ee";
    p.warning = "#d97706";
    p.warningSoft = "#fef3e2";
    p.error = "#dc2626";
    p.errorSoft = "#fdecec";
    p.violet = "#7c5cff";
    p.teal = "#0e9488";
    p.toastBg = "#1c2130";
    p.toastText = "#f5f7ff";
    p.toastMuted = "#b7bfd4";
    p.primary = accent.name();
    p.primaryHover = accent.darker(112).name();
    p.primarySoft = QColor::fromRgbF(
        accent.redF() * 0.10 + 0.90, accent.greenF() * 0.10 + 0.90,
        accent.blueF() * 0.10 + 0.90).name();
    return p;
}

Pal make_dark(const QColor& accent = QColor("#4f6ef7")) {
    Pal p;
    p.bg = "#0e1117";
    p.surface = "#161a23";
    p.surfaceAlt = "#1d2230";
    p.surfaceHover = "#242a3a";
    p.text = "#e8eaf2";
    p.muted = "#939cb3";
    p.primary = "#7c9cff";
    p.primaryHover = "#94afff";
    p.primarySoft = "#232f55";
    p.border = "#2a3142";
    p.input = "#11151d";
    p.success = "#4ade80";
    p.successSoft = "#16321f";
    p.warning = "#fbbf24";
    p.warningSoft = "#3a2c12";
    p.error = "#f87171";
    p.errorSoft = "#3a1c1f";
    p.violet = "#a78bfa";
    p.teal = "#2dd4bf";
    p.toastBg = "#242a3a";
    p.toastText = "#f5f7ff";
    p.toastMuted = "#aab3c8";
    p.primary = accent.lighter(125).name();
    p.primaryHover = accent.lighter(145).name();
    p.primarySoft = QColor::fromRgbF(
        accent.redF() * 0.22 + 0.09, accent.greenF() * 0.22 + 0.10,
        accent.blueF() * 0.22 + 0.14).name();
    return p;
}

QString build_qss(const Pal& p) {
    QString qss = QStringLiteral(R"QSS(
* {
    font-family: "Microsoft YaHei UI", "Microsoft YaHei", "Segoe UI", sans-serif;
    color: @{text};
    outline: none;
}
QMainWindow, QWidget#Root { background: @{bg}; }
QToolTip { background: @{toastBg}; color: @{toastText}; border: none; padding: 6px 10px; border-radius: 6px; }

QFrame#Sidebar { background: @{surface}; border-right: 1px solid @{border}; }
QFrame#CenterPanel { background: @{bg}; }
QFrame#EditorPanel { background: @{surface}; border-left: 1px solid @{border}; }

QLabel[role="muted"] { color: @{muted}; }
QLabel[role="h1"] { font-size: 21px; font-weight: 700; }
QLabel[role="h2"] { font-size: 15px; font-weight: 700; }
QLabel[role="h3"] { font-size: 12px; font-weight: 700; color: @{muted}; }
QLabel[role="statValue"] { font-size: 19px; font-weight: 700; }
QLabel[role="statCaption"] { font-size: 10px; color: @{muted}; }

QFrame#StatCard { background: @{surfaceAlt}; border-radius: 12px; }
QFrame#BrandMark { background: @{primary}; border-radius: 16px; }
QLabel#BrandMarkText { color: #ffffff; font-size: 28px; font-weight: 700; }

QPushButton {
    background: @{surfaceAlt};
    border: 1px solid @{border};
    border-radius: 10px;
    padding: 7px 14px;
    font-size: 10.5pt;
}
QPushButton:hover { background: @{surfaceHover}; }
QPushButton:pressed { background: @{border}; }
QPushButton:disabled { color: @{muted}; background: @{surfaceAlt}; }
QPushButton#Primary { background: @{primary}; color: #ffffff; border: none; font-weight: 600; padding: 8px 18px; }
QPushButton#Primary:hover { background: @{primaryHover}; }
QPushButton#Danger { color: @{error}; background: transparent; border: 1px solid @{error}; }
QPushButton#Danger:hover { background: @{errorSoft}; }
QPushButton#Ghost { background: transparent; border: none; color: @{muted}; padding: 6px 10px; }
QPushButton#Ghost:hover { background: @{surfaceHover}; color: @{text}; }
QPushButton#NavButton {
    text-align: left;
    background: transparent;
    border: none;
    color: @{muted};
    padding: 7px 12px;
    border-radius: 10px;
    font-size: 10.5pt;
}
QPushButton#NavButton:hover { background: @{surfaceHover}; color: @{text}; }
QPushButton#NavButton:checked { background: @{primarySoft}; color: @{primary}; font-weight: 600; }
QPushButton#TagChip {
    background: @{surfaceAlt};
    color: @{muted};
    border: 1px solid transparent;
    border-radius: 9px;
    padding: 3px 10px;
    font-size: 9.5pt;
    text-align: left;
}
QPushButton#TagChip:hover { color: @{text}; background: @{surfaceHover}; }
QPushButton#TagChip:checked { background: @{primarySoft}; color: @{primary}; border: 1px solid @{primary}; }
QPushButton#IconButton { background: transparent; border: none; border-radius: 9px; padding: 6px 9px; font-size: 12pt; }
QPushButton#IconButton:hover { background: @{surfaceHover}; }
QPushButton#ToolButton {
    background: @{surfaceAlt};
    color: @{text};
    border: 1px solid @{border};
    border-radius: 9px;
    padding: 5px 10px;
    font-size: 9.5pt;
    font-weight: 500;
}
QPushButton#ToolButton:hover { background: @{surfaceHover}; border-color: @{primary}; }
QPushButton#ToolButton:pressed { background: @{primarySoft}; }
QPushButton#ToolButton:checked { background: @{primary}; color: #ffffff; border-color: @{primary}; font-weight: 700; }

QLineEdit, QDateTimeEdit, QSpinBox {
    background: @{input};
    border: 1px solid @{border};
    border-radius: 10px;
    padding: 7px 10px;
    selection-background-color: @{primary};
    selection-color: #ffffff;
}
QLineEdit:focus, QDateTimeEdit:focus, QSpinBox:focus { border: 1px solid @{primary}; }
QLineEdit#TitleEdit { background: transparent; border: none; border-radius: 0; padding: 2px 0; font-size: 20px; font-weight: 700; }
QLineEdit#SearchEdit { padding: 9px 14px; border-radius: 12px; font-size: 10.5pt; }
QDateTimeEdit::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 22px; border: none; background: transparent; }
QDateTimeEdit::down-arrow { image: url(:/assets/arrow_down.png); width: 10px; height: 10px; }
QSpinBox::up-button, QSpinBox::down-button {
    subcontrol-origin: border;
    width: 18px;
    border: none;
    border-left: 1px solid @{border};
    background: transparent;
}
QSpinBox::up-button { subcontrol-position: top right; border-top-right-radius: 9px; }
QSpinBox::down-button { subcontrol-position: bottom right; border-bottom-right-radius: 9px; }
QSpinBox::up-arrow { image: url(:/assets/arrow_up.png); width: 9px; height: 9px; }
QSpinBox::down-arrow { image: url(:/assets/arrow_down.png); width: 9px; height: 9px; }

QTextEdit#NoteEdit {
    background: @{input};
    border: 1px solid @{border};
    border-radius: 12px;
    padding: 10px;
    font-size: 10.5pt;
    selection-background-color: @{primary};
    selection-color: #ffffff;
}
QTextEdit#NoteEdit:focus { border: 1px solid @{primary}; }
QTabWidget#NoteTabs::pane { border: 1px solid @{border}; border-radius: 10px; background: @{input}; }
QTabBar::tab { background: @{surfaceAlt}; color: @{muted}; padding: 6px 14px; margin-right: 2px; border-top-left-radius: 8px; border-top-right-radius: 8px; }
QTabBar::tab:selected { background: @{primarySoft}; color: @{primary}; font-weight: 600; }
QTextBrowser#Preview { background: @{input}; border: 1px solid @{border}; border-radius: 10px; padding: 6px; }
QLabel#ViewTitle { font-size: 22px; font-weight: 700; color: @{text}; }
QLabel#ViewMeta { color: @{muted}; font-size: 10pt; }
QLabel#ViewSection { color: @{muted}; font-size: 9pt; font-weight: 700; }

QFrame#Card { background: @{surface}; border: 1px solid @{border}; border-radius: 14px; }
QFrame#Card:hover { background: @{surfaceHover}; border: 1px solid @{primary}; }
QFrame#Card[selected="true"] { background: @{primarySoft}; border: 2px solid @{primary}; }

QCheckBox { spacing: 8px; }
QCheckBox::indicator {
    width: 18px; height: 18px;
    border-radius: 5px;
    border: 2px solid @{border};
    background: @{input};
}
QCheckBox::indicator:hover { border: 2px solid @{primary}; }
QCheckBox::indicator:checked {
    background: @{success};
    border: 2px solid @{success};
    image: url(:/assets/check.png);
}
QFrame#Card[selected="true"] QCheckBox::indicator { background: transparent; border: 2px solid @{primary}; }
QFrame#Card[selected="true"] QCheckBox::indicator:checked { background: @{success}; border: 2px solid @{success}; }

QScrollArea { border: none; background: transparent; }
QScrollArea > QWidget > QWidget { background: transparent; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: @{border}; border-radius: 4px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: @{muted}; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { height: 0; }

QListWidget#Attachments {
    background: @{input};
    border: 1px dashed @{border};
    border-radius: 12px;
    padding: 6px;
    outline: none;
}
QListWidget#Attachments::item { border-radius: 10px; padding: 4px; color: @{muted}; }
QListWidget#Attachments::item:hover { background: @{surfaceHover}; color: @{text}; }
QListWidget#Attachments::item:selected { background: @{primarySoft}; color: @{primary}; }

QMenuBar { background: @{surface}; color: @{text}; padding: 2px 6px; }
QMenuBar::item { padding: 6px 10px; border-radius: 6px; background: transparent; }
QMenuBar::item:selected { background: @{surfaceHover}; }
QMenu { background: @{surface}; border: 1px solid @{border}; border-radius: 10px; padding: 6px; }
QMenu::item { padding: 7px 26px; border-radius: 7px; }
QMenu::item:selected { background: @{primarySoft}; color: @{primary}; }
QMenu::separator { height: 1px; background: @{border}; margin: 6px 10px; }

QProgressBar { background: @{surfaceAlt}; border: none; border-radius: 4px; height: 8px; text-align: center; color: transparent; }
QProgressBar::chunk { background: @{success}; border-radius: 4px; }
QSplitter::handle { background: transparent; width: 2px; }

QFrame#Toast { background: @{toastBg}; border-radius: 14px; }
QLabel#ToastTitle { color: @{toastText}; font-weight: 700; font-size: 11pt; }
QLabel#ToastBody { color: @{toastMuted}; font-size: 10pt; }
QLabel#ToastIcon { color: @{toastText}; font-size: 16pt; }

QDialog { background: @{surface}; }
QFrame#EditorEmpty { background: @{surface}; }
QLabel#EmptyTitle { font-size: 18px; font-weight: 700; color: @{text}; }
QLabel#EmptyHint { font-size: 10.5pt; color: @{muted}; }
QStatusBar { background: @{surface}; color: @{muted}; border-top: 1px solid @{border}; }
QStatusBar::item { border: none; }
)QSS");
    qss.replace("@{bg}", p.bg)
        .replace("@{surfaceAlt}", p.surfaceAlt)
        .replace("@{surfaceHover}", p.surfaceHover)
        .replace("@{surface}", p.surface)
        .replace("@{text}", p.text)
        .replace("@{muted}", p.muted)
        .replace("@{primarySoft}", p.primarySoft)
        .replace("@{primaryHover}", p.primaryHover)
        .replace("@{primary}", p.primary)
        .replace("@{border}", p.border)
        .replace("@{input}", p.input)
        .replace("@{successSoft}", p.successSoft)
        .replace("@{success}", p.success)
        .replace("@{warningSoft}", p.warningSoft)
        .replace("@{warning}", p.warning)
        .replace("@{errorSoft}", p.errorSoft)
        .replace("@{error}", p.error)
        .replace("@{violet}", p.violet)
        .replace("@{teal}", p.teal)
        .replace("@{toastBg}", p.toastBg)
        .replace("@{toastText}", p.toastText)
        .replace("@{toastMuted}", p.toastMuted);
    return qss;
}

QString chip_style(const QString& color, const QString& background) {
    return QString("background:%1;color:%2;border-radius:7px;padding:2px 8px;font-size:9pt;")
        .arg(background, color);
}

const QColor kPriorityFg[] = {QColor("#ef4444"), QColor("#f97316"), QColor("#eab308"),
                              QColor("#22c55e"), QColor("#06b6d4"), QColor("#6366f1")};
const QColor kPriorityBg[] = {QColor("#fee2e2"), QColor("#ffedd5"), QColor("#fef9c3"),
                              QColor("#dcfce7"), QColor("#cffafe"), QColor("#e0e7ff")};

QColor priority_fg(int p) { return kPriorityFg[(std::max(1, p) - 1) % 6]; }
QColor priority_bg(int p) { return kPriorityBg[(std::max(1, p) - 1) % 6]; }

// ---------------------------------------------------------------- 标题省略
class ElidedLabel : public QLabel {
public:
    explicit ElidedLabel(const QString& text, QWidget* parent = nullptr)
        : QLabel(parent), full_(text) {
        setToolTip(text);
        setMinimumWidth(40);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        apply();
    }
    void setText(const QString& text) {
        full_ = text;
        setToolTip(text);
        apply();
    }

protected:
    void resizeEvent(QResizeEvent* e) override {
        QLabel::resizeEvent(e);
        apply();
    }

private:
    void apply() {
        QLabel::setText(fontMetrics().elidedText(full_, Qt::ElideRight, std::max(10, width() - 2)));
    }
    QString full_;
};

// ---------------------------------------------------------------- 事件卡片
class EventCard : public QFrame {
public:
    EventCard(const Event& event, const Pal& pal,
              std::function<void(Qt::KeyboardModifiers)> on_click,
              std::function<void()> on_toggle, std::function<void()> on_double,
              QWidget* parent = nullptr)
        : QFrame(parent),
          event_(event),
          pal_(pal),
          on_click_(std::move(on_click)),
          on_toggle_(std::move(on_toggle)),
          on_double_(std::move(on_double)) {
        setObjectName("Card");
        setProperty("selected", false);
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(88);
        build();
    }

    void set_selected(bool selected) {
        if (property("selected").toBool() == selected) return;
        setProperty("selected", selected);
        style()->unpolish(this);
        style()->polish(this);
        update();
    }

protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && on_click_) on_click_(e->modifiers());
        QFrame::mousePressEvent(e);
    }
    void mouseDoubleClickEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && on_double_) on_double_();
        QFrame::mouseDoubleClickEvent(e);
    }

private:
    void build() {
        const Event& e = event_;
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(12, 10, 14, 10);
        layout->setSpacing(11);

        QColor accent = e.done ? QColor(pal_.muted)
                               : (e.is_overdue() ? QColor(pal_.error)
                                                 : (e.color.empty() ? priority_fg(e.priority)
                                                                    : QColor(qs(e.color))));
        if (!accent.isValid()) accent = priority_fg(e.priority);
        auto* bar = new QLabel(this);
        bar->setFixedSize(4, 48);
        bar->setStyleSheet(QString("background:%1;border-radius:2px;").arg(accent.name()));
        bar->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(bar, 0, Qt::AlignTop);

        auto* check = new QCheckBox(this);
        check->setChecked(e.done);
        check->setToolTip("标记完成 / 取消完成");
        check->setCursor(Qt::PointingHandCursor);
        QObject::connect(check, &QCheckBox::toggled, this, [this](bool) {
            if (on_toggle_) on_toggle_();
        });
        layout->addWidget(check, 0, Qt::AlignTop);

        auto* body = new QVBoxLayout();
        body->setSpacing(5);
        body->setContentsMargins(0, 0, 0, 0);
        QString title_text = (e.icon.empty() ? QString() : qs(e.icon) + " ") + qs(e.title);
        auto* title = new ElidedLabel(title_text, this);
        QFont tf = title->font();
        tf.setPointSizeF(10.8);
        tf.setBold(true);
        tf.setStrikeOut(e.done);
        title->setFont(tf);
        title->setStyleSheet(QString("color:%1;").arg(e.done ? pal_.muted : pal_.text));
        body->addWidget(title);

        auto* meta = new QHBoxLayout();
        meta->setSpacing(6);
        meta->setContentsMargins(0, 0, 0, 0);
        auto chip = [this, meta](const QString& text, const QString& fg, const QString& bg) {
            auto* label = new QLabel(text, this);
            label->setStyleSheet(chip_style(fg, bg));
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
            meta->addWidget(label);
        };
        if (e.done) {
            chip("已完成", pal_.muted, pal_.surfaceAlt);
        } else if (e.due.has_value()) {
            if (e.is_overdue()) {
                chip("⏱ " + qs(humanize_due(e.due)), pal_.error, pal_.errorSoft);
            } else if (e.is_due_today()) {
                chip("⏱ " + qs(humanize_due(e.due)), pal_.warning, pal_.warningSoft);
            } else {
                chip("⏱ " + qs(humanize_due(e.due)), pal_.muted, pal_.surfaceAlt);
            }
        } else {
            chip("无期限", pal_.muted, pal_.surfaceAlt);
        }
        chip("P" + QString::number(e.priority), priority_fg(e.priority).name(),
             priority_bg(e.priority).name());
        int tag_count = 0;
        for (const auto& tag : e.tags) {
            if (tag_count++ >= 2) break;
            auto* label = new QLabel("#" + qs(tag), this);
            label->setStyleSheet(QString("color:%1;font-size:9pt;padding:0 2px;").arg(pal_.violet));
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
            meta->addWidget(label);
        }
        meta->addStretch(1);
        if (!e.attachments.empty()) {
            auto* att = new QLabel("附件 " + QString::number(e.attachments.size()), this);
            att->setStyleSheet(QString("color:%1;font-size:9pt;").arg(pal_.teal));
            att->setAttribute(Qt::WA_TransparentForMouseEvents);
            meta->addWidget(att);
        }
        body->addLayout(meta);
        layout->addLayout(body, 1);
    }

    Event event_;
    Pal pal_;
    std::function<void(Qt::KeyboardModifiers)> on_click_;
    std::function<void()> on_toggle_;
    std::function<void()> on_double_;
};

// ---------------------------------------------------------------- 统计卡
class StatCard : public QFrame {
public:
    StatCard(const QString& caption, QWidget* parent = nullptr) : QFrame(parent) {
        setObjectName("StatCard");
        setMinimumHeight(58);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 7, 10, 7);
        layout->setSpacing(0);
        value_ = new QLabel("0", this);
        value_->setProperty("role", "statValue");
        auto* cap = new QLabel(caption, this);
        cap->setProperty("role", "statCaption");
        layout->addWidget(value_);
        layout->addWidget(cap);
    }
    void set_value(const QString& value, const QString& color = {}) {
        value_->setText(value);
        if (!color.isEmpty()) value_->setStyleSheet(QString("color:%1;").arg(color));
    }

private:
    QLabel* value_ = nullptr;
};

// ---------------------------------------------------------------- Toast
class Toast : public QFrame {
public:
    explicit Toast(QWidget* parent) : QFrame(parent) {
        setObjectName("Toast");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        hide();
        effect_ = new QGraphicsOpacityEffect(this);
        effect_->setOpacity(0.0);
        setGraphicsEffect(effect_);
        timer_ = new QTimer(this);
        timer_->setSingleShot(true);
        connect(timer_, &QTimer::timeout, this, [this] { fade(1.0, 0.0, 220); QTimer::singleShot(240, this, &QWidget::hide); });

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(16, 12, 18, 12);
        layout->setSpacing(10);
        icon_ = new QLabel("⏰", this);
        icon_->setObjectName("ToastIcon");
        layout->addWidget(icon_, 0, Qt::AlignTop);
        auto* body = new QVBoxLayout();
        body->setSpacing(2);
        title_ = new QLabel(this);
        title_->setObjectName("ToastTitle");
        text_ = new QLabel(this);
        text_->setObjectName("ToastBody");
        text_->setWordWrap(true);
        body->addWidget(title_);
        body->addWidget(text_);
        layout->addLayout(body, 1);
    }

    void show_message(const QString& title, const QString& body, const QString& icon = "⏰",
                      int ms = 6000) {
        title_->setText(title);
        text_->setText(body);
        text_->setVisible(!body.isEmpty());
        icon_->setText(icon);
        setFixedWidth(std::min(380, std::max(240, parentWidget() ? parentWidget()->width() / 3 : 300)));
        adjustSize();
        place();
        effect_->setOpacity(0.0);
        show();
        raise();
        fade(0.0, 1.0, 180);
        timer_->start(ms);
    }

private:
    void place() {
        if (!parentWidget()) return;
        move(std::max(16, parentWidget()->width() - width() - 26),
             std::max(16, parentWidget()->height() - height() - 26));
    }
    void fade(double from, double to, int ms) {
        auto* anim = new QPropertyAnimation(effect_, "opacity", this);
        anim->setDuration(ms);
        anim->setStartValue(from);
        anim->setEndValue(to);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
    QLabel* icon_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* text_ = nullptr;
    QGraphicsOpacityEffect* effect_ = nullptr;
    QTimer* timer_ = nullptr;
};

// ---------------------------------------------------------------- 附件列表
class AttachmentList : public QListWidget {
public:
    explicit AttachmentList(QWidget* parent = nullptr) : QListWidget(parent) {
        setObjectName("Attachments");
        setViewMode(QListWidget::IconMode);
        setIconSize(QSize(76, 56));
        setGridSize(QSize(104, 104));
        setResizeMode(QListWidget::Adjust);
        setMovement(QListWidget::Snap);
        setSpacing(4);
        setWordWrap(true);
        setTextElideMode(Qt::ElideRight);
        setDragDropMode(QListWidget::InternalMove);
        setDefaultDropAction(Qt::MoveAction);
        setSelectionMode(QListWidget::SingleSelection);
        setMinimumHeight(118);
        setMaximumHeight(150);
    }

    std::function<void(const QStringList&)> files_dropped;
    std::function<void(const QStringList&)> reordered;
    std::function<void(int)> remove_requested;
    std::function<void(int)> open_requested;

    QStringList paths() const {
        QStringList out;
        for (int i = 0; i < count(); ++i) {
            out << item(i)->data(Qt::UserRole).toString();
        }
        return out;
    }

protected:
    void dragEnterEvent(QDragEnterEvent* e) override {
        if (e->mimeData()->hasUrls()) {
            e->acceptProposedAction();
        } else {
            QListWidget::dragEnterEvent(e);
        }
    }
    void dragMoveEvent(QDragMoveEvent* e) override {
        if (e->mimeData()->hasUrls()) {
            e->acceptProposedAction();
        } else {
            QListWidget::dragMoveEvent(e);
        }
    }
    void dropEvent(QDropEvent* e) override {
        if (e->mimeData()->hasUrls()) {
            QStringList files;
            for (const QUrl& url : e->mimeData()->urls()) {
                if (url.isLocalFile()) files << url.toLocalFile();
            }
            if (!files.isEmpty()) {
                e->acceptProposedAction();
                if (files_dropped) files_dropped(files);
                return;
            }
        }
        QListWidget::dropEvent(e);
        if (reordered) reordered(paths());
    }
    void contextMenuEvent(QContextMenuEvent* e) override {
        auto* item = itemAt(e->pos());
        if (!item) return;
        QMenu menu(this);
        QAction* open = menu.addAction("打开");
        QAction* remove = menu.addAction("从记事中移除");
        QAction* chosen = menu.exec(e->globalPos());
        if (chosen == open) {
            if (open_requested) open_requested(row(item));
        } else if (chosen == remove && remove_requested) {
            remove_requested(row(item));
        }
    }
};

// ---------------------------------------------------------------- 备注编辑
class NoteEdit : public QTextEdit {
public:
    explicit NoteEdit(QWidget* parent = nullptr) : QTextEdit(parent) {
        setObjectName("NoteEdit");
        setAcceptRichText(false);
        setTabChangesFocus(true);
        setPlaceholderText("写点什么…");
    }

    std::function<void(const QImage&)> image_pasted;
    std::function<void(const QStringList&)> files_dropped;

protected:
    bool canInsertFromMimeData(const QMimeData* source) const override {
        return source->hasImage() || source->hasUrls() || QTextEdit::canInsertFromMimeData(source);
    }
    void insertFromMimeData(const QMimeData* source) override {
        if (source->hasImage()) {
            QImage img = qvariant_cast<QImage>(source->imageData());
            if (!img.isNull()) {
                if (image_pasted) image_pasted(img);
                return;
            }
        }
        if (source->hasUrls()) {
            QStringList files;
            for (const QUrl& url : source->urls()) {
                if (url.isLocalFile()) files << url.toLocalFile();
            }
            if (!files.isEmpty()) {
                if (files_dropped) files_dropped(files);
                return;
            }
        }
        QTextEdit::insertFromMimeData(source);
    }
    void dragEnterEvent(QDragEnterEvent* e) override {
        if (e->mimeData()->hasUrls() || e->mimeData()->hasImage()) {
            e->acceptProposedAction();
        } else {
            QTextEdit::dragEnterEvent(e);
        }
    }
    void dropEvent(QDropEvent* e) override {
        if (e->mimeData()->hasUrls()) {
            QStringList files;
            for (const QUrl& url : e->mimeData()->urls()) {
                if (url.isLocalFile()) files << url.toLocalFile();
            }
            if (!files.isEmpty()) {
                if (files_dropped) files_dropped(files);
                e->acceptProposedAction();
                return;
            }
        }
        QTextEdit::dropEvent(e);
    }
};

// ---------------------------------------------------------------- 手绘
class SketchCanvas : public QWidget {
public:
    explicit SketchCanvas(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(760, 400);
        image_ = QImage(size(), QImage::Format_ARGB32);
        image_.fill(Qt::white);
        setAttribute(Qt::WA_StaticContents);
        color_ = QColor("#1f2430");
        width_ = 4;
    }
    QImage image() const { return image_; }
    void set_color(const QColor& c) { color_ = c; }
    void set_width(int w) { width_ = w; }
    void undo() {
        if (!undo_.isEmpty()) {
            image_ = undo_.takeLast();
            update();
        }
    }
    void clear_canvas() {
        undo_.append(image_);
        if (undo_.size() > 20) undo_.removeFirst();
        image_.fill(Qt::white);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.drawImage(0, 0, image_);
    }
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() != Qt::LeftButton) return;
        undo_.append(image_);
        if (undo_.size() > 20) undo_.removeFirst();
        last_ = e->position().toPoint();
        QPainter p(&image_);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color_, width_, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPoint(last_);
        update();
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (!(e->buttons() & Qt::LeftButton)) return;
        QPoint pt = e->position().toPoint();
        QPainter p(&image_);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color_, width_, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(last_, pt);
        last_ = pt;
        update();
    }

private:
    QImage image_;
    QList<QImage> undo_;
    QPoint last_;
    QColor color_;
    int width_ = 4;
};

class SketchDialog : public QDialog {
public:
    explicit SketchDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("手绘笔迹");
        setModal(true);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(10);
        auto* toolbar = new QHBoxLayout();
        toolbar->addWidget(new QLabel("颜色", this));
        const QColor colors[] = {QColor("#1f2430"), QColor("#ef4444"), QColor("#4f6ef7"),
                                 QColor("#16a34a"), QColor("#f97316"), QColor("#7c5cff")};
        for (const QColor& c : colors) {
            auto* b = new QPushButton(this);
            b->setFixedSize(24, 24);
            b->setCursor(Qt::PointingHandCursor);
            b->setStyleSheet(QString("background:%1;border-radius:12px;border:2px solid transparent;").arg(c.name()));
            connect(b, &QPushButton::clicked, this, [this, c] { canvas_->set_color(c); });
            toolbar->addWidget(b);
        }
        toolbar->addSpacing(10);
        toolbar->addWidget(new QLabel("粗细", this));
        auto* width = new QSpinBox(this);
        width->setRange(1, 18);
        width->setValue(4);
        width->setFixedWidth(64);
        connect(width, qOverload<int>(&QSpinBox::valueChanged), this,
                [this](int v) { canvas_->set_width(v); });
        toolbar->addWidget(width);
        toolbar->addStretch(1);
        auto* undo = new QPushButton("撤销", this);
        undo->setObjectName("Ghost");
        connect(undo, &QPushButton::clicked, this, [this] { canvas_->undo(); });
        auto* clear = new QPushButton("清空", this);
        clear->setObjectName("Ghost");
        connect(clear, &QPushButton::clicked, this, [this] { canvas_->clear_canvas(); });
        toolbar->addWidget(undo);
        toolbar->addWidget(clear);
        layout->addLayout(toolbar);

        canvas_ = new SketchCanvas(this);
        layout->addWidget(canvas_);

        auto* buttons = new QHBoxLayout();
        auto* hint = new QLabel("画好的笔迹会作为 PNG 附件保存到当前记事", this);
        hint->setProperty("role", "muted");
        buttons->addWidget(hint);
        buttons->addStretch(1);
        auto* cancel = new QPushButton("取消", this);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        auto* save = new QPushButton("保存为附件", this);
        save->setObjectName("Primary");
        connect(save, &QPushButton::clicked, this, &QDialog::accept);
        buttons->addWidget(cancel);
        buttons->addWidget(save);
        layout->addLayout(buttons);
    }
    QImage image() const { return canvas_->image(); }

private:
    SketchCanvas* canvas_ = nullptr;
};

// ---------------------------------------------------------------- 主窗口
class MainWindow : public QMainWindow {
public:
    explicit MainWindow(Repository& repo, bool shot_mode = false)
        : repo_(repo), shot_mode_(shot_mode) {
        cfg_ = repo_.store.load_config();
        theme_ = QString::fromStdString(cfg_.value("theme", std::string("light")));
        accent_ = QColor(QString::fromStdString(cfg_.value("accent", std::string("#4f6ef7"))));
        if (!accent_.isValid()) accent_ = QColor("#4f6ef7");
        ringtone_ = QString::fromStdString(cfg_.value("ringtone", std::string("")));
        pal_ = theme_ == "dark" ? make_dark(accent_) : make_light(accent_);
        build_ui();
        build_menus();
        apply_theme(theme_, false);
        refresh_list(true);
        toast_ = new Toast(this);
        if (!shot_mode_) setup_tray();

        save_timer_ = new QTimer(this);
        save_timer_->setSingleShot(true);
        save_timer_->setInterval(650);
        connect(save_timer_, &QTimer::timeout, this, [this] { save_current(); });

        search_timer_ = new QTimer(this);
        search_timer_->setSingleShot(true);
        search_timer_->setInterval(220);
        connect(search_timer_, &QTimer::timeout, this, [this] {
            flush_pending();
            refresh_list(true);
        });

        if (!shot_mode_) {
            remind_timer_ = new QTimer(this);
            remind_timer_->setInterval(30000);
            connect(remind_timer_, &QTimer::timeout, this, [this] { check_reminders(); });
            remind_timer_->start();
            QTimer::singleShot(1000, this, [this] {
                greet_today();
                check_reminders();
            });
        }

        int w = cfg_.contains("window") ? cfg_["window"].value("w", 1280) : 1280;
        int h = cfg_.contains("window") ? cfg_["window"].value("h", 820) : 820;
        resize(std::max(1020, w), std::max(660, h));
        setMinimumSize(1020, 660);
        setAcceptDrops(true);
    }

    void load_first() {
        // 启动时只显示卡片列表，不预读任何卡片内容；点击卡片后才展开页面。
        apply_card_selection();
        update_batch_buttons();
    }

    // 自动化测试/截图用
    void debug_set_mode(const QString& mode) {
        mode_ = mode;
        tag_filter_.clear();
        if (nav_buttons_.contains(mode)) nav_buttons_[mode]->setChecked(true);
        refresh_list(true);
    }

    void debug_click_first() {
        auto events = visible_events();
        if (!events.empty()) select_event(events.front()->id, Qt::NoModifier);
    }

    void debug_open_first(bool edit) {
        auto events = visible_events();
        if (events.empty()) return;
        current_id_ = qs(events.front()->id);
        selection_.clear();
        selection_.insert(current_id_);
        anchor_id_ = current_id_;
        load_event(events.front(), edit);
    }

    void debug_close_detail() { close_detail(); }

    // 自动化测试/截图用：模拟 Ctrl（或 Ctrl+Shift）连选
    void select_test_multiple(int count, bool shift) {
        auto events = visible_events();
        if (events.size() < 2) return;
        count = std::min<int>(count, (int)events.size());
        select_event(events[0]->id, Qt::NoModifier);
        for (int i = 1; i < count; ++i) {
            select_event(events[i]->id,
                         shift ? (Qt::ShiftModifier | Qt::ControlModifier) : Qt::ControlModifier);
        }
    }

protected:
    void dragEnterEvent(QDragEnterEvent* e) override {
        if (e->mimeData()->hasUrls()) {
            e->acceptProposedAction();
        } else {
            QMainWindow::dragEnterEvent(e);
        }
    }
    void dropEvent(QDropEvent* e) override {
        if (e->mimeData()->hasUrls()) {
            QStringList files;
            for (const QUrl& url : e->mimeData()->urls()) {
                if (url.isLocalFile()) files << url.toLocalFile();
            }
            if (!files.isEmpty()) {
                attach_files(files);
                e->acceptProposedAction();
                return;
            }
        }
        QMainWindow::dropEvent(e);
    }

    void closeEvent(QCloseEvent* e) override {
        flush_pending();
        json cfg = repo_.store.load_config();
        cfg["theme"] = theme_.toStdString();
        cfg["window"] = json{{"w", width()}, {"h", height()}};
        repo_.store.save_config(cfg);
        QMainWindow::closeEvent(e);
    }

private:
    // ---------- UI 构建 ----------
    void build_ui() {
        auto* root = new QWidget(this);
        root->setObjectName("Root");
        auto* root_layout = new QHBoxLayout(root);
        root_layout->setContentsMargins(0, 0, 0, 0);
        root_layout->setSpacing(0);
        root_layout->addWidget(build_sidebar());

        splitter_ = new QSplitter(Qt::Horizontal, root);
        splitter_->setChildrenCollapsible(false);
        splitter_->setHandleWidth(2);
        splitter_->addWidget(build_center());
        splitter_->addWidget(build_editor_stack());
        splitter_->setStretchFactor(0, 0);
        splitter_->setStretchFactor(1, 1);
        splitter_->setSizes({420, 680});
        root_layout->addWidget(splitter_, 1);
        setCentralWidget(root);
        statusBar()->showMessage("墨记 MoJi　·　数据保存在本地，修改自动保存");
    }

    QFrame* build_sidebar() {
        auto* side = new QFrame(this);
        side->setObjectName("Sidebar");
        side->setFixedWidth(252);
        auto* layout = new QVBoxLayout(side);
        layout->setContentsMargins(18, 20, 18, 16);
        layout->setSpacing(10);

        auto* brand = new QHBoxLayout();
        brand->setSpacing(12);
        auto* mark = new QFrame(side);
        mark->setObjectName("BrandMark");
        mark->setFixedSize(50, 50);
        auto* ml = new QVBoxLayout(mark);
        ml->setContentsMargins(0, 0, 0, 0);
        auto* mark_text = new QLabel("墨", mark);
        mark_text->setObjectName("BrandMarkText");
        mark_text->setAlignment(Qt::AlignCenter);
        ml->addWidget(mark_text);
        brand->addWidget(mark);
        auto* titles = new QVBoxLayout();
        titles->setSpacing(0);
        auto* t1 = new QLabel("墨记 MoJi", side);
        t1->setProperty("role", "h1");
        auto* t2 = new QLabel("电子记事本", side);
        t2->setProperty("role", "muted");
        titles->addWidget(t1);
        titles->addWidget(t2);
        brand->addLayout(titles);
        brand->addStretch(1);
        layout->addLayout(brand);

        auto* grid = new QGridLayout();
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(8);
        stat_today_ = new StatCard("今日安排", side);
        stat_pending_ = new StatCard("待办", side);
        stat_overdue_ = new StatCard("逾期", side);
        stat_rate_ = new StatCard("完成率", side);
        grid->addWidget(stat_today_, 0, 0);
        grid->addWidget(stat_pending_, 0, 1);
        grid->addWidget(stat_overdue_, 1, 0);
        grid->addWidget(stat_rate_, 1, 1);
        layout->addLayout(grid);

        auto* view = new QLabel("视图", side);
        view->setProperty("role", "h3");
        layout->addWidget(view);
        nav_group_ = new QButtonGroup(this);
        nav_group_->setExclusive(true);
        const std::pair<const char*, const char*> modes[] = {
            {"all", "全部"}, {"today", "今天"}, {"pending", "待办"},
            {"done", "已完成"}, {"overdue", "逾期"}};
        for (const auto& kv : modes) {
            auto* b = new QPushButton(qs(kv.second), side);
            b->setObjectName("NavButton");
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            nav_group_->addButton(b);
            nav_buttons_[kv.first] = b;
            nav_titles_[kv.first] = qs(kv.second);
            connect(b, &QPushButton::clicked, this, [this, m = std::string(kv.first)] {
                flush_pending();
                mode_ = qs(m);
                tag_filter_.clear();
                refresh_list(true);
            });
            layout->addWidget(b);
        }
        nav_buttons_["all"]->setChecked(true);

        auto* tags_header = new QLabel("标签", side);
        tags_header->setProperty("role", "h3");
        layout->addWidget(tags_header);
        tags_scroll_ = new QScrollArea(side);
        tags_scroll_->setWidgetResizable(true);
        tags_scroll_->setFrameShape(QFrame::NoFrame);
        tags_scroll_->setMinimumHeight(88);
        tags_scroll_->setMaximumHeight(230);
        tags_container_ = new QWidget(tags_scroll_);
        tags_layout_ = new QVBoxLayout(tags_container_);
        tags_layout_->setContentsMargins(0, 0, 0, 0);
        tags_layout_->setSpacing(4);
        tags_scroll_->setWidget(tags_container_);
        layout->addWidget(tags_scroll_, 1);

        auto* progress_row = new QHBoxLayout();
        auto* progress_label = new QLabel("完成度", side);
        progress_label->setProperty("role", "muted");
        progress_ = new QProgressBar(side);
        progress_->setRange(0, 100);
        progress_->setTextVisible(false);
        progress_row->addWidget(progress_label);
        progress_row->addWidget(progress_, 1);
        layout->addLayout(progress_row);

        auto* bottom = new QHBoxLayout();
        theme_button_ = new QPushButton(theme_ == "dark" ? "浅色主题" : "深色主题", side);
        theme_button_->setObjectName("Ghost");
        theme_button_->setCursor(Qt::PointingHandCursor);
        connect(theme_button_, &QPushButton::clicked, this, [this] {
            apply_theme(theme_ == "dark" ? "light" : "dark", true);
        });
        auto* data_button = new QPushButton("数据目录", side);
        data_button->setObjectName("Ghost");
        data_button->setCursor(Qt::PointingHandCursor);
        connect(data_button, &QPushButton::clicked, this, [this] {
            QDesktopServices::openUrl(QUrl::fromLocalFile(qpath(repo_.store.dir)));
        });
        data_button->setToolTip(qpath(repo_.store.dir));
        bottom->addWidget(theme_button_);
        bottom->addWidget(data_button);
        bottom->addStretch(1);
        layout->addLayout(bottom);
        auto* local = new QLabel("数据保存在本地\nv1.0.0 · C++17 / Qt", side);
        local->setProperty("role", "muted");
        local->setStyleSheet("font-size:9pt;");
        layout->addWidget(local);
        return side;
    }

    QFrame* build_center() {
        auto* panel = new QFrame(this);
        panel->setObjectName("CenterPanel");
        auto* layout = new QVBoxLayout(panel);
        layout->setContentsMargins(22, 18, 16, 12);
        layout->setSpacing(10);

        auto* header = new QHBoxLayout();
        auto* title = new QLabel("记事", panel);
        title->setProperty("role", "h1");
        header->addWidget(title);
        header->addStretch(1);
        auto* add = new QPushButton("+ 新建", panel);
        add->setObjectName("Primary");
        add->setCursor(Qt::PointingHandCursor);
        connect(add, &QPushButton::clicked, this, [this] { new_event(); });
        header->addWidget(add);
        layout->addLayout(header);

        search_edit_ = new QLineEdit(panel);
        search_edit_->setObjectName("SearchEdit");
        search_edit_->setPlaceholderText("🔍  搜索标题、标签、正文…");
        search_edit_->setClearButtonEnabled(true);
        connect(search_edit_, &QLineEdit::textChanged, this, [this] { search_timer_->start(); });
        layout->addWidget(search_edit_);

        auto* count_row = new QHBoxLayout();
        count_label_ = new QLabel("共 0 条", panel);
        count_label_->setProperty("role", "muted");
        count_row->addWidget(count_label_);
        count_row->addStretch(1);
        batch_done_button_ = new QPushButton("完成选中", panel);
        batch_done_button_->setObjectName("Ghost");
        batch_done_button_->setStyleSheet("font-size:9pt;");
        batch_done_button_->setVisible(false);
        batch_delete_button_ = new QPushButton("删除选中", panel);
        batch_delete_button_->setObjectName("Danger");
        batch_delete_button_->setStyleSheet("font-size:9pt;");
        batch_delete_button_->setVisible(false);
        connect(batch_done_button_, &QPushButton::clicked, this, [this] { batch_done(); });
        connect(batch_delete_button_, &QPushButton::clicked, this, [this] { batch_delete(); });
        count_row->addWidget(batch_done_button_);
        count_row->addWidget(batch_delete_button_);
        layout->addLayout(count_row);

        cards_scroll_ = new QScrollArea(panel);
        cards_scroll_->setWidgetResizable(true);
        cards_scroll_->setFrameShape(QFrame::NoFrame);
        cards_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        cards_container_ = new QWidget(cards_scroll_);
        cards_layout_ = new QVBoxLayout(cards_container_);
        cards_layout_->setContentsMargins(0, 0, 6, 12);
        cards_layout_->setSpacing(10);
        cards_layout_->addStretch(1);
        cards_scroll_->setWidget(cards_container_);
        layout->addWidget(cards_scroll_, 1);
        return panel;
    }

    QStackedWidget* build_editor_stack() {
        editor_stack_ = new QStackedWidget(this);
        editor_stack_->setMinimumWidth(430);
        editor_stack_->addWidget(build_view_page());
        editor_stack_->addWidget(build_editor());
        editor_stack_->setCurrentIndex(0);
        editor_stack_->hide();
        return editor_stack_;
    }

    QFrame* build_view_page() {
        auto* panel = new QFrame(this);
        view_panel_ = panel;
        panel->setObjectName("EditorPanel");
        auto* layout = new QVBoxLayout(panel);
        layout->setContentsMargins(26, 20, 26, 14);
        layout->setSpacing(10);

        auto* header = new QHBoxLayout();
        header->setSpacing(8);
        view_title_ = new QLabel("", panel);
        view_title_->setObjectName("ViewTitle");
        view_title_->setWordWrap(true);
        header->addWidget(view_title_, 1);
        auto* edit = new QPushButton("✎ 编辑", panel);
        edit->setObjectName("Primary");
        edit->setCursor(Qt::PointingHandCursor);
        edit->setToolTip("进入编辑界面");
        connect(edit, &QPushButton::clicked, this, [this] {
            if (Event* e = current()) load_editor(e);
        });
        auto* close = new QPushButton("✕", panel);
        close->setObjectName("IconButton");
        close->setCursor(Qt::PointingHandCursor);
        close->setToolTip("关闭右侧页面");
        connect(close, &QPushButton::clicked, this, [this] { close_detail(); });
        header->addWidget(edit);
        header->addWidget(close);
        layout->addLayout(header);

        view_meta_ = new QLabel(panel);
        view_meta_->setObjectName("ViewMeta");
        view_meta_->setWordWrap(true);
        layout->addWidget(view_meta_);

        view_page_ = new PageEditor(repo_, panel);
        view_page_->set_read_only(true);
        layout->addWidget(view_page_, 1);

        view_att_panel_ = new QWidget(panel);
        auto* att_layout = new QVBoxLayout(view_att_panel_);
        att_layout->setContentsMargins(0, 0, 0, 0);
        att_layout->setSpacing(4);
        auto* att_title = new QLabel("附件", view_att_panel_);
        att_title->setObjectName("ViewSection");
        att_layout->addWidget(att_title);
        view_att_list_ = new QListWidget(view_att_panel_);
        view_att_list_->setObjectName("Attachments");
        view_att_list_->setViewMode(QListWidget::IconMode);
        view_att_list_->setIconSize(QSize(76, 56));
        view_att_list_->setGridSize(QSize(104, 104));
        view_att_list_->setResizeMode(QListWidget::Adjust);
        view_att_list_->setMovement(QListWidget::Snap);
        view_att_list_->setSpacing(4);
        view_att_list_->setWordWrap(true);
        view_att_list_->setTextElideMode(Qt::ElideRight);
        view_att_list_->setDragDropMode(QAbstractItemView::NoDragDrop);
        view_att_list_->setMaximumHeight(122);
        connect(view_att_list_, &QListWidget::itemDoubleClicked, this,
                [this](QListWidgetItem* item) { open_attachment(item); });
        att_layout->addWidget(view_att_list_);
        layout->addWidget(view_att_panel_);
        view_att_panel_->hide();
        return panel;
    }

    QFrame* build_editor() {
        auto* panel = new QFrame(this);
        panel->setObjectName("EditorPanel");
        auto* layout = new QVBoxLayout(panel);
        layout->setContentsMargins(26, 20, 26, 14);
        layout->setSpacing(10);

        auto* edit_header = new QHBoxLayout();
        auto* edit_label = new QLabel("编辑记事", panel);
        edit_label->setProperty("role", "h3");
        auto* finish = new QPushButton("完成", panel);
        finish->setObjectName("Primary");
        finish->setCursor(Qt::PointingHandCursor);
        finish->setToolTip("保存并返回页面视图");
        connect(finish, &QPushButton::clicked, this, [this] {
            if (save_current()) show_view_current();
        });
        auto* close = new QPushButton("✕", panel);
        close->setObjectName("IconButton");
        close->setCursor(Qt::PointingHandCursor);
        close->setToolTip("关闭右侧页面");
        connect(close, &QPushButton::clicked, this, [this] { close_detail(); });
        edit_header->addWidget(edit_label);
        edit_header->addStretch(1);
        edit_header->addWidget(finish);
        edit_header->addWidget(close);
        layout->addLayout(edit_header);

        title_edit_ = new QLineEdit(panel);
        title_edit_->setObjectName("TitleEdit");
        title_edit_->setPlaceholderText("标题");
        connect(title_edit_, &QLineEdit::textEdited, this, [this] { schedule_save(); });
        connect(title_edit_, &QLineEdit::editingFinished, this, [this] { save_current(); });
        layout->addWidget(title_edit_);

        meta_label_ = new QLabel(panel);
        meta_label_->setProperty("role", "muted");
        layout->addWidget(meta_label_);

        auto* look_row = new QHBoxLayout();
        look_row->setSpacing(6);
        auto* look_label = new QLabel("外观", panel);
        look_label->setProperty("role", "muted");
        color_button_ = new QPushButton(panel);
        color_button_->setFixedSize(24, 24);
        color_button_->setCursor(Qt::PointingHandCursor);
        color_button_->setToolTip("事项色条颜色");
        connect(color_button_, &QPushButton::clicked, this, [this] { choose_color(); });
        auto* icon_label = new QLabel("图标", panel);
        icon_label->setProperty("role", "muted");
        icon_edit_ = new QLineEdit(panel);
        icon_edit_->setFixedWidth(110);
        icon_edit_->setPlaceholderText("emoji / 字符");
        connect(icon_edit_, &QLineEdit::textEdited, this, [this](const QString& text) {
            current_icon_ = text;
            schedule_save();
        });
        connect(icon_edit_, &QLineEdit::editingFinished, this, [this] {
            current_icon_ = icon_edit_->text();
            save_current();
        });
        look_row->addWidget(look_label);
        look_row->addWidget(color_button_);
        look_row->addSpacing(8);
        look_row->addWidget(icon_label);
        look_row->addWidget(icon_edit_);
        look_row->addStretch(1);
        layout->addLayout(look_row);

        auto* due_row = new QHBoxLayout();
        due_check_ = new QCheckBox("截止", panel);
        due_edit_ = new QDateTimeEdit(panel);
        due_edit_->setCalendarPopup(true);
        due_edit_->setDisplayFormat("yyyy-MM-dd HH:mm");
        due_edit_->setMinimumWidth(168);
        remind_spin_ = new QSpinBox(panel);
        remind_spin_->setRange(0, 10080);
        remind_spin_->setSuffix(" 分钟前提醒");
        remind_spin_->setToolTip("0 表示到点提醒；最多提前 7 天");
        connect(due_check_, &QCheckBox::toggled, this, [this](bool checked) {
            due_edit_->setEnabled(checked);
            if (checked && !loading_ && due_edit_->dateTime().isNull()) {
                due_edit_->setDateTime(QDateTime::currentDateTime().addSecs(3600));
            }
            schedule_save();
        });
        connect(due_edit_, &QDateTimeEdit::dateTimeChanged, this, [this] { schedule_save(); });
        connect(remind_spin_, qOverload<int>(&QSpinBox::valueChanged), this,
                [this](int) { schedule_save(); });
        due_row->addWidget(due_check_);
        due_row->addWidget(due_edit_, 1);
        due_row->addWidget(remind_spin_);
        layout->addLayout(due_row);

        auto* notify_row = new QHBoxLayout();
        system_notify_check_ = new QCheckBox("提醒时使用 Windows 系统通知", panel);
        system_notify_check_->setToolTip("开启后，到点提醒会在 Windows 通知中心弹出系统通知");
        connect(system_notify_check_, &QCheckBox::toggled, this, [this](bool) {
            schedule_save();
        });
        notify_row->addWidget(system_notify_check_);
        notify_row->addStretch(1);
        layout->addLayout(notify_row);

        auto* tags_row = new QHBoxLayout();
        tags_edit_ = new QLineEdit(panel);
        tags_edit_->setPlaceholderText("标签：课程, 截止（逗号分隔）");
        connect(tags_edit_, &QLineEdit::textEdited, this, [this] { schedule_save(); });
        connect(tags_edit_, &QLineEdit::editingFinished, this, [this] { save_current(); });
        tags_row->addWidget(tags_edit_, 1);
        auto* priority_label = new QLabel("优先级", panel);
        priority_label->setProperty("role", "muted");
        priority_spin_ = new QSpinBox(panel);
        priority_spin_->setRange(1, 999);
        priority_spin_->setPrefix("P");
        priority_spin_->setFixedWidth(72);
        priority_spin_->setToolTip("直接输入目标优先级；原位置及之后自动顺延");
        connect(priority_spin_, qOverload<int>(&QSpinBox::valueChanged), this,
                [this](int value) { move_to_priority(value); });
        tags_row->addWidget(priority_label);
        tags_row->addWidget(priority_spin_);
        layout->addLayout(tags_row);

        auto* toolbar = new QHBoxLayout();
        toolbar->setSpacing(6);
        auto* save = new QPushButton("保存", panel);
        save->setObjectName("Primary");
        connect(save, &QPushButton::clicked, this, [this] { save_current(true); });
        done_button_ = new QPushButton("标记完成", panel);
        done_button_->setObjectName("Ghost");
        connect(done_button_, &QPushButton::clicked, this, [this] { toggle_current(); });
        auto* duplicate = new QPushButton("复制", panel);
        duplicate->setObjectName("Ghost");
        connect(duplicate, &QPushButton::clicked, this, [this] { duplicate_current(); });
        auto* up = new QPushButton("↑", panel);
        up->setObjectName("IconButton");
        up->setToolTip("提高优先级");
        connect(up, &QPushButton::clicked, this, [this] { move_current(-1); });
        auto* down = new QPushButton("↓", panel);
        down->setObjectName("IconButton");
        down->setToolTip("降低优先级");
        connect(down, &QPushButton::clicked, this, [this] { move_current(1); });
        toolbar->addWidget(save);
        toolbar->addWidget(done_button_);
        toolbar->addWidget(duplicate);
        toolbar->addWidget(up);
        toolbar->addWidget(down);
        toolbar->addStretch(1);
        auto* undo = new QPushButton("撤销", panel);
        undo->setObjectName("Ghost");
        undo->setToolTip("撤销应用操作 (Ctrl+Alt+Z)");
        connect(undo, &QPushButton::clicked, this, [this] { undo_action(false); });
        auto* redo = new QPushButton("重做", panel);
        redo->setObjectName("Ghost");
        connect(redo, &QPushButton::clicked, this, [this] { undo_action(true); });
        auto* del = new QPushButton("删除", panel);
        del->setObjectName("Danger");
        connect(del, &QPushButton::clicked, this, [this] { delete_current(); });
        toolbar->addWidget(undo);
        toolbar->addWidget(redo);
        toolbar->addWidget(del);
        layout->addLayout(toolbar);

        auto* note_header = new QHBoxLayout();
        auto* note_title = new QLabel("备注", panel);
        note_title->setProperty("role", "h3");
        auto* note_hint = new QLabel("支持 Markdown · 可粘贴图片、拖入文件", panel);
        note_hint->setProperty("role", "muted");
        note_hint->setStyleSheet("font-size:9pt;");
        auto* table = new QPushButton("插入表格", panel);
        table->setObjectName("Ghost");
        table->setStyleSheet("font-size:9pt;");
        connect(table, &QPushButton::clicked, this, [this] { insert_table(); });
        auto* sketch = new QPushButton("墨迹页", panel);
        sketch->setObjectName("Ghost");
        sketch->setStyleSheet("font-size:9pt;");
        connect(sketch, &QPushButton::clicked, this, [this] {
            if (note_tabs_) note_tabs_->setCurrentIndex(2);
            if (page_) page_->set_ink_mode(true);
        });
        note_header->addWidget(note_title);
        note_header->addWidget(note_hint);
        note_header->addStretch(1);
        note_header->addWidget(table);
        note_header->addWidget(sketch);
        layout->addLayout(note_header);

        note_tabs_ = new QTabWidget(panel);
        note_tabs_->setObjectName("NoteTabs");
        auto* edit_tab = new QWidget(note_tabs_);
        auto* edit_layout = new QVBoxLayout(edit_tab);
        edit_layout->setContentsMargins(0, 0, 0, 0);
        edit_layout->setSpacing(4);

        auto* fmt = new QHBoxLayout();
        fmt->setSpacing(4);
        auto add_fmt = [&](const QString& label, const QString& prefix,
                           const QString& suffix, const QString& tip) {
            auto* b = new QPushButton(label, edit_tab);
            b->setObjectName("IconButton");
            b->setToolTip(tip);
            b->setStyleSheet("font-size:10pt;");
            connect(b, &QPushButton::clicked, this, [this, prefix, suffix] {
                wrap_note(prefix, suffix);
            });
            fmt->addWidget(b);
        };
        add_fmt("B", "**", "**", "粗体");
        add_fmt("I", "*", "*", "斜体");
        add_fmt("U", "++", "++", "下划线");
        add_fmt("S", "~~", "~~", "删除线");
        auto* color_btn = new QPushButton("颜色", edit_tab);
        color_btn->setObjectName("Ghost");
        color_btn->setStyleSheet("font-size:9pt;");
        connect(color_btn, &QPushButton::clicked, this, [this] { wrap_note_color(); });
        fmt->addWidget(color_btn);
        auto* table_btn = new QPushButton("表格", edit_tab);
        table_btn->setObjectName("Ghost");
        table_btn->setStyleSheet("font-size:9pt;");
        connect(table_btn, &QPushButton::clicked, this, [this] { insert_table(); });
        fmt->addWidget(table_btn);
        fmt->addStretch(1);
        edit_layout->addLayout(fmt);

        note_edit_ = new NoteEdit(edit_tab);
        note_edit_->setPlaceholderText("写点什么… 支持 Markdown、表格、{color:#e11}彩色{/color}");
        connect(note_edit_, &QTextEdit::textChanged, this, [this] {
            schedule_save();
            if (markdown_timer_) markdown_timer_->start();
        });
        note_edit_->image_pasted = [this](const QImage& img) { attach_image(img); };
        note_edit_->files_dropped = [this](const QStringList& files) { attach_files(files); };
        edit_layout->addWidget(note_edit_, 1);

        preview_ = new QTextBrowser(note_tabs_);
        preview_->setObjectName("Preview");
        preview_->setOpenExternalLinks(true);

        page_ = new PageEditor(repo_, note_tabs_);
        page_->on_add_image = [this] { choose_attachments(); };
        page_->on_layout_changed = [this](const QString& json) {
            if (loading_ || current_id_.isEmpty()) return;
            try {
                repo_.set_visual(current_id_.toStdString(), std::nullopt, std::nullopt,
                                 json.toStdString());
            } catch (...) {
            }
        };

        note_tabs_->addTab(edit_tab, "编辑");
        note_tabs_->addTab(preview_, "预览");
        note_tabs_->addTab(page_, "页面");
        connect(note_tabs_, &QTabWidget::currentChanged, this, [this](int index) {
            if (index == 1) update_preview();
            if (index == 2) {
                if (Event* e = current()) page_->set_event(e);
            }
            if (att_panel_) att_panel_->setVisible(index != 2);
        });
        layout->addWidget(note_tabs_, 1);

        markdown_timer_ = new QTimer(this);
        markdown_timer_->setSingleShot(true);
        markdown_timer_->setInterval(180);
        connect(markdown_timer_, &QTimer::timeout, this, [this] {
            update_preview();
            if (page_) page_->set_note(note_edit_->toPlainText());
        });

        att_panel_ = new QWidget(panel);
        auto* att_panel_layout = new QVBoxLayout(att_panel_);
        att_panel_layout->setContentsMargins(0, 0, 0, 0);
        att_panel_layout->setSpacing(4);
        att_panel_->setMaximumHeight(170);

        auto* att_header = new QHBoxLayout();
        auto* att_title = new QLabel("附件", panel);
        att_title->setProperty("role", "h3");
        auto* att_hint = new QLabel("拖入文件 · 双击打开 · 右键移除", panel);
        att_hint->setProperty("role", "muted");
        att_hint->setStyleSheet("font-size:9pt;");
        auto* add_att = new QPushButton("+ 添加", panel);
        add_att->setObjectName("Ghost");
        add_att->setStyleSheet("font-size:9pt;");
        connect(add_att, &QPushButton::clicked, this, [this] { choose_attachments(); });
        att_header->addWidget(att_title);
        att_header->addWidget(att_hint);
        att_header->addStretch(1);
        att_header->addWidget(add_att);
        att_panel_layout->addLayout(att_header);

        att_list_ = new AttachmentList(panel);
        att_list_->files_dropped = [this](const QStringList& files) { attach_files(files); };
        att_list_->reordered = [this](const QStringList& paths) { reorder_attachments(paths); };
        att_list_->remove_requested = [this](int index) { remove_attachment(index); };
        att_list_->open_requested = [this](int index) {
            if (att_list_->item(index)) open_attachment(att_list_->item(index));
        };
        connect(att_list_, &QListWidget::itemDoubleClicked, this,
                [this](QListWidgetItem* item) { open_attachment(item); });
        att_panel_layout->addWidget(att_list_);
        layout->addWidget(att_panel_);

        status_label_ = new QLabel("", panel);
        status_label_->setProperty("role", "muted");
        status_label_->setStyleSheet("font-size:9pt;");
        layout->addWidget(status_label_);
        return panel;
    }

    QWidget* build_empty_page() {
        auto* page = new QFrame(this);
        page->setObjectName("EditorEmpty");
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(40, 40, 40, 40);
        layout->addStretch(2);
        auto* icon = new QLabel("🗒", page);
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet("font-size:46pt;");
        auto* title = new QLabel("开始记录今天", page);
        title->setObjectName("EmptyTitle");
        title->setAlignment(Qt::AlignCenter);
        auto* hint = new QLabel("在左侧选择一条记事，或新建一条。\n所有内容都会自动保存到本地。", page);
        hint->setObjectName("EmptyHint");
        hint->setAlignment(Qt::AlignCenter);
        auto* btn = new QPushButton("+ 新建记事", page);
        btn->setObjectName("Primary");
        btn->setFixedWidth(160);
        connect(btn, &QPushButton::clicked, this, [this] { new_event(); });
        layout->addWidget(icon);
        layout->addWidget(title);
        layout->addWidget(hint);
        layout->addSpacing(14);
        auto* row = new QHBoxLayout();
        row->addStretch(1);
        row->addWidget(btn);
        row->addStretch(1);
        layout->addLayout(row);
        layout->addStretch(3);
        return page;
    }

    void build_menus() {
        auto* file = menuBar()->addMenu("文件");
        auto* act_new = file->addAction("新建记事");
        act_new->setShortcut(QKeySequence("Ctrl+N"));
        connect(act_new, &QAction::triggered, this, [this] { new_event(); });
        auto* act_import = file->addAction("导入…");
        act_import->setShortcut(QKeySequence("Ctrl+O"));
        connect(act_import, &QAction::triggered, this, [this] { import_data(); });
        auto* act_export = file->addAction("导出…");
        act_export->setShortcut(QKeySequence("Ctrl+Shift+S"));
        connect(act_export, &QAction::triggered, this, [this] { export_data(); });
        file->addSeparator();
        auto* act_quit = file->addAction("退出");
        act_quit->setShortcut(QKeySequence("Ctrl+Q"));
        connect(act_quit, &QAction::triggered, this, [this] { close(); });

        auto* edit = menuBar()->addMenu("编辑");
        auto* act_undo = edit->addAction("撤销（应用操作）");
        act_undo->setShortcut(QKeySequence("Ctrl+Alt+Z"));
        connect(act_undo, &QAction::triggered, this, [this] { undo_action(false); });
        auto* act_redo = edit->addAction("重做");
        act_redo->setShortcut(QKeySequence("Ctrl+Alt+Y"));
        connect(act_redo, &QAction::triggered, this, [this] { undo_action(true); });
        edit->addSeparator();
        auto* act_done = edit->addAction("标记完成 / 取消");
        act_done->setShortcut(QKeySequence("Ctrl+Return"));
        connect(act_done, &QAction::triggered, this, [this] { toggle_current(); });
        auto* act_table = edit->addAction("插入表格");
        connect(act_table, &QAction::triggered, this, [this] { insert_table(); });
        auto* act_sketch = edit->addAction("手绘笔迹…");
        connect(act_sketch, &QAction::triggered, this, [this] { open_sketch(); });

        auto* view = menuBar()->addMenu("视图");
        auto* act_search = view->addAction("聚焦搜索");
        act_search->setShortcut(QKeySequence("Ctrl+F"));
        connect(act_search, &QAction::triggered, this, [this] { search_edit_->setFocus(); });
        auto* act_refresh = view->addAction("刷新");
        act_refresh->setShortcut(QKeySequence("F5"));
        connect(act_refresh, &QAction::triggered, this, [this] {
            repo_.reload();
            refresh_list(true);
            status_label_->setText("已刷新");
        });
        view->addSeparator();
        auto* act_accent = view->addAction("主题色…");
        connect(act_accent, &QAction::triggered, this, [this] { choose_accent(); });
        auto* act_ring = view->addAction("选择铃声…");
        connect(act_ring, &QAction::triggered, this, [this] { choose_ringtone(); });
        view->addSeparator();
        auto* group = new QActionGroup(this);
        group->setExclusive(true);
        light_action_ = view->addAction("浅色主题");
        light_action_->setCheckable(true);
        dark_action_ = view->addAction("深色主题");
        dark_action_->setCheckable(true);
        group->addAction(light_action_);
        group->addAction(dark_action_);
        connect(light_action_, &QAction::triggered, this, [this] { apply_theme("light", true); });
        connect(dark_action_, &QAction::triggered, this, [this] { apply_theme("dark", true); });

        auto* help = menuBar()->addMenu("帮助");
        auto* act_keys = help->addAction("快捷键");
        act_keys->setShortcut(QKeySequence("F1"));
        connect(act_keys, &QAction::triggered, this, [this] { show_help(); });
        auto* act_about = help->addAction("关于墨记");
        connect(act_about, &QAction::triggered, this, [this] { show_about(); });
    }

    void setup_tray() {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
        tray_ = new QSystemTrayIcon(windowIcon(), this);
        tray_->setToolTip("墨记 MoJi");
        auto* menu = new QMenu(this);
        menu->addAction("显示主窗口", this, [this] {
            showNormal();
            raise();
            activateWindow();
        });
        menu->addSeparator();
        menu->addAction("退出", this, [this] { close(); });
        tray_->setContextMenu(menu);
        tray_->show();
    }

    void show_system_notification(const QString& title, const QString& body, int ms = 10000) {
        if (!tray_ || !tray_->isVisible()) return;
        tray_->showMessage(title, body, QSystemTrayIcon::Information, ms);
    }

    // ---------- 数据与刷新 ----------
    std::vector<Event*> visible_events() {
        std::vector<Event*> base;
        if (mode_ == "today") base = repo_.due_on(now_time());
        else if (mode_ == "pending") base = repo_.pending();
        else if (mode_ == "done") base = repo_.finished();
        else if (mode_ == "overdue") base = repo_.overdue();
        else base = repo_.all();
        if (!tag_filter_.isEmpty()) {
            std::vector<Event*> filtered;
            for (Event* e : base) {
                if (std::find(e->tags.begin(), e->tags.end(), tag_filter_.toStdString()) != e->tags.end()) {
                    filtered.push_back(e);
                }
            }
            base = filtered;
        }
        QString key = search_edit_ ? search_edit_->text().trimmed().toLower() : QString();
        if (key.isEmpty()) return base;
        std::vector<Event*> out;
        for (Event* e : base) {
            QString hay = qs(e->title + " " + e->note + " " + join(e->tags, " ")).toLower();
            for (const auto& a : e->attachments) hay += " " + qs(a.name).toLower();
            if (hay.contains(key)) out.push_back(e);
        }
        return out;
    }

    void refresh_list(bool /*select_first*/) {
        if (!cards_layout_) return;
        int scroll_value = cards_scroll_->verticalScrollBar()->value();
        while (QLayoutItem* item = cards_layout_->takeAt(0)) {
            if (QWidget* w = item->widget()) w->deleteLater();
            delete item;
        }
        cards_.clear();
        auto events = visible_events();
        for (Event* e : events) {
            std::string id = e->id;
            auto* card = new EventCard(*e, pal_,
                                       [this, id](Qt::KeyboardModifiers mods) {
                                           select_event(id, mods);
                                       },
                                       [this, id] { toggle_event(id); },
                                       [this, id] {
                                           select_event(id, Qt::NoModifier);
                                           Event* e = repo_.find(id);
                                           if (e) {
                                               load_editor(e);
                                               title_edit_->setFocus();
                                           }
                                       },
                                       cards_container_);
            card->set_selected(selection_.contains(qs(id)));
            cards_[qs(id)] = card;
            cards_layout_->addWidget(card);
        }
        if (events.empty()) {
            // 逾期/已完成等分类为空时只留一句提示，不再推“新建”引导，
            // 右侧详情分屏也保持关闭。
            auto* empty = new QLabel("没有符合条件的记事", cards_container_);
            empty->setAlignment(Qt::AlignCenter);
            empty->setProperty("role", "muted");
            empty->setMinimumHeight(120);
            cards_layout_->addWidget(empty);
        }
        cards_layout_->addStretch(1);

        auto stats = repo_.stats();
        stat_today_->set_value(QString::number(stats.today), pal_.primary);
        stat_pending_->set_value(QString::number(stats.pending));
        stat_overdue_->set_value(QString::number(stats.overdue),
                                 stats.overdue ? pal_.error : pal_.muted);
        stat_rate_->set_value(QString::number(stats.rate) + "%", pal_.success);
        progress_->setValue(stats.rate);
        const int counts[] = {stats.total, stats.today, stats.pending, stats.done, stats.overdue};
        const char* keys[] = {"all", "today", "pending", "done", "overdue"};
        for (int i = 0; i < 5; ++i) {
            nav_buttons_[keys[i]]->setText(nav_titles_[keys[i]] + "   " + QString::number(counts[i]));
        }
        count_label_->setText("共 " + QString::number(events.size()) + " 条 · 按优先级排序");
        QSet<QString> visible_ids;
        for (Event* e : events) visible_ids.insert(qs(e->id));
        selection_.intersect(visible_ids);
        update_batch_buttons();

        bool visible_current = false;
        for (Event* e : events) {
            if (e->id == current_id_.toStdString()) visible_current = true;
        }
        if (events.empty() || !visible_current) {
            // 启动、切换分类、搜索后都不自动读取卡片内容；只有用户点击才展开。
            if (events.empty()) selection_.clear();
            current_id_.clear();
            close_detail(false);
        }
        rebuild_tags();
        int id = std::max(0, std::min(scroll_value, cards_scroll_->verticalScrollBar()->maximum()));
        QTimer::singleShot(0, this, [this, id] { cards_scroll_->verticalScrollBar()->setValue(id); });
    }

    void rebuild_tags() {
        if (!tags_layout_) return;
        while (QLayoutItem* item = tags_layout_->takeAt(0)) {
            if (QWidget* w = item->widget()) w->deleteLater();
            delete item;
        }
        auto tags = repo_.tags();
        if (tags.empty()) {
            auto* empty = new QLabel("暂无标签", tags_container_);
            empty->setProperty("role", "muted");
            empty->setStyleSheet("font-size:9pt;");
            tags_layout_->addWidget(empty);
        } else {
            int shown = 0;
            for (const auto& kv : tags) {
                if (++shown > 20) break;
                QString tag = qs(kv.first);
                auto* button = new QPushButton("#" + tag + "  " + QString::number(kv.second), tags_container_);
                button->setObjectName("TagChip");
                button->setCheckable(true);
                button->setChecked(tag_filter_ == tag);
                button->setCursor(Qt::PointingHandCursor);
                button->setToolTip(QString::number(kv.second) + " 条待办");
                connect(button, &QPushButton::clicked, this, [this, tag](bool checked) {
                    flush_pending();
                    tag_filter_ = checked ? tag : QString();
                    mode_ = "all";
                    nav_buttons_["all"]->setChecked(true);
                    refresh_list(true);
                });
                tags_layout_->addWidget(button);
            }
        }
        tags_layout_->addStretch(1);
    }

    QString meta_for(Event* e) const {
        if (!e) return QString();
        QString meta = "P" + QString::number(e->priority) + "  ·  截止 " +
                       qs(humanize_due(e->due));
        if (e->created_at.has_value()) {
            meta += "  ·  创建于 " + qs(date_str(*e->created_at));
        }
        if (!e->attachments.empty()) {
            meta += "  ·  附件 " + QString::number(e->attachments.size());
        }
        if (e->system_notify) meta += "  ·  Windows 通知";
        return meta;
    }

    void update_meta(Event* e) {
        if (!e) {
            meta_label_->clear();
            return;
        }
        meta_label_->setText(meta_for(e));
    }

    void fill_attachments(QListWidget* list, Event* e) {
        if (!list) return;
        list->clear();
        if (!e) return;
        QFileIconProvider provider;
        for (const auto& att : e->attachments) {
            fs::path absolute;
            try {
                absolute = repo_.store.resolve(att);
            } catch (...) {
            }
            QIcon icon;
            QPixmap px(qpath(absolute));
            if (!px.isNull()) {
                icon = QIcon(px.scaled(QSize(76, 56), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                icon = provider.icon(QFileInfo(qpath(absolute)));
            }
            auto* item = new QListWidgetItem(icon, qs(att.name), list);
            item->setData(Qt::UserRole, qs(att.path));
            item->setToolTip(qpath(absolute));
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        }
    }

    void load_event(Event* e, bool edit) {
        loading_ = true;
        if (!e) {
            loading_ = false;
            close_detail(false);
            return;
        }
        title_edit_->setText(qs(e->title));
        note_edit_->setPlainText(qs(e->note));
        due_check_->setChecked(e->due.has_value());
        due_edit_->setEnabled(e->due.has_value());
        if (e->due.has_value()) {
            due_edit_->setDateTime(QDateTime::fromSecsSinceEpoch((qint64)*e->due));
        } else {
            due_edit_->setDateTime(QDateTime::currentDateTime().addSecs(3600));
        }
        remind_spin_->setValue(e->remind_before);
        if (system_notify_check_) system_notify_check_->setChecked(e->system_notify);
        tags_edit_->setText(qs(join(e->tags, ", ")));
        current_color_ = qs(e->color);
        current_icon_ = qs(e->icon);
        if (icon_edit_) icon_edit_->setText(current_icon_);
        update_color_button();
        if (priority_spin_) {
            priority_spin_->blockSignals(true);
            priority_spin_->setMaximum(std::max(1, (int)repo_.events.size()));
            priority_spin_->setValue(e->priority);
            priority_spin_->blockSignals(false);
        }
        update_meta(e);
        reload_attachments();
        if (page_) page_->set_event(e);
        if (view_page_) {
            view_page_->set_dark(theme_ == "dark");
            view_page_->set_event(e);
        }
        if (view_title_) view_title_->setText(qs(e->title));
        if (view_meta_) view_meta_->setText(meta_for(e));
        if (view_att_list_) fill_attachments(view_att_list_, e);
        if (view_att_panel_) view_att_panel_->setVisible(!e->attachments.empty());
        update_preview();
        done_button_->setText(e->done ? "取消完成" : "标记完成");
        status_label_->setText("已保存 · " + clock_text(e->updated_at.value_or(now_time())));
        editing_ = edit;
        if (editor_stack_) editor_stack_->setCurrentIndex(edit ? 1 : 0);
        loading_ = false;
        show_detail();
        apply_card_selection();
    }

    void load_editor(Event* e) { load_event(e, true); }
    void load_view(Event* e) { load_event(e, false); }

    void show_view_current() {
        if (Event* e = current()) load_view(e);
    }

    void apply_detail_sizes() {
        if (!splitter_ || !editor_stack_ || !detail_visible_) return;
        QList<int> sizes = splitter_->sizes();
        if (sizes.size() == 2 && sizes[1] > 300) detail_width_ = sizes[1];
        int total = std::max(700, splitter_->width());
        // 中间列表至少保留约 480px，详情最多 760px，避免卡片被挤得太窄。
        int right = std::max(430, std::min(760, std::min(detail_width_, total - 480)));
        splitter_->setSizes({total - right, right});
    }

    void show_detail() {
        if (!editor_stack_) return;
        editor_stack_->show();
        detail_visible_ = true;
        apply_detail_sizes();
        // QSplitter 在子控件由隐藏变可见时会重新分配尺寸，下一轮再压一次。
        QTimer::singleShot(0, this, [this] { apply_detail_sizes(); });
    }

    void close_detail(bool flush = true) {
        if (flush) flush_pending();
        if (editor_stack_) {
            QList<int> sizes = splitter_->sizes();
            if (sizes.size() == 2 && sizes[1] > 300) detail_width_ = sizes[1];
            editor_stack_->hide();
        }
        detail_visible_ = false;
        editing_ = false;
    }

    void select_event(const std::string& id, Qt::KeyboardModifiers mods = Qt::NoModifier) {
        QString qid = qs(id);
        flush_pending();
        if ((mods & Qt::ShiftModifier) && !anchor_id_.isEmpty()) {
            auto events = visible_events();
            int a = -1, b = -1;
            for (int i = 0; i < (int)events.size(); ++i) {
                if (qs(events[i]->id) == anchor_id_) a = i;
                if (qs(events[i]->id) == qid) b = i;
            }
            if (a >= 0 && b >= 0) {
                if (!(mods & Qt::ControlModifier)) selection_.clear();
                for (int i = std::min(a, b); i <= std::max(a, b); ++i) {
                    selection_.insert(qs(events[i]->id));
                }
            } else {
                selection_.clear();
                selection_.insert(qid);
                anchor_id_ = qid;
            }
        } else if (mods & Qt::ControlModifier) {
            if (selection_.contains(qid)) {
                selection_.remove(qid);
            } else {
                selection_.insert(qid);
            }
            if (selection_.isEmpty()) selection_.insert(qid);
            anchor_id_ = qid;
        } else {
            selection_.clear();
            selection_.insert(qid);
            anchor_id_ = qid;
        }
        if (!selection_.contains(qid)) {
            qid = *selection_.begin();
        }
        current_id_ = qid;
        if (mods == Qt::NoModifier) {
            load_view(repo_.find(qid.toStdString()));
        } else {
            // Ctrl / Shift 只做多选，不自动展开右侧页面
            close_detail(false);
        }
        apply_card_selection();
        update_batch_buttons();
    }

    void apply_card_selection() {
        for (auto it = cards_.begin(); it != cards_.end(); ++it) {
            it.value()->set_selected(selection_.contains(it.key()));
        }
    }

    void update_batch_buttons() {
        const bool multi = selection_.size() > 1;
        if (batch_done_button_) {
            batch_done_button_->setVisible(multi);
            batch_done_button_->setText("完成选中 (" + QString::number(selection_.size()) + ")");
        }
        if (batch_delete_button_) {
            batch_delete_button_->setVisible(multi);
            batch_delete_button_->setText("删除选中 (" + QString::number(selection_.size()) + ")");
        }
        if (count_label_) {
            QString base = count_label_->text().section(" · 已选", 0, 0);
            if (multi) base += " · 已选 " + QString::number(selection_.size());
            count_label_->setText(base);
        }
    }

    void batch_done() {
        if (selection_.size() < 2) return;
        flush_pending();
        int count = 0;
        for (const QString& id : selection_) {
            try {
                repo_.set_done(id.toStdString(), true);
                ++count;
            } catch (...) {
            }
        }
        refresh_list(false);
        apply_card_selection();
        if (toast_) toast_->show_message("批量完成", QString::number(count) + " 条", "✓", 2500);
    }

    void batch_delete() {
        if (selection_.size() < 2) return;
        auto answer = QMessageBox::question(
            this, "删除选中记事",
            "确定删除选中的 " + QString::number(selection_.size()) + " 条记事？",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
        flush_pending();
        QStringList ids(selection_.begin(), selection_.end());
        for (const QString& id : ids) repo_.remove(id.toStdString());
        selection_.clear();
        current_id_.clear();
        refresh_list(true);
        apply_card_selection();
        update_batch_buttons();
        if (toast_) toast_->show_message("已删除", QString::number(ids.size()) + " 条", "🗑", 2500);
    }

    void schedule_save() {
        if (loading_) return;
        save_timer_->start();
    }

    void flush_pending() {
        if (save_timer_ && save_timer_->isActive()) {
            save_timer_->stop();
            save_current();
        }
    }

    bool save_current(bool show_toast = false) {
        if (loading_ || current_id_.isEmpty()) return true;
        Event* e = repo_.find(current_id_.toStdString());
        if (!e) return true;
        QString title = title_edit_->text().trimmed();
        if (title.isEmpty()) {
            status_label_->setText("标题不能为空");
            return false;
        }
        std::optional<std::time_t> due;
        if (due_check_->isChecked()) {
            due = (std::time_t)due_edit_->dateTime().toSecsSinceEpoch();
        }
        QString tags_text = tags_edit_->text();
        tags_text.replace("，", ",");
        std::vector<std::string> tags;
        for (const QString& part : tags_text.split(',', Qt::SkipEmptyParts)) {
            QString t = part.trimmed();
            while (t.startsWith('#')) t.remove(0, 1);
            if (!t.isEmpty() && std::find(tags.begin(), tags.end(), t.toStdString()) == tags.end()) {
                tags.push_back(t.toStdString());
            }
        }
        try {
            repo_.update(current_id_.toStdString(), title.toStdString(),
                         note_edit_->toPlainText().toStdString(), due, tags,
                         remind_spin_->value(), current_color_.toStdString(),
                         current_icon_.toStdString(), std::nullopt,
                         system_notify_check_ && system_notify_check_->isChecked());
        } catch (const std::exception& ex) {
            status_label_->setText(QString("保存失败：") + ex.what());
            return false;
        }
        dirty_ = false;
        status_label_->setText("已保存 · " + clock_text(now_time()));
        Event* updated = repo_.find(current_id_.toStdString());
        if (updated) update_meta(updated);
        refresh_list(false);
        if (show_toast && toast_) {
            toast_->show_message("已保存", qs(title.toStdString()), "✓", 2200);
        }
        return true;
    }

    void update_color_button() {
        if (!color_button_) return;
        QString color = current_color_.isEmpty() ? pal_.primary : current_color_;
        color_button_->setStyleSheet(
            QString("background:%1;border:1px solid %2;border-radius:12px;")
                .arg(color, pal_.border));
    }

    void choose_color() {
        QColor initial(current_color_.isEmpty() ? pal_.primary : current_color_);
        QColor chosen = QColorDialog::getColor(initial, this, "事项颜色");
        if (!chosen.isValid()) return;
        current_color_ = chosen.name();
        update_color_button();
        save_current(true);
    }

    void move_to_priority(int value) {
        if (loading_ || current_id_.isEmpty()) return;
        Event* e = current();
        if (!e || value == e->priority) return;
        flush_pending();
        try {
            repo_.move_to(current_id_.toStdString(), value);
            refresh_list(false);
            if (Event* moved = repo_.find(current_id_.toStdString())) {
                load_editor(moved);
                status_label_->setText("优先级 → P" + QString::number(moved->priority));
            }
        } catch (const std::exception& ex) {
            status_label_->setText(QString("调整失败：") + ex.what());
        }
    }

    void update_preview() {
        if (!preview_) return;
        preview_->setHtml(markdown_to_html(note_edit_->toPlainText(), theme_ == "dark"));
    }

    void wrap_note(const QString& prefix, const QString& suffix) {
        if (!note_edit_) return;
        QTextCursor cursor = note_edit_->textCursor();
        QString selected = cursor.selectedText();
        selected.replace(QChar::ParagraphSeparator, '\n');
        if (selected.isEmpty()) selected = "文本";
        cursor.insertText(prefix + selected + suffix);
        note_edit_->setTextCursor(cursor);
        schedule_save();
        if (markdown_timer_) markdown_timer_->start();
    }

    void wrap_note_color() {
        QColor chosen = QColorDialog::getColor(QColor("#e11d48"), this, "文字颜色");
        if (!chosen.isValid()) return;
        wrap_note("{color:" + chosen.name() + "}", "{/color}");
    }

    void play_alert() {
        if (!ringtone_.isEmpty() && QFileInfo::exists(ringtone_) &&
            play_ringtone(ringtone_.toStdString())) {
            return;
        }
        QApplication::beep();
    }

    void choose_ringtone() {
        QString path = QFileDialog::getOpenFileName(
            this, "选择提醒铃声（WAV）", ringtone_,
            "WAV 音频 (*.wav);;所有文件 (*.*)");
        if (path.isEmpty()) return;
        ringtone_ = path;
        json cfg = repo_.store.load_config();
        cfg["ringtone"] = ringtone_.toStdString();
        repo_.store.save_config(cfg);
        if (!play_ringtone(ringtone_.toStdString())) {
            toast_->show_message("铃声播放失败", "当前仅支持 WAV；将回退为系统蜂鸣", "🔔", 5000);
        } else {
            toast_->show_message("已设置铃声", QFileInfo(path).fileName(), "🔔", 3000);
        }
    }

    void choose_accent() {
        QColor chosen = QColorDialog::getColor(accent_, this, "主题色");
        if (!chosen.isValid()) return;
        accent_ = chosen;
        json cfg = repo_.store.load_config();
        cfg["accent"] = accent_.name().toStdString();
        repo_.store.save_config(cfg);
        apply_theme(theme_, false);
        if (toast_) toast_->show_message("主题色已更新", accent_.name(), "🎨", 2000);
    }

    Event* current() {
        return current_id_.isEmpty() ? nullptr : repo_.find(current_id_.toStdString());
    }

    // ---------- 操作 ----------
    void new_event() {
        flush_pending();
        mode_ = "all";
        tag_filter_.clear();
        search_edit_->blockSignals(true);
        search_edit_->clear();
        search_edit_->blockSignals(false);
        nav_buttons_["all"]->setChecked(true);
        try {
            Event& e = repo_.add("新记事");
            current_id_ = qs(e.id);
            selection_.clear();
            selection_.insert(current_id_);
            anchor_id_ = current_id_;
            refresh_list(false);
            load_editor(&e);
            title_edit_->setFocus();
            title_edit_->selectAll();
            status_label_->setText("已新建记事");
        } catch (const std::exception& ex) {
            status_label_->setText(QString("新建失败：") + ex.what());
        }
    }

    void toggle_current() {
        if (current_id_.isEmpty()) return;
        toggle_event(current_id_.toStdString());
    }

    void toggle_event(const std::string& id) {
        flush_pending();
        try {
            Event& e = repo_.toggle_done(id);
            if (id == current_id_.toStdString() && detail_visible_) load_event(&e, editing_);
            refresh_list(true);
            if (toast_) toast_->show_message(e.done ? "已完成" : "恢复待办", qs(e.title), e.done ? "✓" : "↺", 2200);
        } catch (const std::exception& ex) {
            status_label_->setText(QString("操作失败：") + ex.what());
        }
    }

    void duplicate_current() {
        if (current_id_.isEmpty()) return;
        flush_pending();
        try {
            Event& e = repo_.duplicate(current_id_.toStdString());
            current_id_ = qs(e.id);
            refresh_list(false);
            load_editor(&e);
            status_label_->setText("已复制");
        } catch (const std::exception& ex) {
            status_label_->setText(QString("复制失败：") + ex.what());
        }
    }

    void move_current(int delta) {
        if (current_id_.isEmpty()) return;
        flush_pending();
        try {
            repo_.move_by(current_id_.toStdString(), delta);
            Event* e = repo_.find(current_id_.toStdString());
            if (e) {
                load_editor(e);
                status_label_->setText("优先级 → P" + QString::number(e->priority));
            }
            refresh_list(false);
        } catch (const std::exception& ex) {
            status_label_->setText(QString("调整失败：") + ex.what());
        }
    }

    void delete_current() {
        if (current_id_.isEmpty()) return;
        Event* e = current();
        if (!e) return;
        auto answer = QMessageBox::question(this, "删除记事",
                                            "确定删除「" + qs(e->title) + "」吗？\n\n删除后仍可撤销。",
                                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
        flush_pending();
        auto events = visible_events();
        std::string id = current_id_.toStdString();
        int index = -1;
        for (size_t i = 0; i < events.size(); ++i) {
            if (events[i]->id == id) index = (int)i;
        }
        std::string title = e->title;
        repo_.remove(id);
        auto after = visible_events();
        if (after.empty()) {
            current_id_.clear();
            load_editor(nullptr);
        } else {
            int next = index >= 0 ? std::min(index, (int)after.size() - 1) : 0;
            current_id_ = qs(after[(size_t)next]->id);
            load_editor(after[(size_t)next]);
        }
        refresh_list(false);
        if (toast_) toast_->show_message("已删除", qs(title), "🗑", 2600);
    }

    void undo_action(bool redo) {
        flush_pending();
        bool ok = redo ? repo_.redo() : repo_.undo();
        if (!repo_.find(current_id_.toStdString()) && !repo_.events.empty()) {
            current_id_ = qs((&*repo_.events.begin())->id);
        }
        refresh_list(true);
        if (!current_id_.isEmpty()) load_editor(current());
        status_label_->setText(redo ? (ok ? "已重做" : "没有可重做的操作")
                                    : (ok ? "已撤销" : "没有可撤销的操作"));
    }

    void insert_table() {
        if (current_id_.isEmpty()) return;
        note_edit_->insertPlainText("\n| 项目 | 内容 |\n| --- | --- |\n|  |  |\n");
        schedule_save();
    }

    // ---------- 附件 ----------
    void attach_files(const QStringList& files) {
        if (current_id_.isEmpty()) return;
        flush_pending();
        int added = 0;
        for (const QString& file : files) {
            try {
                repo_.add_attachment(current_id_.toStdString(), fspath(file));
                ++added;
            } catch (const std::exception& ex) {
                status_label_->setText(QString("附件失败：") + ex.what());
            }
        }
        if (added) {
            reload_attachments();
            refresh_list(false);
            if (page_ && current()) page_->refresh_images(current());
            status_label_->setText("已添加 " + QString::number(added) + " 个附件");
        }
    }

    void attach_image(const QImage& image) {
        if (current_id_.isEmpty() || image.isNull()) return;
        fs::path tmp = repo_.store.dir / "_clipboard.png";
        if (!image.save(qpath(tmp), "PNG")) {
            status_label_->setText("图片保存失败");
            return;
        }
        try {
            repo_.add_attachment(current_id_.toStdString(), tmp);
            reload_attachments();
            refresh_list(false);
            if (page_ && current()) page_->refresh_images(current());
            status_label_->setText("图片已存为附件");
        } catch (const std::exception& ex) {
            status_label_->setText(QString("粘贴失败：") + ex.what());
        }
        std::error_code ec;
        fs::remove(tmp, ec);
    }

    void choose_attachments() {
        if (current_id_.isEmpty()) return;
        QStringList files = QFileDialog::getOpenFileNames(
            this, "添加附件", QString(),
            "所有文件 (*.*);;图片 (*.png *.jpg *.jpeg *.gif *.bmp);;音频 (*.mp3 *.wav *.ogg *.m4a)");
        if (!files.isEmpty()) attach_files(files);
    }

    void reload_attachments() {
        if (!att_list_) return;
        att_list_->clear();
        Event* e = current();
        if (!e) return;
        QFileIconProvider provider;
        for (const auto& att : e->attachments) {
            fs::path absolute;
            try {
                absolute = repo_.store.resolve(att);
            } catch (...) {
            }
            QIcon icon;
            QPixmap px(qpath(absolute));
            if (!px.isNull()) {
                icon = QIcon(px.scaled(QSize(76, 56), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                icon = provider.icon(QFileInfo(qpath(absolute)));
            }
            auto* item = new QListWidgetItem(icon, qs(att.name), att_list_);
            item->setData(Qt::UserRole, qs(att.path));
            item->setToolTip(qpath(absolute));
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        }
    }

    void open_attachment(QListWidgetItem* item) {
        if (!item) return;
        try {
            fs::path path = repo_.store.resolve(item->data(Qt::UserRole).toString().toStdString());
            if (!fs::exists(path)) {
                status_label_->setText("附件文件丢失");
                return;
            }
            QDesktopServices::openUrl(QUrl::fromLocalFile(qpath(path)));
        } catch (const std::exception& ex) {
            status_label_->setText(QString("无法打开：") + ex.what());
        }
    }

    void remove_attachment(int index) {
        if (current_id_.isEmpty()) return;
        try {
            repo_.remove_attachment(current_id_.toStdString(), index);
            reload_attachments();
            refresh_list(false);
            if (page_ && current()) page_->refresh_images(current());
            status_label_->setText("已移除附件");
        } catch (const std::exception& ex) {
            status_label_->setText(QString("移除失败：") + ex.what());
        }
    }

    void reorder_attachments(const QStringList& paths) {
        if (current_id_.isEmpty()) return;
        std::vector<std::string> rel;
        for (const QString& p : paths) rel.push_back(p.toStdString());
        try {
            repo_.reorder_attachments(current_id_.toStdString(), rel);
            status_label_->setText("附件顺序已保存");
        } catch (const std::exception& ex) {
            status_label_->setText(QString("排序失败：") + ex.what());
        }
    }

    void open_sketch() {
        if (current_id_.isEmpty()) return;
        SketchDialog dlg(this);
        if (dlg.exec() != QDialog::Accepted) return;
        QImage image = dlg.image();
        fs::path tmp = repo_.store.dir / "_sketch.png";
        if (!image.save(qpath(tmp), "PNG")) {
            status_label_->setText("笔迹保存失败");
            return;
        }
        try {
            repo_.add_attachment(current_id_.toStdString(), tmp);
            reload_attachments();
            refresh_list(false);
            status_label_->setText("笔迹已保存为附件");
        } catch (const std::exception& ex) {
            status_label_->setText(QString("保存笔迹失败：") + ex.what());
        }
        std::error_code ec;
        fs::remove(tmp, ec);
    }

    // ---------- 导入导出 ----------
    void export_data() {
        flush_pending();
        QString filters = "墨记打包 (*.mjnote);;JSON (*.json);;todo.txt (*.txt);;Markdown (*.md)";
        QString path = QFileDialog::getSaveFileName(this, "导出记事", "moji-export.mjnote", filters);
        if (path.isEmpty()) return;
        try {
            export_auto(repo_, fspath(path));
            status_label_->setText("已导出到 " + path);
            if (toast_) toast_->show_message("导出完成", path, "⬇", 4000);
        } catch (const std::exception& ex) {
            QMessageBox::critical(this, "导出失败", ex.what());
        }
    }

    void import_data() {
        flush_pending();
        QString path = QFileDialog::getOpenFileName(this, "导入记事", QString(),
                                                     "墨记数据 (*.mjnote *.json *.txt);;所有文件 (*.*)");
        if (path.isEmpty()) return;
        auto answer = QMessageBox::question(this, "导入记事",
                                             "合并导入？\n\n选择「No」会先清空现有数据。",
                                             QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                             QMessageBox::Yes);
        if (answer == QMessageBox::Cancel) return;
        try {
            ImportReport report = import_auto(repo_, fspath(path), answer == QMessageBox::Yes);
            current_id_.clear();
            refresh_list(true);
            if (toast_) {
                toast_->show_message("导入完成",
                                     "新增 " + QString::number(report.added) + " 条，跳过 " +
                                         QString::number(report.skipped) + " 条",
                                     "⬆", 4000);
            }
        } catch (const std::exception& ex) {
            QMessageBox::critical(this, "导入失败", ex.what());
        }
    }

    // ---------- 主题 / 提醒 ----------
    void apply_theme(const QString& name, bool save) {
        theme_ = name == "dark" ? "dark" : "light";
        pal_ = theme_ == "dark" ? make_dark(accent_) : make_light(accent_);
        qApp->setStyleSheet(build_qss(pal_));
        if (light_action_) light_action_->setChecked(theme_ == "light");
        if (dark_action_) dark_action_->setChecked(theme_ == "dark");
        if (theme_button_) theme_button_->setText(theme_ == "dark" ? "浅色主题" : "深色主题");
        refresh_list(false);
        if (detail_visible_) {
            if (Event* e = current()) load_event(e, editing_);
        }
        update_color_button();
        if (preview_) update_preview();
        if (page_ && current()) {
            page_->set_event(current());
            page_->set_note(note_edit_->toPlainText());
        }
        if (view_page_) view_page_->set_dark(theme_ == "dark");
        if (save) {
            json cfg = repo_.store.load_config();
            cfg["theme"] = theme_.toStdString();
            repo_.store.save_config(cfg);
            if (toast_) toast_->show_message("已切换主题", theme_ == "dark" ? "深色模式" : "浅色模式", "🎨", 1800);
        }
    }

    void greet_today() {
        int pending = 0;
        for (Event* e : repo_.due_on(now_time())) {
            if (!e->done) ++pending;
        }
        if (pending > 0) {
            play_alert();
            if (toast_) toast_->show_message("今日安排", "今天有 " + QString::number(pending) + " 项待办", "📅", 6000);
        }
    }

    void check_reminders() {
        auto due = repo_.due_reminders(now_time());
        if (due.empty()) return;
        play_alert();
        QString text;
        for (size_t i = 0; i < due.size() && i < 5; ++i) {
            if (i) text += "\n";
            text += "• " + qs(due[i]->title) + "（" + qs(humanize_due(due[i]->due)) + "）";
        }
        if (due.size() > 5) text += "\n……以及另外 " + QString::number(due.size() - 5) + " 项";
        if (toast_) toast_->show_message("⏰ 待办提醒", text, "⏰", 12000);
        int sent = 0;
        for (Event* e : due) {
            if (!e->system_notify || sent >= 5) continue;
            show_system_notification("墨记提醒 · " + qs(e->title),
                                     "截止 " + qs(humanize_due(e->due)), 12000);
            ++sent;
        }
        repo_.mark_reminded(due, now_time());
    }

    void show_help() {
        QMessageBox::information(
            this, "快捷键",
            "<b>常用快捷键</b><br><br>"
            "Ctrl+N　新建记事<br>"
            "Ctrl+S　保存<br>"
            "Ctrl+F　搜索<br>"
            "Ctrl+O　导入　Ctrl+Shift+S　导出<br>"
            "Ctrl+Alt+Z / Ctrl+Alt+Y　撤销 / 重做<br>"
            "Ctrl+Return　标记完成<br>"
            "F5　刷新　F1　本帮助<br><br>"
            "点击卡片先看页面，右上角「✎ 编辑」进入修改；<br>"
            "Ctrl/Shift 多选，页面右上角 ✕ 关闭分屏。<br><br>"
            "附件：拖入窗口或备注框；双击缩略图打开；右键移除；<br>"
            "备注 Ctrl+V 可直接粘贴剪贴板图片。");
    }

    void show_about() {
        QMessageBox::about(
            this, "关于墨记",
            "<h3>墨记 MoJi v1.0.0</h3>"
            "<p>课程设计《电子记事本的实现》C++17 / Qt Widgets 版本。</p>"
            "<p>优先级单向链表、UTF-8 文本存储、TUI + Qt GUI、附件与手绘、四种导入导出。</p>" +
                QString("<p style='color:#888'>数据目录：%1</p>").arg(qpath(repo_.store.dir)));
    }

    Repository& repo_;
    json cfg_;
    QString theme_ = "light";
    QColor accent_ = QColor("#4f6ef7");
    QString ringtone_;
    QString current_color_;
    QString current_icon_;
    Pal pal_;
    bool shot_mode_ = false;
    bool loading_ = false;
    bool dirty_ = false;
    bool detail_visible_ = false;
    bool editing_ = false;
    int detail_width_ = 680;

    QString mode_ = "all";
    QString tag_filter_;
    QString current_id_;
    QString anchor_id_;
    QSet<QString> selection_;
    QHash<QString, EventCard*> cards_;
    QHash<QString, QPushButton*> nav_buttons_;
    QHash<QString, QString> nav_titles_;

    QSplitter* splitter_ = nullptr;
    QScrollArea* cards_scroll_ = nullptr;
    QWidget* cards_container_ = nullptr;
    QVBoxLayout* cards_layout_ = nullptr;
    QLabel* count_label_ = nullptr;
    QPushButton* batch_done_button_ = nullptr;
    QPushButton* batch_delete_button_ = nullptr;
    QLineEdit* search_edit_ = nullptr;
    QStackedWidget* editor_stack_ = nullptr;
    QFrame* view_panel_ = nullptr;
    QLabel* view_title_ = nullptr;
    QLabel* view_meta_ = nullptr;
    PageEditor* view_page_ = nullptr;
    QWidget* view_att_panel_ = nullptr;
    QListWidget* view_att_list_ = nullptr;
    QLineEdit* title_edit_ = nullptr;
    QLabel* meta_label_ = nullptr;
    QCheckBox* system_notify_check_ = nullptr;
    QCheckBox* due_check_ = nullptr;
    QDateTimeEdit* due_edit_ = nullptr;
    QSpinBox* remind_spin_ = nullptr;
    QLineEdit* tags_edit_ = nullptr;
    QPushButton* done_button_ = nullptr;
    NoteEdit* note_edit_ = nullptr;
    QTabWidget* note_tabs_ = nullptr;
    QTextBrowser* preview_ = nullptr;
    PageEditor* page_ = nullptr;
    QTimer* markdown_timer_ = nullptr;
    QSpinBox* priority_spin_ = nullptr;
    QPushButton* color_button_ = nullptr;
    QLineEdit* icon_edit_ = nullptr;
    AttachmentList* att_list_ = nullptr;
    QWidget* att_panel_ = nullptr;
    QLabel* status_label_ = nullptr;
    StatCard* stat_today_ = nullptr;
    StatCard* stat_pending_ = nullptr;
    StatCard* stat_overdue_ = nullptr;
    StatCard* stat_rate_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QScrollArea* tags_scroll_ = nullptr;
    QWidget* tags_container_ = nullptr;
    QVBoxLayout* tags_layout_ = nullptr;
    QButtonGroup* nav_group_ = nullptr;
    QPushButton* theme_button_ = nullptr;
    QAction* light_action_ = nullptr;
    QAction* dark_action_ = nullptr;
    Toast* toast_ = nullptr;
    QSystemTrayIcon* tray_ = nullptr;
    QTimer* save_timer_ = nullptr;
    QTimer* search_timer_ = nullptr;
    QTimer* remind_timer_ = nullptr;
};

}  // namespace

int run_qt_gui(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("墨记 MoJi");
    app.setApplicationDisplayName("墨记 MoJi");
    app.setWindowIcon(QIcon(":/assets/app.png"));
    app.setStyle("Fusion");
    QFont font("Microsoft YaHei UI", 10);
    app.setFont(font);

    QString data_dir;
    QString screenshot;
    const QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--data-dir" && i + 1 < args.size()) {
            data_dir = args[++i];
        } else if (args[i] == "--screenshot" && i + 1 < args.size()) {
            screenshot = args[++i];
        }
    }

    try {
        fs::path dir;
        if (!data_dir.isEmpty()) dir = fspath(data_dir);
        Repository repo(dir);
        MainWindow window(repo, !screenshot.isEmpty());
        window.load_first();
        const QString mode = qEnvironmentVariable("MOJI_GUI_MODE");
        if (!mode.isEmpty()) window.debug_set_mode(mode);
        const QString open = qEnvironmentVariable("MOJI_GUI_OPEN");
        if (!open.isEmpty()) window.debug_open_first(open == "edit");
        if (qEnvironmentVariableIsSet("MOJI_GUI_CLICK")) window.debug_click_first();
        const int multi = qEnvironmentVariableIntValue("MOJI_GUI_MULTI");
        if (multi > 1) window.select_test_multiple(multi, qEnvironmentVariableIsSet("MOJI_GUI_SHIFT"));
        const QString tab = qEnvironmentVariable("MOJI_GUI_TAB");
        if (!tab.isEmpty()) {
            if (auto* tabs = window.findChild<QTabWidget*>("NoteTabs")) {
                tabs->setCurrentIndex(tab == "preview" ? 1 : (tab == "page" ? 2 : 0));
            }
        }
        if (!screenshot.isEmpty()) {
            window.setAttribute(Qt::WA_DontShowOnScreen, true);
        }
        window.show();
        if (!screenshot.isEmpty()) {
            QTimer::singleShot(1400, &app, [&window, screenshot, &app] {
                window.grab().save(screenshot, "PNG");
                app.quit();
            });
        }
        return app.exec();
    } catch (const std::exception& ex) {
        QMessageBox::critical(nullptr, "墨记 MoJi",
                              QString("启动失败：") + QString::fromUtf8(ex.what()));
        return 1;
    }
}

}  // namespace moji
