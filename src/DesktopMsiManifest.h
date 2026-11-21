#pragma once

#include "CoreUpdate.h"

#include <QJsonObject>
#include <QRegularExpression>

namespace DesktopMsiManifest {
struct Artifact {
    QString name;
    QString architecture;
    QString productCode;
    QString packageCode;
    QString guiComponentCode;
    QString msiSha256;
    QString guiSha256;
    qint64 size = 0;
};

// desktopTrust is supplied by the Desktop release policy, independently of
// the Core update keys. One signed manifest describes one MSI artifact.
inline bool verify(const QByteArray &bytes, const QByteArray &signature,
                   const QList<QByteArray> &desktopTrust, const QString &releaseTag,
                   const QString &installedVersion, const QString &expectedArchitecture,
                   Artifact *artifact, QString *error) {
    if (artifact) *artifact = {};
    QJsonObject object;
    if (!CoreUpdate::verifySignedJson(bytes, signature, desktopTrust, &object, error)) return false;
    static const QRegularExpression guid(QStringLiteral(R"(\A\{[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}\}\z)"));
    static const QRegularExpression digest(QStringLiteral(R"(\A[0-9a-f]{64}\z)"));
    static const QRegularExpression stableVersion(QStringLiteral(R"(\A(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\z)"));
    const QString version = object.value("desktop_version").toString();
    const auto msi = object.value("msi").toObject();
    const double size = msi.value("size").toDouble(-1);
    Artifact candidate{msi.value("name").toString(), msi.value("architecture").toString(),
                       msi.value("product_code").toString(), msi.value("package_code").toString(),
                       msi.value("gui_component_code").toString(), msi.value("sha256").toString(),
                       msi.value("gui_sha256").toString()};
    const QString expectedName = QStringLiteral("bimeron-desktop-%1-windows-%2.msi").arg(version, expectedArchitecture);
    if (object.value("schema") != QJsonValue(1) || object.value("kind") != QJsonValue("desktop_msi")
        || object.value("tag").toString() != releaseTag || releaseTag != 'v' + version
        || !stableVersion.match(version).hasMatch() || !CoreUpdate::newerVersion(version, installedVersion)
        || (expectedArchitecture != "amd64" && expectedArchitecture != "arm64")
        || candidate.name != expectedName || candidate.architecture != expectedArchitecture
        || !guid.match(candidate.productCode).hasMatch() || !guid.match(candidate.packageCode).hasMatch()
        || !guid.match(candidate.guiComponentCode).hasMatch() || !digest.match(candidate.msiSha256).hasMatch()
        || !digest.match(candidate.guiSha256).hasMatch() || size < 1 || size > 64 * 1024 * 1024
        || size != qint64(size)) {
        if (error) *error = QStringLiteral("Invalid signed Desktop MSI identity");
        return false;
    }
    candidate.size = qint64(size);
    if (artifact) *artifact = candidate;
    return true;
}
}
