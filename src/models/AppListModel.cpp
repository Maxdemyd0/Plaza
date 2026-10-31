#include "AppListModel.h"
#include "../backends/FlatpakCatalog.h"

#include <QDebug>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QSysInfo>

#include <utility>
#include <algorithm>

namespace {
QVariantMap sourceRecord(const QString& backend, const QString& packageId, const QString& repository)
{
    return {{QStringLiteral("backend"), backend}, {QStringLiteral("packageId"), packageId},
            {QStringLiteral("repository"), repository}};
}

bool compatibleSource(const QString& backend, const QString& architecture)
{
    QString currentArch = QSysInfo::currentCpuArchitecture();
    if (currentArch == QStringLiteral("arm64")) currentArch = QStringLiteral("aarch64");
    if (!architecture.isEmpty() && architecture != currentArch && architecture != QStringLiteral("all") && architecture != QStringLiteral("noarch")) return false;
    if (backend == QStringLiteral("flatpak")) return true;
    QFile osRelease(QStringLiteral("/etc/os-release"));
    if (!osRelease.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QString ids;
    while (!osRelease.atEnd()) {
        const QString line = QString::fromUtf8(osRelease.readLine()).trimmed();
        if (line.startsWith(QStringLiteral("ID=")) || line.startsWith(QStringLiteral("ID_LIKE=")))
            ids += QLatin1Char(' ') + line.mid(line.indexOf(QLatin1Char('=')) + 1).remove(QLatin1Char('"'));
    }
    const QStringList families = ids.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (backend == QStringLiteral("apt")) return families.contains(QStringLiteral("debian")) || families.contains(QStringLiteral("ubuntu"));
    if (backend == QStringLiteral("dnf")) return families.contains(QStringLiteral("fedora")) || families.contains(QStringLiteral("rhel"));
    if (backend == QStringLiteral("rpm-ostree")) return QFile::exists(QStringLiteral("/run/ostree-booted"));
    return false;
}

void appendSource(QVariantList& sources, const QVariantMap& source)
{
    for (const QVariant& value : sources) {
        const auto item = value.toMap();
        if (item.value(QStringLiteral("backend")) == source.value(QStringLiteral("backend")) &&
            item.value(QStringLiteral("packageId")) == source.value(QStringLiteral("packageId")) &&
            item.value(QStringLiteral("repository")) == source.value(QStringLiteral("repository"))) return;
    }
    sources.append(source);
}
}

AppListModel::AppListModel(
    const QSqlDatabase& database,
    QObject* parent
)
    : QAbstractListModel(parent),
      database_(database)
{
}

int AppListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(apps_.size());
}

QVariant AppListModel::data(
    const QModelIndex& index,
    const int role
) const
{
    if (!index.isValid()) {
        return {};
    }

    if (index.row() < 0 ||
        index.row() >= static_cast<int>(apps_.size())) {
        return {};
    }

    const AppRecord& app = apps_.at(index.row());

    switch (role) {
    case AppIdRole:
        return app.appId;

    case ComponentIdRole:
        return app.componentId;

    case DisplayNameRole:
        return app.displayName;

    case SummaryRole:
        return app.summary;

    case DescriptionRole:
        return app.description;

    case IconNameRole:
        return app.iconName;

    case BackendRole:
        return app.backend;

    case PackageIdRole:
        return app.packageId;

    case RepositoryRole:
        return app.repository;

    case VerifiedRole:
        return app.verified;

    case SourcesRole:
        return app.sources;

    case PopularityRole:
        for (const QVariant& source : app.sources) {
            const QVariantMap item = source.toMap();
            if (item.value(QStringLiteral("backend")).toString() == QStringLiteral("flatpak") &&
                item.value(QStringLiteral("repository")).toString().compare(QStringLiteral("flathub"), Qt::CaseInsensitive) == 0)
                return popularity_.value(item.value(QStringLiteral("packageId")).toString().toCaseFolded(), -1);
        }
        return qint64(-1);

    default:
        return {};
    }
}

QHash<int, QByteArray> AppListModel::roleNames() const
{
    return {
        { AppIdRole, "appId" },
        { ComponentIdRole, "componentId" },
        { DisplayNameRole, "displayName" },
        { SummaryRole, "summary" },
        { DescriptionRole, "description" },
        { IconNameRole, "iconName" },
        { BackendRole, "backend" },
        { PackageIdRole, "packageId" },
        { RepositoryRole, "repository" },
        { VerifiedRole, "verified" },
        { SourcesRole, "sources" },
        { PopularityRole, "popularity" }
    };
}

bool AppListModel::reload()
{
    return load(QString());
}

bool AppListModel::search(const QString& text)
{
    return load(text.trimmed());
}

void AppListModel::setPopularity(const QHash<QString, qint64>& scores)
{
    popularity_ = scores;
    if (!apps_.isEmpty()) emit dataChanged(index(0, 0), index(apps_.size() - 1, 0), {PopularityRole});
}

QVariantList AppListModel::storeApps() const
{
    QVariantList entries;
    entries.reserve(apps_.size());

    for (const AppRecord& app : apps_) {
        QVariantMap entry;
        entry.insert(QStringLiteral("componentId"), app.componentId);
        entry.insert(QStringLiteral("displayName"), app.displayName);
        entry.insert(QStringLiteral("summary"), app.summary);
        entry.insert(QStringLiteral("description"), app.description);
        entry.insert(QStringLiteral("iconName"), app.iconName);
        entry.insert(QStringLiteral("packageId"), app.packageId);
        entry.insert(QStringLiteral("verified"), app.verified);
        entries.append(entry);
    }

    return entries;
}

QVariantList AppListModel::sourceList() const
{
    QVariantList result;
    for (const AppRecord& app : apps_) {
        QStringList countedSources;
        for (const QVariant& entry : app.sources) {
            const QVariantMap source = entry.toMap();
            const QString backend = source.value(QStringLiteral("backend")).toString();
            const QString repository = source.value(QStringLiteral("repository")).toString();
            const QString key = backend + QLatin1Char('\n') + repository;
            if (countedSources.contains(key)) continue;
            countedSources.append(key);

            bool found = false;
            for (QVariant& listed : result) {
                QVariantMap summary = listed.toMap();
                if (summary.value(QStringLiteral("backend")).toString() == backend &&
                    summary.value(QStringLiteral("repository")).toString() == repository) {
                    summary.insert(QStringLiteral("appCount"), summary.value(QStringLiteral("appCount")).toInt() + 1);
                    listed = summary;
                    found = true;
                    break;
                }
            }
            if (!found) result.append(QVariantMap{{QStringLiteral("backend"), backend},
                                                  {QStringLiteral("repository"), repository},
                                                  {QStringLiteral("appCount"), 1}});
        }
    }
    return result;
}

void AppListModel::mergeFlatpakApps(const QVector<CatalogApp>& catalog)
{
    QVector<AppRecord> merged = apps_;
    for (AppRecord& app : merged) {
        app.sources.erase(std::remove_if(app.sources.begin(), app.sources.end(), [](const QVariant& value) {
            const QVariantMap source = value.toMap();
            return source.value(QStringLiteral("backend")).toString() == QStringLiteral("flatpak") &&
                source.value(QStringLiteral("repository")).toString().compare(QStringLiteral("flathub"), Qt::CaseInsensitive) != 0;
        }), app.sources.end());
        if (app.backend == QStringLiteral("flatpak")) {
            app.backend.clear();
            app.packageId.clear();
            app.repository.clear();
            if (!app.sources.isEmpty()) {
                const QVariantMap fallback = app.sources.first().toMap();
                app.backend = fallback.value(QStringLiteral("backend")).toString();
                app.packageId = fallback.value(QStringLiteral("packageId")).toString();
                app.repository = fallback.value(QStringLiteral("repository")).toString();
            }
        }
    }
    merged.erase(std::remove_if(merged.begin(), merged.end(), [](const AppRecord& app) {
        return app.appId == 0 && app.backend.isEmpty() && app.sources.isEmpty();
    }), merged.end());
    QHash<QString, int> byId;
    for (int i = 0; i < merged.size(); ++i) {
        byId.insert(merged[i].componentId.toCaseFolded(), i);
        if (!merged[i].packageId.isEmpty()) byId.insert(merged[i].packageId.toCaseFolded(), i);
    }
    for (const CatalogApp& entry : catalog) {
        const QString componentKey = entry.componentId.toCaseFolded();
        const QString packageKey = entry.packageId.toCaseFolded();
        const int index = byId.value(packageKey, byId.value(componentKey, -1));
        if (index >= 0) {
            AppRecord& existing = merged[index];
            appendSource(existing.sources, sourceRecord(QStringLiteral("flatpak"), entry.packageId, entry.repository));
            if (existing.backend.isEmpty()) {
                existing.backend = QStringLiteral("flatpak");
                existing.packageId = entry.packageId;
                existing.repository = entry.repository;
            }
            if (entry.iconName.startsWith(QStringLiteral("file:"))) existing.iconName = entry.iconName;
            byId.insert(componentKey, index);
            byId.insert(packageKey, index);
            continue;
        }
        AppRecord app;
        app.componentId = entry.componentId;
        app.displayName = entry.displayName;
        app.summary = entry.summary;
        app.description = entry.description;
        app.iconName = entry.iconName;
        app.backend = QStringLiteral("flatpak");
        app.packageId = entry.packageId;
        app.repository = entry.repository;
        app.verified = entry.verified;
        app.sources.append(sourceRecord(QStringLiteral("flatpak"), entry.packageId, entry.repository));
        const int newIndex = merged.size();
        merged.append(std::move(app));
        byId.insert(componentKey, newIndex);
        byId.insert(packageKey, newIndex);
    }
    std::sort(merged.begin(), merged.end(), [](const AppRecord& a, const AppRecord& b) {
        return QString::localeAwareCompare(a.displayName, b.displayName) < 0;
    });
    beginResetModel();
    apps_ = std::move(merged);
    endResetModel();
    emit sourceListChanged();
    qInfo() << "Plaza catalog contains" << apps_.size() << "applications after Flatpak metadata import";
}

bool AppListModel::load(const QString& searchText)
{
    // Keep the initial search value non-null so the empty-search predicate
    // works consistently with SQLite and PostgreSQL.
    const QString normalizedSearchText = searchText.isNull()
        ? QStringLiteral("")
        : searchText;

    if (!database_.isValid()) {
        const QString message =
            QStringLiteral(
                "The Plaza app catalog database handle is invalid."
            );

        qCritical().noquote() << message;
        emit loadFailed(message);

        return false;
    }

    if (!database_.isOpen()) {
        const QString message =
            QStringLiteral(
                "The Plaza app catalog is not open: %1"
            ).arg(database_.lastError().text());

        qCritical().noquote() << message;
        emit loadFailed(message);

        return false;
    }

    qInfo()
        << "Loading catalog through Qt connection"
        << database_.connectionName()
        << "database"
        << database_.databaseName();

    // Use the exact catalog connection rather than Qt's default connection.
    QSqlQuery query(database_);

    const QString statement = QStringLiteral(R"SQL(
        SELECT
            a.id,
            a.component_id,
            a.name,
            a.summary,
            a.description,
            a.icon_name,
            a.is_verified
        FROM apps AS a
        WHERE a.is_published = TRUE
          AND (
              :search_empty = ''
              OR LOWER(a.name) LIKE '%' || LOWER(:search_name) || '%'
              OR LOWER(a.summary) LIKE '%' || LOWER(:search_summary) || '%'
              OR LOWER(a.component_id) LIKE '%' || LOWER(:search_component) || '%'
          )
        ORDER BY a.name ASC
    )SQL");

    if (!query.prepare(statement)) {
        const QString message =
            QStringLiteral(
                "Could not prepare the application catalog query: %1"
            ).arg(query.lastError().text());

        qCritical().noquote() << message;
        emit loadFailed(message);

        return false;
    }

    query.bindValue(
        QStringLiteral(":search_empty"),
        normalizedSearchText
    );

    query.bindValue(
        QStringLiteral(":search_name"),
        normalizedSearchText
    );

    query.bindValue(
        QStringLiteral(":search_summary"),
        normalizedSearchText
    );

    query.bindValue(
        QStringLiteral(":search_component"),
        normalizedSearchText
    );

    if (!query.exec()) {
        const QString message =
            QStringLiteral(
                "Could not load Plaza applications: %1"
            ).arg(query.lastError().text());

        qCritical().noquote() << message;
        emit loadFailed(message);

        return false;
    }

    QVector<AppRecord> loadedApps;

    while (query.next()) {
        AppRecord app;

        app.appId = query.value(0).toLongLong();
        app.componentId = query.value(1).toString();
        app.displayName = query.value(2).toString();
        app.summary = query.value(3).toString();
        app.description = query.value(4).toString();
        app.iconName = query.value(5).toString();
        app.verified = query.value(6).toBool();
        loadedApps.append(std::move(app));
    }

    QHash<qint64, int> appIndexes;
    for (int i = 0; i < loadedApps.size(); ++i) appIndexes.insert(loadedApps[i].appId, i);
    QSqlQuery sourcesQuery(database_);
    if (!sourcesQuery.exec(QStringLiteral("SELECT app_id, backend, package_id, COALESCE(repository, ''), COALESCE(architecture, '') FROM app_sources WHERE is_available = TRUE ORDER BY priority, id"))) {
        qWarning() << "Could not load application sources:" << sourcesQuery.lastError().text();
        emit loadFailed(QStringLiteral("Could not load application sources."));
        return false;
    }
    while (sourcesQuery.next()) {
        const int index = appIndexes.value(sourcesQuery.value(0).toLongLong(), -1);
        const QString backend = sourcesQuery.value(1).toString();
        if (index < 0 || !compatibleSource(backend, sourcesQuery.value(4).toString())) continue;
        AppRecord& app = loadedApps[index];
        const QString packageId = sourcesQuery.value(2).toString();
        const QString repository = sourcesQuery.value(3).toString();
        appendSource(app.sources, sourceRecord(backend, packageId, repository));
        if (app.backend.isEmpty()) {
            app.backend = backend;
            app.packageId = packageId;
            app.repository = repository;
        }
    }

    beginResetModel();
    apps_ = std::move(loadedApps);
    endResetModel();
    emit sourceListChanged();

    qInfo()
        << "Loaded"
        << apps_.size()
        << "published Plaza applications";

    return true;
}
