#pragma once
#include <QJsonArray>
#include <QString>
class QWidget;
class GeoDataDownload;

void manageSharedGeoData(QWidget *parent, const QString &core, const QString &profilePath,
                         const QJsonArray &rules, GeoDataDownload *transport = nullptr);
