#include "job_kind.h"

namespace arachnel::core {

QString jobKindLabel(JobKind kind)
{
    switch (kind) {
    case JobKind::Download:
        return QStringLiteral("Загрузка");
    case JobKind::Install:
        return QStringLiteral("Установка");
    case JobKind::Update:
        return QStringLiteral("Обновление");
    }
    return QStringLiteral("Задача");
}

} // namespace arachnel::core
