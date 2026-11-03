#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <QStringList>
#include <QSet>

// Desktop presentation edits. Only explicitly changed fields are rewritten;
// Core remains the validator and owner of ordered routing-rule edits.
namespace ProfileTextEdit {
inline QString encode(const QString &value) {
    if (!value.isEmpty() && value == value.trimmed() && !value.startsWith('"')
        && !value.contains(QRegularExpression("[\\x00-\\x1f]"))) return value;
    const QByteArray json = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}
inline bool decode(const QString &text, QString *value) {
    const QString trimmed = text.trimmed();
    if (!trimmed.startsWith('"')) { *value = trimmed; return true; }
    const auto json = QJsonDocument::fromJson(("[" + trimmed + "]").toUtf8());
    if (!json.isArray() || json.array().size() != 1 || !json.array().first().isString()) return false;
    *value = json.array().first().toString();
    return true;
}
inline QString clean(QString line) {
    if (line.startsWith(QChar::ByteOrderMark)) line.remove(0, 1);
    return line.trimmed();
}
inline QString header(QString line) {
    line = clean(line);
    if (line.startsWith('#')) {
        line = line.mid(1).trimmed();
        if (line != "[Rule]" && line != "[ShadowTLS]") return {};
    }
    return line.startsWith('[') && line.endsWith(']') ? line.mid(1, line.size() - 2).trimmed() : QString();
}
inline QStringList lines(const QString &text) {
    QStringList result;
    int start = 0;
    for (int i = 0; i < text.size(); ++i) if (text[i] == '\n') {
        result.append(text.mid(start, i - start + 1)); start = i + 1;
    }
    if (start < text.size()) result.append(text.mid(start));
    return result;
}
struct Shadow {
    bool present = false, enabled = false, safe = true;
    QString name, password;
};
inline Shadow shadow(const QString &text) {
    Shadow result;
    bool inside = false;
    QMap<QString, bool> seen;
    for (QString line : lines(text)) {
        const QString section = header(line);
        if (!section.isEmpty()) {
            if (inside && result.enabled && clean(line).startsWith('#')) result.safe = false;
            inside = section == "ShadowTLS";
            if (!inside) continue;
            if (result.present) result.safe = false;
            result.present = true;
            result.enabled = !clean(line).startsWith('#');
            continue;
        }
        if (!inside) continue;
        line = clean(line);
        if (line.isEmpty() || line.startsWith(';')) continue;
        if (!result.enabled) {
            if (!line.startsWith('#')) { result.safe = false; continue; }
            line = line.mid(1).trimmed();
            if (line.startsWith('#') || line.startsWith(';')) continue;
        } else if (line.startsWith('#')) continue;
        const int eq = line.indexOf('=');
        if (eq < 0) { if (result.enabled) result.safe = false; continue; }
        const QString key = line.left(eq).trimmed();
        if ((key != "ServerName" && key != "Password") || seen.contains(key)) { result.safe = false; continue; }
        seen.insert(key, true);
        QString value;
        if (!decode(line.mid(eq + 1), &value)) { result.safe = false; continue; }
        if (key == "ServerName") result.name = value; else result.password = value;
    }
    return result;
}
inline QString shadowBlock(bool enabled, const QString &name, const QString &password, const QString &eol) {
    if (name.isEmpty() && password.isEmpty() && !enabled) return {};
    const QString prefix = enabled ? QString() : "# ";
    QString block = prefix + "[ShadowTLS]" + eol;
    if (!name.isEmpty()) block += prefix + "ServerName = " + encode(name) + eol;
    if (!password.isEmpty()) block += prefix + "Password = " + encode(password) + eol;
    return block;
}
inline QString replaceShadow(const QString &text, bool enabled, const QString &name, const QString &password) {
    const QString eol = text.contains("\r\n") ? "\r\n" : "\n";
    QString result;
    bool inside = false, replaced = false, disabled = false;
    for (const QString &line : lines(text)) {
        const QString section = header(line);
        if (!section.isEmpty()) {
            inside = section == "ShadowTLS";
            if (inside) {
                disabled = clean(line).startsWith('#');
                if (line.startsWith(QChar::ByteOrderMark)) result += QChar::ByteOrderMark;
                result += shadowBlock(enabled, name, password, eol);
                replaced = true;
                continue;
            }
        }
        if (inside) {
            QString content = clean(line);
            if (!disabled && (content.startsWith('#') || content.startsWith(';'))) {
                result += enabled ? line : "# " + line;
                continue;
            }
            if (content.startsWith('#')) content = content.mid(1).trimmed();
            if (disabled && (content.startsWith('#') || content.startsWith(';'))) {
                result += enabled ? line.mid(line.indexOf('#') + 1).remove(QRegularExpression("^ ?")) : line;
                continue;
            }
            const QString key = content.left(content.indexOf('=')).trimmed();
            if (content.contains('=') && (key == "ServerName" || key == "Password")) continue;
            // Preserve user comments and separators surrounding the owned fields.
        }
        result += line;
    }
    if (!replaced && (enabled || !name.isEmpty() || !password.isEmpty())) {
        if (!result.isEmpty() && !result.endsWith('\n')) result += eol;
        if (!result.isEmpty()) result += eol;
        result += shadowBlock(enabled, name, password, eol);
    }
    return result;
}
inline QString field(QString text, const QString &section, const QString &key, const QString &value) {
    QStringList source = lines(text);
    const QString eol = text.contains("\r\n") ? "\r\n" : "\n";
    int start = -1, end = source.size(), found = -1, insertion = -1;
    bool inside = false;
    for (int i = 0; i < source.size(); ++i) {
        if (inside && insertion < 0 && clean(source[i]).startsWith('#') && !header(source[i]).isEmpty()) insertion = i;
        const QString h = clean(source[i]).startsWith('#') ? QString() : header(source[i]);
        if (!h.isEmpty()) {
            if (inside) { end = i; break; }
            inside = h == section && !clean(source[i]).startsWith('#');
            if (inside) start = i;
        } else if (inside) {
            const QString line = clean(source[i]);
            if (!line.startsWith('#') && !line.startsWith(';') && line.contains('=')
                && line.left(line.indexOf('=')).trimmed() == key) found = i;
        }
    }
    if (found >= 0) {
        if (value.isEmpty()) source.removeAt(found);
        else source[found] = key + " = " + encode(value) + (source[found].endsWith('\n') ? eol : QString());
    } else if (!value.isEmpty()) {
        if (insertion >= 0) end = insertion;
        QString addition;
        if (end > 0 && !source[end - 1].endsWith('\n')) addition += eol;
        if (start < 0) addition += eol + "[" + section + "]" + eol;
        addition += key + " = " + encode(value) + eol;
        source.insert(end, addition);
    }
    return source.join("");
}
inline QString normalizeRules(const QString &text, const QSet<int> &changed) {
    QString result;
    int index = -1;
    bool rule = false, disabled = false;
    for (const QString &source : lines(text)) {
        QString line = clean(source);
        const QString h = header(source);
        if (!h.isEmpty()) {
            rule = h == "Rule";
            disabled = line.startsWith('#');
            if (rule) ++index;
        }
        if (rule && changed.contains(index)) {
            if (disabled && line.startsWith('#')) line = line.mid(1).trimmed();
            const int eq = line.indexOf('=');
            if (eq > 0 && !line.startsWith('#') && !line.startsWith(';')) {
                QString value;
                if (decode(line.mid(eq + 1), &value)) {
                    result += (disabled ? "# " : "") + line.left(eq).trimmed() + " = " + encode(value)
                        + (source.endsWith("\r\n") ? "\r\n" : source.endsWith('\n') ? "\n" : "");
                    continue;
                }
            }
        }
        result += source;
    }
    return result;
}
inline QString copyShadowBlock(const QString &target, const QString &source) {
    // A BOM belongs to the document, never to a section moved within it.
    const bool bom = source.startsWith(QChar::ByteOrderMark) || target.startsWith(QChar::ByteOrderMark);
    const QString sourceBody = source.startsWith(QChar::ByteOrderMark) ? source.mid(1) : source;
    const QString targetBody = target.startsWith(QChar::ByteOrderMark) ? target.mid(1) : target;
    const QString eol = source.contains("\r\n") ? "\r\n" : "\n";
    QString block;
    bool inside = false;
    for (const QString &line : lines(sourceBody)) {
        const QString h = header(line);
        if (!h.isEmpty()) inside = h == "ShadowTLS";
        if (inside) block += line;
    }
    QString result;
    bool inserted = false;
    inside = false;
    for (const QString &line : lines(targetBody)) {
        const QString h = header(line);
        if (!h.isEmpty()) {
            // The original block may end in a comment without a final newline.
            if (inside && h != "ShadowTLS" && !result.isEmpty() && !result.endsWith('\n')) result += eol;
            inside = h == "ShadowTLS";
            if (inside) {
                if (!block.isEmpty() && !result.isEmpty() && !result.endsWith('\n')) result += eol;
                result += block;
                inserted = true;
            }
        }
        if (!inside) result += line;
    }
    if (!inserted && !block.isEmpty()) {
        if (!result.isEmpty() && !result.endsWith('\n')) result += eol;
        result += block;
    }
    return (bom ? QString(QChar::ByteOrderMark) : QString()) + result;
}
}
