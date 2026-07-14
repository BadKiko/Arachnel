#pragma once

#include <QObject>
#include <QString>
#include <QTranslator>
#include <QVariantList>
#include <memory>

class QQmlApplicationEngine;

namespace arachnel::setup {

class SetupTranslation : public QObject
{
    Q_OBJECT

public:
    explicit SetupTranslation(QObject* parent = nullptr);

    void setEngine(QQmlApplicationEngine* engine);
    void applyLanguage(const QString& languageCode);

private:
    QQmlApplicationEngine* m_engine = nullptr;
    std::unique_ptr<QTranslator> m_translator;
};

} // namespace arachnel::setup
