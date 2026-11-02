#include "GeoDataSelection.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <functional>
#include <cstdio>
#include <cstdlib>

static void require(bool value) { if (!value) std::abort(); }
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

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
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
    require(incompatibleError.contains("geodata_identifier_invalid") && incompatibleError.contains("Phase: catalog validation"));
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
    const auto relative = GeoDataSelection::materialize(source, first, core, {{"geoip", "geoip.dat"}}, &error);
    require(!relative.isEmpty() && error.isEmpty());
    require(relative.contains("GeoIPFile=\"geoip.dat\""));
    QString invalid = source;
    invalid.replace("Port=443", "Port=invalid");
    require(GeoDataSelection::materialize(invalid, first, core, paths, &error).isEmpty() && !error.isEmpty());
    require(GeoDataSelection::materialize(source, first, core, {}, &error).isEmpty() && !error.isEmpty());
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
