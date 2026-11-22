#pragma once

#include "ReconnectPolicy.h"

#include <QCoreApplication>
#include <QString>

// Confirmation text for installing a downloaded Core update (bimeron-desktop #60 follow-up).
namespace CoreUpdatePrompt {

struct Prompt {
    QString text;
    bool reconnect = false; // the same profile is started again after a verified update
};

inline QString text(const char *source) {
    return QCoreApplication::translate("CoreUpdatePrompt", source);
}

// A session counts as active while it connects, is connected or reconnects, and
// also in a failed run that still holds kill switch protection: the update stops
// it either way, and Desktop starts the same profile again afterwards.
inline bool sessionActive(const QString &coreState, const ReconnectPolicy::State &reconnect) {
    return coreState == QLatin1String("connected") || coreState == QLatin1String("connecting")
        || coreState == QLatin1String("reconnecting")
        || (coreState == QLatin1String("failed") && reconnect.protectionHeld);
}

inline Prompt prompt(const QString &version, const QString &coreState,
                     const ReconnectPolicy::State &reconnect, bool proxy, bool keepProtection = false) {
    Prompt result;
    result.reconnect = sessionActive(coreState, reconnect);
    if (!result.reconnect) {
        result.text = text("Core %1 is downloaded and verified. Update the service now?").arg(version);
    } else if (reconnect.sessionProtected && keepProtection) {
        result.text = text("Installing Core %1 will interrupt the VPN while keeping leak protection in place. "
                           "Bimeron will try to resume the protected profile. Update now?").arg(version);
    } else if (reconnect.sessionProtected) {
        // The service restart releases the protection that this session owns.
        result.text = text("Installing Core %1 will interrupt the active VPN. Leak protection will be removed during "
                           "the update, and internet traffic will bypass the VPN. Bimeron will reconnect the same "
                           "profile after a successful update. Update now?").arg(version);
    } else {
        result.text = (proxy
            ? text("Installing Core %1 will interrupt the active proxy. Bimeron will reconnect the same profile after a successful update. Update now?")
            : text("Installing Core %1 will interrupt the active VPN. Bimeron will reconnect the same profile after a successful update. Update now?"))
            .arg(version);
    }
    return result;
}

} // namespace CoreUpdatePrompt
