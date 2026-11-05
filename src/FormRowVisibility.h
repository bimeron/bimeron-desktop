#pragma once

#include <QFormLayout>
#include <QLayoutItem>
#include <QWidget>

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
    form->setRowVisible(row, visible);
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 3)
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
    if (row < 0) {
        form->setRowVisible(widget, visible); // Preserve Qt's invalid-widget warning.
        return;
    }
    setFormRowVisible(form, row, visible);
}
