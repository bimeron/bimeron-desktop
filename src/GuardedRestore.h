#pragma once

#include <QJsonObject>
#include <QRandomGenerator>

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
