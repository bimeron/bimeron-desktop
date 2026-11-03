#pragma once
#include "GeoSiteLabels.h"
#include "RoutingCompatibility.h"
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
#include <QToolButton>

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
        const QString idText = (index.data(Qt::UserRole + 7).toBool() ? "  " + key : " (" + key + ')') + suffix;
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

// Qt's model sort retains row identity, hidden state and persistent selection.
// Rank is cached once using the existing localized-name/ID comparison.
class GeoSiteListItem final : public QListWidgetItem {
public:
    using QListWidgetItem::QListWidgetItem;
    int rank = 0;
    int order = 0;
    bool operator<(const QListWidgetItem &other) const override {
        return order < static_cast<const GeoSiteListItem &>(other).order;
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
        search->setClearButtonEnabled(true);
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
        sections->setStyleSheet("QListWidget { border: 0; border-right: 1px solid #dce5ef; border-radius: 0; } QListWidget::item:disabled { color: #95a2b2; } QListWidget::item { min-height: 28px; padding: 2px 7px; border: 0; }");
        sections->addItem(tr("All groups"));
        for (const auto &section : geoSiteSections()) sections->addItem(QObject::tr(qPrintable(section)));
        sections->addItem(tr("Other"));
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
        QStringList groups = routingGroupRows(available, selected);
        const auto availableSet = routingGroupSet(available);
        const auto selectedSet = routingGroupSet(selected);
        QSet<int> populatedSections;
        for (const auto &id : availableSet) {
            const auto entry = geoSiteEntries().constFind(id);
            populatedSections.insert(entry == geoSiteEntries().cend() ? 9 : entry->section + 1);
        }
        // Retain fixed row identities used by navigation and AI ordering.
        for (int row = 1; row < sections->count(); ++row)
            sections->item(row)->setHidden(!populatedSections.contains(row));
        std::sort(groups.begin(), groups.end(), [](const QString &a, const QString &b) {
            const QString an = geoSiteName(a), bn = geoSiteName(b);
            const int compared = QString::localeAwareCompare(an.isEmpty() ? a : an, bn.isEmpty() ? b : bn);
            return compared == 0 ? a < b : compared < 0;
        });
        int rank = 0;
        // Rows retain identity while ordering changes; role +2 means unavailable.
        for (const auto &id : groups) {
            const QString name = geoSiteName(id);
            QString label = name.isEmpty() ? id : name + " (" + id + ')';
            if (!availableSet.contains(routingGroupIdentity(id))) label += " — " + tr("Unavailable");
            auto *item = new GeoSiteListItem(label, list_);
            item->rank = rank++;
            item->setToolTip(label);
            item->setData(Qt::UserRole + 3, name);
            item->setData(Qt::UserRole + 4, availableSet.contains(routingGroupIdentity(id)) ? QString{} : " — " + tr("Unavailable"));
            item->setData(Qt::UserRole, id);
            const auto it = geoSiteEntries().constFind(id);
            item->setData(Qt::UserRole + 1, it == geoSiteEntries().cend() ? -1 : it->section);
            item->setData(Qt::UserRole + 2, !availableSet.contains(routingGroupIdentity(id)));
            item->setData(Qt::UserRole + 5, it == geoSiteEntries().cend() || !it->topic ? QString{} : QString::fromUtf8(it->topic));
            item->setData(Qt::UserRole + 6, geoSiteSearchText(id));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(selectedSet.contains(routingGroupIdentity(id)) ? Qt::Checked : Qt::Unchecked);
        }
        // Keep all potential headings; visibility follows their matching rows.
        for (int section = 0; section < 10; ++section) {
            const QString title = section == 9 ? tr("Unavailable groups") : section == 8 ? tr("Other")
                : QObject::tr(qPrintable(geoSiteSections().at(section)));
            auto *heading = new GeoSiteListItem(title, list_);
            heading->setData(Qt::UserRole + 1, section);
            heading->setFlags(Qt::NoItemFlags);
            heading->setBackground(palette().alternateBase());
            QFont font = heading->font(); font.setBold(true); heading->setFont(font);
        }
        auto *empty = new QLabel(tr("No matching groups"));
        layout->addWidget(empty);
        auto *footer = new QHBoxLayout;
        auto *count = new QPushButton;
        count->setObjectName("geoSiteSelectionCount");
        count->setCheckable(true);
        count->setStyleSheet("QPushButton:checked { color: #1677ff; border-color: #1677ff; }");
        count->setAutoDefault(false);
        count->setToolTip(tr("Show only selected groups"));
        count->setAccessibleName(tr("Only selected"));
        footer->addWidget(count); footer->addStretch();
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        footer->addWidget(buttons); layout->addLayout(footer);
        auto filter = [this, search, sections, topics, count, empty] {
            const QSignalBlocker rowsBlocked(list_);
            const QSignalBlocker topicsBlocked(topics);
            const QString query = search->text().trimmed();
            const bool selectedOnly = count->isChecked();
            const bool global = !query.isEmpty() || selectedOnly;
            if (!sections->currentItem() || sections->currentItem()->isHidden()) {
                const QSignalBlocker sectionsBlocked(sections);
                sections->setCurrentRow(0);
            }
            const int sectionRow = sections->currentRow();
            if (!global && !globalMode_ && topicScope_ == sectionRow)
                browseTopic_ = topics->currentData().toString();
            if (global != globalMode_ || topicScope_ != sectionRow) {
                topics->clear();
                topics->addItem(tr("All topics"), QString{});
                if (!global) {
                    if (topicScope_ != sectionRow) browseTopic_.clear();
                    QSet<QString> present;
                    bool without = false;
                    for (int i = 0; i < list_->count(); ++i) {
                        const auto *item = list_->item(i);
                        if (!item->data(Qt::UserRole).isValid() || item->data(Qt::UserRole + 2).toBool()) continue;
                        const int section = item->data(Qt::UserRole + 1).toInt();
                        if (section < 0 || (sectionRow != 0 && section != sectionRow - 1)) continue;
                        const QString topic = item->data(Qt::UserRole + 5).toString();
                        if (topic.isEmpty()) without = true; else present.insert(topic);
                    }
                    if (!present.isEmpty()) {
                        for (const auto &topic : geoSiteTopics())
                            if (present.contains(QString::fromUtf8(topic.id)))
                                topics->addItem(QObject::tr(topic.name), QString::fromUtf8(topic.id));
                        if (without) topics->addItem(tr("Miscellaneous"), "without");
                    }
                    const int restored = topics->findData(browseTopic_);
                    topics->setCurrentIndex(restored < 0 ? 0 : restored);
                    browseTopic_ = topics->currentData().toString();
                }
                topicScope_ = sectionRow;
                globalMode_ = global;
            }
            sections->setEnabled(!global);
            topics->setEnabled(!global && topics->count() > 1);
            const QString scopeTip = global ? tr("Search and Only selected include all sections and topics") : QString{};
            sections->setToolTip(scopeTip); topics->setToolTip(scopeTip);
            search->setPlaceholderText(selectedOnly ? tr("Search selected names or IDs") : tr("Search names or IDs"));
            search->setAccessibleName(search->placeholderText());
            count->setToolTip(tr("Show only selected groups; typing searches within the selection"));
            const QString topic = topics->currentData().toString();
            const int mode = !query.isEmpty() ? 2 : !selectedOnly && (sectionRow == 7 || topic == "4-ai") ? 1 : 0;
            if (mode != orderingMode_) {
                const int stride = list_->count() + 1;
                for (int i = 0; i < list_->count(); ++i) {
                    auto *item = static_cast<GeoSiteListItem *>(list_->item(i));
                    if (!item->data(Qt::UserRole).isValid()) {
                        const int section = item->data(Qt::UserRole + 1).toInt();
                        item->order = mode == 2 || section == 9 ? (section + 1) * stride : -1;
                        continue;
                    }
                    const bool unavailable = item->data(Qt::UserRole + 2).toBool();
                    const int section = item->data(Qt::UserRole + 1).toInt();
                    const QString id = item->data(Qt::UserRole).toString();
                    if (unavailable) item->order = 10 * stride + item->rank + 1;
                    else if (mode == 2) item->order = (section < 0 ? 9 : section + 1) * stride + item->rank + 1;
                    else if (mode == 1) item->order = id == "openai" ? 0 : id == "anthropic" ? 1 : id == "cursor" ? 2 : item->rank + 3;
                    else item->order = item->rank + 1;
                }
                list_->sortItems();
                orderingMode_ = mode;
            }
            int visibleCount = 0, checkedCount = 0;
            QListWidgetItem *heading = nullptr;
            for (int i = 0; i < list_->count(); ++i) {
                auto *item = list_->item(i);
                if (!item->data(Qt::UserRole).isValid()) {
                    item->setHidden(true);
                    heading = mode == 2 || item->data(Qt::UserRole + 1).toInt() == 9 ? item : nullptr;
                    continue;
                }
                const int section = item->data(Qt::UserRole + 1).toInt();
                const bool checked = item->checkState() == Qt::Checked;
                checkedCount += checked;
                const bool sectionMatch = sectionRow == 0 || (sectionRow == 9 ? section < 0 && !item->data(Qt::UserRole + 2).toBool() : section == sectionRow - 1);
                const bool topicMatch = topic.isEmpty() || (topic == "without" ? section >= 0 && item->data(Qt::UserRole + 5).toString().isEmpty()
                    : item->data(Qt::UserRole + 5).toString() == topic);
                const bool visible = (!selectedOnly || checked)
                    && (query.isEmpty() || item->data(Qt::UserRole + 6).toString().contains(query, Qt::CaseInsensitive))
                    && (global || (sectionMatch && topicMatch));
                item->setHidden(!visible);
                if (visible) { ++visibleCount; if (heading) heading->setHidden(false); }
            }
            count->setText((selectedOnly ? tr("Selected: %1 · Only selected") : tr("Selected: %1")).arg(checkedCount));
            empty->setText(selectedOnly ? (query.isEmpty() ? tr("No selected groups") : tr("No matching selected groups")) : tr("No matching groups"));
            empty->setVisible(visibleCount == 0);
        };
        for (auto *button : search->findChildren<QToolButton *>()) {
            button->setToolTip(tr("Clear search"));
            button->setAccessibleName(tr("Clear search"));
            button->setFocusPolicy(Qt::StrongFocus);
            connect(button, &QToolButton::clicked, search, [search] { search->setFocus(); });
        }
        connect(search, &QLineEdit::textChanged, this, filter);
        connect(sections, &QListWidget::currentRowChanged, this, filter);
        connect(topics, &QComboBox::currentIndexChanged, this, filter);
        connect(count, &QPushButton::toggled, this, filter);
        connect(list_, &QListWidget::itemChanged, this, filter);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        filter(); search->setFocus();
    }
    QStringList selectedIds() const {
        QStringList checked;
        for (int i = 0; i < list_->count(); ++i) {
            const auto *item = list_->item(i);
            if (item->data(Qt::UserRole).isValid() && item->checkState() == Qt::Checked)
                checked.append(item->data(Qt::UserRole).toString());
        }
        return routingRetainedGroups(checked, original_);
    }
private:
    QString browseTopic_;
    int topicScope_ = -1;
    int orderingMode_ = -1;
    bool globalMode_ = false;
    QStringList original_;
    QListWidget *list_ = nullptr;
};
