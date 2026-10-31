#include "../src/models/AppSearchModel.h"

#include <QStandardItemModel>
#include <QtTest>

class AppSearchModelTest : public QObject
{
    Q_OBJECT

private slots:
    void searchesAndTracksChanges();
};

void AppSearchModelTest::searchesAndTracksChanges()
{
    constexpr int nameRole = Qt::UserRole + 1;
    constexpr int summaryRole = Qt::UserRole + 2;
    constexpr int idRole = Qt::UserRole + 3;
    QStandardItemModel source;
    source.setItemRoleNames({{nameRole, "displayName"}, {summaryRole, "summary"},
                             {idRole, "componentId"}});
    auto* krita = new QStandardItem;
    krita->setData(QStringLiteral("Krita"), nameRole);
    krita->setData(QStringLiteral("Digital painting"), summaryRole);
    krita->setData(QStringLiteral("org.kde.krita"), idRole);
    source.appendRow(krita);
    auto* steam = new QStandardItem;
    steam->setData(QStringLiteral("Steam"), nameRole);
    steam->setData(QStringLiteral("Games"), summaryRole);
    source.appendRow(steam);

    AppSearchModel search;
    search.setSourceModel(&source);
    QCOMPARE(search.count(), 2);
    search.setSearchText(QStringLiteral("KRITA painting"));
    QCOMPARE(search.count(), 1);
    search.setSearchText(QStringLiteral("org.kde"));
    QCOMPARE(search.count(), 1);
    search.setSearchText(QStringLiteral("no such app"));
    QCOMPARE(search.count(), 0);
    search.setSearchText(QStringLiteral("   "));
    QCOMPARE(search.count(), 2);
    search.setSearchText(QStringLiteral("games"));
    source.removeRow(1);
    QCOMPARE(search.count(), 0);
    search.setSearchText(QString());
    QCOMPARE(search.count(), 1);

    for (int i = 0; i < 100; ++i) source.appendRow(new QStandardItem);
    AppPageModel page;
    page.setSourceModel(&search);
    QCOMPARE(page.rowCount(), 48);
    page.showMore();
    QCOMPARE(page.rowCount(), 96);
    page.resetLimit();
    QCOMPARE(page.rowCount(), 48);
}

QTEST_GUILESS_MAIN(AppSearchModelTest)
#include "AppSearchModelTest.moc"
