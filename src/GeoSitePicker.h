#pragma once
#include "GeoSiteLabels.h"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QSignalBlocker>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>
#include <algorithm>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QStyle>

// Keep Qt's native indicator/focus rendering; elide labels and IDs independently.
class GeoSiteRowDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        const QString name = index.data(Qt::UserRole + 3).toString();
        if (name.isEmpty()) { QStyledItemDelegate::paint(painter, option, index); return; }
        QStyleOptionViewItem styled(option);
        initStyleOption(&styled, index);
        const QString key = index.data(Qt::UserRole).toString();
        const QString suffix = index.data(Qt::UserRole + 4).toString();
        const auto *style = styled.widget ? styled.widget->style() : QApplication::style();
        QRect rect = style->subElementRect(QStyle::SE_ItemViewItemText, &styled, styled.widget).adjusted(2, 0, -4, 0);
        rect.setRight(option.rect.right() - 8);
        styled.text.clear();
        style->drawControl(QStyle::CE_ItemViewItem, &styled, painter, styled.widget);
        const QFontMetrics metrics(styled.font);
        const QString idText = " (" + key + ')' + suffix;
        const int idWidth = qMin(rect.width() / 2, metrics.horizontalAdvance(idText) + 4);
        const int nameWidth = qMin(rect.width() - idWidth, metrics.horizontalAdvance(name) + 4);
        painter->save(); painter->setFont(styled.font);
        painter->setPen(styled.palette.color(QPalette::Text));
        painter->drawText(QRect(rect.left(), rect.top(), nameWidth, rect.height()), Qt::AlignVCenter,
            metrics.elidedText(name, Qt::ElideRight, nameWidth));
        painter->setPen(QColor("#748194"));
        painter->drawText(QRect(rect.left() + nameWidth, rect.top(), rect.width() - nameWidth, rect.height()), Qt::AlignVCenter,
            metrics.elidedText(idText, Qt::ElideRight, rect.width() - nameWidth));
        painter->restore();
    }
};

class GeoSitePicker final : public QDialog {
public:
    GeoSitePicker(QWidget *parent, const QStringList &available, QStringList selected,
                  const QString &error) : QDialog(parent), original_(selected) {
        setWindowTitle(tr("Choose groups…"));
        setMinimumSize(590, 350);
        resize(728, 473);
        auto *layout = new QVBoxLayout(this);
        auto *search = new QLineEdit;
        search->setPlaceholderText(tr("Search names or IDs"));
        search->setAccessibleName(tr("Search names or IDs"));
        layout->addWidget(search);
        auto *topicRow = new QHBoxLayout;
        auto *topicLabel = new QLabel(tr("Topic"));
        auto *topics = new QComboBox;
        topics->setObjectName("geoSiteTopics");
        topics->setAccessibleName(tr("Topic"));
        topics->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        topics->setMinimumContentsLength(12);
        topicLabel->setBuddy(topics);
        topicRow->addWidget(topicLabel);
        topicRow->addWidget(topics, 1);
        if (!error.isEmpty()) {
            auto *notice = new QLabel(error);
            notice->setWordWrap(true); notice->setTextFormat(Qt::PlainText);
            layout->addWidget(notice);
        }
        auto *body = new QHBoxLayout;
        auto *sections = new QListWidget;
        sections->setObjectName("geoSiteSections");
        sections->setAccessibleName(tr("Available sections"));
        sections->setFixedWidth(190);
        sections->setWordWrap(true);
        sections->setTextElideMode(Qt::ElideNone);
        sections->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        sections->setStyleSheet("QListWidget { border: 0; border-right: 1px solid #dce5ef; border-radius: 0; } QListWidget::item { min-height: 28px; padding: 2px 7px; border: 0; }");
        sections->addItem(tr("All groups"));
        for (const auto &section : geoSiteSections()) sections->addItem(QObject::tr(qPrintable(section)));
        sections->addItem(tr("Unclassified"));
        sections->setCurrentRow(0);
        body->addWidget(sections);
        list_ = new QListWidget;
        list_->setObjectName("geoSiteGroups");
        list_->setAccessibleName(tr("Available groups"));
        list_->setFont(QApplication::font());
        list_->setItemDelegate(new GeoSiteRowDelegate(list_));
        list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list_->setStyleSheet("QListWidget::item { min-height: 24px; padding: 2px 4px; border: none; } QListWidget::indicator { width: 16px; height: 16px; border: 1px solid #b8c6d6; border-radius: 3px; } QListWidget::indicator:checked { border-color: #1677ff; image: url(:/icons/check.xpm); }");
        auto *results = new QVBoxLayout;
        results->addLayout(topicRow);
        results->addWidget(list_, 1);
        body->addLayout(results, 1);
        layout->addLayout(body, 1);
        QStringList groups = available;
        groups.removeDuplicates();
        for (const auto &id : selected) if (!groups.contains(id)) groups.append(id);
        const QSet<QString> availableSet(available.begin(), available.end());
        const auto bucket = [&availableSet](const QString &id) {
            if (!availableSet.contains(id)) return 3;
            const auto it = geoSiteEntries().constFind(id);
            return it == geoSiteEntries().cend() ? 2 : (it->collection ? 0 : 1);
        };
        std::sort(groups.begin(), groups.end(), [&](const QString &a, const QString &b) {
            if (bucket(a) != bucket(b)) return bucket(a) < bucket(b);
            const QString an = geoSiteName(a), bn = geoSiteName(b);
            const int compared = QString::localeAwareCompare(an.isEmpty() ? a : an, bn.isEmpty() ? b : bn);
            return compared == 0 ? a < b : compared < 0;
        });
        int previous = -1;
        for (const auto &id : groups) {
            const int group = bucket(id);
            if (group != previous) {
                auto *heading = new QListWidgetItem(group == 0 ? tr("Collections") : group == 1 ? tr("Individual services") : group == 2 ? tr("Unclassified") : tr("Unavailable groups"), list_);
                heading->setFlags(Qt::NoItemFlags);
                heading->setBackground(QColor("#f7f9fc"));
                QFont font = heading->font(); font.setBold(true); heading->setFont(font);
                previous = group;
            }
            const QString name = geoSiteName(id);
            QString label = name.isEmpty() ? id : name + " (" + id + ')';
            if (!availableSet.contains(id)) label += " — " + tr("Unavailable");
            auto *item = new QListWidgetItem(label, list_);
            item->setToolTip(label);
            item->setData(Qt::UserRole + 3, name);
            item->setData(Qt::UserRole + 4, availableSet.contains(id) ? QString{} : " — " + tr("Unavailable"));
            item->setData(Qt::UserRole, id);
            const auto it = geoSiteEntries().constFind(id);
            item->setData(Qt::UserRole + 1, it == geoSiteEntries().cend() ? -1 : it->section);
            item->setData(Qt::UserRole + 2, group);
            item->setData(Qt::UserRole + 5, it == geoSiteEntries().cend() || !it->topic ? QString{} : QString::fromUtf8(it->topic));
            item->setData(Qt::UserRole + 6, geoSiteSearchText(id));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(selected.contains(id) ? Qt::Checked : Qt::Unchecked);
        }
        auto *empty = new QLabel(tr("No matching groups"));
        layout->addWidget(empty);
        auto *footer = new QHBoxLayout;
        auto *count = new QPushButton;
        count->setObjectName("geoSiteSelectionCount");
        count->setCheckable(true);
        count->setAutoDefault(false);
        count->setToolTip(tr("Show only selected groups"));
        count->setAccessibleName(tr("Only selected"));
        footer->addWidget(count); footer->addStretch();
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        footer->addWidget(buttons); layout->addLayout(footer);
        auto updateCount = [this, count] {
            count->setText((count->isChecked() ? tr("Selected: %1 · Only selected") : tr("Selected: %1")).arg(selectedIds().size()));
        };
        connect(list_, &QListWidget::itemChanged, this, updateCount);
        connect(count, &QPushButton::toggled, this, updateCount);
        updateCount();
        auto filter = [this, search, sections, topics, count, empty] {
            const QString query = search->text().trimmed();
            const QString topic = topics->currentData().toString();
            QSet<int> visibleBuckets;
            for (int i = 0; i < list_->count(); ++i) {
                auto *item = list_->item(i);
                if (!item->data(Qt::UserRole).isValid()) continue;
                const int section = item->data(Qt::UserRole + 1).toInt();
                const bool sectionMatch = sections->currentRow() == 0
                    || (sections->currentRow() == geoSiteSections().size() + 1 ? item->data(Qt::UserRole + 2).toInt() == 2
                        : section == sections->currentRow() - 1);
                const bool topicMatch = topic.isEmpty() || (topic == "without" ? section >= 0 && item->data(Qt::UserRole + 5).toString().isEmpty()
                    : item->data(Qt::UserRole + 5).toString() == topic);
                const bool visible = !query.isEmpty() ? item->data(Qt::UserRole + 6).toString().contains(query, Qt::CaseInsensitive)
                    : count->isChecked() ? item->checkState() == Qt::Checked : sectionMatch && topicMatch;
                item->setHidden(!visible);
                if (visible) visibleBuckets.insert(item->data(Qt::UserRole + 2).toInt());
            }
            empty->setVisible(visibleBuckets.isEmpty());
            int bucket = -1;
            for (int i = 0; i < list_->count(); ++i) {
                auto *item = list_->item(i);
                if (!item->data(Qt::UserRole).isValid()) {
                    ++bucket;
                    // A catalog can omit any bucket; use the following row's bucket.
                    if (i + 1 < list_->count()) bucket = list_->item(i + 1)->data(Qt::UserRole + 2).toInt();
                    item->setHidden(!visibleBuckets.contains(bucket));
                }
            }
        };
        auto resetTopics = [sections, topics, count, filter] {
            const QSignalBlocker blocker(topics);
            topics->clear();
            topics->addItem(tr("All topics"), QString{});
            topics->addItem(tr("Without topic refinement"), "without");
            for (const auto &topic : geoSiteTopics())
                if (sections->currentRow() == 0 || topic.section == sections->currentRow() - 1)
                    topics->addItem(QObject::tr(topic.name), QString::fromUtf8(topic.id));
            topics->setEnabled(sections->currentRow() != geoSiteSections().size() + 1);
            count->setChecked(false);
            filter();
        };
        connect(search, &QLineEdit::textChanged, this, filter);
        connect(sections, &QListWidget::currentRowChanged, this, resetTopics);
        connect(topics, &QComboBox::currentIndexChanged, this, [count, filter] { count->setChecked(false); filter(); });
        connect(count, &QPushButton::toggled, this, filter);
        connect(list_, &QListWidget::itemChanged, this, filter);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        resetTopics(); search->setFocus();
    }
    QStringList selectedIds() const {
        QStringList checked;
        for (int i = 0; i < list_->count(); ++i) {
            const auto *item = list_->item(i);
            if (item->data(Qt::UserRole).isValid() && item->checkState() == Qt::Checked)
                checked.append(item->data(Qt::UserRole).toString());
        }
        QStringList result;
        for (const auto &id : original_) if (checked.contains(id) && !result.contains(id)) result.append(id);
        checked.sort();
        for (const auto &id : checked) if (!result.contains(id)) result.append(id);
        return result;
    }
private:
    QStringList original_;
    QListWidget *list_ = nullptr;
};
