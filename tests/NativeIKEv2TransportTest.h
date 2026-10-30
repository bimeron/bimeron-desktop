#pragma once

#include "NativeIKEv2Transport.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <sddl.h>
#endif

// Include in DeveloperModeTest.cpp and call NativeIKEv2TransportTest::run().
// Pure wire/identity fixtures and an unapproved asynchronous request only:
// no elevated launch, production endpoint, real Core or network mutation.
// Define NATIVE_IKEV2_TRANSPORT_TESTING for both transport and harness to also
// exercise isolated Windows IO/pin boundaries; this never bypasses authentication.
struct NativeIKEv2TransportTest {
    static void check(bool value, const char *message) {
        if (!value) qFatal("Native transport regression: %s", message);
    }
    static void run() {
        using T = NativeIKEv2Transport;
#ifdef Q_OS_WIN
#ifdef NATIVE_IKEV2_TRANSPORT_TESTING
        check(T::offlineWindowsBoundaries(), "pending pipe cancellation and retained file pins");
#endif
        auto descriptor = [](const wchar_t *sddl, bool expected, bool unknownAce = false) {
            PSECURITY_DESCRIPTOR sd = nullptr;
            check(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sd, nullptr), "ACL fixture parses");
            if (unknownAce) {
                PACL acl = nullptr; BOOL present = FALSE, defaulted = FALSE; void *entry = nullptr;
                check(GetSecurityDescriptorDacl(sd, &present, &acl, &defaulted) && present && acl && GetAce(acl, 0, &entry), "ACL fixture ACE");
                static_cast<ACE_HEADER *>(entry)->AceType = ACCESS_ALLOWED_CALLBACK_ACE_TYPE;
            }
            check(T::protectedDescriptor(sd) == expected, "protected effective ACL contract");
            LocalFree(sd);
        };
        descriptor(L"O:SYD:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;WD)", true);
        descriptor(L"O:BAD:P(A;;GA;;;SY)(A;;GA;;;BA)", true);
        descriptor(L"O:WDD:P(A;;GA;;;SY)", false);
        descriptor(L"O:SYD:NO_ACCESS_CONTROL", false);
        descriptor(L"O:SYD:P(A;;GW;;;WD)", false);
        descriptor(L"O:SYD:P(A;;0x100;;;WD)", false); // attributes, including ancestors
        descriptor(L"O:SYD:P(A;;0x40;;;WD)", false); // delete-child
        descriptor(L"O:SYD:P(D;;GW;;;WD)(A;;GW;;;WD)", false); // conservative refusal
        descriptor(L"O:SYD:P(A;IO;GW;;;WD)(A;;GA;;;SY)", true);
        descriptor(L"O:SYD:P(A;;GR;;;WD)", false, true);
#endif
        const QString revision = "66d84d8f4dcfc6ada625eb11894b19feea51b7ba";
        const QString installation(32, 'a'), instance(32, 'b'), digest(64, 'c');
        const T::Approval approval{"C:/Program Files/Bimeron/bimeron-core.exe", digest, revision, "amd64"};
        check(T::approved(approval), "provided immutable approval");
        auto missing = approval; missing.sha256.clear();
        check(!T::approved(missing), "no missing digest bypass");
        missing = approval; missing.revision = QString(40, 'a');
        check(!T::approved(missing), "only accepted source revision");
        const auto endpoint = T::endpointName("S-1-5-21-fixture", 9);
        check(endpoint.startsWith("\\\\.\\pipe\\Bimeron.Core.IKEv2.") && endpoint.endsWith(".9") &&
            !endpoint.contains("Service") && T::endpointName("sid", 0).isEmpty(), "deterministic private endpoint");
        auto encode = [&](T::Endpoint ep, const QString &command, QJsonObject fields = {}) {
            return T::encodeRequest(ep, command, "request", installation, revision, fields);
        };
        check(!encode(T::Endpoint::Session, "hello").isEmpty(), "read-only discovery");
        check(encode(T::Endpoint::Session, "start").isEmpty(), "no start command");
        check(encode(T::Endpoint::Session, "hello", {{"instance", instance}}).isEmpty(), "no partial activation");
        check(!encode(T::Endpoint::Session, "hello", {{"instance", instance}, {"accept_revision", revision}}).isEmpty(), "exact activation");
        check(encode(T::Endpoint::Session, "hello", {{"instance", instance}, {"accept_revision", QString(40, 'd')}}).isEmpty(), "wrong revision activation");
        check(encode(T::Endpoint::Session, "status", {{"instance", instance}}).isEmpty(), "session binding required");
        check(!encode(T::Endpoint::Session, "stop", {{"instance", instance}, {"session", ""}}).isEmpty(), "preactivation stop");
        check(encode(T::Endpoint::Session, "status", {{"instance", instance}, {"session", ""}, {"config", "secret-sentinel"}}).isEmpty(), "unknown request field");
        check(encode(T::Endpoint::Provisioning, "status", {{"instance", instance}, {"session", ""}}).isEmpty(), "provisioning omits session");
        check(!encode(T::Endpoint::Provisioning, "apply", {{"instance", instance}, {"accept_revision", revision}, {"plan_token", digest}}).isEmpty(), "exact provisioning token");
        check(encode(T::Endpoint::Provisioning, "apply", {{"instance", instance}, {"accept_revision", revision}}).isEmpty(), "no unreviewed confirmation");
        check(encode(T::Endpoint::Session, "events", {{"instance", instance}, {"session", ""}, {"after", -1}}).isEmpty(), "negative cursor");
        check(T::encodeRequest(T::Endpoint::Session, "hello", QString(65, 'x'), installation, revision, {}).isEmpty(), "request ID byte bound");

        T::Identity peer;
        peer.pid = 321; peer.created = 18446744073709551614ull; peer.session = 9;
        peer.imageVolume = 15; peer.imageIndex = 18446744073709551613ull;
        peer.sid = "S-1-5-21-fixture"; peer.imageSha256 = digest; peer.elevated = true;
        QJsonObject owner{{"pid", int(peer.pid)}, {"created", QString::number(peer.created)},
            {"session", int(peer.session)}, {"image_volume", int(peer.imageVolume)},
            {"image_index", QString::number(peer.imageIndex)}, {"sid", peer.sid},
            {"image_sha256", digest}, {"elevated", true}};
        QJsonObject base{{"version", 1}, {"id", "request"}, {"installation", installation},
            {"instance", instance}, {"endpoint", "ikev2_foreground"}, {"owner", owner},
            {"status", QJsonObject{{"State", "disconnected"}, {"Attempt", 0}, {"Since", ""}, {"LastError", "secret-sentinel"}}},
            {"core", QJsonObject{{"version", "experimental"}, {"revision", revision}, {"modified", false},
                {"platform", "windows"}, {"architecture", "amd64"}, {"go_version", "go-test"}}}};
        QJsonObject caps{{"platform", "windows"}};
        for (const auto &key : {"vpn", "proxy", "ipv4", "ipv6", "ip_split", "domain_split", "app_split", "kill_switch", "daemon"}) caps.insert(key, true);
        QJsonObject ikev2;
        for (const auto &key : {"administrator_required", "supported", "foreground_only", "preconfigured_profile", "proxy", "ip_split", "domain_split", "kill_switch", "dns_protection", "stats"}) ikev2.insert(key, false);
        ikev2.insert("supported", true); caps.insert("ikev2", ikev2); base.insert("capabilities", caps);
        QJsonObject result;
        auto bytes = [](const QJsonObject &o) { return QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n'; };
        auto decode = [&](const QByteArray &frame, const QString &expectedInstance = QString{}) {
            result = {};
            return T::decodeResponse(frame, "request", installation, approval, T::Endpoint::Session, peer, expectedInstance, &result);
        };
        check(decode(bytes(base)), "authenticated response with exact 64-bit identities");
        check(!QJsonDocument(result).toJson().contains("secret-sentinel"), "diagnostics stripped");
        // Source-derived wire fixture: encoding/json of the exact Event and
        // LoggedEvent declarations from Core 66d84d8f, core/engine.go and
        // internal/control/server.go. Zero value and populated diagnostic event;
        // no Core execution, credentials, network or elevated owner is involved.
        const auto coreEvents = QJsonDocument::fromJson(R"json([
            {"sequence":1,"event":{"Time":"0001-01-01T00:00:00Z","State":"","Code":"","Message":"","Attempt":0,"LastError":""}},
            {"sequence":2,"event":{"Time":"2026-10-30T00:00:00.123456789Z","State":"connected","Code":"secret-sentinel","Message":"secret-sentinel","Attempt":1,"LastError":"secret-sentinel","ErrorStage":"secret-sentinel"}}
        ])json").array();
        auto eventResponse = base;
        eventResponse.insert("events", coreEvents);
        check(decode(bytes(eventResponse)), "Core zero and populated event serialization");
        check(!QJsonDocument(result).toJson().contains("secret-sentinel"), "event diagnostics stripped or mapped");
        const auto zeroEvent = coreEvents.first().toObject().value("event").toObject();
        auto withEvent = [&](const QJsonValue &event) {
            auto response = base;
            response.insert("events", QJsonArray{QJsonObject{{"sequence", 1}, {"event", event}}});
            return bytes(response);
        };
        check(!decode(withEvent(QJsonObject{})), "empty event rejected");
        for (const auto &key : {"Time", "State", "Code", "Message", "Attempt", "LastError"}) {
            auto event = zeroEvent; event.remove(key);
            check(!decode(withEvent(event)), "each mandatory event field required");
        }
        for (const auto &key : {"Time", "State", "Code", "Message", "Attempt", "LastError", "ErrorStage"}) {
            for (const auto &value : QJsonArray{QJsonValue::Null, true, QJsonObject{}, QJsonArray{}, 1, ""}) {
                if ((QString(key) == "Attempt" && value.isDouble()) ||
                    (QString(key) != "Attempt" && value.isString())) continue;
                auto event = zeroEvent; event.insert(key, value);
                check(!decode(withEvent(event)), "each event field type checked before stripping");
            }
        }
        auto event = zeroEvent; event.insert("ErrorStage", "");
        check(decode(withEvent(event)), "empty optional event stage");
        event.insert("unknown", ""); check(!decode(withEvent(event)), "unknown event field refused");
        event = zeroEvent; event.insert("Attempt", -1); check(!decode(withEvent(event)), "negative event attempt refused");
        event.insert("Attempt", 9007199254740992.0); check(!decode(withEvent(event)), "event numeric bound retained");
        auto nilStatus = base;
        nilStatus.insert("error", "response_too_large");
        nilStatus.insert("status", QJsonObject{{"State", ""}, {"Attempt", 0},
            {"Since", "0001-01-01T00:00:00Z"}, {"LastError", ""}});
        check(decode(bytes(nilStatus)), "Core zero status omits nil Session and Native pointers");
        auto bad = base; bad.insert("id", "stale"); check(!decode(bytes(bad)), "correlation");
        bad = base; bad.insert("version", 2); check(!decode(bytes(bad)), "wire version");
        bad = base; bad.insert("installation", QString(32, 'd')); check(!decode(bytes(bad)), "installation binding");
        bad = base; bad.insert("endpoint", "ikev2_profile"); check(!decode(bytes(bad)), "endpoint binding");
        check(!decode(bytes(base), QString(32, 'd')), "instance replay");
        for (const auto &key : {"pid", "created", "sid", "session", "image_sha256", "image_volume", "image_index", "elevated"}) {
            bad = base; auto wrong = owner; wrong.remove(key); bad.insert("owner", wrong);
            check(!decode(bytes(bad)), "every OS identity field required");
            wrong = owner;
            if (QString(key) == "elevated") wrong.insert(key, false);
            else if (wrong.value(key).isDouble()) wrong.insert(key, wrong.value(key).toDouble() + 1);
            else wrong.insert(key, QString(key) == "image_sha256" ? QString(64, 'd') : "1");
            bad.insert("owner", wrong);
            check(!decode(bytes(bad)), "every OS identity field must match");
        }
        bad = base; auto wrong = owner; wrong.insert("created", "18446744073709551616"); bad.insert("owner", wrong);
        check(!decode(bytes(bad)), "uint64 overflow");
        bad = base; wrong = owner; wrong.insert("elevated", false); bad.insert("owner", wrong);
        check(!decode(bytes(bad)), "elevation");
        bad = base; wrong = base.value("core").toObject(); wrong.insert("modified", true); bad.insert("core", wrong);
        check(!decode(bytes(bad)), "modified core");
        bad = base; bad.insert("unknown", true); check(!decode(bytes(bad)), "unknown response field");
        bad = base; wrong = base.value("status").toObject(); wrong.insert("unknown", true); bad.insert("status", wrong);
        check(!decode(bytes(bad)), "unknown nested field");
        bad = base; wrong = caps; wrong.insert("ikev2", QJsonObject{{"supported", "true"}}); bad.insert("capabilities", wrong);
        check(!decode(bytes(bad)), "capability types and completeness");
        bad = base; wrong = base.value("status").toObject(); wrong.remove("Attempt"); bad.insert("status", wrong);
        check(!decode(bytes(bad)), "required status field");
        auto frame = bytes(base); frame.insert(1, "\"id\":\"request\","); check(!decode(frame), "duplicate key");
        frame = bytes(base); frame.insert(1, "\"\\u0069d\":\"request\","); check(!decode(frame), "escaped duplicate key");
        check(!decode(bytes(base) + "{}\n"), "multiple frames");
        check(!decode(bytes(base).chopped(1)), "missing delimiter");
        check(!decode("{broken}\n"), "malformed JSON");
        frame = bytes(base); frame.replace("\"pid\":321", "\"pid\":321,\"pid\":321"); check(!decode(frame), "nested duplicate");
        frame = bytes(base); frame.replace("\"pid\":321", "\"pid\":321.0000000000000000001"); check(!decode(frame), "fractional identity cannot round to integer");
        frame = bytes(base); frame.replace("\"version\":1", "\"version\":1e0"); check(!decode(frame), "integer lexical contract");
        frame = bytes(base); frame.insert(1, QByteArray("\"bad\":\"\xff\",")); check(!decode(frame), "invalid UTF-8");
        check(!decode(QByteArray(256 * 1024, ' ') + bytes(base)), "response size bound");
        bad = base; bad.insert("error", "secret-sentinel");
        check(decode(bytes(bad)) && result.value("error") == "foreground_operation_failed", "unknown errors never exposed");
        bad.insert("error", "cleanup_pending"); check(decode(bytes(bad)) && result.value("error") == "cleanup_pending", "safe error preserved");
        auto active = base;
        auto status = active.value("status").toObject();
        status.insert("State", "connected");
        status.insert("Session", QJsonObject{{"id", instance}, {"backend", "ikev2"}, {"stats_unavailable", true},
            {"active", true}, {"mode", "vpn"}, {"ipv6", "system"}, {"kill_switch_requested", true}});
        QJsonObject protection{{"requested", true}, {"recovery_required", false}, {"wfp", "active"}, {"dns", "active"},
            {"profile_readiness", "validated"}, {"credential_readiness", "usable"}, {"certificate_readiness", "windows_authenticated"},
            {"ras_cleanup", "not_started"}, {"dns_cleanup", "not_started"}, {"wfp_cleanup", "not_started"}};
        status.insert("native", protection); active.insert("status", status);
        check(decode(bytes(active)), "native structured lifecycle passes unchanged");
        protection.insert("wfp", "invented"); status.insert("native", protection); active.insert("status", status);
        check(!decode(bytes(active)), "unknown protection state refused");
        auto profile = base; profile.insert("endpoint", "ikev2_profile"); profile.insert("operation", "previewed");
        profile.insert("profile", QJsonObject{{"state_token", digest}, {"action", "create"}, {"windows_profile", "Fixture"},
            {"server_name", "vpn.example.test"}, {"authentication", "EAP"}, {"ipsec", "public settings"},
            {"ipv6_routes", QJsonArray{"::/1", "8000::/1"}}, {"credentials", "Windows-owned"}, {"trust", "Windows-owned"}});
        check(T::decodeResponse(bytes(profile), "request", installation, approval, T::Endpoint::Provisioning, peer, {}, &result), "provisioning public plan");
        check(result.value("profile").toObject().value("state_token") == digest, "exact confirmation token preserved");
        check(!decode(bytes(profile)), "provisioning cannot masquerade as session");

        // Empty approval can never open a protected endpoint or launch Core.
        T transport({});
        check(transport.processState() == T::ProcessState::Unknown, "no process observation without pinned process");
        QEventLoop loop;
        bool failed = false;
        QObject::connect(&transport, &T::failed, &loop, [&](const QString &, T::Failure reason) {
#ifdef Q_OS_WIN
            check(reason == T::Failure::ApprovalRequired, "Windows execution remains gated");
#else
            check(reason == T::Failure::Unsupported, "Linux unsupported stub");
#endif
            failed = true; loop.quit();
        });
        QTimer timeout; timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        check(transport.discover(T::Endpoint::Session) && transport.busy(), "asynchronous discovery");
        check(!transport.discover(T::Endpoint::Session), "serialized exchanges");
        timeout.start(3000); loop.exec();
        check(failed && !transport.busy(), "bounded asynchronous failure");
        transport.releaseHandles();
        check(!transport.discover(T::Endpoint::Session), "closed client cannot reuse forgotten ownership");

        T cancelled({});
        QEventLoop cancelledLoop;
        bool cancelledResult = false;
        QObject::connect(&cancelled, &T::failed, &cancelledLoop, [&](const QString &, T::Failure reason) {
            check(reason == T::Failure::Cancelled, "cancel exchange result"); cancelledResult = true; cancelledLoop.quit();
        });
        check(cancelled.discover(T::Endpoint::Session), "cancel fixture starts");
        cancelled.cancelExchange();
        QTimer::singleShot(3000, &cancelledLoop, &QEventLoop::quit); cancelledLoop.exec();
        check(cancelledResult, "cancellation completion");

        T released({});
        bool staleCompletion = false;
        QObject::connect(&released, &T::failed, &released, [&](const QString &, T::Failure) { staleCompletion = true; });
        check(released.discover(T::Endpoint::Session), "release fixture starts");
        released.releaseHandles();
        QEventLoop drain; QTimer::singleShot(50, &drain, &QEventLoop::quit); drain.exec();
        check(!staleCompletion, "release suppresses stale completion without waiting for worker");
#ifdef Q_OS_WIN
        auto writableApproval = approval; writableApproval.executable = QCoreApplication::applicationFilePath();
        T writable(writableApproval);
        QEventLoop writableLoop;
        bool writableRejected = false;
        QObject::connect(&writable, &T::failed, &writableLoop, [&](const QString &, T::Failure reason) {
            check(reason == T::Failure::IdentityRejected, "writable development requester refused before IPC");
            writableRejected = true; writableLoop.quit();
        });
        check(writable.discover(T::Endpoint::Session), "writable path fixture starts");
        QTimer::singleShot(3000, &writableLoop, &QEventLoop::quit); writableLoop.exec();
        check(writableRejected, "no development bypass");
#endif
    }
};
