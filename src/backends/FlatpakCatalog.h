#pragma once

#include <QObject>
#include <QFutureWatcher>
#include <QVector>

struct CatalogApp {
    QString componentId;
    QString displayName;
    QString summary;
    QString description;
    QString iconName;
    QString packageId;
    QString repository;
    bool verified = false;
};

QVector<CatalogApp> parseFlatpakAppstream(const QByteArray& data, const QString& directory,
                                          const QString& remote, const QString& architecture);

class FlatpakCatalog final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
public:
    explicit FlatpakCatalog(QObject* parent = nullptr);
    ~FlatpakCatalog() override;
    bool loading() const;
    void load();

signals:
    void loadingChanged();
    void loaded(const QVector<CatalogApp>& apps);

private:
    QFutureWatcher<QVector<CatalogApp>> watcher_;
    bool reloadPending_ = false;
};
