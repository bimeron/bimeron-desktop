#pragma once

// Whether a staged, verified Core update is installed now, kept for later, or
// discarded at each step of the install flow (bimeron-desktop #68). A staged
// file is discarded only when it no longer applies to the running service;
// anything temporary postpones it for the rest of the session.
namespace CoreUpdateStaging {

enum class Action { Proceed, Postpone, Discard };

// Before the confirmation opens. `applies` is meaningful only once the
// connection is synchronized, so a busy connection postpones first.
inline Action beforePrompt(bool prerequisitesMet, bool connectionBusy, bool applies) {
    if (!prerequisitesMet || connectionBusy) return Action::Postpone;
    return applies ? Action::Proceed : Action::Discard;
}

// After the confirmation closes. Declining postpones. A changed connection in the
// same service instance postpones because the confirmed text described another state.
inline Action afterPrompt(bool confirmed, bool connectionBusy, bool applies, bool sameConnection) {
    if (!confirmed || connectionBusy) return Action::Postpone;
    if (!applies) return Action::Discard;
    return sameConnection ? Action::Proceed : Action::Postpone;
}

// The update command did not start, so nothing was elevated.
inline Action afterStartFailure(bool argumentsBuilt) {
    return argumentsBuilt ? Action::Postpone : Action::Discard;
}

// The privileged helper exited. Only a canceled Linux authorization (pkexec 126)
// leaves the service untouched; success installed the file and a failure leaves
// the service state to be checked again.
inline Action afterHelper(int exitCode, bool linuxHelper) {
    return linuxHelper && exitCode == 126 ? Action::Postpone : Action::Discard;
}

} // namespace CoreUpdateStaging
