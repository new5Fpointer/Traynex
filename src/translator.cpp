#include "translator.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

Translator& Translator::instance()
{
    static Translator inst;
    return inst;
}

bool Translator::loadLanguage(const QString& langFile)
{
    QFile file(langFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open language file:" << langFile;
        return false;
    }

    translations.clear();
    QTextStream in(&file);
    QString currentSection;
    QString line;

    while (!in.atEnd()) {
        line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) continue;

        if (line.startsWith('[') && line.endsWith(']')) {
            currentSection = line.mid(1, line.length() - 2);
        }
        else if (line.contains('=')) {
            int eq = line.indexOf('=');
            QString key = line.left(eq).trimmed();
            QString value = line.mid(eq + 1).trimmed();

            // 支持带引号的值（可选）
            if (value.length() >= 2 && value.startsWith('"') && value.endsWith('"'))
                value = value.mid(1, value.length() - 2);

            QString fullKey = currentSection + '|' + key;
            translations[fullKey] = value;
        }
    }

    file.close();
    qDebug() << "Loaded" << translations.size() << "translations from" << langFile;
    return true;
}

QString Translator::translate(const QString& context, const QString& sourceText) const
{
    QString key = context + '|' + sourceText;
    auto it = translations.find(key);
    if (it != translations.end()) {
        return *it;
    }
    return sourceText; // fallback
}