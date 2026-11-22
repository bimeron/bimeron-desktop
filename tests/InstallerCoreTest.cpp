#include "InstallerCore.h"
#include "../packaging/windows/msi-handoff-outcome.h"
#include <QTemporaryDir>
#ifdef Q_OS_WIN
#include "../packaging/windows/installer-preflight-digest.h"
#include "../packaging/windows/installer-preflight-process.h"
#include "../packaging/windows/msi-handoff-lab-ack.h"
#include <QDir>
#include <QThread>
#endif
int main(int argc, char **argv) {
    QCoreApplication app(argc,argv);
#ifdef Q_OS_WIN
    if (app.arguments().contains("--fake-delayed-success"))
        return std::getchar() == 'x' ? 0 : 1;
    if (app.arguments().contains("--fake-desktop-process")) {
        QThread::sleep(10);
        return 0;
    }
#endif
    if(app.arguments().contains("--fake-failure")) return 7;
    if(app.arguments().contains("--fake-version")) {
        printf("{\"version\":\"0.2.0-beta.1\"}"); return 0;
    }
    using MsiHandoff::Outcome;
    using MsiHandoff::select;
    if (select(std::nullopt, false, true, false) != Outcome::Waiting ||
        select(0, false, true, false) != Outcome::LaunchNew ||
        select(1603, true, false, false) != Outcome::LaunchOld ||
        select(1603, false, true, false) != Outcome::Recovery ||
        select(0, true, true, false) != Outcome::Recovery ||
        select(0, false, true, true) != Outcome::Recovery ||
        select(3010, false, true, false) != Outcome::WaitForReboot ||
        select(1641, false, true, true) != Outcome::WaitForReboot) return 26;
    using InstallerCore::older;
    const QString h1(64, 'a'), h2(64, 'b'), r1(40, 'a'), r2(40, 'b');
    const struct {QString a,b,r,s; bool differs;} identities[]={
        {h1,h2,r1,r1,true}, {h1,h1,r1,r2,false},
        {{},h2,r1,r2,true}, {{},h2,r1,r1,false},
        {{},h2,"unknown",r2,false}, {{},h2,r1,{},false},
        {{},h2,"abcdef0",r2,false}, {h1.toUpper(),h1,r1,r2,false}};
    for (const auto &id:identities) {
        const bool differs=InstallerCore::differentBuild(id.a,id.b,id.r,id.s);
        if(differs!=id.differs) return 10;
        for (const auto &v:QStringList{"0.3.0","0.4.0","0.5.0","unknown",""}) {
            int count=0;
            if(InstallerCore::serviceDecision(true,v,"0.4.0",[&]{++count;return false;},differs)!=0)
                return 11;
            if(count!=int(v=="0.3.0" || (v=="0.4.0" && differs))) return 12;
            InstallerCore::serviceDecision(false,v,"0.4.0",[&]{++count;},differs);
            if(count!=int(v=="0.3.0" || (v=="0.4.0" && differs))) return 13;
        }
    }
    if(InstallerCore::shouldUpdate("0.4.0","invalid",true) ||
       !InstallerCore::shouldUpdate("0.4.0-beta.1","0.4.0-beta.1",true) ||
       InstallerCore::shouldUpdate("0.4.0-beta.2","0.4.0-beta.1",true) ||
       !InstallerCore::shouldUpdate("0.4.0+one","0.4.0+two",true)) return 14;
    QTemporaryDir dir;
    QFile fixture(dir.filePath("core"));
    if(!fixture.open(QIODevice::WriteOnly) || fixture.write("abc")!=3) return 15;
    fixture.close();
    if(InstallerCore::digest(fixture.fileName())!="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ||
       !InstallerCore::digest(dir.filePath("missing")).isEmpty()) return 16;
#ifdef Q_OS_WIN
    // Exercise the actual native preflight digest check without querying or
    // changing any service registration on the test host.
    constexpr auto pinned = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    const auto native = QDir::toNativeSeparators(fixture.fileName()).toStdWString();
    {
        const auto pinnedFile = InstallerPreflight::openPinnedFile(native, pinned);
        if (!pinnedFile || fixture.open(QIODevice::Append)) return 17;
    }
    using InstallerPreflight::canRetainDistinctCore;
    if (!canRetainDistinctCore(native, pinned, SERVICE_RUNNING, L"2") ||
        canRetainDistinctCore(native, pinned, SERVICE_STOPPED, L"2") ||
        canRetainDistinctCore(native, pinned, SERVICE_STOP_PENDING, L"2") ||
        canRetainDistinctCore(native, pinned, SERVICE_RUNNING, L"") ||
        canRetainDistinctCore(native, pinned, SERVICE_RUNNING, L"5") ||
        canRetainDistinctCore(native, "", SERVICE_RUNNING, L"2") ||
        canRetainDistinctCore(native, std::string(64, '0'), SERVICE_RUNNING, L"2")) return 36;
    if (!InstallerPreflight::matchesPinnedDigest(native, pinned)) return 30;
    if (InstallerPreflight::matchesPinnedDigest(native, "") ||
        InstallerPreflight::matchesPinnedDigest(native, "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD") ||
        InstallerPreflight::matchesPinnedDigest(QDir::toNativeSeparators(dir.filePath("missing")).toStdWString(), pinned) ||
        InstallerPreflight::matchesPinnedDigest(QDir::toNativeSeparators(dir.path()).toStdWString(), pinned))
        return 18;
    if (!fixture.open(QIODevice::Append) || fixture.write("d") != 1) return 19;
    fixture.close();
    if (InstallerPreflight::matchesPinnedDigest(native, pinned)) return 20;
    if (!fixture.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 21;
    fixture.close();
    if (InstallerPreflight::matchesPinnedDigest(native, pinned)) return 22;
    const QString fakeDesktop = dir.filePath("bimeron-desktop.exe");
    if (!QFile::copy(app.applicationFilePath(), fakeDesktop)) return 23;
    QProcess process;
    process.start(fakeDesktop, {"--fake-desktop-process"});
    if (!process.waitForStarted(3000)) return 24;
    const bool refusedWhileRunning = !InstallerPreflight::desktopIsClosed()
        && process.state() == QProcess::Running;
    process.kill();
    process.waitForFinished();
    if (!refusedWhileRunning) return 25;

    // A registration that looks ready during an installer invocation must not
    // authorize a launch until the retained client process handle is signaled.
    QProcess delayed;
    delayed.start(app.applicationFilePath(), {"--fake-delayed-success"});
    if (!delayed.waitForStarted(3000)) return 27;
    const InstallerPreflight::FileHandle client(OpenProcess(
        SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(delayed.processId())));
    if (!client || select(MsiHandoff::observedExit(client.get()), false, true, false) != Outcome::Waiting)
        return 28;
    if (delayed.write("x", 1) != 1) return 29;
    delayed.closeWriteChannel();
    if (!delayed.waitForFinished(3000) ||
        select(MsiHandoff::observedExit(client.get()), false, true, false) != Outcome::LaunchNew)
        return 29;

    const InstallerPreflight::FileHandle ready(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    HANDLE acknowledgement = nullptr;
    if (!ready || WaitForSingleObject(ready.get(), 0) != WAIT_TIMEOUT
        || !DuplicateHandle(GetCurrentProcess(), ready.get(), GetCurrentProcess(), &acknowledgement,
                            0, FALSE, DUPLICATE_SAME_ACCESS))
        return 31;
    if (MsiHandoff::signalLabReady(QString::number(quintptr(acknowledgement))) != 0
        || WaitForSingleObject(ready.get(), 0) != WAIT_OBJECT_0
        || WaitForSingleObject(ready.get(), 0) != WAIT_TIMEOUT)
        return 32;
#endif
    const struct {const char *a,*b;bool want;} cases[]={
        {"0.2.0-beta.1","0.4.0-beta.1",true},
        {"0.4.0-beta.1","0.4.0",true},
        {"0.4.0","0.4.0-beta.1",false},
        {"0.4.0-beta.1","0.4.0-beta.1",false},
        {"unknown","0.4.0",false},{"","0.4.0",false},
        {"1.0.0-beta.2","1.0.0-beta.10",true},
        {"1.0.0-01","1.0.0",false},{"01.0.0","2.0.0",false},
        {"1.0.0+build","1.0.0+other",false},
        {"1.0.0-alpha","1.0.0-alpha.1",true}};
    for(auto c:cases) if(older(c.a,c.b)!=c.want) return 1;
    int calls=0;
    for (auto c:cases) {
        int before=calls;
        if (InstallerCore::serviceDecision(true,c.a,c.b,[&]{++calls; return false;}) != 0) return 2;
        if(calls-before != int(c.want)) return 3;
    }
    int before=calls;
    if(InstallerCore::serviceDecision(false,"0.1.0","0.4.0",[&]{++calls;return true;}) != 0 || calls!=before) return 4;
    QByteArray output;
    if(!InstallerCore::run(app.applicationFilePath(),{"--fake-version"},output) ||
       QJsonDocument::fromJson(output).object().value("version").toString()!="0.2.0-beta.1") return 5;
    bool failed=false;
    if(InstallerCore::serviceDecision(true,"0.2.0","0.4.0",[&]{
        failed=!InstallerCore::run(app.applicationFilePath(),{"--fake-failure"},output);
    })!=0 || !failed) return 6;
    return 0;
}
