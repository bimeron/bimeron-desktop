#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QTimer>
#include <QPointer>
#include <cstring>
namespace GeoDataFixture {
static void require(bool ok, const char *message) { if (!ok) qFatal("%s", message); }
struct Response {
    QByteArray body;
    int status = 200;
    QUrl redirect;
    qint64 length = -1;
    bool hold = false;
    bool tls = false;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
};
class Reply final : public QNetworkReply {
public:
    Reply(const QNetworkRequest &request, Response response, QObject *parent)
        : QNetworkReply(parent), response_(std::move(response)) {
        setRequest(request); setUrl(request.url());
        open(QIODevice::ReadOnly);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response_.status);
        if (!response_.redirect.isEmpty()) setAttribute(QNetworkRequest::RedirectionTargetAttribute, response_.redirect);
        if (response_.length >= 0) setHeader(QNetworkRequest::ContentLengthHeader, response_.length);
        QTimer::singleShot(0, this, [this] {
            if (aborted_) return;
            emit metaDataChanged();
            if (aborted_) return;
            if (response_.tls) { emit sslErrors({}); if (aborted_) return; }
            if (response_.hold) return;
            emit readyRead();
            if (aborted_) return;
            finish();
        });
    }
    void abort() override { aborted_ = true; }
    void finish() {
        if (response_.error != NoError) setError(response_.error, "fixture https://user:SYNTHETIC_SECRET@example.invalid/?token=SYNTHETIC_TOKEN");
        setFinished(true); emit finished();
    }
    qint64 bytesAvailable() const override { return response_.body.size() - offset_ + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *data, qint64 maximum) override {
        const qint64 count = qMin(maximum, qint64(response_.body.size()) - offset_);
        if (!count) return -1;
        memcpy(data, response_.body.constData() + offset_, size_t(count)); offset_ += count; return count;
    }
private:
    Response response_;
    qint64 offset_ = 0;
    bool aborted_ = false;
};
class Network final : public QNetworkAccessManager {
public:
    QList<Response> responses;
    QList<QUrl> urls;
    QList<QPointer<Reply>> replies;
protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request, QIODevice *outgoing) override {
        Q_UNUSED(outgoing);
        require(operation == GetOperation, "unexpected network operation");
        require(request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt() == QNetworkRequest::ManualRedirectPolicy,
                "automatic redirect policy enabled");
        require(!responses.isEmpty(), "unexpected request");
        urls.append(request.url());
        auto *reply = new Reply(request, responses.takeFirst(), this);
        replies.append(reply);
        return reply;
    }
};
static QByteArray metadata(const QString &kind = "geoip", const QString &tag = "20261101") {
    QJsonArray assets;
    for (const auto &suffix : {QStringLiteral(".dat"), QStringLiteral(".dat.sha256sum")}) {
        const QString name = kind + suffix;
        assets.append(QJsonObject{{"name", name}, {"browser_download_url",
            "https://github.com/Loyalsoldier/v2ray-rules-dat/releases/download/" + tag + '/' + name}});
    }
    return QJsonDocument(QJsonObject{{"tag_name", tag}, {"assets", assets}}).toJson();
}
static QByteArray checksum(const QByteArray &data, const QByteArray &kind = "geoip") {
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex() + "  " + kind + ".dat\n";
}
}
