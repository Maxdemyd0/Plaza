#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThread>
#include <QVariantList>

class FlatpakBackendWorker;

class FlatpakBackend final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString currentAppId READ currentAppId NOTIFY currentAppIdChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(bool cacheReady READ cacheReady NOTIFY cacheReadyChanged)
    Q_PROPERTY(int cacheVersion READ cacheVersion NOTIFY cacheVersionChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QVariantList updates READ updates NOTIFY updatesChanged)
    Q_PROPERTY(QVariantList remotes READ remotes NOTIFY remotesChanged)
    Q_PROPERTY(QString sourceStatusText READ sourceStatusText NOTIFY sourceStatusTextChanged)

public:
    explicit FlatpakBackend(QObject* parent = nullptr);
    ~FlatpakBackend() override;

    FlatpakBackend(const FlatpakBackend&) = delete;
    FlatpakBackend& operator=(const FlatpakBackend&) = delete;

    [[nodiscard]] bool busy() const;
    [[nodiscard]] QString currentAppId() const;
    [[nodiscard]] int progress() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] bool cacheReady() const;
    [[nodiscard]] int cacheVersion() const;
    [[nodiscard]] QString errorText() const;
    [[nodiscard]] QVariantList updates() const;
    [[nodiscard]] QVariantList remotes() const;
    [[nodiscard]] QString sourceStatusText() const;

    Q_INVOKABLE bool isInstalled(const QString& appId) const;
    Q_INVOKABLE void install(const QString& remote, const QString& appId);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void launch(const QString& appId);
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void checkUpdates();
    Q_INVOKABLE void updateAll();
    Q_INVOKABLE void refreshSources();
    Q_INVOKABLE void addSource(const QString& name, const QString& url);
    Q_INVOKABLE void setSourceEnabled(const QString& name, bool enabled);
    Q_INVOKABLE void removeSource(const QString& name);

signals:
    void busyChanged();
    void currentAppIdChanged();
    void progressChanged();
    void statusTextChanged();
    void cacheReadyChanged();
    void cacheVersionChanged();
    void errorTextChanged();
    void updatesChanged();
    void remotesChanged();
    void sourceStatusTextChanged();

    void installed(const QString& appId);
    void failed(const QString& appId, const QString& message);
    void launched(const QString& appId);
    void updatesInstalled();
    void sourcesChanged();

private slots:
    void onCacheLoaded(const QStringList& appIds);
    void onCacheLoadFailed(const QString& message);
    void onProgress(int progress, const QString& status);
    void onInstallSucceeded(const QString& appId);
    void onOperationFailed(const QString& appId, const QString& message);
    void onLaunchSucceeded(const QString& appId);
    void onLaunchFailed(const QString& appId, const QString& message);
    void onUpdatesLoaded(const QVariantList& updates);
    void onUpdatesInstalled(const QStringList& appIds);
    void onSourcesLoaded(const QVariantList& sources, const QString& status);

private:
    void setBusy(bool busy);
    void setCurrentAppId(const QString& appId);
    void setProgress(int progress);
    void setStatusText(const QString& status);
    void setErrorText(const QString& error);

    FlatpakBackendWorker* worker_ = nullptr;
    QThread workerThread_;
    QSet<QString> installedApps_;
    bool busy_ = false;
    QString currentAppId_;
    int progress_ = 0;
    QString statusText_;
    bool cacheReady_ = false;
    int cacheVersion_ = 0;
    QString errorText_;
    QVariantList updates_;
    QVariantList remotes_;
    QString sourceStatusText_;
};
