#include "MainWindow.h"
#include "WindowsTheme.h"
#include "HiDpiCheckBox.h"
#include "HiDpiComboBox.h"
#include "ProfileEditorDialog.h"
#include "CoreDiagnostic.h"
#include "TrustedCore.h"

#include <QColor>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QIcon>
#include <QImage>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <limits>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QShortcut>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QToolTip>
#include <QStyleHints>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QSysInfo>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTimer>
#include <QTime>
#include <QToolButton>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QVBoxLayout>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shellapi.h>
#include <sddl.h>
#elif defined(Q_OS_LINUX)
#include <unistd.h>
#endif

namespace {
QJsonObject coreIdentity(const QJsonObject &identity) {
    QJsonObject result;
    const QString version = identity.value("version").toString();
    const QString revision = identity.value("revision").toString();
    if (QRegularExpression(QStringLiteral("^v?[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}(?:-(?:dev|(?:alpha|beta|rc)\\.[0-9]{1,4})(?:-dev)?)?$")).match(version).hasMatch())
        result.insert("version", version);
    if (QRegularExpression(QStringLiteral("^[0-9a-f]{40}$")).match(revision).hasMatch()) result.insert("revision", revision);
    if (identity.value("modified").isBool()) result.insert("modified", identity.value("modified"));
    if (QStringList{"windows", "linux", "darwin"}.contains(identity.value("platform").toString()))
        result.insert("platform", identity.value("platform"));
    if (QStringList{"amd64", "arm64"}.contains(identity.value("architecture").toString()))
        result.insert("architecture", identity.value("architecture"));
    return result;
}

#ifdef Q_OS_WIN
QString processUserSid(HANDLE process) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token)) return {};
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    QByteArray data(static_cast<qsizetype>(size), '\0');
    QString sid;
    if (GetTokenInformation(token, TokenUser, data.data(), size, &size)) {
        LPWSTR value = nullptr;
        if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(data.data())->User.Sid, &value)) {
            sid = QString::fromWCharArray(value);
            LocalFree(value);
        }
    }
    CloseHandle(token);
    return sid;
}

bool startUnelevated(const QStringList &arguments) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_LINKED_TOKEN linked{};
    DWORD size = 0;
    const bool available = GetTokenInformation(token, TokenLinkedToken, &linked, sizeof(linked), &size);
    CloseHandle(token);
    if (!available) return false;
    const std::wstring executable = QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString();
    std::wstring commandLine = TrustedCore::windowsCommandLine(QStringList{QCoreApplication::applicationFilePath()} + arguments).toStdWString();
    const std::wstring directory = QDir::toNativeSeparators(QCoreApplication::applicationDirPath()).toStdWString();
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const bool started = CreateProcessWithTokenW(linked.LinkedToken, LOGON_WITH_PROFILE, executable.c_str(),
        commandLine.data(), 0, nullptr, directory.c_str(), &startup, &process);
    CloseHandle(linked.LinkedToken);
    if (started) { CloseHandle(process.hThread); CloseHandle(process.hProcess); }
    return started;
}
#endif
class ProfileRowDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        if (!index.data(Qt::UserRole + 1).toBool()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        QStyleOptionViewItem row = option;
        initStyleOption(&row, index);
        // initStyleOption converts newlines to Unicode line separators.
        const QString name = index.data(Qt::DisplayRole).toString().section('\n', 0, 0);
        const QWidget *widget = row.widget;
        const QStyle *style = widget ? widget->style() : QApplication::style();
        const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &row, widget);
        row.text.clear();
        style->drawControl(QStyle::CE_ItemViewItem, &row, painter, widget);
        painter->save();
        painter->setClipRect(row.rect);
        painter->setFont(row.font);
        painter->setPen(row.palette.color(QPalette::Text));
        painter->drawText(textRect.adjusted(0, 2, 0, -textRect.height() / 2), Qt::AlignVCenter, name);
        painter->setPen(QColor(appDarkMode() ? "#ff9a92" : "#c62828"));
        painter->drawText(textRect.adjusted(0, textRect.height() / 2, 0, -2), Qt::AlignVCenter, QObject::tr("Unknown protocol"));
        painter->restore();
    }
};

QString themedStyle(QString style) {
    if (!appDarkMode()) return style;
    const QPair<const char *, const char *> colors[] = {
        {"#f8fafc", "#181e27"}, {"#f1f6fc", "#202937"},
        {"#ffffff", "#202937"}, {"white", "#202937"},
        {"#f7f9fc", "#263244"}, {"#f7fbff", "#30445d"},
        {"#f2f7fd", "#30445d"}, {"#edf5ff", "#30445d"},
        {"#e1edfb", "#365579"}, {"#dcecff", "#365579"},
        {"#edf1f5", "#2b3544"}, {"#dce5ef", "#40516a"},
        {"#d6dee7", "#40516a"},
        {"#e5ebf2", "#40516a"}, {"#ccd8e5", "#52647c"},
        {"#b8c6d6", "#657b95"}, {"#152238", "#e8edf5"},
        {"#26364a", "#e8edf5"}, {"#526174", "#c1cedd"},
        {"#718096", "#aab8ca"}, {"#95a2b2", "#8493a7"}, {"#66768a", "#aab8ca"},
        {"#c62828", "#ff9a92"}, {"#16803c", "#76d49a"},
        {"#d69200", "#f3cf72"}, {"#b54708", "#f3cf72"},
        {"#1267d6", "#79b4ff"}, {"#0d56b4", "#a4ccff"},
        {"#1677ff", "#79b4ff"}, {"#b42318", "#ff9a92"},
        {"#912018", "#ffb5ae"}, {"#fff5f4", "#493039"}
    };
    for (const auto &[light, dark] : colors)
        style.replace(QString::fromLatin1(light), QString::fromLatin1(dark));
    return style;
}
class ConnectionButton final : public QPushButton {
public:
    using QPushButton::QPushButton;

protected:
    void paintEvent(QPaintEvent *event) override {
        if (devicePixelRatioF() <= 1) {
            QPushButton::paintEvent(event);
            return;
        }
        const bool disconnect = property("mode").toString() == QStringLiteral("disconnect");
        const bool pressed = isDown();
        const bool hovered = underMouse();
        const QColor blue = pressed ? QColor("#0c5fc8") : hovered ? QColor("#0e6fe9") : QColor("#1677ff");
        const bool dark = appDarkMode();
        const QColor border = isEnabled() ? blue : QColor(dark ? "#40516a" : "#d6dee7");
        const QColor background = !isEnabled() ? QColor(dark ? "#2b3544" : "#edf1f5")
            : disconnect ? (pressed ? palette().color(QPalette::Highlight) : hovered ? palette().color(QPalette::AlternateBase) : palette().color(QPalette::Base))
                         : blue;
        const QColor foreground = !isEnabled() ? QColor(dark ? "#8493a7" : "#95a2b2")
            : disconnect ? QColor("#1677ff") : QColor(Qt::white);

        const qreal scale = devicePixelRatioF();
        QImage image(qFloor(width() * scale), qFloor(height() * scale), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        const int radius = qRound(6 * scale);
        QPainter shape(&image);
        shape.setRenderHint(QPainter::Antialiasing);
        shape.setPen(Qt::NoPen);
        shape.setBrush(border);
        shape.drawRoundedRect(QRectF(0, 0, image.width(), image.height()), radius, radius);
        if (disconnect || !isEnabled()) {
            shape.setBrush(background);
            shape.drawRoundedRect(QRectF(1, 1, image.width() - 2, image.height() - 2),
                                  radius - 1, radius - 1);
        }
        shape.fillRect(QRect(radius, 0, image.width() - 2 * radius, 1), border);
        shape.fillRect(QRect(radius, image.height() - 1, image.width() - 2 * radius, 1), border);
        shape.fillRect(QRect(0, radius, 1, image.height() - 2 * radius), border);
        shape.fillRect(QRect(image.width() - 1, radius, 1, image.height() - 2 * radius), border);
        shape.end();
        for (int y = 0; y < radius; ++y) {
            for (int x = 0; x < radius; ++x) {
                const QRgb pixel = image.pixel(image.width() - 1 - x, image.height() - 1 - y);
                image.setPixel(x, y, pixel);
                image.setPixel(image.width() - 1 - x, y, pixel);
                image.setPixel(x, image.height() - 1 - y, pixel);
            }
        }
        image.setDevicePixelRatio(scale);
        QPainter painter(this);
        painter.drawImage(QPointF(0, 0), image);
        painter.setPen(foreground);
        painter.drawText(rect(), Qt::AlignCenter | Qt::TextShowMnemonic, text());
    }
};

#ifdef Q_OS_WIN
class ProfileListOutline final : public QWidget {
public:
    ProfileListOutline(QWidget *parent, QListWidget *list) : QWidget(parent), list_(list) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        const qreal scale = devicePixelRatioF();
        QImage image(qFloor(width() * scale), qFloor(height() * scale), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        const QColor border = list_->hasFocus() ? QColor("#1677ff") : QColor(appDarkMode() ? "#40516a" : "#dce5ef");
        const int radius = qRound(7 * scale);
        QPainter shape(&image);
        shape.setRenderHint(QPainter::Antialiasing);
        shape.setPen(Qt::NoPen);
        shape.setBrush(border);
        shape.drawRoundedRect(QRectF(0, 0, image.width(), image.height()), radius, radius);
        shape.setCompositionMode(QPainter::CompositionMode_Clear);
        shape.setBrush(Qt::black);
        shape.drawRoundedRect(QRectF(1, 1, image.width() - 2, image.height() - 2),
                              radius - 1, radius - 1);
        shape.setCompositionMode(QPainter::CompositionMode_SourceOver);
        shape.fillRect(QRect(radius, 0, image.width() - 2 * radius, 1), border);
        shape.fillRect(QRect(radius, image.height() - 1, image.width() - 2 * radius, 1), border);
        shape.fillRect(QRect(0, radius, 1, image.height() - 2 * radius), border);
        shape.fillRect(QRect(image.width() - 1, radius, 1, image.height() - 2 * radius), border);
        shape.end();
        for (int y = 0; y < radius; ++y) {
            for (int x = 0; x < radius; ++x) {
                const QRgb pixel = image.pixel(image.width() - 1 - x, image.height() - 1 - y);
                image.setPixel(x, y, pixel);
                image.setPixel(image.width() - 1 - x, y, pixel);
                image.setPixel(x, image.height() - 1 - y, pixel);
            }
        }
        image.setDevicePixelRatio(scale);
        QPainter painter(this);
        painter.drawImage(QPointF(0, 0), image);
    }

private:
    QListWidget *list_;
};

class ProfileListFrame final : public QWidget {
public:
    explicit ProfileListFrame(QListWidget *list) : list_(list) {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(1, 1, 1, 1);
        layout->addWidget(list_);
        outline_ = new ProfileListOutline(this, list_);
        list_->installEventFilter(this);
        outline_->raise();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == list_ && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut))
            outline_->update();
        return QWidget::eventFilter(watched, event);
    }

    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        outline_->setGeometry(rect());
        outline_->raise();
    }

private:
    QListWidget *list_;
    ProfileListOutline *outline_;
};
#endif

QString coreExecutablePath() {
#ifdef Q_OS_WIN
    return QCoreApplication::applicationDirPath() + QStringLiteral("/bimeron-core.exe");
#else
    return QCoreApplication::applicationDirPath() + QStringLiteral("/bimeron-core");
#endif
}

QPushButton *navButton(const QString &text) {
    auto *button = new QPushButton(text);
    button->setCheckable(true);
    button->setAutoExclusive(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setObjectName("nav");
    return button;
}

QIcon statusDot(const QColor &color) {
    QPixmap canvas(26, 26);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QRectF(8.5, 8.5, 9, 9));
    return QIcon(canvas);
}

}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), core_(this) {
    developerMode_ = QCoreApplication::arguments().contains("--development")
        && QCoreApplication::arguments().contains("--restart")
        && !QCoreApplication::arguments().contains("--autostart");
    development_ = new DevelopmentCore(coreExecutablePath(), this);
    validationExecutable_ = coreExecutablePath();
    setWindowTitle(tr("Bimeron"));
#ifdef Q_OS_LINUX
    setWindowIcon(QIcon(QStringLiteral(":/icons/tray/png/light-disconnected-64.png")));
#else
    setWindowIcon(QIcon(QStringLiteral(":/icons/bimeron.ico")));
#endif
    resize(720, 600);
    setMinimumSize(720, 600);

    auto *root = new QWidget;
    auto *layout = new QHBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *sidebar = new QWidget;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(150);
    auto *navigation = new QVBoxLayout(sidebar);
    navigation->setContentsMargins(10, 14, 10, 10);
    auto *brand = new QLabel(QStringLiteral("◈  Bimeron"));
    brand->setObjectName("brand");
    navigation->addWidget(brand);
    navigation->addSpacing(14);
    QStringList labels{tr("Profiles"), tr("Settings"), tr("Log"), tr("About")};
    if (developerMode_) labels << tr("Development");
    for (int i = 0; i < labels.size(); ++i) {
        auto *button = navButton(labels[i]);
        if (i == (developerMode_ ? 4 : 0)) button->setChecked(true);
        connect(button, &QPushButton::clicked, this, [this, i] { pages_->setCurrentIndex(i); });
        const QString key = i == 1 ? QStringLiteral(",") : QString::number(i + 1);
#ifdef Q_OS_MACOS
        auto *shortcut = new QShortcut(QKeySequence(QStringLiteral("Meta+") + key), this);
#else
        auto *shortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+") + key), this);
#endif
        connect(shortcut, &QShortcut::activated, this, [this, button, i] {
            pages_->setCurrentIndex(i);
            button->setChecked(true);
        });
        navigation->addWidget(button);
    }
    navigation->addStretch();

    pages_ = new QStackedWidget;
    pages_->addWidget(profilesPage());
    pages_->addWidget(settingsPage());
    pages_->addWidget(logPage());
    pages_->addWidget(aboutPage());
    if (developerMode_) {
        pages_->addWidget(developmentPage());
        pages_->setCurrentIndex(4);
        autoConnectAttempted_ = true;
    }
    auto *newProfile = new QShortcut(QKeySequence::New, this);
    connect(newProfile, &QShortcut::activated, this, [this] {
        if (pages_->currentIndex() == 0) addProfile();
    });
    layout->addWidget(sidebar);
    layout->addWidget(pages_, 1);
    setCentralWidget(root);
    initTray();

    QString applicationStyle = QStringLiteral(R"(
        QMainWindow { background: #f8fafc; color: #152238; font-size: 12px; }
        QDialog { background: white; color: #152238; }
        #sidebar { background: #f1f6fc; border-right: 1px solid #dce5ef; }
        #brand { font-size: 15px; font-weight: 700; color: #1677ff; padding: 3px; }
        QPushButton#nav { border: 0; border-radius: 6px; text-align: left; padding: 7px 9px; color: #526174; }
        QPushButton#nav:checked { background: #dcecff; color: #1267d6; }
        QPushButton { min-height: 27px; border: 1px solid #ccd8e5; border-radius: 6px; padding: 0 10px; background: white; }
        QPushButton:hover { color: #1267d6; background: #f2f7fd; border-color: #77aef4; }
        QPushButton:focus { color: #1267d6; border: 1px solid #1677ff; }
        QPushButton:pressed { color: #0d56b4; background: #e1edfb; border-color: #1267d6; }
        QPushButton:disabled { color: #95a2b2; background: #edf1f5; border-color: #d6dee7; }
        QPushButton#connectButton[mode="connect"] { color: white; background: #1677ff; border-color: #1677ff; }
        QPushButton#connectButton[mode="connect"]:hover { background: #0e6fe9; border-color: #0e6fe9; }
        QPushButton#connectButton[mode="connect"]:pressed { background: #0c5fc8; border-color: #0c5fc8; }
        QPushButton#connectButton[mode="disconnect"] { color: #1677ff; background: white; border: 1px solid #1677ff; }
        QPushButton#connectButton[mode="disconnect"]:hover { background: #edf5ff; border-color: #0e6fe9; }
        QPushButton#connectButton[mode="disconnect"]:pressed { background: #dcecff; border-color: #0c5fc8; }
        QPushButton#connectButton:disabled { color: #95a2b2; background: #edf1f5; border-color: #d6dee7; }
        QFrame#settingsCard { background: white; border: 1px solid #dce5ef; border-radius: 9px; }
        QLabel#sectionTitle { color: #26364a; font-size: 13px; font-weight: 700; }
        QLabel#sectionHint { color: #718096; }
        QLabel#fieldValue { min-height: 27px; padding: 0 8px; color: #526174; background: #f7f9fc; border: 1px solid #dce5ef; border-radius: 6px; }
        QPushButton#dangerButton { color: #b42318; }
        QPushButton#dangerButton:hover { color: #912018; background: #fff5f4; border-color: #e5a29c; }
        QLineEdit, QComboBox, QSpinBox { min-height: 27px; background: white; border: 1px solid #ccd8e5; border-radius: 6px; }
        QLineEdit { padding: 0 8px; }
        QComboBox, QSpinBox { padding: 0 30px 0 8px; }
        QComboBox[popupOpen="true"] { border-bottom-left-radius: 0; border-bottom-right-radius: 0; }
        QLineEdit:hover, QComboBox:hover, QSpinBox:hover { border-color: #77aef4; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: #1677ff; }
        QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled { color: #95a2b2; background: #edf1f5; border-color: #d6dee7; }
        QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 26px; border: 0; }
        QComboBox::down-arrow { image: url(:/icons/chevron-down.xpm); width: 9px; height: 5px; }
        QComboBox QAbstractItemView { padding: 0; background: white; border: 1px solid #ccd8e5; border-top: 0; border-top-left-radius: 0; border-top-right-radius: 0; border-bottom-left-radius: 6px; border-bottom-right-radius: 6px; outline: 0; selection-color: #1267d6; selection-background-color: #edf5ff; }
        QComboBox QAbstractItemView::item { min-height: 26px; padding: 0 6px; border: 0; }
        QComboBox QAbstractItemView::item:selected { color: #1267d6; background: #edf5ff; border: 0; outline: 0; }
        QSpinBox::up-button, QSpinBox::down-button { subcontrol-origin: border; width: 26px; background: transparent; border: 0; border-left: 1px solid #e5ebf2; }
        QSpinBox::up-button { subcontrol-position: top right; border-top-right-radius: 6px; }
        QSpinBox::down-button { subcontrol-position: bottom right; border-bottom-right-radius: 6px; }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: #edf5ff; }
        QSpinBox::up-arrow { image: url(:/icons/chevron-up.xpm); width: 9px; height: 5px; }
        QSpinBox::down-arrow { image: url(:/icons/chevron-down.xpm); width: 9px; height: 5px; }
        QCheckBox { min-height: 27px; spacing: 7px; }
        QCheckBox:focus { color: #1267d6; }
        QCheckBox::indicator { width: 15px; height: 15px; background: white; border: 1px solid #b8c6d6; border-radius: 4px; }
        QCheckBox::indicator:hover { border-color: #1677ff; background: #f2f7fd; }
        QCheckBox::indicator:checked { image: url(:/icons/check.xpm); background: white; border-color: #1677ff; }
        QCheckBox::indicator:checked:hover { background: #edf5ff; border-color: #0e6fe9; }
        QCheckBox::indicator:disabled { background: #edf1f5; border-color: #d6dee7; }
        QCheckBox:disabled { color: #95a2b2; }
        QTabWidget::pane { background: white; border: 0; border-top: 1px solid #dce5ef; top: -1px; }
        QTabBar { background: white; outline: 0; }
        QTabBar::scroller { width: 0; }
        QTabBar QToolButton { width: 0; height: 0; border: 0; padding: 0; margin: 0; }
        QTabBar::tab { min-height: 32px; padding: 0 13px; color: #526174; background: transparent; border: 0; border-bottom: 2px solid transparent; outline: 0; }
        QTabBar::tab:hover { color: #1267d6; background: #f2f7fd; }
        QTabBar::tab:selected { color: #1267d6; font-weight: 600; border-bottom-color: #1677ff; }
        QTableWidget { background: white; border: 1px solid #ccd8e5; border-radius: 0; gridline-color: #e5ebf2; outline: 0; }
        QTableWidget::item:selected { color: #152238; background: #edf5ff; }
        QHeaderView::section { min-height: 28px; padding: 0 7px; color: #526174; font-weight: 600; background: #f7f9fc; border: 0; border-right: 1px solid #e5ebf2; border-bottom: 1px solid #dce5ef; }
        QLineEdit#tableEditor, QComboBox#tableEditor { min-height: 28px; padding-top: 0; padding-bottom: 0; background: white; border: 0; border-radius: 0; }
        QLineEdit#tableEditor:focus, QComboBox#tableEditor:focus { background: #f7fbff; border: 0; border-bottom: 2px solid #1677ff; }
        QListWidget, QTextEdit { background: white; border: 1px solid #dce5ef; border-radius: 7px; }
        QListWidget:focus, QTextEdit:focus { border-color: #1677ff; }
        QListWidget::item { min-height: 48px; padding: 2px 0; border-bottom: 1px solid #edf1f5; }
        QListWidget::item:selected { color: #152238; background: #edf5ff; border: none; }
    )");
#ifdef Q_OS_LINUX
    applicationStyle += QStringLiteral(R"(
        QComboBox { combobox-popup: 0; }
    )");
#endif
    applicationStyle += QStringLiteral(R"(
        QComboBox QAbstractItemView { border: 0; border-radius: 0; }
    )");
    setStyleSheet(themedStyle(applicationStyle));

    connect(&core_, &CoreClient::replied, this, &MainWindow::handleReply);
    connect(&core_, &CoreClient::failed, this, &MainWindow::connectionFailure);
    pollClock_.start();
    auto *pollTimer = new QTimer(this);
    pollTimer->setInterval(250);
    connect(pollTimer, &QTimer::timeout, this, &MainWindow::poll);
    if (!QCoreApplication::arguments().contains("--visual-check")) {
        pollTimer->start();
        QTimer::singleShot(0, this, &MainWindow::poll);
    }
    connect(development_, &DevelopmentCore::changed, this, &MainWindow::updateDevelopment);
    connect(development_, &DevelopmentCore::forcedExitReady, this, [] { QApplication::quit(); });
    connect(development_, &DevelopmentCore::completed, this, [this](bool success) {
        if (!developmentExit_) return;
        developmentExit_ = false;
        if (success && !development_->needsCleanup()) {
            if (restartRequested_) restartApplication();
            else QApplication::quit();
        } else {
            restartRequested_ = false;
            showWindow();
            QMessageBox prompt(QMessageBox::Warning, tr("Development"), development_->status(), QMessageBox::NoButton, this);
            auto *stay = prompt.addButton(tr("Stay"), QMessageBox::RejectRole);
            auto *close = prompt.addButton(tr("Close GUI only"), QMessageBox::DestructiveRole);
            QPushButton *force = nullptr;
            if (development_->orphanAvailable())
                force = prompt.addButton(tr("Force stop Core and close"), QMessageBox::DestructiveRole);
            prompt.setDefaultButton(stay);
            prompt.exec();
            if (prompt.clickedButton() == close) QApplication::quit();
            else if (force && prompt.clickedButton() == force) {
                if (QMessageBox::warning(this, tr("Force stop Core and close"),
                        tr("Core will be terminated without network cleanup. Routes, DNS and the kill switch may remain. Continue?"),
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
                    development_->stopOrphan(true);
            }
        }
    });
    refreshProfiles();
    connectionLabel_->setText(tr("Connecting to core…"));
    resetMeasurements();
    probeValidationCli();
    updateConnectionButton();
    if (trayMenu_) rebuildTrayMenu();
}

QWidget *MainWindow::profilesPage() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);
    auto *title = new QLabel(tr("Profiles"));
    title->setStyleSheet("font-size: 17px; font-weight: 700");
    layout->addWidget(title);
    connectionLabel_ = new QLabel(tr("Disconnected"));
    connectionLabel_->setStyleSheet(themedStyle("color: #66768a"));
    layout->addWidget(connectionLabel_);
    trafficLabel_ = new QLabel(tr("Traffic: ↑ 0 B  ↓ 0 B"));
    trafficLabel_->setStyleSheet(themedStyle("color: #66768a"));
    layout->addWidget(trafficLabel_);
    sessionStatsLabel_ = new QLabel(tr("Connections: —"));
    sessionStatsLabel_->setStyleSheet(themedStyle("color: #66768a"));
    sessionStatsLabel_->setWordWrap(true);
    sessionStatsLabel_->setVisible(QSettings().value("app/showDetailedStats", false).toBool());
    layout->addWidget(sessionStatsLabel_);
    profileList_ = new QListWidget;
    profileList_->setItemDelegate(new ProfileRowDelegate(profileList_));
#ifdef Q_OS_WIN
    profileList_->setStyleSheet(QStringLiteral("QListWidget { padding: 1px; border: 0; border-radius: 0; }"));
#else
    profileList_->setStyleSheet(QStringLiteral("QListWidget { padding: 1px; }"));
#endif
    profileList_->setFocusPolicy(Qt::StrongFocus);
    profileList_->setIconSize(QSize(26, 26));
    profileList_->installEventFilter(this);
    profileList_->viewport()->installEventFilter(this);
    profileGear_ = new QToolButton(profileList_->viewport());
    profileGear_->setIcon(QIcon(QStringLiteral(":/icons/edit.xpm")));
    profileGear_->setIconSize(QSize(14, 14));
    profileGear_->setToolTip(tr("Edit profile"));
    profileGear_->setFixedSize(30, 30);
    profileGear_->setStyleSheet(themedStyle("QToolButton { border: 1px solid #c7d5e5; border-radius: 6px; background: white; } QToolButton:hover { background: #edf5ff; border-color: #77aef4; } QToolButton:pressed { background: #dcecff; border-color: #1677ff; }"));
    profileGear_->hide();
    connect(profileGear_, &QToolButton::clicked, this, [this] {
        const int row = gearRow_;
        profileGear_->hide();
        gearRow_ = -1;
        editProfile(row);
    });
    connect(profileList_, &QListWidget::currentRowChanged, this, [this](int row) {
        QTimer::singleShot(0, this, [this, row] {
            if (row < 0 || row != profileList_->currentRow() || !profileList_->hasFocus()
                || !profileList_->item(row)->isSelected()) return;
            const QRect rect = profileList_->visualItemRect(profileList_->item(row));
            gearRow_ = row;
            profileGear_->move(rect.right() - profileGear_->width() - 14,
                               rect.center().y() - profileGear_->height() / 2);
            profileGear_->show();
            profileGear_->raise();
        });
    });
    connect(profileList_, &QListWidget::itemSelectionChanged, this, [this] {
        if (removeProfileButton_)
            removeProfileButton_->setEnabled(!profileList_->selectedItems().isEmpty());
        if (profileList_->selectedItems().isEmpty()) {
            profileGear_->hide();
            gearRow_ = -1;
        } else if (profileList_->hasFocus()) {
            const int row = profileList_->currentRow();
            const QRect rect = profileList_->visualItemRect(profileList_->item(row));
            gearRow_ = row;
            profileGear_->move(rect.right() - profileGear_->width() - 14,
                               rect.center().y() - profileGear_->height() / 2);
            profileGear_->show();
            profileGear_->raise();
        }
        updateConnectionButton();
    });
    connect(profileList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        profileList_->setCurrentItem(item);
        toggleConnection();
    });
    auto *activateProfile = new QShortcut(QKeySequence(Qt::Key_Return), profileList_);
    activateProfile->setContext(Qt::WidgetWithChildrenShortcut);
    connect(activateProfile, &QShortcut::activated, this, &MainWindow::toggleConnection);
    auto *activateProfileNumpad = new QShortcut(QKeySequence(Qt::Key_Enter), profileList_);
    activateProfileNumpad->setContext(Qt::WidgetWithChildrenShortcut);
    connect(activateProfileNumpad, &QShortcut::activated, this, &MainWindow::toggleConnection);
    auto *deleteProfile = new QShortcut(QKeySequence(Qt::Key_Delete), profileList_);
    deleteProfile->setContext(Qt::WidgetWithChildrenShortcut);
    connect(deleteProfile, &QShortcut::activated, this, &MainWindow::removeProfile);
    for (int delta : {-1, 1}) {
        auto *shortcut = new QShortcut(QKeySequence(delta < 0 ? Qt::SHIFT | Qt::Key_Up : Qt::SHIFT | Qt::Key_Down), profileList_);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, delta] { moveProfile(delta); });
    }
#ifdef Q_OS_WIN
    layout->addWidget(new ProfileListFrame(profileList_), 1);
#else
    layout->addWidget(profileList_, 1);
#endif
    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(tr("+  Add key"));
    auto *import = new QPushButton(tr("Import .conf"));
    auto *remove = new QPushButton(tr("Remove"));
    removeProfileButton_ = remove;
    remove->setEnabled(false);
    connectButton_ = new ConnectionButton(tr("Connect"));
    connectButton_->setObjectName("connectButton");
    connectionLabel_->setObjectName("connectionStatus");
    connectButton_->setProperty("mode", "connect");
    connectButton_->setEnabled(false);
    connect(add, &QPushButton::clicked, this, &MainWindow::addProfile);
    connect(import, &QPushButton::clicked, this, &MainWindow::importProfile);
    connect(remove, &QPushButton::clicked, this, &MainWindow::removeProfile);
    connect(connectButton_, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    buttons->addWidget(add);
    buttons->addWidget(import);
    buttons->addWidget(remove);
    buttons->addStretch();
    buttons->addWidget(connectButton_);
    layout->addLayout(buttons);
    nativeStatusLabel_ = new QLabel;
    nativeStatusLabel_->setTextFormat(Qt::PlainText);
    nativeStatusLabel_->setWordWrap(true);
    nativeStatusLabel_->hide();
    layout->addWidget(nativeStatusLabel_);
    return page;
}

QWidget *MainWindow::settingsPage() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);
    auto *title = new QLabel(tr("Settings"));
    title->setStyleSheet("font-size: 17px; font-weight: 700");
    layout->addWidget(title);

    auto *hint = new QLabel(tr("Application behavior, automatic connection, and Bimeron maintenance."));
    hint->setObjectName("sectionHint");
    layout->addWidget(hint);

    auto *generalCard = new QFrame;
    generalCard->setObjectName("settingsCard");
    auto *general = new QVBoxLayout(generalCard);
    general->setContentsMargins(14, 12, 14, 12);
    general->setSpacing(7);
    auto *generalTitle = new QLabel(tr("Application"));
    generalTitle->setObjectName("sectionTitle");
    general->addWidget(generalTitle);
    auto *themeRow = new QHBoxLayout;
    auto *themeLabel = new QLabel(tr("Theme"));
    auto *theme = new HiDpiComboBox;
    theme->setMinimumWidth(125);
    theme->addItem(tr("System"), QStringLiteral("system"));
    theme->addItem(tr("Light"), QStringLiteral("light"));
    theme->addItem(tr("Dark"), QStringLiteral("dark"));
    const QString selectedTheme = QSettings().value("app/theme", "system").toString();
    theme->setCurrentIndex(qMax(0, theme->findData(selectedTheme)));
    themeRow->addWidget(themeLabel);
    themeRow->addStretch();
    themeRow->addWidget(theme);
    general->addLayout(themeRow);
    connect(theme, &QComboBox::currentIndexChanged, this, [this, theme](int index) {
        QSettings settings;
        const bool hadOverride = settings.contains("app/theme");
        const QString previousTheme = settings.value("app/theme", "system").toString();
        const QString nextTheme = theme->itemData(index).toString();
        if (nextTheme == previousTheme) return;
        settings.setValue("app/theme", nextTheme);
        settings.sync();
        QMessageBox prompt(QMessageBox::Information, tr("Change theme"),
            tr("Restart Bimeron to apply the new theme."), QMessageBox::NoButton, this);
        auto *restart = prompt.addButton(tr("Restart"), QMessageBox::AcceptRole);
        prompt.addButton(tr("Cancel"), QMessageBox::RejectRole);
        prompt.exec();
        if (prompt.clickedButton() == restart) {
            restartApplication();
            return;
        }
        if (hadOverride) settings.setValue("app/theme", previousTheme);
        else settings.remove("app/theme");
        const QSignalBlocker blocker(theme);
        theme->setCurrentIndex(theme->findData(previousTheme));
    });
    auto *languageRow = new QHBoxLayout;
    auto *languageLabel = new QLabel(tr("Language"));
    auto *language = new HiDpiComboBox;
    language->setMinimumWidth(125);
#ifdef Q_OS_LINUX
    language->setStyleSheet(QStringLiteral("QComboBox { combobox-popup: 0; }"));
#endif
    language->addItem(QStringLiteral("English"), QStringLiteral("en"));
    language->addItem(QStringLiteral("Русский"), QStringLiteral("ru"));
    QString selectedLanguage = QSettings().value("app/language").toString();
    if (selectedLanguage.isEmpty())
        selectedLanguage = QLocale::system().language() == QLocale(QStringLiteral("ru")).language() ? "ru" : "en";
    language->setCurrentIndex(language->findData(selectedLanguage));
    languageRow->addWidget(languageLabel);
    languageRow->addStretch();
    languageRow->addWidget(language);
    general->addLayout(languageRow);
    connect(language, &QComboBox::currentIndexChanged, this, [this, language](int index) {
        QSettings settings;
        const bool hadOverride = settings.contains("app/language");
        const QString previousLanguage = hadOverride ? settings.value("app/language").toString()
            : (QLocale::system().language() == QLocale(QStringLiteral("ru")).language() ? "ru" : "en");
        const QString nextLanguage = language->itemData(index).toString();
        if (nextLanguage == previousLanguage) return;
        settings.setValue("app/language", nextLanguage);
        settings.sync();

        QMessageBox prompt(QMessageBox::Information, tr("Change language"),
            tr("Restart Bimeron to apply the new language."), QMessageBox::NoButton, this);
        auto *restart = prompt.addButton(tr("Restart"), QMessageBox::AcceptRole);
        prompt.addButton(tr("Cancel"), QMessageBox::RejectRole);
        prompt.exec();
        if (prompt.clickedButton() == restart) {
            restartApplication();
            return;
        }

        if (hadOverride) settings.setValue("app/language", previousLanguage);
        else settings.remove("app/language");
        const QSignalBlocker blocker(language);
        language->setCurrentIndex(language->findData(previousLanguage));
    });
    auto *stopOnExit = new HiDpiCheckBox(tr("Disconnect and stop core when quitting"));
    stopOnExit->setChecked(QSettings().value("app/stopCoreOnExit", false).toBool());
    connect(stopOnExit, &QCheckBox::toggled, this, [](bool checked) {
        QSettings().setValue("app/stopCoreOnExit", checked);
    });
    general->addWidget(stopOnExit);
    auto *notifications = new HiDpiCheckBox(tr("Show system notifications"));
    notifications->setChecked(QSettings().value("app/notifications", true).toBool());
    connect(notifications, &QCheckBox::toggled, this, [](bool checked) { QSettings().setValue("app/notifications", checked); });
    general->addWidget(notifications);
    auto *detailedStats = new HiDpiCheckBox(tr("Show detailed connection statistics"));
    detailedStats->setChecked(QSettings().value("app/showDetailedStats", false).toBool());
    connect(detailedStats, &QCheckBox::toggled, this, [this](bool checked) {
        QSettings().setValue("app/showDetailedStats", checked);
        sessionStatsLabel_->setVisible(checked);
    });
    general->addWidget(detailedStats);
    auto *autoConnect = new HiDpiCheckBox(tr("Connect automatically on startup"));
    autoConnect->setChecked(QSettings().value("app/autoConnect", false).toBool());
    autoConnectProfile_ = new HiDpiComboBox;
    autoConnectProfile_->setEnabled(autoConnect->isChecked());
    connect(autoConnect, &QCheckBox::toggled, this, [this](bool checked) {
        QSettings().setValue("app/autoConnect", checked);
        autoConnectProfile_->setEnabled(checked);
        if (checked && autoConnectProfile_->currentIndex() >= 0)
            QSettings().setValue("app/autoConnectProfile", autoConnectProfile_->currentData().toString());
    });
    connect(autoConnectProfile_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0) QSettings().setValue("app/autoConnectProfile", autoConnectProfile_->itemData(index).toString());
    });
    auto *autoConnectRow = new QHBoxLayout;
    autoConnectRow->setSpacing(10);
    autoConnectRow->addWidget(autoConnect);
    autoConnectRow->addWidget(autoConnectProfile_, 1);
    general->addLayout(autoConnectRow);
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
#ifdef Q_OS_WIN
    QSettings runKey("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    auto *startOnLogin = new HiDpiCheckBox(tr("Start Bimeron with Windows"));
    startOnLogin->setChecked(runKey.contains("Bimeron"));
#else
    const QString autostartPath = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
        .filePath(QStringLiteral("autostart/bimeron.desktop"));
    QFile existingAutostart(autostartPath);
    if (existingAutostart.open(QIODevice::ReadOnly)) {
        const QByteArray entry = existingAutostart.readAll();
        if (entry.startsWith("[Desktop Entry]\nType=Application\nName=Bimeron\nExec=")
            && !entry.contains("--autostart"))
            applyStartupSetting(true);
    }
    auto *startOnLogin = new HiDpiCheckBox(tr("Start Bimeron with Linux"));
    startOnLogin->setChecked(QFileInfo::exists(autostartPath));
#endif
    connect(startOnLogin, &QCheckBox::toggled, this, &MainWindow::applyStartupSetting);
    general->addWidget(startOnLogin);
#endif
    layout->addWidget(generalCard);

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    auto *serviceCard = new QFrame;
    serviceCard->setObjectName("settingsCard");
    auto *service = new QVBoxLayout(serviceCard);
    service->setContentsMargins(14, 12, 14, 12);
    service->setSpacing(8);
    auto *serviceHeader = new QHBoxLayout;
    auto *serviceTitle = new QLabel(tr("Bimeron service"));
    serviceTitle->setObjectName("sectionTitle");
    serviceStatusLabel_ = new QLabel(tr("Service: checking…"));
    serviceStatusLabel_->setObjectName("sectionHint");
    serviceHeader->addWidget(serviceTitle);
    serviceHeader->addStretch();
    serviceHeader->addWidget(serviceStatusLabel_);
    service->addLayout(serviceHeader);
    auto *serviceButtons = new QHBoxLayout;
    serviceInstallButton_ = new QPushButton(tr("Install"));
    serviceAutomaticCheck_ = new HiDpiCheckBox(tr("Start the service at system boot"));
    serviceAutomaticCheck_->setChecked(true);
    serviceStartButton_ = new QPushButton(tr("Start"));
    serviceStopButton_ = new QPushButton(tr("Stop"));
    serviceUpdateButton_ = new QPushButton(tr("Check for core update"));
    serviceUninstallButton_ = new QPushButton(tr("Remove"));
    for (auto *button : {serviceInstallButton_, serviceStartButton_, serviceStopButton_, serviceUpdateButton_, serviceUninstallButton_})
        button->hide();
    serviceAutomaticCheck_->hide();
    connect(serviceInstallButton_, &QPushButton::clicked, this, [this] {
        QStringList command{"service", "install"};
        if (serviceAutomaticCheck_->isChecked()) command << "-automatic";
#ifdef Q_OS_LINUX
        command << "-owner" << QString::number(getuid());
#endif
        runServiceCommand(command, tr("Installing service"));
    });
    connect(serviceStartButton_, &QPushButton::clicked, this, [this] { runServiceCommand({"service", "start"}, tr("Starting service")); });
    connect(serviceStopButton_, &QPushButton::clicked, this, [this] { runServiceCommand({"service", "stop"}, tr("Stopping service")); });
    connect(serviceUpdateButton_, &QPushButton::clicked, this, [this] {
        if (!coreUpdatePrerequisite()) return;
        updater_.check(runningCoreVersion_, 1, betaUpdatesCheck_->isChecked());
    });
    connect(serviceUninstallButton_, &QPushButton::clicked, this, [this] {
        if (QMessageBox::warning(this, tr("Remove service"), tr("The service must be stopped before removal. Remove the installed service and its files?"),
            QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Ok)
            runServiceCommand({"service", "uninstall"}, tr("Removing service"));
    });
    for (auto *button : {serviceInstallButton_, serviceStartButton_, serviceStopButton_, serviceUpdateButton_, serviceUninstallButton_}) serviceButtons->addWidget(button);
    serviceButtons->addStretch();
    service->addLayout(serviceButtons);
    coreUpdateLabel_ = new QLabel(updater_.available() ? tr("Core updates have not been checked") : tr("Signed updates are not available yet"));
    coreUpdateLabel_->setObjectName("sectionHint");
    coreUpdateLabel_->setWordWrap(true);
    service->addWidget(coreUpdateLabel_);
    connect(&updater_, &CoreUpdater::statusChanged, coreUpdateLabel_, &QLabel::setText);
    connect(&updater_, &CoreUpdater::ready, this, [this](const QString &version, const QString &path, const QString &digest) {
        if (!coreUpdatePrerequisite()) { updater_.discardStaged(); return; }
        if (!helloReady_ || statusNeeded_ || !operation_.isEmpty() || state_ == "stopping" || cleanupBlocked_) {
            updater_.discardStaged();
            return;
        }
        const QString confirmedInstance = instance_;
        const QString confirmedSession = sessionId_;
        const bool sessionActive = state_ == "connected" || state_ == "connecting" || state_ == "reconnecting";
        const QString warning = sessionActive
            ? (activeProxyMode() ? tr("Installing Core %1 will interrupt the active proxy. Bimeron will reconnect the same profile after a successful update. Update now?")
                                 : tr("Installing Core %1 will interrupt the active VPN. Bimeron will reconnect the same profile after a successful update. Update now?")).arg(version)
            : tr("Core %1 is downloaded and verified. Update the service now?").arg(version);
        if (QMessageBox::question(this, tr("Core update ready"), warning,
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
            updater_.discardStaged();
            return;
        }
        if (!helloReady_ || statusNeeded_ || !operation_.isEmpty() || cleanupBlocked_
            || instance_ != confirmedInstance || sessionId_ != confirmedSession) {
            updater_.discardStaged();
            return;
        }
        autoConnectAttempted_ = true;
        reconnectProfileAfterUpdate_ = sessionActive ? activeProfileId_ : QString{};
        expectedCoreVersionAfterUpdate_ = version;
        updater_.setInstalling(true);
        coreUpdateCommand_ = true;
        const QStringList arguments = TrustedCore::updateArguments(path, version, digest);
        if (arguments.isEmpty() || !runServiceCommand(arguments, tr("Updating service"))) {
            coreUpdateCommand_ = false;
            reconnectProfileAfterUpdate_.clear();
            expectedCoreVersionAfterUpdate_.clear();
            updater_.setInstalling(false);
            updater_.discardStaged();
            if (coreUpdateLabel_->text() != tr("Update the GUI package to enable safe core updates") &&
                coreUpdateLabel_->text() != tr("Core updates require a protected system installation of the GUI package"))
                coreUpdateLabel_->setText(tr("Core update canceled or could not start"));
        }
    });
    QSettings().remove("app/coreBetaUpdates");
    betaUpdatesCheck_ = new HiDpiCheckBox(tr("Include beta core updates"));
    service->addWidget(betaUpdatesCheck_);
    coreUpdateLabel_->hide();
    betaUpdatesCheck_->hide();
    service->addWidget(serviceAutomaticCheck_);
    serviceRecoveryButton_ = new QPushButton(tr("Emergency network recovery"));
    serviceRecoveryButton_->setObjectName("dangerButton");
    serviceRecoveryButton_->hide();
    connect(serviceRecoveryButton_, &QPushButton::clicked, this, &MainWindow::recoverNetwork);
    serviceButtons->addWidget(serviceRecoveryButton_);
    layout->addWidget(serviceCard);
    QTimer::singleShot(0, this, &MainWindow::refreshServiceStatus);
#endif
    layout->addStretch();

    auto *footer = new QHBoxLayout;
    auto *diagnostics = new QPushButton(tr("Save diagnostic report…"));
    connect(diagnostics, &QPushButton::clicked, this, &MainWindow::saveDiagnostics);
    auto *quit = new QPushButton(tr("Quit Bimeron"));
    quit->setObjectName("dangerButton");
    connect(quit, &QPushButton::clicked, this, &MainWindow::requestShutdown);
    footer->addWidget(diagnostics);
    footer->addStretch();
    footer->addWidget(quit);
    layout->addLayout(footer);
    return page;
}

QWidget *MainWindow::logPage() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 16, 18, 16);
    auto *title = new QLabel(tr("Log"));
    title->setStyleSheet("font-size: 17px; font-weight: 700");
    layout->addWidget(title);
    auto *keepPrevious = new HiDpiCheckBox(tr("Keep previous entries"));
    keepPrevious->setChecked(QSettings().value("logs/keepPrevious", true).toBool());
    connect(keepPrevious, &QCheckBox::toggled, this, [](bool checked) {
        QSettings().setValue("logs/keepPrevious", checked);
    });
    layout->addWidget(keepPrevious);
    log_ = new QTextEdit;
    log_->setReadOnly(true);
    layout->addWidget(log_);
    auto *logButtons = new QHBoxLayout;
    auto *save = new QPushButton(tr("Save log…"));
    auto *clear = new QPushButton(tr("Clear"));
    const auto updateLogActions = [this, save, clear] {
        const bool hasEntries = !log_->document()->isEmpty();
        save->setEnabled(hasEntries);
        clear->setEnabled(hasEntries);
    };
    connect(log_, &QTextEdit::textChanged, this, updateLogActions);
    updateLogActions();
    connect(save, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, tr("Save log"), QDir::homePath() + "/bimeron.log", tr("Text (*.log *.txt)"));
        if (path.isEmpty()) return;
        QSaveFile file(path);
        const QByteArray data = log_->toPlainText().toUtf8();
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
            QMessageBox::warning(this, tr("Log"), tr("Failed to save the log: %1").arg(file.errorString()));
    });
    connect(clear, &QPushButton::clicked, log_, &QTextEdit::clear);
    logButtons->addWidget(save);
    logButtons->addWidget(clear);
    logButtons->addStretch();
    layout->addLayout(logButtons);
    return page;
}

QWidget *MainWindow::aboutPage() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 16, 18, 16);
    auto *title = new QLabel(tr("About"));
    title->setStyleSheet("font-size: 17px; font-weight: 700");
    layout->addWidget(title);
    desktopVersionLabel_ = new QLabel(tr("Bimeron Desktop %1").arg(BIMERON_GUI_VERSION));
    desktopVersionLabel_->setObjectName("desktopVersion");
    desktopVersionLabel_->installEventFilter(this);
    layout->addWidget(desktopVersionLabel_);
    coreVersionLabel_ = new QLabel(tr("Core: unknown"));
    layout->addWidget(coreVersionLabel_);
    layout->addStretch();
    return page;
}

ProfileConnectionInfo MainWindow::profileInfo(int row) const {
    if (row < 0 || row >= profiles_.size()) return {};
    QFile file(profiles_[row].path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return ProfileStore::connectionInfo(QString::fromUtf8(file.read(64 * 1024)));
}

bool MainWindow::activeProxyMode() const {
    if (sessionMode_ == "proxy" || sessionMode_ == "vpn") return sessionMode_ == "proxy";
    for (int row = 0; row < profiles_.size(); ++row)
        if (profiles_[row].id == activeProfileId_)
            return profileInfo(row).interfaceMode == "proxy";
    return lastProxyMode_;
}

void MainWindow::refreshProfiles() {
    QString error;
    profiles_ = store_.load(&error);
    profileList_->clear();
    for (int row = 0; row < profiles_.size(); ++row) {
        const auto &profile = profiles_[row];
        const auto info = profileInfo(row);
        const QString interfaceName = info.interfaceMode == "proxy" ? tr("Local SOCKS5") : tr("System VPN");
        const QString detail = info.backend == ProfileConnectionInfo::Backend::NativeIKEv2 ? tr("Native IKEv2 unavailable")
            : !info.connectable ? tr("Unknown protocol")
            : info.keyFile.isEmpty() ? QString("%1 → %2").arg(info.protocol, interfaceName)
            : QString("%1%2").arg(activeProfileId_ == profile.id && state_ == "connected" && !activeFileProtocol_.isEmpty()
                ? activeFileProtocol_ + QStringLiteral(" → ") : QString(), interfaceName);
        auto *item = new QListWidgetItem(statusDot(QColor("#94a3b8")), QString("%1\n%2").arg(profile.name, detail), profileList_);
        item->setData(Qt::UserRole + 1, !info.connectable && info.backend != ProfileConnectionInfo::Backend::NativeIKEv2);
        if (info.protocol.endsWith(QStringLiteral("⟳"))) item->setToolTip(tr("Dynamic profile"));
        item->setSizeHint(QSize(0, 50));
    }
    if (trayMenu_) rebuildTrayMenu();
    if (autoConnectProfile_) {
        const QString selected = QSettings().value("app/autoConnectProfile").toString();
        const QSignalBlocker blocker(autoConnectProfile_);
        autoConnectProfile_->clear();
        for (int row = 0; row < profiles_.size(); ++row)
            if (profileInfo(row).backend != ProfileConnectionInfo::Backend::NativeIKEv2)
                autoConnectProfile_->addItem(profiles_[row].name, profiles_[row].id);
        const int index = autoConnectProfile_->findData(selected);
        autoConnectProfile_->setCurrentIndex(index);
    }
    updateProfileAppearance();
    if (!error.isEmpty()) appendLog(tr("Failed to load profiles: %1").arg(error));
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == desktopVersionLabel_ && !developerMode_ && developmentClicks_ < 5
        && event->type() == QEvent::MouseButtonRelease) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton && desktopVersionLabel_->rect().contains(mouse->position().toPoint())
            && ++developmentClicks_ == 5)
            QTimer::singleShot(0, this, [this] { restartApplication(true); });
    }
    if (profileList_ && watched == profileList_->viewport() && event->type() == QEvent::MouseButtonPress) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton && !profileList_->itemAt(mouse->position().toPoint())) {
            profileList_->clearSelection();
            profileList_->setCurrentItem(nullptr);
        }
    }
    if (profileList_ && watched == profileList_) {
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Right) {
            editProfile(profileList_->currentRow());
            return true;
        }
        if (event->type() == QEvent::FocusIn && profileList_->currentRow() >= 0
            && profileList_->currentItem()->isSelected()) {
            const int row = profileList_->currentRow();
            const QRect rect = profileList_->visualItemRect(profileList_->item(row));
            gearRow_ = row;
            profileGear_->move(rect.right() - profileGear_->width() - 14,
                               rect.center().y() - profileGear_->height() / 2);
            profileGear_->show();
            profileGear_->raise();
        } else if (event->type() == QEvent::FocusOut) {
            profileGear_->hide();
            gearRow_ = -1;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::editProfile(int row) {
    if (row < 0 || row >= profiles_.size())
        return;
    QFile source(profiles_[row].path);
    if (!source.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Profile could not be opened"), source.errorString());
        return;
    }
    const QByteArray originalData = source.readAll();
    source.close();
    const auto validateDraft = [this, row](const QString &text, QString *error) {
        const QByteArray data = text.toUtf8();
        const QFileInfo profileInfo(profiles_[row].path);
        const QString temporary = profileInfo.dir().filePath(QString(".bimeron-validate-%1.conf").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
        if (!ProfileStore::writeConfig(temporary, data, error)) return false;
        const bool valid = validateConfig(temporary, error);
        if (!QFile::remove(temporary)) {
            if (error) *error = tr("Cannot remove the protected validation draft; the original profile was retained.");
            return false;
        }
        return valid;
    };
    ProfileEditorDialog dialog(QString::fromUtf8(originalData), capabilities_, this, validateDraft);
    while (dialog.exec() == QDialog::Accepted) {
        if (dialog.configText() == QString::fromUtf8(originalData)) break;
        const QByteArray data = dialog.configText().toUtf8();
        QString error;
        if (!validateDraft(dialog.configText(), &error)) {
            QMessageBox::warning(&dialog, tr("Error"), error, QMessageBox::Ok);
            continue;
        }
        if (!ProfileStore::writeConfig(profiles_[row].path, data, &error)) {
            QMessageBox::warning(&dialog, tr("Error"), error, QMessageBox::Ok);
            continue;
        }
        profiles_[row].name = ProfileStore::displayName(profiles_[row].path);
        if (!store_.save(profiles_, &error)) {
            QMessageBox::warning(&dialog, tr("Error"),
                tr("The profile was saved, but its list entry could not be updated. Retry Save to update the list."), QMessageBox::Ok);
            continue;
        }
        const QString editedId = profiles_[row].id;
        refreshProfiles();
        for (int index = 0; index < profiles_.size(); ++index)
            if (profiles_[index].id == editedId) profileList_->setCurrentRow(index);
        if (editedId == activeProfileId_ && state_ != "disconnected")
            QMessageBox::information(this, tr("Reconnect required"), tr("Changes have been saved and will apply after disconnecting and reconnecting."));
        break;
    }
}

void MainWindow::addProfile() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add profile"));
    dialog.setWindowFlag(Qt::MSWindowsFixedSizeDialogHint);
    dialog.setMinimumWidth(440);
    auto *layout = new QVBoxLayout(&dialog);
    auto *hint = new QLabel(tr("Enter a supported protocol URI"));
    auto *hintRow = new QHBoxLayout;
    auto *uriHelp = new QToolButton(&dialog);
    uriHelp->setText("?");
    uriHelp->setAutoRaise(true);
    uriHelp->setAccessibleName(tr("Supported protocol URIs"));
    connect(uriHelp, &QToolButton::clicked, &dialog, [uriHelp] {
        QToolTip::showText(uriHelp->mapToGlobal(QPoint(0, uriHelp->height())),
            QStringLiteral("ss://, ssconf://, socks5://, vless://,\nhysteria2://, hy2://, vmess://, trojan://, tuic://,\nhttp://, https://, anytls://,\nnaive+https://, naive+quic://, masque://, ssh://"), uriHelp, uriHelp->rect());
    });
    hintRow->addWidget(hint);
    hintRow->addWidget(uriHelp);
    hintRow->addStretch();
    auto *key = new QLineEdit;
    key->setObjectName("profileUri");
    key->setPlaceholderText(tr("Protocol URI"));
    key->setMinimumHeight(30);
    auto *paste = new QPushButton;
    paste->setText(tr("Paste"));
    auto *nameLabel = new QLabel(tr("Profile name"));
    auto *name = new QLineEdit;
    name->setObjectName("profileName");
    name->setMinimumHeight(30);
    auto *sshFiles = new QWidget;
    auto *sshForm = new QFormLayout(sshFiles);
    auto *identity = new QLineEdit;
    auto *hosts = new QLineEdit;
    auto *identityBrowse = new QPushButton(tr("Browse…"));
    auto *hostsBrowse = new QPushButton(tr("Browse…"));
    auto *identityRow = new QWidget;
    auto *identityLayout = new QHBoxLayout(identityRow);
    identityLayout->setContentsMargins(0, 0, 0, 0);
    identityLayout->addWidget(identity);
    identityLayout->addWidget(identityBrowse);
    auto *hostsRow = new QWidget;
    auto *hostsLayout = new QHBoxLayout(hostsRow);
    hostsLayout->setContentsMargins(0, 0, 0, 0);
    hostsLayout->addWidget(hosts);
    hostsLayout->addWidget(hostsBrowse);
    sshForm->addRow(tr("Identity file (optional)"), identityRow);
    sshForm->addRow(tr("Known hosts file"), hostsRow);
    sshFiles->hide();
    connect(identityBrowse, &QPushButton::clicked, &dialog, [&] {
        const QString path = QFileDialog::getOpenFileName(&dialog, tr("SSH identity file"));
        if (!path.isEmpty()) identity->setText(QFileInfo(path).absoluteFilePath());
    });
    connect(hostsBrowse, &QPushButton::clicked, &dialog, [&] {
        const QString path = QFileDialog::getOpenFileName(&dialog, tr("SSH known_hosts file"));
        if (!path.isEmpty()) hosts->setText(QFileInfo(path).absoluteFilePath());
    });
    layout->addWidget(nameLabel);
    layout->addWidget(name);
    nameLabel->hide();
    name->hide();
    layout->addLayout(hintRow);
    layout->addWidget(key);
    layout->addWidget(paste);
    layout->addWidget(sshFiles);
    QDialogButtonBox buttons(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons.button(QDialogButtonBox::Save)->setText(tr("Save"));
    buttons.button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    buttons.button(QDialogButtonBox::Save)->setEnabled(false);
    layout->addWidget(&buttons);
    layout->activate();
    dialog.setFixedWidth(440);
    dialog.setFixedHeight(dialog.sizeHint().height());
    auto keyName = [](const QString &value) {
        if (!QStringList{"SS", "SSConf", "VLESS", "Hysteria2"}.contains(ProfileStore::uriSourceField(value))) return QString();
        const qsizetype hash = value.indexOf('#');
        if (hash < 0 || hash + 1 >= value.size()) return QString();
        return QUrl::fromPercentEncoding(value.mid(hash + 1).toUtf8()).trimmed();
    };
    auto isKey = [](const QString &value) {
        const QString trimmed = value.trimmed();
        if (trimmed.contains(QRegularExpression("\\s"))) return false;
        if (QStringList{"VMess", "Trojan", "TUIC", "HTTP", "AnyTLS", "Naive", "MASQUE"}.contains(ProfileStore::uriSourceField(trimmed))) return true;
        const QUrl url(trimmed);
        if (url.scheme() == "ssconf")
            return url.isValid() && !url.host().isEmpty();
        if (url.scheme() == "ssh")
            return ProfileStore::connectionInfo("[AccessKey]\nSSH = " + trimmed).connectable;
        if (url.scheme() == "socks5") {
            if (!url.isValid() || url.host().isEmpty() || url.port() < 1 || url.port() > 65535
                || !url.path().isEmpty() || url.hasQuery() || url.hasFragment()) return false;
            const bool hasUser = !url.userName().isEmpty();
            return hasUser == !url.password().isEmpty();
        }
        if (url.scheme() == "vless")
            return url.isValid() && !url.host().isEmpty() && url.port() > 0 && url.port() <= 65535
                && url.password().isEmpty() && url.path().isEmpty()
                && QRegularExpression("^[0-9a-fA-F]{8}(-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}$").match(url.userName()).hasMatch();
        if (url.scheme() == "hysteria2" || url.scheme() == "hy2") {
            const QUrlQuery query(url);
            const bool auth = !query.queryItemValue("auth", QUrl::FullyDecoded).isEmpty();
            return url.isValid() && !url.host().isEmpty() && url.port() > 0 && url.port() <= 65535
                && (auth != !url.userName().isEmpty()) && url.password().isEmpty() && url.path().isEmpty();
        }
        if (url.scheme() != "ss") return false;

        QString body = trimmed.mid(5);
        const qsizetype suffix = body.indexOf(QRegularExpression("[?#]"));
        if (suffix >= 0) body.truncate(suffix);
        if (body.size() < 8) return false;
        if (!body.contains('@'))
            return QRegularExpression("^[A-Za-z0-9_+/=-]+$").match(body).hasMatch();

        const QUrl endpoint("ss://" + body.mid(body.lastIndexOf('@') + 1));
        return !endpoint.host().isEmpty() && endpoint.port() > 0;
    };
    bool nameEdited = false;
    bool keyValid = false;
    const auto updateSave = [&] {
        const QString uriHosts = QUrlQuery(QUrl(key->text())).queryItemValue("known_hosts", QUrl::FullyDecoded);
        const bool sshHostsReady = QFileInfo(hosts->text()).isAbsolute()
            || (hosts->text().isEmpty() && QFileInfo(uriHosts).isAbsolute());
        buttons.button(QDialogButtonBox::Save)->setEnabled(keyValid && !name->text().trimmed().isEmpty()
            && (!key->text().startsWith("ssh://") || (sshHostsReady
                && (identity->text().isEmpty() || QFileInfo(identity->text()).isAbsolute()))));
    };
    connect(name, &QLineEdit::textEdited, &dialog, [&] { nameEdited = true; });
    connect(name, &QLineEdit::textChanged, &dialog, updateSave);
    connect(hosts, &QLineEdit::textChanged, &dialog, updateSave);
    connect(identity, &QLineEdit::textChanged, &dialog, updateSave);
    connect(key, &QLineEdit::textChanged, &dialog, [&](const QString &value) {
        keyValid = isKey(value);
        nameLabel->setVisible(keyValid);
        name->setVisible(keyValid);
        sshFiles->setVisible(keyValid && value.startsWith("ssh://"));
        layout->invalidate();
        layout->activate();
        dialog.setFixedHeight(dialog.sizeHint().height());
        const QString suggested = keyName(value);
        if (!nameEdited && !suggested.isEmpty())
            name->setText(suggested);
        updateSave();
    });
    auto updatePaste = [&] {
        const QString clipboardKey = QApplication::clipboard()->text().trimmed();
        paste->setEnabled(isKey(clipboardKey));
        paste->setVisible(paste->isEnabled());
        if (paste->isEnabled()) {
            const QString suggested = keyName(clipboardKey);
            paste->setText(suggested.isEmpty() ? tr("Paste key") : tr("Paste key #%1").arg(suggested));
        } else paste->setText(tr("Paste"));
        layout->invalidate();
        layout->activate();
        dialog.setFixedHeight(dialog.sizeHint().height());
    };
    connect(QApplication::clipboard(), &QClipboard::dataChanged, &dialog, updatePaste);
    connect(paste, &QPushButton::clicked, &dialog, [&] {
        nameEdited = false;
        key->setText(QApplication::clipboard()->text().trimmed());
        key->setFocus();
    });
    updatePaste();
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (keyValid && !name->text().trimmed().isEmpty()) dialog.accept();
    });
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    while (dialog.exec() == QDialog::Accepted) {
        QString error;
        const QString path = store_.createConfig(name->text(), key->text(), &error, identity->text(), hosts->text());
        if (path.isEmpty()) {
            QMessageBox::warning(&dialog, tr("Error"), error, QMessageBox::Ok);
            continue;
        }
        QFile imported(path);
        const bool referencesReady = imported.open(QIODevice::ReadOnly)
            && ProfileStore::sshFilesReadable(QString::fromUtf8(imported.read(64 * 1024 + 1)), path, &error);
        imported.close();
        if (!referencesReady || !validateConfig(path, &error)) {
            QFile::remove(path);
            QMessageBox::warning(&dialog, tr("Error"), error, QMessageBox::Ok);
            continue;
        }
        const QString profileName = name->text().trimmed();
        profiles_.append({ProfileStore::identity(path), profileName, path});
        if (!store_.save(profiles_, &error)) {
            profiles_.removeLast();
            QFile::remove(path);
            QMessageBox::warning(&dialog, tr("Error"), error, QMessageBox::Ok);
            continue;
        }
        key->clear();
        refreshProfiles();
        break;
    }
}

void MainWindow::importProfile() {
    const QString source = QFileDialog::getOpenFileName(this, tr("Import configuration"), {}, tr("Bimeron (*.conf *.ssconf);;All files (*)"));
    if (source.isEmpty()) return;
    QString error;
    const QString path = store_.importConfig(source, &error);
    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("Import failed"), error);
        return;
    }
    if (!validateConfig(path, &error)) {
        QFile::remove(path);
        QMessageBox::warning(this, tr("Import failed"), error);
        return;
    }
    profiles_.append({ProfileStore::identity(path), ProfileStore::displayName(path), path});
    if (!store_.save(profiles_, &error)) {
        profiles_.removeLast();
        QFile::remove(path);
        QMessageBox::warning(this, tr("Import failed"), error);
        return;
    }
    refreshProfiles();
}

void MainWindow::removeProfile() {
    const int row = profileList_->currentRow();
    if (row < 0 || !profileList_->currentItem()->isSelected()) return;
    if (profiles_[row].id == activeProfileId_) {
        QMessageBox::information(this, tr("Profile in use"), tr("Disconnect the active session first."));
        return;
    }
    QMessageBox confirmation(QMessageBox::Question, tr("Delete profile"),
        tr("Delete profile “%1”?").arg(profiles_[row].name), QMessageBox::NoButton, this);
    auto *confirm = confirmation.addButton(tr("Remove"), QMessageBox::DestructiveRole);
    confirmation.addButton(tr("Cancel"), QMessageBox::RejectRole);
    confirmation.exec();
    if (confirmation.clickedButton() != confirm) return;
    QFile::remove(profiles_[row].path);
    profiles_.removeAt(row);
    QString error;
    store_.save(profiles_, &error);
    refreshProfiles();
}

void MainWindow::moveProfile(int delta) {
    const int row = profileList_->currentRow();
    const int target = row + delta;
    if (row < 0 || !profileList_->currentItem()->isSelected() || target < 0 || target >= profiles_.size()) return;
    profiles_.swapItemsAt(row, target);
    QString error;
    if (!store_.save(profiles_, &error)) {
        profiles_.swapItemsAt(row, target);
        QMessageBox::warning(this, tr("Error"), error, QMessageBox::Ok);
        return;
    }
    refreshProfiles();
    profileList_->setCurrentRow(target, QItemSelectionModel::ClearAndSelect);
    profileList_->setFocus();
}

bool MainWindow::packetProfile(int row) const {
    const auto info = profileInfo(row);
    return info.backend == ProfileConnectionInfo::Backend::Packet && info.connectable;
}

QString MainWindow::nativeUnavailableReason() const {
    return tr("Native IKEv2 connect, preview, apply and remove are unavailable. Final Core artifact approval, protected installation and MSI safety evidence, and Windows enrollment are pending. Native capabilities and owner/protection absence have not been verified. Statistics are unavailable. Automatic connection and reconnection are disabled.");
}

void MainWindow::bindNativeController(NativeIKEv2Session *controller) {
    // No production caller until protected installation approval is available.
    if (!controller || nativeOwnerBlocked_) return;
    nativeController_ = controller;
    const QPointer<NativeIKEv2Session> guarded(controller);
    connect(controller, &NativeIKEv2Session::changed, this, [this, guarded] {
        if (guarded) showNativeSnapshot(guarded->snapshot());
    });
    connect(controller, &QObject::destroyed, this, [this] {
        nativeController_.clear();
        showNativeSnapshot({}); // Disappearance never establishes cleanup.
    });
    showNativeSnapshot(controller->snapshot());
}

void MainWindow::showNativeSnapshot(const NativeIKEv2Session::Snapshot &snapshot) {
    // A snapshot never grants launch approval or absence.
    nativeOwnerBlocked_ = true;
    const auto protection = [this](const QString &value) {
        if (value == "active") return tr("Active");
        if (value == "retained") return tr("Retained");
        if (value == "modified") return tr("Modified");
        if (value == "inactive") return tr("Inactive");
        return tr("Unknown");
    };
    nativeStatusLabel_->setText(tr("Native IKEv2 protection requested: %1. WFP: %2. DNS: %3. Cleanup and owner closure require authoritative confirmation. Statistics are unavailable.")
        .arg(snapshot.requested ? tr("Yes") : tr("No"),
             protection(snapshot.authoritative ? snapshot.wfp : QString{}),
             protection(snapshot.authoritative ? snapshot.dns : QString{})));
    nativeStatusLabel_->show();
    connectionLabel_->setText(nativeStatusLabel_->text());
    if (tray_) {
        tray_->setIcon(trayIcon(false));
        tray_->setToolTip(tr("Bimeron — %1").arg(nativeStatusLabel_->text()));
    }
    if (trayStatusAction_) trayStatusAction_->setText(tr("Status: %1").arg(nativeStatusLabel_->text()));
    resetMeasurements();
    updateConnectionButton();
}

bool MainWindow::controlBlocked() const {
    return nativeOwnerBlocked_ || development_->busy() || development_->failed() || coreUpdateCommand_
        || coreUpdateAwaitingResult_ || serviceCommandProcess_ || linuxServiceProcess_ || quitRequested_;
}

bool MainWindow::cleanDisconnected() const {
    return state_ == "disconnected" && !sessionActive_ && !cleanupBlocked_;
}

bool MainWindow::canActivateProfile(int row) const {
    if (row < 0 || row >= profiles_.size() || !helloReady_ || statusNeeded_ || controlBlocked()
        || nativeSession_ || !operation_.isEmpty() || state_ == "stopping"
        || profileInfo(row).backend == ProfileConnectionInfo::Backend::NativeIKEv2) return false;
    const bool active = sessionActive_ || state_ == "connecting" || state_ == "connected" || state_ == "reconnecting";
    return (active && profiles_[row].id == activeProfileId_) || (!cleanupBlocked_ && packetProfile(row));
}

void MainWindow::probeValidationCli() {
    if (QCoreApplication::arguments().contains("--visual-check")) return;
    auto *process = new QProcess(this);
    const quint64 generation = connectionGeneration_;
    const QString executable = validationExecutable_;
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [this, process, generation, executable](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        if (generation != connectionGeneration_ || executable != validationExecutable_) return;
        validationIdentity_ = {};
        if (code == 0 && status == QProcess::NormalExit) {
            validationIdentity_ = coreIdentity(QJsonDocument::fromJson(process->readAllStandardOutput().left(16384)).object());
        }
        updateCoreIdentity();
    });
    connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) process->deleteLater();
    });
    connect(process, &QProcess::readyReadStandardError, process, [process] { process->readAllStandardError(); });
    connect(process, &QProcess::readyReadStandardOutput, process, [process] {
        if (process->bytesAvailable() > 16384) process->kill();
    });
    QTimer::singleShot(5000, process, [process] { if (process->state() != QProcess::NotRunning) process->kill(); });
    process->start(executable, {"version"});
}

void MainWindow::updateCoreIdentity() {
    const auto describe = [this](const QJsonObject &identity) {
        return QString("%1; %2; %3/%4; %5").arg(identity.value("version").toString(tr("unknown")),
            identity.value("revision").toString(tr("unknown")), identity.value("platform").toString(tr("unknown")),
            identity.value("architecture").toString(tr("unknown")), identity.contains("modified")
                ? (identity.value("modified").toBool() ? tr("modified") : tr("unmodified")) : tr("unknown"));
    };
    coreVersionLabel_->setText(tr("Core: %1").arg(runtimeIdentity_.value("version").toString(tr("unknown"))));
    coreVersionLabel_->setToolTip(tr("Validation CLI: %1\nRuntime Core: %2").arg(describe(validationIdentity_), describe(runtimeIdentity_)));
}

void MainWindow::resetMeasurements() {
    statsClock_.invalidate();
    previousUp_ = previousDown_ = 0;
    trafficLabel_->setText(tr("Traffic: —"));
    sessionStatsLabel_->setText(tr("Connections: —"));
    trayTraffic_ = trafficLabel_->text();
    if (trayTrafficAction_) trayTrafficAction_->setText(trayTraffic_);
}

void MainWindow::invalidateConnection() {
    clearSSHAuthentication();
    sshSeen_.clear();
    sshAuthSupported_ = true;
    ++connectionGeneration_;
    if (validationProcess_) {
        validationProcess_->disconnect(this);
        validationProcess_->kill();
        validationProcess_->deleteLater();
        validationProcess_ = nullptr;
    }
    core_.cancel();
    if (serviceStatusProcess_) {
        serviceStatusProcess_->disconnect(this);
        serviceStatusProcess_->kill();
        serviceStatusProcess_->deleteLater();
        serviceStatusProcess_ = nullptr;
    }
    operation_.clear();
    pendingStartPath_.clear();
    startReadyPath_.clear();
    stopRequested_ = false;
    helloReady_ = false;
    statusNeeded_ = true;
    capabilities_ = {};
    runtimeIdentity_ = {};
    runningCoreVersion_.clear();
    updateCoreIdentity();
    instance_.clear();
    sessionId_.clear();
    eventCursor_ = 0;
    nextEvents_ = nextStatus_ = nextStats_ = 0;
    resetMeasurements();
    connectionLabel_->setText(tr("Connecting to core…"));
    updateConnectionButton();
    updateProfileAppearance();
    if (tray_) tray_->setIcon(trayIcon(false));
    if (trayMenu_) rebuildTrayMenu();
}

void MainWindow::connectionFailure(const QString &, CoreClient::Failure reason, const QString &message) {
    if (nativeOwnerBlocked_) return;
    invalidateConnection();
    autoConnectAttempted_ = true;
    reconnectProfileAfterUpdate_.clear();
    if (coreUpdateAwaitingResult_ || !reconnectResultProfileId_.isEmpty()) {
        coreUpdateAwaitingResult_ = false;
        expectedCoreVersionAfterUpdate_.clear();
        reconnectResultProfileId_.clear();
        reportCoreUpdateResult(tr("Core connection could not be verified; reconnection was canceled"));
    }
    static const int delays[]{1000, 2000, 5000, 10000};
    retryAt_ = pollClock_.elapsed() + delays[qMin(retryCount_++, 3)];
    const QString text = reason == CoreClient::Failure::AccessDenied ? tr("Access to Core was denied")
        : reason == CoreClient::Failure::Incompatible ? tr("The GUI and core protocol versions are incompatible")
        : tr("Core unavailable");
    connectionLabel_->setText(text);
    connectionLabel_->setStyleSheet(themedStyle("color: #c62828; font-weight: 500"));
    if (tray_) { tray_->setIcon(trayIcon(false)); tray_->setToolTip(tr("Bimeron — %1").arg(text)); }
    if (lastCoreError_ != message) { appendLog(message); lastCoreError_ = message; }
    updateConnectionButton();
    if (trayMenu_) rebuildTrayMenu();
    if (quitRequested_) { quitRequested_ = false; if (tray_) tray_->show(); showWindow(); }
}

void MainWindow::toggleConnection() {
    if (controlBlocked() || !helloReady_ || statusNeeded_ || !operation_.isEmpty() || nativeSession_) return;
    const int row = profileList_->currentItem() && profileList_->currentItem()->isSelected()
        ? profileList_->currentRow() : -1;
    if (state_ == "unknown" || state_ == "stopping") return;
    autoConnectAttempted_ = true;
    const bool active = sessionActive_ || state_ == "connected" || state_ == "connecting"
        || state_ == "reconnecting";
    if (active) {
        pendingStartPath_.clear();
        if (row >= 0 && profiles_[row].id != activeProfileId_ && packetProfile(row) && !cleanupBlocked_)
            pendingStartPath_ = profiles_[row].path;
        operation_ = "stop";
        clearSSHAuthentication();
        stopRequested_ = true;
        updateConnectionButton();
        poll();
    } else if (row >= 0 && !cleanupBlocked_) {
        startProfile(row);
    }
}

bool MainWindow::startProfile(int row) {
    if (controlBlocked() || !helloReady_ || statusNeeded_ || !operation_.isEmpty() || cleanupBlocked_
        || nativeSession_ || sessionActive_ || (state_ != "disconnected" && state_ != "failed")) return false;
    if (row < 0 || row >= profiles_.size())
        return false;
    const auto info = profileInfo(row);
    if (!packetProfile(row)) return false;
    QFile profileFile(profiles_[row].path);
    if (!profileFile.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Profile unavailable"), profileFile.errorString());
        return false;
    }
    const QByteArray profileBytes = profileFile.read(64 * 1024 + 1);
    const QString profileText = QString::fromUtf8(profileBytes);
    if (profileBytes.size() > 64 * 1024) return false;
    if (profileText.contains(QRegularExpression(QStringLiteral("(?m)^\\s*\\[IKEv2\\]\\s*$")))) {
        QMessageBox::warning(this, tr("Feature not supported"), tr("Native IKEv2 requires a separate foreground backend"));
        return false;
    }
    activeFileProtocol_.clear();
    if (!info.keyFile.isEmpty()) {
        QFile keyFile(QFileInfo(profiles_[row].path).dir().filePath(info.keyFile));
        if (QFileInfo(info.keyFile).isAbsolute()) keyFile.setFileName(info.keyFile);
        if (keyFile.open(QIODevice::ReadOnly)) {
            QString key = QString::fromUtf8(keyFile.read(64 * 1024)).trimmed();
            if (key.startsWith(QChar::ByteOrderMark)) key.remove(0, 1);
            activeFileProtocol_ = ProfileStore::connectionInfo("[AccessKey]\nKey = " + key).protocol;
        }
    }
    QString sshFileError;
    if (!ProfileStore::sshFilesReadable(profileText, profiles_[row].path, &sshFileError)) {
        QMessageBox::warning(this, tr("SSH files unavailable"), sshFileError.isEmpty()
            ? tr("The profile dependency file is unavailable.") : sshFileError);
        return false;
    }
    QString mode = QRegularExpression(R"((?im)^\s*Mode\s*=\s*(vpn|proxy)\s*$)").match(profileText).captured(1).toLower();
    if (mode.isEmpty()) mode = "vpn";
    QString unsupported;
    if (mode == "vpn" && !capabilities_.value("vpn").toBool()) unsupported = tr("system VPN");
    else if (mode == "proxy" && !capabilities_.value("proxy").toBool()) unsupported = tr("local SOCKS5");
    else if (QRegularExpression(R"((?im)^\s*Domain\s*=)").match(profileText).hasMatch() && !capabilities_.value("domain_split").toBool()) unsupported = tr("domain rules");
    else if (QRegularExpression(R"((?im)^\s*CIDR\s*=)").match(profileText).hasMatch() && !capabilities_.value("ip_split").toBool()) unsupported = tr("IP/CIDR rules");
    else if (mode == "vpn" && !capabilities_.value("ipv6").toBool()
             && !QRegularExpression(R"((?im)^\s*IPv6\s*=\s*disable\s*$)").match(profileText).hasMatch()) unsupported = tr("IPv6");
    else if (QRegularExpression(R"((?im)^\s*KillSwitch\s*=\s*true\s*$)").match(profileText).hasMatch() && !capabilities_.value("kill_switch").toBool()) unsupported = tr("kill switch");
    if (!unsupported.isEmpty()) {
        QMessageBox::warning(this, tr("Feature not supported"), tr("The current core build does not support: %1.").arg(unsupported));
        return false;
    }
    const QString path = QFileInfo(profiles_[row].path).absoluteFilePath();
    const QString executable = validationExecutable_;
    const quint64 generation = connectionGeneration_;
    operation_ = "validate";
    updateConnectionButton();
    auto *process = new QProcess(this);
    validationProcess_ = process;
    process->setProgram(executable);
    process->setArguments({"validate", path});
    connect(process, &QProcess::readyReadStandardOutput, this, [process] { process->readAllStandardOutput(); });
    connect(process, &QProcess::readyReadStandardError, this, [process] { process->readAllStandardError(); });
    const auto finished = [this, process, path, generation, profileBytes](int code, QProcess::ExitStatus status) {
        if (validationProcess_ != process || generation != connectionGeneration_) return;
        validationProcess_ = nullptr;
        process->deleteLater();
        operation_.clear();
        QFile current(path);
        const bool unchanged = current.open(QIODevice::ReadOnly) && current.read(64 * 1024 + 1) == profileBytes;
        if (status != QProcess::NormalExit || code != 0) {
            appendLog(tr("Core rejected the configuration. Check the key and profile settings."));
            if (!reconnectResultProfileId_.isEmpty())
                reportCoreUpdateResult(tr("Core was updated, but profile validation failed; reconnection was canceled"));
            reconnectResultProfileId_.clear();
        } else if (unchanged && !controlBlocked() && helloReady_ && !statusNeeded_ && !sessionActive_
                   && !cleanupBlocked_ && (state_ == "disconnected" || state_ == "failed")) {
            for (const auto &profile : profiles_) {
                if (QFileInfo(profile.path).absoluteFilePath() == path && QFileInfo::exists(path)) {
                    startReadyPath_ = path;
                    operation_ = "start";
                    break;
                }
            }
        }
        if (startReadyPath_.isEmpty() && !reconnectResultProfileId_.isEmpty()) {
            reconnectResultProfileId_.clear();
            reportCoreUpdateResult(tr("Core was updated, but reconnection was canceled"));
        }
        updateConnectionButton();
        poll();
    };
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, finished);
    connect(process, &QProcess::errorOccurred, this, [finished](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) finished(-1, QProcess::CrashExit);
    });
    QTimer::singleShot(20000, process, [this, process, generation] {
        if (validationProcess_ == process && generation == connectionGeneration_) process->kill();
    });
    process->start();
    return true;
}

void MainWindow::poll() {
    if (development_->busy() || development_->failed() || coreUpdateCommand_
        || serviceCommandProcess_ || linuxServiceProcess_ || quitRequested_ || core_.busy()) return;
    if (nativeOwnerBlocked_) return;
    const qint64 now = pollClock_.elapsed();
    if (!helloReady_) {
        if (now >= retryAt_) core_.request("hello");
        return;
    }
    if (stopRequested_ && !nativeSession_) {
        clearSSHAuthentication();
        stopRequested_ = false;
        core_.request("stop", {}, 120000);
        return;
    }
    if (statusNeeded_) { core_.request("status"); return; }
    if (!startReadyPath_.isEmpty()) {
        const QString path = startReadyPath_;
        startReadyPath_.clear();
        QFile file(path);
        const auto info = file.open(QIODevice::ReadOnly)
            ? ProfileStore::connectionInfo(QString::fromUtf8(file.read(64 * 1024 + 1))) : ProfileConnectionInfo{};
        if (!controlBlocked() && !cleanupBlocked_ && !sessionActive_ && !nativeSession_
            && (state_ == "disconnected" || state_ == "failed")
            && info.backend == ProfileConnectionInfo::Backend::Packet && info.connectable) {
            if (!QSettings().value("logs/keepPrevious", true).toBool()) log_->clear();
            core_.request("start", {{"config", path}});
        } else {
            operation_.clear();
            if (!reconnectResultProfileId_.isEmpty()) {
                reconnectResultProfileId_.clear();
                reportCoreUpdateResult(tr("Core was updated, but reconnection was canceled"));
            }
        }
        return;
    }
    if (sshReplyReady_ && !nativeSession_) {
        sshReplyReady_ = false;
        const QJsonObject arguments{{"challenge_id", sshChallenge_}, {"answers", sshAnswers_}};
        sshAnswers_ = {};
        core_.request("auth_reply", arguments);
        return;
    }
    // Prefer the oldest due read so delayed status/events cannot starve authentication.
    if (!nativeSession_ && sshAuthSupported_ && now >= nextSSHAuth_
        && nextSSHAuth_ <= nextStatus_ && nextSSHAuth_ <= nextEvents_
        && (state_ == "connecting" || state_ == "reconnecting")) {
        nextSSHAuth_ = now + 1000;
        core_.request("auth_pending");
        return;
    }
    if (now >= nextStatus_) {
        nextStatus_ = now + 2000;
        core_.request("status");
    } else if (now >= nextEvents_) {
        nextEvents_ = now + 1000;
        core_.request("events", {{"after", static_cast<qint64>(eventCursor_)}});
    } else if (state_ == "connected" && !nativeSession_ && now >= nextStats_) {
        nextStats_ = now + 1000;
        core_.request("stats");
    }
}

void MainWindow::handleReply(const QString &command, const QJsonObject &response) {
    if (nativeOwnerBlocked_) return;
    const bool wasReady = helloReady_ && !statusNeeded_ && operation_.isEmpty();
    const QString previousSession = sessionId_;
    const QString newInstance = response.value("instance").toString();
    if (newInstance.isEmpty()) {
        connectionFailure(command, CoreClient::Failure::InvalidResponse, tr("Incompatible response from core"));
        return;
    }
    if (!instance_.isEmpty() && newInstance != instance_) {
        invalidateConnection();
        autoConnectAttempted_ = true;
        retryAt_ = 0;
        appendLog(tr("Core was restarted"));
    }
    instance_ = newInstance;
    sshAuthSupported_ = sshUnsupportedInstance_ != instance_;
    if (recoveryVerified_ && recoveredInstance_ != instance_) recoveryVerified_ = false;
    if (command != "hello" && !helloReady_) {
        if (quitRequested_) connectionFailure(command, CoreClient::Failure::InvalidResponse,
            tr("Core cleanup could not be confirmed; check Core before quitting"));
        else poll();
        return;
    }
    const QString error = response.value("error").toString();
    if (error == "unsupported_protocol") {
        connectionFailure(command, CoreClient::Failure::Incompatible, tr("The GUI and core protocol versions are incompatible"));
        return;
    }
    const auto status = response.value("status").toObject();
    if (!QStringList{"disconnected", "connecting", "connected", "reconnecting", "stopping", "failed"}
            .contains(status.value("State").toString())) {
        connectionFailure(command, CoreClient::Failure::InvalidResponse, tr("Incompatible response from core"));
        return;
    }
    if (command == "hello" && error.isEmpty()) {
        helloReady_ = true;
        statusNeeded_ = true;
        retryCount_ = 0;
        capabilities_ = response.value("capabilities").toObject();
        runtimeIdentity_ = coreIdentity(response.value("core").toObject());
        runningCoreVersion_ = runtimeIdentity_.value("version").toString();
        const QString version = runningCoreVersion_;
        updateCoreIdentity();
        if (!coreUpdateCheckStarted_ && !version.isEmpty() && updater_.available()) {
            coreUpdateCheckStarted_ = true;
            QTimer::singleShot(3000, this, [this] {
                if (coreUpdatePrerequisite()) updater_.check(runningCoreVersion_, 1, betaUpdatesCheck_->isChecked());
            });
            auto *updateTimer = new QTimer(this);
            updateTimer->setInterval(24 * 60 * 60 * 1000);
            connect(updateTimer, &QTimer::timeout, this, [this] {
                if (coreUpdatePrerequisite()) updater_.check(runningCoreVersion_, 1, betaUpdatesCheck_->isChecked());
            });
            updateTimer->start();
        }
    }
    handleStatus(status);
    if (command == "auth_pending" || command == "auth_reply") {
        if (error == "unknown_command") {
            sshAuthSupported_ = false;
            sshUnsupportedInstance_ = instance_;
            clearSSHAuthentication();
        } else if (!error.isEmpty()) {
            clearSSHAuthentication();
            statusNeeded_ = true;
            appendLog(tr("SSH authentication could not be completed. Check the connection state."));
        } else if (command == "auth_pending") {
            handleSSHAuthentication(response.value("auth").toObject());
        }
        return;
    }
    if (command == "status") {
        statusNeeded_ = false;
        nextStatus_ = pollClock_.elapsed() + 2000;
    }
    if (!error.isEmpty()) {
        if (command == "start" || command == "stop") {
            operation_.clear();
            startReadyPath_.clear();
            pendingStartPath_.clear();
            if (!reconnectResultProfileId_.isEmpty())
                reportCoreUpdateResult(tr("Core was updated, but reconnection failed"));
            reconnectResultProfileId_.clear();
        }
        if (command == "stop") {
            cleanupBlocked_ = true;
            autoConnectAttempted_ = true;
            if (serviceRecoveryButton_) serviceRecoveryButton_->show();
        }
        if (command == "events" && error == "invalid_cursor") {
            eventCursor_ = 0;
            statusNeeded_ = true;
        } else {
            if (command == "hello") {
                connectionFailure(command, CoreClient::Failure::InvalidResponse, tr("Incompatible response from core"));
                return;
            }
            if (command == "start" || command == "stop" || command == "shutdown") showCoreError(response);
            else if (lastCoreError_ != error) { appendLog(safeCoreCode(error)); lastCoreError_ = error; }
            if (command != "status") statusNeeded_ = true;
            if (command == "shutdown") { quitRequested_ = false; if (tray_) tray_->show(); showWindow(); }
        }
    } else if (command == "shutdown") {
        if (cleanDisconnected()) {
            if (tray_) tray_->hide();
            QApplication::quit();
        } else {
            quitRequested_ = false;
            if (tray_) tray_->show();
            showWindow();
            appendLog(tr("Core cleanup could not be confirmed; check Core before quitting"));
        }
    } else if (command == "start") {
        operation_.clear();
        statusNeeded_ = true;
    } else if (command == "stop") {
        operation_.clear();
        stopRequested_ = false;
        const QString path = pendingStartPath_;
        pendingStartPath_.clear();
        if (cleanDisconnected() && !path.isEmpty()) {
            for (int row = 0; row < profiles_.size(); ++row) {
                if (profiles_[row].path == path) {
                    profileList_->setCurrentRow(row);
                    startProfile(row);
                    break;
                }
            }
        }
    } else if (command == "stats" && state_ == "connected" && !nativeSession_) {
        if (sessionId_ != previousSession) { resetMeasurements(); return; }
        const auto stats = response.value("stats").toObject();
        if (stats.value("unavailable").toBool() || !stats.value("bytes_up").isDouble()
            || !stats.value("bytes_down").isDouble()) { resetMeasurements(); return; }
        const qint64 up = stats.value("bytes_up").toInteger(-1);
        const qint64 down = stats.value("bytes_down").toInteger(-1);
        if (up < 0 || down < 0) { resetMeasurements(); return; }
        const bool baseline = statsClock_.isValid() && up >= previousUp_ && down >= previousDown_;
        const double seconds = statsClock_.isValid() ? statsClock_.restart() / 1000.0 : 0;
        if (!statsClock_.isValid()) statsClock_.start();
        const double rate = baseline && seconds > 0
            ? ((double(up) - previousUp_) + (double(down) - previousDown_)) / seconds : 0;
        previousUp_ = up; previousDown_ = down;
        const QString speed = baseline && seconds > 0
            ? humanBytes(static_cast<qint64>(qMin(rate, double(std::numeric_limits<qint64>::max() / 2)))) + "/s" : QStringLiteral("—");
        trafficLabel_->setText(tr("Traffic: ↑ %1  ↓ %2  ·  %3").arg(humanBytes(up), humanBytes(down), speed));
        sessionStatsLabel_->setText((activeProxyMode()
            ? tr("TCP: %1 active / %2 total / %3 errors  ·  UDP: %4 / %5 / %6  ·  DNS: %7 requests / %8 errors  ·  Routes: proxy %9, direct %10, blocked %11")
            : tr("TCP: %1 active / %2 total / %3 errors  ·  UDP: %4 / %5 / %6  ·  DNS: %7 requests / %8 errors  ·  Routes: VPN %9, direct %10, blocked %11"))
            .arg(stats.value("tcp_active").toInteger()).arg(stats.value("tcp_total").toInteger()).arg(stats.value("tcp_failed").toInteger())
            .arg(stats.value("udp_active").toInteger()).arg(stats.value("udp_total").toInteger()).arg(stats.value("udp_failed").toInteger())
            .arg(stats.value("dns_queries").toInteger()).arg(stats.value("dns_failed").toInteger())
            .arg(stats.value("routed_proxy").toInteger()).arg(stats.value("routed_direct").toInteger()).arg(stats.value("routed_block").toInteger()));
        trayTraffic_ = trafficLabel_->text();
        if (trayTrafficAction_)
            trayTrafficAction_->setText(trayTraffic_);
    } else if (command == "events") {
        const qint64 cursor = response.value("cursor").toInteger();
        if (cursor >= 0) eventCursor_ = static_cast<quint64>(cursor);
        if (response.value("dropped").toBool()) {
            appendLog(tr("Some events were missed; reloading state from Core"));
            statusNeeded_ = true;
        }
        for (const auto &item : response.value("events").toArray()) {
            const auto event = item.toObject().value("event").toObject();
            appendLog(safeCoreEvent(event.value("Code").toString(), event.value("Message").toString()));
        }
    }
    if (command == "status" && error.isEmpty()) completeSynchronization();
    updateConnectionButton();
    if (trayMenu_ && wasReady != (helloReady_ && !statusNeeded_ && operation_.isEmpty())) rebuildTrayMenu();
    if (statusNeeded_ || stopRequested_ || !startReadyPath_.isEmpty())
        QTimer::singleShot(0, this, &MainWindow::poll);
}

void MainWindow::completeSynchronization() {
    if (coreUpdateAwaitingResult_ && !coreUpdateCommand_ && !expectedCoreVersionAfterUpdate_.isEmpty()) {
        const QString profileId = reconnectProfileAfterUpdate_;
        reconnectResultProfileId_.clear();
        const bool verified = runningCoreVersion_ == expectedCoreVersionAfterUpdate_ && cleanDisconnected();
        coreUpdateAwaitingResult_ = false;
        expectedCoreVersionAfterUpdate_.clear();
        reconnectProfileAfterUpdate_.clear();
        if (!verified) {
            reportCoreUpdateResult(activeProxyMode() ? tr("Core update could not be verified; proxy was not reconnected") : tr("Core update could not be verified; VPN was not reconnected"));
        } else if (profileId.isEmpty()) {
            reportCoreUpdateResult(tr("Core %1 installed and verified; connection remains disconnected").arg(runningCoreVersion_));
        } else {
            bool found = false;
            for (int row = 0; row < profiles_.size(); ++row) {
                if (profiles_[row].id == profileId && packetProfile(row)) {
                    found = true;
                    reconnectResultProfileId_ = profileId;
                    coreUpdateLabel_->setText((activeProxyMode() ? tr("Core %1 installed and verified; reconnecting proxy…") : tr("Core %1 installed and verified; reconnecting VPN…")).arg(runningCoreVersion_));
                    profileList_->setCurrentRow(row);
                    if (!startProfile(row)) {
                        reconnectResultProfileId_.clear();
                        reportCoreUpdateResult(tr("Core was updated, but reconnection was canceled"));
                    }
                    break;
                }
            }
            if (!found)
                reportCoreUpdateResult(tr("Core %1 installed, but the previous profile is unavailable").arg(runningCoreVersion_));
        }
    }
    if (!autoConnectAttempted_) {
        autoConnectAttempted_ = true;
        if (cleanDisconnected() && QSettings().value("app/autoConnect", false).toBool()) {
            const QString wanted = QSettings().value("app/autoConnectProfile").toString();
            for (int row = 0; row < profiles_.size(); ++row)
                if (profiles_[row].id == wanted && packetProfile(row)) { profileList_->setCurrentRow(row); startProfile(row); break; }
        }
    }
}

void MainWindow::handleStatus(const QJsonObject &status) {
    if (nativeOwnerBlocked_) return;
    const QString previousState = state_;
    const QString previousProfileId = activeProfileId_;
    lastProxyMode_ = activeProxyMode();
    state_ = status.value("State").toString("unknown");
    if (state_ != "connected" && state_ != "connecting" && state_ != "reconnecting") activeFileProtocol_.clear();
    if (state_ == "connecting" || state_ == "connected" || state_ == "reconnecting")
        connectionAttempted_ = true;
    else if (state_ == "disconnected")
        connectionAttempted_ = false;
    const auto session = status.value("Session").toObject();
    sessionActive_ = session.value("active").toBool();
    sessionMode_ = session.value("mode").toString();
    if (recoveryVerified_ && session.value("id").toString() != recoveredSession_) recoveryVerified_ = false;
    nativeSession_ = session.value("backend").toString() == "ikev2";
    if (sessionActive_ && !startReadyPath_.isEmpty()) {
        startReadyPath_.clear();
        operation_.clear();
    }
    if (status.value("LastError").toString().endsWith("cleanup_failed_run_recover") && !recoveryVerified_) {
        cleanupBlocked_ = true;
        autoConnectAttempted_ = true;
        pendingStartPath_.clear();
        if (serviceRecoveryButton_) serviceRecoveryButton_->show();
    }
    activeProfileId_ = session.value("profile_id").toString();
    if (!reconnectResultProfileId_.isEmpty()) {
        if (state_ == "connected" && activeProfileId_ == reconnectResultProfileId_) {
            reconnectResultProfileId_.clear();
            reportCoreUpdateResult((activeProxyMode() ? tr("Core %1 installed and verified; proxy reconnected") : tr("Core %1 installed and verified; VPN reconnected")).arg(runningCoreVersion_));
        } else if (state_ == "failed" || (state_ == "connected" && activeProfileId_ != reconnectResultProfileId_)) {
            reconnectResultProfileId_.clear();
            reportCoreUpdateResult((activeProxyMode() ? tr("Core %1 installed, but proxy reconnection failed") : tr("Core %1 installed, but VPN reconnection failed")).arg(runningCoreVersion_));
        }
    }
    if (trayMenu_ && (state_ != previousState || activeProfileId_ != previousProfileId))
        rebuildTrayMenu();
    const QString newSession = session.value("id").toString();
    if (newSession != sessionId_) {
        clearSSHAuthentication();
        sshSeen_.clear();
        sessionId_ = newSession;
        resetMeasurements();
    }
    if ((state_ != "connecting" && state_ != "reconnecting") || nativeSession_) clearSSHAuthentication();
    const QHash<QString, QString> labels{{"disconnected", tr("Disconnected")}, {"connecting", tr("Connecting…")}, {"connected", tr("Connected")}, {"reconnecting", tr("Reconnecting…")}, {"stopping", tr("Disconnecting…")}, {"failed", tr("Connection failed")}};
    QString connectionText = labels.value(state_, tr("Unknown state"));
    if (state_ != "connected" || nativeSession_) resetMeasurements();
    if (state_ == "connected") {
        const auto displayEndpoint = [](QString endpoint) {
            static const QRegularExpression mappedIpv4(QStringLiteral(R"(^\[::ffff:(\d{1,3}(?:\.\d{1,3}){3})\]:(\d+)$)"), QRegularExpression::CaseInsensitiveOption);
            const auto match = mappedIpv4.match(endpoint);
            return match.hasMatch() ? QString("%1:%2").arg(match.captured(1), match.captured(2)) : endpoint;
        };
        const QString tcp = displayEndpoint(session.value("server_tcp").toString());
        const QString udp = displayEndpoint(session.value("server_udp").toString());
        const QString tcpName = session.value("server_tcp_name").toString();
        const QString udpName = session.value("server_udp_name").toString();
        if (!tcp.isEmpty()) {
            connectionText += " · " + tcp;
            if (!tcpName.isEmpty()) connectionText += " (" + tcpName + ")";
            if (!udp.isEmpty() && udp != tcp)
                connectionText += tr(" (TCP) · %1%2 (UDP)").arg(udp, udpName.isEmpty() ? QString() : " (" + udpName + ")");
        }
    }
    connectionLabel_->setTextFormat(Qt::PlainText);
    connectionLabel_->setWordWrap(true);
    connectionLabel_->setText(connectionText);
    if (state_ == "connected")
        connectionLabel_->setStyleSheet(themedStyle("color: #16803c; font-weight: 500"));
    else if (state_ == "connecting" || state_ == "reconnecting" || state_ == "stopping")
        connectionLabel_->setStyleSheet(themedStyle("color: #d69200; font-weight: 500"));
    else if (state_ == "failed")
        connectionLabel_->setStyleSheet(themedStyle("color: #c62828; font-weight: 500"));
    else
        connectionLabel_->setStyleSheet(themedStyle("color: #66768a"));
    updateProfileAppearance();
    updateConnectionButton();
    if (tray_) {
        tray_->setIcon(trayIcon(state_ == "connected"));
        tray_->setToolTip(tr("Bimeron — %1").arg(connectionLabel_->text()));
        if (trayStatusAction_)
            trayStatusAction_->setText(tr("Status: %1").arg(connectionLabel_->text()));
        const bool notifications = QSettings().value("app/notifications", true).toBool();
        if (notifications && state_ == "connected" && previousState != "connected")
            tray_->showMessage(tr("Bimeron"), activeProxyMode() ? tr("Proxy connected") : tr("VPN connected"), QSystemTrayIcon::Information, 3000);
        else if (notifications && state_ == "failed" && connectionAttempted_ && previousState != "failed")
            tray_->showMessage(tr("Bimeron"), activeProxyMode() ? tr("Failed to connect the proxy") : tr("Failed to connect the VPN"), QSystemTrayIcon::Critical, 4000);
        else if (notifications && state_ == "disconnected" && previousState != "disconnected" && previousState != "unknown")
            tray_->showMessage(tr("Bimeron"), activeProxyMode() ? tr("Proxy disconnected") : tr("VPN disconnected"), QSystemTrayIcon::Information, 3000);
    }
    const QString lastError = status.value("LastError").toString();
    if (lastError != lastCoreError_) {
        lastCoreError_ = lastError;
        if (!lastError.isEmpty()) appendLog(tr("Core: %1").arg(safeCoreCode(lastError)));
    }
    if (state_ == "failed" && previousState != "failed" && lastError == "access_key_unavailable")
        QMessageBox::warning(this, tr("Key unavailable"),
            tr("Access with this key is no longer available: it has been exhausted or revoked. Replace the key in the profile and reconnect."));
}

void MainWindow::updateConnectionButton() {
    const int row = profileList_->currentRow();
    const bool active = sessionActive_ || state_ == "connecting" || state_ == "connected" || state_ == "reconnecting";
    const bool selected = row >= 0 && row < profiles_.size() && profileList_->currentItem()->isSelected();
    const bool nativeSelected = selected && profileInfo(row).backend == ProfileConnectionInfo::Backend::NativeIKEv2;
    if (nativeStatusLabel_ && !nativeOwnerBlocked_) {
        nativeStatusLabel_->setVisible(nativeSelected);
        if (nativeSelected) nativeStatusLabel_->setText(nativeUnavailableReason());
    }
    connectButton_->setToolTip(nativeSelected ? nativeUnavailableReason() : QString{});
    const bool switchProfile = active && selected && profiles_[row].id != activeProfileId_
        && packetProfile(row) && !cleanupBlocked_;
    connectButton_->setText(!operation_.isEmpty() ? tr("Please wait…")
        : state_ == "stopping" ? tr("Disconnecting…")
        : active && !switchProfile ? tr("Disconnect") : tr("Connect"));
    const QString mode = active && !switchProfile ? "disconnect" : "connect";
    if (connectButton_->property("mode").toString() != mode) {
        connectButton_->setProperty("mode", mode);
        connectButton_->style()->unpolish(connectButton_);
        connectButton_->style()->polish(connectButton_);
    }
    connectButton_->setEnabled(helloReady_ && !statusNeeded_ && !controlBlocked() && !nativeSession_
        && operation_.isEmpty() && state_ != "stopping" && state_ != "unknown"
        && ((active && !switchProfile) || (selected && canActivateProfile(row))));
    if (trayMenu_) {
        for (auto *action : trayMenu_->actions())
            if (action->data().isValid()) action->setEnabled(canActivateProfile(action->data().toInt()));
    }
}

void MainWindow::updateProfileAppearance() {
    for (int row = 0; row < profileList_->count() && row < profiles_.size(); ++row) {
        auto *item = profileList_->item(row);
        const auto info = profileInfo(row);
        const QString interfaceName = info.interfaceMode == "proxy" ? tr("Local SOCKS5") : tr("System VPN");
        const QString detail = info.backend == ProfileConnectionInfo::Backend::NativeIKEv2 ? tr("Native IKEv2 unavailable")
            : !info.connectable ? tr("Unknown protocol")
            : info.keyFile.isEmpty() ? QString("%1 → %2").arg(info.protocol, interfaceName)
            : QString("%1%2").arg(profiles_[row].id == activeProfileId_ && state_ == "connected" && !activeFileProtocol_.isEmpty()
                ? activeFileProtocol_ + QStringLiteral(" → ") : QString(), interfaceName);
        item->setText(QString("%1\n%2").arg(profiles_[row].name, detail));
        item->setBackground(QColor(appDarkMode() ? "#202937" : "#ffffff"));
        QString color = appDarkMode() ? "#e8edf5" : "#152238";
        QString dotColor = "#94a3b8";
        if (helloReady_ && profiles_[row].id == activeProfileId_ && state_ == "connected") {
            color = appDarkMode() ? "#76d49a" : "#16803c";
            dotColor = "#19c15f";
            item->setBackground(QColor(appDarkMode() ? "#1d4235" : "#e8f7ed"));
        } else if (helloReady_ && profiles_[row].id == activeProfileId_ && (state_ == "connecting" || state_ == "reconnecting" || state_ == "stopping")) {
            color = appDarkMode() ? "#f3cf72" : "#d69200";
            dotColor = "#f4c20d";
            item->setBackground(QColor(appDarkMode() ? "#493e23" : "#fff4bf"));
        } else if (helloReady_ && profiles_[row].id == activeProfileId_ && state_ == "failed") {
            color = appDarkMode() ? "#ff9a92" : "#c62828";
            dotColor = "#dc3545";
            item->setBackground(QColor(appDarkMode() ? "#492d35" : "#fdecec"));
        }
        item->setIcon(statusDot(QColor(dotColor)));
        item->setForeground(QColor(color));
        item->setData(Qt::UserRole + 1, !info.connectable && info.backend != ProfileConnectionInfo::Backend::NativeIKEv2);
    }
}

void MainWindow::showCoreError(const QJsonObject &response) {
    const QString code = response.value("error").toString();
    const QHash<QString, QString> messages{{"unsupported_protocol", tr("The GUI and core protocol versions are incompatible")}, {"absolute_config_path_required", tr("Core requires an absolute profile path")}, {"invalid_configuration", tr("The profile configuration is invalid")}, {"session_already_running", tr("A session is already running")}, {"session_start_failed", tr("Failed to start the session")}, {"session_stop_failed", tr("Failed to fully clean up network settings")}, {"daemon_shutting_down", tr("Core is shutting down")}};
    const QString message = messages.value(code, tr("Core error: %1").arg(safeCoreCode(code)));
    appendLog(message);
    QMessageBox::warning(this, tr("Error"), message, QMessageBox::Ok);
}

void MainWindow::appendLog(const QString &message) {
    if (log_) log_->append(QString("[%1] %2").arg(QTime::currentTime().toString("HH:mm:ss"), safeLogMessage(message).toHtmlEscaped()));
}

bool MainWindow::validateConfig(const QString &path, QString *error) const {
    const QString executable = development_->local() ? development_->path() : coreExecutablePath();
    if (!QFileInfo::exists(executable)) {
        if (error) *error = tr("The core CLI was not found next to the application; the profile cannot be validated");
        return false;
    }
    QProcess process;
    process.setProgram(executable);
    process.setArguments({"validate", QFileInfo(path).absoluteFilePath()});
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForStarted(5000) || !process.waitForFinished(20000)) {
        process.kill();
        if (error) *error = tr("Core did not finish validating the profile in time");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString details = safeCoreDiagnostic(process.readAllStandardError() + '\n' + process.readAllStandardOutput());
        if (error) *error = details.isEmpty()
            ? tr("Core rejected the configuration. Check the key and profile settings.")
            : tr("Core rejected the configuration:\n%1").arg(details);
        return false;
    }
    return true;
}

QString MainWindow::humanBytes(qint64 bytes) const {
    const QStringList units{tr("B"), tr("KB"), tr("MB"), tr("GB")};
    double value = bytes;
    int unit = 0;
    while (value >= 1024 && unit < units.size() - 1) { value /= 1024; ++unit; }
    return QString::number(value, 'f', unit ? 1 : 0) + " " + units[unit];
}

QIcon MainWindow::trayIcon(bool connected) const {
#ifdef Q_OS_WIN
    DWORD systemUsesLightTheme = 1;
    DWORD valueSize = sizeof(systemUsesLightTheme);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &systemUsesLightTheme, &valueSize);
    const bool darkPanel = systemUsesLightTheme == 0;
#else
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP") + ':'
        + qEnvironmentVariable("XDG_SESSION_DESKTOP") + ':' + qEnvironmentVariable("DESKTOP_SESSION");
    const bool gnomePanel = desktop.contains("GNOME", Qt::CaseInsensitive)
        || desktop.contains("ubuntu", Qt::CaseInsensitive)
        || (desktop == "::" && QSysInfo::productType() == "ubuntu");
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const bool darkPanel = gnomePanel || QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    const bool darkPanel = gnomePanel || QApplication::palette().color(QPalette::Window).lightness() < 128;
#endif
#endif
    const QString theme = darkPanel ? "dark" : "light";
    const QString state = connected ? "connected" : "disconnected";
    QIcon icon;
    for (const int size : {16, 20, 22, 24, 32, 40, 48, 64}) {
        const QPixmap source(QString(":/icons/tray/png/%1-%2-%3.png").arg(theme, state).arg(size));
        if (source.isNull())
            continue;
        QPixmap canvas(size, size);
        canvas.fill(Qt::transparent);
#ifdef Q_OS_WIN
        const int visibleSize = qRound(size * 0.82);
#else
        const int visibleSize = size;
#endif
        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const QPixmap visible = source.scaled(visibleSize, visibleSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawPixmap((size - visible.width()) / 2, (size - visible.height()) / 2, visible);
        icon.addPixmap(canvas);
    }
    return icon;
}

void MainWindow::initTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;
    trayMenu_ = new QMenu(this);
    tray_ = new QSystemTrayIcon(trayIcon(false), this);
    rebuildTrayMenu();
    tray_->setContextMenu(trayMenu_);
    tray_->setToolTip(tr("Bimeron — retrieving status…"));
    connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            showWindow();
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        tray_->setIcon(trayIcon(state_ == "connected"));
    });
#endif
    tray_->show();
}

void MainWindow::rebuildTrayMenu() {
    trayMenu_->clear();
    const QHash<QString, QString> labels{{"disconnected", tr("Disconnected")}, {"connecting", tr("Connecting…")}, {"connected", tr("Connected")}, {"reconnecting", tr("Reconnecting…")}, {"stopping", tr("Disconnecting…")}, {"failed", tr("Connection failed")}};
    trayStatusAction_ = trayMenu_->addAction(tr("Status: %1").arg(nativeOwnerBlocked_ ? nativeStatusLabel_->text() : helloReady_
        ? labels.value(state_, tr("Unknown state")) : connectionLabel_->text()));
    trayStatusAction_->setEnabled(false);
    trayTrafficAction_ = trayMenu_->addAction(trayTraffic_);
    trayTrafficAction_->setEnabled(false);
    trayMenu_->addSeparator();
    for (int row = 0; row < profiles_.size(); ++row) {
        auto *profile = trayMenu_->addAction(profiles_[row].name);
        profile->setData(row);
        profile->setEnabled(canActivateProfile(row));
        profile->setCheckable(true);
        profile->setChecked(helloReady_ && profiles_[row].id == activeProfileId_
            && (state_ == "connected" || state_ == "reconnecting" || state_ == "stopping"));
        connect(profile, &QAction::triggered, this, [this, row] {
            profileList_->setCurrentRow(row, QItemSelectionModel::ClearAndSelect);
            toggleConnection();
        });
    }
    trayMenu_->addSeparator();
    auto *open = trayMenu_->addAction(tr("Open Bimeron"));
    connect(open, &QAction::triggered, this, &MainWindow::showWindow);
    auto *quit = trayMenu_->addAction(tr("Quit Bimeron"));
    connect(quit, &QAction::triggered, this, &MainWindow::requestShutdown);
}

void MainWindow::showWindow() {
    if (quitRequested_)
        return;
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::requestShutdown() {
    if (nativeOwnerBlocked_ || nativeSession_) {
        quitRequested_ = false;
        if (tray_) tray_->show();
        const QString message = tr("Native owner cleanup is unconfirmed; exit was canceled. Use the verified native owner to stop, clean up and close it.");
        nativeStatusLabel_->setText(message);
        nativeStatusLabel_->show();
        appendLog(message);
        showWindow();
        return;
    }
    clearSSHAuthentication();
    if (development_->busy()) {
        QTimer::singleShot(250, this, &MainWindow::requestShutdown);
        return;
    }
    if (development_->needsCleanup()) {
        developmentExit_ = true;
        development_->select({});
        return;
    }
    if (coreUpdateCommand_ || coreUpdateAwaitingResult_ || serviceCommandProcess_ || linuxServiceProcess_) {
        QTimer::singleShot(250, this, &MainWindow::requestShutdown);
        return;
    }
    if (!quitRequested_) {
        const bool compatible = helloReady_ && !statusNeeded_;
        const QString currentInstance = instance_;
        invalidateConnection();
        helloReady_ = compatible;
        instance_ = currentInstance;
        autoConnectAttempted_ = true;
        quitRequested_ = true;
        hide();
        if (tray_)
            tray_->hide();
    }
    if (!QSettings().value("app/stopCoreOnExit", false).toBool()) {
        QApplication::quit();
        return;
    }
    if (!helloReady_ || nativeSession_) {
        quitRequested_ = false;
        if (tray_) tray_->show();
        showWindow();
        appendLog(tr("Core cleanup could not be confirmed; check Core before quitting"));
        return;
    }
    if (core_.busy()) {
        QTimer::singleShot(100, this, &MainWindow::requestShutdown);
        return;
    }
    core_.request("shutdown", {}, 120000);
}

void MainWindow::saveDiagnostics() {
    if (development_->local() || development_->busy() || development_->failed()) {
        QMessageBox::information(this, tr("Diagnostics"), tr("Diagnostics are unavailable while using a local core"));
        return;
    }
    const QString executable = coreExecutablePath();
    if (!QFileInfo::exists(executable)) {
        QMessageBox::warning(this, tr("Diagnostics"), tr("The core CLI was not found next to the application."));
        return;
    }
    QString path = QFileDialog::getSaveFileName(this, tr("Save diagnostic report"),
        QDir::homePath() + "/bimeron-diagnostics.json", tr("JSON (*.json)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(".json", Qt::CaseInsensitive))
        path += ".json";
    if (QFileInfo::exists(path)) {
        QMessageBox::warning(this, tr("File already exists"), tr("The diagnostic report does not overwrite existing files. Choose another name."));
        return;
    }

    QProcess process;
    process.setProgram(executable);
    process.setArguments({"doctor"});
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForStarted(5000) || !process.waitForFinished(30000)) {
        process.kill();
        QMessageBox::warning(this, tr("Diagnostics"), tr("Core did not finish diagnostics in time."));
        return;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString details = safeCoreDiagnostic(process.readAllStandardError() + '\n' + process.readAllStandardOutput());
        QMessageBox::warning(this, tr("Diagnostics"), details.isEmpty() ? tr("Failed to create the report.") : details);
        return;
    }
    const QByteArray report = safeDiagnosticReport(process.readAllStandardOutput());
    if (report.isEmpty()) {
        QMessageBox::warning(this, tr("Diagnostics"), tr("Core returned an invalid diagnostic report; no report was saved."));
        return;
    }
    QFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        QMessageBox::warning(this, tr("Diagnostics"), tr("Cannot create a new diagnostic report."));
        return;
    }
    if (!output.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || output.write(report) != report.size() || !output.flush()) {
        output.close();
        output.remove();
        QMessageBox::warning(this, tr("Diagnostics"), tr("Cannot save the diagnostic report."));
        return;
    }
    output.close();
    appendLog(tr("Diagnostic report saved: %1").arg(QDir::toNativeSeparators(path)));
    QMessageBox::information(this, tr("Diagnostics"), tr("The report has been saved. It contains no keys, IP addresses, or profile paths."));
}

bool MainWindow::coreUpdatePrerequisite() {
    if (nativeSession_ || nativeOwnerBlocked_ || development_->local() || development_->busy() || development_->failed()) return false;
    if (!updater_.available()) {
        coreUpdateLabel_->setText(tr("Signed updates are not available yet"));
        return false;
    }
    const QString bundled = coreExecutablePath();
    if (!TrustedCore::protectedExecutable(QCoreApplication::applicationFilePath()) ||
        !TrustedCore::protectedExecutable(bundled)) {
        coreUpdateLabel_->setText(tr("Core updates require a protected system installation of the GUI package"));
        return false;
    }
    if (!TrustedCore::supportsSourceUpdate(bundled)) {
        coreUpdateLabel_->setText(tr("Update the GUI package to enable safe core updates"));
        return false;
    }
    return true;
}

bool MainWindow::runServiceCommand(const QStringList &arguments, const QString &description) {
    if (nativeSession_ || nativeOwnerBlocked_ || development_->local() || development_->busy() || development_->failed()) return false;
    if (!operation_.isEmpty()) return false;
    if (serviceCommandProcess_ || linuxServiceProcess_)
        return false;
    const QString executable = coreExecutablePath();
    if (arguments.size() >= 2 && arguments[0] == "service" && arguments[1] == "update" &&
        !coreUpdatePrerequisite()) {
        return false;
    }
    if (!QFileInfo::exists(executable)) {
        QMessageBox::warning(this, description, tr("The core CLI was not found next to the application."));
        return false;
    }
    if (arguments == QStringList{"recover"}) {
        recoveredSession_ = sessionId_;
        recoveredInstance_ = instance_;
    }
    invalidateConnection();
    autoConnectAttempted_ = true;
    retryAt_ = 0;
    recoveryCommand_ = arguments == QStringList{"recover"};
    updateConnectionButton();
#ifdef Q_OS_WIN
    const QString parameters = TrustedCore::windowsCommandLine(arguments);
    const std::wstring file = QDir::toNativeSeparators(executable).toStdWString();
    const std::wstring params = parameters.toStdWString();
    const std::wstring directory = QDir::toNativeSeparators(QCoreApplication::applicationDirPath()).toStdWString();
    HANDLE handle = nullptr;
    if (developerMode_) {
        std::wstring commandLine = TrustedCore::windowsCommandLine(QStringList{executable} + arguments).toStdWString();
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (CreateProcessW(file.c_str(), commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                           nullptr, directory.c_str(), &startup, &process)) {
            CloseHandle(process.hThread);
            handle = process.hProcess;
        }
    } else {
        SHELLEXECUTEINFOW info{};
        info.cbSize = sizeof(info);
        info.fMask = SEE_MASK_NOCLOSEPROCESS;
        info.hwnd = reinterpret_cast<HWND>(winId());
        info.lpVerb = L"runas";
        info.lpFile = file.c_str();
        info.lpParameters = params.c_str();
        info.lpDirectory = directory.c_str();
        info.nShow = SW_HIDE;
        if (ShellExecuteExW(&info)) handle = info.hProcess;
    }
    if (!handle) {
        serviceStatusLabel_->setText(tr("Service: action canceled"));
        serviceStatusLabel_->setStyleSheet(themedStyle("color: #b54708"));
        return false;
    }

    serviceCommandProcess_ = reinterpret_cast<quintptr>(handle);
    serviceCommandDescription_ = description;
    serviceStatusLabel_->setText(description + QStringLiteral("…"));
    serviceStatusLabel_->setStyleSheet(themedStyle("color: #1267d6; font-weight: 600"));
    for (auto *button : {serviceInstallButton_, serviceStartButton_, serviceStopButton_, serviceUpdateButton_, serviceUninstallButton_, serviceRecoveryButton_})
        button->setEnabled(false);
    serviceAutomaticCheck_->setEnabled(false);
    appendLog(description + tr(": in progress"));
    QTimer::singleShot(250, this, &MainWindow::pollServiceCommand);
    return true;
#elif defined(Q_OS_LINUX)
    QString elevationProgram;
    QStringList elevationArguments;
    const QString distro = qEnvironmentVariable("WSL_DISTRO_NAME");
    if (!distro.isEmpty()) {
        elevationProgram = QStandardPaths::findExecutable(QStringLiteral("wsl.exe"));
        if (!elevationProgram.isEmpty())
            elevationArguments = {"-d", distro, "-u", "root", "--", executable};
    }
    if (elevationProgram.isEmpty()) {
        elevationProgram = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
        elevationArguments = {executable};
    }
    if (elevationProgram.isEmpty()) {
        QMessageBox::warning(this, description, tr("pkexec was not found. Install polkit to manage the service."));
        return false;
    }
    auto *process = new QProcess(this);
    linuxServiceProcess_ = process;
    serviceCommandDescription_ = description;
    serviceStatusLabel_->setText(description + QStringLiteral("…"));
    serviceStatusLabel_->setStyleSheet(themedStyle("color: #1267d6; font-weight: 600"));
    for (auto *button : {serviceInstallButton_, serviceStartButton_, serviceStopButton_, serviceUpdateButton_, serviceUninstallButton_, serviceRecoveryButton_})
        button->setEnabled(false);
    serviceAutomaticCheck_->setEnabled(false);
    appendLog(description + tr(": in progress"));
    process->setProgram(elevationProgram);
    process->setArguments(elevationArguments + arguments);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        if (linuxServiceProcess_ != process) return;
        const QString details = safeCoreDiagnostic(process->readAllStandardError() + '\n' + process->readAllStandardOutput());
        linuxServiceProcess_ = nullptr;
        process->deleteLater();
        finishServiceCommand(exitStatus == QProcess::NormalExit ? exitCode : -1, details);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || linuxServiceProcess_ != process) return;
        const QString details = process->errorString();
        linuxServiceProcess_ = nullptr;
        process->deleteLater();
        finishServiceCommand(-1, details);
    });
    process->start();
    return true;
#else
    Q_UNUSED(arguments); Q_UNUSED(description);
    return false;
#endif
}

void MainWindow::pollServiceCommand() {
#ifdef Q_OS_WIN
    if (!serviceCommandProcess_)
        return;
    auto process = reinterpret_cast<HANDLE>(serviceCommandProcess_);
    DWORD exitCode = STILL_ACTIVE;
    if (GetExitCodeProcess(process, &exitCode) && exitCode == STILL_ACTIVE) {
        QTimer::singleShot(250, this, &MainWindow::pollServiceCommand);
        return;
    }
    CloseHandle(process);
    serviceCommandProcess_ = 0;
    finishServiceCommand(static_cast<int>(exitCode));
#endif
}

void MainWindow::finishServiceCommand(int exitCode, const QString &details) {
    if (recoveryCommand_ && exitCode == 0) {
        cleanupBlocked_ = false;
        recoveryVerified_ = true;
    }
    recoveryCommand_ = false;
    const bool finishedCoreUpdate = coreUpdateCommand_;
    coreUpdateCommand_ = false;
    if (finishedCoreUpdate) { updater_.setInstalling(false); updater_.discardStaged(); }
    if (finishedCoreUpdate && exitCode != 0) {
        reconnectProfileAfterUpdate_.clear();
        expectedCoreVersionAfterUpdate_.clear();
    }
    for (auto *button : {serviceInstallButton_, serviceStartButton_, serviceStopButton_, serviceUpdateButton_, serviceUninstallButton_, serviceRecoveryButton_})
        button->setEnabled(true);
    serviceAutomaticCheck_->setEnabled(true);
#ifdef Q_OS_LINUX
    if (exitCode == 126) {
        startServiceAfterInstall_ = false;
        serviceStatusLabel_->setText(tr("Service: action canceled"));
        serviceStatusLabel_->setStyleSheet(themedStyle("color: #b54708"));
        appendLog(serviceCommandDescription_ + tr(": action canceled"));
        if (finishedCoreUpdate) coreUpdateLabel_->setText(tr("Core update canceled"));
        refreshServiceStatus();
        return;
    }
#endif
    if (exitCode != 0) {
        startServiceAfterInstall_ = false;
        appendLog(details.isEmpty() ? serviceCommandDescription_ + tr(": error, code %1").arg(exitCode)
                                    : serviceCommandDescription_ + QStringLiteral(": ") + details);
        refreshServiceStatus();
        serviceRecoveryButton_->show();
        if (finishedCoreUpdate) {
            coreUpdateLabel_->setText(tr("Core service update failed; checking service state"));
            QMessageBox::warning(this, tr("Core update"), TrustedCore::failureMessage(exitCode, details));
        }
    } else {
        serviceRecoveryButton_->hide();
        appendLog(serviceCommandDescription_ + tr(": completed"));
        if (startServiceAfterInstall_) {
            startServiceAfterInstall_ = false;
            runServiceCommand({"service", "start"}, tr("Starting service"));
            return;
        }
        refreshServiceStatus();
        if (finishedCoreUpdate) {
            coreUpdateAwaitingResult_ = true;
            coreUpdateLabel_->setText(tr("Core service updated; verifying version and connection state…"));
        }
    }
    if (finishedCoreUpdate)
        QTimer::singleShot(500, this, &MainWindow::refreshCoreAfterUpdate);
}

void MainWindow::refreshCoreAfterUpdate() {
    if (core_.busy()) QTimer::singleShot(250, this, &MainWindow::refreshCoreAfterUpdate);
    else { invalidateConnection(); core_.request("hello"); }
}

void MainWindow::reportCoreUpdateResult(const QString &message) {
    coreUpdateLabel_->setText(message);
    appendLog(message);
}

void MainWindow::refreshServiceStatus() {
    if (QCoreApplication::arguments().contains("--visual-check")) return;
    if (development_->local() || development_->busy() || development_->failed()) return;
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    if (!serviceStatusLabel_ || serviceCommandProcess_ || linuxServiceProcess_ || serviceStatusProcess_) return;
    auto *process = new QProcess(this);
    serviceStatusProcess_ = process;
    const quint64 generation = connectionGeneration_;
    const auto finished = [this, process, generation](int exitCode, QProcess::ExitStatus exitStatus) {
        if (serviceStatusProcess_ != process) return;
        serviceStatusProcess_ = nullptr;
        process->deleteLater();
        if (generation != connectionGeneration_ || development_->local() || development_->busy() || development_->failed()) return;
        const auto showActions = [this](bool install, bool start, bool stop, bool update, bool uninstall) {
            serviceInstallButton_->setVisible(install);
            serviceAutomaticCheck_->setVisible(install);
            serviceStartButton_->setVisible(start);
            serviceStopButton_->setVisible(stop);
            serviceUpdateButton_->setVisible(update);
            coreUpdateLabel_->setVisible(update);
            betaUpdatesCheck_->setVisible(update);
            serviceUninstallButton_->setVisible(uninstall);
        };
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            const QString diagnostic = QString::fromUtf8(process->readAllStandardError() + process->readAllStandardOutput());
            const bool notInstalled = QRegularExpression(QStringLiteral("service (?:is )?not installed"), QRegularExpression::CaseInsensitiveOption).match(diagnostic).hasMatch();
            serviceStatusLabel_->setText(notInstalled ? tr("Service: not installed") : tr("Service: unavailable"));
            serviceStatusLabel_->setStyleSheet(themedStyle("color: #66768a"));
            showActions(notInstalled, false, false, false, false);
            return;
        }
        const QString output = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
        const QHash<QString, QString> states{{"1", tr("stopped")}, {"2", tr("starting")}, {"3", tr("stopping")}, {"4", tr("running")}, {"5", tr("resuming")}, {"6", tr("pausing")}, {"7", tr("paused")}};
        const QString code = output.section(':', 1).trimmed();
        const QString state = states.value(code, tr("unknown"));
        if (code == "4") {
            serviceStatusLabel_->setText(tr("Service: <span style=\"color:#15803d; font-weight:600\">%1</span>").arg(state));
            serviceStatusLabel_->setStyleSheet(themedStyle("color: #66768a"));
        } else {
            serviceStatusLabel_->setText(tr("Service: %1").arg(state));
            serviceStatusLabel_->setStyleSheet(themedStyle((code == "2" || code == "3" || code == "5" || code == "6")
                ? "color: #b54708; font-weight: 600"
                : "color: #66768a"));
        }
        if (code == "1") showActions(false, true, false, true, true);
        else if (code == "4") showActions(false, false, true, true, false);
        else if (code == "7") showActions(false, false, true, false, false);
        else {
            showActions(false, false, false, false, false);
            QTimer::singleShot(1000, this, &MainWindow::refreshServiceStatus);
        }
    };
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, finished);
    connect(process, &QProcess::errorOccurred, this, [finished](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) finished(-1, QProcess::CrashExit);
    });
    QTimer::singleShot(5000, process, [process] { if (process->state() != QProcess::NotRunning) process->kill(); });
    process->start(coreExecutablePath(), {"service", "status"});
#endif
}

void MainWindow::recoverNetwork() {
    if (QMessageBox::warning(this, tr("Emergency recovery"),
        tr("Recovery removes leftover routes, DNS settings, and the kill switch. Direct network access may be restored. Use this only after a cleanup failure. Continue?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    runServiceCommand({"recover"}, tr("Emergency network recovery"));
}

void MainWindow::applyStartupSetting(bool enabled) {
#ifdef Q_OS_WIN
    QSettings runKey("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (enabled)
        runKey.setValue("Bimeron", QString("\"%1\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
    else
        runKey.remove("Bimeron");
#elif defined(Q_OS_LINUX)
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
        .filePath(QStringLiteral("autostart/bimeron.desktop"));
    bool success = false;
    if (enabled) {
        const QString executable = QCoreApplication::applicationFilePath()
            .replace(QStringLiteral("\\"), QStringLiteral("\\\\"))
            .replace(QStringLiteral("\""), QStringLiteral("\\\""));
        if (QDir().mkpath(QFileInfo(path).absolutePath())) {
            QSaveFile file(path);
            if (file.open(QIODevice::WriteOnly)) {
                const QByteArray entry = QStringLiteral("[Desktop Entry]\nType=Application\nName=Bimeron\nExec=\"%1\" --autostart\nIcon=bimeron\nTerminal=false\n")
                    .arg(executable).toUtf8();
                success = file.write(entry) == entry.size() && file.commit();
            }
        }
    } else {
        success = !QFileInfo::exists(path) || QFile::remove(path);
    }
    if (!success) {
        QMessageBox::warning(this, tr("Startup setting"), tr("Could not update the Linux autostart setting."));
        // The checkbox must reflect the file on disk after a failed change.
        if (auto *check = qobject_cast<QCheckBox *>(sender())) {
            const QSignalBlocker blocker(check);
            check->setChecked(QFileInfo::exists(path));
        }
    }
#else
    Q_UNUSED(enabled);
#endif
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (tray_ && tray_->isVisible() && !quitRequested_) {
        hide();
        event->ignore();
        return;
    }
    event->ignore();
    requestShutdown();
}

QWidget *MainWindow::developmentPage() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 16, 18, 16);
    auto *title = new QLabel(tr("Development"));
    title->setStyleSheet("font-size: 17px; font-weight: 700");
    layout->addWidget(title);
    auto *hint = new QLabel(tr("The active core resets when you quit; closing to the tray keeps this session. The last local path is remembered."));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    developmentBackend_ = new HiDpiComboBox;
    developmentBackend_->setObjectName("developmentBackend");
    developmentBackend_->addItems({tr("System service"), tr("Local core executable")});
    layout->addWidget(developmentBackend_);
    auto *pathRow = new QHBoxLayout;
    developmentPath_ = new QLineEdit;
    developmentPath_->setObjectName("developmentPath");
    developmentPath_->setPlaceholderText(tr("Absolute path to the core executable"));
    developmentPath_->setText(QSettings().value("developer/lastCorePath").toString());
    developmentPath_->setEnabled(false);
    const auto rememberPath = [this] {
        const QFileInfo file(developmentPath_->text().trimmed());
        if (file.isAbsolute() && file.isFile())
            QSettings().setValue("developer/lastCorePath", file.absoluteFilePath());
    };
    connect(developmentPath_, &QLineEdit::editingFinished, this, rememberPath);
    auto *browse = new QPushButton(tr("Browse…"));
    browse->setEnabled(false);
    pathRow->addWidget(developmentPath_, 1);
    pathRow->addWidget(browse);
    layout->addLayout(pathRow);
    developmentModified_ = new QLabel;
    developmentModified_->setObjectName("developmentModified");
    layout->addWidget(developmentModified_);
    const auto updateModified = [this] {
        const QFileInfo file(developmentPath_->text().trimmed());
        const QDateTime modified = file.isFile() ? file.lastModified() : QDateTime{};
        developmentModified_->setText(tr("File modified: %1").arg(modified.isValid()
            ? QLocale().toString(modified.toLocalTime(), QLocale::ShortFormat) : tr("unavailable")));
        developmentModified_->setVisible(developmentBackend_->currentIndex() == 1 && !developmentPath_->text().trimmed().isEmpty());
    };
    connect(developmentPath_, &QLineEdit::textChanged, this, updateModified);
    connect(developmentBackend_, &QComboBox::currentIndexChanged, this, [this, browse, updateModified](int index) {
        developmentPath_->setEnabled(index == 1);
        browse->setEnabled(index == 1);
        updateModified();
    });
    updateModified();
    connect(browse, &QPushButton::clicked, this, [this, rememberPath] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Select core executable"), developmentPath_->text());
        if (!path.isEmpty()) {
            developmentPath_->setText(path);
            rememberPath();
        }
    });
    developmentApply_ = new QPushButton(tr("Apply"));
    layout->addWidget(developmentApply_);
    developmentStatus_ = new QLabel(tr("System service"));
    developmentStatus_->setWordWrap(true);
    developmentStatus_->setTextFormat(Qt::PlainText);
    layout->addWidget(developmentStatus_);
    developmentStopOrphan_ = new QPushButton(tr("Stop remaining local daemon"));
    developmentStopOrphan_->hide();
    layout->addWidget(developmentStopOrphan_);
    connect(developmentStopOrphan_, &QPushButton::clicked, this, [this] {
        if (QMessageBox::warning(this, tr("Stop remaining local daemon"),
                tr("Disconnect and shut down the remaining local Core?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
            development_->stopOrphan();
    });
    layout->addStretch();
    connect(developmentApply_, &QPushButton::clicked, this, [this] {
        if (nativeSession_ || nativeOwnerBlocked_ || development_->busy()) return;
        if (coreUpdateCommand_ || coreUpdateAwaitingResult_ || serviceCommandProcess_ || linuxServiceProcess_) {
            QMessageBox::information(this, tr("Development"), tr("Wait for the current service operation to finish"));
            return;
        }
        const QString path = developmentBackend_->currentIndex() == 1 ? developmentPath_->text().trimmed() : QString{};
        if (developmentBackend_->currentIndex() == 1 && path.isEmpty()) return;
        if (!development_->failed() && path == development_->path()) return;
        if (QMessageBox::warning(this, tr("Switch core"),
                activeProxyMode() ? tr("Switching cores disconnects the proxy. The selected local executable will run with administrator privileges. Continue?")
                                  : tr("Switching cores disconnects the VPN. The selected local executable will run with administrator privileges. Continue?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        if (nativeSession_ || nativeOwnerBlocked_ || coreUpdateCommand_ || coreUpdateAwaitingResult_
            || serviceCommandProcess_ || linuxServiceProcess_ || development_->busy()) return;
        development_->select(path);
    });
    return page;
}

void MainWindow::updateDevelopment() {
    autoConnectAttempted_ = true;
    invalidateConnection();
    activeProfileId_.clear();
    coreVersionLabel_->setText(tr("Core: unknown"));
    const bool blocked = development_->busy() || development_->failed();
    validationExecutable_ = development_->local() ? development_->path() : coreExecutablePath();
    validationIdentity_ = {};
    core_.setEndpoint(development_->local() ? development_->endpoint() : QString{}, development_->processId());
    state_ = "unknown";
    connectionLabel_->setText(blocked ? tr("Core unavailable") : tr("Connecting to core…"));
    updateConnectionButton();
    if (developmentStatus_) {
        developmentStatus_->setText(development_->status()
            + (development_->local() ? "\n" + QDir::toNativeSeparators(development_->path()) : QString{}));
        developmentApply_->setEnabled(!development_->busy());
        developmentStopOrphan_->setVisible(development_->orphanAvailable());
        developmentStopOrphan_->setEnabled(!development_->busy());
    }
    const bool system = !blocked && !development_->local();
    for (auto *button : {serviceInstallButton_, serviceStartButton_, serviceStopButton_, serviceUpdateButton_, serviceUninstallButton_, serviceRecoveryButton_})
        button->setEnabled(system);
    serviceAutomaticCheck_->setEnabled(system);
    if (!system) {
        serviceStatusLabel_->setText(tr("System service controls are suspended"));
        serviceUpdateButton_->hide();
        coreUpdateLabel_->hide();
        betaUpdatesCheck_->hide();
    }
    if (!blocked) {
        cleanupBlocked_ = false;
        retryAt_ = 0;
        core_.request("hello");
        probeValidationCli();
        if (system) refreshServiceStatus();
    }
}

void MainWindow::restartApplication(bool development) {
    if (nativeSession_ || nativeOwnerBlocked_) return;
    if (coreUpdateCommand_ || coreUpdateAwaitingResult_ || serviceCommandProcess_ || linuxServiceProcess_ || development_->busy()) {
        QTimer::singleShot(250, this, [this, development] { restartApplication(development); });
        return;
    }
    if (development_->needsCleanup()) {
        restartRequested_ = true;
        developmentExit_ = true;
        development_->select({});
        return;
    }
    QStringList arguments{"--restart"};
    if (development) arguments << "--development";
    if (QCoreApplication::arguments().contains("--visual-check")) arguments << "--visual-check";
    bool started = false;
#ifdef Q_OS_WIN
    if (!property("developmentRestartWithoutElevation").toBool()) {
        if (development) {
            const std::wstring executable = QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString();
            const std::wstring parameters = TrustedCore::windowsCommandLine(arguments).toStdWString();
            const std::wstring directory = QDir::toNativeSeparators(QCoreApplication::applicationDirPath()).toStdWString();
            SHELLEXECUTEINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
            info.lpVerb = L"runas";
            info.lpFile = executable.c_str();
            info.lpParameters = parameters.c_str();
            info.lpDirectory = directory.c_str();
            info.nShow = SW_SHOWNORMAL;
            if (ShellExecuteExW(&info) && info.hProcess) {
                const QString sid = processUserSid(info.hProcess);
                started = !sid.isEmpty() && sid == processUserSid(GetCurrentProcess());
                if (!started) TerminateProcess(info.hProcess, 1);
                CloseHandle(info.hProcess);
            }
        } else if (developerMode_) {
            started = startUnelevated(arguments);
        } else {
            started = QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments);
        }
    } else
#endif
        started = QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments);
    if (started) QApplication::quit();
    else {
        developmentClicks_ = 0;
        restartRequested_ = false;
        QMessageBox::warning(this, tr("Restart"), tr("Could not restart Bimeron"));
    }
}

void MainWindow::clearSSHAuthentication() {
    sshReplyReady_ = false;
    sshAnswers_ = {};
    sshChallenge_.clear();
    if (sshAuthExpiry_) sshAuthExpiry_->stop();
    if (sshDialog_) { sshDialog_->reject(); sshDialog_->deleteLater(); sshDialog_ = nullptr; }
}

void MainWindow::handleSSHAuthentication(const QJsonObject &auth) {
    if (auth.isEmpty()) { clearSSHAuthentication(); return; }
    if (state_ != "connecting" && state_ != "reconnecting") return;
    const QString id = auth.value("id").toString();
    if (id != sshChallenge_) clearSSHAuthentication();
    const QString kind = auth.value("kind").toString();
    const auto prompts = auth.value("prompts").toArray();
    const auto echo = auth.value("echo").toArray();
    if (id.isEmpty() || id.size() > 256 || !QStringList{"key_passphrase", "password", "keyboard_interactive"}.contains(kind)
        || prompts.isEmpty() || prompts.size() > 8 || prompts.size() != echo.size()) { clearSSHAuthentication(); return; }
    for (qsizetype i = 0; i < prompts.size(); ++i)
        if (!prompts[i].isString() || prompts[i].toString().toUtf8().size() > 256 || !echo[i].isBool()) { clearSSHAuthentication(); return; }
    if (id == sshChallenge_ || sshSeen_.contains(id)) return;
    clearSSHAuthentication();
    if (sshSeen_.size() >= 256) return; // Bound server-controlled state within one session.
    sshSeen_.insert(id);
    sshChallenge_ = id;
    auto *dialog = new QDialog(this);
    sshDialog_ = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("SSH authentication"));
    auto *layout = new QVBoxLayout(dialog);
    auto *form = new QFormLayout;
    QList<QLineEdit *> inputs;
    for (qsizetype i = 0; i < prompts.size(); ++i) {
        auto *input = new QLineEdit(dialog);
        input->setMaxLength(1024);
        input->setEchoMode(kind == "keyboard_interactive" && echo[i].toBool() ? QLineEdit::Normal : QLineEdit::Password);
        const QString prompt = prompts[i].toString();
        auto *label = new QPlainTextEdit(dialog);
        label->setReadOnly(true);
        label->setPlainText(prompt);
        label->setWordWrapMode(QTextOption::WrapAnywhere);
        label->setFrameShape(QFrame::NoFrame);
        label->setFixedWidth(300);
        label->setFixedHeight(label->fontMetrics().boundingRect(QRect(0, 0, 280, 4096),
            Qt::TextWrapAnywhere, prompt).height() + 16);
        input->setAccessibleName(prompt);
        form->addRow(label, input);
        inputs.append(input);
    }
    if (prompts.size() > 3) {
        auto *body = new QWidget(dialog);
        body->setLayout(form);
        auto *scroll = new QScrollArea(dialog);
        scroll->setWidgetResizable(true);
        scroll->setWidget(body);
        scroll->setMinimumHeight(240);
        scroll->setMaximumHeight(400);
        layout->addWidget(scroll);
    } else {
        layout->addLayout(form);
    }
    auto *answerError = new QLabel(tr("SSH answers exceed the allowed size."), dialog);
    answerError->setObjectName("sshAnswerError");
    answerError->setTextFormat(Qt::PlainText);
    answerError->setWordWrap(true);
    answerError->hide();
    layout->addWidget(answerError);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, inputs, id, answerError] {
        if (sshDialog_ != dialog || sshChallenge_ != id || stopRequested_
            || (state_ != "connecting" && state_ != "reconnecting")) return;
        QJsonArray answers;
        for (auto *input : inputs) {
            if (input->text().toUtf8().size() > 1024) { answerError->show(); return; }
            answers.append(input->text());
        }
        const QJsonObject request{{"version", 1}, {"id", QString(32, 'x')}, {"command", "auth_reply"},
            {"challenge_id", id}, {"answers", answers}};
        if (QJsonDocument(request).toJson(QJsonDocument::Compact).size() + 1 > 16 * 1024) { answerError->show(); return; }
        sshAnswers_ = answers;
        sshReplyReady_ = true;
        for (auto *input : inputs) input->clear();
        sshDialog_ = nullptr;
        dialog->accept();
    });
    connect(dialog, &QDialog::rejected, this, [inputs] { for (auto *input : inputs) input->clear(); });
    if (!sshAuthExpiry_) {
        sshAuthExpiry_ = new QTimer(this);
        sshAuthExpiry_->setSingleShot(true);
        connect(sshAuthExpiry_, &QTimer::timeout, this, &MainWindow::clearSSHAuthentication);
    }
    sshAuthExpiry_->start(120000);
    dialog->show();
}
