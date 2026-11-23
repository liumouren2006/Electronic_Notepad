#include "page_view.hpp"

#include <QColorDialog>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QtAlgorithms>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QScrollBar>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/model.hpp"
#include "core/repository.hpp"
#include "core/util.hpp"
#include "markdown.hpp"

namespace moji {

namespace {

constexpr qreal kPageWidth = 820.0;
constexpr qreal kPageMinHeight = 1200.0;
constexpr qreal kTextWidth = 680.0;
constexpr qreal kInkZ = 1000.0;

struct InkStroke {
    QColor color = QColor("#1f2430");
    qreal width = 4.0;
    QVector<QPointF> points;
};

class InkLayer : public QGraphicsItem {
public:
    InkLayer() { setZValue(kInkZ); setAcceptedMouseButtons(Qt::NoButton); }

    QRectF boundingRect() const override {
        if (strokes_.isEmpty()) return QRectF(0, 0, 1, 1);
        qreal minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9;
        for (const auto& s : strokes_) {
            for (const auto& p : s.points) {
                minx = std::min(minx, p.x());
                miny = std::min(miny, p.y());
                maxx = std::max(maxx, p.x());
                maxy = std::max(maxy, p.y());
            }
        }
        return QRectF(minx - 12, miny - 12, maxx - minx + 24, maxy - miny + 24);
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->setRenderHint(QPainter::Antialiasing);
        for (const auto& s : strokes_) {
            QPen pen(s.color, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter->setPen(pen);
            if (s.points.size() == 1) {
                painter->drawPoint(s.points.front());
            } else if (s.points.size() > 1) {
                painter->drawPolyline(s.points.constData(), s.points.size());
            }
        }
    }

    void begin_stroke(const QPointF& p, const QColor& color, qreal width) {
        InkStroke s;
        s.color = color;
        s.width = width;
        s.points << p;
        strokes_.append(s);
        prepareGeometryChange();
        update();
    }
    void add_point(const QPointF& p) {
        if (strokes_.isEmpty()) return;
        prepareGeometryChange();
        strokes_.last().points << p;
        update();
    }
    void end_stroke() { update(); }
    void clear() {
        prepareGeometryChange();
        strokes_.clear();
        update();
    }
    void undo() {
        if (strokes_.isEmpty()) return;
        prepareGeometryChange();
        strokes_.removeLast();
        update();
    }
    bool erase_at(const QPointF& p, qreal radius = 14.0) {
        for (int i = strokes_.size() - 1; i >= 0; --i) {
            for (const auto& pt : strokes_[i].points) {
                if (QLineF(pt, p).length() <= radius) {
                    prepareGeometryChange();
                    strokes_.removeAt(i);
                    update();
                    return true;
                }
            }
        }
        return false;
    }

    nlohmann::json to_json() const {
        auto arr = nlohmann::json::array();
        for (const auto& s : strokes_) {
            auto pts = nlohmann::json::array();
            for (const auto& p : s.points) pts.push_back({p.x(), p.y()});
            arr.push_back({{"color", s.color.name().toStdString()},
                           {"width", s.width},
                           {"points", pts}});
        }
        return arr;
    }
    void from_json(const nlohmann::json& arr) {
        prepareGeometryChange();
        strokes_.clear();
        if (!arr.is_array()) return;
        for (const auto& item : arr) {
            if (!item.is_object()) continue;
            InkStroke s;
            s.color = QColor(QString::fromStdString(item.value("color", std::string("#1f2430"))));
            if (!s.color.isValid()) s.color = QColor("#1f2430");
            s.width = item.value("width", 4.0);
            if (item.contains("points") && item["points"].is_array()) {
                for (const auto& p : item["points"]) {
                    if (p.is_array() && p.size() >= 2) {
                        s.points << QPointF(p[0].get<double>(), p[1].get<double>());
                    }
                }
            }
            if (!s.points.isEmpty()) strokes_.append(s);
        }
        update();
    }

private:
    QVector<InkStroke> strokes_;
};

class PageTextItem : public QGraphicsTextItem {
public:
    explicit PageTextItem(QGraphicsItem* parent = nullptr) : QGraphicsTextItem(parent) {
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        setTextInteractionFlags(Qt::NoTextInteraction);
    }
    std::function<void()> changed;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override {
        if (change == ItemPositionHasChanged && changed) changed();
        return QGraphicsTextItem::itemChange(change, value);
    }
};

class PageImageItem : public QGraphicsItem {
public:
    explicit PageImageItem(const QPixmap& pixmap, const QString& path)
        : pixmap_(pixmap), path_(path) {
        size_ = pixmap.size();
        if (size_.width() > 280 || size_.height() > 220) {
            size_.scale(QSizeF(280, 220), Qt::KeepAspectRatio);
        }
        if (size_.width() < 40 || size_.height() < 40) size_ = QSizeF(160, 100);
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setAcceptHoverEvents(true);
        setTransformOriginPoint(QPointF(size_.width() / 2, size_.height() / 2));
    }

    QRectF boundingRect() const override {
        return QRectF(-12, -18, size_.width() + 24, size_.height() + 30);
    }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        QRectF target(0, 0, size_.width(), size_.height());
        painter->drawPixmap(target, pixmap_, pixmap_.rect());
        if (isSelected()) {
            QPen border(QColor("#4f6ef7"), 1.4, Qt::DashLine);
            painter->setPen(border);
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(target);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor("#4f6ef7"));
            painter->drawRect(QRectF(size_.width() - 9, size_.height() - 9, 10, 10));
            painter->setBrush(QColor("#ffffff"));
            painter->drawEllipse(QPointF(size_.width() / 2, -13), 6, 6);
        }
    }

    QSizeF content_size() const { return size_; }
    QString path() const { return path_; }
    void set_content_size(const QSizeF& size) {
        prepareGeometryChange();
        size_ = size;
        setTransformOriginPoint(QPointF(size_.width() / 2, size_.height() / 2));
        update();
    }
    std::function<void()> changed;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override {
        if (change == ItemPositionHasChanged && changed) changed();
        return QGraphicsItem::itemChange(change, value);
    }
    void mousePressEvent(QGraphicsSceneMouseEvent* e) override {
        if (isSelected()) {
            QPointF p = e->pos();
            if (QRectF(size_.width() - 14, size_.height() - 14, 20, 20).contains(p)) {
                mode_ = Mode::Resize;
                e->accept();
                return;
            }
            if (QLineF(p, QPointF(size_.width() / 2, -13)).length() <= 14) {
                mode_ = Mode::Rotate;
                start_rotation_ = rotation();
                QPointF scene = e->scenePos();
                center_scene_ = mapToScene(QPointF(size_.width() / 2, size_.height() / 2));
                start_angle_ = std::atan2(scene.y() - center_scene_.y(), scene.x() - center_scene_.x());
                e->accept();
                return;
            }
        }
        mode_ = Mode::Move;
        QGraphicsItem::mousePressEvent(e);
    }
    void mouseMoveEvent(QGraphicsSceneMouseEvent* e) override {
        if (mode_ == Mode::Resize) {
            QSizeF new_size(std::max<qreal>(30, e->pos().x()),
                            std::max<qreal>(30, e->pos().y()));
            set_content_size(new_size);
            e->accept();
            return;
        }
        if (mode_ == Mode::Rotate) {
            QPointF scene = e->scenePos();
            qreal angle = std::atan2(scene.y() - center_scene_.y(), scene.x() - center_scene_.x());
            setRotation(start_rotation_ + (angle - start_angle_) * 180.0 / M_PI);
            e->accept();
            return;
        }
        QGraphicsItem::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override {
        if (mode_ != Mode::Move) {
            mode_ = Mode::None;
            if (changed) changed();
        }
        QGraphicsItem::mouseReleaseEvent(e);
    }

private:
    enum class Mode { None, Move, Resize, Rotate };
    QPixmap pixmap_;
    QString path_;
    QSizeF size_;
    Mode mode_ = Mode::None;
    qreal start_rotation_ = 0;
    QPointF center_scene_;
    qreal start_angle_ = 0;
};

class PageView : public QGraphicsView {
public:
    explicit PageView(QGraphicsScene* scene, QWidget* parent = nullptr)
        : QGraphicsView(scene, parent) {
        setRenderHint(QPainter::Antialiasing);
        setRenderHint(QPainter::SmoothPixmapTransform);
        setDragMode(QGraphicsView::NoDrag);
        setAlignment(Qt::AlignLeft | Qt::AlignTop);
        setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
        setBackgroundBrush(QColor("#f4f6fb"));
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }

    std::function<bool(QPointF, Qt::KeyboardModifiers)> on_press;
    std::function<bool(QPointF, Qt::KeyboardModifiers)> on_move;
    std::function<void()> on_release;

protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (on_press && on_press(mapToScene(e->pos()), e->modifiers())) {
            e->accept();
            return;
        }
        QGraphicsView::mousePressEvent(e);
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (on_move && on_move(mapToScene(e->pos()), e->modifiers())) {
            e->accept();
            return;
        }
        QGraphicsView::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (on_release) on_release();
        QGraphicsView::mouseReleaseEvent(e);
    }
};

}  // namespace

class PageEditor::Impl {
public:
    Impl(PageEditor* owner, Repository& repo) : owner_(owner), repo_(repo) {
        scene_ = new QGraphicsScene(owner);
        scene_->setBackgroundBrush(QColor("#ffffff"));
        view_ = new PageView(scene_, owner);
        auto* layout = new QVBoxLayout(owner);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);

        auto* toolbar = new QHBoxLayout();
        toolbar->setSpacing(6);
        ink_button_ = new QPushButton("墨迹", owner);
        ink_button_->setCheckable(true);
        ink_button_->setToolTip("打开后可在页面上直接书写");
        auto* eraser = new QPushButton("橡皮", owner);
        eraser->setCheckable(true);
        eraser->setToolTip("点击/拖动擦除墨迹");
        pen_button_ = new QPushButton(owner);
        pen_button_->setFixedSize(24, 24);
        pen_button_->setToolTip("墨迹颜色");
        width_spin_ = new QSpinBox(owner);
        width_spin_->setRange(1, 20);
        width_spin_->setValue(4);
        width_spin_->setFixedWidth(60);
        auto* undo_ink = new QPushButton("撤销", owner);
        auto* clear_ink = new QPushButton("清空", owner);
        auto* add_image = new QPushButton("加图", owner);
        auto* front = new QPushButton("置顶", owner);
        auto* back = new QPushButton("置底", owner);
        auto* remove_image = new QPushButton("删图", owner);
        for (QPushButton* b : {ink_button_, eraser, undo_ink, clear_ink, add_image, front, back,
                               remove_image}) {
            b->setObjectName("Ghost");
            b->setStyleSheet("font-size:9pt;padding:2px 6px;");
            b->setCursor(Qt::PointingHandCursor);
        }
        toolbar->addWidget(ink_button_);
        toolbar->addWidget(eraser);
        toolbar->addWidget(pen_button_);
        toolbar->addWidget(width_spin_);
        toolbar->addWidget(undo_ink);
        toolbar->addWidget(clear_ink);
        toolbar->addStretch(1);
        toolbar->addWidget(add_image);
        toolbar->addWidget(front);
        toolbar->addWidget(back);
        toolbar->addWidget(remove_image);
        layout->addLayout(toolbar);
        view_->setMinimumHeight(220);
        layout->addWidget(view_, 1);

        set_pen_color(QColor("#1f2430"));
        connect(ink_button_, &QPushButton::toggled, owner, [this](bool on) {
            set_ink_mode(on);
        });
        connect(eraser, &QPushButton::toggled, owner, [this, eraser](bool on) {
            eraser_ = on;
            if (on) {
                ink_button_->setChecked(true);
                set_ink_mode(true);
            }
            view_->setCursor(on ? Qt::CrossCursor : (ink_mode_ ? Qt::CrossCursor : Qt::ArrowCursor));
        });
        connect(pen_button_, &QPushButton::clicked, owner, [this] {
            QColor c = QColorDialog::getColor(pen_color_, owner_, "墨迹颜色");
            if (c.isValid()) set_pen_color(c);
        });
        connect(width_spin_, qOverload<int>(&QSpinBox::valueChanged), owner,
                [this](int v) { pen_width_ = v; });
        connect(undo_ink, &QPushButton::clicked, owner, [this] {
            if (ink_) {
                ink_->undo();
                schedule_save();
            }
        });
        connect(clear_ink, &QPushButton::clicked, owner, [this] {
            if (ink_) {
                ink_->clear();
                schedule_save();
            }
        });
        connect(add_image, &QPushButton::clicked, owner, [this] {
            if (owner_->on_add_image) owner_->on_add_image();
        });
        connect(front, &QPushButton::clicked, owner, [this] { change_z(true); });
        connect(back, &QPushButton::clicked, owner, [this] { change_z(false); });
        connect(remove_image, &QPushButton::clicked, owner, [this] { remove_selected_image(); });

        save_timer_ = new QTimer(owner);
        save_timer_->setSingleShot(true);
        save_timer_->setInterval(450);
        connect(save_timer_, &QTimer::timeout, owner, [this] { save_layout(); });
        connect(scene_, &QGraphicsScene::changed, owner, [this](const QList<QRectF>&) {
            if (!loading_) save_timer_->start();
        });

        view_->on_press = [this](QPointF p, Qt::KeyboardModifiers) -> bool {
            if (!ink_mode_ || !ink_) return false;
            if (eraser_) {
                if (ink_->erase_at(p)) schedule_save();
                return true;
            }
            ink_->begin_stroke(p, pen_color_, pen_width_);
            drawing_ = true;
            return true;
        };
        view_->on_move = [this](QPointF p, Qt::KeyboardModifiers) -> bool {
            if (!ink_mode_ || !ink_) return false;
            if (eraser_) {
                if (ink_->erase_at(p)) schedule_save();
                return true;
            }
            if (drawing_) {
                ink_->add_point(p);
                return true;
            }
            return false;
        };
        view_->on_release = [this] {
            if (drawing_ && ink_) {
                ink_->end_stroke();
                drawing_ = false;
                schedule_save();
            }
        };
    }

    void set_dark(bool dark) {
        dark_ = dark;
        view_->setBackgroundBrush(QColor(dark ? "#f4f6fb" : "#f4f6fb"));
        if (owner_->on_layout_changed || text_) update_text();
    }

    void set_ink_mode(bool on) {
        ink_mode_ = on;
        if (ink_button_ && ink_button_->isChecked() != on) ink_button_->setChecked(on);
        view_->setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
    }

    bool ink_mode() const { return ink_mode_; }

    void set_event(Event* event) {
        loading_ = true;
        current_ = event ? *event : Event();
        has_event_ = event != nullptr;
        scene_->clear();
        text_ = nullptr;
        ink_ = nullptr;
        images_.clear();
        if (!has_event_) {
            loading_ = false;
            return;
        }
        layout_ = nlohmann::json::object();
        if (!current_.page_layout.empty()) {
            try {
                layout_ = nlohmann::json::parse(current_.page_layout);
            } catch (...) {
                layout_ = nlohmann::json::object();
            }
        }
        hidden_images_.clear();
        if (layout_.contains("hidden") && layout_["hidden"].is_array()) {
            for (const auto& h : layout_["hidden"]) {
                if (h.is_string()) hidden_images_.insert(QString::fromStdString(h.get<std::string>()));
            }
        }
        text_ = new PageTextItem();
        text_->setDefaultTextColor(QColor("#1c2130"));
        text_->setHtml(markdown_to_html(QString::fromStdString(current_.note), false));
        text_->setTextWidth(kTextWidth);
        qreal tx = 40, ty = 40, tz = 10;
        if (layout_.contains("text") && layout_["text"].is_object()) {
            auto& t = layout_["text"];
            tx = t.value("x", 40.0);
            ty = t.value("y", 40.0);
            tz = t.value("z", 10.0);
        }
        text_->setPos(tx, ty);
        text_->setZValue(tz);
        text_->changed = [this] { schedule_save(); };
        scene_->addItem(text_);

        ink_ = new InkLayer();
        if (layout_.contains("ink")) ink_->from_json(layout_["ink"]);
        scene_->addItem(ink_);

        nlohmann::json image_layout = nlohmann::json::array();
        if (layout_.contains("images") && layout_["images"].is_array()) {
            image_layout = layout_["images"];
        }
        int index = 0;
        for (const auto& att : current_.attachments) {
            if (!att.is_image()) continue;
            if (hidden_images_.contains(QString::fromStdString(att.path))) continue;
            fs::path file;
            try {
                file = repo_.store.resolve(att);
            } catch (...) {
                continue;
            }
            QPixmap pix(QString::fromStdWString(file.wstring()));
            if (pix.isNull()) continue;
            auto* item = new PageImageItem(pix, QString::fromStdString(att.path));
            qreal base_y = text_ ? text_->pos().y() + text_->boundingRect().height() + 40 : 120;
            qreal x = 60 + index * 28, y = base_y + index * 24, w = 0, h = 0, rot = 0, z = 20 + index;
            for (const auto& entry : image_layout) {
                if (entry.is_object() &&
                    entry.value("path", std::string()) == att.path) {
                    x = entry.value("x", x);
                    y = entry.value("y", y);
                    w = entry.value("w", 0.0);
                    h = entry.value("h", 0.0);
                    rot = entry.value("rot", 0.0);
                    z = entry.value("z", z);
                    break;
                }
            }
            if (w > 20 && h > 20) item->set_content_size(QSizeF(w, h));
            item->setPos(x, y);
            item->setRotation(rot);
            item->setZValue(z);
            item->changed = [this] { schedule_save(); };
            scene_->addItem(item);
            images_.append(item);
            ++index;
        }
        update_scene_rect();
        loading_ = false;
    }

    void set_note(const QString& markdown) {
        if (!text_) return;
        text_->setHtml(markdown_to_html(markdown, false));
        update_scene_rect();
    }

    void refresh_images(Event* event) {
        if (!event) return;
        // 保留墨迹和文字位置，只重建图片项
        loading_ = true;
        for (auto* item : images_) scene_->removeItem(item);
        qDeleteAll(images_);
        images_.clear();
        current_ = *event;
        nlohmann::json image_layout = nlohmann::json::array();
        if (layout_.contains("images") && layout_["images"].is_array()) image_layout = layout_["images"];
        int index = 0;
        for (const auto& att : current_.attachments) {
            if (!att.is_image()) continue;
            if (hidden_images_.contains(QString::fromStdString(att.path))) continue;
            fs::path file;
            try {
                file = repo_.store.resolve(att);
            } catch (...) {
                continue;
            }
            QPixmap pix(QString::fromStdWString(file.wstring()));
            if (pix.isNull()) continue;
            auto* item = new PageImageItem(pix, QString::fromStdString(att.path));
            qreal base_y = text_ ? text_->pos().y() + text_->boundingRect().height() + 40 : 120;
            qreal x = 60 + index * 28, y = base_y + index * 24, w = 0, h = 0, rot = 0, z = 20 + index;
            for (const auto& entry : image_layout) {
                if (entry.is_object() && entry.value("path", std::string()) == att.path) {
                    x = entry.value("x", x); y = entry.value("y", y);
                    w = entry.value("w", 0.0); h = entry.value("h", 0.0);
                    rot = entry.value("rot", 0.0); z = entry.value("z", z);
                    break;
                }
            }
            if (w > 20 && h > 20) item->set_content_size(QSizeF(w, h));
            item->setPos(x, y);
            item->setRotation(rot);
            item->setZValue(z);
            item->changed = [this] { schedule_save(); };
            scene_->addItem(item);
            images_.append(item);
            ++index;
        }
        update_scene_rect();
        loading_ = false;
    }

    void update_text() {
        set_note(QString::fromStdString(current_.note));
    }

private:
    void set_pen_color(const QColor& color) {
        pen_color_ = color;
        pen_button_->setStyleSheet(
            QString("background:%1;border:2px solid #ffffff;border-radius:12px;")
                .arg(color.name()));
    }
    void schedule_save() { if (!loading_) save_timer_->start(); }
    void update_scene_rect() {
        qreal height = kPageMinHeight;
        if (text_) height = std::max(height, text_->pos().y() + text_->boundingRect().height() + 120);
        for (auto* item : images_) {
            height = std::max(height, item->pos().y() + item->boundingRect().height() + 120);
        }
        scene_->setSceneRect(0, 0, kPageWidth, height);
    }
    void change_z(bool front) {
        auto selected = scene_->selectedItems();
        for (auto* item : selected) {
            if (item == ink_) continue;
            item->setZValue(front ? 900 : 1);
        }
        if (!selected.isEmpty()) schedule_save();
    }
    void remove_selected_image() {
        auto selected = scene_->selectedItems();
        for (auto* item : selected) {
            auto* image = dynamic_cast<PageImageItem*>(item);
            if (!image) continue;
            hidden_images_.insert(image->path());
            images_.removeAll(image);
            scene_->removeItem(image);
            delete image;
        }
        update_scene_rect();
        schedule_save();
    }
    void save_layout() {
        if (!has_event_ || loading_) return;
        nlohmann::json out = nlohmann::json::object();
        if (text_) {
            out["text"] = {{"x", text_->pos().x()}, {"y", text_->pos().y()},
                           {"z", text_->zValue()}, {"w", text_->textWidth()}};
        }
        auto arr = nlohmann::json::array();
        for (auto* item : images_) {
            arr.push_back({{"path", item->path().toStdString()},
                           {"x", item->pos().x()}, {"y", item->pos().y()},
                           {"w", item->content_size().width()},
                           {"h", item->content_size().height()},
                           {"rot", item->rotation()}, {"z", item->zValue()}});
        }
        out["images"] = arr;
        auto hidden = nlohmann::json::array();
        for (const QString& h : hidden_images_) hidden.push_back(h.toStdString());
        out["hidden"] = hidden;
        if (ink_) out["ink"] = ink_->to_json();
        layout_ = out;
        if (owner_->on_layout_changed) {
            owner_->on_layout_changed(QString::fromStdString(out.dump()));
        }
    }

    PageEditor* owner_;
    Repository& repo_;
    QGraphicsScene* scene_ = nullptr;
    PageView* view_ = nullptr;
    PageTextItem* text_ = nullptr;
    InkLayer* ink_ = nullptr;
    QVector<PageImageItem*> images_;
    QPushButton* ink_button_ = nullptr;
    QPushButton* pen_button_ = nullptr;
    QSpinBox* width_spin_ = nullptr;
    QTimer* save_timer_ = nullptr;
    QColor pen_color_ = QColor("#1f2430");
    qreal pen_width_ = 4;
    bool ink_mode_ = false;
    bool eraser_ = false;
    bool drawing_ = false;
    bool loading_ = false;
    bool dark_ = false;
    bool has_event_ = false;
    Event current_;
    nlohmann::json layout_ = nlohmann::json::object();
    QSet<QString> hidden_images_;
};

PageEditor::PageEditor(Repository& repo, QWidget* parent)
    : QWidget(parent), impl_(new Impl(this, repo)) {}

PageEditor::~PageEditor() { delete impl_; }

void PageEditor::set_event(Event* event) { impl_->set_event(event); }
void PageEditor::set_note(const QString& markdown) { impl_->set_note(markdown); }
void PageEditor::set_dark(bool dark) { impl_->set_dark(dark); }
void PageEditor::set_ink_mode(bool enabled) { impl_->set_ink_mode(enabled); }
bool PageEditor::ink_mode() const { return impl_->ink_mode(); }
void PageEditor::refresh_images(Event* event) { impl_->refresh_images(event); }

}  // namespace moji
