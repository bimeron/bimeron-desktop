#include "../src/OfflineIKEv2Failure.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QCryptographicHash>
#include <cstdlib>
using namespace OfflineIKEv2Failure;
static void check(bool ok) { if (!ok) std::abort(); }
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    check(argc == 3);
    QFile file(QString::fromLocal8Bit(argv[1])); check(file.open(QIODevice::ReadOnly));
    const auto bytes = file.readAll();
    check(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() ==
        "946b21470b8e39df4ac55fb7762d1cf6d90b91d17d623e0a22b0da45f97e8e2e");
    const auto fixtures = QJsonDocument::fromJson(bytes).object();
    Failure result;
    for (const auto &group : {"valid", "invalid"}) for (const auto &item : fixtures.value(group).toArray()) {
        const auto row = item.toObject();
        auto member = QJsonDocument(row.value("native_failure").toObject()).toJson(QJsonDocument::Compact);
        if (row.value("native_failure").isNull()) member = "null";
        const auto got = parse(member, row.contains("native_failure"), row.value("attempt").toInteger(), result);
        check(got == (QString(group) == "invalid" ? Result::Invalid :
            row.contains("native_failure") ? Result::Valid : Result::Absent));
        if (got != Result::Valid) check(!result.operation && !result.cleanup);
        if (row.value("name") == "operation_and_cleanup") check(result.operation->code == 775 && result.cleanup->code == 668);
        if (row.value("name") == "unknown_uint32_max") check(result.operation->code == 4294967295u);
    }
    const QByteArray valid = R"({"version":1,"operation":{"domain":"windows_ras","code":775,"stage":"dial","attempt":1}})";
    for (const auto &bad : {QByteArray("{"), QByteArray("[]"), QByteArray("{}"), valid + QByteArray(513, ' ')}) {
        check(parse(valid, true, 1, result) == Result::Valid);
        check(parse(bad, true, 1, result) == Result::Invalid && !result.operation && !result.cleanup);
    }
    for (const auto &value : {"true", "null", "\"775\"", "-1", "0", "1.5", "4294967296"}) {
        auto bad = valid; bad.replace("775", value);
        check(parse(bad, true, 1, result) == Result::Invalid);
    }
    check(parse(valid, true, 2, result) == Result::Invalid);
    check(parse(valid, false, 1, result) == Result::Absent && !result.operation);
    QFile rawFile(QString::fromLocal8Bit(argv[2])); check(rawFile.open(QIODevice::ReadOnly));
    const auto rawBytes = rawFile.readAll();
    check(QCryptographicHash::hash(rawBytes,QCryptographicHash::Sha256).toHex()=="facd9603e45a910235d38326c1271ac52abc1ac5e69daaa55544f0b6d13a8a9b");
    for (const auto &item : QJsonDocument::fromJson(rawBytes).object().value("cases").toArray()) {
        const auto row=item.toObject();
        check((parse(row.value("raw").toString().toUtf8(),true,row.value("attempt").toInteger(),result)==Result::Valid)==row.value("valid").toBool());
    }
    check(parse(valid+QByteArray(512-valid.size(),' '),true,1,result)==Result::Valid);
    for (const auto &bad : {valid+"x", valid+QByteArray(513-valid.size(),' '), QByteArray("{\"version\":{}}"), QByteArray("{\"version\":[]}"), QByteArray("{\"version\":1,\"operation\":{\"domain\":\"\\uD800\"}}"), QByteArray("{\"version\":1,\"operation\":{\"domain\":\"\\\"\\\\\"}}"), QByteArray("{\"version\":1,\"operation\":{\"domain\":\"")+char(0xff)+"\"}}"})
        check(parse(bad,true,1,result)==Result::Invalid && !result.operation && !result.cleanup);
    for (const auto &escape : {"v", "i", "x", "u123", "u12G4"}) {
        auto key = valid; key.replace("version", QByteArray("\\")+escape+"version");
        auto value = valid; value.replace("dial", QByteArray("d\\")+escape+"ial");
        check(parse(key,true,1,result)==Result::Invalid);
        check(parse(value,true,1,result)==Result::Invalid);
    }
    // Exact r1 inputs remain otherwise valid after Qt's permissive escape decode.
    for (const auto &bad : {
        QByteArray(R"({"\version":1,"operation":{"domain":"windows_ras","code":775,"stage":"dial","attempt":1}})"),
        QByteArray(R"({"version":1,"operation":{"domain":"windows_ras","code":775,"stage":"d\ial","attempt":1}})")}) {
        if (parse(bad,true,1,result)!=Result::Invalid) {
            fprintf(stderr,"FAILED: exact illegal escape regression\n");
            std::abort();
        }
    }
    // Preflight checks syntax; final semantics still reject non-allowlisted strings.
    check(RawPreflight(QByteArray(R"({"version":1,"operation":{"domain":"a\\b","stage":"a\"b","code":1,"attempt":1}})")).valid());
    check(RawPreflight(QByteArray(R"({"vers\u0069on":1,"operation":{"domain":"\uD834\uDD1E","stage":"dial","code":1,"attempt":1}})")).valid());
    auto escapedKey=valid; escapedKey.replace("version","vers\\u0069on");
    check(parse(escapedKey,true,1,result)==Result::Valid);
    return 0;
}
