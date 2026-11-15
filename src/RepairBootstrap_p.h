#pragma once
#include <QString>
#include <functional>
namespace RepairBootstrap::detail {
QString packageDirectory(const QString &gui);
bool provision(const QString &target, const QByteArray &data,
    const std::function<bool(const QString &)> &trustedDirectory,
    const std::function<bool(const QString &)> &trustedFile, QString *error);
// Per-call payload and path policy seam; OS identity checks are unconditional.
quintptr authenticatedChild(quintptr parent, unsigned long pid, const QString &payloadPath,
    const std::function<bool(const QString &)> &trusted);
}
