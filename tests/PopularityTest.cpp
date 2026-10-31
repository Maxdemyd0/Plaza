#include "../src/backends/PopularityProvider.h"
#include "../src/models/AppSearchModel.h"

#include <QStandardItemModel>
#include <QtTest>

class PopularityTest : public QObject
{
    Q_OBJECT
private slots:
    void parseAndSort();
};
void PopularityTest::parseAndSort()
{
    const auto scores = PopularityProvider::parse(R"({"hits":[{"app_id":"org.test.App","installs_last_month":42},{"app_id":"bad","installs_last_month":-1}]})");
    QCOMPARE(scores.value(QStringLiteral("org.test.app")), 42);
    QCOMPARE(scores.size(), 1);
    QVERIFY(PopularityProvider::parse("not json").isEmpty());
    constexpr int name = Qt::UserRole + 1, popularity = Qt::UserRole + 2;
    QStandardItemModel source;
    source.setItemRoleNames({{name, "displayName"}, {popularity, "popularity"}});
    for (const auto& item : {qMakePair(QStringLiteral("Alpha"), -1), qMakePair(QStringLiteral("Beta"), 5), qMakePair(QStringLiteral("Zeta"), 100)}) {
        auto* row = new QStandardItem; row->setData(item.first, name); row->setData(item.second, popularity); source.appendRow(row);
    }
    AppSearchModel model; model.setSourceModel(&source); model.enablePopularitySorting();
    QCOMPARE(model.data(model.index(0, 0), name).toString(), QStringLiteral("Zeta"));
    QCOMPARE(model.data(model.index(2, 0), name).toString(), QStringLiteral("Alpha"));
    source.item(0)->setData(200, popularity);
    QCOMPARE(model.data(model.index(0, 0), name).toString(), QStringLiteral("Alpha"));
}
QTEST_GUILESS_MAIN(PopularityTest)
#include "PopularityTest.moc"
