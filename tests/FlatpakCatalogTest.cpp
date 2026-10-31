#include "../src/backends/FlatpakCatalog.h"
#include "../src/models/AppListModel.h"

#include <QtTest>

class FlatpakCatalogTest : public QObject
{
    Q_OBJECT
private slots:
    void parseAndMerge();
    void databaseSources();
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

QTEST_GUILESS_MAIN(FlatpakCatalogTest)
#include "FlatpakCatalogTest.moc"
