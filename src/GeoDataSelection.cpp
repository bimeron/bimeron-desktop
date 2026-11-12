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
#include <QSet>
#include <QUuid>

namespace {
constexpr auto settingsKey = "geodata/paths";
constexpr qsizetype catalogOutputLimit = 32 * 1024 * 1024;
constexpr qsizetype catalogErrorLimit = 4096;
constexpr int catalogDeadline = 15000;

// A Core without the operation answers `unknown command` and exits non-zero.
bool unsupportedOperation(int exitCode, const QByteArray &text) {
    return exitCode != 0 && text.toLower().contains("unknown command");
}
// Diagnostics are plain text: replace control characters and bound the length.
QString sanitized(const QString &text, int limit) {
    QString result;
    for (const QChar c : text.trimmed()) result += c.isPrint() ? c : QChar(' ');
    return result.size() > limit ? result.left(limit) + QStringLiteral("…") : result;
}
QString fileIdentity(const QString &path) {
    const QFileInfo info(path);
    return info.canonicalFilePath() + '/' + QString::number(info.size()) + '/'
        + QString::number(info.lastModified().toMSecsSinceEpoch());
}
// A validator replaced at the same path must not reuse earlier results.
QString validatorIdentity(const QString &core) {
    return QFileInfo::exists(core) ? fileIdentity(core) : core;
}
QJsonObject run(const QString &core, const QStringList &args, const QByteArray &input, QString *error) {
    QProcess process;
    QElapsedTimer timer;
    timer.start();
    process.start(core, args);
    if (!process.waitForStarted(2000)) { *error = QObject::tr("Core GeoData bridge is unavailable."); return {}; }
    process.write(input);
    process.closeWriteChannel();
    QByteArray output, errors;
    qint64 total = 0;
    while (true) {
        process.waitForReadyRead(25);
        const auto bytes = process.readAllStandardOutput();
        const auto diagnostic = process.readAllStandardError();
        total += bytes.size() + diagnostic.size();
        output += bytes;
        errors += diagnostic.left(qMax<qsizetype>(0, catalogErrorLimit - errors.size()));
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
        *error = unsupportedOperation(process.exitCode(), output + errors)
            ? QObject::tr("The selected Core cannot check country and site databases.")
            : QObject::tr("Core rejected GeoData validation or returned an invalid bridge response.");
        return {};
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

QString GeoDataSelection::failureStatus(CatalogFailure failure) {
    switch (failure) {
    case CatalogFailure::None: return tr("Checked");
    case CatalogFailure::Missing: return tr("File not found");
    case CatalogFailure::Unreadable: return tr("Cannot read file");
    case CatalogFailure::Unsupported: return tr("Validation unavailable");
    case CatalogFailure::ValidatorFailed: return tr("Validation failed");
    case CatalogFailure::Rejected: return tr("Invalid database");
    }
    return {};
}

QString GeoDataSelection::failureExplanation(CatalogFailure failure) {
    switch (failure) {
    case CatalogFailure::Missing: return tr("The selected file no longer exists. The selection is kept until you change it.");
    case CatalogFailure::Unreadable: return tr("The application cannot read the file. This does not mean the database is damaged.");
    case CatalogFailure::Unsupported: return tr("The selected Core cannot check databases. The file stays selected but cannot be verified.");
    case CatalogFailure::ValidatorFailed: return tr("The check did not complete. This does not mean the database is damaged.");
    case CatalogFailure::Rejected: return tr("The selected Core rejected the database contents.");
    case CatalogFailure::None: break;
    }
    return {};
}

GeoDataSelection::CatalogOutcome GeoDataSelection::precheckCatalog(const QString &core, const QString &kind,
                                                                  const QString &path) {
    CatalogOutcome outcome;
    const QString facts = tr("Operation: check %1 database").arg(kind) + '\n' + tr("Validator: %1").arg(QDir::toNativeSeparators(core));
    if (!QFileInfo::exists(path)) {
        outcome.failure = CatalogFailure::Missing;
        outcome.diagnostic = facts + '\n' + tr("Cause: the file does not exist.");
        return outcome;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        outcome.failure = CatalogFailure::Unreadable;
        outcome.diagnostic = facts + '\n' + tr("Cause: the application cannot read the file (%1).")
            .arg(sanitized(file.errorString(), 200));
    }
    return outcome;
}

GeoDataSelection::CatalogOutcome GeoDataSelection::classifyCatalog(const QString &core, const QString &kind,
                                                                   const QString &path, const CatalogRun &run) {
    CatalogOutcome outcome;
    QStringList facts{tr("Operation: check %1 database").arg(kind), tr("Validator: %1").arg(QDir::toNativeSeparators(core))};
    const auto fail = [&](CatalogFailure failure, const QString &cause) {
        outcome.failure = failure;
        outcome.groups.clear();
        facts << tr("Cause: %1").arg(cause);
        const QString firstLine = QString::fromUtf8(run.errors.left(catalogErrorLimit)).trimmed().section('\n', 0, 0);
        if (!firstLine.isEmpty()) facts << tr("Validator output: %1").arg(sanitized(firstLine, 200));
        outcome.diagnostic = facts.join('\n');
        return outcome;
    };
    if (!run.started) return fail(CatalogFailure::ValidatorFailed, tr("the validator could not be started."));
    if (run.timedOut) return fail(CatalogFailure::ValidatorFailed, tr("the validator did not finish within 15 seconds."));
    if (run.overLimit) return fail(CatalogFailure::ValidatorFailed, tr("the validator response exceeded its size limit."));
    if (run.crashed) return fail(CatalogFailure::ValidatorFailed, tr("the validator stopped unexpectedly."));
    facts << tr("Exit code: %1").arg(run.exitCode);
    if (unsupportedOperation(run.exitCode, run.output + run.errors))
        return fail(CatalogFailure::Unsupported, tr("this Core does not support database checks."));
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(run.output, &parse);
    const auto result = document.object();
    if (parse.error != QJsonParseError::NoError || !document.isObject() || result.value("version").toInt() != 1
        || (result.contains("error") && !result.value("error").isString()))
        return fail(CatalogFailure::ValidatorFailed, tr("the validator returned an invalid response."));
    const QString code = result.value("details").toObject().value("code").toString();
    const bool geoCode = QRegularExpression("^geodata_[a-z_]{1,80}$").match(code).hasMatch();
    if (geoCode) facts << tr("Core code: %1").arg(code);
    const auto problems = result.value("problems").toArray();
    const QString message = problems.isEmpty() ? QString()
        : sanitized(problems.first().toObject().value("message").toString(), 300);
    if (!message.isEmpty()) facts << tr("Core message: %1").arg(message);
    if (!result.value("error").toString().isEmpty() || run.exitCode != 0) {
        if (code == "geodata_unreadable")
            return fail(QFileInfo::exists(path) ? CatalogFailure::Unreadable : CatalogFailure::Missing,
                        tr("the validator cannot read the file."));
        if (geoCode) return fail(CatalogFailure::Rejected, tr("the validator rejected the database."));
        return fail(CatalogFailure::ValidatorFailed, run.exitCode != 0
            ? tr("the validator exited with an error.") : tr("the validator reported an unclassified error."));
    }
    if (!result.value("groups").isArray())
        return fail(CatalogFailure::ValidatorFailed, tr("the validator returned an invalid response."));
    for (const auto &value : result.value("groups").toArray()) {
        if (!value.isString() || !QRegularExpression(R"(\A[a-z0-9_!-]{1,256}\z)").match(value.toString()).hasMatch())
            return fail(CatalogFailure::ValidatorFailed, tr("the validator returned an invalid group list."));
        outcome.groups.append(value.toString());
    }
    if (outcome.groups.isEmpty()) return fail(CatalogFailure::Rejected, tr("the database contains no groups."));
    return outcome;
}

QStringList GeoDataSelection::inspect(const QString &core, const QString &kind, const QString &path, QString *error,
                                      CatalogFailure *failure) {
    QString local;
    CatalogFailure localFailure;
    if (!error) error = &local;
    if (!failure) failure = &localFailure;
    error->clear();
    *failure = CatalogFailure::None;
    if ((kind != "geoip" && kind != "geosite") || path.isEmpty() || !QDir::isAbsolutePath(path)) {
        *error = tr("Select an absolute GeoData file path and a supported database kind.");
        *failure = CatalogFailure::Missing;
        return {};
    }
    auto outcome = precheckCatalog(core, kind, path);
    if (outcome.failure == CatalogFailure::None) {
        CatalogRun run;
        QProcess process;
        QElapsedTimer timer;
        timer.start();
        process.start(core, {"geodata", "groups", kind, path});
        run.started = process.waitForStarted(2000);
        if (run.started) {
            process.closeWriteChannel();
            const auto drain = [&] {
                run.output += process.readAllStandardOutput();
                run.errors += process.readAllStandardError().left(qMax<qsizetype>(0, catalogErrorLimit - run.errors.size()));
            };
            while (process.state() != QProcess::NotRunning) {
                process.waitForReadyRead(25);
                drain();
                run.overLimit = run.output.size() > catalogOutputLimit;
                run.timedOut = timer.elapsed() >= catalogDeadline;
                if (run.overLimit || run.timedOut) { process.kill(); process.waitForFinished(1000); break; }
            }
            drain();
            run.crashed = !run.overLimit && !run.timedOut && process.exitStatus() != QProcess::NormalExit;
            run.exitCode = process.exitCode();
        }
        outcome = classifyCatalog(core, kind, path, run);
    }
    *failure = outcome.failure;
    *error = outcome.diagnostic;
    return outcome.failure == CatalogFailure::None ? outcome.groups : QStringList{};
}

QString GeoDataSelection::materialize(const QString &text, const QString &profilePath, const QString &core,
                                     const Paths &paths, QString *error, const QString &validationDirectory) {
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
    if (!validationDirectory.isEmpty() && !ProfileStore::importReferencesSafe(result.toUtf8(), error)) return {};
    const QString temporary = (validationDirectory.isEmpty() ? QFileInfo(profilePath).absoluteDir() : QDir(validationDirectory)).filePath(
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
    QMap<QString, QSet<QString>> inspectedGroups;
    const auto groupsFor = [&](const QString &kind, const QString &path) {
        const QString absolute = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        const QString key = kind + '\n' + absolute;
        if (!inspectedGroups.contains(key)) {
            const auto groups = inspect(core, kind, absolute, error);
            inspectedGroups.insert(key, QSet<QString>(groups.cbegin(), groups.cend()));
        }
        return inspectedGroups.value(key);
    };
    Paths candidate;
    for (const auto &kind : {QString("geoip"), QString("geosite")}) candidate[kind] = paths.value(kind);
    for (auto it = candidate.cbegin(); it != candidate.cend(); ++it) {
        if (it.value().isEmpty()) continue;
        // Relative manual selections are checked in each affected profile's directory.
        if (QDir::isAbsolutePath(it.value())) groupsFor(it.key(), it.value());
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
            const auto groups = groupsFor(kind, absolute);
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
                                     QStringList *groups, QString *error, CatalogFailure *failure) const {
    const auto it = catalogs_.constFind(catalogKey(core, kind, path));
    if (it == catalogs_.cend() || it->process || it->identity != fileIdentity(path)
        || it->validator != validatorIdentity(core)) return false;
    *groups = it->groups; *error = it->error;
    if (failure) *failure = it->failure;
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
    catalogs_.erase(it);
}
void GeoDataSelection::requestCatalog(const QString &core, const QString &kind, const QString &path, bool force) {
    const QString key = catalogKey(core, kind, path);
    auto &catalog = catalogs_[key];
    const QString identity = fileIdentity(path);
    const QString validator = validatorIdentity(core);
    if (catalog.process) return;
    // Deliver asynchronously, and only if nothing superseded this result meanwhile.
    const auto deliver = [this, key](const Catalog &entry) {
        const quint64 generation = entry.generation;
        const QString expected = entry.identity;
        QTimer::singleShot(0, this, [this, key, generation, expected] {
            const auto current = catalogs_.constFind(key);
            if (current != catalogs_.cend() && current->generation == generation && !current->process
                && current->identity == expected)
                emit catalogResult(key, current->groups, current->error, current->failure);
        });
    };
    if (!force && catalog.identity == identity && catalog.validator == validator) { deliver(catalog); return; }
    ++catalog.generation;
    catalog.identity = identity;
    catalog.validator = validator;
    catalog.groups.clear();
    catalog.error.clear();
    catalog.failure = CatalogFailure::None;
    if (catalog.watcher) delete catalog.watcher;
    auto *watcher = new QFileSystemWatcher(this); catalog.watcher = watcher;
    if (QFileInfo::exists(path)) watcher->addPath(path);
    watcher->addPath(QFileInfo(path).absolutePath());
    const auto invalidated = [this, key] {
        catalogs_[key].identity.clear(); emit catalogInvalidated(key);
    };
    connect(watcher, &QFileSystemWatcher::fileChanged, this, invalidated);
    connect(watcher, &QFileSystemWatcher::directoryChanged, this, [this, key, path, invalidated] {
        if (catalogs_.value(key).identity != fileIdentity(path)) invalidated();
    });
    const auto local = precheckCatalog(core, kind, path);
    if (local.failure != CatalogFailure::None) {
        catalog.error = local.diagnostic;
        catalog.failure = local.failure;
        deliver(catalog);
        return;
    }
    auto *process = new QProcess(this); catalog.process = process;
    auto run = std::make_shared<CatalogRun>();
    auto done = std::make_shared<bool>(false);
    auto *timer = new QTimer(process); timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, process, [process, run] { run->timedOut = true; process->kill(); });
    connect(process, &QProcess::readyReadStandardOutput, process, [process, run] {
        run->output += process->readAllStandardOutput();
        if (run->output.size() > catalogOutputLimit) { run->overLimit = true; process->kill(); }
    });
    connect(process, &QProcess::readyReadStandardError, process, [process, run] {
        run->errors += process->readAllStandardError().left(qMax<qsizetype>(0, catalogErrorLimit - run->errors.size()));
    });
    const auto finish = [this, key, core, kind, path, process, run, done](bool started, bool crashed, int exitCode) {
        if (*done) return; *done = true;
        run->output += process->readAllStandardOutput();
        run->errors += process->readAllStandardError().left(qMax<qsizetype>(0, catalogErrorLimit - run->errors.size()));
        run->started = started;
        run->crashed = crashed && !run->timedOut && !run->overLimit;
        run->exitCode = exitCode;
        auto &entry = catalogs_[key]; entry.process = nullptr;
        if (entry.identity.isEmpty()) {
            process->deleteLater(); requestCatalog(core, kind, path, true); return;
        }
        const auto outcome = classifyCatalog(core, kind, path, *run);
        entry.groups = outcome.groups;
        entry.error = outcome.diagnostic;
        entry.failure = outcome.failure;
        emit catalogResult(key, entry.groups, entry.error, entry.failure);
        process->deleteLater();
    };
    connect(process, &QProcess::errorOccurred, this, [finish](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) finish(false, false, -1);
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [finish](int code, QProcess::ExitStatus status) { finish(true, status != QProcess::NormalExit, code); });
    process->start(core, {"geodata", "groups", kind, path});
    timer->start(catalogDeadline);
}
