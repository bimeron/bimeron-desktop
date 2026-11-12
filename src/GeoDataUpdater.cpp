#include "GeoDataUpdater.h"
#include "GeoDataDownload.h"
#include "GeoDataSelection.h"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QWidget>

namespace {
constexpr auto lastCheckKey = "geodata/lastCheck";
constexpr auto lastCheckFailedKey = "geodata/lastCheckFailed";
constexpr qint64 checkInterval = 24 * 60 * 60;
}

GeoDataUpdater::GeoDataUpdater(std::function<QString()> core, std::function<QList<Profile>()> profiles,
                               QObject *parent, GeoDataDownload *transport)
    : QObject(parent), core_(std::move(core)), profiles_(std::move(profiles)),
      download_(transport ? transport : new GeoDataDownload(this)) {
    timer_.setInterval(60 * 60 * 1000);
    connect(&timer_, &QTimer::timeout, this, [this] { check(); });
    connect(&first_, &QTimer::timeout, this, [this] { check(); });
    connect(download_, &GeoDataDownload::upToDate, this, [this](const QString &kind) {
        if (kind == current_) { current_.clear(); next(); }
    });
    connect(download_, &GeoDataDownload::failed, this, [this](const QString &kind) {
        if (kind == current_) fail();
    });
    connect(download_, &GeoDataDownload::candidateReady, this, [this](const QString &kind, const QString &path) {
        if (kind != current_) return;
        auto *selection = GeoDataSelection::instance();
        catalogKey_ = GeoDataSelection::catalogKey(core_(), kind, path);
        QObject::disconnect(catalogConnection_);
        catalogConnection_ = connect(selection, &GeoDataSelection::catalogResult, this,
            [this, kind](const QString &key, const QStringList &groups, const QString &, GeoDataSelection::CatalogFailure failure) {
                if (key != catalogKey_ || kind != current_) return;
                QObject::disconnect(catalogConnection_);
                const QString finishedKey = catalogKey_;
                catalogKey_.clear();
                QTimer::singleShot(0, GeoDataSelection::instance(), [finishedKey] {
                    GeoDataSelection::instance()->cancelCatalog(finishedKey);
                });
                // Only a release the selected Core can read may replace the current one.
                if (failure != GeoDataSelection::CatalogFailure::None || groups.isEmpty()) {
                    download_->cancel(kind);
                    fail();
                    return;
                }
                if (!download_->publishAsync(kind)) fail();
            });
        selection->requestCatalog(core_(), kind, path, true);
    });
    connect(download_, &GeoDataDownload::published, this, [this](const QString &kind, const QString &path, const QString &tag) {
        if (kind != current_) return;
        auto *selection = GeoDataSelection::instance();
        GeoDataSelection::Selection choice = selection->selection();
        // The user may have chosen another source meanwhile; then the release is unused.
        if (choice.sources.value(kind) != GeoDataSelection::Source::Loyalsoldier) { current_.clear(); next(); return; }
        const QString previous = choice.paths.value(kind);
        choice.paths[kind] = path;
        choice.tags[kind] = tag;
        QString error;
        if (!selection->apply(choice, core_(), profiles_(), &error)) { fail(); return; }
        removeStaleCopies(kind, {path, previous});
        updated_ << kind;
        current_.clear();
        next();
    });
}

void GeoDataUpdater::start(int firstDelayMs) {
    first_.setSingleShot(true);
    first_.start(firstDelayMs);
    timer_.start();
}

QDateTime GeoDataUpdater::lastCheck() { return QSettings().value(lastCheckKey).toDateTime(); }
bool GeoDataUpdater::lastCheckFailed() { return QSettings().value(lastCheckFailedKey, false).toBool(); }

bool GeoDataUpdater::check(bool force) {
    if (busy()) return false;
    auto *selection = GeoDataSelection::instance();
    if (!force && !selection->autoUpdate()) return false;
    const QDateTime last = lastCheck();
    if (!force && last.isValid() && last.secsTo(QDateTime::currentDateTimeUtc()) < checkInterval) return false;
    // The database manager owns the selection while it is open; check again later.
    if (QApplication::activeModalWidget()) return false;
    for (const QString &kind : GeoDataSelection::kinds())
        if (selection->source(kind) == GeoDataSelection::Source::Loyalsoldier) queue_ << kind;
    if (queue_.isEmpty()) return false;
    failed_ = false;
    updated_.clear();
    next();
    return true;
}

void GeoDataUpdater::fail() {
    failed_ = true;
    current_.clear();
    next();
}

void GeoDataUpdater::next() {
    if (!current_.isEmpty()) return;
    if (queue_.isEmpty()) {
        QSettings settings;
        settings.setValue(lastCheckKey, QDateTime::currentDateTimeUtc());
        settings.setValue(lastCheckFailedKey, failed_);
        const QStringList kinds = updated_;
        updated_.clear();
        emit finished(failed_);
        if (!kinds.isEmpty()) emit updated(kinds);
        return;
    }
    current_ = queue_.takeFirst();
    const QString kind = current_;
    if (!download_->start(kind, GeoDataSelection::instance()->selection().tags.value(kind))) fail();
}

void GeoDataUpdater::removeStaleCopies(const QString &kind, const QStringList &keep) {
    QStringList kept;
    for (const QString &path : keep) if (!path.isEmpty()) kept << QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    // Selected own files may also live in the store; they are never removed.
    for (const QString &path : GeoDataSelection::instance()->paths())
        if (!path.isEmpty()) kept << QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QDir root(GeoDataSelection::managedDirectory());
    const QRegularExpression hash("^[0-9a-f]{64}$");
    for (const QString &name : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!hash.match(name).hasMatch()) continue;
        QDir directory(root.filePath(name));
        const QString file = QDir::cleanPath(directory.absoluteFilePath(kind + ".dat"));
        if (!QFileInfo(file).isFile() || kept.contains(file, Qt::CaseInsensitive)) continue;
        if (!QFile::remove(file)) continue;
        QFile::remove(directory.filePath(".publish.lock"));
        root.rmdir(name);
    }
}
