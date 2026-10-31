#include <cstdlib>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "src/database/DatabaseController.h"
#include "src/backends/FlatpakBackend.h"
#include "src/backends/FlatpakCatalog.h"
#include "src/backends/GitHubUpdateController.h"
#include "src/backends/PopularityProvider.h"
#include "src/backends/PackageKitBackend.h"
#include "src/models/AppListModel.h"
#include "src/models/AppSearchModel.h"
#include "src/models/InstalledAppModel.h"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/plaza.svg")));

    QCoreApplication::setOrganizationName(
        QStringLiteral("Maxdemyd0")
    );

    QCoreApplication::setOrganizationDomain(
        QStringLiteral("maxdemyd0.dev")
    );

    QCoreApplication::setApplicationName(
        QStringLiteral("Plaza")
    );
    QCoreApplication::setApplicationVersion(QStringLiteral(PLAZA_VERSION));

    if (qEnvironmentVariableIsEmpty(
            "QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(
            QStringLiteral("org.kde.desktop")
        );
    }

    DatabaseController database;

    const bool databaseReady = database.open();

    // The bundled local SQLite catalog works without a PostgreSQL service.
    AppListModel appCatalog(database.database());

    if (databaseReady) {
        appCatalog.reload();
    }

    FlatpakBackend flatpakBackend;
    PackageKitBackend packageBackend;
    GitHubUpdateController plazaUpdates(QCoreApplication::applicationVersion(), &packageBackend);
    InstalledAppModel installedApps;
    installedApps.setStoreApps(appCatalog.storeApps());
    installedApps.reload();

    AppSearchModel catalogSearch;
    catalogSearch.setSourceModel(&appCatalog);
    catalogSearch.enablePopularitySorting();
    AppSearchModel installedSearch;
    installedSearch.setSourceModel(&installedApps);
    AppPageModel catalogPage;
    catalogPage.setSourceModel(&catalogSearch);
    AppPageModel installedPage;
    installedPage.setSourceModel(&installedSearch);
    QObject::connect(&catalogSearch, &AppSearchModel::searchTextChanged, &catalogPage, &AppPageModel::resetLimit);
    QObject::connect(&installedSearch, &AppSearchModel::searchTextChanged, &installedPage, &AppPageModel::resetLimit);

    FlatpakCatalog flatpakCatalog;
    QObject::connect(&flatpakCatalog, &FlatpakCatalog::loaded, &appCatalog, &AppListModel::mergeFlatpakApps);
    QObject::connect(&flatpakBackend, &FlatpakBackend::sourcesChanged, &flatpakCatalog, &FlatpakCatalog::load);
    flatpakCatalog.load();
    PopularityProvider popularity;
    QObject::connect(&popularity, &PopularityProvider::loaded, &appCatalog, &AppListModel::setPopularity);
    popularity.load();

    QObject::connect(
        &appCatalog,
        &QAbstractItemModel::modelReset,
        &installedApps,
        [&appCatalog, &installedApps] {
            installedApps.setStoreApps(appCatalog.storeApps());
            installedApps.reload();
        }
    );

    QObject::connect(
        &flatpakBackend,
        &FlatpakBackend::installed,
        &installedApps,
        [&installedApps] {
            installedApps.reload();
        }
    );
    QObject::connect(&packageBackend, &PackageKitBackend::installed, &installedApps,
                     [&installedApps] { installedApps.reload(); });
    QObject::connect(&flatpakBackend, &FlatpakBackend::updatesInstalled, &installedApps,
                     [&installedApps] { installedApps.reload(); });
    QObject::connect(&packageBackend, &PackageKitBackend::updatesInstalled, &installedApps,
                     [&installedApps] { installedApps.reload(); });

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty(
        QStringLiteral("appCatalog"),
        &appCatalog
    );

    engine.rootContext()->setContextProperty(
        QStringLiteral("flatpakBackend"),
        &flatpakBackend
    );

    engine.rootContext()->setContextProperty(
        QStringLiteral("installedApps"),
        &installedApps
    );

    engine.rootContext()->setContextProperty(QStringLiteral("catalogSearch"), &catalogSearch);
    engine.rootContext()->setContextProperty(QStringLiteral("installedSearch"), &installedSearch);
    engine.rootContext()->setContextProperty(QStringLiteral("catalogPage"), &catalogPage);
    engine.rootContext()->setContextProperty(QStringLiteral("installedPage"), &installedPage);
    engine.rootContext()->setContextProperty(QStringLiteral("flatpakCatalog"), &flatpakCatalog);
    engine.rootContext()->setContextProperty(QStringLiteral("packageBackend"), &packageBackend);
    engine.rootContext()->setContextProperty(QStringLiteral("plazaUpdates"), &plazaUpdates);
    engine.rootContext()->setContextProperty(QStringLiteral("plazaVersion"), QCoreApplication::applicationVersion());

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [] {
            QCoreApplication::exit(EXIT_FAILURE);
        },
        Qt::QueuedConnection
    );

    engine.loadFromModule(
        QStringLiteral("Plaza"),
        QStringLiteral("Main")
    );

    plazaUpdates.checkForUpdates();

    return app.exec();
}
