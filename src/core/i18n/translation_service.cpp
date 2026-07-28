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
    , m_translator(new QTranslator(this))
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

    QCoreApplication::removeTranslator(m_translator);

    // English is the source language — qsTr()/translate() fall back to the string in code.
    if (effective != QStringLiteral("en")) {
        const QString resourceName = QStringLiteral("arachnel_%1").arg(effective);
        if (m_translator->load(resourceName, QStringLiteral(":/i18n")))
            QCoreApplication::installTranslator(m_translator);
    }

    m_currentLanguage = effective;

    if (m_engine)
        m_engine->retranslate();
}

} // namespace arachnel::core
