#include "../src/GuardedRestore.h"
#include <QRegularExpression>
#include <cstdlib>

int main() {
    const QString digest(64, 'a');
    const QJsonObject guard{{"instance", "instance"}, {"residual_profile", digest}, {"residual_generation", digest}};
    const auto request = GuardedRestore::request(guard, "0.5.0", digest, true, true);
    const auto id = request.value("operation_id").toString();
    if (!QRegularExpression("^[0-9a-f]{32}$").match(id).hasMatch() || request.value("profile_id") != digest
        || request.value("filter_generation") != digest || request.value("target").toObject().value("sha256") != digest)
        return EXIT_FAILURE;
    QJsonObject proof{{"operation_id", id}, {"version", 2}, {"code", "ok"}, {"outcome", "restored"}, {"current_state", "clear"}};
    if (GuardedRestore::outcome(proof, id) != "restored" || GuardedRestore::outcome(proof, "other") != "unknown") return EXIT_FAILURE;
    proof["current_state"] = "held";
    if (GuardedRestore::outcome(proof, id) != "unknown") return EXIT_FAILURE;
    proof["outcome"] = "held";
    if (GuardedRestore::outcome(proof, id) != "held") return EXIT_FAILURE;
    proof.remove("version");
    return GuardedRestore::outcome(proof, id) == "unknown" ? EXIT_SUCCESS : EXIT_FAILURE;
}
