#include "CoreUpdate.h"

#include "monocypher-ed25519.h"
#include "miniz.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace {
bool fail(QString *error, const QString &message) {
    if (error) *error = message;
    return false;
}

const QRegularExpression versionPattern(QStringLiteral(R"(^([0-9]+)\.([0-9]+)\.([0-9]+)(?:-([0-9A-Za-z.-]+))?$)"));
const QRegularExpression digestPattern(QStringLiteral("^[0-9a-f]{64}$"));
const QRegularExpression assetPattern(QStringLiteral(R"(^bimeron-v[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?-(windows|linux)-(amd64|arm64)\.zip$)"));

int compareVersions(const QString &a, const QString &b, bool *valid) {
    const auto ma = versionPattern.match(a), mb = versionPattern.match(b);
    *valid = ma.hasMatch() && mb.hasMatch();
    if (!*valid) return 0;
    for (int i = 1; i <= 3; ++i) {
        bool aOk = false, bOk = false;
        const auto av = ma.captured(i).toULongLong(&aOk), bv = mb.captured(i).toULongLong(&bOk);
        if (!aOk || !bOk) { *valid = false; return 0; }
        if (av != bv) return av < bv ? -1 : 1;
    }
    const QString as = ma.captured(4), bs = mb.captured(4);
    if (as.isEmpty() || bs.isEmpty()) return as.isEmpty() == bs.isEmpty() ? 0 : (as.isEmpty() ? 1 : -1);
    const auto ap = as.split('.'), bp = bs.split('.');
    for (int i = 0; i < qMin(ap.size(), bp.size()); ++i) {
        bool an = false, bn = false;
        const auto ai = ap[i].toULongLong(&an), bi = bp[i].toULongLong(&bn);
        if (an && bn && ai != bi) return ai < bi ? -1 : 1;
        if (an != bn) return an ? -1 : 1;
        const int cmp = QString::compare(ap[i], bp[i], Qt::CaseSensitive);
        if (cmp) return cmp < 0 ? -1 : 1;
    }
    return ap.size() == bp.size() ? 0 : (ap.size() < bp.size() ? -1 : 1);
}

bool safeAssetName(const QString &name) {
    return assetPattern.match(name).hasMatch() && name.toLatin1().size() == name.size();
}
}

bool CoreUpdate::newerVersion(const QString &candidate, const QString &installed) {
    bool valid = false;
    const int result = compareVersions(candidate, installed, &valid);
    return valid && result > 0;
}

bool CoreUpdate::verifyManifest(const QByteArray &bytes, const QByteArray &signature,
                                const QByteArray &publicKey, const QString &releaseTag,
                                const QString &guiVersion, int ipcVersion, int profileFormat,
                                const QString &os, const QString &arch, bool betaAllowed,
                                CoreUpdateRelease *result, QString *error) {
    if (publicKey.size() != 32 || bytes.isEmpty() || bytes.size() > 64 * 1024 || !bytes.endsWith('\n') ||
        !signature.endsWith('\n') || signature.count('\n') != 1)
        return fail(error, QStringLiteral("Invalid update signature or signing key"));
    const QByteArray encoded = signature.left(signature.size() - 1);
    const QByteArray decoded = QByteArray::fromBase64(encoded);
    if (decoded.size() != 64 || decoded.toBase64() != encoded)
        return fail(error, QStringLiteral("Invalid update signature encoding"));
    // Parse only the untrusted key ID to select the pinned key; trust no manifest field yet.
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(error, QStringLiteral("Invalid update manifest JSON"));
    const auto object = document.object();
    const QByteArray keyId = QCryptographicHash::hash(publicKey, QCryptographicHash::Sha256).left(8).toHex();
    if (object.value("key_id").toString().toLatin1() != keyId)
        return fail(error, QStringLiteral("Unknown update signing key"));
    if (crypto_ed25519_check(reinterpret_cast<const uint8_t *>(decoded.constData()),
                             reinterpret_cast<const uint8_t *>(publicKey.constData()),
                             reinterpret_cast<const uint8_t *>(bytes.constData()), size_t(bytes.size())) != 0)
        return fail(error, QStringLiteral("Update signature verification failed"));

    if (object.value("schema").toInt() != 1 || object.value("tag").toString() != releaseTag ||
        !releaseTag.startsWith('v') || object.value("core_version").toString() != releaseTag.mid(1))
        return fail(error, QStringLiteral("Unsupported update manifest or release tag"));
    const QString version = object.value("core_version").toString();
    if (!versionPattern.match(version).hasMatch())
        return fail(error, QStringLiteral("Invalid core version"));
    const QString channel = object.value("channel").toString();
    if (channel != (version.contains('-') ? "beta" : "stable") || (channel == "beta" && !betaAllowed))
        return fail(error, QStringLiteral("Update channel is not enabled"));
    const auto compatibility = object.value("compatibility").toObject();
    if (compatibility.value("schema").toInt() != 1 || compatibility.value("gui_reviewed") != true ||
        !compatibility.value("gui_min").isString() || !compatibility.value("gui_max_exclusive").isString() ||
        compatibility.value("gui_min").toString().isEmpty() || compatibility.value("gui_max_exclusive").toString().isEmpty() ||
        !compatibility.value("ipc_min").isDouble() || !compatibility.value("ipc_max").isDouble() ||
        !compatibility.value("profile_format").isDouble())
        return fail(error, QStringLiteral("Unreviewed or incomplete update compatibility"));
    bool minValid = false, maxValid = false;
    const int minCmp = compareVersions(guiVersion, compatibility.value("gui_min").toString(), &minValid);
    const int maxCmp = compareVersions(guiVersion, compatibility.value("gui_max_exclusive").toString(), &maxValid);
    if (!minValid || !maxValid || minCmp < 0 || maxCmp >= 0 ||
        compatibility.value("ipc_min").toDouble() != compatibility.value("ipc_min").toInt() ||
        compatibility.value("ipc_max").toDouble() != compatibility.value("ipc_max").toInt() ||
        compatibility.value("profile_format").toDouble() != compatibility.value("profile_format").toInt() ||
        compatibility.value("ipc_min").toInt() < 1 ||
        compatibility.value("ipc_max").toInt() < compatibility.value("ipc_min").toInt() ||
        ipcVersion < compatibility.value("ipc_min").toInt() || ipcVersion > compatibility.value("ipc_max").toInt() ||
        profileFormat != compatibility.value("profile_format").toInt())
        return fail(error, QStringLiteral("Core release is incompatible with this desktop"));
    const auto archives = object.value("archives").toArray();
    if (archives.size() != 4)
        return fail(error, QStringLiteral("Incomplete update target list"));
    QSet<QString> targets;
    CoreUpdateRelease selected;
    for (const auto &value : archives) {
        if (!value.isObject()) return fail(error, QStringLiteral("Invalid update target"));
        const auto a = value.toObject();
        const QString targetOs = a.value("os").toString(), targetArch = a.value("arch").toString();
        const QString target = targetOs + '-' + targetArch;
        const QString name = a.value("name").toString();
        const QString expected = QStringLiteral("bimeron-%1-%2.zip").arg(releaseTag, target);
        const QString digest = a.value("sha256").toString();
        const double size = a.value("size").toDouble(-1);
        if ((targetOs != "windows" && targetOs != "linux") || (targetArch != "amd64" && targetArch != "arm64") ||
            targets.contains(target) || !safeAssetName(name) || name != expected ||
            !digestPattern.match(digest).hasMatch() || size < 1 || size > 100 * 1024 * 1024 || qint64(size) != size)
            return fail(error, QStringLiteral("Invalid or duplicate update target"));
        targets.insert(target);
        if (targetOs == os && targetArch == arch)
            selected = {releaseTag, version, name, qint64(size), QByteArray::fromHex(digest.toLatin1())};
    }
    if (targets.size() != 4 || selected.assetName.isEmpty())
        return fail(error, QStringLiteral("No update for this platform"));
    if (result) *result = selected;
    return true;
}

bool CoreUpdate::extractExecutable(const QByteArray &zip, const CoreUpdateRelease &release,
                                   const QString &os, const QString &arch,
                                   QByteArray *executable, QString *error) {
    if (zip.size() != release.assetSize ||
        QCryptographicHash::hash(zip, QCryptographicHash::Sha256) != release.assetSha256)
        return fail(error, QStringLiteral("Update archive size or hash mismatch"));
    mz_zip_archive archive{};
    if (!mz_zip_reader_init_mem(&archive, zip.constData(), size_t(zip.size()), 0))
        return fail(error, QStringLiteral("Invalid update ZIP"));
    const QByteArray wanted = QStringLiteral("%1-%2/bimeron%3").arg(os, arch, os == "windows" ? ".exe" : "").toLatin1();
    int found = -1;
    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&archive); ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&archive, i, &stat)) { mz_zip_reader_end(&archive); return fail(error, QStringLiteral("Invalid ZIP entry")); }
        const QByteArray name(stat.m_filename);
        if (name.isEmpty() || name.startsWith('/') || name.contains('\\') || name.contains(':') ||
            name.split('/').contains("..") || name.size() >= MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE - 1) {
            mz_zip_reader_end(&archive);
            return fail(error, QStringLiteral("Unsafe ZIP entry name"));
        }
        if (name == wanted) {
            const mz_uint32 unixMode = stat.m_external_attr >> 16;
            if (found >= 0 || mz_zip_reader_is_file_a_directory(&archive, i) ||
                (unixMode & 0170000) == 0120000 || stat.m_is_encrypted || !stat.m_is_supported ||
                stat.m_uncomp_size == 0 || stat.m_uncomp_size > 64 * 1024 * 1024) {
                mz_zip_reader_end(&archive);
                return fail(error, QStringLiteral("Duplicate or invalid core executable"));
            }
            found = int(i);
        }
    }
    if (found < 0) { mz_zip_reader_end(&archive); return fail(error, QStringLiteral("Core executable missing from ZIP")); }
    mz_zip_archive_file_stat stat{};
    mz_zip_reader_file_stat(&archive, mz_uint(found), &stat);
    QByteArray data(qsizetype(stat.m_uncomp_size), Qt::Uninitialized);
    const bool ok = mz_zip_reader_extract_to_mem(&archive, mz_uint(found), data.data(), size_t(data.size()), 0);
    mz_zip_reader_end(&archive);
    if (!ok) return fail(error, QStringLiteral("Could not extract core executable"));
    if (executable) *executable = data;
    return true;
}
