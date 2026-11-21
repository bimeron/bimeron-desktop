#pragma once

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QLockFile>
#include <QRegularExpression>
#include <QSettings>

namespace CoreUpdateResume {
inline bool supported(const QJsonObject &capabilities) {
    return capabilities.value("kill_switch_update_hold") == QJsonValue(true)
        && capabilities.value("kill_switch_update_resume") == QJsonValue(true);
}

inline QJsonObject guard(const QJsonObject &status, const QJsonObject &capabilities, const QString &instance) {
    static const QRegularExpression digest(QStringLiteral(R"(\A[0-9a-f]{64}\z)"));
    const QString profile = status.value("ResidualProfile").toString();
    const QString generation = status.value("ResidualGeneration").toString();
    if (!supported(capabilities) || instance.isEmpty() || status.value("State") != QJsonValue("failed")
        || status.value("Session").toObject().value("active").toBool()
        || status.value("LastError") != QJsonValue("cleanup_failed_run_recover")
        || status.value("KillSwitch") != QJsonValue("residual")
        || status.value("ResidualReason") != QJsonValue("update")
        || !digest.match(profile).hasMatch() || !digest.match(generation).hasMatch()) return {};
    return {{"instance", instance}, {"residual_profile", profile}, {"residual_generation", generation}};
}

// Match Go's Windows filepath.Clean and simple Unicode lowercase mapping.
inline QString windowsProfileIdentity(const QString &path) {
    auto codePoints = QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toUcs4();
    for (auto &point : codePoints) {
        if (point == '/') point = '\\';
        // QString lowercase expands U+0130 to two code points. Go folds it to i.
        else if (point == 0x0130) point = 'i';
        else point = QChar::toLower(point);
    }
    const auto normalized = QString::fromUcs4(codePoints.constData(), codePoints.size());
    return QString::fromLatin1(QCryptographicHash::hash(normalized.toUtf8(), QCryptographicHash::Sha256).toHex());
}

inline bool claim(const QString &localDataRoot, const QJsonObject &guard) {
    if (localDataRoot.isEmpty() || !QDir().mkpath(localDataRoot)) return false;
    const QString journalPath = QDir(localDataRoot).filePath("update-resume.ini");
    QLockFile lock(journalPath + ".claim");
    if (!lock.tryLock(0)) return false;
    QSettings journal(journalPath, QSettings::IniFormat);
    const QString key = QStringLiteral("BimeronCore/%1/%2").arg(guard.value("residual_profile").toString(),
                                                               guard.value("residual_generation").toString());
    journal.sync();
    if (journal.status() != QSettings::NoError || journal.contains(key)) return false;
    // Consume the automatic attempt before validation and dispatch, even when
    // the outcome is unknown. Core independently authorizes the actual resume.
    journal.setValue(key, true);
    journal.sync();
    return journal.status() == QSettings::NoError;
}
}
