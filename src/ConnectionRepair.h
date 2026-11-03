#pragma once
#include "CoreClient.h"
#include "DevelopmentCore.h"

namespace ConnectionRepair {
enum class Service { Unknown, Missing, Stopped, Running, Transitional, Paused, Failed };
enum class Phase { Diagnosing, AwaitingConsent, Executing, Verifying, Ready, Failed };
enum class Cause { Checking, ComponentsMissing, ComponentsUntrusted, Cleanup, Interrupted, Local, Ready,
    Conflict, Permission, Incompatible, Untrusted, Missing, Stopped, Unresponsive, Transition, StopFailed, Unknown };
enum class Action { None, Restore };
struct Snapshot {
    qint64 observedAt = 0;
    quint64 generation = 0;
    bool helperAvailable = false;
    Service service = Service::Unknown;
    CoreClient::Failure ipcFailure = CoreClient::Failure::Transport;
    DevelopmentCore::StartBlock conflict = DevelopmentCore::StartBlock::None;
    bool ready = false, checking = false, executing = false, verifying = false;
    bool cleanup = false, interrupted = false, localFailed = false, failed = false;
    bool stopFailed = false;
    bool helperTrusted = true;
};
struct Diagnosis { Cause cause; Action action; Phase phase; };
inline Diagnosis diagnose(const Snapshot &s) {
    const Phase phase = s.executing ? Phase::Executing : s.verifying ? Phase::Verifying
        : s.checking ? Phase::Diagnosing : s.ready ? Phase::Ready
        : s.failed ? Phase::Failed : Phase::AwaitingConsent;
    if (s.ready) return {Cause::Ready, Action::None, phase};
    if (s.executing || s.checking || s.verifying) return {Cause::Checking, Action::None, phase};
    if (!s.helperAvailable) return {Cause::ComponentsMissing, Action::Restore, phase};
    if (!s.helperTrusted) return {Cause::ComponentsUntrusted, Action::Restore, phase};
    if (s.stopFailed) return {Cause::StopFailed, Action::Restore, phase};
    if (s.cleanup)
        return {Cause::Cleanup, Action::Restore, phase};
    if (s.interrupted) return {Cause::Interrupted, Action::Restore, phase};
    if (s.localFailed) return {Cause::Local, Action::Restore, phase};
    if (s.conflict != DevelopmentCore::StartBlock::None) return {Cause::Conflict, Action::Restore, phase};
    if (s.ipcFailure == CoreClient::Failure::AccessDenied) return {Cause::Permission, Action::Restore, phase};
    if (s.ipcFailure == CoreClient::Failure::Incompatible) return {Cause::Incompatible, Action::Restore, phase};
    if (s.ipcFailure == CoreClient::Failure::PeerMismatch || s.ipcFailure == CoreClient::Failure::InvalidResponse)
        return {Cause::Untrusted, Action::Restore, phase};
    if (s.service == Service::Missing) return {Cause::Missing, Action::Restore, phase};
    if (s.service == Service::Stopped) return {Cause::Stopped, Action::Restore, phase};
    if (s.service == Service::Running) return {Cause::Unresponsive, Action::Restore, phase};
    if (s.service == Service::Transitional) return {Cause::Transition, Action::Restore, phase};
    return {Cause::Unknown, Action::Restore, phase};
}
}
