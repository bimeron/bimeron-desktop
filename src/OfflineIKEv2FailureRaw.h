#pragma once
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QStringDecoder>

namespace OfflineIKEv2Failure {
// Fixed draft shape only: root -> record. All input and token work <=512 bytes.
class RawPreflight {
    const QByteArray &b;
    qsizetype p = 0;
    void space() { while (p < b.size() && (b[p]==' ' || b[p]=='\t' || b[p]=='\r' || b[p]=='\n')) ++p; }
    bool take(char c) { space(); if (p == b.size() || b[p] != c) return false; ++p; return true; }
    bool string(QString &out) {
        space(); const auto start = p;
        if (p == b.size() || b[p++] != '"') return false;
        bool closed = false;
        while (p < b.size()) {
            const char c = b[p++];
            if (c == '\\') {
                if (p == b.size()) return false;
                const char escape = b[p++];
                if (escape == 'u') {
                    for (int digit=0; digit<4; ++digit) {
                        if (p == b.size()) return false;
                        const char h = b[p++];
                        if (!((h>='0' && h<='9') || (h>='a' && h<='f') || (h>='A' && h<='F'))) return false;
                    }
                } else if (escape!='"' && escape!='\\' && escape!='/' && escape!='b' &&
                           escape!='f' && escape!='n' && escape!='r' && escape!='t') return false;
            }
            else if (c == '"') { closed = true; break; }
        }
        if (!closed) return false;
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson("[" + b.mid(start, p-start) + "]", &error);
        if (error.error != QJsonParseError::NoError || !doc.isArray() || doc.array().size()!=1 || !doc.array()[0].isString()) return false;
        out = doc.array()[0].toString();
        for (qsizetype i=0; i<out.size(); ++i) {
            if (out[i].isHighSurrogate()) {
                if (++i == out.size() || !out[i].isLowSurrogate()) return false;
            } else if (out[i].isLowSurrogate()) return false;
        }
        return true;
    }
    bool number() {
        space(); if (p == b.size() || b[p]<'1' || b[p]>'9') return false;
        do { ++p; } while (p<b.size() && b[p]>='0' && b[p]<='9');
        return true; // Object delimiter check rejects decimals/exponents/suffixes.
    }
    bool object(bool record) {
        if (!take('{')) return false;
        QSet<QString> seen;
        for (int count=0; count<(record ? 4 : 3); ++count) {
            QString key;
            if (!string(key) || seen.contains(key) || !take(':')) return false;
            seen.insert(key);
            if (!record && (key=="operation" || key=="cleanup")) {
                if (!object(true)) return false;
            } else if ((record && (key=="code" || key=="attempt")) || (!record && key=="version")) {
                if (!number()) return false;
            } else if (record && (key=="domain" || key=="stage")) {
                QString value; if (!string(value)) return false;
            } else return false;
            if (take('}')) return true;
            if (!take(',')) return false;
        }
        return false;
    }
public:
    explicit RawPreflight(const QByteArray &bytes) : b(bytes) {}
    bool valid() {
        if (b.size()>512) return false;
        QStringDecoder decoder(QStringDecoder::Utf8);
        const QString decoded = decoder.decode(b);
        if (decoder.hasError()) return false;
        if (!object(false)) return false;
        space(); return p==b.size();
    }
};
}
