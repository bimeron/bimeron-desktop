#pragma once

#include <QEvent>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QWidget>

namespace WindowPlacement {

// Returns the frame position that keeps `frame` inside `available`. A frame
// that already fits keeps its position. Otherwise it is centered over
// `anchor` (when valid) and clamped so the title bar is never above the top
// edge, even when the frame is taller than the available area.
inline QPoint fittedFramePosition(const QRect &frame, const QRect &available, const QRect &anchor) {
    if (available.contains(frame)) return frame.topLeft();
    QPoint target = anchor.isValid()
        ? anchor.center() - QPoint(frame.width() / 2, frame.height() / 2)
        : frame.topLeft();
    target.setX(qMax(available.left(), qMin(target.x(), available.right() + 1 - frame.width())));
    target.setY(qMax(available.top(), qMin(target.y(), available.bottom() + 1 - frame.height())));
    return target;
}

// Re-checks a top-level window after it is shown or resized. Qt centers a
// dialog before its native frame size is known; on Windows 11 this can leave
// the title bar above the screen, so the window cannot be moved. The check
// runs after the event loop turn, when the real frame geometry is available.
class KeepFrameOnScreen final : public QObject {
public:
    KeepFrameOnScreen(QWidget *window, QWidget *anchor) : QObject(window), window_(window), anchor_(anchor) {
        window->installEventFilter(this);
    }

    void apply() {
        if (!window_ || !window_->isVisible() || !window_->screen()
            || (window_->windowState() & (Qt::WindowMinimized | Qt::WindowMaximized | Qt::WindowFullScreen)))
            return;
        const QRect frame = window_->frameGeometry();
        const QRect anchor = anchor_ && anchor_->isVisible() && !anchor_->isMinimized()
            ? anchor_->window()->frameGeometry() : QRect{};
        const QPoint target = fittedFramePosition(frame, window_->screen()->availableGeometry(), anchor);
        if (target != frame.topLeft()) window_->move(target);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == window_ && (event->type() == QEvent::Show || event->type() == QEvent::Resize)
            && !pending_) {
            pending_ = true;
            QTimer::singleShot(0, this, [this] { pending_ = false; apply(); });
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QPointer<QWidget> window_;
    QPointer<QWidget> anchor_;
    bool pending_ = false;
};

}
