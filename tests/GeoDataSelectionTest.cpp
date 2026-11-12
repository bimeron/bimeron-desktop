#include "GeoDataSelection.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QProcess>
#include <QStandardPaths>
#include <QFileSystemWatcher>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QDateTime>
#include <QEventLoop>
#include <functional>
#include <cstdio>
#include <cstdlib>

static void requireAt(bool value, int line) { if (!value) { std::fprintf(stderr, "FAILED line %d", line); std::fputc(10, stderr); std::abort(); } }
#define require(x) requireAt((x), __LINE__)
static QByteArray read(const QString &path) {
    QFile file(path); require(file.open(QIODevice::ReadOnly)); return file.readAll();
}
static void write(const QString &path, const QByteArray &data) {
    QFile file(path); require(file.open(QIODevice::WriteOnly)); require(file.write(data) == data.size());
}

static void waitFor(const std::function<bool()> &done) {
    QElapsedTimer timer; timer.start();
    while (!done() && timer.elapsed() < 5000) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    require(done());
}


// Runs in a child process so each case gets a fresh selection singleton.
static int migrationCase(const QString &name) {
    QTemporaryDir directory;
    require(directory.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("BimeronSelectionTest");
    QCoreApplication::setApplicationName("Migration-" + name);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    const QString bundled = directory.filePath("bundled");
    require(QDir().mkpath(bundled));
    write(bundled + "/release.json", "{\"tag\":\"202611112204\"}");
    write(bundled + "/geoip.dat", "bundled geoip");
    write(bundled + "/geosite.dat", "bundled geosite");
    qputenv("BIMERON_BUNDLED_GEODATA", bundled.toUtf8());
    const QString managed = GeoDataSelection::managedDirectory() + "/" + QString(64, 'a');
    require(QDir().mkpath(managed));
    write(managed + "/geoip.dat", "managed geoip");
    const QString own = directory.filePath("own-geosite.dat");
    write(own, "own geosite");
    QSettings settings;
    using Source = GeoDataSelection::Source;
    if (name == "empty") {
        // A fresh install selects the shipped databases with automatic updates.
    } else if (name == "legacy") {
        settings.setValue("geodata/paths", QVariantMap{{"geoip", managed + "/geoip.dat"}, {"geosite", own}});
    } else if (name == "newer-bundled") {
        settings.setValue("geodata/paths", QVariantMap{{"geoip", managed + "/geoip.dat"}, {"geosite", own}});
        settings.setValue("geodata/sources", QVariantMap{{"geoip", "loyalsoldier"}, {"geosite", "none"}});
        settings.setValue("geodata/tags", QVariantMap{{"geoip", "202601010000"}});
    } else if (name == "missing-managed") {
        settings.setValue("geodata/paths", QVariantMap{{"geoip", directory.filePath("gone/geoip.dat")}});
        settings.setValue("geodata/sources", QVariantMap{{"geoip", "loyalsoldier"}, {"geosite", "custom"}});
        settings.setValue("geodata/tags", QVariantMap{{"geoip", "202612312359"}});
    }
    settings.sync();
    auto *selection = GeoDataSelection::instance();
    const auto choice = selection->selection();
    const QString bundledIp = QDir::cleanPath(bundled + "/geoip.dat"), bundledSite = QDir::cleanPath(bundled + "/geosite.dat");
    if (name == "empty") {
        require(choice.sources.value("geoip") == Source::Loyalsoldier && choice.sources.value("geosite") == Source::Loyalsoldier);
        require(choice.paths.value("geoip") == bundledIp && choice.paths.value("geosite") == bundledSite);
        require(choice.tags.value("geoip") == "202611112204" && choice.autoUpdate);
    } else if (name == "legacy") {
        // A managed download keeps its file; another path is the user's own file.
        require(choice.sources.value("geoip") == Source::Loyalsoldier && choice.paths.value("geoip") == managed + "/geoip.dat");
        require(choice.tags.value("geoip").isEmpty());
        require(choice.sources.value("geosite") == Source::Custom && choice.paths.value("geosite") == own);
    } else if (name == "newer-bundled") {
        require(choice.paths.value("geoip") == bundledIp && choice.tags.value("geoip") == "202611112204");
        require(choice.sources.value("geosite") == Source::None);
    } else if (name == "missing-managed") {
        require(choice.paths.value("geoip") == bundledIp && choice.tags.value("geoip") == "202611112204");
        require(choice.sources.value("geosite") == Source::Custom);
    }
    // The migrated state is persisted.
    require(QSettings().value("geodata/sources").toMap().size() == 2);
    return 0;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().value(1) == "--migration") return migrationCase(app.arguments().value(2));
    if (app.arguments().value(1) == "geodata" && qEnvironmentVariableIsSet("BIMERON_FAKE_GEODATA")) {
        if (qgetenv("BIMERON_FAKE_GEODATA") == "unsupported") { std::fputs("error: unknown command\n", stderr); return 1; }
        std::puts("{\"version\":1,\"groups\":[\"ru\"]}"); return 0;
    }
    if (app.arguments().value(1) == "geodata" && !qEnvironmentVariable("BIMERON_COUNT_LOG").isEmpty()) {
        QFile log(qEnvironmentVariable("BIMERON_COUNT_LOG"));
        require(log.open(QIODevice::Append)); log.write("catalog\n");
        std::puts("{\"version\":1,\"groups\":[\"ru\"]}"); return 0;
    }
    if (app.arguments().value(1) == "document" && !qEnvironmentVariable("BIMERON_COUNT_LOG").isEmpty()) {
        std::puts("{\"version\":1,\"profile\":\"fixture\",\"rules\":[{\"enabled\":true,\"matcher\":\"GeoIP\",\"value\":\"ru\"},{\"enabled\":true,\"matcher\":\"GeoIP\",\"value\":\"ru\"}]}"); return 0;
    }
    QTemporaryDir directory;
    require(directory.isValid());
    QCoreApplication::setOrganizationName("BimeronSelectionTest");
    QCoreApplication::setApplicationName("Selection");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    QString error;
    require(GeoDataSelection::inspect({}, "wrong", "relative", &error).isEmpty() && !error.isEmpty());
    auto *selection = GeoDataSelection::instance();
    int changes = 0;
    QObject::connect(selection, &GeoDataSelection::changed, [&] { ++changes; });
    const auto previous = selection->paths();
    const QString countLog = directory.filePath("count.log");
    const QString countData = directory.filePath("count.dat");
    const QString countProfile = directory.filePath("count.conf");
    write(countData, "fixture"); write(countProfile, "[Rule]\nGeoIP=ru\n");
    qputenv("BIMERON_COUNT_LOG", countLog.toUtf8());
    require(selection->apply({{"geoip", countData}}, app.applicationFilePath(),
        {{"a", "A", countProfile}, {"b", "B", countProfile}}, &error));
    require(read(countLog) == "catalog\n");
    require(selection->apply({{"geoip", countData}}, app.applicationFilePath(),
        {{"a", "A", countProfile}}, &error));
    require(read(countLog) == "catalog\ncatalog\n"); // Cache lifetime is one Apply.
    qunsetenv("BIMERON_COUNT_LOG");
    require(selection->apply(previous, {}, {}, &error)); changes = 0;

    {
        // Rule scanning and validation copies need no Core.
        const QString profile = "[Profile]\nName=P\n[Rule]\nGeoIP=ru\nAction=direct\n# [Rule]\n# GeoSite=github\n"
                                "# Action=proxy\n[Rule]\r\nGeoSite = youtube\r\nAction=proxy\r\n[Rule]\nCIDR=10.0.0.0/8\nAction=direct\n";
        require(GeoDataSelection::requiredKinds(profile) == QStringList({"geoip", "geosite"}));
        require(GeoDataSelection::requiredKinds("[Rule]\n# GeoIP=ru\n[GeoData]\nGeoIP=ru\n").isEmpty());
        const QString withoutSites = GeoDataSelection::withoutRules(profile, {"geosite"});
        require(GeoDataSelection::requiredKinds(withoutSites) == QStringList({"geoip"}));
        require(withoutSites.contains("# [Rule]\r\n# GeoSite = youtube\r\n# Action=proxy\r\n") && withoutSites.contains("CIDR=10.0.0.0/8"));
        require(GeoDataSelection::withoutRules(profile, {}) == profile);
        require(GeoDataSelection::releaseDate("202611112204") == "11.11.2026" && GeoDataSelection::releaseDate("x") == "x");
        for (const QString &name : {QStringLiteral("empty"), QStringLiteral("legacy"), QStringLiteral("newer-bundled"), QStringLiteral("missing-managed")}) {
            QProcess child;
            child.start(app.applicationFilePath(), {"--migration", name});
            require(child.waitForFinished(15000) && child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0);
        }
        // Sources persist with the selection, and a kind may be disabled even when a
        // profile uses it: such profiles are blocked when connecting instead.
        const QString used = directory.filePath("uses-geoip.conf");
        write(used, "[Rule]\nGeoIP=ru\nAction=direct\n");
        GeoDataSelection::Selection disabled = selection->selection();
        disabled.paths = {{"geoip", QString()}, {"geosite", QString()}};
        disabled.autoUpdate = false;
        require(selection->apply(disabled, {}, {{"u", "Uses", used}}, &error) && error.isEmpty());
        require(selection->source("geoip") == GeoDataSelection::Source::None && !selection->autoUpdate());
        require(QSettings().value("geodata/autoUpdate").toBool() == false
            && QSettings().value("geodata/sources").toMap().value("geoip").toString() == "none");
        disabled.autoUpdate = true;
        require(selection->apply(disabled, {}, {}, &error) && selection->autoUpdate());
        changes = 0;
    }
    {
        // Catalog classification from raw validator facts; no Core is required.
        using Failure = GeoDataSelection::CatalogFailure;
        using Run = GeoDataSelection::CatalogRun;
        const QString present = directory.filePath("present.dat"), absent = directory.filePath("absent.dat");
        write(present, "fixture");
        const auto classify = [&](const Run &run, const QString &path = {}) {
            return GeoDataSelection::classifyCatalog("validator", "geoip", path.isEmpty() ? present : path, run);
        };
        const auto finished = [](int code, const QByteArray &output, const QByteArray &errors = {}) {
            Run run; run.started = true; run.exitCode = code; run.output = output; run.errors = errors; return run;
        };
        Run run;
        require(classify(run).failure == Failure::ValidatorFailed && classify(run).diagnostic.contains("could not be started"));
        run.started = true; run.timedOut = true;
        require(classify(run).failure == Failure::ValidatorFailed && classify(run).diagnostic.contains("15 seconds"));
        run.timedOut = false; run.overLimit = true;
        require(classify(run).failure == Failure::ValidatorFailed && classify(run).diagnostic.contains("size limit"));
        run.overLimit = false; run.crashed = true;
        require(classify(run).failure == Failure::ValidatorFailed && classify(run).diagnostic.contains("stopped unexpectedly"));
        const auto unsupported = classify(finished(1, {}, "error: unknown command\nusage"));
        require(unsupported.failure == Failure::Unsupported && unsupported.diagnostic.contains("error: unknown command")
            && !unsupported.diagnostic.contains("usage") && unsupported.groups.isEmpty());
        require(classify(finished(0, "not json")).failure == Failure::ValidatorFailed);
        require(classify(finished(2, "")).failure == Failure::ValidatorFailed);
        const QByteArray rejected = R"({"version":1,"groups":[],"error":"request failed","details":{"code":"geodata_invalid"},)"
            R"("problems":[{"code":"geodata_invalid","message":"geoip: wire type 7)" "\x07" R"("}]})";
        const auto invalid = classify(finished(0, rejected));
        require(invalid.failure == Failure::Rejected && invalid.diagnostic.contains("Core code: geodata_invalid")
            && invalid.diagnostic.contains("wire type 7") && !invalid.diagnostic.contains(QChar(7)));
        const QByteArray unreadable = R"({"version":1,"groups":[],"error":"request failed","details":{"code":"geodata_unreadable"}})";
        require(classify(finished(0, unreadable)).failure == Failure::Unreadable);
        require(classify(finished(0, unreadable), absent).failure == Failure::Missing);
        const QByteArray unclassified = R"({"version":1,"groups":[],"error":"request failed","problems":[{"code":"unclassified"}]})";
        require(classify(finished(0, unclassified)).failure == Failure::ValidatorFailed);
        const auto ok = classify(finished(0, R"({"version":1,"groups":["ru","private"]})"));
        require(ok.failure == Failure::None && ok.groups == QStringList({"ru", "private"}) && ok.diagnostic.isEmpty());
        require(classify(finished(0, R"({"version":1,"groups":[]})")).failure == Failure::Rejected);
        require(classify(finished(0, R"({"version":1,"groups":["Bad Id"]})")).failure == Failure::ValidatorFailed);
        const auto bounded = classify(finished(0, QByteArray(R"({"version":1,"groups":[],"error":"x","details":{"code":"geodata_invalid"},"problems":[{"message":")")
            + QByteArray(5000, 'a') + R"("}]})"));
        require(bounded.failure == Failure::Rejected && bounded.diagnostic.size() < 1000);
        require(GeoDataSelection::precheckCatalog("validator", "geoip", absent).failure == Failure::Missing);
        // Directories cannot be opened as files on any supported platform.
        require(GeoDataSelection::precheckCatalog("validator", "geoip", directory.path()).failure == Failure::Unreadable);
        require(GeoDataSelection::precheckCatalog("validator", "geoip", present).failure == Failure::None);

        // Process level: this executable acts as the validator through BIMERON_FAKE_GEODATA.
        Failure failure = Failure::None;
        qputenv("BIMERON_FAKE_GEODATA", "unsupported");
        require(GeoDataSelection::inspect(app.applicationFilePath(), "geoip", present, &error, &failure).isEmpty()
            && failure == Failure::Unsupported && error.contains("unknown command"));
        require(GeoDataSelection::inspect(directory.filePath("no-such-validator"), "geoip", present, &error, &failure).isEmpty()
            && failure == Failure::ValidatorFailed);
        require(GeoDataSelection::inspect(app.applicationFilePath(), "geoip", absent, &error, &failure).isEmpty()
            && failure == Failure::Missing);
        // Apply reports an unsupported validator plainly and keeps the previous selection.
        require(!selection->apply({{"geoip", present}}, app.applicationFilePath(), {}, &error)
            && error.contains("unknown command") && selection->paths() == previous);

        // A validator replaced at the same path must not reuse an earlier cached result.
        const QFileInfo self(app.applicationFilePath());
        // The copy lives in the temporary directory; Windows finds its Qt libraries through PATH.
        const QString validator = directory.filePath("geodata-validator-copy-test"
            + (self.suffix().isEmpty() ? QString() : "." + self.suffix()));
        qputenv("PATH", QDir::toNativeSeparators(self.absolutePath()).toLocal8Bit()
            + QDir::listSeparator().toLatin1() + qgetenv("PATH"));
        QFile::remove(validator);
        { QFile source(app.applicationFilePath()); const bool copied = source.copy(validator); if (!copied) std::fprintf(stderr, "copy error: %s -> %s\n", qPrintable(source.errorString()), qPrintable(validator)); require(copied); }
        qputenv("BIMERON_FAKE_GEODATA", "ok");
        const QString validatorKey = GeoDataSelection::catalogKey(validator, "geoip", present);
        Failure delivered = Failure::Rejected;
        int deliveries = 0;
        QObject receiver;
        QObject::connect(selection, &GeoDataSelection::catalogResult, &receiver,
            [&](const QString &key, const QStringList &, const QString &, Failure value) {
                if (key == validatorKey) { delivered = value; ++deliveries; }
            });
        selection->requestCatalog(validator, "geoip", present, true);
        waitFor([&] { return deliveries == 1; });
        QStringList groups; QString diagnostic; Failure cached = Failure::Rejected;
        require(delivered == Failure::None && selection->cachedCatalog(validator, "geoip", present, &groups, &diagnostic, &cached)
            && cached == Failure::None);
        {
            QFile replaced(validator);
            require(replaced.open(QIODevice::ReadWrite)
                && replaced.setFileTime(QDateTime::currentDateTimeUtc().addSecs(120), QFileDevice::FileModificationTime));
        }
        require(!selection->cachedCatalog(validator, "geoip", present, &groups, &diagnostic, &cached));
        qputenv("BIMERON_FAKE_GEODATA", "unsupported");
        selection->requestCatalog(validator, "geoip", present); // Not forced: must still re-run.
        waitFor([&] { return deliveries == 2; });
        require(delivered == Failure::Unsupported);
        // Missing files are reported without starting a validator.
        const QString absentKey = GeoDataSelection::catalogKey(validator, "geoip", absent);
        Failure absentFailure = Failure::None;
        QObject::connect(selection, &GeoDataSelection::catalogResult, &receiver,
            [&](const QString &key, const QStringList &, const QString &, Failure value) { if (key == absentKey) absentFailure = value; });
        selection->requestCatalog(validator, "geoip", absent, true);
        require(!selection->catalogsBusy());
        waitFor([&] { return absentFailure == Failure::Missing; });
        selection->cancelCatalog(validatorKey);
        selection->cancelCatalog(absentKey);
        qunsetenv("BIMERON_FAKE_GEODATA");
        require(QFile::remove(validator));
    }
    // A file occupying the settings directory forces sync failure on every host.
    const QString blocked = directory.filePath("blocked");
    write(blocked, "file");
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, blocked);
    const QString core = qEnvironmentVariable("BIMERON_GEODATA_CORE");
    if (core.isEmpty()) {
        std::puts("SKIP: real Core bridge fixtures require BIMERON_GEODATA_CORE");
        return 0;
    }
    require(QFileInfo(core).isExecutable());
    const auto field = [](char tag, const QByteArray &value) {
        require(value.size() < 128); return QByteArray(1, tag) + QByteArray(1, char(value.size())) + value;
    };
    const auto group = [&](const QByteArray &id, const QByteArray &entry) {
        return field(0x0a, field(0x0a, id) + field(0x12, entry));
    };
    const QString ip = directory.filePath("geoip.dat"), site = directory.filePath("geosite.dat");
    write(ip, group("ru", field(0x0a, QByteArray::fromHex("cb007100")) + QByteArray::fromHex("1018")));
    write(site, group("github", QByteArray::fromHex("0802") + field(0x12, "github.example")));
    const GeoDataSelection::Paths paths{{"geoip", ip}, {"geosite", site}};
    require(GeoDataSelection::inspect(core, "geoip", ip, &error) == QStringList{"ru"} && error.isEmpty());
    require(GeoDataSelection::inspect(core, "geosite", site, &error) == QStringList{"github"} && error.isEmpty());
    require(GeoDataSelection::inspect(core, "geoip", site, &error).isEmpty() && !error.isEmpty());
    require(!selection->apply(paths, core, {}, &error) && !error.isEmpty());
    require(selection->paths() == previous && changes == 0);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    int catalogs = 0;
    QString catalogError; QStringList catalogGroups;
    const QString key = GeoDataSelection::catalogKey(core, "geoip", ip);
    QObject observer;
    QObject::connect(selection, &GeoDataSelection::catalogResult, &observer,
        [&](const QString &resultKey, const QStringList &groups, const QString &failure) {
            if (resultKey == key) { ++catalogs; catalogGroups = groups; catalogError = failure; }
        });
    selection->requestCatalog(core, "geoip", ip, true);
    selection->cancelCatalog(key);
    QCoreApplication::processEvents();
    require(catalogs == 0 && !selection->catalogsBusy() && read(ip).size() > 0);
    selection->requestCatalog(core, "geoip", ip, true);
    selection->requestCatalog(core, "geoip", ip, true); // Reuse one pending inspection.
    waitFor([&] { return catalogs == 1; });
    require(catalogGroups == QStringList{"ru"} && catalogError.isEmpty());
    QStringList cachedGroups; QString cachedError;
    require(selection->cachedCatalog(core, "geoip", ip, &cachedGroups, &cachedError));
    require(cachedGroups == QStringList{"ru"} && cachedError.isEmpty() && !selection->catalogsBusy());
    require(!selection->cachedCatalog(core, "geoip", directory.filePath("nonexistent.dat"), &cachedGroups, &cachedError));
    selection->requestCatalog(core, "geoip", ip); // Queue a cached delivery.
    selection->requestCatalog(core, "geoip", ip, true); // Supersede it before dispatch.
    waitFor([&] { return catalogs >= 2; });
    QCoreApplication::processEvents();
    require(catalogs == 2 && catalogGroups == QStringList{"ru"} && catalogError.isEmpty());
    const QString missing = directory.filePath("nonexistent.dat");
    const QString missingKey = GeoDataSelection::catalogKey(core, "geoip", missing);
    bool missingDone = false;
    QObject::connect(selection, &GeoDataSelection::catalogResult, &observer,
        [&](QString k, QStringList groups, QString failure) {
            if (k != missingKey) return;
            require(groups.isEmpty() && !failure.isEmpty()); missingDone = true;
        });
    selection->requestCatalog(core, "geoip", missing);
    waitFor([&] { return missingDone; });
    require(selection->cachedCatalog(core, "geoip", missing, &cachedGroups, &cachedError));
    require(cachedGroups.isEmpty() && !cachedError.isEmpty() && !QFile::exists(missing));
    for (const QString path : {ip, missing}) {
        const int watchers = selection->findChildren<QFileSystemWatcher *>().size();
        selection->cancelCatalog(GeoDataSelection::catalogKey(core, "geoip", path));
        require(!selection->cachedCatalog(core, "geoip", path, &cachedGroups, &cachedError));
        require(selection->findChildren<QFileSystemWatcher *>().size() == watchers - 1);
    }
    const QString bang = directory.filePath("bang-geosite.dat");
    write(bang, group("CATEGORY-GAMES-!CN", QByteArray::fromHex("0802") + field(0x12, "example.com")));
    require(GeoDataSelection::inspect(core, "geosite", bang, &error) == QStringList{"category-games-!cn"} && error.isEmpty());
    const QString bangKey = GeoDataSelection::catalogKey(core, "geosite", bang);
    bool bangDone = false;
    QObject::connect(selection, &GeoDataSelection::catalogResult, &observer,
        [&](QString k, QStringList groups, QString failure) {
            if (k == bangKey) { require(groups == QStringList{"category-games-!cn"} && failure.isEmpty()); bangDone = true; }
        });
    selection->requestCatalog(core, "geosite", bang, true);
    waitFor([&] { return bangDone; });
    const QString incompatible = directory.filePath("invalid-geosite.dat");
    write(incompatible, group("sites@cn", QByteArray::fromHex("0802") + field(0x12, "example.com")));
    QString incompatibleError;
    const QString incompatibleKey = GeoDataSelection::catalogKey(core, "geosite", incompatible);
    QObject::connect(selection, &GeoDataSelection::catalogResult, &observer,
        [&](QString k, QStringList, QString failure) { if (k == incompatibleKey) incompatibleError = failure; });
    selection->requestCatalog(core, "geosite", incompatible, true);
    waitFor([&] { return !incompatibleError.isEmpty(); });
    require(incompatibleError.contains("geodata_identifier_invalid") && incompatibleError.contains("Operation: check geosite database"));
    const QString base = "[Profile]\nVersion=1\nName=Bridge\n[AccessKey]\nType=shadowsocks\n"
        "Server=example.invalid\nPort=443\nMethod=aes-256-gcm\nPassword=SYNTHETIC\n"
        "[Routing]\nDefault=proxy\n";
    const QString bangProfile = base + "[Rule]\nGeoSite=category-games-!cn\nAction=proxy\n";
    const QString bangPath = directory.filePath("bang.conf");
    write(bangPath, bangProfile.toUtf8());
    const GeoDataSelection::Paths bangPaths{{"geosite", bang}};
    require(selection->apply(bangPaths, core, {{"bang", "Bang", bangPath}}, &error) && error.isEmpty());
    const QString bangSerialized = GeoDataSelection::materialize(bangProfile, bangPath, core, bangPaths, &error);
    require(!bangSerialized.isEmpty() && error.isEmpty() && bangSerialized.contains("GeoSite=category-games-!cn"));
    require(read(bangPath) == bangProfile.toUtf8());
    require(selection->apply(previous, core, {}, &error));
    changes = 0;
    const QString disabled = "# [Rule]\n# GeoSite=missing\n# Action=direct\n";
    const QString source = base + "# preserve comment\n[Rule]\nGeoIP=ru\nAction=direct\n" + disabled;
    const QString first = directory.filePath("first.conf"), second = directory.filePath("second.conf");
    write(first, source.toUtf8());
    write(second, (base + "[Rule]\nGeoSite=github\nAction=proxy\n").toUtf8());
    const QList<Profile> profiles{{"1", "First", first}, {"2", "Second", second}};
    require(selection->apply(paths, core, profiles, &error) && error.isEmpty());
    require(changes == 1 && selection->paths() == paths);
    require(QSettings().value("geodata/paths").toMap().value("geosite").toString() == site);
    require(read(first) == source.toUtf8());
    const auto patched = GeoDataSelection::materialize(source, first, core, paths, &error);
    require(error.isEmpty() && patched.contains(disabled) && patched.contains("# preserve comment"));
    require(patched.contains("GeoIPFile=") && !patched.contains("GeoSiteFile="));
    require(read(first) == source.toUtf8());
    require(GeoDataSelection::materialize(base + disabled, first, core, {}, &error) == base + disabled && error.isEmpty());
    QTemporaryDir validation;
    require(validation.isValid());
    const QString unavailableSource = directory.filePath("absent/source.conf");
    require(!GeoDataSelection::materialize(source, unavailableSource, core, paths, &error, validation.path()).isEmpty());
    require(error.isEmpty() && !QFile::exists(unavailableSource));
    require(GeoDataSelection::materialize(source, first, core, {{"geoip", "geoip.dat"}}, &error, validation.path()).isEmpty());
    require(!error.isEmpty() && read(first) == source.toUtf8());
    require(GeoDataSelection::materialize(source, first, "missing-core", paths, &error, validation.path()).isEmpty());
    require(QDir(validation.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
    const auto relative = GeoDataSelection::materialize(source, first, core, {{"geoip", "geoip.dat"}}, &error);
    require(!relative.isEmpty() && error.isEmpty());
    require(relative.contains("GeoIPFile=\"geoip.dat\""));
    QString invalid = source;
    invalid.replace("Port=443", "Port=invalid");
    require(GeoDataSelection::materialize(invalid, first, core, paths, &error).isEmpty() && !error.isEmpty());
    require(GeoDataSelection::materialize(source, first, core, {}, &error).isEmpty() && !error.isEmpty());
    {
        // Saving tolerates a missing database: rules stay, the kind is reported, and Core
        // validates the rest of the profile.
        QStringList skipped;
        const QString kept = GeoDataSelection::materialize(source, first, core, {}, &error, {}, &skipped);
        require(error.isEmpty() && kept == source && skipped == QStringList{"geoip"});
        QString broken = source;
        broken.replace("Port=443", "Port=invalid");
        require(GeoDataSelection::materialize(broken, first, core, {}, &error, {}, &skipped).isEmpty() && !error.isEmpty());
        // A missing group in an available database still blocks saving.
        const QString missingGroup = base + "[Rule]\nGeoIP=missing\nAction=direct\n";
        require(GeoDataSelection::materialize(missingGroup, first, core, paths, &error, {}, &skipped).isEmpty() && !error.isEmpty());
    }
    write(second, (base + "[Rule]\nGeoSite=missing\nAction=proxy\n").toUtf8());
    require(!selection->apply(paths, core, profiles, &error) && error.contains("Second"));
    require(selection->paths() == paths && changes == 1 && read(first) == source.toUtf8());
    write(second, QByteArray("\xff", 1));
    require(selection->apply(paths, core, profiles, &error) && error.isEmpty());
    write(second, QByteArray("\xff\n[Rule]\nGeoSite=github\nAction=proxy\n"));
    require(!selection->apply(paths, core, profiles, &error) && error.contains("UTF-8"));
    require(GeoDataSelection::materialize(source, first, "missing-core", paths, &error).isEmpty());
    require(!error.isEmpty());
    require(QDir(directory.path()).entryList({".bimeron-geodata-*"}, QDir::Files | QDir::Hidden).isEmpty());
    std::puts("GeoDataSelection tests passed");
    return 0;
}
