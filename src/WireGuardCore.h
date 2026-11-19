#pragma once

#include "CoreProblems.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <functional>

// Client of Core's `bimeron wireguard` bridge (Core docs/DESKTOP_DOCUMENT_API.MD).
// Desktop never interprets WireGuard/AmneziaWG text: format detection, decoded
// fields, edits, link conversion and validation all come from Core. Requests
// and responses travel through stdin/stdout only and are secret-bearing.
namespace WireGuardCore {

struct Reply {
    QJsonObject object;          // Version-1 response; empty when the bridge failed.
    QString error;               // Safe bridge failure text; empty when Core answered.
    QList<CoreProblem> problems; // Core problems (validation or operation failures).

    bool answered() const { return error.isEmpty(); }
    bool clean() const { return error.isEmpty() && problems.isEmpty(); }
    QString text() const { return object.value("text").toString(); }
    QString format() const { return object.value("format").toString(); }
    QJsonObject fields() const { return object.value("fields").toObject(); }
    QString fieldsError() const { return object.value("fields_error").toString(); }
    QJsonObject allowedIps() const { return object.value("allowed_ips").toObject(); }
};

// Executes one bridge request. The default runner starts `<executable> wireguard`.
using Runner = std::function<QJsonObject(const QString &executable, const QByteArray &request, QString *error)>;
void setRunner(Runner runner); // Tests inject a fake bridge; an empty runner restores the default.

Reply request(const QString &executable, const QJsonObject &request);
Reply detect(const QString &executable, const QString &text);
Reply inspect(const QString &executable, const QString &text);
Reply edit(const QString &executable, const QString &text, const QJsonArray &operations);
Reply importUri(const QString &executable, const QString &uri);
QJsonObject setField(const QString &section, const QString &key, const QString &value);

// True when the selected CLI answers the capabilities request. Cached per
// executable identity, so replacing the file at the same path is noticed.
bool cliSupported(const QString &executable);
// True when the running service advertises WireGuard support in hello.
bool serviceSupported(const QJsonObject &capabilities);
// "WireGuard" or "AmneziaWG <version>" from an inspect response.
QString protocolLabel(const QJsonObject &inspection);
// Scheme check only: routes Add-profile input to Core's link conversion.
bool linkScheme(const QString &text);

} // namespace WireGuardCore
