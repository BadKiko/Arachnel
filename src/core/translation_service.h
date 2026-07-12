#pragma once

#include <QObject>
#include <QString>

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

private:
    explicit TranslationService(QObject* parent = nullptr);

    QQmlApplicationEngine* m_engine = nullptr;
    QTranslator* m_appTranslator = nullptr;
    QString m_currentLanguage = QStringLiteral("en");
};

} // namespace arachnel::core
