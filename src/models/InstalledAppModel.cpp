#include "InstalledAppModel.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QVariantMap>

#include <algorithm>

namespace {

QStringList applicationDirectories()
{
    QStringList directories = QStandardPaths::standardLocations(
        QStandardPaths::ApplicationsLocation
    );

    const QString userFlatpakApplications =
        QStandardPaths::writableLocation(
            QStandardPaths::GenericDataLocation
        ) + QStringLiteral("/flatpak/exports/share/applications");

    directories.append(userFlatpakApplications);
    directories.append(
        QStringLiteral("/var/lib/flatpak/exports/share/applications")
    );

    directories.removeDuplicates();
    return directories;
}

} // namespace

InstalledAppModel::InstalledAppModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int InstalledAppModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(apps_.size());
}

QVariant InstalledAppModel::data(
    const QModelIndex& index,
    const int role
) const
{
    if (!index.isValid() || index.row() < 0 ||
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
        return QStringLiteral("local");
    case PackageIdRole:
        return app.packageId;
    case RepositoryRole:
        return app.source;
    case VerifiedRole:
        return app.verified;
    case SourcesRole:
        return QVariantList{};
    default:
        return {};
    }
}

QHash<int, QByteArray> InstalledAppModel::roleNames() const
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
        { SourcesRole, "sources" }
    };
}

int InstalledAppModel::version() const
{
    return version_;
}

void InstalledAppModel::setStoreApps(const QVariantList& storeApps)
{
    storeById_.clear();
    storeByName_.clear();
    for (const QVariant& value : storeApps) {
        const QVariantMap app = value.toMap();
        for (const QString& field : {QStringLiteral("componentId"), QStringLiteral("packageId")}) {
            const QString key = matchKey(app.value(field).toString());
            if (!key.isEmpty() && !storeById_.contains(key)) storeById_.insert(key, app);
        }
        const QString name = matchKey(app.value(QStringLiteral("displayName")).toString());
        if (!name.isEmpty() && !storeByName_.contains(name)) storeByName_.insert(name, app);
    }
}

bool InstalledAppModel::reload()
{
    QVector<AppRecord> loadedApps;
    QSet<QString> loadedIds;
    QSet<QString> loadedNames;
    QSet<QString> seenDesktopFiles;
    QSet<QString> seenNames;

    for (const QString& directory : applicationDirectories()) {
        if (!QDir(directory).exists()) {
            continue;
        }

        QDirIterator desktopFiles(
            directory,
            { QStringLiteral("*.desktop") },
            QDir::Files,
            QDirIterator::Subdirectories
        );

        while (desktopFiles.hasNext()) {
            const QString filePath = desktopFiles.next();
            const QFileInfo fileInfo(filePath);
            const QString desktopId = fileInfo.completeBaseName();

            if (desktopId.isEmpty() || seenDesktopFiles.contains(desktopId)) {
                continue;
            }

            QSettings desktopEntry(filePath, QSettings::IniFormat);
            desktopEntry.beginGroup(QStringLiteral("Desktop Entry"));

            const QString type = desktopEntry.value(
                QStringLiteral("Type")
            ).toString();
            const bool hidden = desktopEntry.value(
                QStringLiteral("Hidden"), false
            ).toBool();
            const bool noDisplay = desktopEntry.value(
                QStringLiteral("NoDisplay"), false
            ).toBool();
            const QString displayName = desktopEntry.value(
                QStringLiteral("Name")
            ).toString().trimmed();

            if (type != QStringLiteral("Application") || hidden ||
                noDisplay || displayName.isEmpty()) {
                desktopEntry.endGroup();
                continue;
            }

            const QString flatpakId = desktopEntry.value(
                QStringLiteral("X-Flatpak")
            ).toString().trimmed();
            const QString nameKey = matchKey(displayName);

            desktopEntry.endGroup();

            seenDesktopFiles.insert(desktopId);
            if (seenNames.contains(nameKey)) {
                continue;
            }

            AppRecord app;
            app.appId = static_cast<qint64>(loadedApps.size() + 1);
            app.componentId = flatpakId.isEmpty() ? desktopId : flatpakId;
            app.displayName = displayName;
            app.summary = desktopEntry.value(
                QStringLiteral("Desktop Entry/Comment")
            ).toString().trimmed();
            app.description = app.summary;
            app.iconName = desktopEntry.value(
                QStringLiteral("Desktop Entry/Icon")
            ).toString().trimmed();
            app.packageId = desktopId;
            app.source = flatpakId.isEmpty()
                ? QStringLiteral("System desktop entry")
                : QStringLiteral("Flatpak desktop entry");

            applyStoreMetadata(app);

            loadedIds.insert(matchKey(app.componentId));
            loadedIds.insert(matchKey(desktopId));
            loadedNames.insert(nameKey);
            seenNames.insert(nameKey);
            loadedApps.append(std::move(app));
        }
    }

    std::sort(
        loadedApps.begin(),
        loadedApps.end(),
        [](const AppRecord& left, const AppRecord& right) {
            return left.displayName.localeAwareCompare(
                right.displayName
            ) < 0;
        }
    );

    beginResetModel();
    apps_ = std::move(loadedApps);
    installedIds_ = std::move(loadedIds);
    installedNames_ = std::move(loadedNames);
    endResetModel();

    ++version_;
    emit versionChanged();

    return true;
}

bool InstalledAppModel::isInstalled(
    const QString& componentId,
    const QString& packageId,
    const QString& displayName
) const
{
    return installedIds_.contains(matchKey(componentId)) ||
        installedIds_.contains(matchKey(packageId)) ||
        installedNames_.contains(matchKey(displayName));
}

void InstalledAppModel::applyStoreMetadata(AppRecord& app) const
{
    const QString componentKey = matchKey(app.componentId);
    const QString packageKey = matchKey(app.packageId);
    const QString nameKey = matchKey(app.displayName);

    QVariantMap storeApp = storeById_.value(componentKey);
    if (storeApp.isEmpty()) storeApp = storeById_.value(packageKey);
    if (storeApp.isEmpty()) storeApp = storeByName_.value(nameKey);
    if (!storeApp.isEmpty()) {
        const QString storeComponentId = storeApp.value(
            QStringLiteral("componentId")
        ).toString();
        const QString storeDisplayName = storeApp.value(
            QStringLiteral("displayName")
        ).toString();

        app.componentId = storeComponentId.isEmpty()
            ? app.componentId
            : storeComponentId;
        app.displayName = storeDisplayName.isEmpty()
            ? app.displayName
            : storeDisplayName;

        const QString storeSummary = storeApp.value(
            QStringLiteral("summary")
        ).toString();
        const QString storeDescription = storeApp.value(
            QStringLiteral("description")
        ).toString();
        const QString storeIconName = storeApp.value(
            QStringLiteral("iconName")
        ).toString();

        if (!storeSummary.isEmpty()) {
            app.summary = storeSummary;
        }
        if (!storeDescription.isEmpty()) {
            app.description = storeDescription;
        }
        if (!storeIconName.isEmpty()) {
            app.iconName = storeIconName;
        }
        app.verified = storeApp.value(QStringLiteral("verified")).toBool();

    }
}

QString InstalledAppModel::matchKey(const QString& value)
{
    QString key;

    for (const QChar character : value.toCaseFolded()) {
        if (character.isLetterOrNumber()) {
            key.append(character);
        }
    }

    return key;
}
