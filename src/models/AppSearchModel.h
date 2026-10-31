#pragma once

#include <QSortFilterProxyModel>
#include <QStringList>

class AppSearchModel final : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit AppSearchModel(QObject* parent = nullptr);

    [[nodiscard]] QString searchText() const;
    [[nodiscard]] int count() const;
    void setSearchText(const QString& text);
    void enablePopularitySorting();

signals:
    void searchTextChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QString searchText_;
    QStringList terms_;
    int popularityRole_ = -1;
    int nameRole_ = -1;
};

class AppPageModel final : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int limit READ limit NOTIFY limitChanged)
public:
    explicit AppPageModel(QObject* parent = nullptr);
    int limit() const;
    Q_INVOKABLE void showMore();
    void resetLimit();
signals:
    void limitChanged();
protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
private:
    int limit_ = 48;
};
