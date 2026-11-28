#pragma once

#include <QByteArray>
#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class QJsonObject;

namespace moji {

// 服务端 /api/sync/meta 的精简表示；summary 用于启动时判断“变化较大”。
struct CloudMeta {
    bool valid = false;
    int version = 0;
    qint64 updated_at = 0;
    qint64 size = 0;
    int event_count = 0;
    int attachment_count = 0;
    QHash<QString, qint64> updated;   // id -> updated_at 秒
    QHash<QString, int> attachments;  // id -> 附件数量
    QHash<QString, qint64> tombstones; // id -> 删除时间（秒）
};

// 轻量 HTTP 客户端：只做 JSON 账号接口和 .mjnote 原样上传/下载。
class CloudClient : public QObject {
    Q_OBJECT
public:
    explicit CloudClient(QObject* parent = nullptr);

    void configure(const QString& server, const QString& token = {});
    QString server() const { return server_; }
    QString token() const { return token_; }
    bool hasToken() const { return !token_.isEmpty(); }

    void login(const QString& email, const QString& password);
    void registerAccount(const QString& email, const QString& password,
                         const QString& displayName);
    void logout();
    void fetchMeta();
    void downloadBundle();
    void uploadBundle(const QByteArray& data, int baseVersion,
                      const QStringList& deletedIds = {});
    void cancel();

signals:
    void authOk(const QString& token, const QString& email, const QString& name);
    void authFailed(const QString& message);
    void loggedOut();
    void metaReady(moji::CloudMeta meta);
    void metaFailed(const QString& message);
    void bundleReady(moji::CloudMeta meta, const QByteArray& data);
    void bundleFailed(const QString& message);
    void uploadOk(moji::CloudMeta meta);
    void uploadConflict();
    void uploadFailed(const QString& message, int status);

private:
    QString apiUrl(const QString& path) const;
    void attachAuth(QNetworkRequest& request) const;
    static QByteArray jsonBody(const QJsonObject& obj);
    static QString errorOf(const QByteArray& body, const QString& fallback);
    void handleAuthReply(QNetworkReply* reply);
    void handleMetaReply(QNetworkReply* reply);
    void handleDownloadReply(QNetworkReply* reply);
    void handleUploadReply(QNetworkReply* reply);
    static CloudMeta parseMeta(const QByteArray& body, bool* ok);

    QNetworkAccessManager* nam_ = nullptr;
    QString server_;
    QString token_;
    CloudMeta lastMeta_;
    QNetworkReply* active_ = nullptr;
};

}  // namespace moji

Q_DECLARE_METATYPE(moji::CloudMeta)
