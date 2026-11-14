#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc != 2) return 1;
    QFile payload(":/repair-bootstrap/broker");
    if (!payload.open(QIODevice::ReadOnly)) return 2;
    const auto digest = QCryptographicHash::hash(payload.readAll(), QCryptographicHash::Sha256).toHex();
    return digest == QByteArray(argv[1]) ? 0 : 3;
}
