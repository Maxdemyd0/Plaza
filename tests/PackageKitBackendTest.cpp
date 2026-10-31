#include "../src/backends/PackageKitBackend.h"

#include <QDBusObjectPath>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

class FakeTransaction : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.PackageKit.Transaction")
public:
    QString repository = QStringLiteral("fedora");
    bool* installed = nullptr;
    bool* trusted = nullptr;
public slots:
    void Resolve(qulonglong, const QStringList& names) {
        const QString id = names.first() + QStringLiteral(";1;x86_64;") + repository;
        QTimer::singleShot(0, this, [this, id] { emit Package(2, id, QString()); emit Finished(1, 0); });
    }
    void InstallPackages(qulonglong flags, const QStringList&) {
        *installed = true; *trusted = (flags & 2) != 0;
        QTimer::singleShot(0, this, [this] { emit Finished(1, 0); });
    }
    void Cancel() { emit Finished(3, 0); }
signals:
    void Package(uint info, const QString& id, const QString& summary);
    void Finished(uint exit, uint runtime);
    void ErrorCode(uint code, const QString& details);
};

class FakeManager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.PackageKit")
public:
    QDBusConnection bus = QDBusConnection::sessionBus();
    QString repository = QStringLiteral("fedora");
    bool installed = false;
    bool trusted = false;
    int next = 0;
public slots:
    QDBusObjectPath CreateTransaction() {
        const QString path = QStringLiteral("/test/transaction%1").arg(++next);
        auto* transaction = new FakeTransaction;
        transaction->setParent(this);
        transaction->repository = repository;
        transaction->installed = &installed;
        transaction->trusted = &trusted;
        bus.registerObject(path, transaction, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
        return QDBusObjectPath(path);
    }
};

class PackageKitBackendTest : public QObject
{
    Q_OBJECT
private slots:
    void exactSourceAndTrustedInstall();
};

void PackageKitBackendTest::exactSourceAndTrustedInstall()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.PackageKit")));
    FakeManager manager;
    QVERIFY(bus.registerObject(QStringLiteral("/org/freedesktop/PackageKit"), &manager,
                               QDBusConnection::ExportAllSlots));
    PackageKitBackend backend(bus);
    const QString nativeBackend = backend.supports(QStringLiteral("dnf")) ? QStringLiteral("dnf") : QStringLiteral("apt");
    if (!backend.supports(nativeBackend)) QSKIP("No supported native distribution");
    QSignalSpy installed(&backend, &PackageKitBackend::installed);
    backend.install(nativeBackend, QStringLiteral("fedora"), QStringLiteral("krita"));
    QTRY_COMPARE_WITH_TIMEOUT(installed.size(), 1, 3000);
    QVERIFY(manager.installed);
    QVERIFY(manager.trusted);
    QVERIFY(backend.isInstalled(QStringLiteral("krita")));
    QVERIFY(!backend.busy());

    manager.installed = false;
    manager.repository = QStringLiteral("other-repo");
    backend.install(nativeBackend, QStringLiteral("fedora"), QStringLiteral("steam"));
    QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 3000);
    QVERIFY(!manager.installed);
    QVERIFY(backend.errorText().contains(QStringLiteral("fedora")));
    backend.install(nativeBackend, QStringLiteral("fedora"), QStringLiteral("../invalid"));
    QVERIFY(!backend.busy());
    QVERIFY(!backend.errorText().isEmpty());
    backend.install(nativeBackend, QStringLiteral("fedora"), QStringLiteral("example"));
    backend.cancel();
    QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 3000);
    QVERIFY(!manager.installed);
    bus.unregisterService(QStringLiteral("org.freedesktop.PackageKit"));
}

QTEST_GUILESS_MAIN(PackageKitBackendTest)
#include "PackageKitBackendTest.moc"
