#pragma once
#include <QString>
#include <functional>
namespace RepairBootstrap::detail {
// Per-call payload and path policy seam; OS identity checks are unconditional.
quintptr authenticatedChild(quintptr parent, unsigned long pid, const QString &payloadPath,
    const std::function<bool(const QString &)> &trusted);
}
