#include "ui/fluent_lite/buttons.h"
#include "core/ui_scale.h"
#include <QDesktopServices>
#include <QUrl>
#include <QStylePainter>
#include <QStyleOptionButton>
#include <QLinearGradient>
#include <algorithm>

namespace jietuba {
namespace {
void paint_action_button(QPushButton *button, QStylePainter &painter, QStyleOptionButton option, bool primary) {
    const auto &t = ui_tokens(button);
    const bool dark = t.is_dark();
    const bool down = button->isDown() || button->isChecked();
    const bool hover = button->underMouse() && button->isEnabled();
    QColor top, bottom, edge;
    if (primary) {
        const QColor accent(t.accent_strong);
        top = down ? accent.darker(108) : accent.lighter(hover ? 116 : 108);
        bottom = down ? accent.darker(108) : accent.darker(hover ? 100 : 104);
        edge = accent.darker(115);
    } else {
        const float hue = std::max(0.0f,QColor(t.accent).hslHueF());
        const float lightness = dark ? (down ? 0.19f : hover ? 0.26f : 0.22f) : (down ? 0.92f : hover ? 0.99f : 0.975f);
        top = bottom = QColor::fromHslF(hue,0.10f,lightness);
        edge = QColor::fromHslF(hue,0.12f,dark ? 0.34f : 0.79f);
    }
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF bounds = QRectF(button->rect()).adjusted(0.75,0.75,-0.75,-0.75);
    const qreal radius = widget_scaled(button,8);
    if (!button->isEnabled()) painter.setOpacity(0.45);
    QLinearGradient fill(bounds.topLeft(),bounds.bottomLeft());
    fill.setColorAt(0,top); fill.setColorAt(1,bottom);
    painter.setBrush(fill);
    painter.setPen(QPen(edge,1));
    painter.drawRoundedRect(bounds,radius,radius);
    if (button->hasFocus() && option.state.testFlag(QStyle::State_KeyboardFocusChange)) {
        QColor focus(primary ? Qt::white : QColor(t.accent));
        focus.setAlpha(primary ? 180 : 190);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(focus,1.5));
        painter.drawRoundedRect(bounds.adjusted(2,2,-2,-2),radius-2,radius-2);
    }
    option.palette.setColor(QPalette::ButtonText,primary ? Qt::white : QColor(t.text));
    painter.drawControl(QStyle::CE_PushButtonLabel,option);
}

QString base_style(const QWidget *widget) {
    const auto &t = ui_tokens(widget);
    auto s = [widget](double v) { return widget_scaled(widget, v); };
    return QString(R"(
    QPushButton { min-height: %1px; padding: %2px %3px; color: %4;
     background: %5; border: 1px solid %6; border-radius: %7px;
     font: %8px %9; outline: none; }
    QPushButton:hover { background: %10; border-color: %11; }
    QPushButton:pressed { background: %12; }
    QPushButton:focus { border-color: %13; }
    QPushButton:disabled { color: %14; background: %12; border-color: %6; }
    )").arg(s(28)).arg(s(4)).arg(s(16)).arg(t.text, t.surface, t.border).arg(s(10)).arg(s(13)).arg(FONT_FAMILY, t.surface_strong, t.border_hover, t.surface_subtle)
        .arg(get_ui_theme()->tokens().accent, t.text_disabled);
}

QString primary_style(const QWidget *widget) {
    const auto &t = ui_tokens(widget);
    auto s = [widget](double v) { return widget_scaled(widget, v); };
    return QString(R"(
    QPushButton { min-height: %1px; padding: %2px %3px; color: white;
     background: %4; border: 1px solid rgba(255,255,255,.34); border-radius: %5px;
     font: 600 %6px %7; outline: none; }
    QPushButton:hover { background: %8; border-color: %8; }
    QPushButton:pressed { background: %9; border-color: %9; padding-top: %10px; padding-bottom: %11px; }
    QPushButton:disabled { color: rgba(255,255,255,.84); background: #687D8F; border-color: #687D8F; }
    )").arg(s(28)).arg(s(4)).arg(s(18)).arg(t.accent_strong).arg(s(10)).arg(s(13)).arg(FONT_FAMILY, t.accent_strong_hover, t.accent_strong_pressed).arg(s(5)).arg(s(3));
}

QString transparent_style(const QWidget *widget) {
    const auto &t = ui_tokens(widget);
    auto s = [widget](double v) { return widget_scaled(widget, v); };
    return QString(R"(
    QPushButton { min-height: %1px; padding: %2px %3px; color: %4;
     background: transparent; border: 1px solid transparent; border-radius: %5px;
     font: %6px %7; outline: none; }
    QPushButton:hover { color: %8; background: %9; }
    QPushButton:pressed { background: %10; }
    QPushButton:disabled { color: %11; }
    )").arg(s(28)).arg(s(4)).arg(s(12)).arg(t.text_muted).arg(s(10)).arg(s(13)).arg(FONT_FAMILY, t.text, t.surface_subtle, t.surface, t.text_disabled);
}
}

PushButton::PushButton(const QString &text, QWidget *parent, const IconSource &icon) : QPushButton(text, parent) {
    setCursor(cursor().shape());
    init_theme();
    if (!std::holds_alternative<std::monostate>(icon)) setIcon(icon);
}

void PushButton::init_theme() {
    // style_sheet() is virtual but not dispatched to subclasses during this
    // constructor; each subclass constructor applies its own style again.
    _apply_theme();
    connect(get_ui_theme(), &UIThemeManager::theme_changed, this, [this] { _apply_theme(); });
}

void PushButton::setIcon(const IconSource &icon) {
    theme_icon_source_ = icon;
    QPushButton::setIcon(to_qicon(icon, this));
}

void PushButton::setBaseIconSize(int base) {
    base_icon_size_ = base;
    _apply_theme();
}

QString PushButton::style_sheet() const { return base_style(this); }

void PushButton::_apply_theme() {
    apply_style_sheet(this, style_sheet());
    const int side = widget_scaled(this, base_icon_size_);
    setIconSize(QSize(side, side));
    if (!std::holds_alternative<std::monostate>(theme_icon_source_)) QPushButton::setIcon(to_qicon(theme_icon_source_, this));
}

ColorSwatchButton::ColorSwatchButton(QWidget *parent) : PushButton(QString(), parent) {
    setProperty("_materialPreserveColor",true);
    setCursor(Qt::PointingHandCursor);
    _apply_theme();
}

void ColorSwatchButton::setFill(const QString &fill) {
    fill_ = fill;
    _apply_theme();
}

void ColorSwatchButton::setSelected(bool selected) {
    selected_ = selected;
    setProperty("_materialSelected",selected);
    _apply_theme();
}

QString ColorSwatchButton::style_sheet() const {
    const auto &t = ui_tokens(this);
    auto s = [this](double v) { return widget_scaled(this, v); };
    const auto border = selected_ ? QString("2px solid %1").arg(get_ui_theme()->tokens().accent) : QString("1px solid %1").arg(t.border);
    const auto hover = selected_ ? border : QString("1px solid %1").arg(t.border_hover);
    return QString(R"(
        QPushButton { background: %1; border: %2;
         border-radius: %3px; min-height: %4px;
         padding: %5px 0; color: %6;
         font: 600 %7px %8; outline: none; }
        QPushButton:hover { border: %9; }
        )").arg(fill_, border).arg(s(INPUT_RADIUS)).arg(s(INPUT_CONTENT_HEIGHT)).arg(s(INPUT_PADDING_V))
        .arg(t.text).arg(s(13)).arg(FONT_FAMILY, hover);
}

PrimaryPushButton::PrimaryPushButton(const QString &text, QWidget *parent, const IconSource &icon) : PushButton(text, parent, icon) {
    setProperty("_materialPrimary",true);
    setProperty("_materialNative",true);
    _apply_theme();
}
QString PrimaryPushButton::style_sheet() const { return primary_style(this); }
void PrimaryPushButton::paintEvent(QPaintEvent *) {
    QStylePainter painter(this);
    QStyleOptionButton option; initStyleOption(&option);
    paint_action_button(this,painter,option,true);
}

SecondaryPushButton::SecondaryPushButton(const QString &text, QWidget *parent) : PushButton(text,parent) {
    setProperty("_materialNative",true);
    _apply_theme();
}
void SecondaryPushButton::paintEvent(QPaintEvent *) {
    QStylePainter painter(this);
    QStyleOptionButton option; initStyleOption(&option);
    paint_action_button(this,painter,option,false);
}
TransparentPushButton::TransparentPushButton(const QString &text, QWidget *parent, const IconSource &icon) : PushButton(text, parent, icon) { _apply_theme(); }
QString TransparentPushButton::style_sheet() const { return transparent_style(this); }

HyperlinkButton::HyperlinkButton(const QString &url, const QString &text, QWidget *parent, const IconSource &icon)
    : TransparentPushButton(text, parent, icon), url_(url) {
    _apply_theme();
    connect(this, &QPushButton::clicked, this, [this] {
        if (!url_.isEmpty()) QDesktopServices::openUrl(QUrl(url_));
    });
}

QString HyperlinkButton::style_sheet() const { return transparent_style(this) + QString("QPushButton { color: %1; }").arg(get_ui_theme()->tokens().accent); }

TransparentToolButton::TransparentToolButton(const IconSource &icon, QWidget *parent) : QToolButton(parent) {
    _apply_theme();
    connect(get_ui_theme(), &UIThemeManager::theme_changed, this, [this] { _apply_theme(); });
    if (!std::holds_alternative<std::monostate>(icon)) setIcon(icon);
}

void TransparentToolButton::setBaseMetrics(int side, int icon_size) {
    base_side_ = side;
    base_icon_size_ = icon_size;
    _apply_theme();
}

void TransparentToolButton::setBaseIconSize(int base) {
    base_icon_size_ = base;
    _apply_theme();
}

void TransparentToolButton::setIcon(const IconSource &icon) {
    theme_icon_source_ = icon;
    QToolButton::setIcon(to_qicon(icon, this));
}

void TransparentToolButton::_apply_theme() {
    const auto &t = ui_tokens(this);
    auto s = [this](double v) { return widget_scaled(this, v); };
    setFixedSize(s(base_side_), s(base_side_));
    setIconSize(QSize(s(base_icon_size_), s(base_icon_size_)));
    apply_style_sheet(this, QString(R"(
            QToolButton { background: transparent; border: none; border-radius: %1px; }
            QToolButton:hover { background: %2; }
            QToolButton:pressed { background: %3; }
        )").arg(s(9)).arg(t.surface_hover, t.surface_subtle));
    if (!std::holds_alternative<std::monostate>(theme_icon_source_)) QToolButton::setIcon(to_qicon(theme_icon_source_, this));
}
}
