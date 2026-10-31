#pragma once

#include <QSqlDatabase>
#include <QString>

class DatabaseController
{
public:
    explicit DatabaseController(QString path = {});
    ~DatabaseController();

    DatabaseController(const DatabaseController&) = delete;
    DatabaseController& operator=(const DatabaseController&) = delete;

    [[nodiscard]] bool open();
    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] QSqlDatabase database() const;
    [[nodiscard]] QString databasePath() const;

private:
    static constexpr auto ConnectionName = "plaza-catalog";

    QString databasePath_;
    QSqlDatabase database_;
    QString lastError_;
};
