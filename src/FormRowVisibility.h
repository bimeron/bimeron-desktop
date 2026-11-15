#pragma once

#include <QFormLayout>
#include <QHash>
#include <QLayoutItem>
#include <QPointer>
#include <QWidget>

#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
namespace FormRowVisibilityDetail {
// Qt 6.2 has no row-visibility API, and QFormLayout still adds vertical
// spacing for a row whose widgets are merely hidden: it skips only rows whose
// cells are empty. A form with many hidden rows therefore reserves a tall
// blank band. Hidden rows are emptied instead, and their widgets are parked
// here so the same row can be refilled when it is shown again.
struct HiddenRow {
    QPointer<QWidget> label;
    QPointer<QWidget> field;
    bool spanning = false;
};

class HiddenRows : public QObject {
public:
    explicit HiddenRows(QFormLayout *form) : QObject(form) { setObjectName(name()); }
    static QString name() { return QStringLiteral("bimeronHiddenFormRows"); }
    QHash<int, HiddenRow> rows;
};

inline HiddenRows *hiddenRows(QFormLayout *form, bool create)
{
    if (auto *found = form->findChild<QObject *>(HiddenRows::name(), Qt::FindDirectChildrenOnly))
        return static_cast<HiddenRows *>(found);
    return create ? new HiddenRows(form) : nullptr;
}

inline int hiddenRowOf(QFormLayout *form, QWidget *widget)
{
    if (auto *hidden = hiddenRows(form, false)) {
        for (auto it = hidden->rows.cbegin(); it != hidden->rows.cend(); ++it) {
            if (it->label == widget || it->field == widget)
                return it.key();
        }
    }
    return -1;
}
} // namespace FormRowVisibilityDetail
#endif

// Shows or hides a QFormLayout row without triggering QTBUG-109237.
//
// Qt 6.4.0-6.4.2 keep the vertical layout index that a row received while it
// was visible after the row is hidden. Once fewer rows are visible, a form
// that contains a height-for-width item (for example, a word-wrapped QLabel)
// indexes its resized row array with that stale value and writes past the
// heap allocation. Qt 6.4.3 resets the index of hidden rows. On affected
// versions, re-inserting the hidden row gives it fresh layout items whose
// index is unset, which is the state the upstream fix restores. Widgets keep
// their parent, buddy, focus order, and explicit hidden state.
//
// Only rows built from widgets are supported; every caller uses widget rows.
inline void setFormRowVisible(QFormLayout *form, int row, bool visible)
{
    if (!form || row < 0 || row >= form->rowCount())
        return;
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
    // While a row is hidden its cells stay empty, so itemAt(), labelForField()
    // and getWidgetPosition() do not report its widgets. Rows must not be
    // inserted or removed above a hidden row until it is shown again.
    using namespace FormRowVisibilityDetail;
    auto *hidden = hiddenRows(form, !visible);
    if (hidden && hidden->rows.contains(row)) {
        if (!visible)
            return;
        const HiddenRow parked = hidden->rows.take(row);
        if (parked.spanning) {
            if (parked.field) form->setWidget(row, QFormLayout::SpanningRole, parked.field);
        } else {
            if (parked.label) form->setWidget(row, QFormLayout::LabelRole, parked.label);
            if (parked.field) form->setWidget(row, QFormLayout::FieldRole, parked.field);
        }
        for (QWidget *widget : {parked.label.data(), parked.field.data()})
            if (widget) widget->setVisible(true);
        return;
    }
    QLayoutItem *spanningItem = form->itemAt(row, QFormLayout::SpanningRole);
    QLayoutItem *labelItem = spanningItem ? nullptr : form->itemAt(row, QFormLayout::LabelRole);
    QLayoutItem *fieldItem = spanningItem ? spanningItem : form->itemAt(row, QFormLayout::FieldRole);
    QWidget *label = labelItem ? labelItem->widget() : nullptr;
    QWidget *field = fieldItem ? fieldItem->widget() : nullptr;
    for (QWidget *widget : {label, field})
        if (widget) widget->setVisible(visible);
    if (visible || (labelItem && !label) || (fieldItem && !field) || (!label && !field))
        return;
    // removeWidget() keeps the widget's parent and leaves an empty row behind.
    for (QWidget *widget : {label, field})
        if (widget) form->removeWidget(widget);
    hidden->rows.insert(row, HiddenRow{label, field, spanningItem != nullptr});
#else
    form->setRowVisible(row, visible);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0) && QT_VERSION < QT_VERSION_CHECK(6, 4, 3)
    if (visible)
        return;
    const bool spanning = form->itemAt(row, QFormLayout::SpanningRole) != nullptr;
    QLayoutItem *labelItem = form->itemAt(row, QFormLayout::LabelRole);
    QLayoutItem *fieldItem = form->itemAt(row, spanning ? QFormLayout::SpanningRole : QFormLayout::FieldRole);
    if ((labelItem && !labelItem->widget()) || (fieldItem && !fieldItem->widget()))
        return;
    const QFormLayout::TakeRowResult taken = form->takeRow(row);
    QWidget *label = taken.labelItem ? taken.labelItem->widget() : nullptr;
    QWidget *field = taken.fieldItem ? taken.fieldItem->widget() : nullptr;
    // takeRow() transfers the wrapper items; deleting them leaves the widgets alive.
    delete taken.labelItem;
    delete taken.fieldItem;
    if (spanning)
        form->insertRow(row, field);
    else
        form->insertRow(row, label, field);
#endif
}

inline void setFormRowVisible(QFormLayout *form, QWidget *widget, bool visible)
{
    if (!form || !widget)
        return;
    int row = -1;
    QFormLayout::ItemRole role;
    form->getWidgetPosition(widget, &row, &role);
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
    if (row < 0)
        row = FormRowVisibilityDetail::hiddenRowOf(form, widget);
#endif
    if (row < 0) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
        form->setRowVisible(widget, visible); // Preserve Qt's invalid-widget warning.
#else
        qWarning("setFormRowVisible: widget is not in this form");
#endif
        return;
    }
    setFormRowVisible(form, row, visible);
}
