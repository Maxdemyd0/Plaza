#include "../src/backends/GitHubUpdateController.h"

#include <QtTest>

class GitHubUpdateControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void comparesReleaseVersions()
    {
        QVERIFY(GitHubUpdateController::isNewerVersion(QStringLiteral("v1.1.0"), QStringLiteral("1.0.0")));
        QVERIFY(GitHubUpdateController::isNewerVersion(QStringLiteral("1.10.0"), QStringLiteral("1.9.0")));
        QVERIFY(!GitHubUpdateController::isNewerVersion(QStringLiteral("1.1.0"), QStringLiteral("v1.1.0")));
        QVERIFY(!GitHubUpdateController::isNewerVersion(QStringLiteral("1.0.0-rc1"), QStringLiteral("1.0.0")));
        QVERIFY(!GitHubUpdateController::isNewerVersion(QStringLiteral("latest"), QStringLiteral("1.0.0")));
    }

    void mapsCpuArchitecturesToDebian()
    {
        QCOMPARE(GitHubUpdateController::debianArchitectureForCpu(QStringLiteral("x86_64")), QStringLiteral("amd64"));
        QCOMPARE(GitHubUpdateController::debianArchitectureForCpu(QStringLiteral("i686")), QStringLiteral("i386"));
        QCOMPARE(GitHubUpdateController::debianArchitectureForCpu(QStringLiteral("aarch64")), QStringLiteral("arm64"));
        QCOMPARE(GitHubUpdateController::debianArchitectureForCpu(QStringLiteral("armv7l")), QStringLiteral("armhf"));
        QVERIFY(GitHubUpdateController::debianArchitectureForCpu(QStringLiteral("riscv64")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(GitHubUpdateControllerTest)

#include "GitHubUpdateControllerTest.moc"
