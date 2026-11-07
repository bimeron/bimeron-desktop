#pragma once

// Structured Core problems and the Core error catalog. See Core docs/ERRORS.MD
// and docs/core-error-presentation.md. Decisions use codes and locations only;
// the English Core message is shown as details.

#include "CoreDiagnostic.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMap>
#include <QMutex>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStringList>

struct CoreProblem {
    QString code, message, section, field, source, value;
    QStringList related;
    int rule = 0, line = 0;
    QMap<QString, QString> args;

    // fieldKey is "Section.Field", for example "Interface.Name".
    QString fieldKey() const { return field.isEmpty() || section.isEmpty() ? QString() : section + '.' + field; }
    QString sourceKey() const { return source.isEmpty() ? QString() : QStringLiteral("AccessKey.") + source; }
};

// CoreFailure carries a rendered text for legacy callers plus structured problems.
struct CoreFailure {
    QString text;
    QList<CoreProblem> problems;
    void clear() { text.clear(); problems.clear(); }
    bool isEmpty() const { return text.isEmpty() && problems.isEmpty(); }
};

namespace coreproblems {

inline QString boundedString(const QJsonValue &value, int limit) {
    return value.isString() ? value.toString().left(limit) : QString();
}

inline bool identifier(const QString &value) {
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z][A-Za-z0-9_*]{0,63}$"));
    return value.isEmpty() || pattern.match(value).hasMatch();
}

} // namespace coreproblems

// Parses a bounded "problems" array; malformed entries are skipped.
inline QList<CoreProblem> parseCoreProblems(const QJsonValue &value) {
    using namespace coreproblems;
    QList<CoreProblem> result;
    static const QRegularExpression code(QStringLiteral("^[a-z][a-z0-9_]{0,95}$"));
    for (const auto &entry : value.toArray()) {
        if (result.size() >= 16) break;
        const auto object = entry.toObject();
        CoreProblem p;
        p.code = boundedString(object.value("code"), 96);
        if (!code.match(p.code).hasMatch()) continue;
        p.message = safeCoreDiagnostic(boundedString(object.value("message"), 4096).toUtf8());
        p.section = boundedString(object.value("section"), 64);
        p.field = boundedString(object.value("field"), 64);
        p.source = boundedString(object.value("source"), 64);
        if (!identifier(p.section) || !identifier(p.field) || !identifier(p.source)) continue;
        for (const auto &related : object.value("related").toArray()) {
            const QString name = boundedString(related, 64);
            if (!name.isEmpty() && identifier(name) && p.related.size() < 8) p.related.append(name);
        }
        p.rule = qBound(0, object.value("rule").toInt(), 100000);
        p.line = qBound(0, object.value("line").toInt(), 100000);
        const QString state = boundedString(object.value("value"), 16);
        if (state == "present" || state == "absent") p.value = state;
        const auto args = object.value("args").toObject();
        for (auto it = args.begin(); it != args.end() && p.args.size() < 16; ++it) {
            if (identifier(it.key()) && it.value().isString()) p.args.insert(it.key(), it.value().toString().left(256));
        }
        result.append(p);
    }
    return result;
}

// Returns the interface language used for catalog texts ("ru" or "en").
inline QString coreUiLanguage() {
    const QString configured = QSettings().value("app/language").toString();
    if (configured == "ru" || configured == "en") return configured;
    return QLocale::system().language() == QLocale::Russian ? QStringLiteral("ru") : QStringLiteral("en");
}

class CoreErrorCatalog {
public:
    struct Entry { QString kind, en, ru; };

    bool available() const { return !codes_.isEmpty(); }
    bool contains(const QString &code) const { return codes_.contains(code); }
    QString kind(const QString &code) const { return codes_.value(code).kind; }
    bool isSecret(const QString &field) const { return secrets_.contains(field); }
    QStringList codes() const { return codes_.keys(); }

    // Renders code in language with args; unknown codes use "unclassified".
    QString text(const QString &code, const QMap<QString, QString> &args, const QString &language) const {
        Entry entry = codes_.value(code, codes_.value(QStringLiteral("unclassified")));
        QString result = language == "ru" && !entry.ru.isEmpty() ? entry.ru : entry.en;
        for (auto it = args.begin(); it != args.end(); ++it) result.replace('{' + it.key() + '}', it.value());
        return result;
    }

    static CoreErrorCatalog fromJson(const QByteArray &data) {
        CoreErrorCatalog catalog;
        if (data.size() > 1024 * 1024) return catalog;
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(data, &error);
        const auto root = document.object();
        if (error.error != QJsonParseError::NoError || root.value("version").toInt() != 1) return catalog;
        const auto codes = root.value("codes").toObject();
        for (auto it = codes.begin(); it != codes.end(); ++it) {
            const auto object = it.value().toObject();
            Entry entry{object.value("kind").toString(), object.value("en").toString().left(2048), object.value("ru").toString().left(2048)};
            if (!entry.en.isEmpty()) catalog.codes_.insert(it.key(), entry);
        }
        for (const auto &field : root.value("secret_fields").toArray()) catalog.secrets_.insert(field.toString());
        if (!catalog.codes_.contains(QStringLiteral("unclassified"))) catalog.codes_.clear();
        return catalog;
    }

    // Loads and caches the catalog of a Core CLI; an older Core yields an
    // unavailable catalog. The cache key includes size and modification time.
    static CoreErrorCatalog forExecutable(const QString &executable) {
        const QFileInfo info(executable);
        if (executable.isEmpty() || !info.isFile()) return {};
        const QString key = info.absoluteFilePath() + '\n' + QString::number(info.size()) + '\n'
            + QString::number(info.lastModified().toMSecsSinceEpoch());
        static QMutex mutex;
        static QHash<QString, CoreErrorCatalog> cache;
        {
            QMutexLocker lock(&mutex);
            if (cache.contains(key)) return cache.value(key);
        }
        QProcess process;
        process.setProgram(info.absoluteFilePath());
        process.setArguments({QStringLiteral("errors"), QStringLiteral("--json")});
        process.setProcessChannelMode(QProcess::SeparateChannels);
        process.start();
        CoreErrorCatalog catalog;
        if (process.waitForStarted(5000) && process.waitForFinished(10000)
            && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0)
            catalog = fromJson(process.readAllStandardOutput());
        else
            process.kill();
        QMutexLocker lock(&mutex);
        cache.insert(key, catalog);
        return catalog;
    }

private:
    QHash<QString, Entry> codes_;
    QSet<QString> secrets_;
};

// Fixed-length mask; never derived from the value length.
inline QString coreSecretMask() { return QString(8, QChar(0x25CF)); }

// Renders one problem. label is the client's field label (may be empty); value
// is shown only for a secret field as a mask or "not set".
inline QString describeCoreProblem(const CoreProblem &problem, const CoreErrorCatalog &catalog,
                                   const QString &label, const QString &language = coreUiLanguage()) {
    QString text = catalog.available() ? catalog.text(problem.code, problem.args, language) : problem.message;
    if (text.isEmpty()) text = problem.message;
    QString prefix = label;
    if (!prefix.isEmpty() && catalog.isSecret(problem.field) && !problem.value.isEmpty()) {
        const QString state = problem.value == "present" ? coreSecretMask()
            : QCoreApplication::translate("CoreProblems", "not set");
        prefix += QStringLiteral(" (") + state + ')';
    }
    return prefix.isEmpty() ? text : prefix + QStringLiteral(": ") + text;
}

// Generic labels for messages shown outside the profile editor. The editor
// uses the labels of its own form fields instead.
inline QString coreFieldLabel(const CoreProblem &problem) {
    static const QMap<QString, const char *> labels{
        {"Profile.Name", QT_TRANSLATE_NOOP("CoreProblems", "Name")},
        {"Interface.Mode", QT_TRANSLATE_NOOP("CoreProblems", "Mode")},
        {"Interface.Name", QT_TRANSLATE_NOOP("CoreProblems", "Interface name")},
        {"Interface.MTU", QT_TRANSLATE_NOOP("CoreProblems", "MTU")},
        {"Interface.DNS", QT_TRANSLATE_NOOP("CoreProblems", "DNS (up to four IPv4 addresses)")},
        {"Interface.Listen", QT_TRANSLATE_NOOP("CoreProblems", "Local proxy address")},
        {"AccessKey.Server", QT_TRANSLATE_NOOP("CoreProblems", "Server")},
        {"AccessKey.Port", QT_TRANSLATE_NOOP("CoreProblems", "Port")},
        {"AccessKey.Password", QT_TRANSLATE_NOOP("CoreProblems", "Password")},
        {"AccessKey.Username", QT_TRANSLATE_NOOP("CoreProblems", "Username")},
        {"AccessKey.ID", QT_TRANSLATE_NOOP("CoreProblems", "User ID (UUID)")},
        {"AccessKey.ServerName", QT_TRANSLATE_NOOP("CoreProblems", "Server name (SNI)")},
    };
    const auto label = labels.find(problem.fieldKey());
    if (label != labels.end()) return QCoreApplication::translate("CoreProblems", label.value());
    if (!problem.source.isEmpty() && problem.field.isEmpty()) return QCoreApplication::translate("CoreProblems", "Access key");
    return {};
}

// Technical details for the "Details" area: code, location and Core message.
inline QString coreProblemDetails(const CoreProblem &problem) {
    QStringList location;
    if (!problem.section.isEmpty()) location << '[' + problem.section + ']';
    if (!problem.source.isEmpty()) location << problem.source;
    if (!problem.field.isEmpty()) location << problem.field;
    if (problem.rule > 0) location << QCoreApplication::translate("CoreProblems", "rule %1").arg(problem.rule);
    if (problem.line > 0) location << QCoreApplication::translate("CoreProblems", "line %1").arg(problem.line);
    QString details = problem.code;
    if (!location.isEmpty()) details += QStringLiteral(" — ") + location.join(' ');
    if (!problem.message.isEmpty()) details += '\n' + problem.message;
    return details;
}

// Human text for a connection or control code; empty when the catalog lacks it.
inline QString coreCodeText(const CoreErrorCatalog &catalog, const QString &code, const QString &language = coreUiLanguage()) {
    if (!catalog.contains(code)) return {};
    return catalog.text(code, {}, language);
}
