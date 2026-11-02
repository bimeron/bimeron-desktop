#include "GeoDataDownload.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QDir>
#include <QPointer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QTimer>
#include <cstring>
#include <cstdio>
#include <functional>

static void require(bool value, const char *message) { if (!value) qFatal("%s", message); }
#include "GeoDataDownloadFixtures.h"
using GeoDataFixture::Network; using GeoDataFixture::Response;
using GeoDataFixture::metadata; using GeoDataFixture::checksum;
static void waitFor(const std::function<bool()> &done) {
    QElapsedTimer timer; timer.start();
    while (!done() && timer.elapsed() < 3000) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    require(done(), "asynchronous fixture timed out");
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("GeoDataDownloadTest");
    QStandardPaths::setTestModeEnabled(true);
    if (app.arguments().contains("--real-probe")) {
        GeoDataDownload download;
        QEventLoop loop;
        QObject::connect(&download, &GeoDataDownload::failed, &app, [&](QString, QString error) {
            std::fprintf(stderr, "REAL FAILURE: %s\n", qPrintable(error)); loop.quit();
        });
        QObject::connect(&download, &GeoDataDownload::candidateReady, &app, [&](QString kind, QString path, QString hash) {
            std::printf("REAL VERIFIED: %s tag=%s bytes=%lld SHA256=%s\n", qPrintable(kind),
                qPrintable(download.releaseTag(kind)), static_cast<long long>(QFileInfo(path).size()), qPrintable(hash));
            download.cancel(kind); loop.quit();
        });
        download.start(app.arguments().contains("--geosite") ? "geosite" : "geoip"); loop.exec(); return 0;
    }
    const QByteArray data("synthetic database fixture");
    {
        Network network;
        network.responses = {{metadata()}, {checksum(data)}, {data}};
        GeoDataDownload download(&network);
        QString candidate, hash;
        int finished = 0;
        QObject::connect(&download, &GeoDataDownload::candidateReady, &app, [&](QString, QString p, QString h) { candidate = p; hash = h; });
        QObject::connect(&download, &GeoDataDownload::finished, &app, [&](QString) { ++finished; });
        require(!download.start("invalid"), "invalid kind accepted");
        require(download.start("geoip") && !download.start("geoip"), "duplicate operation accepted");
        waitFor([&] { return !candidate.isEmpty(); });
        require(download.busy("geoip") && QFile::exists(candidate), "candidate ownership ended before validation");
        require(network.urls.size() == 3 && network.urls[1].path().contains("/20261101/")
                && network.urls[2].path().contains("/20261101/"), "assets were not pinned to one release");
        QString error;
        const QString path = download.publish("geoip", &error);
        require(!path.isEmpty() && error.isEmpty() && path.contains(hash + "/geoip.dat"), "publication failed");
        require(!download.busy("geoip") && finished == 1, "publication lifecycle failed");
        QFile file(path); require(file.open(QIODevice::ReadOnly) && file.readAll() == data, "published bytes differ");
        file.close();
        require(!QFile::exists(candidate), "published staging file leaked");
        // Reuse identical content without replacing an existing immutable file.
        network.responses = {{metadata()}, {checksum(data)}, {data}};
        candidate.clear(); download.start("geoip"); waitFor([&] { return !candidate.isEmpty(); });
        require(download.publish("geoip", &error) == path, "content deduplication failed");
        QFile::remove(path); // Only the synthetic file owned by this test.
    }
    auto failure = [&](QList<Response> responses, const QString &expected, int requests) {
        Network network; network.responses = std::move(responses);
        GeoDataDownload download(&network);
        QString error; bool ready = false;
        QObject::connect(&download, &GeoDataDownload::failed, &app, [&](QString, QString message) { error = message; });
        QObject::connect(&download, &GeoDataDownload::candidateReady, &app, [&](QString, QString, QString) { ready = true; });
        download.start("geoip"); waitFor([&] { return !download.busy("geoip"); });
        require(error.contains(expected) && !ready && network.urls.size() == requests, "failure handling mismatch");
        require(error.contains("Source:") && error.contains("Phase:") && !error.contains("SYNTHETIC_")
            && !error.contains("token="), "diagnostics exposed network credentials or omitted context");
    };
    failure({{{}, 429, {}, -1, false, false, QNetworkReply::UnknownContentError}}, "HTTP 429", 1);
    failure({{{}, 404, {}, -1, false, false, QNetworkReply::ContentNotFoundError}}, "HTTP 404", 1);
    auto mixed = metadata();
    mixed.replace("20261101/geoip.dat.sha256sum", "20261031/geoip.dat.sha256sum");
    failure({{mixed}}, "exact tag", 1);
    auto missing = QJsonDocument::fromJson(metadata()).object();
    missing["assets"] = QJsonArray{missing["assets"].toArray().first()};
    failure({{QJsonDocument(missing).toJson()}}, "missing", 1);
    failure({{QByteArray(1024 * 1024 + 1, 'x')}}, "size limit", 1);
    failure({{{}, 200, {}, 1024 * 1024 + 1}}, "size limit", 1);
    failure({{{}, 302, QUrl("http://github.com/no")}}, "untrusted", 1);
    failure({{{}, 302, QUrl("https://github.com.evil.invalid/no")}}, "untrusted", 1);
    failure({{{}, 302, QUrl("https://github.com:444/no")}}, "untrusted", 1);
    failure({{{}, 200, {}, -1, false, true}}, "TLS", 1);
    QList<Response> loop;
    for (int i = 0; i < 6; ++i) loop.append({{}, 302, QUrl("https://github.com/loop")});
    failure(loop, "redirect limit", 6);
    failure({{metadata()}, {QByteArray(4097, 'x')}}, "size limit", 2);
    failure({{metadata()}, {checksum("different")}, {data}}, "mismatch", 3);
    failure({{metadata()}, {"not a checksum"}}, "checksum", 2);
    failure({{metadata()}, {checksum(data)}, {QByteArray(64 * 1024 * 1024 + 1, 'x'), 200, {}, 1}}, "size limit", 3);
    {
        Network network; network.responses = {{{}, 200, {}, -1, true}, {metadata()}, {checksum(data)}, {data}};
        GeoDataDownload download(&network);
        int ready = 0, errors = 0, finished = 0;
        QString candidate;
        QObject::connect(&download, &GeoDataDownload::candidateReady, &app, [&](QString, QString path, QString) { ++ready; candidate = path; });
        QObject::connect(&download, &GeoDataDownload::failed, &app, [&](QString, QString) { ++errors; });
        QObject::connect(&download, &GeoDataDownload::finished, &app, [&](QString) { ++finished; });
        download.start("geoip"); download.cancel("geoip");
        require(!download.busy("geoip") && finished == 1, "cancel did not end operation");
        download.start("geoip");
        network.replies.first()->finish(); // Late completion of the canceled same-kind job.
        require(download.busy("geoip") && ready == 0 && errors == 0, "stale completion changed replacement");
        waitFor([&] { return ready == 1; });
        download.discard("geoip"); QCoreApplication::processEvents();
        require(errors == 0 && finished == 2 && !QFile::exists(candidate), "cancel/retry/discard leaked candidate or callbacks");
    }
    {
        Network network; network.responses = {{metadata()}, {checksum(data)}, {data}};
        QString candidate;
        auto download = std::make_unique<GeoDataDownload>(&network);
        QObject::connect(download.get(), &GeoDataDownload::candidateReady, &app, [&](QString, QString path, QString) { candidate = path; });
        download->start("geoip"); waitFor([&] { return !candidate.isEmpty(); });
        download.reset(); require(!QFile::exists(candidate), "destruction leaked temporary file");
    }
    {
        Network network;
        network.responses = {{metadata()}, {checksum(data)}, {{}, 302, QUrl("https://release-assets.githubusercontent.com/fixture")}, {data}};
        GeoDataDownload download(&network);
        QString candidate;
        QObject::connect(&download, &GeoDataDownload::candidateReady, &app, [&](QString, QString path, QString) { candidate = path; });
        download.start("geoip"); waitFor([&] { return !candidate.isEmpty(); });
        QFile file(candidate);
        require(file.open(QIODevice::ReadOnly) && file.readAll() == data && network.urls.size() == 4,
                "redirected data was not verified intact");
    }
    {
        Network network;
        // Both metadata requests precede either checksum request.
        network.responses = {{metadata()}, {metadata("geosite")}, {checksum(data)}, {checksum(data, "geosite")}, {data}, {data}};
        GeoDataDownload download(&network);
        int ready = 0;
        QObject::connect(&download, &GeoDataDownload::candidateReady, &app, [&](QString, QString, QString) { ++ready; });
        require(download.start("geoip") && download.start("geosite"), "different kinds could not run together");
        waitFor([&] { return ready == 2; });
        download.cancel("geoip");
        require(!download.busy("geoip") && download.busy("geosite"), "cancel crossed kind boundary");
    }
    {
        Network network; network.responses = {{metadata()}, {checksum(data)}, {data, 200, {}, -1, true}};
        GeoDataDownload download(&network);
        const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/geodata";
        QDir().mkpath(directory);
        const auto before = QDir(directory).entryList({"candidate-*"}, QDir::Files);
        qint64 transferred = 0;
        QObject::connect(&download, &GeoDataDownload::progress, &app, [&](QString, qint64 bytes, qint64) { transferred = bytes; });
        download.start("geoip"); waitFor([&] { return network.urls.size() == 3; });
        network.replies.last()->readyRead();
        require(transferred == data.size() && download.busy("geoip"), "download did not stage partial data");
        // Linux QTemporaryFile may remain unnamed until fileName() is requested.
        download.cancel("geoip"); QCoreApplication::processEvents();
        require(QDir(directory).entryList({"candidate-*"}, QDir::Files) == before, "cancel during data transfer leaked partial file");
    }
    for (const auto &name : {QStringLiteral("deadline"), QStringLiteral("inactivity")}) {
        Network network; network.responses = {{{}, 200, {}, -1, true}};
        GeoDataDownload download(&network);
        QString error; int finished = 0;
        QObject::connect(&download, &GeoDataDownload::failed, &app, [&](QString, QString message) { error = message; });
        QObject::connect(&download, &GeoDataDownload::finished, &app, [&](QString) { ++finished; });
        download.start("geoip");
        auto *timer = download.findChild<QTimer *>("geoip/" + name);
        require(timer && timer->interval() == (name == "deadline" ? 120000 : 30000), "production deadline changed");
        require(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection), "could not trigger timeout");
        require(!download.busy("geoip") && finished == 1 && error.contains(name == "deadline" ? "deadline" : "inactive"),
                "timeout did not terminate pending request");
    }
    {
        Network network; network.responses = {{metadata()}, {checksum(data)}, {data}};
        GeoDataDownload download(&network);
        QString candidate, hash;
        QObject::connect(&download, &GeoDataDownload::candidateReady, &app,
                         [&](QString, QString path, QString digest) { candidate = path; hash = digest; });
        download.start("geoip"); waitFor([&] { return !candidate.isEmpty(); });
        const QString directory = QFileInfo(candidate).absolutePath() + '/' + hash;
        require(QDir().mkpath(directory), "could not prepare publication failure");
        const QString path = directory + "/geoip.dat";
        QFile cache(path);
        require(cache.open(QIODevice::WriteOnly) && cache.write("prior cache") == 11, "could not prepare prior cache");
        cache.close();
        QString error;
        require(download.publish("geoip", &error).isEmpty() && !error.isEmpty() && download.busy("geoip")
                && QFile::exists(candidate), "publication failure lost candidate");
        require(cache.open(QIODevice::ReadOnly) && cache.readAll() == "prior cache", "publication failure replaced cache");
        cache.close(); require(QFile::remove(path), "could not remove synthetic cache");
        // A directory in place of the destination exercises the disk publication path.
        require(QDir().mkpath(path), "could not block publication path");
        require(download.publish("geoip", &error).isEmpty() && QFile::exists(candidate), "disk failure lost candidate");
        require(QDir().rmdir(path), "could not remove synthetic directory");
        require(!download.publish("geoip", &error).isEmpty(), "publication retry failed");
        require(QFile::remove(path), "could not remove published fixture");
    }
    {
        Network network; network.responses = {{{}, 200, {}, -1, true}};
        auto download = std::make_unique<GeoDataDownload>(&network);
        int callbacks = 0;
        QObject::connect(download.get(), &GeoDataDownload::failed, &app, [&](QString, QString) { ++callbacks; });
        QObject::connect(download.get(), &GeoDataDownload::candidateReady, &app, [&](QString, QString, QString) { ++callbacks; });
        download->start("geoip");
        download.reset();
        network.replies.first()->finish();
        QCoreApplication::processEvents();
        require(callbacks == 0, "pending destruction delivered callbacks");
    }
    {
        Network network; network.responses = {{metadata()}, {checksum(data)}, {data}};
        GeoDataDownload download(&network);
        QString candidate, error;
        int finished = 0, published = 0;
        QObject::connect(&download, &GeoDataDownload::candidateReady, &app,
                         [&](QString, QString path, QString) { candidate = path; });
        QObject::connect(&download, &GeoDataDownload::failed, &app, [&](QString, QString message) { error = message; });
        QObject::connect(&download, &GeoDataDownload::finished, &app, [&](QString) { ++finished; });
        QObject::connect(&download, &GeoDataDownload::published, &app, [&](QString, QString) { ++published; });
        require(!download.publishAsync("geoip"), "publication without candidate accepted");
        download.start("geoip"); waitFor([&] { return !candidate.isEmpty(); });
        QFile changed(candidate);
        require(changed.open(QIODevice::WriteOnly | QIODevice::Truncate) && changed.write("changed") == 7,
                "could not mutate candidate");
        changed.close();
        require(download.publishAsync("geoip"), "async failure did not start");
        waitFor([&] { return finished == 1; });
        require(error.contains("changed") && published == 0 && !download.busy("geoip") && !QFile::exists(candidate),
                "async failure did not fail and clean up");
    }
    {
        // The exact ceiling must publish while the GUI event loop continues running.
        const QByteArray large(64 * 1024 * 1024, 'x');
        Network network; network.responses = {{metadata()}, {checksum(large)}, {large}};
        GeoDataDownload download(&network);
        QString candidate;
        QObject::connect(&download, &GeoDataDownload::candidateReady, &app, [&](QString, QString path, QString) { candidate = path; });
        download.start("geoip"); waitFor([&] { return !candidate.isEmpty(); });
        QString path;
        int ticks = 0, finished = 0;
        qint64 maximumGap = 0;
        QElapsedTimer timer, gap; timer.start(); gap.start();
        QTimer heartbeat;
        heartbeat.setInterval(1);
        QObject::connect(&heartbeat, &QTimer::timeout, &app, [&] {
            ++ticks; maximumGap = qMax(maximumGap, gap.restart());
        });
        QObject::connect(&download, &GeoDataDownload::finished, &app, [&](QString) { ++finished; });
        QObject::connect(&download, &GeoDataDownload::published, &app, [&](QString kind, QString published) {
            require(kind == "geoip" && !download.busy(kind), "published before discard");
            path = published;
        });
        heartbeat.start();
        require(download.publishAsync("geoip") && !download.publishAsync("geoip"), "async duplicate accepted");
        const qint64 startTime = timer.elapsed();
        waitFor([&] { return !path.isEmpty(); });
        heartbeat.stop();
        std::printf("64 MiB async publication: %lld ms; start: %lld ms; timer ticks: %d; maximum gap: %lld ms\n",
                    static_cast<long long>(timer.elapsed()), static_cast<long long>(startTime), ticks,
                    static_cast<long long>(maximumGap));
        require(ticks > 5 && maximumGap < 250 && finished == 1, "publication blocked event loop");
        require(QFileInfo(path).size() == large.size() && !QFile::exists(candidate), "exact-limit publication failed");
        // Cancellation and destruction race an active worker; its staging file stays alive until joined.
        for (bool destroy : {false, true}) {
            if (destroy) require(QFile::remove(path), "could not prepare absent-cache cancellation");
            Network pending; pending.responses = {{metadata()}, {checksum(large)}, {large}};
            auto active = std::make_unique<GeoDataDownload>(&pending);
            QString staged;
            int published = 0, errors = 0;
            QObject::connect(active.get(), &GeoDataDownload::candidateReady, &app,
                             [&](QString, QString p, QString) { staged = p; });
            QObject::connect(active.get(), &GeoDataDownload::published, &app, [&](QString, QString) { ++published; });
            QObject::connect(active.get(), &GeoDataDownload::failed, &app, [&](QString, QString) { ++errors; });
            active->start("geoip"); waitFor([&] { return !staged.isEmpty(); });
            require(active->publishAsync("geoip"), "could not start cancellable publication");
            QTimer::singleShot(5, &app, [&] {
                if (destroy) active.reset(); else active->cancel("geoip");
            });
            waitFor([&] { return !active || !active->busy("geoip"); });
            QCoreApplication::processEvents();
            require(published == 0 && errors == 0 && !QFile::exists(staged), "canceled publication leaked callbacks or staging");
            if (destroy && !QFile::exists(path)) continue;
            QFile preserved(path);
            require(preserved.open(QIODevice::ReadOnly)
                    && QCryptographicHash::hash(preserved.readAll(), QCryptographicHash::Sha256)
                       == QCryptographicHash::hash(large, QCryptographicHash::Sha256), "cancel changed existing bytes");
        }
        if (QFile::exists(path)) require(QFile::remove(path), "could not remove large synthetic fixture");
    }
    return 0;
}
