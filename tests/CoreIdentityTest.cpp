#include "CoreIdentity.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QRegularExpression>

#include <cstdio>

namespace {
const QString pinnedVersion = QStringLiteral("v0.3.0-beta.2");
const QString pinnedRevision = QStringLiteral("66d84d8f4dcfc6ada625eb11894b19feea51b7ba");
const QString otherRevision = QStringLiteral("6b5340a1c0ffee0000000000000000000000beef");

int failures = 0;

void expect(bool condition, const char *message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}

void expectText(const QString &actual, const QString &expected, const char *message) {
    if (actual != expected) {
        std::fprintf(stderr, "FAIL: %s\n  expected: %s\n  actual:   %s\n", message,
                     qPrintable(expected), qPrintable(actual));
        ++failures;
    }
}

QJsonObject identity(const QString &version, const QString &revision, const QJsonValue &modified = false) {
    QJsonObject result{{"platform", "windows"}, {"architecture", "amd64"}};
    if (!version.isEmpty()) result.insert("version", version);
    if (!revision.isEmpty()) result.insert("revision", revision);
    if (!modified.isUndefined()) result.insert("modified", modified);
    return result;
}

QString label(const QJsonObject &core) { return CoreIdentity::label(core, pinnedVersion, pinnedRevision); }
QString about(const QJsonObject &runtime, const QJsonObject &validation) {
    return CoreIdentity::aboutText(runtime, validation, pinnedVersion, pinnedRevision);
}
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    using CoreIdentity::Kind;
    const auto kind = [](const QJsonObject &core) { return CoreIdentity::classify(core, pinnedVersion, pinnedRevision); };

    const QJsonObject pinned = identity("0.3.0-beta.2", pinnedRevision);
    const QJsonObject newerRelease = identity("0.3.0-beta.3", otherRevision);
    const QJsonObject olderRelease = identity("0.1.0-beta.1", "979b1479e18ff09b52e097ed853941bc27207331");
    const QJsonObject masterBuild = identity("0.1.0-dev", otherRevision);
    const QJsonObject unstampedBuild = identity("0.1.0-dev", {});

    expect(kind(pinned) == Kind::Release, "pinned release with pinned revision is a release");
    expect(kind(identity("v0.3.0-beta.2", pinnedRevision)) == Kind::Release, "leading v is accepted");
    expect(kind(identity("0.3.0-beta.2", pinnedRevision.toUpper())) == Kind::Development,
           "uppercase revisions are not Core output");
    expect(kind(newerRelease) == Kind::Release, "newer signed release stays a release");
    expect(kind(olderRelease) == Kind::Release, "older release stays a release");
    expect(kind(identity("0.3.0-beta.2", otherRevision)) == Kind::Development,
           "pinned version from another commit is a development build");
    expect(kind(identity("0.3.0-beta.2", pinnedRevision, true)) == Kind::Development, "modified pinned build");
    expect(kind(identity("0.3.0-beta.2", pinnedRevision, QJsonValue::Undefined)) == Kind::Development,
           "unknown modification state");
    expect(kind(identity("0.3.0-beta.3", {})) == Kind::Development, "release version without revision");
    expect(kind(identity("0.3.0-beta.3", "6b5340a")) == Kind::Development, "abbreviated revision");
    expect(kind(masterBuild) == Kind::Development, "dev version");
    expect(kind(identity("0.3.0-beta.3-dev", otherRevision)) == Kind::Development, "pre-release dev suffix");
    expect(kind(identity({}, otherRevision)) == Kind::Development, "rejected version with other identity fields");
    expect(kind({}) == Kind::Unknown, "no identity");
    expect(CoreIdentity::classify(identity("0.3.0-beta.2", otherRevision), {}, {}) == Kind::Release,
           "without a pin only release properties are checked");

    expectText(label(pinned), "0.3.0-beta.2", "release label");
    expectText(label(identity("v0.3.0-beta.3", otherRevision)), "0.3.0-beta.3", "release label drops v");
    expectText(label(masterBuild), "dev-core (6b5340a)", "development label with revision");
    expectText(label(unstampedBuild), "dev-core", "development label without revision");
    expectText(label({}), "unknown", "unknown label");

    expectText(about(pinned, pinned), "Core: 0.3.0-beta.2", "same runtime and validation build");
    expectText(about(pinned, {}), "Core: 0.3.0-beta.2", "validation CLI not probed");
    expectText(about(pinned, masterBuild), "Core: 0.3.0-beta.2 · validation CLI: dev-core (6b5340a)",
               "development validation CLI beside release service");
    expectText(about(masterBuild, masterBuild), "Core: dev-core (6b5340a)", "development backend uses one build");
    expectText(about(newerRelease, pinned), "Core: 0.3.0-beta.3 · validation CLI: 0.3.0-beta.2",
               "updated service beside bundled CLI");
    expectText(about({}, pinned), "Core: unknown", "release CLI is not called out while the service is unknown");
    expectText(about({}, masterBuild), "Core: unknown · validation CLI: dev-core (6b5340a)",
               "development CLI is called out while the service is unknown");

    // The build must pass the pinned release from cmake/core-release.cmake.
    expect(QRegularExpression("^v[0-9]+\\.[0-9]+\\.[0-9]+(-[0-9A-Za-z.]+)?$")
               .match(QStringLiteral(BIMERON_PINNED_CORE_VERSION)).hasMatch(), "pinned version definition");
    expect(QRegularExpression("^[0-9a-f]{40}$").match(QStringLiteral(BIMERON_PINNED_CORE_REVISION)).hasMatch(),
           "pinned revision definition");

    if (failures) std::fprintf(stderr, "%d core identity check(s) failed\n", failures);
    else std::printf("core identity checks passed\n");
    return failures ? 1 : 0;
}
