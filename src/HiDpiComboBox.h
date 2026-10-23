#pragma once

#include <QComboBox>
#include <QAbstractItemView>
#include <QEvent>
#include <QFrame>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QStyle>

class HiDpiPopupOutline final : public QWidget {
public:
    explicit HiDpiPopupOutline(QWidget *parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        const qreal scale = devicePixelRatioF();
        QImage image(qFloor(width() * scale), qFloor(height() * scale), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        const int width = image.width(), height = image.height(), radius = qRound(6 * scale);
        const QColor border = palette().color(QPalette::Mid).lightness() < 128
            ? palette().color(QPalette::Mid) : QColor("#ccd8e5");
        QPainterPath outer;
        outer.moveTo(0, 0);
        outer.lineTo(width, 0);
        outer.lineTo(width, height - radius);
        outer.quadTo(width, height, width - radius, height);
        outer.lineTo(radius, height);
        outer.quadTo(0, height, 0, height - radius);
        outer.closeSubpath();
        QPainterPath inner;
        inner.moveTo(1, 0);
        inner.lineTo(width - 1, 0);
        inner.lineTo(width - 1, height - radius);
        inner.quadTo(width - 1, height - 1, width - radius, height - 1);
        inner.lineTo(radius, height - 1);
        inner.quadTo(1, height - 1, 1, height - radius);
        inner.closeSubpath();
        QPainter shape(&image);
        shape.setRenderHint(QPainter::Antialiasing);
        shape.fillPath(outer, border);
        shape.setCompositionMode(QPainter::CompositionMode_Clear);
        shape.fillPath(inner, Qt::black);
        shape.setCompositionMode(QPainter::CompositionMode_SourceOver);
        shape.fillRect(QRect(0, 0, 1, height - radius), border);
        shape.fillRect(QRect(width - 1, 0, 1, height - radius), border);
        shape.fillRect(QRect(radius, height - 1, width - 2 * radius, 1), border);
        shape.end();
        for (int y = 0; y < radius; ++y) {
            for (int x = 0; x < radius; ++x)
                image.setPixel(x, height - 1 - y, image.pixel(width - 1 - x, height - 1 - y));
        }
        image.setDevicePixelRatio(scale);
        QPainter painter(this);
        painter.drawImage(QPointF(0, 0), image);
    }
};

class HiDpiComboBox final : public QComboBox {
public:
    using QComboBox::QComboBox;

protected:
    void showPopup() override {
        setPopupOpen(true);
        QWidget *popup = view()->window();
        popup->installEventFilter(this);
        popup->setWindowFlag(Qt::NoDropShadowWindowHint, true);
        if (auto *frame = qobject_cast<QFrame *>(popup))
            frame->setFrameShape(QFrame::NoFrame);
        QComboBox::showPopup();
        if (!popup->isVisible()) {
            setPopupOpen(false);
            return;
        }
        const auto bottomRoundedMask = [](const QSize &size, qreal radius) {
            const qreal width = size.width(), height = size.height();
            QPainterPath shape;
            shape.moveTo(0, 0);
            shape.lineTo(width, 0);
            shape.lineTo(width, height - radius);
            shape.quadTo(width, height, width - radius, height);
            shape.lineTo(radius, height);
            shape.quadTo(0, height, 0, height - radius);
            shape.closeSubpath();
            return QRegion(shape.toFillPolygon().toPolygon());
        };
        popup->setMask(bottomRoundedMask(popup->size(), 6));
        QWidget *outline = popup->findChild<QWidget *>(QStringLiteral("hiDpiPopupOutline"), Qt::FindDirectChildrenOnly);
        if (!outline) {
            outline = new HiDpiPopupOutline(popup);
            outline->setObjectName(QStringLiteral("hiDpiPopupOutline"));
        }
        outline->setGeometry(popup->rect());
        outline->raise();
        outline->show();
    }

    void hidePopup() override {
        QComboBox::hidePopup();
        setPopupOpen(false);
    }

    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == view()->window() && event->type() == QEvent::Hide)
            setPopupOpen(false);
        return QComboBox::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent *event) override {
        if (
#ifdef Q_OS_WIN
            devicePixelRatioF() <= 1 ||
#endif
            isEditable() || objectName() == QStringLiteral("tableEditor")) {
            QComboBox::paintEvent(event);
            return;
        }

        const qreal scale = devicePixelRatioF();
        QImage image(qFloor(width() * scale), qFloor(height() * scale), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        const bool dark = palette().color(QPalette::Window).lightness() < 128;
        const QColor border = !isEnabled() ? QColor(dark ? "#40516a" : "#d6dee7")
            : hasFocus() ? QColor("#1677ff") : underMouse() ? QColor("#77aef4")
            : QColor(dark ? "#52647c" : "#ccd8e5");
        const QColor background = isEnabled() ? palette().color(QPalette::Base)
            : QColor(dark ? "#2b3544" : "#edf1f5");
        const int radius = qRound(6 * scale);
        QPainter shape(&image);
        shape.setRenderHint(QPainter::Antialiasing);
        shape.setPen(Qt::NoPen);
        shape.setBrush(border);
        shape.drawRoundedRect(QRectF(0, 0, image.width(), image.height()), radius, radius);
        shape.setBrush(background);
        shape.drawRoundedRect(QRectF(1, 1, image.width() - 2, image.height() - 2),
                              radius - 1, radius - 1);
        shape.fillRect(QRect(radius, 0, image.width() - 2 * radius, 1), border);
        shape.fillRect(QRect(radius, image.height() - 1, image.width() - 2 * radius, 1), border);
        shape.fillRect(QRect(0, radius, 1, image.height() - 2 * radius), border);
        shape.fillRect(QRect(image.width() - 1, radius, 1, image.height() - 2 * radius), border);
        shape.end();
        for (int y = 0; y < radius; ++y) {
            for (int x = 0; x < radius; ++x) {
                const QRgb pixel = image.pixel(image.width() - 1 - x, image.height() - 1 - y);
                image.setPixel(x, y, pixel);
                image.setPixel(image.width() - 1 - x, y, pixel);
                image.setPixel(x, image.height() - 1 - y, pixel);
            }
        }
        if (popupOpen_) {
            QPainter square(&image);
            square.fillRect(QRect(0, image.height() - radius, image.width(), radius), border);
            square.fillRect(QRect(1, image.height() - radius, image.width() - 2, radius - 1), background);
        }
        image.setDevicePixelRatio(scale);

        QPainter painter(this);
        painter.drawImage(QPointF(0, 0), image);
        painter.setPen(isEnabled() ? palette().color(QPalette::Text)
            : QColor(dark ? "#8493a7" : "#95a2b2"));
        painter.drawText(QRect(9, 0, width() - 39, height()), Qt::AlignVCenter | Qt::TextSingleLine,
                         fontMetrics().elidedText(currentText(), Qt::ElideRight, width() - 39));
        const QPixmap arrow(QStringLiteral(":/icons/chevron-down.xpm"));
        painter.setOpacity(isEnabled() ? 1.0 : 0.5);
        painter.drawPixmap(width() - 18, (height() - arrow.height()) / 2, arrow);
    }

private:
    void setPopupOpen(bool open) {
        if (popupOpen_ == open) return;
        popupOpen_ = open;
        setProperty("popupOpen", open);
        style()->unpolish(this);
        style()->polish(this);
        update();
    }

    bool popupOpen_ = false;
};
