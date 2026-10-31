#include "../src/backends/FlatpakCatalog.h"
#include "../src/database/DatabaseController.h"
#include "../src/models/AppListModel.h"

#include <QTemporaryDir>
#include <QtTest>

class FlatpakCatalogTest : public QObject
{
    Q_OBJECT
private slots:
    void parseAndMerge();
    void databaseSources();
    void bundledCatalog();
};

void FlatpakCatalogTest::parseAndMerge()
{
    const QByteArray xml = R"XML(<components>
      <component type="desktop-application">
        <id>org.example.Paint</id><name>Paint</name><name xml:lang="fr">Peindre</name>
        <summary>Draw things</summary><description><p>Make art</p></description>
        <icon type="stock">paint-icon</icon>
        <custom><value key="flathub::verification::verified">true</value></custom>
        <bundle type="flatpak">app/org.example.Paint/x86_64/stable</bundle>
      </component>
      <component type="desktop"><id>org.example.Beta</id><name>Beta</name>
        <bundle type="flatpak">app/org.example.Beta/x86_64/beta</bundle>
      </component>
      <component type="runtime"><id>org.example.Runtime</id><name>Runtime</name>
        <bundle type="flatpak">app/org.example.Runtime/x86_64/stable</bundle>
      </component>
    </components>)XML";
    const auto apps = parseFlatpakAppstream(xml, QString(), QStringLiteral("flathub"), QStringLiteral("x86_64"));
    QCOMPARE(apps.size(), 1);
    QCOMPARE(apps[0].displayName, QStringLiteral("Paint"));
    QCOMPARE(apps[0].packageId, QStringLiteral("org.example.Paint"));
    QCOMPARE(apps[0].repository, QStringLiteral("flathub"));
    QVERIFY(apps[0].verified);
    QVERIFY(parseFlatpakAppstream(xml, QString(), QStringLiteral("flathub"), QStringLiteral("aarch64")).isEmpty());

    CatalogApp duplicate = apps[0];
    duplicate.repository = QStringLiteral("fedora");
    AppListModel model{QSqlDatabase()};
    model.mergeFlatpakApps({apps[0], duplicate});
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), AppListModel::RepositoryRole).toString(), QStringLiteral("flathub"));
    QCOMPARE(model.data(model.index(0, 0), AppListModel::BackendRole).toString(), QStringLiteral("flatpak"));
    QCOMPARE(model.data(model.index(0, 0), AppListModel::SourcesRole).toList().size(), 2);
}

void FlatpakCatalogTest::databaseSources()
{
    const QString host = qEnvironmentVariable("PLAZA_TEST_DB_HOST");
    if (host.isEmpty()) QSKIP("Temporary PostgreSQL test catalog not configured");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), QStringLiteral("catalog-test"));
    db.setHostName(host);
    db.setPort(qEnvironmentVariableIntValue("PLAZA_TEST_DB_PORT"));
    db.setDatabaseName(QStringLiteral("plaza"));
    db.setUserName(qEnvironmentVariable("USER"));
    QVERIFY(db.open());
    AppListModel model(db);
    QVERIFY(model.reload());
    bool foundKrita = false;
    for (int i = 0; i < model.rowCount(); ++i) {
        const QModelIndex index = model.index(i, 0);
        if (model.data(index, AppListModel::ComponentIdRole).toString() != QStringLiteral("org.kde.krita")) continue;
        foundKrita = true;
        const QVariantList sources = model.data(index, AppListModel::SourcesRole).toList();
        QVERIFY(!sources.isEmpty());
        QCOMPARE(sources.first().toMap().value(QStringLiteral("backend")).toString(), QStringLiteral("flatpak"));
        QCOMPARE(sources.first().toMap().value(QStringLiteral("repository")).toString(), QStringLiteral("flathub"));
    }
    QVERIFY(foundKrita);
}

void FlatpakCatalogTest::bundledCatalog()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DatabaseController database(directory.filePath(QStringLiteral("catalog.sqlite")));
    QVERIFY2(database.open(), qPrintable(database.lastError()));
    QVERIFY(database.isOpen());

    AppListModel model(database.database());
    QVERIFY(model.reload());
    QVERIFY(model.rowCount() >= 15);

    bool foundKrita = false;
    bool foundSteam = false;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        const QString componentId = model.data(index, AppListModel::ComponentIdRole).toString();
        if (componentId == QStringLiteral("org.kde.krita")) {
            foundKrita = true;
            QCOMPARE(model.data(index, AppListModel::PackageIdRole).toString(), componentId);
            QCOMPARE(model.data(index, AppListModel::RepositoryRole).toString(), QStringLiteral("flathub"));
        } else if (componentId == QStringLiteral("com.valvesoftware.Steam")) {
            foundSteam = true;
        }
    }
    QVERIFY(foundKrita);
    QVERIFY(foundSteam);

    // The starter Flathub entries remain installable before a remote's
    // AppStream catalog has been downloaded on a new installation.
    model.mergeFlatpakApps({});
    bool starterSourcePreserved = false;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        if (model.data(index, AppListModel::ComponentIdRole).toString() != QStringLiteral("org.kde.krita")) continue;
        const QVariantList sources = model.data(index, AppListModel::SourcesRole).toList();
        starterSourcePreserved = !sources.isEmpty()
            && sources.first().toMap().value(QStringLiteral("repository")).toString() == QStringLiteral("flathub");
        break;
    }
    QVERIFY(starterSourcePreserved);

    QVERIFY(model.search(QStringLiteral("firefox")));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), AppListModel::ComponentIdRole).toString(),
             QStringLiteral("org.mozilla.firefox"));
}

QTEST_GUILESS_MAIN(FlatpakCatalogTest)
#include "FlatpakCatalogTest.moc"
