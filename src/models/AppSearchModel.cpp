#include "AppSearchModel.h"

#include <QRegularExpression>

AppSearchModel::AppSearchModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    connect(this, &QAbstractItemModel::modelReset, this, &AppSearchModel::countChanged);
    connect(this, &QAbstractItemModel::rowsInserted, this, &AppSearchModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &AppSearchModel::countChanged);
}

QString AppSearchModel::searchText() const
{
    return searchText_;
}

int AppSearchModel::count() const
{
    return rowCount();
}

void AppSearchModel::enablePopularitySorting()
{
    if (!sourceModel()) return;
    popularityRole_ = sourceModel()->roleNames().key("popularity", -1);
    nameRole_ = sourceModel()->roleNames().key("displayName", -1);
    if (popularityRole_ >= 0 && nameRole_ >= 0) {
        setSortRole(popularityRole_);
        sort(0);
    }
}

bool AppSearchModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    if (popularityRole_ < 0) return QSortFilterProxyModel::lessThan(left, right);
    const qint64 a = sourceModel()->data(left, popularityRole_).toLongLong();
    const qint64 b = sourceModel()->data(right, popularityRole_).toLongLong();
    if (a != b) return a > b;
    return QString::localeAwareCompare(sourceModel()->data(left, nameRole_).toString(),
                                       sourceModel()->data(right, nameRole_).toString()) < 0;
}

void AppSearchModel::setSearchText(const QString& text)
{
    if (searchText_ == text) {
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
#endif
    searchText_ = text;
    terms_ = text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    invalidateFilter();
#endif
    emit searchTextChanged();
}

bool AppSearchModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (terms_.isEmpty()) {
        return true;
    }
    if (!sourceModel()) {
        return false;
    }

    const QModelIndex item = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto roles = sourceModel()->roleNames();
    QString searchable;
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        const QByteArray& role = it.value();
        if (role == "displayName" || role == "summary" || role == "description" ||
            role == "componentId" || role == "packageId") {
            searchable += QLatin1Char(' ') + sourceModel()->data(item, it.key()).toString();
        }
    }
    for (const QString& term : terms_) {
        if (!searchable.contains(term, Qt::CaseInsensitive)) {
            return false;
        }
    }
    return true;
}

AppPageModel::AppPageModel(QObject* parent) : QSortFilterProxyModel(parent) {}
int AppPageModel::limit() const { return limit_; }
void AppPageModel::showMore()
{
    limit_ += 48;
    invalidate();
    emit limitChanged();
}
void AppPageModel::resetLimit()
{
    if (limit_ == 48) return;
    limit_ = 48;
    invalidate();
    emit limitChanged();
}
bool AppPageModel::filterAcceptsRow(int row, const QModelIndex&) const { return row < limit_; }
