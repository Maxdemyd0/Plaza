#include "PopularityProvider.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
constexpr qint64 maxResponseBytes = 4 * 1024 * 1024;
QString cachePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/flathub-popularity.json");
}
}

PopularityProvider::PopularityProvider(QObject* parent) : QObject(parent) {}

QHash<QString, qint64> PopularityProvider::parse(const QByteArray& data)
{
    QHash<QString, qint64> scores;
    const QJsonDocument document = QJsonDocument::fromJson(data);
    const QJsonValue hits = document.object().value(QStringLiteral("hits"));
    if (!hits.isArray()) return scores;
    for (const QJsonValue& value : hits.toArray()) {
        const QJsonObject entry = value.toObject();
        const QString id = entry.value(QStringLiteral("app_id")).toString();
        const QJsonValue installs = entry.value(QStringLiteral("installs_last_month"));
        const double count = installs.toDouble(-1);
        if (!id.isEmpty() && installs.isDouble() && count >= 0 && count <= 1e12)
            scores.insert(id.toCaseFolded(), static_cast<qint64>(count));
    }
    return scores;
}

void PopularityProvider::load()
{
    const QString path = cachePath();
    QFile cache(path);
    bool cacheValid = false;
    if (cache.open(QIODevice::ReadOnly) && cache.size() <= maxResponseBytes) {
        const auto scores = parse(cache.readAll());
        if (!scores.isEmpty()) {
            emit loaded(scores);
            cacheValid = true;
        }
    }
    const QFileInfo info(path);
    if (cacheValid && info.lastModified().secsTo(QDateTime::currentDateTime()) < 24 * 60 * 60) return;

    QNetworkRequest request(QUrl(QStringLiteral("https://flathub.org/api/v2/collection/popular")));
    request.setTransferTimeout(10000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", "Plaza/0.1 (software catalog)");
    QNetworkReply* reply = network_.get(request);
    connect(reply, &QIODevice::readyRead, this, [reply] {
        if (reply->bytesAvailable() > maxResponseBytes) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, path] {
        const QByteArray data = reply->readAll();
        if (reply->error() == QNetworkReply::NoError && data.size() <= maxResponseBytes) {
            const auto scores = parse(data);
            if (!scores.isEmpty()) {
                emit loaded(scores);
                QDir().mkpath(QFileInfo(path).absolutePath());
                QSaveFile cacheFile(path);
                if (cacheFile.open(QIODevice::WriteOnly) && cacheFile.write(data) == data.size()) cacheFile.commit();
            }
        } else {
            qInfo() << "Flathub popularity unavailable:" << reply->errorString();
        }
        reply->deleteLater();
    });
}
