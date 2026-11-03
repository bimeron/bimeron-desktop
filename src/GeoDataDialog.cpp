#include "GeoDataDialog.h"
#include "GeoDataDownload.h"
#include "GeoDataSelection.h"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QRegularExpression>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QSet>
#include <QToolButton>
#include <QVBoxLayout>
#include <memory>
#include <functional>

void manageSharedGeoData(QWidget *parent, const QString &core, const QString &profilePath,
                         const QJsonArray &rules, GeoDataDownload *transport) {
    const auto tr = [](const char *s) { return QObject::tr(s); };
    QDialog dialog(parent);
    dialog.setObjectName("sharedGeoDataDialog");
    dialog.setWindowTitle(tr("Country and site databases"));
    dialog.setWindowIcon(parent->windowIcon());
    dialog.resize(480, 300);
    dialog.setMinimumWidth(440);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setSpacing(8);
    auto *intro = new QLabel(tr("Download ready-to-use databases or choose your own files."));
    intro->setWordWrap(true); layout->addWidget(intro);
    const QString linkColor = dialog.palette().color(QPalette::Window).lightness() < 128 ? "#8ab4ff" : "#1267d6";
    auto *source = new QLabel(QString("<a style=\"color:%1\" href=\"https://github.com/Loyalsoldier/v2ray-rules-dat\">Loyalsoldier · GitHub</a>").arg(linkColor));
    source->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    source->setFocusPolicy(Qt::StrongFocus);
    source->setOpenExternalLinks(true); layout->addWidget(source);
    auto *selection = GeoDataSelection::instance();
    auto paths = selection->paths();
    const auto original = paths;
    // One app-owned transport, used only by this modal manager; close cancels its jobs.
    static auto *sharedDownload = new GeoDataDownload(qApp);
    auto *download = transport ? transport : sharedDownload;
    QMap<QString, QLabel *> statuses;
    QMap<QString, QHBoxLayout *> statusRows;
    QMap<QString, QProgressBar *> progress;
    QMap<QString, QPushButton *> downloads, choices, cancellations;
    QMap<QString, QToolButton *> errors;
    QMap<QString, QString> ownedCatalogs;
    QMap<QString, QMetaObject::Connection> catalogConnections;
    QMap<QString, bool> pending, valid;
    enum class Inspection { Passive, Manual, Downloaded };
    QMap<QString, bool> transfers;
    QSet<QString> ownedDownloads;
    QMap<QString, int> generations;
    QString applyError;
    QMap<QString, QString> diagnostics, sourceDetails;
    auto *details = new QPlainTextEdit;
    details->setReadOnly(true);
    details->setMaximumHeight(140);
    details->setMinimumHeight(80);
    details->setVisible(false);
    auto *detailActions = new QWidget;
    auto *detailRow = new QHBoxLayout(detailActions); detailRow->setContentsMargins(0, 0, 0, 0);
    detailActions->hide();
    auto *message = new QLabel;
    message->setMaximumHeight(48);
    message->setWordWrap(true); message->setTextFormat(Qt::PlainText);
    message->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    const auto refresh = [&] {
        bool busy = false, ready = true;
        QStringList lines;
        for (const QString &kind : {QString("geoip"), QString("geosite")}) {
            busy |= pending.value(kind);
            ready &= paths.value(kind).isEmpty() || valid.value(kind);
            if (downloads.contains(kind)) {
                downloads[kind]->setVisible(!transfers.value(kind));
                downloads[kind]->setEnabled(!pending.value(kind));
                choices[kind]->setVisible(!transfers.value(kind));
                cancellations[kind]->setVisible(transfers.value(kind));
                const bool showError = !pending.value(kind) && !diagnostics.value(kind).isEmpty()
                    && (statuses[kind]->text() == tr("Download failed") || statuses[kind]->text() == tr("Invalid file"));
                errors[kind]->setVisible(showError);
                statuses[kind]->setSizePolicy(showError ? QSizePolicy::Maximum : QSizePolicy::Ignored, QSizePolicy::Preferred);
                statusRows[kind]->setStretch(0, showError ? 0 : 1);
                statusRows[kind]->setStretch(2, showError ? 1 : 0);
                downloads[kind]->setText(paths.value(kind).isEmpty() ? tr("Download") : tr("Update"));
                choices[kind]->setEnabled(!pending.value(kind));
                downloads[kind]->setAccessibleName(downloads[kind]->text() + ' ' + (kind == "geoip" ? tr("Country database") : tr("Site database")));
            }
            lines << kind + ": " + paths.value(kind) + '\n' + diagnostics.value(kind) + '\n' + sourceDetails.value(kind);
        }
        for (const auto &value : rules) {
            const auto rule = value.toObject();
            if (!rule.value("enabled").toBool()) continue;
            const QString matcher = rule.value("matcher").toString();
            if (matcher == "GeoIP" || matcher == "GeoSite") {
                const QString kind = matcher == "GeoIP" ? "geoip" : "geosite";
                ready &= !paths.value(kind).isEmpty() && valid.value(kind);
            }
        }
        buttons->button(QDialogButtonBox::Apply)->setEnabled(!busy && ready && applyError.isEmpty() && paths != original);
        details->setPlainText(tr("Used by all profiles with country or site rules") + "\n\n"
            + tr("Shared choices replace profile database paths on Save, import and Start. Canceling the profile editor does not undo Apply.")
            + "\n\n" + tr("Files use V2Ray protobuf, up to 64 MiB. The service needs read access. Profile exports still need their referenced files.")
            + "\n\n" + tr("SHA-256 checks release consistency; it is not a publisher signature.")
            + "\n\n" + QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/geodata\n\n" + lines.join("\n\n"));
    };
    std::function<void(const QString &, const QString &, Inspection)> inspect;
    inspect = [&](const QString &kind, const QString &path, Inspection inspection) {
        const bool downloaded = inspection == Inspection::Downloaded;
        applyError.clear();
        const int generation = ++generations[kind];
        pending[kind] = true;
        if (path == paths.value(kind)) valid[kind] = false;
        transfers[kind] = downloaded;
        statuses[kind]->setText(inspection == Inspection::Passive ? tr("Checking…") : tr("Checking file…"));
        progress[kind]->setRange(0, 0); progress[kind]->setVisible(downloaded); refresh();
        const QString absolute = QDir::isAbsolutePath(path) ? path : QFileInfo(profilePath).dir().absoluteFilePath(path);
        const QString key = GeoDataSelection::catalogKey(core, kind, absolute);
        if (downloaded) ownedCatalogs[kind] = key;
        QObject::disconnect(catalogConnections.take(kind));
        const auto resolved = [&, key, kind, path, downloaded, generation](const QString &resultKey, const QStringList &groups, const QString &catalogError) {
            if (key != resultKey) return;
            if (generations.value(kind) != generation) return;
            QObject::disconnect(catalogConnections.take(kind));
            ownedCatalogs.remove(kind);
            if (downloaded) QTimer::singleShot(0, selection, [selection, key] { selection->cancelCatalog(key); });
            pending[kind] = false; transfers[kind] = false; progress[kind]->hide();
            bool success = catalogError.isEmpty() && !groups.isEmpty();
            QString error;
            if (success) for (const auto &value : rules) {
                const auto rule = value.toObject();
                if (!rule.value("enabled").toBool() || rule.value("matcher").toString() != (kind == "geoip" ? "GeoIP" : "GeoSite")) continue;
                for (const auto &id : rule.value("value").toString().split(',')) if (!groups.contains(id.trimmed().toLower())) {
                    const QString safeId = QRegularExpression("^[a-zA-Z0-9_-]{1,256}$").match(id.trimmed()).hasMatch()
                        ? id.trimmed() : QStringLiteral("invalid identifier");
                    error = tr("Dataset group is unavailable: %1").arg(safeId); success = false; break;
                }
            }
            QString readyPath = path;
            if (success && downloaded) {
                diagnostics[kind] = tr("Validated locally: %1 groups. Not proof of service access.").arg(groups.size());
                pending[kind] = true; transfers[kind] = true; progress[kind]->setRange(0, 0); progress[kind]->show();
                refresh(); download->publishAsync(kind); return;
            }
            if (!success) {
                if (downloaded && ownedDownloads.contains(kind)) download->cancel(kind);
                diagnostics[kind] = error.isEmpty() ? catalogError : error;
                statuses[kind]->setText(downloaded ? tr("Download failed") : tr("Invalid file"));
                message->setText(downloaded ? tr("The selected Core rejected this database. Choose a compatible local file or check the error details.") : tr("Dataset validation failed. Check the file, schema and permissions."));
                // A failed replacement leaves the previously validated candidate intact.
            } else {
                paths[kind] = readyPath; valid[kind] = true;
                statuses[kind]->setText(tr("Ready") + " · " + QFileInfo(readyPath).fileName());
                diagnostics[kind] = tr("Validated locally: %1 groups. Not proof of service access.").arg(groups.size()) + "\n" + groups.mid(0, 256).join(", ");
                message->clear();
            }
            refresh();
        };
        QStringList cachedGroups; QString cachedError;
        if (inspection == Inspection::Passive && selection->cachedCatalog(core, kind, absolute, &cachedGroups, &cachedError)) {
            resolved(key, cachedGroups, cachedError);
        } else {
            catalogConnections[kind] = QObject::connect(selection, &GeoDataSelection::catalogResult, &dialog, resolved);
            selection->requestCatalog(core, kind, absolute, inspection != Inspection::Passive);
        }
    };
    for (const QString &kind : {QString("geoip"), QString("geosite")}) {
        auto *row = new QHBoxLayout;
        auto *labels = new QVBoxLayout;
        labels->setSpacing(2);
        labels->addWidget(new QLabel(kind == "geoip" ? tr("Country database") : tr("Site database")));
        auto *status = new QLabel(tr("Not downloaded")); status->setTextFormat(Qt::PlainText);
        status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        status->setAccessibleName(kind + " " + tr("Status"));
        status->setObjectName(kind + "Status");
        statuses[kind] = status;
        auto *statusRow = new QHBoxLayout; statusRow->addWidget(status, 1);
        auto *errorButton = new QToolButton; errorButton->setText("(?)");
        errorButton->setObjectName(kind + "Error"); errorButton->setFocusPolicy(Qt::StrongFocus);
        errorButton->setAccessibleName(tr("Download error details")); errorButton->hide();
        errors[kind] = errorButton; statusRow->addWidget(errorButton); statusRow->addStretch(0); statusRows[kind] = statusRow;
        QObject::connect(errorButton, &QToolButton::clicked, &dialog, [&, kind] {
            QMessageBox popup(QMessageBox::Warning, tr("Download error details"),
                tr("Download failed. Try again or choose a local file."), QMessageBox::Ok, &dialog);
            popup.setObjectName("geodataErrorDialog"); popup.setTextFormat(Qt::PlainText);
            popup.setInformativeText(diagnostics.value(kind).left(1024)); popup.exec();
        });
        labels->addLayout(statusRow); row->addLayout(labels, 1);
        auto *get = new QPushButton(tr("Download")); get->setObjectName(kind + "Download");
        auto *choose = new QPushButton(tr("Choose file…")); choose->setObjectName(kind + "Choose");
        for (auto *button : {get, choose}) { button->setAutoDefault(false); button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); row->addWidget(button); }
        choose->setAccessibleName(tr("Choose file…") + ' ' + (kind == "geoip" ? tr("Country database") : tr("Site database")));
        auto *cancel = new QPushButton(tr("Cancel")); cancel->setAutoDefault(false);
        cancel->setObjectName(kind + "Cancel"); cancel->hide(); row->addWidget(cancel);
        cancel->setAccessibleName(tr("Cancel") + ' ' + (kind == "geoip" ? tr("Country database") : tr("Site database")));
        cancellations[kind] = cancel;
        QObject::connect(cancel, &QPushButton::clicked, &dialog, [&, kind] {
            ++generations[kind];
            QObject::disconnect(catalogConnections.take(kind));
            if (ownedCatalogs.contains(kind)) selection->cancelCatalog(ownedCatalogs.take(kind));
            if (ownedDownloads.contains(kind)) download->cancel(kind); pending[kind] = false; transfers[kind] = false; progress[kind]->hide();
            statuses[kind]->setText(paths.value(kind).isEmpty() ? tr("Not downloaded") : tr("Ready") + " · " + QFileInfo(paths.value(kind)).fileName());
            diagnostics.remove(kind); message->clear(); refresh();
        });
        downloads[kind] = get; choices[kind] = choose; layout->addLayout(row);
        auto *bar = new QProgressBar; bar->setObjectName(kind + "Progress"); bar->setTextVisible(false);
        bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); bar->setMaximumHeight(12); bar->hide(); progress[kind] = bar; layout->addWidget(bar);
        QObject::connect(get, &QPushButton::clicked, &dialog, [&, kind] {
            if (pending.value(kind) || download->busy(kind)) return;
            applyError.clear(); diagnostics.remove(kind); sourceDetails.remove(kind); pending[kind] = true; transfers[kind] = true; ownedDownloads.insert(kind); statuses[kind]->setText(tr("Downloading…"));
            progress[kind]->setRange(0, 0); progress[kind]->show(); message->clear(); refresh(); download->start(kind);
        });
        QObject::connect(choose, &QPushButton::clicked, &dialog, [&, kind] {
            const QString path = QFileDialog::getOpenFileName(&dialog, tr("Choose local database"));
            if (!path.isEmpty()) inspect(kind, QFileInfo(path).absoluteFilePath(), Inspection::Manual);
        });
    }
    for (const QString &kind : {QString("geoip"), QString("geosite")}) {
        auto *clear = new QPushButton(kind == "geoip" ? tr("Clear country file") : tr("Clear site file"));
        clear->setAutoDefault(false); detailRow->addWidget(clear);
        QObject::connect(clear, &QPushButton::clicked, &dialog, [&, kind] {
            if (pending.value(kind)) return;
            applyError.clear(); paths[kind].clear(); valid[kind] = false; statuses[kind]->setText(tr("No file selected")); refresh();
        });
    }
    auto *revalidate = new QPushButton(tr("Check again")); revalidate->setAutoDefault(false); detailRow->addWidget(revalidate);
    QObject::connect(revalidate, &QPushButton::clicked, &dialog, [&] {
        for (const QString &kind : {QString("geoip"), QString("geosite")})
            if (!pending.value(kind) && !paths.value(kind).isEmpty()) inspect(kind, paths.value(kind), Inspection::Manual);
    });
    auto *toggle = new QToolButton;
    toggle->setObjectName("geodataDetails"); toggle->setText(tr("Details")); toggle->setCheckable(true);
    toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggle->setIcon(QIcon(":/icons/chevron-down.xpm")); toggle->setIconSize(QSize(9, 5));
    layout->addWidget(toggle, 0, Qt::AlignLeft); layout->addWidget(details); layout->addWidget(detailActions);
    QObject::connect(toggle, &QToolButton::toggled, &dialog, [&, toggle](bool checked) {
        toggle->setIcon(QIcon(checked ? ":/icons/chevron-up.xpm" : ":/icons/chevron-down.xpm")); details->setVisible(checked); detailActions->setVisible(checked);
        dialog.resize(dialog.width(), checked ? 440 : 300);
    });
    auto *scope = new QLabel(tr("Used by all profiles with country or site rules")); scope->setWordWrap(true); layout->addWidget(scope);
    layout->addWidget(message); layout->addStretch(); layout->addWidget(buttons);
    QObject::connect(download, &GeoDataDownload::progress, &dialog, [&](const QString &kind, qint64 received, qint64 total) {
        if (!transfers.value(kind)) return;
        progress[kind]->setRange(0, total > 0 ? 100 : 0);
        if (total > 0) progress[kind]->setValue(int(received * 100 / total));
    });
    QObject::connect(download, &GeoDataDownload::candidateReady, &dialog, [&](const QString &kind, const QString &path, const QString &hash) {
        if (!ownedDownloads.contains(kind) || !transfers.value(kind)) return;
        sourceDetails[kind] = tr("Release: %1").arg(download->releaseTag(kind)) + "\nSHA-256: " + hash; inspect(kind, path, Inspection::Downloaded);
    });
    QObject::connect(download, &GeoDataDownload::published, &dialog, [&](const QString &kind, const QString &path) {
        if (!ownedDownloads.contains(kind) || !transfers.value(kind)) return;
        pending[kind] = false; transfers[kind] = false; progress[kind]->hide(); paths[kind] = path; valid[kind] = true;
        statuses[kind]->setText(tr("Ready") + " · " + QFileInfo(path).fileName());
        message->clear(); refresh();
    });
    QObject::connect(download, &GeoDataDownload::failed, &dialog, [&](const QString &kind, const QString &error) {
        if (!ownedDownloads.contains(kind) || !transfers.value(kind)) return;
        pending[kind] = false; transfers[kind] = false; progress[kind]->hide(); statuses[kind]->setText(tr("Download failed"));
        diagnostics[kind] = error; message->setText(tr("Download failed. Try again or choose a local file.")); refresh();
    });
    QObject::connect(selection, &GeoDataSelection::catalogInvalidated, &dialog, [&](const QString &key) {
        for (const QString &kind : {QString("geoip"), QString("geosite")}) {
            const QString path = paths.value(kind);
            if (path.isEmpty() || pending.value(kind)) continue;
            const QString absolute = QDir::isAbsolutePath(path) ? path : QFileInfo(profilePath).dir().absoluteFilePath(path);
            if (GeoDataSelection::catalogKey(core, kind, absolute) != key) continue;
            valid[kind] = false; inspect(kind, path, Inspection::Passive);
        }
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, &dialog, [&] {
        QString error;
        for (const auto &value : rules) {
            const auto rule = value.toObject();
            const QString matcher = rule.value("matcher").toString();
            if (!rule.value("enabled").toBool() || (matcher != "GeoIP" && matcher != "GeoSite")) continue;
            const QString kind = matcher == "GeoIP" ? "geoip" : "geosite";
            if (paths.value(kind).isEmpty()) { error = tr("No shared %1 database is selected.").arg(kind); break; }
        }
        const auto profiles = ProfileStore().load(&error);
        if (error.isEmpty() && selection->apply(paths, core, profiles, &error)) dialog.accept();
        else { applyError = error; diagnostics["geoip"] = error; message->setText(error.left(180)); refresh(); }
    });
    for (const QString &kind : {QString("geoip"), QString("geosite")}) if (!paths.value(kind).isEmpty()) {
        inspect(kind, paths.value(kind), Inspection::Passive);
    }
    refresh(); dialog.exec();
    QObject::disconnect(selection, nullptr, &dialog, nullptr);
    QObject::disconnect(download, nullptr, &dialog, nullptr);
    for (const QString &kind : {QString("geoip"), QString("geosite")}) { ++generations[kind]; if (ownedCatalogs.contains(kind)) selection->cancelCatalog(ownedCatalogs.take(kind)); if (ownedDownloads.contains(kind)) download->cancel(kind); }
    for (auto *process : dialog.findChildren<QProcess *>()) {
        process->disconnect(&dialog); process->kill(); process->waitForFinished(1000);
    }
}
