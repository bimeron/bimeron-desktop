#include "WireGuardCore.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QMutex>
#include <QProcess>
#include <QPromise>
#include <QThreadPool>
#include <memory>
#include <QUrl>

namespace WireGuardCore {
namespace {
constexpr qsizetype outputLimit = 32 * 1024 * 1024;

Runner &customRunner() {
    static Runner runner;
    return runner;
}

QJsonObject runProcess(const QString &executable, const QByteArray &input, QString *error, bool *definitive) {
    *definitive = false;
    QProcess process;
    process.start(executable, {QStringLiteral("wireguard")});
    if (!process.waitForStarted(2000)) {
        *error = QObject::tr("Core WireGuard bridge is unavailable. Use the WG/AWG config tab.");
        return {};
    }
    process.write(input);
    process.closeWriteChannel();
    QByteArray output, diagnostics;
    QElapsedTimer elapsed;
    elapsed.start();
    // Diagnostics are never shown or logged; a bounded prefix only recognizes an older CLI.
    const auto readDiagnostics = [&process, &diagnostics] {
        diagnostics += process.readAllStandardError().left(qMax<qsizetype>(0, 4096 - diagnostics.size()));
    };
    while (process.state() != QProcess::NotRunning && elapsed.elapsed() < 10000) {
        process.waitForReadyRead(50);
        output += process.readAllStandardOutput();
        readDiagnostics();
        if (output.size() > outputLimit) break;
    }
    if (process.state() != QProcess::NotRunning) {
        process.kill();
        process.waitForFinished(1000);
        *error = QObject::tr("Core WireGuard bridge timed out or exceeded its response limit. The draft has been retained.");
        return {};
    }
    output += process.readAllStandardOutput();
    readDiagnostics();
    QJsonParseError parse;
    const auto json = QJsonDocument::fromJson(output, &parse);
    const bool valid = output.size() <= outputLimit && process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0 && parse.error == QJsonParseError::NoError && json.isObject();
    // Only a valid reply or an older CLI's normal "unknown command" refusal is a
    // verdict. A crash or malformed output is transient and retried later.
    *definitive = valid || (process.exitStatus() == QProcess::NormalExit && process.exitCode() != 0
        && (output + diagnostics).toLower().contains("unknown command"));
    if (!valid) {
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
// A reply is usable only with the version-1 envelope and the result its
// operation promises, or with problems. Anything else is a transient fault.
bool wellFormed(const QString &op, const QJsonObject &object) {
    if (object.value("version").toInt() != 1) return false;
    const QJsonValue problems = object.value("problems");
    if (!problems.isUndefined() && !problems.isArray()) return false;
    if (!problems.toArray().isEmpty() && op != "edit") return true;
    if (op == "capabilities") return object.value("capabilities").toObject().value("supported").isBool();
    if (op == "generate_keypair")
        return !object.value("private_key").toString().isEmpty() && !object.value("public_key").toString().isEmpty();
    if (op == "public_key") return !object.value("public_key").toString().isEmpty();
    if (op == "generate_preshared_key") return !object.value("preshared_key").toString().isEmpty();
    if (op == "detect") {
        const QString format = object.value("format").toString();
        return format == "bimeron" || format == "wireguard";
    }
    if (op == "inspect" || op == "edit" || op == "import_uri") {
        if ((op == "edit" || op == "import_uri") && !object.value("text").isString()) return false;
        // Core omits an empty fields map, for example after removing the last key.
        return object.value("fields").isObject() || object.value("fields_error").isString()
            || (object.value("format").toString() == "wireguard" && object.value("fields").isUndefined());
    }
    return false;
}
Reply runRequest(const QString &executable, const QJsonObject &request, const Runner &runner) {
    Reply reply;
    const QByteArray input = QJsonDocument(request).toJson(QJsonDocument::Compact);
    if (input.size() > 1024 * 1024) {
        reply.error = QObject::tr("The WireGuard configuration exceeds the 64 KiB limit.");
        return reply;
    }
    if (executable.isEmpty() && !runner) {
        reply.error = QObject::tr("Core WireGuard bridge is unavailable. Use the WG/AWG config tab.");
        return reply;
    }
    bool definitive = true;
    const QJsonObject object = runner ? runner(executable, input, &reply.error)
                                              : runProcess(executable, input, &reply.error, &definitive);
    reply.definitive = definitive && (!runner || reply.error.isEmpty());
    if (!reply.error.isEmpty()) return reply;
    if (!wellFormed(request.value("op").toString(), object)) {
        reply.error = QObject::tr("Core WireGuard bridge returned an invalid response. The draft has been retained.");
        reply.definitive = false;
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

} // namespace

void setRunner(Runner runner) { customRunner() = std::move(runner); }

Reply request(const QString &executable, const QJsonObject &request) {
    return runRequest(executable, request, customRunner());
}

QFuture<QJsonObject> keyOperation(const QString &executable, const QJsonObject &request) {
    auto promise = std::make_shared<QPromise<QJsonObject>>();
    auto future = promise->future();
    promise->start();
    QThreadPool::globalInstance()->start([promise, executable, request, runner = customRunner()] {
        const auto reply = runRequest(executable, request, runner);
        promise->addResult(reply.clean() ? reply.object : QJsonObject());
        promise->finish();
    });
    return future;
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

bool cliSupported(const QString &executable, bool *definitive, QJsonArray *keyOperations, bool *reserved) {
    if (keyOperations) *keyOperations = {};
    if (reserved) *reserved = false;
    if (definitive) *definitive = true;
    if (executable.isEmpty() && !customRunner()) return false;
    const QString key = identity(executable);
    static QMutex mutex;
    static QHash<QString, QJsonObject> cache;
    if (!customRunner()) {
        QMutexLocker lock(&mutex);
        if (cache.contains(key)) {
            const auto caps = cache.value(key);
            if (keyOperations) *keyOperations = caps.value("key_operations").toArray();
            if (reserved) *reserved = caps.value("reserved").toBool();
            return !caps.isEmpty();
        }
    }
    const Reply reply = request(executable, {{"version", 1}, {"op", "capabilities"}});
    if (definitive) *definitive = reply.definitive;
    // document_api 0 (mobile packet backends) connects but offers no editing bridge.
    const QJsonObject capabilities = reply.object.value("capabilities").toObject();
    const bool supported = reply.answered() && reply.problems.isEmpty()
        && capabilities.value("supported").toBool() && capabilities.value("document_api").toInt() >= 1;
    if (supported && keyOperations) *keyOperations = capabilities.value("key_operations").toArray();
    if (supported && reserved) *reserved = capabilities.value("reserved").toBool();
    if (!customRunner() && reply.definitive) {
        QMutexLocker lock(&mutex);
        cache.insert(key, supported ? capabilities : QJsonObject());
    }
    return supported;
}

bool serviceSupported(const QJsonObject &capabilities) {
    return capabilities.value("wireguard").toObject().value("supported").toBool();
}

bool serviceSupportsReserved(const QJsonObject &capabilities) {
    return serviceSupported(capabilities) && capabilities.value("wireguard").toObject().value("reserved").toBool();
}

bool hasReserved(const QJsonObject &inspection) {
    if (inspection.value("fields").toObject().contains("Peer/Reserved")) return true;
    for (const auto &problem : parseCoreProblems(inspection.value("problems")))
        if (problem.section == "Peer" && problem.field == "Reserved") return true;
    return false;
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
