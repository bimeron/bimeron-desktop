#include "InstallerCore.h"
int main(int argc, char **argv) {
    QCoreApplication app(argc,argv);
    if(app.arguments().contains("--fake-failure")) return 7;
    if(app.arguments().contains("--fake-version")) {
        printf("{\"version\":\"0.2.0-beta.1\"}"); return 0;
    }
    using InstallerCore::older;
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
