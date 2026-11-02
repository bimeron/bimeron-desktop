#pragma once

#include "ProfileStore.h"
#include <QMap>
#include <QObject>
#include <QStringList>

class QProcess;
class QFileSystemWatcher;

class GeoDataSelection : public QObject {
    Q_OBJECT
public:
    using Paths = QMap<QString, QString>;
    static GeoDataSelection *instance();
    ~GeoDataSelection() override;
    Paths paths() const;
    bool apply(const Paths &paths, const QString &core, const QList<Profile> &profiles,
               QString *error = nullptr);
    // Never writes profilePath. Empty return plus nonempty error means failure.
    static QString materialize(const QString &text, const QString &profilePath,
                               const QString &core, const Paths &paths, QString *error = nullptr);
    // path must be absolute; relative selections are resolved per profile by materialize.
    static QStringList inspect(const QString &core, const QString &kind, const QString &path,
                               QString *error = nullptr);
    static QString catalogKey(const QString &core, const QString &kind, const QString &path);
    void requestCatalog(const QString &core, const QString &kind, const QString &path, bool force = false);
    bool catalogsBusy() const;
    bool cachedCatalog(const QString &core, const QString &kind, const QString &path,
                       QStringList *groups, QString *error) const;
    // Only for unique attempt-owned staging paths, never a shared selected-file inspection.
    void cancelCatalog(const QString &key);
signals:
    void changed();
    void catalogResult(QString key, QStringList groups, QString error);
    void catalogInvalidated(QString key);
private:
    explicit GeoDataSelection(QObject *parent);
    Paths paths_;
    struct Catalog {
        QString identity, error;
        quint64 generation = 0;
        QStringList groups;
        QProcess *process = nullptr;
        QFileSystemWatcher *watcher = nullptr;
    };
    QMap<QString, Catalog> catalogs_;
};
