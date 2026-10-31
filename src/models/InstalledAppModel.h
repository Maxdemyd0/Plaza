#pragma once

#include <QAbstractListModel>
#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVector>

class InstalledAppModel final : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int version READ version NOTIFY versionChanged)

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
        SourcesRole
    };

    explicit InstalledAppModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()
    ) const override;

    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role
    ) const override;

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int version() const;

    Q_INVOKABLE bool reload();
    void setStoreApps(const QVariantList& storeApps);
    Q_INVOKABLE bool isInstalled(
        const QString& componentId,
        const QString& packageId,
        const QString& displayName
    ) const;

signals:
    void versionChanged();

private:
    struct AppRecord {
        qint64 appId = 0;
        QString componentId;
        QString displayName;
        QString summary;
        QString description;
        QString iconName;
        QString packageId;
        QString source;
        bool verified = false;
    };

    static QString matchKey(const QString& value);
    void applyStoreMetadata(AppRecord& app) const;

    QVector<AppRecord> apps_;
    QHash<QString, QVariantMap> storeById_;
    QHash<QString, QVariantMap> storeByName_;
    QSet<QString> installedIds_;
    QSet<QString> installedNames_;
    int version_ = 0;
};
