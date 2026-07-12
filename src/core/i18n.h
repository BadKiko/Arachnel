#pragma once

#include <QCoreApplication>
#include <QString>

namespace arachnel::core {

inline QString trCtx(const char* context, const char* text)
{
    return QCoreApplication::translate(context, text);
}

inline QString trCore(const char* text)
{
    return trCtx("Core", text);
}

} // namespace arachnel::core
