#pragma once

#include <QCheckBox>
#include <QImage>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionButton>

class HiDpiCheckBox final : public QCheckBox {
public:
    using QCheckBox::QCheckBox;

protected:
    void paintEvent(QPaintEvent *event) override {
#ifndef Q_OS_WIN
        QCheckBox::paintEvent(event);
#else
        if (devicePixelRatioF() <= 1) {
            QCheckBox::paintEvent(event);
            return;
        }

        QStyleOptionButton option;
        initStyleOption(&option);
        const QRect indicator = style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option, this);
        const qreal scale = devicePixelRatioF();
        QImage image(qFloor(indicator.width() * scale), qFloor(indicator.height() * scale),
                     QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        const bool hovered = option.state & QStyle::State_MouseOver;
        const QColor border = !isEnabled() ? QColor("#d6dee7")
            : isChecked() ? QColor(hovered ? "#0e6fe9" : "#1677ff")
                          : QColor(hovered ? "#1677ff" : "#b8c6d6");
        const QColor background = !isEnabled() ? palette().color(QPalette::Window)
            : hovered ? palette().color(QPalette::AlternateBase) : palette().color(QPalette::Base);
        const int radius = qRound(4 * scale);
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
        image.setDevicePixelRatio(scale);

        QPainter painter(this);
        const QRect label(indicator.right() + 8, 0, width() - indicator.right() - 8, height());
        style()->drawItemText(&painter, label, Qt::AlignVCenter | Qt::TextShowMnemonic,
                              option.palette, isEnabled(), text(), QPalette::WindowText);
        painter.drawImage(indicator.topLeft(), image);
        if (isChecked()) {
            const QPixmap check(QStringLiteral(":/icons/check.xpm"));
            painter.setOpacity(isEnabled() ? 1.0 : 0.5);
            painter.drawPixmap(indicator.center().x() - check.width() / 2,
                               indicator.center().y() - check.height() / 2, check);
        }
#endif
    }
};
