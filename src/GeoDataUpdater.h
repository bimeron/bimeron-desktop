#pragma once

#include "ProfileStore.h"
#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <functional>

class GeoDataDownload;

// Keeps Loyalsoldier databases current: at most one check a day while the application
// runs, a download only when a newer release exists, a Core check before replacement,
// and removal of managed copies older than the previous one. Own files and disabled
// kinds are never touched.
class GeoDataUpdater final : public QObject {
    Q_OBJECT
public:
    GeoDataUpdater(std::function<QString()> core, std::function<QList<Profile>()> profiles,
                   QObject *parent = nullptr, GeoDataDownload *transport = nullptr);
    // Schedules the first check shortly after startup and then hourly due checks.
    void start(int firstDelayMs = 15000);
    // Checks now when due (or always when forced); returns false when nothing started.
    bool check(bool force = false);
    bool busy() const { return !queue_.isEmpty() || !current_.isEmpty(); }
    static QDateTime lastCheck();
    static bool lastCheckFailed();
    // Deletes managed copies of kind other than keep; never touches other paths.
    static void removeStaleCopies(const QString &kind, const QStringList &keep);
signals:
    void updated(QStringList kinds);
    void finished(bool failed);
private:
    void next();
    void fail();
    std::function<QString()> core_;
    std::function<QList<Profile>()> profiles_;
    GeoDataDownload *download_ = nullptr;
    QTimer timer_, first_;
    QStringList queue_, updated_;
    QString current_, catalogKey_;
    QMetaObject::Connection catalogConnection_;
    bool failed_ = false;
};
