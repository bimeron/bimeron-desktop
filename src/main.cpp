#include "MainWindow.h"
#include "WindowsTheme.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QIcon>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProxyStyle>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLibraryInfo>
#include <QLocale>
#include <QLockFile>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleOptionViewItem>
#include <QSystemTrayIcon>
#include <QThread>
#include <QTranslator>

class AppStyle final : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter, const QWidget *widget = nullptr) const override {
        if (element != PE_FrameFocusRect)
            QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
    void drawControl(ControlElement element, const QStyleOption *option,
                     QPainter *painter, const QWidget *widget = nullptr) const override {
        if (element == CE_ItemViewItem) {
            if (const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
                QStyleOptionViewItem clean(*item);
                clean.state &= ~State_HasFocus;
                QProxyStyle::drawControl(element, &clean, painter, widget);
                return;
            }
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }
    QRect subElementRect(SubElement element, const QStyleOption *option,
                         const QWidget *widget = nullptr) const override {
        QRect rect = QProxyStyle::subElementRect(element, option, widget);
        if (element != SE_ItemViewItemText || !widget)
            return rect;
        for (const QObject *owner = widget; owner; owner = owner->parent()) {
            if (qobject_cast<const QComboBox *>(owner)) {
                rect.adjust(10, 0, -10, 0);
                break;
            }
        }
        return rect;
    }
};

class JsonTranslator final : public QTranslator {
public:
    bool load(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return false;
        const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
        for (auto it = json.begin(); it != json.end(); ++it)
            translations_.insert(it.key(), it.value().toString());
        return !translations_.isEmpty();
    }

    QString translate(const char *, const char *sourceText, const char *, int) const override {
        return translations_.value(QString::fromUtf8(sourceText));
    }

private:
    QHash<QString, QString> translations_;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyle(new AppStyle(app.style()));
    QCoreApplication::setOrganizationName(QStringLiteral("Bimeron"));
    QCoreApplication::setApplicationName(QStringLiteral("Bimeron Desktop"));
    applyThemePalette(app);
    QCoreApplication::setApplicationVersion(QStringLiteral(BIMERON_GUI_VERSION));
    const QString configuredLanguage = QSettings().value("app/language").toString();
    const QString language = configuredLanguage.isEmpty()
        ? (QLocale::system().language() == QLocale(QStringLiteral("ru")).language() ? QStringLiteral("ru") : QStringLiteral("en"))
        : configuredLanguage;
    QTranslator qtTranslator;
    if (language == "ru") {
        const QString translationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
        if (qtTranslator.load(QStringLiteral("qt_ru"), translationsPath)
            || qtTranslator.load(QStringLiteral("qtbase_ru"), translationsPath))
            app.installTranslator(&qtTranslator);
    }
    JsonTranslator localeTranslator;
    if (language == "ru" && localeTranslator.load(QStringLiteral(":/translations/ru.json")))
        app.installTranslator(&localeTranslator);
#ifdef Q_OS_LINUX
    QApplication::setDesktopFileName(QStringLiteral("bimeron"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/tray/png/light-disconnected-64.png")));
#else
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/bimeron.ico")));
#endif
    QApplication::setQuitOnLastWindowClosed(false);
    const bool restarting = app.arguments().contains(QStringLiteral("--restart"));
#ifdef Q_OS_LINUX
    const bool autostarting = app.arguments().contains(QStringLiteral("--autostart"));
#else
    const bool autostarting = false;
#endif
    const bool visualCheck = app.arguments().contains(QStringLiteral("--visual-check"));
    const QString instanceName = visualCheck ? QStringLiteral("BimeronDesktopGuiVisualCheck") : QStringLiteral("BimeronDesktopGui");
    QLockFile instanceLock(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                               .filePath(visualCheck ? "bimeron-desktop-visual-check.lock" : "bimeron-desktop.lock"));
    instanceLock.setStaleLockTime(0);
    if (!instanceLock.tryLock()) {
        if (restarting) {
            for (int attempt = 0; attempt < 100 && !instanceLock.tryLock(); ++attempt)
                QThread::msleep(50);
            if (!instanceLock.isLocked()) return 0;
        } else {
        for (int attempt = 0; attempt < 10; ++attempt) {
            QLocalSocket socket;
            socket.connectToServer(instanceName);
            if (socket.waitForConnected(200)) {
                socket.write(autostarting ? "autostart\n" : "show\n");
                socket.waitForBytesWritten(200);
                return 0;
            }
            QThread::msleep(100);
        }
        return 0;
        }
    }
    QLocalServer::removeServer(instanceName);
    QLocalServer instanceServer;
    if (!instanceServer.listen(instanceName))
        return 0;
    MainWindow window;
    QObject::connect(&instanceServer, &QLocalServer::newConnection, &window, [&] {
        while (auto *socket = instanceServer.nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::readyRead, &window, [&window, socket] {
                if (socket->readAll().startsWith("show"))
                    window.showWindow();
                socket->deleteLater();
            });
        }
    });
    if (!autostarting || !QSystemTrayIcon::isSystemTrayAvailable()) window.show();
    return app.exec();
}

