#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>

class PackageKitBackend final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString currentPackage READ currentPackage NOTIFY changed)
    Q_PROPERTY(int progress READ progress NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(int cacheVersion READ cacheVersion NOTIFY changed)
    Q_PROPERTY(QVariantList updates READ updates NOTIFY changed)
public:
    explicit PackageKitBackend(QObject* parent = nullptr);
    explicit PackageKitBackend(const QDBusConnection& connection, QObject* parent = nullptr);
    bool busy() const { return busy_; }
    QString currentPackage() const { return package_; }
    int progress() const { return progress_; }
    QString statusText() const { return status_; }
    QString errorText() const { return error_; }
    int cacheVersion() const { return cacheVersion_; }
    QVariantList updates() const { return updates_; }
    Q_INVOKABLE bool supports(const QString& backend) const;
    Q_INVOKABLE bool isInstalled(const QString& package) const;
    Q_INVOKABLE void install(const QString& backend, const QString& repository, const QString& package);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void checkUpdates();
    Q_INVOKABLE void updateAll();
signals:
    void changed();
    void installed(const QString& package);
    void updatesInstalled();
private slots:
    void packageFound(uint info, const QString& packageId, const QString& summary);
    void transactionFinished(uint exit, uint runtime);
    void transactionError(uint code, const QString& details);
    void propertiesChanged(const QString& interface, const QVariantMap& changed, const QStringList& invalidated);
private:
    void createTransaction();
    void disconnectTransaction();
    void fail(const QString& message);
    QDBusConnection bus_;
    bool busy_ = false;
    bool resolving_ = false;
    bool cancelled_ = false;
    bool alreadyInstalled_ = false;
    enum class Operation { Resolve, Install, CheckUpdates, UpdatePackages };
    Operation operation_ = Operation::Resolve;
    int progress_ = 0;
    int cacheVersion_ = 0;
    QString package_, repository_, path_, status_, error_, transactionError_;
    QStringList candidates_;
    QStringList updatePackageIds_;
    QSet<QString> installed_;
    QVariantList updates_;
};
