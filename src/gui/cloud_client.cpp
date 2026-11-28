#include "cloud_client.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <algorithm>

namespace moji {

namespace {
QString normalizeServer(QString raw) {
    raw = raw.trimmed();
    if (raw.isEmpty()) raw = "http://dsbot.taild07bbe.ts.net:8789";
    while (raw.endsWith('/')) raw.chop(1);
    if (!raw.startsWith("http://") && !raw.startsWith("https://")) {
        raw.prepend("http://");
    }
    return raw;
}

qint64 parseTime(const QString& raw) {
    if (raw.trimmed().isEmpty()) return 0;
    QDateTime dt = QDateTime::fromString(raw, "yyyy-MM-dd'T'HH:mm:ss");
    if (!dt.isValid()) dt = QDateTime::fromString(raw, Qt::ISODate);
    if (!dt.isValid()) dt = QDateTime::fromString(raw, "yyyy-MM-dd HH:mm:ss");
    if (!dt.isValid()) return 0;
    return dt.toSecsSinceEpoch();
}
}  // namespace

CloudClient::CloudClient(QObject* parent) : QObject(parent) {
    qRegisterMetaType<CloudMeta>("moji::CloudMeta");
    nam_ = new QNetworkAccessManager(this);
}

void CloudClient::configure(const QString& server, const QString& token) {
    server_ = normalizeServer(server);
    token_ = token.trimmed();
}

QString CloudClient::apiUrl(const QString& path) const {
    return server_ + path;
}

void CloudClient::attachAuth(QNetworkRequest& request) const {
    request.setHeader(QNetworkRequest::UserAgentHeader, "MoJi-Desktop/1.2.0");
    if (!token_.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + token_).toUtf8());
    }
}

QByteArray CloudClient::jsonBody(const QJsonObject& obj) {
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QString CloudClient::errorOf(const QByteArray& body, const QString& fallback) {
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        const QString msg = doc.object().value("error").toString();
        if (!msg.isEmpty()) return msg;
    }
    return fallback;
}

void CloudClient::cancel() {
    if (active_) {
        QNetworkReply* reply = active_;
        active_ = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
}

void CloudClient::login(const QString& email, const QString& password) {
    cancel();
    QNetworkRequest request(QUrl(apiUrl("/api/login")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    attachAuth(request);
    request.setTransferTimeout(20000);
    QJsonObject body{{"email", email.trimmed()}, {"password", password}};
    auto* reply = nam_->post(request, jsonBody(body));
    active_ = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (active_ == reply) active_ = nullptr;
        handleAuthReply(reply);
        reply->deleteLater();
    });
}

void CloudClient::registerAccount(const QString& email, const QString& password,
                                  const QString& displayName) {
    cancel();
    QNetworkRequest request(QUrl(apiUrl("/api/register")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    attachAuth(request);
    request.setTransferTimeout(20000);
    QJsonObject body{{"email", email.trimmed()},
                     {"password", password},
                     {"display_name", displayName.trimmed()}};
    auto* reply = nam_->post(request, jsonBody(body));
    active_ = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (active_ == reply) active_ = nullptr;
        handleAuthReply(reply);
        reply->deleteLater();
    });
}

void CloudClient::logout() {
    cancel();
    if (!token_.isEmpty()) {
        QNetworkRequest request(QUrl(apiUrl("/api/logout")));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        attachAuth(request);
        request.setTransferTimeout(10000);
        auto* reply = nam_->post(request, QByteArray("{}"));
        connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
    }
    token_.clear();
    lastMeta_ = {};
    emit loggedOut();
}

void CloudClient::fetchMeta() {
    cancel();
    QNetworkRequest request(QUrl(apiUrl("/api/sync/meta")));
    attachAuth(request);
    request.setTransferTimeout(20000);
    auto* reply = nam_->get(request);
    active_ = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (active_ == reply) active_ = nullptr;
        handleMetaReply(reply);
        reply->deleteLater();
    });
}

void CloudClient::downloadBundle() {
    cancel();
    QNetworkRequest request(QUrl(apiUrl("/api/sync/download")));
    attachAuth(request);
    request.setTransferTimeout(60000);
    auto* reply = nam_->get(request);
    active_ = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (active_ == reply) active_ = nullptr;
        handleDownloadReply(reply);
        reply->deleteLater();
    });
}

void CloudClient::uploadBundle(const QByteArray& data, int baseVersion,
                               const QStringList& deletedIds) {
    cancel();
    QNetworkRequest request(QUrl(apiUrl("/api/sync/upload")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    request.setRawHeader("X-Moji-Base-Version", QByteArray::number(baseVersion));
    if (!deletedIds.isEmpty()) {
        QJsonArray arr;
        for (const QString& id : deletedIds) arr.append(id);
        request.setRawHeader("X-Moji-Deleted",
                             QJsonDocument(arr).toJson(QJsonDocument::Compact));
    }
    attachAuth(request);
    request.setTransferTimeout(120000);
    auto* reply = nam_->post(request, data);
    active_ = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (active_ == reply) active_ = nullptr;
        handleUploadReply(reply);
        reply->deleteLater();
    });
}

void CloudClient::handleAuthReply(QNetworkReply* reply) {
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    if (status < 200 || status >= 300) {
        if (status == 0) {
            emit authFailed("无法连接服务器：" + reply->errorString() +
                            "\n请确认已安装并登录 Tailscale，或检查服务器地址。");
        } else {
            emit authFailed(errorOf(body, QString("请求失败（HTTP %1）").arg(status)));
        }
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    const QJsonObject obj = doc.object();
    const QString token = obj.value("token").toString();
    const QJsonObject user = obj.value("user").toObject();
    if (token.isEmpty()) {
        emit authFailed("服务器没有返回登录令牌");
        return;
    }
    token_ = token;
    emit authOk(token, user.value("email").toString(), user.value("display_name").toString());
}

void CloudClient::handleMetaReply(QNetworkReply* reply) {
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    if (status < 200 || status >= 300) {
        if (status == 0) {
            emit metaFailed("无法连接服务器：" + reply->errorString());
        } else if (status == 401) {
            emit metaFailed("登录已失效，请在云同步设置中重新登录");
        } else {
            emit metaFailed(errorOf(body, QString("获取云端状态失败（HTTP %1）").arg(status)));
        }
        return;
    }
    bool ok = false;
    CloudMeta meta = parseMeta(body, &ok);
    if (!ok) {
        emit metaFailed("云端返回的同步信息格式不正确");
        return;
    }
    lastMeta_ = meta;
    emit metaReady(meta);
}

void CloudClient::handleDownloadReply(QNetworkReply* reply) {
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    if (status == 404) {
        emit bundleFailed("云端还没有数据");
        return;
    }
    if (status < 200 || status >= 300) {
        if (status == 0) {
            emit bundleFailed("下载失败：" + reply->errorString());
        } else {
            emit bundleFailed(errorOf(body, QString("下载失败（HTTP %1）").arg(status)));
        }
        return;
    }
    emit bundleReady(lastMeta_, body);
}

void CloudClient::handleUploadReply(QNetworkReply* reply) {
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    if (status == 409) {
        emit uploadConflict();
        return;
    }
    if (status < 200 || status >= 300) {
        if (status == 0) {
            emit uploadFailed("上传失败：" + reply->errorString(), 0);
        } else {
            emit uploadFailed(errorOf(body, QString("上传失败（HTTP %1）").arg(status)), status);
        }
        return;
    }
    bool ok = false;
    CloudMeta meta = parseMeta(body, &ok);
    if (!ok) {
        emit uploadFailed("上传成功但服务器返回格式异常", status);
        return;
    }
    lastMeta_ = meta;
    emit uploadOk(meta);
}

CloudMeta CloudClient::parseMeta(const QByteArray& body, bool* ok) {
    if (ok) *ok = false;
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return {};
    const QJsonObject obj = doc.object();
    CloudMeta meta;
    meta.valid = true;
    meta.version = obj.value("version").toInt();
    meta.updated_at = static_cast<qint64>(obj.value("updated_at").toDouble());
    meta.size = static_cast<qint64>(obj.value("size").toDouble());
    meta.event_count = obj.value("event_count").toInt();
    meta.attachment_count = obj.value("attachment_count").toInt();
    const QJsonArray summary = obj.value("summary").toArray();
    for (const auto& v : summary) {
        const QJsonObject item = v.toObject();
        const QString id = item.value("id").toString();
        if (id.isEmpty()) continue;
        meta.updated.insert(id, parseTime(item.value("updated_at").toString()));
        meta.attachments.insert(id, item.value("attachments").toInt());
    }
    const QJsonObject tombstones = obj.value("tombstones").toObject();
    for (auto it = tombstones.constBegin(); it != tombstones.constEnd(); ++it) {
        meta.tombstones.insert(it.key(), static_cast<qint64>(it.value().toDouble()));
    }
    if (ok) *ok = true;
    return meta;
}

}  // namespace moji
