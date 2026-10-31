#pragma once

#include <QSqlDatabase>
#include <QString>

class DatabaseController
{
public:
    DatabaseController() = default;
    ~DatabaseController();

    DatabaseController(const DatabaseController&) = delete;
    DatabaseController& operator=(const DatabaseController&) = delete;

    [[nodiscard]] bool open();
    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] QString lastError() const;

    // QSqlDatabase is a lightweight handle to the existing connection.
    [[nodiscard]] QSqlDatabase database() const;

private:
    static constexpr auto ConnectionName = "plaza-catalog";

    QSqlDatabase database_;
    QString lastError_;
};
