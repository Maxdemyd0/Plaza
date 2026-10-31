#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>

class PopularityProvider final : public QObject
{
    Q_OBJECT
public:
    explicit PopularityProvider(QObject* parent = nullptr);
    void load();
    static QHash<QString, qint64> parse(const QByteArray& data);

signals:
    void loaded(const QHash<QString, qint64>& scores);

private:
    QNetworkAccessManager network_;
};
