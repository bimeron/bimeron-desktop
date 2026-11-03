#pragma once

#include <QLocale>
#include <QMap>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>

inline QString geoIPSpecialName(const QString &id) {
    static const QMap<QString, QString> names{
        {"cloudflare", "Cloudflare"}, {"cloudfront", "Amazon CloudFront"},
        {"facebook", "Facebook"}, {"fastly", "Fastly"}, {"google", "Google"},
        {"netflix", "Netflix"}, {"private", "Private and special networks"},
        {"telegram", "Telegram"}, {"tor", "Tor networks"}, {"twitter", "Twitter"}};
    return names.value(id.toLower());
}

inline QString geoIPLabel(const QString &id, bool localized = false) {
    const QString code = id.toUpper();
    QString name = geoIPSpecialName(id);
    if (name.isEmpty() && code.size() == 2) {
        const auto territory = QLocale::codeToTerritory(code);
        if (territory != QLocale::AnyTerritory) name = QLocale::territoryToString(territory);
    }
    if (localized && !name.isEmpty()) name = QObject::tr(name.toUtf8().constData());
    return name.isEmpty() ? id : name + " (" + code + ')';
}

inline QString geoIPSearchText(const QString &id) {
    static const QJsonObject russian = [] {
        QFile file(":/translations/ru.json");
        return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject{};
    }();
    const QString english = geoIPLabel(id);
    const QString suffix = " (" + id.toUpper() + ')';
    const QString name = english.endsWith(suffix) ? english.chopped(suffix.size()) : english;
    return english + '\n' + russian.value(name).toString() + '\n' + id;
}
