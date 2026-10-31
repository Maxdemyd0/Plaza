#include "../src/backends/FlatpakBackend.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

class FlatpakBackendTest final : public QObject
{
    Q_OBJECT

private slots:
    void installConfiguresMissingFlathub();
};

void FlatpakBackendTest::installConfiguresMissingFlathub()
{
    if (qEnvironmentVariableIsEmpty("PLAZA_RUN_FLATPAK_NETWORK_TESTS")) {
        QSKIP("Set PLAZA_RUN_FLATPAK_NETWORK_TESTS=1 to run the isolated Flathub integration test");
    }

    QTemporaryDir dataHome;
    QVERIFY(dataHome.isValid());
    qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());

    FlatpakBackend backend;
    QSignalSpy failedSpy(&backend, &FlatpakBackend::failed);
    QVERIFY(failedSpy.isValid());

    const QString missingAppId = QStringLiteral("org.plaza.TestAppThatDoesNotExist");
    backend.install(QStringLiteral("flathub"), missingAppId);

    QTRY_VERIFY_WITH_TIMEOUT(!failedSpy.isEmpty(), 120000);
    QCOMPARE(failedSpy.constFirst().at(0).toString(), missingAppId);
    QVERIFY(!failedSpy.constFirst().at(1).toString().contains(
        QStringLiteral("software source is not configured"), Qt::CaseInsensitive));

    backend.refreshSources();
    QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 30000);
    const QVariantList remotes = backend.remotes();
    const bool flathubConfigured = std::any_of(
        remotes.cbegin(), remotes.cend(), [](const QVariant& value) {
            return value.toMap().value(QStringLiteral("remoteName")).toString()
                .compare(QStringLiteral("flathub"), Qt::CaseInsensitive) == 0;
        });
    QVERIFY(flathubConfigured);
}

QTEST_GUILESS_MAIN(FlatpakBackendTest)
#include "FlatpakBackendTest.moc"
