#define WIN32_LEAN_AND_MEAN
#include "installer-preflight-digest.h"
#include "DesktopMsiManifest.h"
#include <msi.h>
#include <msiquery.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <cstdio>

namespace {
struct Identity {
    QString product;
    QString package;
    QString component;
    QString path;
    QString digest;
};

struct InstallerHandle {
    MSIHANDLE value = 0;
    ~InstallerHandle() { if (value) MsiCloseHandle(value); }
    InstallerHandle() = default;
    InstallerHandle(const InstallerHandle &) = delete;
    InstallerHandle &operator=(const InstallerHandle &) = delete;
};

bool matchesFile(const QString &path, const QString &digest) {
    return QFileInfo(path).isAbsolute() && InstallerPreflight::matchesPinnedDigest(
        QDir::toNativeSeparators(path).toStdWString(), digest.toStdString());
}

Identity identity(const QJsonObject &json) {
    return {json.value("product").toString(), json.value("package").toString(),
            json.value("component").toString(), json.value("path").toString(), json.value("sha256").toString()};
}

bool validIdentity(const Identity &value) {
    static const QRegularExpression guid(QStringLiteral(R"(\A\{[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}\}\z)"));
    static const QRegularExpression digest(QStringLiteral(R"(\A[0-9a-f]{64}\z)"));
    return guid.match(value.product).hasMatch() && guid.match(value.package).hasMatch()
        && guid.match(value.component).hasMatch() && QFileInfo(value.path).isAbsolute()
        && QFileInfo(value.path).fileName().compare("bimeron-desktop.exe", Qt::CaseInsensitive) == 0
        && digest.match(value.digest).hasMatch();
}

QString query(MSIHANDLE database, const wchar_t *sql) {
    InstallerHandle view, record;
    if (MsiDatabaseOpenViewW(database, sql, &view.value) != ERROR_SUCCESS
        || MsiViewExecute(view.value, 0) != ERROR_SUCCESS
        || MsiViewFetch(view.value, &record.value) != ERROR_SUCCESS) return {};
    InstallerHandle duplicate;
    if (MsiViewFetch(view.value, &duplicate.value) != ERROR_NO_MORE_ITEMS) return {};
    wchar_t result[1024]{};
    DWORD length = std::size(result);
    if (MsiRecordGetStringW(record.value, 1, result, &length) != ERROR_SUCCESS) return {};
    return QString::fromWCharArray(result, length);
}

QString summary(MSIHANDLE database, UINT property) {
    InstallerHandle info;
    if (MsiGetSummaryInformationW(database, nullptr, 0, &info.value) != ERROR_SUCCESS) return {};
    UINT type = 0;
    wchar_t result[1024]{};
    DWORD length = std::size(result);
    if (MsiSummaryInfoGetPropertyW(info.value, property, &type, nullptr, nullptr, result, &length) != ERROR_SUCCESS)
        return {};
    return QString::fromWCharArray(result, length);
}

QByteArray resource(const QString &name) {
    QFile file(":/handoff/" + name);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

InstallerPreflight::FileHandle verifiedOffer(const QJsonObject &receipt, const Identity &next) {
    const QString path = receipt.value("msi_path").toString();
    const QString digest = receipt.value("msi_sha256").toString();
    const QString architecture = receipt.value("architecture").toString();
#if defined(_M_ARM64)
    if (architecture != "arm64") return {};
    const QString platform = "Arm64";
#elif defined(_M_X64)
    if (architecture != "amd64") return {};
    const QString platform = "x64";
#else
    return {};
#endif
    QList<QByteArray> keys;
    DesktopMsiManifest::Artifact signedArtifact;
    if (!CoreUpdate::parseTrustedKeys(resource("public-key").trimmed(), &keys)
        || !DesktopMsiManifest::verify(resource("manifest"), resource("signature"), keys,
                                      receipt.value("release_tag").toString(),
                                      receipt.value("installed_version").toString(),
                                      architecture, &signedArtifact, nullptr)
        || signedArtifact.name != QFileInfo(path).fileName() || signedArtifact.size != QFileInfo(path).size()
        || signedArtifact.msiSha256 != digest || signedArtifact.guiSha256 != next.digest
        || signedArtifact.productCode != next.product || signedArtifact.packageCode != next.package
        || signedArtifact.guiComponentCode != next.component)
        return {};

    if (!QFileInfo(path).isAbsolute()) return {};
    auto verified = InstallerPreflight::openPinnedFile(QDir::toNativeSeparators(path).toStdWString(), digest.toStdString());
    InstallerHandle database;
    if (!verified || MsiOpenDatabaseW(reinterpret_cast<LPCWSTR>(path.utf16()), nullptr, &database.value) != ERROR_SUCCESS)
        return {};
    if (query(database.value, L"SELECT `Value` FROM `Property` WHERE `Property` = 'ProductCode'") != next.product
        || query(database.value, L"SELECT `Value` FROM `Property` WHERE `Property` = 'ProductVersion'") != receipt.value("release_tag").toString().mid(1)
        || summary(database.value, 9) != next.package
        || summary(database.value, 7).section(';', 0, 0).compare(platform, Qt::CaseInsensitive) != 0
        || query(database.value, L"SELECT `ComponentId` FROM `Component` WHERE `KeyPath` = 'CM_FP_bimeron_desktop.exe'") != next.component)
        return {};
    return verified;
}

bool installedMatches(const Identity &expected) {
    const auto product = expected.product.toStdWString();
    const auto component = expected.component.toStdWString();
    wchar_t package[64]{};
    DWORD packageLength = std::size(package);
    if (MsiGetProductInfoExW(product.c_str(), nullptr, MSIINSTALLCONTEXT_MACHINE, L"PackageCode",
                            package, &packageLength) != ERROR_SUCCESS ||
        QString::fromWCharArray(package, packageLength) != expected.package) return false;
    wchar_t path[1024]{};
    DWORD pathLength = std::size(path);
    if (MsiGetComponentPathExW(product.c_str(), component.c_str(), nullptr, MSIINSTALLCONTEXT_MACHINE,
                              path, &pathLength) != INSTALLSTATE_LOCAL) return false;
    const auto registeredPath = QString::fromWCharArray(path, pathLength);
    return QDir::toNativeSeparators(registeredPath).compare(
        QDir::toNativeSeparators(expected.path), Qt::CaseInsensitive) == 0
        && matchesFile(registeredPath, expected.digest);
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().size() != 1) return 2;
    QFile file(":/handoff/receipt.json");
    if (!file.open(QIODevice::ReadOnly)) return 3;
    const auto receipt = QJsonDocument::fromJson(file.readAll()).object();
    const auto old = identity(receipt.value("old_gui").toObject());
    const auto next = identity(receipt.value("new_gui").toObject());
    const auto verifiedMsi = validIdentity(old) && validIdentity(next) && old.product != next.product
        && old.package != next.package ? verifiedOffer(receipt, next) : InstallerPreflight::FileHandle{};
    if (!verifiedMsi) {
        std::puts("lab_offer_refused");
        return 4;
    }
    std::puts("lab_offer_verified");
    std::puts(installedMatches(old) ? "old_registered_image_verified" : "old_registered_image_unverified");
    std::puts(installedMatches(next) ? "new_registered_image_verified" : "new_registered_image_unverified");
    return 0;
}
