#include "job_status.h"

namespace arachnel::core {

QString jobStatusLabel(const QString& status)
{
    if (status == QStringLiteral("queued"))
        return QStringLiteral("В очереди");
    if (status == QStringLiteral("starting"))
        return QStringLiteral("Запуск");
    if (status == QStringLiteral("checking"))
        return QStringLiteral("Проверка");
    if (status == QStringLiteral("metadata"))
        return QStringLiteral("Метаданные");
    if (status == QStringLiteral("downloading"))
        return QStringLiteral("Загрузка");
    if (status == QStringLiteral("seeding"))
        return QStringLiteral("Раздача");
    if (status == QStringLiteral("paused"))
        return QStringLiteral("Пауза");
    if (status == QStringLiteral("completed"))
        return QStringLiteral("Завершено");
    if (status == QStringLiteral("failed"))
        return QStringLiteral("Ошибка");
    if (status == QStringLiteral("cancelled"))
        return QStringLiteral("Отменено");
    return status;
}

} // namespace arachnel::core
