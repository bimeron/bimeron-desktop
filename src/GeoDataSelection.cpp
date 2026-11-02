#include "GeoDataSelection.h"
#include "RoutingCompatibility.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QDateTime>
#include <QTimer>
#include <memory>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSettings>
#include <QUuid>

namespace {
constexpr auto settingsKey = "geodata/paths";
QJsonObject run(const QString &core, const QStringList &args, const QByteArray &input, QString *error) {
    QProcess process;
    QElapsedTimer timer;
    timer.start();
    process.start(core, args);
    if (!process.waitForStarted(2000)) { *error = QObject::tr("Core GeoData bridge is unavailable."); return {}; }
    process.write(input);
    process.closeWriteChannel();
    QByteArray output;
    qint64 total = 0;
    while (true) {
        process.waitForReadyRead(25);
        const auto bytes = process.readAllStandardOutput();
        total += bytes.size() + process.readAllStandardError().size();
        output += bytes;
        if (total > 32 * 1024 * 1024 || timer.elapsed() >= 15000) {
            process.kill(); process.waitForFinished(1000);
            *error = QObject::tr("Core GeoData bridge timed out or exceeded its response limit."); return {};
        }
        if (process.state() == QProcess::NotRunning) break;
    }
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(output, &parse);
    const auto result = document.object();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0
        || parse.error != QJsonParseError::NoError || !document.isObject()
        || result.value("version").toInt() != 1 || !result.value("error").toString().isEmpty()
        || (result.contains("error") && !result.value("error").isString())) {
        *error = QObject::tr("Core rejected GeoData validation or returned an invalid bridge response."); return {};
    }
    return result;
}
QJsonObject document(const QString &core, const QString &text, const QJsonArray &operations, QString *error) {
    if (text.toUtf8().size() > 65536) { *error = QObject::tr("The profile exceeds the 64 KiB limit."); return {}; }
    const auto result = run(core, {"document"}, QJsonDocument(QJsonObject{{"version", 1},
        {"profile", text}, {"operations", operations}}).toJson(QJsonDocument::Compact), error);
    if (error->isEmpty() && (!result.value("profile").isString() || !result.value("rules").isArray()))
        *error = QObject::tr("Core returned an invalid document response.");
    return result;
}
}

GeoDataSelection::GeoDataSelection(QObject *parent) : QObject(parent) {
    const auto saved = QSettings().value(settingsKey).toMap();
    for (const auto &kind : {QString("geoip"), QString("geosite")}) paths_[kind] = saved.value(kind).toString();
}
GeoDataSelection *GeoDataSelection::instance() {
    static auto *selection = new GeoDataSelection(QCoreApplication::instance());
    return selection;
}
GeoDataSelection::Paths GeoDataSelection::paths() const { return paths_; }

QStringList GeoDataSelection::inspect(const QString &core, const QString &kind, const QString &path, QString *error) {
    QString local;
    if (!error) error = &local;
    error->clear();
    if ((kind != "geoip" && kind != "geosite") || path.isEmpty() || !QDir::isAbsolutePath(path)) {
        *error = tr("Select an absolute GeoData file path and a supported database kind."); return {};
    }
    const auto result = run(core, {"geodata", "groups", kind, path}, {}, error);
    QStringList groups;
    if (error->isEmpty() && result.value("groups").isArray()) {
        for (const auto &value : result.value("groups").toArray()) {
            if (!value.isString() || !QRegularExpression(R"(\A[a-z0-9_!-]{1,256}\z)").match(value.toString()).hasMatch()) {
                *error = tr("Core returned an invalid GeoData catalog."); return {};
            }
            groups.append(value.toString());
        }
    }
    if (error->isEmpty() && groups.isEmpty()) *error = tr("The database contains no available groups.");
    return error->isEmpty() ? groups : QStringList{};
}

QString GeoDataSelection::materialize(const QString &text, const QString &profilePath, const QString &core,
                                     const Paths &paths, QString *error) {
    QString local;
    if (!error) error = &local;
    error->clear();
    // Older CLIs remain usable for profiles without active GeoData fields.
    // Disabled blocks are comments and need neither a catalog nor a bridge.
    if (!QRegularExpression(R"((?m)^\s*Geo(?:IP|Site)\s*=)").match(text).hasMatch()) return text;
    const auto original = document(core, text, {}, error);
    if (!error->isEmpty()) return {};
    QMap<QString, QStringList> active;
    for (const auto &value : original.value("rules").toArray()) {
        const auto rule = value.toObject();
        const QString matcher = rule.value("matcher").toString();
        if (!rule.value("enabled").toBool() || (matcher != "GeoIP" && matcher != "GeoSite")) continue;
        if (!rule.value("error").toString().isEmpty()) { *error = tr("An active GeoData rule is invalid."); return {}; }
        active[matcher == "GeoIP" ? "geoip" : "geosite"].append(rule.value("value").toString().split(',', Qt::SkipEmptyParts));
    }
    if (active.isEmpty()) return text;
    if (profilePath.isEmpty()) { *error = tr("A final profile path is required for GeoData validation."); return {}; }
    QJsonArray operations;
    for (auto it = active.cbegin(); it != active.cend(); ++it) {
        const QString selected = paths.value(it.key());
        if (selected.isEmpty()) { *error = tr("No shared %1 database is selected.").arg(it.key()); return {}; }
        const QString absolute = QDir::isAbsolutePath(selected) ? QFileInfo(selected).absoluteFilePath()
            : QFileInfo(QFileInfo(profilePath).absoluteDir(), selected).absoluteFilePath();
        const auto groups = inspect(core, it.key(), absolute, error);
        if (!error->isEmpty()) return {};
        for (const auto &id : it.value()) if (!groups.contains(id.trimmed().toLower())) {
            *error = tr("The %1 database does not contain active group %2.").arg(it.key(), id.trimmed()); return {};
        }
        operations.append(QJsonObject{{"op", "set_field"}, {"section", "GeoData"},
            {"key", it.key() == "geoip" ? "GeoIPFile" : "GeoSiteFile"}, {"value", selected}});
    }
    const auto patched = document(core, text, operations, error);
    if (!error->isEmpty()) return {};
    const QString result = patched.value("profile").toString();
    if (result.toUtf8().size() > 65536) { *error = tr("The profile exceeds the 64 KiB limit."); return {}; }
    const QString temporary = QFileInfo(profilePath).absoluteDir().filePath(
        ".bimeron-geodata-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".conf");
    if (!ProfileStore::writeConfig(temporary, result.toUtf8(), error)) return {};
    const auto validation = run(core, {"validate", "--json", temporary}, {}, error);
    const bool removed = QFile::remove(temporary);
    if (error->isEmpty()) *error = geoDataValidationError(QJsonDocument(validation).toJson(QJsonDocument::Compact));
    if (error->isEmpty() && !removed) *error = tr("Could not remove the protected validation file.");
    return error->isEmpty() ? result : QString();
}

bool GeoDataSelection::apply(const Paths &paths, const QString &core, const QList<Profile> &profiles, QString *error) {
    QString local;
    if (!error) error = &local;
    error->clear();
    Paths candidate;
    for (const auto &kind : {QString("geoip"), QString("geosite")}) candidate[kind] = paths.value(kind);
    for (auto it = candidate.cbegin(); it != candidate.cend(); ++it) {
        if (it.value().isEmpty()) continue;
        // Relative manual selections are checked in each affected profile's directory.
        if (QDir::isAbsolutePath(it.value())) inspect(core, it.key(), it.value(), error);
        else if (profiles.isEmpty()) *error = tr("Relative database paths require a managed profile.");
        if (!error->isEmpty()) return false;
    }
    for (const auto &profile : profiles) {
        QFile file(profile.path);
        // Unreadable entries cannot run; normal Save/Start preparation remains fail-closed.
        if (!file.open(QIODevice::ReadOnly)) continue;
        const auto bytes = file.read(65537);
        const QString text = QString::fromUtf8(bytes);
        if (!QRegularExpression(R"((?m)^\s*Geo(?:IP|Site)\s*=)").match(text).hasMatch()) continue;
        if (bytes.size() > 65536) { *error = tr("Managed profile %1 exceeds the 64 KiB limit.").arg(profile.name); return false; }
        if (text.toUtf8() != bytes) { *error = tr("Managed profile %1 is not valid UTF-8.").arg(profile.name); return false; }
        const auto inspected = document(core, text, {}, error);
        for (const auto &value : inspected.value("rules").toArray()) {
            const auto rule = value.toObject();
            const QString matcher = rule.value("matcher").toString();
            if (!rule.value("enabled").toBool() || (matcher != "GeoIP" && matcher != "GeoSite")) continue;
            if (!rule.value("error").toString().isEmpty()) { *error = tr("An active GeoData rule is invalid."); break; }
            const QString kind = matcher == "GeoIP" ? "geoip" : "geosite";
            const QString selected = candidate.value(kind);
            if (selected.isEmpty()) { *error = tr("No shared %1 database is selected.").arg(kind); break; }
            const QString absolute = QDir::isAbsolutePath(selected) ? selected : QFileInfo(profile.path).dir().absoluteFilePath(selected);
            const auto groups = inspect(core, kind, absolute, error);
            if (!error->isEmpty()) break;
            for (const auto &id : rule.value("value").toString().split(',')) if (!groups.contains(id.trimmed().toLower())) {
                *error = tr("The %1 database does not contain active group %2.").arg(kind, id.trimmed()); break;
            }
            if (!error->isEmpty()) break;
        }
        if (!error->isEmpty()) { *error = profile.name + ": " + *error; return false; }
    }
    if (candidate == paths_) return true;
    QSettings settings;
    const bool hadPrevious = settings.contains(settingsKey);
    const QVariant previous = settings.value(settingsKey);
    QVariantMap map;
    for (auto it = candidate.cbegin(); it != candidate.cend(); ++it) map.insert(it.key(), it.value());
    settings.setValue(settingsKey, map);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (hadPrevious) settings.setValue(settingsKey, previous); else settings.remove(settingsKey);
        settings.sync();
        *error = tr("Could not save the shared GeoData selection. The previous selection is retained."); return false;
    }
    paths_ = candidate;
    emit changed();
    return true;
}

GeoDataSelection::~GeoDataSelection() {
    for (auto &catalog : catalogs_) if (catalog.process) {
        catalog.process->disconnect(this); catalog.process->kill(); catalog.process->waitForFinished(1000);
    }
}
QString GeoDataSelection::catalogKey(const QString &core, const QString &kind, const QString &path) {
    return core + '\n' + kind + '\n' + QFileInfo(path).absoluteFilePath();
}
bool GeoDataSelection::cachedCatalog(const QString &core, const QString &kind, const QString &path,
                                     QStringList *groups, QString *error) const {
    const auto it = catalogs_.constFind(catalogKey(core, kind, path));
    const QFileInfo info(path);
    const QString identity = info.canonicalFilePath() + '/' + QString::number(info.size()) + '/' + QString::number(info.lastModified().toMSecsSinceEpoch());
    if (it == catalogs_.cend() || it->process || it->identity != identity) return false;
    *groups = it->groups; *error = it->error;
    return true;
}
bool GeoDataSelection::catalogsBusy() const {
    for (const auto &catalog : catalogs_) if (catalog.process) return true;
    return false;
}
void GeoDataSelection::cancelCatalog(const QString &key) {
    auto it = catalogs_.find(key);
    if (it == catalogs_.end()) return;
    ++it->generation;
    if (it->process) {
        auto *process = it->process; it->process = nullptr;
        process->disconnect(this); process->kill(); process->waitForFinished(1000); process->deleteLater();
    }
    if (it->watcher) delete it->watcher;
    it->watcher = nullptr; it->identity.clear(); it->groups.clear(); it->error.clear();
}
void GeoDataSelection::requestCatalog(const QString &core, const QString &kind, const QString &path, bool force) {
    const QString key = catalogKey(core, kind, path);
    auto &catalog = catalogs_[key];
    const QFileInfo info(path);
    const QString identity = info.canonicalFilePath() + '/' + QString::number(info.size()) + '/' + QString::number(info.lastModified().toMSecsSinceEpoch());
    if (catalog.process) return;
    if (!force && catalog.identity == identity) {
        const quint64 generation = catalog.generation;
        const auto groups = catalog.groups; const QString error = catalog.error;
        QTimer::singleShot(0, this, [this, key, generation, identity, groups, error] {
            const auto &current = catalogs_[key];
            if (current.generation == generation && !current.process && current.identity == identity)
                emit catalogResult(key, groups, error);
        });
        return;
    }
    ++catalog.generation;
    catalog.identity = identity;
    catalog.groups.clear();
    if (catalog.watcher) delete catalog.watcher;
    auto *watcher = new QFileSystemWatcher(this); catalog.watcher = watcher;
    watcher->addPath(path);
    watcher->addPath(info.absolutePath());
    const auto invalidated = [this, key] {
        catalogs_[key].identity.clear(); emit catalogInvalidated(key);
    };
    connect(watcher, &QFileSystemWatcher::fileChanged, this, invalidated);
    connect(watcher, &QFileSystemWatcher::directoryChanged, this, [this, key, path, invalidated] {
        const QFileInfo info(path);
        const QString current = info.canonicalFilePath() + '/' + QString::number(info.size()) + '/' + QString::number(info.lastModified().toMSecsSinceEpoch());
        if (catalogs_.value(key).identity != current) invalidated();
    });
    auto *process = new QProcess(this); catalog.process = process;
    auto output = std::make_shared<QByteArray>();
    auto done = std::make_shared<bool>(false);
    auto *timer = new QTimer(process); timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, process, [process] { process->setProperty("failed", true); process->kill(); });
    connect(process, &QProcess::readyReadStandardOutput, process, [process, output] {
        *output += process->readAllStandardOutput();
        if (output->size() > 32 * 1024 * 1024) { process->setProperty("failed", true); process->kill(); }
    });
    connect(process, &QProcess::readyReadStandardError, process, [process] { process->readAllStandardError(); });
    const auto finish = [this, key, core, kind, path, process, output, done](bool success) {
        if (*done) return; *done = true;
        *output += process->readAllStandardOutput();
        QJsonParseError parse;
        const auto document = QJsonDocument::fromJson(*output, &parse);
        const auto result = document.object();
        auto &entry = catalogs_[key]; entry.process = nullptr;
        if (entry.identity.isEmpty()) {
            process->deleteLater(); requestCatalog(core, kind, path, true); return;
        }
        success &= !process->property("failed").toBool() && output->size() <= 32 * 1024 * 1024
            && parse.error == QJsonParseError::NoError && document.isObject()
            && result.value("version").toInt() == 1 && result.value("groups").isArray()
            && (!result.contains("error") || result.value("error").isString()) && result.value("error").toString().isEmpty();
        if (success) for (const auto &value : result.value("groups").toArray()) {
            if (!value.isString() || !QRegularExpression(R"(\A[a-z0-9_!-]{1,256}\z)").match(value.toString()).hasMatch()) { success = false; break; }
            entry.groups.append(value.toString());
        }
        entry.error = success && !entry.groups.isEmpty() ? QString() :
            QStringLiteral("Source: selected Core\nPhase: catalog validation\nProcess code: %1; exit code: %2; JSON code: %3; deadline/limit: %4")
                .arg(int(process->error())).arg(process->exitCode()).arg(int(parse.error))
                .arg(process->property("failed").toBool());
        if (!entry.error.isEmpty()) {
            const QString code = result.value("details").toObject().value("code").toString();
            if (QRegularExpression("^geodata_[a-z_]{1,80}$").match(code).hasMatch()) entry.error += "\nCore code: " + code;
            entry.groups.clear();
        }
        emit catalogResult(key, entry.groups, entry.error);
        process->deleteLater();
    };
    connect(process, &QProcess::errorOccurred, this, [finish](QProcess::ProcessError e) { if (e == QProcess::FailedToStart) finish(false); });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [finish](int code, QProcess::ExitStatus status) { finish(code == 0 && status == QProcess::NormalExit); });
    process->start(core, {"geodata", "groups", kind, path}); timer->start(15000);
}
