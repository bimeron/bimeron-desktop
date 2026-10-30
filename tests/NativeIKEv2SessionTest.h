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
                {"status",QJsonObject{{"State",state},{"native",QJsonObject{{"requested",true},{"wfp","inactive"},{"dns","inactive"},
                    {"ras_cleanup","succeeded"},{"dns_cleanup","succeeded"},{"wfp_cleanup","succeeded"},{"recovery_required",false}}}}}};
        }
        void deliver(QJsonObject r) { controller.received(calls.back().command,r); }
        void discover() { check(controller.discover(),"discover accepted"); deliver(reply()); }
        void launch() { controller.setLaunchGates({true,true,true,true}); check(controller.connectProfile(QDir::rootPath()+"fixture.conf",QString(64,'c')),"launch accepted"); controller.launched(); }
    };
    static void run() {
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
            Fixture f; f.discover(); f.controller.poll(); auto r=f.reply();
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
            Fixture f; f.discover(); auto r=f.reply(); auto status=r.value("status").toObject();
            auto n=status.value("native").toObject(); n.insert("wfp","retained"); status.insert("native",n); r.insert("status",status);
            f.controller.poll(); f.deliver(r); check(!f.controller.closeOwner(),"retained policy blocks clean close");
            f.controller.poll(); r=f.reply(); r.insert("dropped",true); r.insert("cursor",4); f.deliver(r);
            check(f.calls.back().command=="status","dropped events reread status immediately");
            f.deliver(f.reply()); check(f.controller.snapshot().owner.value("created").toString()=="18446744073709551615","uint64 identity remains string");
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
