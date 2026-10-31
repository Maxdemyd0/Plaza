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
};

QTEST_GUILESS_MAIN(GitHubUpdateControllerTest)

#include "GitHubUpdateControllerTest.moc"
