#pragma once

#include <QObject>
#include <QMap>
#include <QString>

class Translator : public QObject
{
    Q_OBJECT

public:
    static Translator& instance();
    bool loadLanguage(const QString& langFile);
    QString translate(const QString& context, const QString& sourceText) const;

private:
    explicit Translator(QObject* parent = nullptr) : QObject(parent) {}
    QMap<QString, QString> translations; // key: "Context|Source"
};