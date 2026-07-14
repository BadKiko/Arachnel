#include "setup_translation.h"

#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QTranslator>

namespace arachnel::setup {

SetupTranslation::SetupTranslation(QObject* parent)
    : QObject(parent)
    , m_translator(std::make_unique<QTranslator>())
{
}

void SetupTranslation::setEngine(QQmlApplicationEngine* engine)
{
    m_engine = engine;
}

void SetupTranslation::applyLanguage(const QString& languageCode)
{
    const QString effective = languageCode.trimmed().toLower().isEmpty()
                                  ? QStringLiteral("en")
                                  : languageCode.trimmed().toLower();

    QCoreApplication::removeTranslator(m_translator.get());

    if (effective != QStringLiteral("en")) {
        const QString qmPath =
            QStringLiteral(":/i18n/arachnel_setup_%1.qm").arg(effective);
        if (!m_translator->load(qmPath))
            return;
        QCoreApplication::installTranslator(m_translator.get());
    }

    if (m_engine)
        m_engine->retranslate();
}

} // namespace arachnel::setup
