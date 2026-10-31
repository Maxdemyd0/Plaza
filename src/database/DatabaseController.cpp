#include "DatabaseController.h"

#include <QDebug>
#include <QProcessEnvironment>
#include <QSqlError>
#include <QStringList>

DatabaseController::~DatabaseController()
{
    if (database_.isValid() && database_.isOpen()) {
        database_.close();
    }
}

bool DatabaseController::open()
{
    if (database_.isValid() && database_.isOpen()) {
        return true;
    }

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QPSQL"))) {
        lastError_ = QStringLiteral(
            "The QPSQL driver is unavailable. Installed Qt SQL drivers: %1"
        ).arg(QSqlDatabase::drivers().join(QStringLiteral(", ")));

        qCritical().noquote() << lastError_;
        return false;
    }

    database_ = QSqlDatabase::addDatabase(
        QStringLiteral("QPSQL"),
        QString::fromLatin1(ConnectionName)
    );

    const QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();

    database_.setDatabaseName(
        environment.value(
            QStringLiteral("PLAZA_DB_NAME"),
            QStringLiteral("plaza")
        )
    );

    database_.setUserName(
        environment.value(
            QStringLiteral("PLAZA_DB_USER"),
            environment.value(QStringLiteral("USER"))
        )
    );

    /*
     * When PLAZA_DB_HOST is unset, QPSQL uses PostgreSQL's local Unix
     * socket. On this development machine, that uses peer authentication.
     *
     * Set PLAZA_DB_HOST=127.0.0.1 when password-authenticated TCP is
     * specifically desired.
     */
    const QString host =
        environment.value(QStringLiteral("PLAZA_DB_HOST"));

    if (!host.isEmpty()) {
        database_.setHostName(host);
    }

    bool portIsValid = false;

    const int port =
        environment.value(
            QStringLiteral("PLAZA_DB_PORT"),
            QStringLiteral("5432")
        ).toInt(&portIsValid);

    if (portIsValid) {
        database_.setPort(port);
    }

    const QString password =
        environment.value(QStringLiteral("PLAZA_DB_PASSWORD"));

    if (!password.isEmpty()) {
        database_.setPassword(password);
    }

    database_.setConnectOptions(
        QStringLiteral(
            "connect_timeout=5;"
            "application_name=Plaza"
        )
    );

    if (!database_.open()) {
        lastError_ = database_.lastError().text();

        qCritical().noquote()
            << "Could not connect to the Plaza database:"
            << lastError_;

        return false;
    }

    lastError_.clear();

    qInfo()
        << "Connected to PostgreSQL database"
        << database_.databaseName()
        << "using Qt connection"
        << database_.connectionName();

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
