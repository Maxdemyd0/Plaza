#include "PackageKitBackend.h"

#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFile>
#include <QRegularExpression>

namespace {
const QString service = QStringLiteral("org.freedesktop.PackageKit");
const QString rootPath = QStringLiteral("/org/freedesktop/PackageKit");
const QString transactionInterface = QStringLiteral("org.freedesktop.PackageKit.Transaction");
}

PackageKitBackend::PackageKitBackend(QObject* parent) : PackageKitBackend(QDBusConnection::systemBus(), parent) {}
PackageKitBackend::PackageKitBackend(const QDBusConnection& connection, QObject* parent) : QObject(parent), bus_(connection) {}
bool PackageKitBackend::supports(const QString& backend) const
{
    if (!bus_.isConnected() || QFile::exists(QStringLiteral("/run/ostree-booted"))) return false;
    QFile release(QStringLiteral("/etc/os-release"));
    if (!release.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QStringList families;
    while (!release.atEnd()) {
        const QString line = QString::fromUtf8(release.readLine()).trimmed();
        if (line.startsWith(QStringLiteral("ID=")) || line.startsWith(QStringLiteral("ID_LIKE=")))
            families += line.mid(line.indexOf(QLatin1Char('=')) + 1).remove(QLatin1Char('"')).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    }
    if (backend == QStringLiteral("apt")) return families.contains(QStringLiteral("debian")) || families.contains(QStringLiteral("ubuntu"));
    if (backend == QStringLiteral("dnf")) return families.contains(QStringLiteral("fedora")) || families.contains(QStringLiteral("rhel"));
    return false;
}
bool PackageKitBackend::isInstalled(const QString& package) const { return installed_.contains(package); }

void PackageKitBackend::install(const QString& backend, const QString& repository, const QString& package)
{
    if (busy_) return;
    if (!supports(backend) || repository.trimmed().isEmpty() ||
        !QRegularExpression(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9+._-]*$")).match(package).hasMatch()) {
        error_ = tr("This package source cannot be used on this system, or its package details are incomplete.");
        emit changed();
        return;
    }
    package_ = package;
    repository_ = repository;
    error_.clear();
    transactionError_.clear();
    candidates_.clear();
    alreadyInstalled_ = false;
    cancelled_ = false;
    resolving_ = true;
    operation_ = Operation::Resolve;
    busy_ = true;
    progress_ = 0;
    status_ = tr("Resolving package…");
    emit changed();
    createTransaction();
}

void PackageKitBackend::checkUpdates()
{
    if (busy_ || (!supports(QStringLiteral("dnf")) && !supports(QStringLiteral("apt")))) return;
    error_.clear();
    transactionError_.clear();
    updatePackageIds_.clear();
    updates_.clear();
    cancelled_ = false;
    resolving_ = false;
    operation_ = Operation::CheckUpdates;
    busy_ = true;
    progress_ = 0;
    status_ = tr("Checking system package updates…");
    emit changed();
    createTransaction();
}

void PackageKitBackend::updateAll()
{
    if (busy_ || updates_.isEmpty()) return;
    error_.clear();
    transactionError_.clear();
    cancelled_ = false;
    resolving_ = false;
    operation_ = Operation::UpdatePackages;
    updatePackageIds_.clear();
    for (const QVariant& entry : updates_) {
        const QString packageId = entry.toMap().value(QStringLiteral("packageId")).toString();
        if (!packageId.isEmpty() && !updatePackageIds_.contains(packageId)) updatePackageIds_.append(packageId);
    }
    if (updatePackageIds_.isEmpty()) return;
    busy_ = true;
    progress_ = 0;
    status_ = tr("Preparing system updates…");
    emit changed();
    createTransaction();
}

void PackageKitBackend::createTransaction()
{
    const auto request = QDBusMessage::createMethodCall(service, rootPath, service, QStringLiteral("CreateTransaction"));
    auto* watcher = new QDBusPendingCallWatcher(bus_.asyncCall(request), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) { fail(tr("Package service unavailable: %1").arg(reply.error().message())); return; }
        path_ = reply.value().path();
        if (cancelled_) { fail(tr("Installation cancelled.")); return; }
        bus_.connect(service, path_, transactionInterface, QStringLiteral("Package"), this, SLOT(packageFound(uint,QString,QString)));
        bus_.connect(service, path_, transactionInterface, QStringLiteral("Finished"), this, SLOT(transactionFinished(uint,uint)));
        bus_.connect(service, path_, transactionInterface, QStringLiteral("ErrorCode"), this, SLOT(transactionError(uint,QString)));
        bus_.connect(service, path_, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"), this, SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
        const QString methodName = operation_ == Operation::Resolve ? QStringLiteral("Resolve")
            : operation_ == Operation::Install ? QStringLiteral("InstallPackages")
            : operation_ == Operation::CheckUpdates ? QStringLiteral("GetUpdates")
            : QStringLiteral("UpdatePackages");
        QDBusMessage request = QDBusMessage::createMethodCall(service, path_, transactionInterface, methodName);
        if (operation_ == Operation::Resolve) {
            request.setArguments({QVariant::fromValue<qulonglong>(1ULL << 18), QStringList{package_}});
        } else if (operation_ == Operation::Install) {
            request.setArguments({QVariant::fromValue<qulonglong>(2), QStringList{candidates_.first()}});
        } else if (operation_ == Operation::CheckUpdates) {
            request.setArguments({QVariant::fromValue<qulonglong>(0)});
        } else {
            request.setArguments({QVariant::fromValue<qulonglong>(2), updatePackageIds_});
        }
        auto* method = new QDBusPendingCallWatcher(bus_.asyncCall(request), this);
        connect(method, &QDBusPendingCallWatcher::finished, this, [this, method] {
            QDBusPendingReply<> result = *method;
            method->deleteLater();
            if (result.isError() && busy_) fail(result.error().message());
        });
    });
}

void PackageKitBackend::packageFound(uint info, const QString& packageId, const QString& summary)
{
    if (operation_ == Operation::CheckUpdates) {
        if (info < 3 || info > 6) return;
        for (const QVariant& entry : updates_) {
            if (entry.toMap().value(QStringLiteral("packageId")).toString() == packageId) return;
        }
        const QStringList parts = packageId.split(QLatin1Char(';'));
        updates_.append(QVariantMap{{QStringLiteral("packageId"), packageId},
                                   {QStringLiteral("name"), parts.value(0)},
                                   {QStringLiteral("summary"), summary}});
        emit changed();
        return;
    }
    if (operation_ != Operation::Resolve) return;
    const QStringList parts = packageId.split(QLatin1Char(';'));
    if (parts.size() != 4 || parts[0] != package_) return;
    if (info == 1 && parts[3].startsWith(QStringLiteral("installed"))) alreadyInstalled_ = true;
    if (info == 2 && parts[3] == repository_ && !candidates_.contains(packageId)) candidates_.append(packageId);
}

void PackageKitBackend::transactionError(uint, const QString& details) { transactionError_ = details; }
void PackageKitBackend::propertiesChanged(const QString& interface, const QVariantMap& properties, const QStringList&)
{
    if (interface != transactionInterface) return;
    if (properties.contains(QStringLiteral("Percentage"))) {
        const uint percent = properties.value(QStringLiteral("Percentage")).toUInt();
        if (percent <= 100) progress_ = static_cast<int>(percent);
    }
    emit changed();
}

void PackageKitBackend::transactionFinished(uint exit, uint)
{
    disconnectTransaction();
    if (!busy_) return;
    if (exit != 1 || cancelled_ || !transactionError_.isEmpty()) {
        fail(cancelled_ ? tr("Installation cancelled.") : transactionError_.isEmpty()
            ? tr("Package transaction failed.") : transactionError_);
        return;
    }
    if (operation_ == Operation::CheckUpdates) {
        busy_ = false;
        progress_ = 100;
        status_ = updates_.isEmpty() ? tr("System is up to date") : tr("System updates are available");
        emit changed();
        return;
    }
    if (operation_ == Operation::UpdatePackages) {
        updates_.clear();
        ++cacheVersion_;
        busy_ = false;
        progress_ = 100;
        status_ = tr("System updates installed");
        emit changed();
        emit updatesInstalled();
        return;
    }
    if (operation_ == Operation::Resolve) {
        if (alreadyInstalled_) {
            installed_.insert(package_); ++cacheVersion_; busy_ = false; status_ = tr("Already installed");
            emit changed(); emit installed(package_); return;
        }
        if (candidates_.size() != 1) {
            fail(tr("Could not uniquely resolve %1 from the %2 source.").arg(package_, repository_));
            return;
        }
        resolving_ = false;
        operation_ = Operation::Install;
        progress_ = 0;
        status_ = tr("Installing package…");
        emit changed();
        createTransaction();
        return;
    }
    installed_.insert(package_); ++cacheVersion_; busy_ = false; progress_ = 100; status_ = tr("Installed");
    emit changed(); emit installed(package_);
}

void PackageKitBackend::disconnectTransaction()
{
    if (path_.isEmpty()) return;
    bus_.disconnect(service, path_, transactionInterface, QStringLiteral("Package"), this, SLOT(packageFound(uint,QString,QString)));
    bus_.disconnect(service, path_, transactionInterface, QStringLiteral("Finished"), this, SLOT(transactionFinished(uint,uint)));
    bus_.disconnect(service, path_, transactionInterface, QStringLiteral("ErrorCode"), this, SLOT(transactionError(uint,QString)));
    bus_.disconnect(service, path_, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"), this, SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
    path_.clear();
}
void PackageKitBackend::fail(const QString& message)
{
    disconnectTransaction();
    qWarning().noquote() << "PackageKit transaction failed:" << message;
    error_ = message;
    busy_ = false;
    progress_ = 0;
    status_ = cancelled_ && operation_ == Operation::CheckUpdates
        ? tr("Update check cancelled") : QString();
    emit changed();
}
void PackageKitBackend::cancel()
{
    if (!busy_) return;
    cancelled_ = true;
    status_ = tr("Cancelling…"); emit changed();
    if (!path_.isEmpty()) {
        const auto request = QDBusMessage::createMethodCall(service, path_, transactionInterface, QStringLiteral("Cancel"));
        bus_.asyncCall(request);
    }
}
