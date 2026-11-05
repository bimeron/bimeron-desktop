#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <optional>
#include "OfflineIKEv2FailureRaw.h"

// Disconnected Core-owned draft only. No protocol or lifecycle authority.
namespace OfflineIKEv2Failure {
enum class Result { Absent, Valid, Invalid };
enum class Stage { ProfileInspection, Dial, ConnectionStatus, CleanupHangup, CleanupStatus };
struct Record { quint32 code; Stage stage; qint32 attempt; };
struct Failure { std::optional<Record> operation, cleanup; };
inline bool integer(const QJsonValue &v, double maximum) {
    const double n = v.toDouble();
    return v.isDouble() && std::isfinite(n) && n >= 1 && n <= maximum && std::floor(n) == n;
}
inline Result parse(const QByteArray &bytes, bool present, qint64 attempt, Failure &output) {
    output = {};
    if (!present) return Result::Absent;
    if (!RawPreflight(bytes).valid() || attempt < 1 || attempt > 2147483647) return Result::Invalid;
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return Result::Invalid;
    const auto root = doc.object();
    if (!integer(root.value("version"), 1) || root.size() < 2) return Result::Invalid;
    Failure parsed;
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (it.key() == "version") continue;
        const bool cleanup = it.key() == "cleanup";
        if ((!cleanup && it.key() != "operation") || !it->isObject()) return Result::Invalid;
        const auto r = it->toObject();
        if (r.size() != 4 || r.value("domain") != QJsonValue("windows_ras") ||
            !integer(r.value("code"), 4294967295.0) || !integer(r.value("attempt"), 2147483647) ||
            r.value("attempt").toDouble() != attempt || !r.value("stage").isString()) return Result::Invalid;
        const auto stage = r.value("stage").toString();
        std::optional<Stage> value;
        if (!cleanup && stage == "profile_inspection") value = Stage::ProfileInspection;
        if (!cleanup && stage == "dial") value = Stage::Dial;
        if (!cleanup && stage == "connection_status") value = Stage::ConnectionStatus;
        if (cleanup && stage == "cleanup_hangup") value = Stage::CleanupHangup;
        if (cleanup && stage == "cleanup_status") value = Stage::CleanupStatus;
        if (!value) return Result::Invalid;
        (cleanup ? parsed.cleanup : parsed.operation) = Record{quint32(r.value("code").toDouble()), *value, qint32(attempt)};
    }
    output = parsed;
    return Result::Valid;
}
}
