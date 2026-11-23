#include "../src/GuardedRestore.h"
#include <QRegularExpression>
#include <cstdlib>
#include <QTemporaryDir>

int main() {
    const QString digest(64, 'a');
    const QJsonObject guard{{"instance", "instance"}, {"residual_profile", digest}, {"residual_generation", digest}};
    const auto request = GuardedRestore::request(guard, "0.5.0", digest, true, true);
    const auto id = request.value("operation_id").toString();
    if (!QRegularExpression("^[0-9a-f]{32}$").match(id).hasMatch() || request.value("profile_id") != digest
        || request.value("filter_generation") != digest || request.value("target").toObject().value("sha256") != digest)
        return EXIT_FAILURE;
    QTemporaryDir dir;
    const QString path = dir.filePath("journal.ini");
    const QJsonObject refusal{{"version", 2}, {"operation_id", id}, {"stage", "inspect"}, {"done", true},
                             {"outcome", "unknown"}, {"current_state", "unavailable"}, {"code", "proof_unavailable"}};
    if (!dir.isValid() || !GuardedRestore::changePending(path, {}, id)
        || GuardedRestore::changePending(path, {}, id)
        || !GuardedRestore::rejectedBeforeIntent(refusal, id, true)
        || GuardedRestore::rejectedBeforeIntent(refusal, id, false)
        || GuardedRestore::rejectedBeforeIntent(refusal, "other", true)) return EXIT_FAILURE;
    for (const auto &key : refusal.keys()) {
        auto changed = refusal;
        changed[key] = key == "done" ? QJsonValue(false) : QJsonValue("different");
        if (GuardedRestore::rejectedBeforeIntent(changed, id, true)) return EXIT_FAILURE;
    }
    // A disconnect on either side of publication carries no terminal evidence.
    // Opening the journal in a new instance still prevents a second dispatch.
    if (GuardedRestore::rejectedBeforeIntent({}, id, true)
        || QSettings(path, QSettings::IniFormat).value("operation").toString() != id
        || !GuardedRestore::changePending(path, id, {}) || !GuardedRestore::changePending(path, {}, "later")
        || GuardedRestore::changePending(path, id, {})) return EXIT_FAILURE;
    QJsonObject proof{{"operation_id", id}, {"version", 2}, {"code", "ok"}, {"outcome", "restored"}, {"current_state", "clear"}};
    if (GuardedRestore::outcome(proof, id) != "restored" || GuardedRestore::outcome(proof, "other") != "unknown") return EXIT_FAILURE;
    proof["current_state"] = "held";
    if (GuardedRestore::outcome(proof, id) != "unknown") return EXIT_FAILURE;
    proof["outcome"] = "held";
    if (GuardedRestore::outcome(proof, id) != "held") return EXIT_FAILURE;
    proof.remove("version");
    return GuardedRestore::outcome(proof, id) == "unknown" ? EXIT_SUCCESS : EXIT_FAILURE;
}
