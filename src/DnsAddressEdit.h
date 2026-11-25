#pragma once

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QHostAddress>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QRegularExpression>
#include <QToolTip>

class DnsAddressEdit : public QLineEdit {
public:
    explicit DnsAddressEdit(bool ipv4Only, QWidget *parent = nullptr)
        : QLineEdit(parent), ipv4Only_(ipv4Only) {}

    static QStringList addresses(const QString &text) {
        return text.split(QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
    }

    QString error(const QString &input) const {
        const auto tokens = addresses(input);
        if (tokens.size() > 4) return tr("Enter no more than four IP addresses.");
        for (const auto &token : tokens) {
            QHostAddress ip;
            if (token.contains('%') || !ip.setAddress(token)
                || (ipv4Only_ && ip.protocol() != QAbstractSocket::IPv4Protocol))
                return ipv4Only_ ? tr("Enter numeric IPv4 addresses.") : tr("Enter numeric IPv4 or IPv6 addresses.");
        }
        return {};
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->matches(QKeySequence::Paste)) {
            pasteAddresses();
            event->accept();
            return;
        }
        QLineEdit::keyPressEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent *event) override {
        QMenu menu(this);
        menu.addAction(tr("Undo"), this, &QLineEdit::undo)->setEnabled(isUndoAvailable());
        menu.addAction(tr("Redo"), this, &QLineEdit::redo)->setEnabled(isRedoAvailable());
        menu.addSeparator();
        menu.addAction(tr("Cut"), this, &QLineEdit::cut)->setEnabled(!isReadOnly() && hasSelectedText());
        menu.addAction(tr("Copy"), this, &QLineEdit::copy)->setEnabled(hasSelectedText());
        menu.addAction(tr("Paste"), this, &DnsAddressEdit::pasteAddresses)->setEnabled(!isReadOnly());
        menu.addAction(tr("Select All"), this, &QLineEdit::selectAll)->setEnabled(!text().isEmpty());
        menu.exec(event->globalPos());
    }

private:
    void pasteAddresses() {
        if (isReadOnly()) return;
        const auto clipboard = QApplication::clipboard()->text();
        if (addresses(clipboard).isEmpty()) return;
        const int start = hasSelectedText() ? selectionStart() : cursorPosition();
        auto candidate = text();
        candidate.replace(start, selectedText().size(), clipboard);
        const auto problem = error(candidate);
        if (!problem.isEmpty()) {
            QToolTip::showText(mapToGlobal(rect().bottomLeft()), problem, this);
            return;
        }
        const auto normalized = addresses(candidate).join(QStringLiteral(", "));
        if (normalized == text()) return;
        selectAll();
        insert(normalized);
    }

    bool ipv4Only_;
};
