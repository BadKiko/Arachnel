#include "translation_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QLocale>
#include <QVariantMap>
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

    // English is the source language, but its catalog still carries the plural forms:
    // without it "%n game(s)" is shown as "4 game(s)".
    const QString resourceName = QStringLiteral("arachnel_%1").arg(effective);
    if (m_translator->load(resourceName, QStringLiteral(":/i18n")))
        QCoreApplication::installTranslator(m_translator);

    m_currentLanguage = effective;

    if (m_engine)
        m_engine->retranslate();
}

QVariantList TranslationService::availableLanguages() const
{
    QStringList codes;
    const QStringList files = QDir(QStringLiteral(":/i18n")).entryList({QStringLiteral("arachnel_*.qm")});
    for (const QString& file : files) {
        const QString code = file.mid(9, file.size() - 9 - 3).toLower(); // arachnel_<code>.qm
        if (!code.isEmpty() && !codes.contains(code))
            codes.append(code);
    }
    if (!codes.contains(QLatin1String("en")))
        codes.append(QStringLiteral("en"));
    codes.sort();
    codes.removeAll(QStringLiteral("en"));
    codes.prepend(QStringLiteral("en"));

    QVariantList out;
    for (const QString& code : codes) {
        const QLocale locale(code);
        // QLocale("en") is "American English"; the interface is just English.
        QString native = code == QLatin1String("en") ? QStringLiteral("English") : locale.nativeLanguageName();
        if (native.isEmpty())
            native = code;
        native[0] = native.at(0).toUpper();
        QVariantMap row;
        row.insert(QStringLiteral("code"), code);
        row.insert(QStringLiteral("name"), native);
        row.insert(QStringLiteral("englishName"), QLocale::languageToString(locale.language()));
        out.append(row);
    }
    return out;
}

} // namespace arachnel::core
