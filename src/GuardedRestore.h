#pragma once

#include <QJsonObject>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QLockFile>
#include <QSettings>

namespace GuardedRestore {
inline QJsonObject request(const QJsonObject &guard, const QString &version, const QString &digest,
                           bool disruption, bool recovery) {
    quint32 words[4];
    QRandomGenerator::system()->generate(words, words + 4);
    const auto id = QByteArray(reinterpret_cast<const char *>(words), sizeof(words)).toHex();
    return {{"version", 2}, {"command", "restore_held"}, {"operation_id", QString::fromLatin1(id)},
            {"expected_instance", guard.value("instance")}, {"profile_id", guard.value("residual_profile")},
            {"filter_generation", guard.value("residual_generation")}, {"disruption", disruption},
            {"network_recovery", recovery}, {"target", QJsonObject{{"version", version}, {"arch", "amd64"}, {"sha256", digest}}}};
}

// Compare and change under one interprocess lock, including the disk sync.
inline bool changePending(const QString &path, const QString &expected, const QString &next) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QLockFile lock(path + ".claim");
    if (!lock.tryLock(0)) return false;
    QSettings journal(path, QSettings::IniFormat);
    journal.sync();
    if (journal.status() != QSettings::NoError || journal.value("operation").toString() != expected)
        return false;
    if (next.isEmpty()) journal.remove("operation");
    else journal.setValue("operation", next);
    journal.sync();
    return journal.status() == QSettings::NoError;
}

inline bool rejectedBeforeIntent(const QJsonObject &event, const QString &dispatchedId, bool pinnedPeer) {
    return pinnedPeer && !dispatchedId.isEmpty() && event == QJsonObject{
        {"version", 2}, {"operation_id", dispatchedId}, {"stage", "inspect"}, {"done", true},
        {"outcome", "unknown"}, {"current_state", "unavailable"}, {"code", "proof_unavailable"}};
}

inline QString outcome(const QJsonObject &proof, const QString &id) {
    if (proof.size() != 5 || proof.value("version") != QJsonValue(2) || proof.value("operation_id") != id
        || proof.value("code").toString().isEmpty()) return "unknown";
    const auto state = proof.value("current_state");
    const auto outcome = proof.value("outcome");
    if (outcome == QJsonValue("restored") && state == QJsonValue("clear")) return "restored";
    if (outcome == QJsonValue("held") && state == QJsonValue("held")) return "held";
    return "unknown";
}
}
