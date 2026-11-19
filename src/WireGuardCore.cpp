#include "WireGuardCore.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QMutex>
#include <QProcess>
#include <QUrl>

namespace WireGuardCore {
namespace {
constexpr qsizetype outputLimit = 32 * 1024 * 1024;

Runner &customRunner() {
    static Runner runner;
    return runner;
}

QJsonObject runProcess(const QString &executable, const QByteArray &input, QString *error) {
    QProcess process;
    process.start(executable, {QStringLiteral("wireguard")});
    if (!process.waitForStarted(2000)) {
        *error = QObject::tr("Core WireGuard bridge is unavailable. Use the WG/AWG config tab.");
        return {};
    }
    process.write(input);
    process.closeWriteChannel();
    QByteArray output;
    QElapsedTimer elapsed;
    elapsed.start();
    while (process.state() != QProcess::NotRunning && elapsed.elapsed() < 10000) {
        process.waitForReadyRead(50);
        output += process.readAllStandardOutput();
        process.readAllStandardError(); // Diagnostics are never shown or logged.
        if (output.size() > outputLimit) break;
    }
    if (process.state() != QProcess::NotRunning) {
        process.kill();
        process.waitForFinished(1000);
        *error = QObject::tr("Core WireGuard bridge timed out or exceeded its response limit. The draft has been retained.");
        return {};
    }
    output += process.readAllStandardOutput();
    QJsonParseError parse;
    const auto json = QJsonDocument::fromJson(output, &parse);
    if (output.size() > outputLimit || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0
        || parse.error != QJsonParseError::NoError || !json.isObject()) {
        *error = QObject::tr("Core WireGuard bridge returned an invalid response. The draft has been retained.");
        return {};
    }
    return json.object();
}

QString identity(const QString &path) {
    const QFileInfo info(path);
    return info.absoluteFilePath() + '\n' + QString::number(info.size()) + '\n'
        + QString::number(info.lastModified().toMSecsSinceEpoch());
}
} // namespace

void setRunner(Runner runner) { customRunner() = std::move(runner); }

Reply request(const QString &executable, const QJsonObject &request) {
    Reply reply;
    const QByteArray input = QJsonDocument(request).toJson(QJsonDocument::Compact);
    if (input.size() > 1024 * 1024) {
        reply.error = QObject::tr("The WireGuard configuration exceeds the 64 KiB limit.");
        return reply;
    }
    if (executable.isEmpty() && !customRunner()) {
        reply.error = QObject::tr("Core WireGuard bridge is unavailable. Use the WG/AWG config tab.");
        return reply;
    }
    const QJsonObject object = customRunner() ? customRunner()(executable, input, &reply.error)
                                              : runProcess(executable, input, &reply.error);
    if (!reply.error.isEmpty()) return reply;
    if (object.value("version").toInt() != 1) {
        reply.error = QObject::tr("Core WireGuard bridge returned an invalid response. The draft has been retained.");
        return reply;
    }
    reply.object = object;
    reply.problems = parseCoreProblems(object.value("problems"));
    // A malformed problem list still means Core refused the request.
    if (reply.problems.isEmpty() && !object.value("problems").toArray().isEmpty()) {
        CoreProblem problem;
        problem.code = QStringLiteral("unclassified");
        reply.problems.append(problem);
    }
    return reply;
}

Reply detect(const QString &executable, const QString &text) {
    return request(executable, {{"version", 1}, {"op", "detect"}, {"text", text}});
}

Reply inspect(const QString &executable, const QString &text) {
    return request(executable, {{"version", 1}, {"op", "inspect"}, {"text", text}});
}

Reply edit(const QString &executable, const QString &text, const QJsonArray &operations) {
    return request(executable, {{"version", 1}, {"op", "edit"}, {"text", text}, {"operations", operations}});
}

Reply importUri(const QString &executable, const QString &uri) {
    return request(executable, {{"version", 1}, {"op", "import_uri"}, {"uri", uri}});
}

QJsonObject setField(const QString &section, const QString &key, const QString &value) {
    return {{"op", "set_field"}, {"section", section}, {"key", key}, {"value", value}};
}

bool cliSupported(const QString &executable) {
    if (executable.isEmpty() && !customRunner()) return false;
    const QString key = identity(executable);
    static QMutex mutex;
    static QHash<QString, bool> cache;
    if (!customRunner()) {
        QMutexLocker lock(&mutex);
        if (cache.contains(key)) return cache.value(key);
    }
    const Reply reply = request(executable, {{"version", 1}, {"op", "capabilities"}});
    const bool supported = reply.answered() && reply.problems.isEmpty()
        && reply.object.value("capabilities").toObject().value("supported").toBool();
    if (!customRunner()) {
        QMutexLocker lock(&mutex);
        cache.insert(key, supported);
    }
    return supported;
}

bool serviceSupported(const QJsonObject &capabilities) {
    return capabilities.value("wireguard").toObject().value("supported").toBool();
}

QString protocolLabel(const QJsonObject &inspection) {
    if (inspection.value("protocol").toString() != "amneziawg") return QStringLiteral("WireGuard");
    const QString version = inspection.value("awg_version").toString();
    static const QStringList known{"1.0", "1.5", "2.0", "3.0", "3.1"};
    return known.contains(version) ? QStringLiteral("AmneziaWG ") + version : QStringLiteral("AmneziaWG");
}

bool linkScheme(const QString &text) {
    const QString value = text.trimmed();
    const QString scheme = value.section("://", 0, 0).toLower();
    return value.contains("://") && (scheme == "vpn" || scheme == "wireguard" || scheme == "wg");
}

} // namespace WireGuardCore
