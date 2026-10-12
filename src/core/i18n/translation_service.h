#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class QQmlApplicationEngine;
class QTranslator;

namespace arachnel::core {

class TranslationService : public QObject
{
    Q_OBJECT

public:
    static TranslationService& instance();

    void setEngine(QQmlApplicationEngine* engine);
    void applyLanguage(const QString& languageCode);

    QString currentLanguage() const { return m_currentLanguage; }

    /**
     * Languages that have a compiled catalog (translations/arachnel_<code>.ts): code, native
     * name, English name. English first, then by code. Discovered at run time, so a language
     * added through Weblate shows up in the pickers without touching the UI.
     */
    QVariantList availableLanguages() const;

private:
    explicit TranslationService(QObject* parent = nullptr);

    QQmlApplicationEngine* m_engine = nullptr;
    QTranslator* m_translator = nullptr;
    QString m_currentLanguage = QStringLiteral("en");
};

} // namespace arachnel::core
