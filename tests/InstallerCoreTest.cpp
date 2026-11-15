#include "InstallerCore.h"
#include <QTemporaryDir>
int main(int argc, char **argv) {
    QCoreApplication app(argc,argv);
    if(app.arguments().contains("--fake-failure")) return 7;
    if(app.arguments().contains("--fake-version")) {
        printf("{\"version\":\"0.2.0-beta.1\"}"); return 0;
    }
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
