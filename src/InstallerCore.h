#pragma once
#include "TrustedCore.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <cstdio>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
namespace InstallerCore {
inline int compareNumber(const QString &a, const QString &b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    return QString::compare(a, b, Qt::CaseSensitive);
}
inline bool parse(const QString &v, QStringList &parts) {
    static const QRegularExpression re(R"(^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$)");
    auto m = re.match(v);
    if (!m.hasMatch()) return false;
    parts = {m.captured(1),m.captured(2),m.captured(3),m.captured(4)};
    for (auto p : parts[3].split('.'))
        if (QRegularExpression("^[0-9]+$").match(p).hasMatch() && p.size()>1 && p[0]=='0') return false;
    return true;
}
inline bool older(const QString &installed, const QString &bundle) {
    QStringList a,b;
    if (!parse(installed,a) || !parse(bundle,b)) return false;
    for(int i=0;i<3;++i) { int c=compareNumber(a[i],b[i]); if(c) return c<0; }
    if(a[3].isEmpty() || b[3].isEmpty()) return !a[3].isEmpty() && b[3].isEmpty();
    auto ap=a[3].split('.'), bp=b[3].split('.');
    for(int i=0;i<qMin(ap.size(),bp.size());++i) {
        bool an=QRegularExpression("^[0-9]+$").match(ap[i]).hasMatch();
        bool bn=QRegularExpression("^[0-9]+$").match(bp[i]).hasMatch();
        int c=an!=bn ? (an?-1:1) : an ? compareNumber(ap[i],bp[i]) : QString::compare(ap[i],bp[i],Qt::CaseSensitive);
        if(c) return c<0;
    }
    return ap.size()<bp.size();
}
// Always return package success; callback failure belongs to Core recovery.
template<class Update> int serviceDecision(bool installed, const QString &current,
                                          const QString &bundle, Update update) {
    if (installed && older(current, bundle)) update();
    return 0;
}
inline bool run(const QString &exe,const QStringList &args,QByteArray &out, int timeout=10000) {
    QProcess p; p.setProcessChannelMode(QProcess::MergedChannels); p.start(exe,args);
    if(!p.waitForStarted(10000) || !p.waitForFinished(timeout)) { p.kill(); p.waitForFinished(); return false; }
    out=p.readAll(); return p.exitStatus()==QProcess::NormalExit && p.exitCode()==0;
}
inline QString serviceExecutable() {
#ifdef Q_OS_WIN
    SC_HANDLE m=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT);
    if(!m) return {};
    SC_HANDLE s=OpenServiceW(m,L"BimeronCore",SERVICE_QUERY_CONFIG);
    CloseServiceHandle(m); if(!s) return {};
    DWORD n=0; QueryServiceConfigW(s,nullptr,0,&n);
    QByteArray data(int(n),0);
    bool ok=n && QueryServiceConfigW(s,reinterpret_cast<QUERY_SERVICE_CONFIGW*>(data.data()),n,&n);
    CloseServiceHandle(s); if(!ok) return {};
    QString command=QString::fromWCharArray(reinterpret_cast<QUERY_SERVICE_CONFIGW*>(data.data())->lpBinaryPathName);
    static QRegularExpression re(R"re(^"([^"]+)" service run S-[0-9-]+$)re");
    auto match=re.match(command); return match.hasMatch()?match.captured(1):QString();
#else
    QFile unit("/etc/systemd/system/bimeron-core.service");
    if(!unit.open(QIODevice::ReadOnly) || !unit.readAll().contains("\nExecStart=/usr/local/lib/bimeron/bimeron-core daemon\n")) return {};
    return "/usr/local/lib/bimeron/bimeron-core";
#endif
}
inline int update() {
    auto log=[](const char *s){fprintf(stderr,"Bimeron installer Core: %s\n",s);};
    QString service=serviceExecutable();
    QString bundle=QCoreApplication::applicationDirPath()+"/bimeron-core"
#ifdef Q_OS_WIN
        ".exe"
#endif
        ;
    if(service.isEmpty() || !TrustedCore::protectedExecutable(service) || !TrustedCore::protectedExecutable(bundle)) { log("absent or untrusted service/payload; skipped"); return 0; }
    QByteArray a,b;
    if(!run(service,{"version"},a) || !run(bundle,{"version"},b)) {log("version query failed; skipped");return 0;}
    QString av=QJsonDocument::fromJson(a).object().value("version").toString();
    QString bv=QJsonDocument::fromJson(b).object().value("version").toString();
    if(!older(av,bv)) {log("equal, newer or unknown version; skipped");return 0;}
    QFile f(bundle); if(!f.open(QIODevice::ReadOnly)) {log("payload read failed; skipped");return 0;}
    QCryptographicHash hash(QCryptographicHash::Sha256); hash.addData(&f); f.close();
    return serviceDecision(true,av,bv,[&] {
    bool ok=run(bundle,TrustedCore::updateArguments(bundle,bv,QString::fromLatin1(hash.result().toHex())),a,-1);
    log(ok?"service updated; reconnect VPN in GUI":"update failed; Core rollback applies; package continues");
    if(!ok) fwrite(a.constData(),1,size_t(a.size()),stderr);
    });
}
}
