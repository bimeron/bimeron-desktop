#pragma once
#include "NativeIKEv2Session.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QDir>
#include <vector>

// Register run() in DeveloperModeTest.cpp with both controller/transport sources.
// Deterministic offline seam only; never opens a pipe or launches a process.
struct NativeIKEv2SessionTest {
    using S = NativeIKEv2Session;
    using T = NativeIKEv2Transport;
    struct Call { S::Endpoint endpoint; QString command; QJsonObject fields; };
    static void check(bool b,const char *m) { if (!b) qFatal("Native session regression: %s",m); }
    static T::Approval approval() { return {"C:/Program Files/Bimeron/bimeron-core.exe",QString(64,'a'),"66d84d8f4dcfc6ada625eb11894b19feea51b7ba","amd64"}; }
    struct Fixture {
        std::vector<Call> calls;
        int launches = 0, cancels = 0;
        T::ProcessState process = T::ProcessState::Running;
        S controller;
        Fixture() : controller(approval(), {
            [this](S::Endpoint e,const QString &c,const QJsonObject &f) { calls.push_back({e,c,f}); return true; },
            [this](const QString &,const QString &) { ++launches; return true; },
            [this](const QString &,const QString &,bool) { ++launches; return true; },
            [this] { ++cancels; }, [this] { return process; }}, nullptr) {}
        QJsonObject reply(QString state = "disconnected") {
            return {{"installation",QString(32,'a')},{"instance",QString(32,'b')},
                {"owner",QJsonObject{{"created","18446744073709551615"},{"image_index","18446744073709551614"},{"pid",42}}},
                {"core",QJsonObject{{"revision",approval().revision},{"modified",false}}},
                {"capabilities",QJsonObject{{"ikev2",QJsonObject{{"supported",true}}}}},
                {"status",QJsonObject{{"State",state},{"Attempt",0},{"Since","2026-10-30T00:00:00Z"},{"LastError",""}}}};
        }
        // Exact Status/SessionInfo/NativeStatus shapes from Core 66d84d8f:
        // engine.go New/Start/Status/Stop, session_info.go, ikev2_windows.go and
        // native_observed.go Close. Before Start both optional objects are absent.
        QJsonObject sessionReply(QString state, bool requested = false) {
            auto r = reply(state); auto status = r.value("status").toObject();
            status.insert("Attempt",1);
            status.insert("Session",QJsonObject{{"backend","ikev2"},{"stats_unavailable",true},
                {"id",QString(32,'f')},{"active",state == "connected" || state == "connecting" || state == "stopping"},
                {"mode","vpn"},{"ipv6","system"},{"kill_switch_requested",requested}});
            status.insert("native",QJsonObject{{"profile_readiness","validated"},{"credential_readiness","usable"},
                {"certificate_readiness","windows_authenticated"},{"requested",requested},
                {"wfp",requested && state == "connected" ? "active" : "inactive"},
                {"dns",requested && state == "connected" ? "active" : "inactive"},
                {"ras_cleanup",state == "connected" ? "not_started" : "succeeded"},
                {"dns_cleanup",requested && state != "connected" ? "succeeded" : "not_started"},
                {"wfp_cleanup",requested && state != "connected" ? "succeeded" : "not_started"},
                {"recovery_required",false}});
            r.insert("status",status); return r;
        }
        void deliver(QJsonObject r) { controller.received(calls.back().command,r); }
        void discover() { check(controller.discover(),"discover accepted"); deliver(reply()); }
        void launch() { controller.setLaunchGates({true,true,true,true}); check(controller.connectProfile(QDir::rootPath()+"fixture.conf",QString(64,'c')),"launch accepted"); controller.launched(); }
    };
    static void run() {
        // Reviewed acceptance criteria: delayed read-only discovery retains bounded
        // intent; cancellation and ambiguous activation can never send it again.
        {
            Fixture f; f.launch(); f.controller.failed("hello",T::Failure::Unavailable);
            f.controller.poll(); f.deliver(f.reply());
            check(f.calls.size()==3 && f.calls.back().fields.value("accept_revision")==approval().revision,
                "delayed pipe discovery activates exactly once");
            f.controller.failed("hello",T::Failure::Timeout); f.controller.poll();
            check(f.calls.back().command=="status","sent activation timeout reconciles status");
            f.deliver(f.sessionReply("connected")); f.controller.poll(); f.deliver(f.sessionReply("connected"));
            int activations=0; for (const auto &c:f.calls) activations+=c.fields.contains("accept_revision");
            f.controller.setLaunchGates({true,true,true,true});
            check(!f.controller.connectProfile(QDir::rootPath()+"fixture.conf",QString(64,'c')) &&
                !f.controller.gates_.ownerAuthoritativelyAbsent && f.launches==1 && activations==1,
                "fresh gates consumed without duplicate launch or activation");
        }
        {
            Fixture f; f.launch();
            check(f.controller.activationDeadline_.interval()==30000,"initial activation intent has fixed bound");
            QMetaObject::invokeMethod(&f.controller.deadline_,"timeout",Qt::DirectConnection);
            check(f.controller.activationPending_,"read-only exchange timeout retains intent");
            f.controller.poll(); f.controller.activationDeadline_.stop();
            QMetaObject::invokeMethod(&f.controller.activationDeadline_,"timeout",Qt::DirectConnection);
            f.deliver(f.reply());
            for (const auto &c:f.calls) check(!c.fields.contains("accept_revision"),"late discovery beyond launch bound cannot activate");
        }
        for (bool stop : {false,true}) {
            Fixture f; f.launch(); f.controller.failed("hello",T::Failure::Unavailable);
            if (stop) f.controller.stop(); else f.controller.cancelExchange();
            f.controller.poll(); f.deliver(f.reply());
            if (stop) {
                check(f.calls.back().command=="stop","stop before pipe queued");
                auto stopped=f.reply(); stopped.remove("core"); stopped.remove("capabilities");
                f.deliver(stopped);
                check(f.controller.snapshot().ikev2.value("supported")==true,
                    "preactivation stop without hello metadata preserves verified capabilities");
            }
            for (const auto &c:f.calls) check(!c.fields.contains("accept_revision"),"cancel before pipe never activates");
            check(f.controller.closeOwner(),"validated never-activated owner may close");
        }
        for (auto failure : {T::Failure::IdentityRejected,T::Failure::InvalidResponse}) {
            Fixture f; f.launch(); f.controller.failed("hello",failure); f.controller.poll();
            check(f.calls.size()==1 && !f.controller.activationPending_ && f.controller.poisoned_,
                "discovery identity or schema failure never activates");
        }
        {
            Fixture f; f.discover();
            check(f.controller.snapshot().native.isEmpty() && f.controller.closeOwner(),"Core initial idle hello may close");
        }
        for (bool requested : {false,true}) {
            Fixture f; f.discover(); f.controller.poll(); f.deliver(f.sessionReply("connected",requested));
            check(f.controller.snapshot().requested==requested && !f.controller.closeOwner(),"connected owner cannot close");
            check(f.controller.stop(),"stop native session");
            auto stopped=f.sessionReply("disconnected",requested);
            stopped.remove("core"); stopped.remove("capabilities");
            f.deliver(stopped);
            check(f.controller.snapshot().ikev2.value("supported")==true,
                "Core stop response omits hello metadata without erasing verified capabilities");
            check(f.controller.closeOwner(),"Core terminal cleanup permits close with protection on or off");
        }
        for (const auto &field : {"wfp","dns"}) for (const auto &value : {"retained","modified","unknown"}) {
            Fixture f; f.discover(); f.controller.poll(); auto r=f.sessionReply("disconnected");
            auto status=r.value("status").toObject(); auto n=status.value("native").toObject();
            n.insert(field,value); status.insert("native",n); r.insert("status",status); f.deliver(r);
            check(!f.controller.closeOwner(),"inactive intent never hides retained modified unknown protection");
        }
        for (const auto &field : {"ras_cleanup","dns_cleanup","wfp_cleanup"}) {
            for (const auto &value : {"pending","failed"}) {
                Fixture f; f.discover(); f.controller.poll(); auto r=f.sessionReply("disconnected");
                auto status=r.value("status").toObject(); auto n=status.value("native").toObject();
                n.insert(field,value); status.insert("native",n); r.insert("status",status); f.deliver(r);
                check(!f.controller.closeOwner(),"incomplete cleanup blocks close even with protection off");
            }
        }
        for (const auto &field : {"requested","recovery_required","ras_cleanup","dns_cleanup","wfp_cleanup","wfp","dns"}) {
            Fixture f; f.discover(); f.controller.poll(); auto r=f.sessionReply("disconnected");
            auto status=r.value("status").toObject(); auto n=status.value("native").toObject();
            n.remove(field); status.insert("native",n); r.insert("status",status); f.deliver(r);
            check(!f.controller.closeOwner(),"missing native cleanup field never proves safety");
        }
        {
            Fixture f; f.launch(); f.deliver(f.reply()); f.controller.failed("hello",T::Failure::Timeout);
            f.controller.poll(); f.deliver(f.reply());
            check(!f.controller.closeOwner(),"missing native after ambiguous activation is not idle proof");
        }
        {
            Fixture f; f.discover(); f.controller.poll(); f.deliver(f.sessionReply("connected"));
            f.controller.poll(); auto r=f.sessionReply("disconnected"); auto status=r.value("status").toObject();
            status.remove("native"); r.insert("status",status); f.deliver(r);
            check(!f.controller.closeOwner(),"missing native after known session is not cleanup proof");
        }
        {
            Fixture f; f.controller.setLaunchGates({true,true,true,true});
            check(f.controller.connectProfile(QDir::rootPath()+"fixture.conf",QString(64,'c')),"consent attempt");
            f.controller.failed("launch",T::Failure::Cancelled);
            check(!f.controller.timer_.isActive() && !f.controller.snapshot().authoritative && f.launches==1,"UAC cancellation ends attempt");
        }
        {
            Fixture f; f.launch(); f.deliver(f.reply()); auto r=f.reply(); r.insert("error","activation_cancelled"); f.deliver(r);
            check(f.controller.snapshot().safeError=="activation_cancelled" && f.controller.activationSent_,"canceled activation never retried");
            f.controller.poll(); r=f.reply(); r.insert("installation",QString(32,'c')); f.deliver(r);
            check(f.controller.poisoned_,"installation replacement rejected");
        }
        {
            Fixture f; f.controller.setLaunchGates({true,true,true,true});
            check(f.controller.previewProfile("fixture","vpn.example","remove"),"immutable remove parameters"); f.controller.launched();
            auto r=f.reply(); r.insert("operation","previewed");
            r.insert("profile",QJsonObject{{"state_token",QString(64,'e')},{"windows_profile","fixture"},{"server_name","vpn.example"},{"action","create"}});
            f.deliver(r); check(!f.controller.confirmPreview(QString(64,'e')),"launch action must match preview");
        }
#ifndef Q_OS_WIN
        {
            S real(approval());
            real.setLaunchGates({true,true,true,true});
            check(!real.discover() && !real.connectProfile("/fixture.conf",QString(64,'c')) &&
                !real.previewProfile("fixture","vpn.example","apply"),"Linux runtime never launches owners");
            check(real.snapshot().safeError=="native_unsupported","Linux safe unsupported snapshot");
        }
#endif
        {
            Fixture f; f.discover();
            QEventLoop loop; QTimer::singleShot(2100,&loop,&QEventLoop::quit); loop.exec();
            check(f.calls.size()==2 && f.calls.back().command=="status","actual timer polls terminal owner once");
            const auto before=f.calls.size();
            QEventLoop delayed; QTimer::singleShot(600,&delayed,&QEventLoop::quit); delayed.exec();
            check(f.calls.size()==before,"delayed status never overlaps polling");
            f.controller.cancelExchange(); f.deliver(f.reply());
            check(!f.controller.snapshot().authoritative,"late canceled reply ignored");
        }
        {
            Fixture f; f.discover(); f.controller.poll(); auto r=f.reply("connected");
            auto status=r.value("status").toObject();
            status.insert("Session",QJsonObject{{"id",QString(32,'f')},{"active",true}}); r.insert("status",status); f.deliver(r);
            f.controller.poll(); r=f.reply("disconnected"); f.deliver(r);
            check(f.controller.poisoned_,"lost established session identity rejected");
        }
        {
            Fixture f; f.discover(); f.controller.poll(); auto r=f.sessionReply("disconnected",true);
            auto status=r.value("status").toObject(); auto n=status.value("native").toObject();
            n.insert("wfp","modified"); n.insert("dns","unknown"); status.insert("native",n); r.insert("status",status); f.deliver(r);
            check(f.controller.snapshot().requested && f.controller.snapshot().wfp=="modified" &&
                f.controller.snapshot().dns=="unknown" && !f.controller.closeOwner(),"requested modified unknown remain distinct");
        }
        {
            Fixture f;
            check(!f.controller.connectProfile(QDir::rootPath()+"fixture.conf",QString(64,'c')) && !f.launches,"gates default closed");
            f.controller.discover(); f.controller.failed("hello",T::Failure::Unavailable);
            check(!f.controller.connectProfile(QDir::rootPath()+"fixture.conf",QString(64,'c')),"missing pipe is not owner absence");
        }
        {
            Fixture f; f.launch(); check(f.calls.size()==1 && f.calls.back().fields.isEmpty(),"first hello read only");
            f.deliver(f.reply()); check(f.calls.size()==2 && f.calls.back().fields.value("accept_revision")==approval().revision,"one reviewed activation");
            f.controller.failed("hello",T::Failure::Timeout); f.controller.poll();
            check(f.calls.back().command=="status","ambiguous activation reconciles status");
            f.deliver(f.reply("connecting")); f.controller.poll();
            check(f.launches==1 && f.controller.activationSent_,"no repeated activation or launch");
            check(f.controller.timer_.interval()==500 || !f.controller.pending_.isEmpty(),"bounded active polling");
        }
        {
            Fixture f; f.launch(); check(f.controller.stop(),"stop queues behind discovery"); f.deliver(f.reply());
            check(f.calls.back().command=="stop" && !f.controller.activationSent_,"preactivation stop cancels activation");
            check(f.controller.deadline_.interval()>=7000,"stop deadline minimum");
            auto r=f.reply(); r.insert("error","cleanup_pending"); f.deliver(r);
            check(f.controller.snapshot().safeError=="cleanup_pending","safe cleanup pending");
            f.controller.poll(); f.deliver(f.reply()); check(f.controller.closeOwner(),"explicit clean close");
            f.deliver(f.reply()); check(!f.controller.snapshot().ownerClosed,"accepted close not yet closure");
            f.process=T::ProcessState::Exited; f.controller.poll(); check(f.controller.snapshot().ownerClosed,"closure plus observed exit");
        }
        {
            Fixture f; f.discover(); auto r=f.sessionReply("disconnected",true); auto status=r.value("status").toObject();
            auto n=status.value("native").toObject(); n.insert("wfp","retained"); status.insert("native",n); r.insert("status",status);
            f.controller.poll(); f.deliver(r); check(!f.controller.closeOwner(),"retained policy blocks clean close");
            f.controller.poll(); r=f.sessionReply("disconnected",true); r.insert("dropped",true); r.insert("cursor",4); f.deliver(r);
            check(f.calls.back().command=="status","dropped events reread status immediately");
            f.deliver(f.sessionReply("disconnected",true)); check(f.controller.snapshot().owner.value("created").toString()=="18446744073709551615","uint64 identity remains string");
            f.controller.poll(); r=f.reply(); r.insert("instance",QString(32,'d')); f.deliver(r);
            check(!f.controller.snapshot().authoritative && f.controller.poisoned_,"instance change fails closed");
        }
        {
            Fixture f; f.discover(); f.controller.poll(); auto r=f.reply();
            auto owner=r.value("owner").toObject(); owner.insert("created","7"); r.insert("owner",owner); f.deliver(r);
            check(f.controller.poisoned_,"peer replacement rejected");
        }
        {
            Fixture f; f.controller.setLaunchGates({true,true,true,true});
            check(f.controller.previewProfile("fixture","vpn.example","remove"),"provision launch"); f.controller.launched();
            auto r=f.reply(); r.insert("operation","previewed");
            r.insert("profile",QJsonObject{{"state_token",QString(64,'e')},{"windows_profile","fixture"},{"server_name","vpn.example"},{"action","remove"}});
            f.deliver(r); check(!f.controller.confirmPreview(QString(64,'f')),"wrong token rejected");
            f.controller.poll();
            check(!f.controller.confirmPreview(QString(64,'e')) && !f.controller.confirmationSent_,"busy poll does not consume confirmation");
            f.deliver(r);
            check(f.controller.confirmPreview(QString(64,'e')),"exact token accepted");
            check(f.calls.back().command=="remove" && !f.calls.back().fields.contains("session"),"separate mutation endpoint without session");
            f.controller.failed("remove",T::Failure::Timeout); check(!f.controller.confirmPreview(QString(64,'e')),"confirmation consumed once");
            f.controller.poll(); check(f.calls.back().command=="status","ambiguous provisioning rereads status");
        }
        {
            Fixture f; f.discover(); f.controller.poll(); f.controller.cancelExchange();
            check(f.cancels==1 && !f.controller.snapshot().authoritative,"local cancel preserves unknown owner");
            check(!f.controller.connectProfile(QDir::rootPath()+"fixture.conf",QString(64,'c')),"unknown owner blocks launch");
            check(f.controller.discover(),"explicit authenticated reopen"); f.deliver(f.reply());
            check(f.controller.snapshot().authoritative,"reopen refreshes status");
        }
        {
            Fixture f; f.discover(); check(f.controller.closeOwner(),"close request");
            f.controller.failed("close_owner",T::Failure::Timeout); f.process=T::ProcessState::Exited; f.controller.poll();
            check(!f.controller.snapshot().ownerClosed && !f.controller.snapshot().authoritative,"exit plus lost close remains unknown");
        }
        {
            Fixture f; f.controller.discover(); auto r=f.reply(); r.insert("error","secret-sentinel"); f.deliver(r);
            check(f.controller.snapshot().safeError=="native_operation_failed","untrusted error hidden");
            f.controller.poll(); r=f.reply(); r.insert("error","stale_instance"); f.deliver(r);
            check(f.controller.poisoned_,"stale instance blocks further commands");
        }
        {
            Fixture f; f.controller.discover();
            QElapsedTimer elapsed; elapsed.start(); QCoreApplication::processEvents();
            check(elapsed.elapsed()<1000 && f.calls.size()==1,"delayed reply leaves UI responsive and requests serial");
            f.controller.poll(); check(f.calls.size()==1,"no concurrent exchange");
            f.deliver(f.reply()); check(f.controller.timer_.interval()==2000,"terminal poll bound");
        }
    }
};
