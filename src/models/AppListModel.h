#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QModelIndex>
#include <QSqlDatabase>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVector>

struct CatalogApp;

class AppListModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QVariantList sourceList READ sourceList NOTIFY sourceListChanged)

public:
    enum Role {
        AppIdRole = Qt::UserRole + 1,
        ComponentIdRole,
        DisplayNameRole,
        SummaryRole,
        DescriptionRole,
        IconNameRole,
        BackendRole,
        PackageIdRole,
        RepositoryRole,
        VerifiedRole,
        SourcesRole,
        PopularityRole
    };

    explicit AppListModel(
        const QSqlDatabase& database,
        QObject* parent = nullptr
    );

    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()
    ) const override;

    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role
    ) const override;

    [[nodiscard]] QHash<int, QByteArray> roleNames()
        const override;

    Q_INVOKABLE bool reload();
    Q_INVOKABLE bool search(const QString& text);
    [[nodiscard]] QVariantList storeApps() const;
    [[nodiscard]] QVariantList sourceList() const;
    void mergeFlatpakApps(const QVector<CatalogApp>& apps);
    void setPopularity(const QHash<QString, qint64>& scores);

signals:
    void loadFailed(const QString& message);
    void sourceListChanged();

private:
    struct AppRecord {
        qint64 appId = 0;
        QString componentId;
        QString displayName;
        QString summary;
        QString description;
        QString iconName;
        QString backend;
        QString packageId;
        QString repository;
        bool verified = false;
        QVariantList sources;
    };

    bool load(const QString& searchText);

    QSqlDatabase database_;
    QVector<AppRecord> apps_;
    QHash<QString, qint64> popularity_;
};
