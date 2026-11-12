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
    // Where each kind's database comes from. Loyalsoldier is the shipped third-party
    // default and the only source that is updated automatically.
    enum class Source { Loyalsoldier, Custom, None };
    Q_ENUM(Source)
    using Sources = QMap<QString, Source>;
    // A complete shared choice: effective paths, their sources, the automatic-update
    // setting, and the Loyalsoldier release tag of each Loyalsoldier path when known.
    struct Selection {
        Paths paths;
        Sources sources;
        Paths tags;
        bool autoUpdate = true;
    };
    struct Release { QString tag, path; };
    static QStringList kinds();
    static GeoDataSelection *instance();
    ~GeoDataSelection() override;
    Paths paths() const;
    Selection selection() const;
    Source source(const QString &kind) const;
    bool autoUpdate() const;
    // Read-only databases shipped in the package, and the managed download store.
    static QString bundledDirectory();
    static QString managedDirectory();
    static Release bundledRelease(const QString &kind);
    // Newest available Loyalsoldier copy: the bundled one or the current managed download.
    Release loyalsoldierRelease(const QString &kind) const;
    // Formats a Loyalsoldier tag (YYYYMMDDhhmm) as a localized release date.
    static QString releaseDate(const QString &tag);
    bool apply(const Paths &paths, const QString &core, const QList<Profile> &profiles,
               QString *error = nullptr);
    // Validates selected databases against active rules of managed profiles and persists
    // the whole selection. Kinds without a database are allowed: profiles that need them
    // are blocked at connection time instead.
    bool apply(const Selection &selection, const QString &core, const QList<Profile> &profiles,
               QString *error = nullptr);
    // Kinds with active GeoIP/GeoSite rules in the profile text (commented rules ignored).
    static QStringList requiredKinds(const QString &text);
    // Comments out every active [Rule] block that uses one of kinds. Used only for
    // validation copies, so Core can check the rest of a profile whose database is missing.
    static QString withoutRules(const QString &text, const QStringList &kinds);
    // Never writes profilePath. Empty return plus nonempty error means failure.
    // When skipped is not null, kinds without a usable database are not patched, and
    // validation ignores their rules; the kinds are reported through skipped.
    static QString materialize(const QString &text, const QString &profilePath,
                               const QString &core, const Paths &paths, QString *error = nullptr,
                               const QString &validationDirectory = {}, QStringList *skipped = nullptr);
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
    bool persist(const Selection &selection, QString *error);
    Paths paths_;
    Sources sources_;
    Paths tags_;
    bool autoUpdate_ = true;
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
