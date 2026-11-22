#pragma once

#include "CoreUpdate.h"

#include <QJsonObject>
#include <QRegularExpression>

namespace DesktopHandoffDirective {
inline QJsonObject verify(const QByteArray &bytes, const QByteArray &signature,
                          const QList<QByteArray> &testKeys, const QString &architecture) {
    QJsonObject object;
    if (!CoreUpdate::verifySignedJson(bytes, signature, testKeys, &object, nullptr)) return {};
    static const QRegularExpression digest(QStringLiteral(R"(\A[0-9a-f]{64}\z)"));
    if (object.value("schema") != QJsonValue(1) || object.value("kind") != QJsonValue("desktop_handoff_directive")
        || object.value("architecture") != QJsonValue(architecture)
        || (architecture != "amd64" && architecture != "arm64")
        || !object.value("helper_path").isString()
        || !digest.match(object.value("helper_sha256").toString()).hasMatch()
        || !digest.match(object.value("old_gui_sha256").toString()).hasMatch()) return {};
    return object;
}
}
