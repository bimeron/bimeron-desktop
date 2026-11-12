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
    // Why a catalog inspection produced no groups. Consumers present these categories;
    // they never parse the diagnostic text.
    enum class CatalogFailure { None, Missing, Unreadable, Unsupported, ValidatorFailed, Rejected };
    Q_ENUM(CatalogFailure)
    // Raw facts from one `geodata groups` run, classified by classifyCatalog().
    struct CatalogRun {
        bool started = false, timedOut = false, overLimit = false, crashed = false;
        int exitCode = 0;
        QByteArray output, errors;
    };
    struct CatalogOutcome {
        QStringList groups;
        QString diagnostic;
        CatalogFailure failure = CatalogFailure::None;
    };
    // Short row status and one-sentence explanation shared by every consumer.
    static QString failureStatus(CatalogFailure failure);
    static QString failureExplanation(CatalogFailure failure);
    static CatalogOutcome classifyCatalog(const QString &core, const QString &kind, const QString &path,
                                          const CatalogRun &run);
    // Local existence/readability check performed before launching a validator.
    static CatalogOutcome precheckCatalog(const QString &core, const QString &kind, const QString &path);
    static GeoDataSelection *instance();
    ~GeoDataSelection() override;
    Paths paths() const;
    bool apply(const Paths &paths, const QString &core, const QList<Profile> &profiles,
               QString *error = nullptr);
    // Never writes profilePath. Empty return plus nonempty error means failure.
    static QString materialize(const QString &text, const QString &profilePath,
                               const QString &core, const Paths &paths, QString *error = nullptr,
                               const QString &validationDirectory = {});
    // path must be absolute; relative selections are resolved per profile by materialize.
    static QStringList inspect(const QString &core, const QString &kind, const QString &path,
                               QString *error = nullptr, CatalogFailure *failure = nullptr);
    static QString catalogKey(const QString &core, const QString &kind, const QString &path);
    void requestCatalog(const QString &core, const QString &kind, const QString &path, bool force = false);
    bool catalogsBusy() const;
    bool cachedCatalog(const QString &core, const QString &kind, const QString &path,
                       QStringList *groups, QString *error, CatalogFailure *failure = nullptr) const;
    // Only for unique attempt-owned staging paths, never a shared selected-file inspection.
    void cancelCatalog(const QString &key);
signals:
    void changed();
    void catalogResult(QString key, QStringList groups, QString error, GeoDataSelection::CatalogFailure failure);
    void catalogInvalidated(QString key);
private:
    explicit GeoDataSelection(QObject *parent);
    Paths paths_;
    struct Catalog {
        QString identity, validator, error;
        CatalogFailure failure = CatalogFailure::None;
        quint64 generation = 0;
        QStringList groups;
        QProcess *process = nullptr;
        QFileSystemWatcher *watcher = nullptr;
    };
    QMap<QString, Catalog> catalogs_;
};
