#include "translation_service.h"

#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QTranslator>

namespace arachnel::core {

TranslationService& TranslationService::instance()
{
    static TranslationService service;
    return service;
}

TranslationService::TranslationService(QObject* parent)
    : QObject(parent)
    , m_appTranslator(new QTranslator(this))
{
}

void TranslationService::setEngine(QQmlApplicationEngine* engine)
{
    m_engine = engine;
}

void TranslationService::applyLanguage(const QString& languageCode)
{
    const QString normalized = languageCode.trimmed().toLower();
    const QString effective = normalized.isEmpty() ? QStringLiteral("en") : normalized;

    if (m_currentLanguage == effective && effective == QStringLiteral("en"))
        return;

    QCoreApplication::removeTranslator(m_appTranslator);

    if (effective != QStringLiteral("en")) {
        const QString resourceName = QStringLiteral("arachnel_%1").arg(effective);
        if (m_appTranslator->load(resourceName, QStringLiteral(":/i18n")))
            QCoreApplication::installTranslator(m_appTranslator);
    }

    m_currentLanguage = effective;

    if (m_engine)
        m_engine->retranslate();
}

} // namespace arachnel::core
