#pragma once

#include <QCoreApplication>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>

// Classifies a sanitized Core build identity (`version` output or IPC hello `core`)
// so About can distinguish official releases from development builds.
namespace CoreIdentity {

enum class Kind { Unknown, Release, Development };

inline QString normalizedVersion(QString version) {
    if (version.startsWith('v')) version.remove(0, 1);
    return version;
}

// Release builds report a non-dev version, their full commit and an unmodified tree.
// The pinned bundled release must additionally match its pinned commit; other
// releases cannot be verified offline and are accepted on those properties alone.
inline Kind classify(const QJsonObject &identity, const QString &pinnedVersion, const QString &pinnedRevision) {
    if (identity.isEmpty()) return Kind::Unknown;
    const QString version = normalizedVersion(identity.value("version").toString());
    const QString revision = identity.value("revision").toString();
    if (version.isEmpty() || version.contains("-dev")) return Kind::Development;
    if (!QRegularExpression(QStringLiteral("^[0-9a-f]{40}$")).match(revision).hasMatch()) return Kind::Development;
    if (!identity.value("modified").isBool() || identity.value("modified").toBool()) return Kind::Development;
    if (!pinnedVersion.isEmpty() && version == normalizedVersion(pinnedVersion)
        && revision != pinnedRevision) return Kind::Development;
    return Kind::Release;
}

inline bool sameBuild(const QJsonObject &left, const QJsonObject &right) {
    for (const char *key : {"version", "revision", "modified"})
        if (left.value(key) != right.value(key)) return false;
    return true;
}

inline QString label(const QJsonObject &identity, const QString &pinnedVersion, const QString &pinnedRevision) {
    switch (classify(identity, pinnedVersion, pinnedRevision)) {
    case Kind::Unknown:
        return QCoreApplication::translate("MainWindow", "unknown");
    case Kind::Release:
        return normalizedVersion(identity.value("version").toString());
    case Kind::Development:
        break;
    }
    const QString revision = identity.value("revision").toString();
    return revision.isEmpty() ? QStringLiteral("dev-core") : QStringLiteral("dev-core (%1)").arg(revision.left(7));
}

// The validation CLI is shown only when it is a different build than the runtime Core.
// While the runtime is unknown, only a development CLI is worth calling out.
inline QString aboutText(const QJsonObject &runtime, const QJsonObject &validation,
                         const QString &pinnedVersion, const QString &pinnedRevision) {
    const QString core = label(runtime, pinnedVersion, pinnedRevision);
    const Kind runtimeKind = classify(runtime, pinnedVersion, pinnedRevision);
    const Kind validationKind = classify(validation, pinnedVersion, pinnedRevision);
    if (validationKind == Kind::Unknown || sameBuild(runtime, validation)
        || (runtimeKind == Kind::Unknown && validationKind != Kind::Development))
        return QCoreApplication::translate("MainWindow", "Core: %1").arg(core);
    return QCoreApplication::translate("MainWindow", "Core: %1 · validation CLI: %2")
        .arg(core, label(validation, pinnedVersion, pinnedRevision));
}

} // namespace CoreIdentity
