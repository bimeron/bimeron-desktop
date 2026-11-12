#include "GeoDataDialog.h"
#include "GeoDataDownload.h"
#include "GeoDataSelection.h"
#include "GeoDataUpdater.h"
#include "HiDpiCheckBox.h"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSet>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <functional>

namespace {
using Failure = GeoDataSelection::CatalogFailure;

// Plain-text label that elides in the middle, so long file names never widen the dialog.
class ElidedLabel final : public QLabel {
public:
    ElidedLabel() {
        setTextFormat(Qt::PlainText);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }
    void setFullText(const QString &text) { full_ = text; elide(); }
    QString fullText() const { return full_; }
protected:
    void resizeEvent(QResizeEvent *event) override { QLabel::resizeEvent(event); elide(); }
private:
    void elide() { QLabel::setText(fontMetrics().elidedText(full_, Qt::ElideMiddle, qMax(0, contentsRect().width()))); }
    QString full_;
};

enum class Purpose { Passive, Recheck, Manual, Downloaded };

// The draft selection (path and its verified status) is kept apart from the latest
// replacement attempt, so a failed download or rejected file never hides the current file.
struct Row {
    GeoDataSelection::Source source = GeoDataSelection::Source::None;
    QString path, tag;
    bool valid = false, checking = false;
    QString status, facts, provenance;
    int groups = -1;
    bool busy = false, transfer = false;
    QString attemptStatus, attemptLabel, attemptSummary, attemptError, pendingProvenance;
    int pendingGroups = -1;
    QString pendingTag;
    int generation = 0;
};

struct RowWidgets {
    ElidedLabel *file = nullptr;
    QLabel *status = nullptr;
    QProgressBar *bar = nullptr;
    QPushButton *cancel = nullptr, *actions = nullptr;
    QAction *update = nullptr, *install = nullptr, *chooseAction = nullptr, *recheck = nullptr, *clear = nullptr;
};
}

void manageSharedGeoData(QWidget *parent, const QString &core, const QString &profilePath,
                         const QJsonArray &rules, GeoDataDownload *transport) {
    const auto tr = [](const char *s) { return QObject::tr(s); };
    const QStringList kinds{QStringLiteral("geoip"), QStringLiteral("geosite")};
    const auto kindName = [&](const QString &kind) { return kind == "geoip" ? tr("Countries") : tr("Sites"); };
    QDialog dialog(parent);
    dialog.setObjectName("sharedGeoDataDialog");
    dialog.setWindowTitle(tr("Country and site databases"));
    dialog.setWindowIcon(parent->windowIcon());
    dialog.setMinimumWidth(460);
    const bool dark = dialog.palette().color(QPalette::Window).lightness() < 128;
    const QString muted = QStringLiteral("color: %1").arg(dark ? "#aab8ca" : "#718096");
    const QString failed = QStringLiteral("color: %1").arg(dark ? "#ff9a92" : "#b42318");
    const QString linkColor = dark ? "#8ab4ff" : "#1267d6";
    const QString textColor = dark ? "#e8edf5" : "#152238";
    auto *layout = new QVBoxLayout(&dialog);
    layout->setSpacing(8);
    auto *scope = new QLabel(tr("These databases serve country and site rules in all profiles."));
    scope->setWordWrap(true);
    layout->addWidget(scope);
    // Bimeron ships these third-party databases; attribute them to their author.
    const QString link = QString("<a style=\"color:%1\" href=\"https://github.com/Loyalsoldier/v2ray-rules-dat\">Loyalsoldier · GitHub</a>").arg(linkColor);
    auto *source = new QLabel(tr("By default, the databases come from %1 (GPL-3.0)").arg(link));
    source->setObjectName("geodataSource");
    source->setStyleSheet(muted);
    source->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    source->setFocusPolicy(Qt::StrongFocus);
    source->setOpenExternalLinks(true);
    source->setWordWrap(true);
    layout->addWidget(source);
    auto *autoUpdate = new HiDpiCheckBox(tr("Update Loyalsoldier databases automatically"));
    autoUpdate->setObjectName("geodataAutoUpdate");
    auto *autoUpdateHint = new QLabel;
    autoUpdateHint->setObjectName("geodataAutoUpdateHint");
    autoUpdateHint->setTextFormat(Qt::PlainText);
    autoUpdateHint->setWordWrap(true);
    autoUpdateHint->setContentsMargins(26, 0, 0, 0);
    layout->addSpacing(2);
    layout->addWidget(autoUpdate);
    layout->addWidget(autoUpdateHint);
    layout->addSpacing(2);

    auto *table = new QTableWidget(2, 3);
    table->setObjectName("geodataTable");
    table->setHorizontalHeaderLabels({tr("Database"), tr("File"), QString()});
    table->verticalHeader()->hide();
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setFocusPolicy(Qt::NoFocus);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setWordWrap(false);
    auto *header = table->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    // Sized in fitHeight(): row actions change (Download, Cancel, the row menu) after layout.
    header->setSectionResizeMode(2, QHeaderView::Fixed);
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setHighlightSections(false);
    header->setSectionsClickable(false);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(table);

    auto *message = new QLabel;
    message->setObjectName("geodataMessage");
    message->setWordWrap(true);
    message->setTextFormat(Qt::PlainText);
    message->setStyleSheet(failed);
    message->hide();
    layout->addWidget(message);
    auto *toggle = new QToolButton;
    toggle->setObjectName("geodataDetails");
    toggle->setCheckable(true);
    toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggle->setIcon(QIcon(":/icons/chevron-down.xpm"));
    toggle->setIconSize(QSize(9, 5));
    // A plain disclosure: no button surface, focus shown by the link color.
    toggle->setStyleSheet(QString("QToolButton { border: 0; background: transparent; padding: 2px 0; color: %1; }"
                                  " QToolButton:focus, QToolButton:hover { color: %2; }").arg(textColor, linkColor));
    layout->addWidget(toggle, 0, Qt::AlignLeft);
    auto *information = new QPlainTextEdit;
    information->setObjectName("geodataInformation");
    information->setReadOnly(true);
    information->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    information->setMinimumHeight(80);
    information->setMaximumHeight(170);
    information->hide();
    layout->addWidget(information);
    layout->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    auto *installAll = buttons->addButton(tr("Install Loyalsoldier databases"), QDialogButtonBox::ResetRole);
    installAll->setObjectName("geodataInstallAll");
    installAll->setAutoDefault(false);
    layout->addWidget(buttons);

    auto *selection = GeoDataSelection::instance();
    const GeoDataSelection::Selection original = selection->selection();
    // One app-owned transport, used only by this modal manager; close cancels its jobs.
    static auto *sharedDownload = new GeoDataDownload(qApp);
    auto *download = transport ? transport : sharedDownload;
    QMap<QString, Row> rows;
    QMap<QString, RowWidgets> ui;
    QMap<QString, QString> ownedCatalogs;
    QMap<QString, QMetaObject::Connection> catalogConnections;
    QSet<QString> ownedDownloads;
    QString applyError;
    for (const QString &kind : kinds) {
        rows[kind].path = original.paths.value(kind);
        rows[kind].tag = original.tags.value(kind);
        rows[kind].source = rows[kind].path.isEmpty() ? GeoDataSelection::Source::None : original.sources.value(kind);
    }
    autoUpdate->setChecked(original.autoUpdate);
    using Source = GeoDataSelection::Source;

    // Relative selections resolve against the profile being edited; without one, report it.
    const auto resolve = [&](const QString &path) {
        if (QDir::isAbsolutePath(path)) return QFileInfo(path).absoluteFilePath();
        return profilePath.isEmpty() ? QString() : QFileInfo(profilePath).dir().absoluteFilePath(path);
    };
    const auto draft = [&] {
        GeoDataSelection::Selection result;
        result.autoUpdate = autoUpdate->isChecked();
        for (const QString &kind : kinds) {
            result.paths[kind] = rows[kind].path;
            result.sources[kind] = rows[kind].path.isEmpty() ? Source::None : rows[kind].source;
            result.tags[kind] = result.sources[kind] == Source::Loyalsoldier ? rows[kind].tag : QString();
        }
        return result;
    };
    const auto changedFromOriginal = [&] {
        const auto current = draft();
        for (const QString &kind : kinds) {
            const Source before = original.paths.value(kind).isEmpty() ? Source::None : original.sources.value(kind);
            if (current.paths.value(kind) != original.paths.value(kind) || current.sources.value(kind) != before) return true;
        }
        return current.autoUpdate != original.autoUpdate;
    };
    // The status line names the source in words; badges would read as branding.
    const auto lowered = [](QString text) { if (!text.isEmpty()) text[0] = text[0].toLower(); return text; };
    const auto requiredGroups = [&](const QString &kind) {
        QStringList ids;
        for (const auto &value : rules) {
            const auto rule = value.toObject();
            if (rule.value("enabled").toBool() && rule.value("matcher").toString() == (kind == "geoip" ? "GeoIP" : "GeoSite"))
                ids.append(rule.value("value").toString().split(',', Qt::SkipEmptyParts));
        }
        return ids;
    };
    const auto informationText = [&] {
        QStringList blocks;
        for (const QString &kind : kinds) {
            const Row &row = rows[kind];
            if (row.path.isEmpty() && row.attemptError.isEmpty()) continue;
            QStringList lines{kind == "geoip" ? tr("Country database") : tr("Site database")};
            if (!row.path.isEmpty()) {
                const QString absolute = resolve(row.path);
                lines << tr("Source: %1").arg(row.source == Source::Loyalsoldier
                    ? (row.tag.isEmpty() ? QStringLiteral("Loyalsoldier")
                                         : tr("Loyalsoldier, release %1").arg(GeoDataSelection::releaseDate(row.tag)))
                    : tr("own file"));
                lines << tr("Path: %1").arg(QDir::toNativeSeparators(absolute.isEmpty() ? row.path : absolute));
                QString check = row.checking ? tr("Checking…") : row.status;
                if (row.valid && row.groups >= 0) check += " · " + tr("%1 groups").arg(row.groups);
                lines << tr("Status: %1").arg(check);
                if (!row.facts.isEmpty()) lines << row.facts;
                if (row.valid) lines << tr("Checked locally; this does not confirm that the service can read the file.");
                if (!row.provenance.isEmpty()) lines << row.provenance;
            }
            if (!row.attemptError.isEmpty()) lines << tr("Last attempt: %1").arg(row.attemptSummary) << row.attemptError;
            blocks << lines.join('\n');
        }
        if (!applyError.isEmpty()) blocks << tr("Apply: %1").arg(applyError);
        return blocks.join("\n\n");
    };
    const auto fitHeight = [&] {
        int actionsWidth = 0;
        for (int row = 0; row < table->rowCount(); ++row) {
            // Visibility changes only post a layout request; measure the current buttons now.
            QLayout *actions = table->cellWidget(row, 2)->layout();
            actions->invalidate();
            actionsWidth = qMax(actionsWidth, actions->sizeHint().width());
        }
        header->resizeSection(2, actionsWidth + (table->showGrid() ? 1 : 0)); // The grid line takes one pixel.
        table->resizeRowsToContents();
        int height = header->sizeHint().height() + 2 * table->frameWidth();
        for (int row = 0; row < table->rowCount(); ++row) height += table->rowHeight(row);
        table->setFixedHeight(height);
        if (dialog.isVisible()) dialog.resize(dialog.width(), dialog.sizeHint().height());
    };
    const auto refresh = [&] {
        bool busy = false, ready = true, anyPath = false, anyError = !applyError.isEmpty();
        QString attemptMessage;
        for (const QString &kind : kinds) {
            const Row &row = rows[kind];
            RowWidgets &w = ui[kind];
            const bool empty = row.path.isEmpty();
            busy |= row.busy || row.checking;
            ready &= empty || row.valid;
            anyPath |= !empty;
            anyError |= !row.attemptError.isEmpty();
            // Empty rows show the failed attempt in their own status line; avoid repeating it.
            if (!empty && !row.attemptSummary.isEmpty() && attemptMessage.isEmpty()) attemptMessage = kindName(kind) + QStringLiteral(" · ") + row.attemptSummary;
            w.file->setFullText(empty ? tr("Not used") : QFileInfo(row.path).fileName());
            w.file->setStyleSheet(empty ? muted : QString());
            const QString absolute = empty ? QString() : resolve(row.path);
            w.file->setToolTip(empty ? QString() : QDir::toNativeSeparators(absolute.isEmpty() ? row.path : absolute));
            QString status;
            bool problem = false;
            if (row.transfer || row.busy) status = row.attemptStatus;
            else if (empty) { status = row.attemptLabel; problem = !status.isEmpty(); }
            else if (row.checking) status = tr("Checking…");
            else { status = row.status; problem = !row.valid; }
            // Raw status for tests and accessibility; the visible line also names the source.
            w.status->setProperty("status", status);
            QStringList line;
            if (!empty && !row.transfer && !row.busy) {
                if (row.source == Source::Loyalsoldier) {
                    line << QStringLiteral("Loyalsoldier");
                    if (!row.tag.isEmpty()) line << tr("release %1").arg(GeoDataSelection::releaseDate(row.tag));
                } else line << tr("Own file");
                if (!status.isEmpty()) line << lowered(status);
            } else if (!status.isEmpty()) line << status;
            w.status->setText(line.join(QStringLiteral(" · ")));
            w.status->setVisible(!line.isEmpty());
            w.status->setStyleSheet(problem ? failed : muted);
            w.bar->setVisible(row.transfer);
            w.cancel->setVisible(row.transfer);
            w.actions->setVisible(!row.transfer);
            w.actions->setEnabled(!row.busy && !row.checking);
            w.update->setVisible(!empty && row.source == Source::Loyalsoldier);
            w.install->setVisible(empty || row.source != Source::Loyalsoldier);
            w.chooseAction->setText(row.source == Source::Custom && !empty ? tr("Choose another file…") : tr("Choose own file…"));
            w.recheck->setVisible(!empty);
            w.recheck->setEnabled(!absolute.isEmpty());
            w.clear->setVisible(!empty);
        }
        bool loyalsoldier = false, everythingDefault = autoUpdate->isChecked();
        for (const QString &kind : kinds) {
            const bool uses = !rows[kind].path.isEmpty() && rows[kind].source == Source::Loyalsoldier;
            loyalsoldier |= uses;
            everythingDefault &= uses;
        }
        installAll->setEnabled(!busy && !everythingDefault);
        QString hint;
        bool hintProblem = false;
        if (!loyalsoldier) hint = tr("Nothing to update: no Loyalsoldier databases are selected.");
        else if (GeoDataUpdater::lastCheckFailed()) { hint = tr("Last check failed. It will be retried later."); hintProblem = true; }
        else if (GeoDataUpdater::lastCheck().isValid())
            hint = tr("Last checked %1").arg(QLocale().toString(GeoDataUpdater::lastCheck().toLocalTime(), QLocale::ShortFormat));
        autoUpdateHint->setText(hint);
        autoUpdateHint->setVisible(autoUpdate->isChecked() && !hint.isEmpty());
        autoUpdateHint->setStyleSheet(hintProblem ? failed : muted);
        buttons->button(QDialogButtonBox::Apply)->setEnabled(!busy && ready && applyError.isEmpty() && changedFromOriginal());
        const QString shown = !applyError.isEmpty() ? applyError.left(180) : attemptMessage;
        message->setText(shown);
        message->setVisible(!shown.isEmpty());
        toggle->setVisible(anyPath || anyError);
        toggle->setText(anyPath ? tr("File information") : tr("Error details"));
        toggle->setAccessibleName(toggle->text());
        if (toggle->isHidden() && toggle->isChecked()) toggle->setChecked(false);
        information->setPlainText(informationText());
        fitHeight();
    };

    std::function<void(const QString &, const QString &, Purpose)> inspect;
    inspect = [&](const QString &kind, const QString &path, Purpose purpose) {
        Row &row = rows[kind];
        applyError.clear();
        const int generation = ++row.generation;
        const bool candidate = purpose == Purpose::Manual || purpose == Purpose::Downloaded;
        if (candidate) {
            row.busy = true;
            row.attemptStatus = purpose == Purpose::Manual ? tr("Checking chosen file…") : tr("Checking downloaded file…");
        } else {
            row.checking = true;
            row.valid = false;
        }
        const QString absolute = resolve(path);
        if (absolute.isEmpty()) {
            row.checking = row.busy = false;
            row.status = tr("Cannot resolve path");
            row.facts = tr("Cause: this relative path is resolved against a profile. Open the manager from the profile editor to check it.");
            refresh();
            return;
        }
        if (purpose == Purpose::Downloaded) {
            row.transfer = true;
            ui[kind].bar->setRange(0, 0);
        }
        refresh();
        const QString key = GeoDataSelection::catalogKey(core, kind, absolute);
        if (purpose == Purpose::Downloaded) ownedCatalogs[kind] = key;
        QObject::disconnect(catalogConnections.take(kind));
        const auto resolved = [&, key, kind, path, purpose, candidate, generation](
                const QString &resultKey, const QStringList &groups, const QString &diagnostic, Failure failure) {
            if (key != resultKey || rows[kind].generation != generation) return;
            Row &row = rows[kind];
            QObject::disconnect(catalogConnections.take(kind));
            ownedCatalogs.remove(kind);
            if (purpose == Purpose::Downloaded)
                QTimer::singleShot(0, selection, [selection, key] { selection->cancelCatalog(key); });
            QString status = GeoDataSelection::failureStatus(failure), facts = diagnostic;
            bool success = failure == Failure::None;
            if (success) for (const QString &id : requiredGroups(kind)) if (!groups.contains(id.trimmed().toLower())) {
                const QString safeId = QRegularExpression("^[a-zA-Z0-9_!-]{1,256}$").match(id.trimmed()).hasMatch()
                    ? id.trimmed() : tr("invalid identifier");
                status = tr("Missing group: %1").arg(safeId);
                facts = tr("Cause: an active rule uses a group that this database does not contain.");
                success = false;
                break;
            }
            if (!success && failure != Failure::None) facts = GeoDataSelection::failureExplanation(failure) + '\n' + facts;
            if (!candidate) {
                row.checking = false;
                row.valid = success;
                row.status = status;
                row.facts = success ? QString() : facts;
                row.groups = success ? int(groups.size()) : -1;
            } else if (!success) {
                // A rejected replacement leaves the current selection and its status untouched.
                if (purpose == Purpose::Downloaded && ownedDownloads.contains(kind)) download->cancel(kind);
                row.busy = row.transfer = false;
                row.attemptLabel = purpose == Purpose::Manual ? tr("File not accepted") : tr("Download failed");
                row.attemptSummary = purpose == Purpose::Manual ? tr("Chosen file was not accepted: %1").arg(status)
                    : (row.path.isEmpty() ? tr("Download failed: %1") : tr("Update failed: %1")).arg(status);
                row.attemptError = facts;
            } else if (purpose == Purpose::Downloaded) {
                row.pendingGroups = int(groups.size());
                row.attemptStatus = tr("Saving…");
                refresh();
                download->publishAsync(kind);
                return;
            } else {
                row.busy = false;
                row.path = path;
                row.source = Source::Custom;
                row.tag.clear();
                row.valid = true;
                row.status = status;
                row.facts.clear();
                row.provenance.clear();
                row.groups = int(groups.size());
                row.attemptLabel.clear();
                row.attemptSummary.clear();
                row.attemptError.clear();
            }
            refresh();
        };
        QStringList cachedGroups;
        QString cachedError;
        Failure cachedFailure = Failure::None;
        if (purpose == Purpose::Passive
            && selection->cachedCatalog(core, kind, absolute, &cachedGroups, &cachedError, &cachedFailure)) {
            resolved(key, cachedGroups, cachedError, cachedFailure);
        } else {
            catalogConnections[kind] = QObject::connect(selection, &GeoDataSelection::catalogResult, &dialog, resolved);
            selection->requestCatalog(core, kind, absolute, purpose != Purpose::Passive);
        }
    };
    const auto startDownload = [&](const QString &kind) {
        Row &row = rows[kind];
        if (row.busy || row.checking || download->busy(kind)) return;
        applyError.clear();
        row.attemptLabel.clear();
        row.attemptSummary.clear();
        row.attemptError.clear();
        row.pendingProvenance.clear();
        row.busy = row.transfer = true;
        row.attemptStatus = row.path.isEmpty() ? tr("Downloading…") : tr("Updating…");
        ownedDownloads.insert(kind);
        ui[kind].bar->setRange(0, 0);
        refresh();
        download->start(kind);
    };
    // Restores the shipped (or current managed) Loyalsoldier copy; downloads when none exists.
    const auto installLoyalsoldier = [&](const QString &kind) {
        Row &row = rows[kind];
        if (row.busy || row.checking) return;
        autoUpdate->setChecked(true);
        if (!row.path.isEmpty() && row.source == Source::Loyalsoldier) { refresh(); return; }
        const auto release = selection->loyalsoldierRelease(kind);
        if (release.path.isEmpty()) { startDownload(kind); return; }
        applyError.clear();
        row.attemptLabel.clear();
        row.attemptSummary.clear();
        row.attemptError.clear();
        row.source = Source::Loyalsoldier;
        row.path = release.path;
        row.tag = release.tag;
        row.provenance.clear();
        inspect(kind, row.path, Purpose::Passive);
    };
    const auto chooseFile = [&](const QString &kind) {
        const QString path = QFileDialog::getOpenFileName(&dialog, tr("Choose local database"));
        if (path.isEmpty()) return;
        rows[kind].attemptLabel.clear();
        rows[kind].attemptSummary.clear();
        rows[kind].attemptError.clear();
        inspect(kind, QFileInfo(path).absoluteFilePath(), Purpose::Manual);
    };

    for (int index = 0; index < kinds.size(); ++index) {
        const QString kind = kinds[index];
        RowWidgets &w = ui[kind];
        auto *name = new QTableWidgetItem(kindName(kind));
        name->setFlags(Qt::ItemIsEnabled);
        table->setItem(index, 0, name);

        auto *fileCell = new QWidget;
        auto *fileLayout = new QVBoxLayout(fileCell);
        fileLayout->setContentsMargins(8, 6, 8, 6);
        fileLayout->setSpacing(2);
        w.file = new ElidedLabel;
        w.file->setObjectName(kind + "File");
        w.status = new QLabel;
        w.status->setObjectName(kind + "Status");
        w.status->setTextFormat(Qt::PlainText);
        w.status->setWordWrap(true);
        w.status->setAccessibleName(kindName(kind) + ' ' + tr("Status"));
        w.bar = new QProgressBar;
        w.bar->setObjectName(kind + "Progress");
        w.bar->setTextVisible(false);
        w.bar->setFixedHeight(6);
        w.bar->hide();
        fileLayout->addWidget(w.file);
        fileLayout->addWidget(w.status);
        fileLayout->addWidget(w.bar);
        table->setCellWidget(index, 1, fileCell);

        auto *actionCell = new QWidget;
        auto *actionLayout = new QHBoxLayout(actionCell);
        actionLayout->setContentsMargins(6, 4, 8, 4);
        actionLayout->setSpacing(6);
        actionLayout->addStretch();
        w.cancel = new QPushButton(tr("Cancel"));
        w.cancel->setObjectName(kind + "Cancel");
        w.actions = new QPushButton(tr("Actions"));
        w.actions->setObjectName(kind + "Actions");
        // The application's secondary button with the product chevron instead of a glyph.
        w.actions->setStyleSheet("QPushButton { padding: 0 26px 0 10px; }"
                                 " QPushButton::menu-indicator { image: url(:/icons/chevron-down.xpm); width: 9px; height: 5px;"
                                 " subcontrol-origin: padding; subcontrol-position: right center; right: 10px; }");
        auto *menu = new QMenu(w.actions);
        w.update = menu->addAction(tr("Update now"));
        w.update->setObjectName(kind + "Update");
        w.install = menu->addAction(tr("Install Loyalsoldier database"));
        w.install->setObjectName(kind + "Install");
        w.chooseAction = menu->addAction(tr("Choose own file…"));
        w.chooseAction->setObjectName(kind + "ChooseAction");
        w.recheck = menu->addAction(tr("Check again"));
        w.recheck->setObjectName(kind + "Recheck");
        menu->addSeparator();
        w.clear = menu->addAction(tr("Disable database"));
        w.clear->setObjectName(kind + "Clear");
        w.actions->setMenu(menu);
        for (auto *button : {w.cancel, w.actions}) {
            button->setAutoDefault(false);
            button->setAccessibleName(button->text() + ' ' + kindName(kind));
            actionLayout->addWidget(button);
        }
        w.actions->setAccessibleName(tr("Actions for %1").arg(kindName(kind)));
        w.actions->setToolTip(w.actions->accessibleName());
        w.cancel->hide();
        table->setCellWidget(index, 2, actionCell);

        QObject::connect(w.update, &QAction::triggered, &dialog, [&, kind] { startDownload(kind); });
        QObject::connect(w.install, &QAction::triggered, &dialog, [&, kind] { installLoyalsoldier(kind); });
        QObject::connect(w.chooseAction, &QAction::triggered, &dialog, [&, kind] { chooseFile(kind); });
        QObject::connect(w.recheck, &QAction::triggered, &dialog, [&, kind] {
            if (!rows[kind].path.isEmpty() && !rows[kind].busy) inspect(kind, rows[kind].path, Purpose::Recheck);
        });
        // Disabling changes only the draft selection; no file is deleted.
        QObject::connect(w.clear, &QAction::triggered, &dialog, [&, kind] {
            Row &row = rows[kind];
            if (row.busy || row.checking) return;
            applyError.clear();
            const int generation = row.generation + 1;
            row = Row{};
            row.generation = generation;
            refresh();
        });
        QObject::connect(w.cancel, &QPushButton::clicked, &dialog, [&, kind] {
            Row &row = rows[kind];
            ++row.generation;
            QObject::disconnect(catalogConnections.take(kind));
            if (ownedCatalogs.contains(kind)) selection->cancelCatalog(ownedCatalogs.take(kind));
            if (ownedDownloads.contains(kind)) download->cancel(kind);
            row.busy = row.transfer = false;
            row.attemptStatus.clear();
            refresh();
        });
    }
    QObject::connect(toggle, &QToolButton::toggled, &dialog, [&](bool open) {
        toggle->setIcon(QIcon(open ? ":/icons/chevron-up.xpm" : ":/icons/chevron-down.xpm"));
        information->setVisible(open);
        dialog.resize(dialog.width(), dialog.sizeHint().height());
    });
    QObject::connect(download, &GeoDataDownload::progress, &dialog, [&](const QString &kind, qint64 received, qint64 total) {
        if (!ownedDownloads.contains(kind) || !rows[kind].transfer) return;
        ui[kind].bar->setRange(0, total > 0 ? 100 : 0);
        if (total > 0) ui[kind].bar->setValue(int(received * 100 / total));
    });
    QObject::connect(download, &GeoDataDownload::candidateReady, &dialog, [&](const QString &kind, const QString &path, const QString &hash) {
        if (!ownedDownloads.contains(kind) || !rows[kind].transfer) return;
        rows[kind].pendingProvenance = tr("Downloaded release: %1").arg(download->releaseTag(kind)) + "\nSHA-256: " + hash;
        inspect(kind, path, Purpose::Downloaded);
    });
    QObject::connect(download, &GeoDataDownload::published, &dialog, [&](const QString &kind, const QString &path, const QString &tag) {
        if (!ownedDownloads.contains(kind) || !rows[kind].transfer) return;
        Row &row = rows[kind];
        row.busy = row.transfer = false;
        row.path = path;
        row.source = Source::Loyalsoldier;
        row.tag = tag;
        row.valid = true;
        row.status = GeoDataSelection::failureStatus(Failure::None);
        row.facts.clear();
        row.groups = row.pendingGroups;
        row.provenance = row.pendingProvenance;
        row.attemptLabel.clear();
        row.attemptSummary.clear();
        row.attemptError.clear();
        refresh();
    });
    QObject::connect(download, &GeoDataDownload::failed, &dialog, [&](const QString &kind, const QString &error) {
        if (!ownedDownloads.contains(kind) || !rows[kind].transfer) return;
        Row &row = rows[kind];
        row.busy = row.transfer = false;
        row.attemptLabel = tr("Download failed");
        row.attemptSummary = row.path.isEmpty() ? tr("Download failed") : tr("Update failed. The current file is kept.");
        row.attemptError = error;
        refresh();
    });
    QObject::connect(selection, &GeoDataSelection::catalogInvalidated, &dialog, [&](const QString &key) {
        for (const QString &kind : kinds) {
            const Row &row = rows[kind];
            const QString absolute = row.path.isEmpty() ? QString() : resolve(row.path);
            if (absolute.isEmpty() || row.busy || row.checking || GeoDataSelection::catalogKey(core, kind, absolute) != key) continue;
            inspect(kind, row.path, Purpose::Passive);
        }
    });
    QObject::connect(autoUpdate, &QCheckBox::toggled, &dialog, [&] { applyError.clear(); refresh(); });
    QObject::connect(installAll, &QPushButton::clicked, &dialog, [&] {
        autoUpdate->setChecked(true);
        for (const QString &kind : kinds) installLoyalsoldier(kind);
        refresh();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, &dialog, [&] {
        // A disabled database is allowed: profiles that need it are blocked when connecting.
        QString error;
        const auto profiles = ProfileStore().load(&error);
        if (error.isEmpty() && selection->apply(draft(), core, profiles, &error)) dialog.accept();
        else { applyError = error; refresh(); }
    });
    for (const QString &kind : kinds) if (!rows[kind].path.isEmpty()) inspect(kind, rows[kind].path, Purpose::Passive);
    refresh();
    dialog.resize(520, dialog.sizeHint().height());
    dialog.exec();
    QObject::disconnect(selection, nullptr, &dialog, nullptr);
    QObject::disconnect(download, nullptr, &dialog, nullptr);
    for (const QString &kind : kinds) {
        ++rows[kind].generation;
        if (ownedCatalogs.contains(kind)) selection->cancelCatalog(ownedCatalogs.take(kind));
        if (ownedDownloads.contains(kind)) download->cancel(kind);
    }
    for (auto *process : dialog.findChildren<QProcess *>()) {
        process->disconnect(&dialog); process->kill(); process->waitForFinished(1000);
    }
}
