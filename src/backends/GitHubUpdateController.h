#pragma once

#include <QNetworkAccessManager>
#include <QCryptographicHash>
#include <QObject>
#include <QString>
#include <QStringList>

class PackageKitBackend;
class QNetworkReply;
class QSaveFile;

class GitHubUpdateController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY changed)
    Q_PROPERTY(bool downloadReady READ downloadReady NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY changed)
    Q_PROPERTY(int progress READ progress NOTIFY changed)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)

public:
    explicit GitHubUpdateController(
        QString currentVersion,
        PackageKitBackend* packageKit,
        QObject* parent = nullptr
    );
    ~GitHubUpdateController() override;

    [[nodiscard]] bool checking() const;
    [[nodiscard]] bool updateAvailable() const;
    [[nodiscard]] bool downloadReady() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] bool restartRequired() const;
    [[nodiscard]] int progress() const;
    [[nodiscard]] QString latestVersion() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString errorText() const;

    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void downloadAndInstall();
    Q_INVOKABLE void restartPlaza();

    static bool isNewerVersion(const QString& candidate, const QString& current);
    static QString debianArchitectureForCpu(const QString& architecture);

signals:
    void changed();

private:
    void setError(const QString& error);
    void finishDownload(QNetworkReply* reply);
    void cleanupDownload();

    QString currentVersion_;
    QString latestVersion_;
    QString statusText_;
    QString errorText_;
    QString assetUrl_;
    QString expectedSha256_;
    QString downloadPath_;
    QStringList restartArguments_;
    PackageKitBackend* packageKit_ = nullptr;
    QNetworkAccessManager network_;
    QCryptographicHash hash_{QCryptographicHash::Sha256};
    QNetworkReply* downloadReply_ = nullptr;
    QSaveFile* downloadFile_ = nullptr;
    bool checking_ = false;
    bool updateAvailable_ = false;
    bool downloadReady_ = false;
    bool downloading_ = false;
    bool installing_ = false;
    bool restartRequired_ = false;
    int downloadProgress_ = 0;
};
