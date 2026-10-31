#include "DatabaseController.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>

#include <utility>

DatabaseController::DatabaseController(QString path)
    : databasePath_(std::move(path))
{
}

DatabaseController::~DatabaseController()
{
    if (!database_.isValid()) return;

    const QString connectionName = database_.connectionName();
    database_.close();
    database_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

bool DatabaseController::open()
{
    if (database_.isValid() && database_.isOpen()) return true;

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        lastError_ = QStringLiteral(
            "The Qt SQLite driver is unavailable. Install the Qt SQLite SQL driver package."
        );
        qCritical().noquote() << lastError_;
        return false;
    }

    if (databasePath_.isEmpty()) {
        databasePath_ = QDir(
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        ).filePath(QStringLiteral("catalog.sqlite"));
    }

    const QFileInfo databaseFile(databasePath_);
    if (!QDir().mkpath(databaseFile.absolutePath())) {
        lastError_ = QStringLiteral("Could not create Plaza's application data directory.");
        qCritical().noquote() << lastError_ << databaseFile.absolutePath();
        return false;
    }

    database_ = QSqlDatabase::addDatabase(
        QStringLiteral("QSQLITE"),
        QString::fromLatin1(ConnectionName)
    );
    database_.setDatabaseName(databasePath_);
    database_.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));

    if (!database_.open()) {
        lastError_ = database_.lastError().text();
        qCritical().noquote() << "Could not open Plaza's local app catalog:" << lastError_;
        return false;
    }

    QFile bundledCatalog(QStringLiteral(":/db/catalog.sql"));
    if (!bundledCatalog.open(QIODevice::ReadOnly | QIODevice::Text)) {
        lastError_ = QStringLiteral("The bundled Plaza app catalog could not be read.");
        qCritical().noquote() << lastError_ << bundledCatalog.errorString();
        database_.close();
        return false;
    }

    const QStringList statements = QString::fromUtf8(bundledCatalog.readAll())
        .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString& statement : statements) {
        const QString sql = statement.trimmed();
        if (sql.isEmpty()) continue;

        QSqlQuery query(database_);
        if (!query.exec(sql)) {
            lastError_ = query.lastError().text();
            qCritical().noquote() << "Could not initialize Plaza's local app catalog:" << lastError_;
            database_.close();
            return false;
        }
    }

    lastError_.clear();
    qInfo().noquote() << "Plaza catalog ready at" << databasePath_;
    return true;
}

bool DatabaseController::isOpen() const
{
    return database_.isValid() && database_.isOpen();
}

QString DatabaseController::lastError() const
{
    return lastError_;
}

QSqlDatabase DatabaseController::database() const
{
    return database_;
}

QString DatabaseController::databasePath() const
{
    return databasePath_;
}
