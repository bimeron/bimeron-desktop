#pragma once
#include "GeoSitePicker.h"

#include <QListWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QPlainTextEdit>
#include <QClipboard>
#include <QMimeData>
#include <QPointer>
#include <QCloseEvent>
#include <QSystemTrayIcon>
#include <QTranslator>
#include <QMenu>
#include "NativeIKEv2SessionTest.h"

// Compile the application translator itself; never invoke its renamed entry point.
#define main bimeronApplicationEntryForTranslationTest
#include "../src/main.cpp"
#undef main
#include "ProfileEditorDialog.h"
#include "GeoDataSelection.h"
#include "WindowsTheme.h"
#include <QLayout>
#include <QTabWidget>
#include <QToolButton>
#include <QTableWidget>
#include <QAction>
#include <QScreen>
#include <QProgressBar>
#include <QStyleOptionProgressBar>
#include "GeoDataDialog.h"
#include "GeoDataDownload.h"
#include "GeoDataDownloadFixtures.h"

// Uses only temporary profiles/settings, a private pipe/socket and this test's
// validate subcommand. Never reaches a system endpoint or privileged helper.
struct ConnectionManagementTest {
    // The Settings startup selector writes preferences only on explicit user activation.
    static void autoConnectSelector() {
        QTemporaryDir directory;
        require(directory.isValid(), "selector fixture directory");
        const QByteArray packet("[Profile]\nName = Packet\n[AccessKey]\nSOCKS5 = socks5://localhost:1080\n[Interface]\nMode = proxy\n");
        const QByteArray native("[Profile]\nName = Native\n[IKEv2]\nWindowsProfile = Fixture\nServerName = vpn.invalid\n[Interface]\nMode = vpn\n");
        const QByteArray unsupported("[Profile]\nName = Unsupported\n[AccessKey]\nType = future\n");
        QList<Profile> profiles;
        const auto add = [&](const QString &file, const QString &name, const QByteArray &bytes) {
            const QString path = directory.filePath(file);
            QFile config(path);
            require(config.open(QIODevice::WriteOnly) && config.write(bytes) == bytes.size(), "write selector fixture");
            profiles.append({ProfileStore::identity(path), name, path});
        };
        add("alpha.conf", "Alpha", packet);
        add("same-1.conf", "Same", packet);
        add("same-2.conf", "Same", packet);
        add("native.conf", "Native", native);
        add("unsupported.conf", "Unsupported", unsupported);
        const QString alpha = profiles[0].id, first = profiles[1].id, second = profiles[2].id, nativeId = profiles[3].id;
        QSettings().remove("app/autoConnect");
        QSettings().remove("app/autoConnectProfile");

        MainWindow window;
        window.core_.cancel();
        window.core_.setEndpoint("bimeron-auto-connect-selector-" + QUuid::createUuid().toString());
        for (auto *timer : window.findChildren<QTimer *>()) timer->stop();
        window.autoConnectAttempted_ = true;
        window.store_ = ProfileStore(directory.path());
        require(window.store_.save(profiles), "save selector fixture");
        auto *selector = window.findChild<QComboBox *>("settingsAutoConnect");
        require(selector && selector == window.autoConnectProfile_ && selector->isEnabled(), "single always-usable selector");
        for (auto *box : window.findChildren<QCheckBox *>())
            require(!box->text().contains("automatically", Qt::CaseInsensitive), "former auto-connect checkbox removed");
        const auto stored = [] {
            const QSettings settings;
            return QVariantList{settings.contains("app/autoConnect"), settings.value("app/autoConnect"),
                                settings.contains("app/autoConnectProfile"), settings.value("app/autoConnectProfile")};
        };
        const auto refreshWithoutWrites = [&](const char *message) {
            const auto before = stored();
            window.refreshProfiles();
            require(stored() == before && window.operation_.isEmpty(), message);
        };
        const auto activate = [&](int index) { emit selector->activated(index); QApplication::processEvents(); };
        const auto enabled = [&](int index) {
            return (selector->model()->index(index, 0).flags() & Qt::ItemIsEnabled) != 0;
        };

        refreshWithoutWrites("initial population creates no preferences");
        require(selector->currentIndex() == 0 && selector->currentText() == "Off" && selector->count() == 5,
                "Off, separator and three eligible profiles");
        require(!enabled(1) && selector->itemData(1).isNull(), "separator is not selectable");
        require(selector->itemData(2) == alpha && selector->itemData(3) == first && selector->itemData(4) == second,
                "eligible profiles keep their order and stable IDs");
        require(selector->findData(nativeId) < 0 && selector->findData(profiles[4].id) < 0,
                "native and non-connectable profiles are excluded");
        require(window.autoConnectHint_->text() == "When Bimeron starts", "startup hint");

        selector->setFocus();
        QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QApplication::sendEvent(selector, &down);
        require(selector->currentIndex() == 2 && QSettings().value("app/autoConnect").toBool()
                && QSettings().value("app/autoConnectProfile") == alpha && window.operation_.isEmpty(),
                "keyboard selection skips the separator, enables auto-connect and connects nothing");

        activate(selector->findData(second));
        require(QSettings().value("app/autoConnectProfile") == second && selector->currentData() == second
                && selector->toolTip() == "Same", "duplicate display names resolve to their own IDs");
        activate(0);
        require(!QSettings().value("app/autoConnect").toBool() && QSettings().value("app/autoConnectProfile") == second
                && selector->currentIndex() == 0 && selector->toolTip().isEmpty(), "Off retains the remembered profile");
        refreshWithoutWrites("refresh keeps Off");
        require(selector->currentIndex() == 0, "disabled preference shows Off despite a remembered profile");

        QSettings().setValue("app/autoConnect", true);
        profiles.move(2, 0);
        profiles[0].name = "Renamed";
        require(window.store_.save(profiles), "save renamed fixture");
        refreshWithoutWrites("rename and reorder refresh without writes");
        require(selector->currentData() == second && selector->currentText() == "Renamed" && selector->itemData(2) == second,
                "selection follows the stable ID after rename and reorder");

        for (const QString &missing : {QStringLiteral("removed-profile"), nativeId}) {
            QSettings().setValue("app/autoConnectProfile", missing);
            refreshWithoutWrites("stale selection is preserved");
            require(selector->currentIndex() == 1 && selector->currentText() == "Profile unavailable" && !enabled(1)
                    && selector->itemData(1).isNull() && enabled(0) && selector->count() == 6
                    && window.autoConnectHint_->text().contains("removed"), "unavailable placeholder without fallback");
            activate(1);
            require(QSettings().value("app/autoConnect").toBool() && QSettings().value("app/autoConnectProfile") == missing,
                    "placeholder is never stored as a profile ID");
        }
        QKeyEvent up(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
        QApplication::sendEvent(selector, &up);
        require(selector->currentIndex() == 0 && !QSettings().value("app/autoConnect").toBool()
                && QSettings().value("app/autoConnectProfile") == nativeId, "keyboard Off from the placeholder");

        require(window.store_.save({profiles[3], profiles[4]}), "save ineligible fixture");
        refreshWithoutWrites("empty eligible list refreshes without writes");
        require(selector->count() == 1 && selector->currentIndex() == 0
                && window.autoConnectHint_->text() == "No profiles can connect automatically yet.", "no eligible profiles");
        require(window.store_.save(profiles), "restore eligible fixture");
        refreshWithoutWrites("a new profile does not enable auto-connect");
        require(selector->currentIndex() == 0 && !QSettings().value("app/autoConnect").toBool(), "Off survives new profiles");

        auto *databases = window.findChild<QPushButton *>("settingsGeoData");
        require(databases && databases->isEnabled(), "header database action exists");
        for (QWidget *parent = databases->parentWidget(); parent; parent = parent->parentWidget())
            require(parent->objectName() != "settingsCard", "database action is in the page header, not a preference card");
        QSettings().remove("app/autoConnect");
        QSettings().remove("app/autoConnectProfile");
    }

    static void settingsGeoDataChecking() {
        const QString core = qEnvironmentVariable("BIMERON_GEODATA_CORE");
        if (core.isEmpty()) return;
        QTemporaryDir directory;
        const QString path = directory.filePath("selected.dat");
        QFile file(path); require(file.open(QIODevice::WriteOnly), "selected database fixture");
        file.write(QByteArray::fromHex("0a0e0a02727512080a04cb0071001018")); file.close();
        auto *selection = GeoDataSelection::instance();
        const auto previous = selection->paths();
        QString error;
        require(selection->apply({{"geoip", path}}, core, {}, &error), "select local database");
        MainWindow window; window.core_.cancel();
        window.core_.setEndpoint("bimeron-settings-geodata-" + QUuid::createUuid().toString());
        for (auto *timer : window.findChildren<QTimer *>()) timer->stop();
        window.validationExecutable_ = core;
        auto *action = window.findChild<QPushButton *>("settingsGeoData");
        require(action && !action->text().isEmpty(), "ordinary Settings database action exists");
        const auto open = [&](bool cached, bool missing) {
            QTimer::singleShot(0, &window, [&, cached, missing] {
                auto *manager = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                require(manager && manager->objectName() == "sharedGeoDataDialog", "Settings opens shared manager");
                auto *status = manager->findChild<QLabel *>("geoipStatus");
                auto *bar = manager->findChild<QProgressBar *>("geoipProgress");
                auto *cancel = manager->findChild<QPushButton *>("geoipCancel");
                auto *apply = manager->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply);
                require(!bar->isVisible() && !cancel->isVisible() && !apply->isEnabled(), "passive inspection has no transfer UI or unsafe Apply");
                require(status->text().startsWith(cached ? "Ready" : "Checking…"), "cached ready or explicit initial checking");
                if (cached) require(!selection->catalogsBusy(), "cached reopen starts no Core process");
                require(waitFor([&] { return !selection->catalogsBusy(); }), "passive catalog completes");
                QApplication::processEvents();
                require(status->text().startsWith(missing ? "Invalid file" : "Ready"), "passive validation result");
                require(!bar->isVisible() && !cancel->isVisible(), "passive success and failure hide progress");
                require(manager->size() == QSize(480, 300), "compact manager dimensions");
                manager->reject();
            });
            action->click();
        };
        open(false, false);
        open(true, false);
        require(QFile::remove(path), "remove selected database fixture");
        open(false, true);
        require(selection->paths().value("geoip") == path, "inspection does not replace shared choices");
        require(selection->apply(previous, core, {}, &error), "restore test selection");
    }
    static void downloadFeedback(const QString &destination = {}) {
        using GeoDataFixture::metadata;
        using GeoDataFixture::checksum;
        GeoDataFixture::Network network;
        network.responses = {{{}, 200, {}, -1, true}, {{}, 429, {}, -1, false, false, QNetworkReply::UnknownContentError}};
        GeoDataDownload download(&network);
        MainWindow parent; parent.core_.cancel();
        parent.core_.setEndpoint("bimeron-feedback-" + QUuid::createUuid().toString());
        for (auto *timer : parent.findChildren<QTimer *>()) timer->stop();
        auto *selection = GeoDataSelection::instance();
        const auto previous = selection->paths();
        const QString core = qEnvironmentVariable("BIMERON_GEODATA_CORE");
        QTemporaryDir directory;
        const QString manual = directory.filePath("manual-geoip.dat");
        const QByteArray bytes = QByteArray::fromHex("0a0e0a02727512080a04cb0071001018");
        if (!core.isEmpty()) {
            QFile file(manual); require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(), "prior manual file"); file.close();
            QString error; require(selection->apply({{"geoip", manual}}, core, {}, &error), "prior shared selection");
        }
        const auto expected = selection->paths();
        QTimer::singleShot(500, &parent, [&] {
            auto *manager = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            require(manager, "feedback manager");
            auto *get = manager->findChild<QPushButton *>("geoipDownload");
            auto *choose = manager->findChild<QPushButton *>("geoipChoose");
            auto *cancel = manager->findChild<QPushButton *>("geoipCancel");
            auto *bar = manager->findChild<QProgressBar *>("geoipProgress");
            require(waitFor([&] { return !selection->catalogsBusy(); }), "initial prior validation");
            get->click(); QApplication::processEvents();
            require(!get->isVisible() && !choose->isVisible() && cancel->isVisible() && cancel->isEnabled(), "single Cancel replaces transfer actions");
            require(!bar->isTextVisible() && bar->sizePolicy().horizontalPolicy() == QSizePolicy::Expanding, "full track has no percent reservation");
            QStyleOptionProgressBar option; option.initFrom(bar); option.rect = bar->rect(); option.textVisible = false;
            const auto groove = bar->style()->subElementRect(QStyle::SE_ProgressBarGroove, &option, bar);
            const auto rowRight = cancel->mapTo(manager, cancel->rect().topRight()).x();
            require(qAbs(bar->mapTo(manager, groove.topRight()).x() - rowRight) <= 1, "painted progress groove aligns with row actions");
            download.progress("geoip", 50, 100); QApplication::processEvents();
            if (!destination.isEmpty()) require(manager->grab().save(destination + ".transfer.png"), "transfer screenshot");
            cancel->click(); require(!download.busy("geoip") && !bar->isVisible() && get->isVisible(), "row cancellation restores actions");
            get->click(); network.replies.first()->finish();
            require(waitFor([&] { return !download.busy("geoip"); }), "retry HTTP failure");
            QApplication::processEvents();
            auto *help = manager->findChild<QToolButton *>("geoipError");
            require(help->isVisible() && help->focusPolicy() == Qt::StrongFocus && !help->accessibleName().isEmpty(), "accessible error button");
            const auto *status = manager->findChild<QLabel *>("geoipStatus");
            require(help->x() - (status->x() + status->width()) <= 12, "error help sits beside failure status");
            if (!destination.isEmpty()) require(manager->grab().save(destination + ".failure.png"), "error screenshot");
            QTimer::singleShot(0, manager, [&] {
                auto *popup = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                require(popup && popup->textFormat() == Qt::PlainText && popup->informativeText().contains("Phase: metadata")
                    && popup->informativeText().contains("HTTP 429") && popup->informativeText().contains("Qt network code") && !popup->informativeText().contains("SYNTHETIC_")
                    && !popup->informativeText().contains("token="), "safe detailed error dialog");
                if (!destination.isEmpty()) require(popup->grab().save(destination + ".error-popup.png"), "error popup screenshot");
                popup->accept();
            });
            help->setFocus();
            QKeyEvent space(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
            QKeyEvent release(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
            QApplication::sendEvent(help, &space); QApplication::sendEvent(help, &release);
            manager->reject();
        });
        manageSharedGeoData(&parent, core, QString(), {}, &download);
        require(selection->paths() == expected, "cancel/failure preserves shared selection");
        if (!core.isEmpty()) {
            network.responses = {{metadata()}, {checksum(bytes)}, {bytes}, {{}, 200, {}, -1, true}};
            QString staged;
            const auto observer = QObject::connect(&download, &GeoDataDownload::candidateReady, &parent,
                [&](QString kind, QString path, QString) {
                staged = path;
                QTimer::singleShot(0, &parent, [&, kind, path] {
                    auto *manager = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                    require(manager && QFile::exists(path), "catalog cancellation owns candidate");
                    const QString key = GeoDataSelection::catalogKey(core, kind, path);
                    manager->findChild<QPushButton *>("geoipCancel")->click();
                    QStringList canceledGroups; QString canceledError;
                    require(!selection->cachedCatalog(core, kind, path, &canceledGroups, &canceledError), "cancellation erases candidate cache");
                    require(!QFile::exists(path) && !download.busy(kind) && !selection->catalogsBusy(), "catalog cancellation stops process and removes candidate");
                    manager->findChild<QPushButton *>("geoipDownload")->click();
                    selection->catalogResult(key, {"ru"}, {}); // Late result from canceled validation.
                    require(download.busy(kind) && manager->findChild<QPushButton *>("geoipCancel")->isVisible(), "stale catalog result cannot alter retry");
                    manager->findChild<QPushButton *>("geoipCancel")->click();
                    manager->reject();
                });
            });
            QTimer::singleShot(500, &parent, [&] {
                auto *manager = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                require(manager && waitFor([&] { return !selection->catalogsBusy(); }), "prior catalog settles before retry");
                manager->findChild<QPushButton *>("geoipDownload")->click();
            });
            manageSharedGeoData(&parent, core, QString(), {}, &download);
            QObject::disconnect(observer);
            require(!staged.isEmpty() && !QFile::exists(staged) && selection->paths() == expected, "catalog cancel preserves previous selection");
            for (const bool valid : {true, false}) {
                const QByteArray candidate = valid ? bytes : QByteArray("invalid protobuf");
                network.responses = {{metadata()}, {checksum(candidate)}, {candidate}};
                QString completedPath;
                const auto capture = QObject::connect(&download, &GeoDataDownload::candidateReady, &parent,
                    [&](QString, QString path, QString) { completedPath = path; });
                QTimer::singleShot(500, &parent, [&] {
                    auto *manager = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                    require(manager && waitFor([&] { return !selection->catalogsBusy(); }), "prior catalog settled");
                    manager->findChild<QPushButton *>("geoipDownload")->click();
                    require(waitFor([&] { return !completedPath.isEmpty() && !download.busy("geoip") && !selection->catalogsBusy(); }), "candidate completion");
                    QApplication::processEvents();
                    QStringList groups; QString failure;
                    require(!selection->cachedCatalog(core, "geoip", completedPath, &groups, &failure), "completed candidate catalog released");
                    require(!QFile::exists(completedPath), "completed candidate removed");
                    const QString status = manager->findChild<QLabel *>("geoipStatus")->text();
                    if (!status.startsWith(valid ? QObject::tr("Ready") : QObject::tr("Download failed")))
                        fprintf(stderr, "Completion valid=%d status=%s\n", int(valid), qPrintable(status));
                    require(status.startsWith(valid ? QObject::tr("Ready") : QObject::tr("Download failed")), "completion result shown");
                    manager->reject();
                });
                manageSharedGeoData(&parent, core, QString(), {}, &download);
                QObject::disconnect(capture);
                require(selection->paths() == expected, "completion without Apply preserves selection");
            }
            QFile file(manual); require(file.open(QIODevice::ReadOnly) && file.readAll() == bytes, "cancel/failure preserves manual bytes");
            QString error; require(selection->apply(previous, core, {}, &error), "restore prior test selection");
        }
    }
    static void sharedGeoDataPreparation() {
        const QString core = qEnvironmentVariable("BIMERON_GEODATA_CORE");
        if (core.isEmpty()) return;
        QTemporaryDir directory; require(directory.isValid(), "shared profile directory");
        const auto read = [](const QString &path) { QFile f(path); require(f.open(QIODevice::ReadOnly), "read shared profile"); return f.readAll(); };
        const QString ip = directory.filePath("geoip.dat"), updated = directory.filePath("updated.dat");
        // One GeoIP group 'ru', CIDR 203.0.113.0/24.
        const QByteArray bytes = QByteArray::fromHex("0a0e0a02727512080a04cb0071001018");
        for (const auto &path : {ip, updated}) { QFile f(path); require(f.open(QIODevice::WriteOnly), "write shared database"); f.write(bytes); }
        const QByteArray source = "[Profile]\nVersion=1\nName=Shared\n[AccessKey]\nType=shadowsocks\nServer=example.invalid\nPort=443\nMethod=aes-256-gcm\nPassword=SYNTHETIC\n[Interface]\nMode=proxy\n[Routing]\nDefault=proxy\n# retained\n[Rule]\nGeoIP=ru\nAction=direct\n# [Rule]\n# GeoSite=missing\n# Action=block\n";
        const QString first = directory.filePath("first.conf"), second = directory.filePath("second.conf");
        require(ProfileStore::writeConfig(first, source) && ProfileStore::writeConfig(second, source), "create managed profiles");
        const QList<Profile> profiles{{ProfileStore::identity(first), "First", first}, {ProfileStore::identity(second), "Second", second}};
        auto *selection = GeoDataSelection::instance(); const auto previous = selection->paths();
        QString error;
        require(selection->apply({{"geoip", ip}}, core, profiles, &error), "apply global GeoIP");
        require(read(first) == source && read(second) == source, "shared Apply must not rewrite profiles");
        MainWindow window; window.core_.cancel();
        window.core_.setEndpoint("bimeron-shared-test-" + QUuid::createUuid().toString());
        for (auto *timer : window.findChildren<QTimer *>()) timer->stop();
        window.validationExecutable_ = core;
        window.capabilities_ = {{"geoip_split", true}, {"geosite_split", true}, {"geodata_schema", "v2ray-protobuf"}, {"proxy", true}};
        // Exercise the actual modal Raw Save gate, not just preparation after acceptance.
        for (const bool stale : {false, true}) {
            QByteArray original = source;
            original.replace("[Rule]\nGeoIP=ru\nAction=direct\n", "");
            original += " # [FutureDesktop]\n# Opaque=retained\n";
            if (stale) original += "[GeoData]\nGeoIPFile=missing-old.dat\n";
            const QByteArray validDraft = original + "[Rule]\nGeoIP=ru\nAction=direct\n";
            for (const int action : {0, 1, 2, 3}) {
                require(ProfileStore::writeConfig(first, original), "reset Raw Save original");
                window.profiles_ = {profiles.first()};
                bool visited = false, rejected = false;
                QTimer::singleShot(0, &window, [&] {
                    auto *dialog = qobject_cast<ProfileEditorDialog *>(QApplication::activeModalWidget());
                    require(dialog, "actual editProfile modal"); visited = true;
                    auto *tabs = dialog->findChild<QTabWidget *>();
                    tabs->setCurrentIndex(tabs->count() - 1);
                    auto *raw = dialog->findChild<QPlainTextEdit *>("rawConfig");
                    QByteArray draft = validDraft;
                    if (action == 1) draft.replace("GeoIP=ru", "GeoIP=missing");
                    if (action == 3) draft += "[FutureDesktop]\nOpaque=retained\n";
                    raw->setPlainText(QString::fromUtf8(draft));
                    auto *buttons = dialog->findChild<QDialogButtonBox *>();
                    if (action == 2) {
                        QTimer::singleShot(0, dialog, [] {
                            auto *prompt = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                            require(prompt, "Cancel asks to discard unsaved Raw draft");
                            for (auto *button : prompt->buttons())
                                if (prompt->buttonRole(button) == QMessageBox::DestructiveRole) { button->click(); return; }
                            require(false, "discard action exists");
                        });
                        buttons->button(QDialogButtonBox::Cancel)->click(); return;
                    }
                    QTimer watchdog;
                    watchdog.setInterval(10);
                    QObject::connect(&watchdog, &QTimer::timeout, dialog, [&] {
                        auto *message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                        if (!message) return;
                        if (action == 0) fprintf(stderr, "Raw validation: %s\n", qPrintable(message->text()));
                        require(action == 1 || action == 3, "valid Raw Save must not show validation error");
                        require(raw->toPlainText().toUtf8() == draft && read(first) == original,
                            "invalid groups and unknown fields retain Raw draft and original");
                        rejected = true; message->accept();
                        QTimer::singleShot(0, dialog, [dialog] { dialog->QDialog::reject(); });
                    });
                    watchdog.start();
                    buttons->button(QDialogButtonBox::Save)->click();
                });
                window.editProfile(0);
                require(visited && ((action != 1 && action != 3) || rejected), "Raw modal regression completed");
                const auto saved = read(first);
                if (action == 0) {
                    require(saved.contains(ip.toUtf8()) && !saved.contains("missing-old.dat")
                        && saved.contains("GeoIP=ru") && saved.contains("# retained")
                        && saved.contains(" # [FutureDesktop]\n# Opaque=retained"),
                        "Raw Save materializes shared selection and preserves opaque content");
                } else require(saved == original, "invalid Save and Cancel preserve original bytes");
            }
        }
        require(ProfileStore::writeConfig(first, source), "restore preparation profile");
        require(window.prepareSharedGeoData(first, &error), "prepare first shared profile");
        require(read(first).contains(ip.toUtf8()) && read(first).contains("# retained") && read(first).contains("# GeoSite=missing"), "materialize only active kinds without losing blocks");
        require(read(second) == source && ProfileStore::identity(first) == profiles.first().id, "preserve inactive profile bytes and original identity");
        require(window.prepareSharedGeoData(second, &error) && read(second).contains(ip.toUtf8()), "second profile reuses shared file");
        const QByteArray secondSaved = read(second);
        ProfileEditorDialog editor(QString::fromUtf8(read(first)), window.capabilities_, nullptr, {}, core, first);
        auto *name = editor.findChild<QLineEdit *>("profileName");
        // The field need not have an object name in older editor layouts.
        if (!name) for (auto *field : editor.findChildren<QLineEdit *>()) if (field->text() == "Shared") { name = field; break; }
        require(name, "editor name field"); name->setText("Unsaved unrelated name");
        require(selection->apply({{"geoip", updated}}, core, profiles, &error), "update shared path");
        require(name->text() == "Unsaved unrelated name" && read(second) == secondSaved, "global Apply preserves unsaved controls and inactive files");
        QTimer::singleShot(500, &editor, [&] {
            auto *manager = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            require(manager && manager->objectName() == "sharedGeoDataDialog", "shared manager cancellation fixture");
            require(manager->findChild<QLabel *>("geoipStatus")->text().contains("Ready"), "manual shared candidate becomes Ready");
            QFile corrupt(updated); require(corrupt.open(QIODevice::WriteOnly), "modify manual candidate"); corrupt.write("invalid"); corrupt.close();
            QTimer::singleShot(500, manager, [manager] {
                require(manager->findChild<QLabel *>("geoipStatus")->text() == "Invalid file", "manual mutation invalidates manager Ready");
                require(!manager->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply)->isEnabled(), "invalid candidate disables Apply");
                manager->reject();
            });
        });
        editor.findChild<QPushButton *>("manageDatabases")->click();
        require(selection->paths().value("geoip") == updated && read(second) == secondSaved, "manager Cancel preserves settings and profile bytes");
        { QFile restore(updated); require(restore.open(QIODevice::WriteOnly), "restore manual candidate"); restore.write(bytes); }

        const auto beforeReadOnly = read(first);
        const auto permissions = QFile::permissions(first);
        require(QFile::setPermissions(first, QFileDevice::ReadOwner), "read-only profile fixture");
        require(!window.prepareSharedGeoData(first, &error) && read(first) == beforeReadOnly, "read-only stale profile blocks preparation without writing");
        require(QFile::setPermissions(first, permissions), "restore profile permissions");
        require(window.prepareSharedGeoData(first, &error) && read(first).contains(updated.toUtf8()), "stale references corrected before Start");
        QByteArray invalid = source; invalid.replace("GeoIP=ru", "GeoIP=missing");
        require(ProfileStore::writeConfig(first, invalid), "write invalid fixture");
        require(!window.prepareSharedGeoData(first, &error) && read(first) == invalid, "failed preparation preserves profile bytes");
        require(ProfileStore::writeConfig(first, "[Profile]\nName=No GeoData\n"), "non GeoData fixture");
        window.validationExecutable_ = "missing-core";
        require(window.prepareSharedGeoData(first, &error) && read(first) == "[Profile]\nName=No GeoData\n", "unrelated profile does not require document bridge");
        editor.QDialog::reject();
        require(selection->apply(previous, core, {}, &error), "restore shared test settings");
    }
    static void captureGeoSite(QWidget *parent, const QString &destination) {
        auto settleSearch = [] {
            QEventLoop loop; QTimer::singleShot(200, &loop, &QEventLoop::quit); loop.exec();
        };
        const QString catalogPath = qEnvironmentVariable("BIMERON_GEOSITE_IDS");
        if (!catalogPath.isEmpty()) {
            QFile catalogFile(catalogPath); require(catalogFile.open(QIODevice::ReadOnly), "catalog IDs fixture");
            QStringList catalog;
            for (const auto &id : QJsonDocument::fromJson(catalogFile.readAll()).array()) catalog.append(id.toString().toLower());
            GeoSitePicker picker(parent, catalog, {"category-games-!cn", "youtube", "missing-fixture"}, {});
            picker.findChild<QListWidget *>("geoSiteSections")->setCurrentRow(2);
            // A hidden parent has no reliable monitor placement on multi-screen hosts.
            // Anchor every DPR sample to the primary screen before native creation.
            auto *captureScreen = QApplication::primaryScreen();
            picker.setScreen(captureScreen);
            picker.move(captureScreen->availableGeometry().topLeft() + QPoint(24, 24));
            picker.show(); settleSearch();
            require(picker.grab().save(destination + ".catalog.png"), "native catalog preview");
            auto *topics = picker.findChild<QComboBox *>("geoSiteTopics");
            topics->setCurrentIndex(topics->findData("2-video"));
            settleSearch();
            require(picker.grab().save(destination + ".topic.png"), "native topic preview");
            for (const QString &topicId : {QString("3-operating-systems"), QString("3-updates-drivers"), QString("4-business-crm"), QString("5-weather"), QString("4-ai")}) {
                picker.findChild<QListWidget *>("geoSiteSections")->setCurrentRow(topicId == "4-ai" ? 7 : topicId.startsWith("4-") ? 4 : topicId.startsWith("5-") ? 5 : 3);
                topics->setCurrentIndex(topics->findData(topicId));
                require(topics->currentData() == topicId, "OS split capture selects actual topic");
                settleSearch();
                require(picker.grab().save(destination + "." + topicId + ".png"), "native OS split preview");
            }
            for (const QString &query : {QString("AI"), QString::fromUtf8("ИИ"), QString("Codex"), QString("Claude"), QString("928Net"), QString("RU-CENTER"), QString("CNET"), QString("madshi"), QString("PayPal"), QString("NVIDIA"), QString("xiaomi-ai")}) {
                picker.findChild<QLineEdit *>()->setText(query);
                settleSearch();
                require(picker.grab().save(destination + ".search-" + query + ".png"), "native bilingual topic search preview");
            }
            picker.findChild<QLineEdit *>()->clear();
            for (int section : {6, 7, 8}) {
                picker.findChild<QListWidget *>("geoSiteSections")->setCurrentRow(section);
                settleSearch();
                require(picker.grab().save(destination + ".section-" + QString::number(section) + ".png"), "native eight-section preview");
            }
            for (const QString &id : {QString("google"), QString("youtube"), QString("github"), QString("neuralink")}) {
                picker.findChild<QListWidget *>("geoSiteSections")->setCurrentRow(geoSiteEntries().value(id).section + 1);
                picker.findChild<QLineEdit *>()->setText(id);
                settleSearch();
                require(picker.grab().save(destination + ".distinction-" + id + ".png"), "native ecosystem/product distinction");
            }
            // Capture every newly placed mixed-scope record by exact original ID.
            for (const QString &id : QStringList{"8btc", "cnet", "falungong", "filimo", "hainanairlines", "hinet", "hisense", "hkbn", "hketgroup", "hupu", "icable", "idg", "ifanr", "kkbox", "kuaishou", "ltn", "madshi", "mega", "nvidia", "okaapps", "oschina", "paypal", "phoenix", "pornhub", "protonmail", "quora", "renren", "secom", "sharethis", "sina", "sohu", "tbank-ru", "uc", "verizon", "vivo", "wiwide", "x", "xiaoheihe", "xiaohongshu", "zhangtao"}) {
                picker.findChild<QLineEdit *>()->setText(id);
                settleSearch();
                require(picker.grab().save(destination + ".mixed-" + id + ".png"), "native mixed-scope preview");
            }
            picker.findChild<QLineEdit *>()->clear();
            picker.findChild<QListWidget *>("geoSiteSections")->setCurrentRow(9);
            settleSearch();
            require(picker.grab().save(destination + ".unknown.png"), "native unknown preview");
            picker.findChild<QPushButton *>("geoSiteSelectionCount")->click();
            settleSearch();
            require(picker.grab().save(destination + ".selected.png"), "native selected preview");
            auto *selectedMode = picker.findChild<QPushButton *>("geoSiteSelectionCount");
            auto *query = picker.findChild<QLineEdit *>();
            query->setText("youtube"); settleSearch();
            require(picker.grab().save(destination + ".selected-search.png"), "native selected search intersection");
            query->setText("fixture-no-match"); settleSearch();
            require(picker.grab().save(destination + ".selected-empty.png"), "native selected no-match state");
            query->clear();
            selectedMode->click();
            query->setText("a"); settleSearch();
            require(picker.grab().save(destination + ".grouped-search.png"), "native grouped global search and clear button");
            query->clear();
            picker.resize(590, 350); settleSearch();
            if (picker.size() != QSize(590, 350))
                fprintf(stderr, "Picker minimum: %dx%d, hint %dx%d, DPR %.6f, screen %s\n",
                    picker.width(), picker.height(), picker.minimumSizeHint().width(), picker.minimumSizeHint().height(),
                    picker.devicePixelRatioF(), qPrintable(picker.screen()->name()));
            require(picker.size() == QSize(590, 350), "minimum picker geometry");
            require(picker.grab().save(destination + ".minimum.png"), "native minimum picker preview");
            QFile geometry(destination + ".catalog.json");
            require(geometry.open(QIODevice::WriteOnly), "catalog geometry log");
            geometry.write(QJsonDocument(QJsonObject{{"dpr", picker.devicePixelRatioF()},
                {"width", picker.width()}, {"height", picker.height()},
                {"screen", picker.screen()->name()},
                {"sectionsWidth", picker.findChild<QListWidget *>("geoSiteSections")->width()},
                {"topicWidth", topics->width()}}).toJson());
            picker.reject();
        }
    }
    // Acceptance: recovery controls remain within the window or scroll content.
    static int recoveryGeometry(QApplication &app, QString path) {
        require(app.arguments().contains("--visual-check") && !path.isEmpty(), "isolated recovery capture");
        class Translator final : public QTranslator {
        public:
            QJsonObject strings;
            QString translate(const char *, const char *text, const char *, int) const override { return strings.value(QString::fromUtf8(text)).toString(); }
            bool isEmpty() const override { return strings.isEmpty(); }
        } translator;
        QFile translations(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../assets/translations/ru.json"));
        require(translations.open(QIODevice::ReadOnly), "geometry translations");
        translator.strings = QJsonDocument::fromJson(translations.readAll()).object();
        if (!app.arguments().contains("--geometry-english")) app.installTranslator(&translator);
        const int themeArgument = app.arguments().indexOf("--geometry-theme");
        if (themeArgument >= 0) QSettings().setValue("app/theme", app.arguments().value(themeArgument + 1));
        applyThemePalette(app);
        MainWindow window;
        window.core_.cancel();
        window.developerMode_ = true;
        window.core_.setEndpoint("bimeron-geometry-" + QUuid::createUuid().toString());
        for (auto *timer : window.findChildren<QTimer *>()) timer->stop();
        for (auto *list : window.findChildren<QListWidget *>())
            if (!list->count()) list->addItems({QString::fromUtf8("Основной сервер\nVLESS → Системный VPN"), QString::fromUtf8("Резервный сервер\nVLESS → Системный VPN")});
        window.resize(720, 600);
        auto *captureScreen = QApplication::primaryScreen();
        window.setScreen(captureScreen);
        window.move(captureScreen->availableGeometry().topLeft() + QPoint(40, 40));
        window.coreManagement_->setScreen(captureScreen);
        window.coreManagement_->move(captureScreen->availableGeometry().topLeft() + QPoint(80, 80));
        window.show();
        QJsonArray observations;
        const auto capture = [&](QString state, QWidget *surface) {
            QEventLoop loop;
            QTimer::singleShot(300, &loop, &QEventLoop::quit);
            loop.exec();
            for (auto *button : surface->findChildren<QAbstractButton *>()) {
                if (!button->isVisibleTo(surface)) continue;
                QWidget *bounds = surface;
                for (auto *parent = button->parentWidget(); parent && parent != surface; parent = parent->parentWidget())
                    if (auto *scroll = qobject_cast<QScrollArea *>(parent)) { bounds = scroll->widget(); break; }
                require(bounds && bounds->rect().contains(QRect(button->mapTo(bounds, QPoint()), button->size())),
                        "actionable control inside window or scroll content");
            }
            const QPixmap shot = surface->grab();
            require(shot.save(path + "." + state + ".png"), "native recovery screenshot");
            observations.append(QJsonObject{{"state", state}, {"width", surface->width()}, {"height", surface->height()},
                {"dpr", shot.devicePixelRatioF()}, {"pixelWidth", shot.width()}, {"pixelHeight", shot.height()}});
        };
        window.startupPreparationPending_ = true;
        window.helloReady_ = false;
        window.statusNeeded_ = true;
        window.updateCoreAvailability();
        capture("preparing", &window);
        window.startupPreparationPending_ = false;
        window.helloReady_ = true;
        window.statusNeeded_ = false;
        window.state_ = "disconnected";
        window.connectionLabel_->setText(QObject::tr("Disconnected"));
        window.updateCoreAvailability();
        capture("ready", &window);
        window.helloReady_ = false;
        window.serviceObservation_ = MainWindow::ServiceObservation::Stopped;
        window.serviceStatusLabel_->setText(QObject::tr("stopped"));
        window.updateCoreAvailability();
        capture("error", &window);
        window.settingsNavigation_->click();
        capture("settings", &window);
        window.openCoreManagement();
        require(window.coreManagement_, "management dialog");
        require(window.coreManagement_->findChildren<QDialog *>().isEmpty(), "service actions never open a nested empty management dialog");
        capture("dialog", window.coreManagement_);
        auto *details = window.coreManagement_->findChild<QPushButton *>("coreManagementDetailsToggle");
        require(details && window.coreManagement_->findChildren<QScrollArea *>().isEmpty(), "recovery dialog has no scroll areas");
        require(!window.repairFindings_->isVisibleTo(window.coreManagement_)
            && !window.repairRefresh_->isVisibleTo(window.coreManagement_),
            "collapsed repair excludes findings and refresh");
        require(details->mapTo(window.coreManagement_, QPoint()).y()
            == window.coreManagementClose_->mapTo(window.coreManagement_, QPoint()).y(),
            "Details and Close share one footer");
        const QSize collapsed = window.coreManagement_->size();
        window.serviceStartBlock_ = DevelopmentCore::StartBlock::CleanupUnconfirmed;
        window.startupDiagnostic_ = QObject::tr("System service start is blocked: a local core, session lock or unverified recovery state may remain.");
        window.updateCoreAvailability();
        details->click();
        capture("dialog-expanded", window.coreManagement_);
        require(window.repairFindings_->isVisibleTo(window.coreManagement_), "findings appear only in Details");
        require(window.coreManagement_->height() > collapsed.height(), "expanded dialog fits additional content");
        require(window.coreManagement_->minimumSize() == window.coreManagement_->maximumSize(), "dialog cannot be resized manually");
        details->click();
        window.serviceStartBlock_ = DevelopmentCore::StartBlock::None;
        window.startupDiagnostic_.clear();
        window.updateCoreAvailability();
        capture("dialog-collapsed", window.coreManagement_);
        require(window.coreManagement_->size() == collapsed, "collapsing restores content-sized bounds");
        for (bool available : {false, true}) {
            window.repairSnapshotTest_ = [available](auto &snapshot) {
                snapshot.helperAvailable = available;
                snapshot.helperTrusted = false;
            };
            window.updateCoreAvailability();
            require(window.repairDiagnosis_.cause == (available ? ConnectionRepair::Cause::ComponentsUntrusted
                                                               : ConnectionRepair::Cause::ComponentsMissing),
                    "helper evidence selects consistent cause");
            const QString name = available ? "untrusted-helper" : "missing-helper";
            capture(name + "-collapsed", window.coreManagement_);
            details->click();
            capture(name + "-expanded", window.coreManagement_);
            require(window.serviceInstallButton_->isHidden() && window.serviceStartButton_->isHidden(),
                    "contextual commands stay hidden in expanded details");
            require(window.coreManagement_->findChildren<QDialog *>().isEmpty(), "no nested management modal");
            details->click();
        }
        window.repairSnapshotTest_ = {};
        window.updateCoreAvailability();
        window.repairSnapshotTest_ = [](auto &snapshot) { snapshot.interrupted = true; };
        window.updateCoreAvailability();
        capture("interrupted", window.coreManagement_);
        window.repairSnapshotTest_ = {};
        window.probeCategory_ = "malformed-response";
        window.probeExitCode_ = 7;
        window.probeDurationMs_ = 23;
        window.probeObservedAt_ = QDateTime::currentMSecsSinceEpoch();
        window.updateCoreAvailability();
        details->click();
        capture("probe-failure-expanded", window.coreManagement_);
        details->click();
        window.probeCategory_.clear();
        window.repairCommandPending_ = true;
        window.serviceCommandDescription_ = QObject::tr("Confirming cleanup");
        window.updateCoreAvailability();
        capture("executing", window.coreManagement_);
        window.repairCommandStarted_ = window.pollClock_.elapsed();
        window.repairWatchdogMs_ = 0;
        window.updateCoreAvailability();
        capture("helper-watchdog", window.coreManagement_);
        window.coreManagement_->hide();
        window.openCoreManagement();
        capture("helper-watchdog-reopened", window.coreManagement_);
        window.repairCommandPending_ = false;
        window.repairCommandStarted_ = -1;
        window.repairWatchdogExpired_ = false;
        window.repairWatchdogMs_ = 120000;
        window.updateCoreAvailability();

        for (const auto state : {MainWindow::ServiceObservation::Missing, MainWindow::ServiceObservation::Running,
                                MainWindow::ServiceObservation::Unknown, MainWindow::ServiceObservation::Failed}) {
            window.serviceObservation_ = state;
            window.updateCoreAvailability();
            capture(QString("remedy-%1").arg(int(state)), window.coreManagement_);
        }
        window.serviceStartBlock_ = DevelopmentCore::StartBlock::LockOccupied;
        window.updateCoreAvailability();
        capture("unknown-owner", window.coreManagement_);
        window.settingsRetry_->click();
        capture("unknown-discovery", window.coreManagement_);
        window.serviceStartBlock_ = DevelopmentCore::StartBlock::None;
        window.availabilityFailure_ = CoreClient::Failure::Incompatible;
        window.updateCoreAvailability();
        capture("incompatible", window.coreManagement_);
        window.availabilityFailure_ = CoreClient::Failure::Transport;
        window.repairFailure_ = QObject::tr("Network cleanup could not be confirmed");
        window.updateCoreAvailability();
        capture("failed", window.coreManagement_);
        window.repairFailure_.clear();
        window.availabilityVerificationDeadline_ = window.pollClock_.elapsed() + 30000;
        window.updateCoreAvailability();
        capture("verification", window.coreManagement_);
        window.helloReady_ = true;
        window.statusNeeded_ = false;
        window.updateCoreAvailability();
        capture("success", window.coreManagement_);
        QFile log(path + ".json");
        require(log.open(QIODevice::WriteOnly), "recovery geometry log");
        log.write(QJsonDocument(observations).toJson());
        app.removeTranslator(&translator);
        return 0;
    }

    static int profileGeometry(QApplication &app, const QString &destination) {
        class Translator final : public QTranslator {
        public:
            QJsonObject strings;
            QString translate(const char *, const char *text, const char *, int) const override { return strings.value(QString::fromUtf8(text)).toString(); }
            bool isEmpty() const override { return strings.isEmpty(); }
        } translator;
        QFile translations(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../assets/translations/ru.json"));
        require(translations.open(QIODevice::ReadOnly), "geometry translations");
        translator.strings = QJsonDocument::fromJson(translations.readAll()).object();
        if (!app.arguments().contains("--geometry-english")) app.installTranslator(&translator);
        const int themeArgument = app.arguments().indexOf("--geometry-theme");
        if (themeArgument >= 0) QSettings().setValue("app/theme", app.arguments().value(themeArgument + 1));
        applyThemePalette(app);
        if (app.arguments().contains("--d3-only")) {
            MainWindow window;
            auto plan = NativeIKEv2SessionTest::publicPlan();
            plan.insert("authentication", "IKEv2 only; EAP-MSCHAPv2; no automatic redial");
            plan.insert("ipsec", "IKE AES256/SHA256/DH14; ESP AES256/SHA256; no PFS");
            plan.insert("credentials", "Profile preparation does not read or write credentials.");
            plan.insert("trust", "Administrator-managed CA validation; no trust policy changes.");
            plan.insert("ipv6_routes", QJsonArray{"::/1", "8000::/1"});
            auto *dialog = ProfileEditorDialog::nativeConfirmation(&window, "fixture", "vpn.example", "remove", true, plan);
            dialog->show(); QCoreApplication::processEvents();
            require(dialog->grab().save(destination), "complete D3 plan capture");
            delete dialog;
            NativeIKEv2SessionTest::Fixture provision;
            window.bindNativeController(&provision.controller);
            window.nativeActions_.select("fixture", "vpn.example", "remove");
            provision.controller.setLaunchGates({true,true,true,true});
            window.nativeActions_.preview(); NativeIKEv2SessionTest::startProvision(provision);
            window.show(); QCoreApplication::processEvents();
            require(window.connectionLabel_->wordWrap(), "native connection status wraps");
            require(window.connectionLabel_->isVisible() && window.connectionLabel_->text().contains(MainWindow::tr("Windows profile plan is ready for review.")), "localized review status visible");
            require(window.controlBlocked(), "review retains packet blocking");
            require(window.grab().save(destination + ".review.png"), "D3 review capture");
            for (const auto &code : {QString("ikev2_profile_preview_changed"), QString("ikev2_profile_rollback_incomplete_manual_review_required"), QString("native_operation_failed")}) {
                auto reply = provision.reply(); reply.insert("operation", "failed"); reply.insert("error", code);
                provision.observe(reply); QCoreApplication::processEvents();
                const QString expected = code == "ikev2_profile_preview_changed"
                    ? MainWindow::tr("Windows profile plan changed. A fresh review is required.")
                    : MainWindow::tr("Windows profile operation failed. Manual review is required.");
                require(window.connectionLabel_->isVisible() && window.connectionLabel_->text().contains(expected), "localized failure status visible");
                require(window.controlBlocked(), "provisioning blocks packet connection");
                require(window.grab().save(destination + "." + code + ".png"), "D3 failure capture");
            }
            provision.controller.cancelExchange(); QCoreApplication::processEvents();
            require(window.controlBlocked(), "unknown retains packet blocking");
            require(window.connectionLabel_->isVisible() && window.connectionLabel_->text().contains(MainWindow::tr("Windows profile outcome is unknown. Manual review is required.")), "localized unknown status visible");
            require(window.grab().save(destination + ".unknown.png"), "D3 unknown capture");
            app.removeTranslator(&translator); return 0;
        }
        downloadFeedback(destination);
        MainWindow window;
        window.core_.cancel();
        if (app.arguments().contains("--geosite-only")) {
            require(!qEnvironmentVariable("BIMERON_GEOSITE_IDS").isEmpty(), "real catalog for isolated picker captures");
            captureGeoSite(&window, destination);
            app.removeTranslator(&translator);
            return 0;
        }
        window.developerMode_ = true;
        window.core_.setEndpoint("bimeron-geometry-" + QUuid::createUuid().toString());
        for (auto *timer : window.findChildren<QTimer *>()) timer->stop();
        QTemporaryDir directory;
        require(directory.isValid(), "geometry directory");
        const QString path = directory.filePath("profile.conf");
        require(ProfileStore::writeConfig(path, "[Profile]\nVersion=1\nName=Size\n[AccessKey]\nSSConf=ssconf://example.invalid\n[Interface]\nMode=vpn\n"
            "[Rule]\nGeoIP=cloudflare,private,ru\nAction=direct\n[Rule]\nGeoSite=github,youtube,google\nAction=proxy\n"), "geometry profile");
        window.profiles_ = {{"geometry", "Size", path}};
        window.capabilities_ = {{"vpn", true}, {"proxy", true}, {"ip_split", true}, {"domain_split", true}, {"ipv6", true}, {"kill_switch", true},
            {"geoip_split", true}, {"geosite_split", true}, {"geodata_schema", "v2ray-protobuf"}};
        window.validationExecutable_ = qEnvironmentVariable("BIMERON_GEODATA_CORE");
        QJsonArray observations;
        const auto capture = [&] {
            auto *dialog = qobject_cast<ProfileEditorDialog *>(QApplication::activeModalWidget());
            require(dialog, "geometry modal editor");
            QJsonArray widgets;
            QList<QWidget *> all{dialog};
            all.append(dialog->findChildren<QWidget *>());
            for (auto *widget : all) {
                const QSize hint = widget->minimumSizeHint();
                if (widget == dialog || hint.height() > 200 || widget->minimumHeight() > 200 || qobject_cast<QTabWidget *>(widget))
                    widgets.append(QJsonObject{{"class", widget->metaObject()->className()}, {"name", widget->objectName()},
                        {"visible", widget->isVisibleTo(dialog)}, {"width", widget->width()}, {"height", widget->height()},
                        {"minimumWidth", widget->minimumWidth()}, {"minimumHeight", widget->minimumHeight()},
                        {"hintWidth", hint.width()}, {"hintHeight", hint.height()},
                        {"layoutMinimumHeight", widget->layout() ? widget->layout()->minimumSize().height() : -1},
                        {"sizeHintHeight", widget->sizeHint().height()},
                        {"heightForWidth", widget->heightForWidth(widget->width())},
                        {"layoutHeightForWidth", widget->layout() ? widget->layout()->totalHeightForWidth(widget->width()) : -1}});
            }
            observations.append(QJsonObject{{"width", dialog->width()}, {"height", dialog->height()}, {"dpr", dialog->devicePixelRatioF()}, {"widgets", widgets}});
            return dialog;
        };
        QTimer::singleShot(0, &window, [&] { capture(); });
        QTimer::singleShot(500, &window, [&] {
            auto *dialog = capture();
            auto *tabs = dialog->findChild<QTabWidget *>();
            require(tabs, "geometry tabs");
            for (int index = 0; index < tabs->count(); ++index) {
                if (!tabs->isTabVisible(index) || !tabs->isTabEnabled(index)) continue;
                tabs->setCurrentIndex(index);
                QApplication::processEvents();
                require(dialog->size() == QSize(680, 520), "switching tabs must not grow the dialog");
            }
            tabs->setCurrentIndex(0);
            QApplication::processEvents();
            require(dialog->grab().save(destination), "geometry screenshot");
            require(!window.styleSheet().isEmpty() && !QPixmap(":/icons/check.xpm").isNull()
                && !QPixmap(":/icons/chevron-down.xpm").isNull(), "real application style and embedded picker icons");
            auto *table = dialog->findChild<QTableWidget *>();
            if (!window.validationExecutable_.isEmpty()) {
            require(table && table->rowCount() == 2, "synthetic GeoIP and GeoSite table rows");
            for (int index = 0; index < tabs->count(); ++index)
                if (tabs->widget(index)->isAncestorOf(table)) tabs->setCurrentIndex(index);
            QApplication::processEvents();
            require(table->isVisible() && dialog->size() == QSize(680, 520), "compact visible routing table");
            require(dialog->grab().save(destination + ".routing.png"), "styled routing table screenshot");
            for (int row = 0; row < 2; ++row) {
                auto *target = qobject_cast<QLineEdit *>(table->cellWidget(row, 2));
                require(target && !target->actions().isEmpty() && !target->actions().first()->icon().isNull(), "actual destination picker action");
                const QStringList ids = target->property("groupIds").toString().split(',');
                require(ids.size() == 3, "manual picker IDs");
                for (int pass = 0; pass < 2; ++pass) {
                    QTimer::singleShot(100, dialog, [&, row, pass] {
                        auto *popup = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                        require(popup && popup != dialog && popup->parentWidget() == dialog, "actual inherited-style group popup");
                        auto *list = row == 0 ? popup->findChild<QListWidget *>() : popup->findChild<QListWidget *>("geoSiteGroups");
                        auto *search = popup->findChild<QLineEdit *>();
                        require(list && search && list->sizeHintForRow(0) < 48, "compact native group rows");
                        int groupCount = 0;
                        for (int i = 0; i < list->count(); ++i) groupCount += list->item(i)->data(Qt::UserRole).isValid();
                        require(groupCount == (pass == 0 ? 3 : 2), "all source IDs present independently of metadata headings");
                        require(popup->width() == (row == 0 ? 440 : 728) && popup->height() == (row == 0 ? 420 : 473), "compact group popup geometry");
                        if (row == 0) {
                            QListWidgetItem *russia = nullptr;
                            for (int index = 0; index < list->count(); ++index)
                                if (list->item(index)->data(Qt::UserRole).toString() == "ru") russia = list->item(index);
                            require(russia, "country search fixture");
                            require(russia->text().startsWith(QObject::tr("Russia") + " (RU)"), "country follows active translator");
                            for (const QString &query : {QString("Russia"), QString::fromUtf8("Россия"), QString("RU")}) {
                                search->setText(query);
                                require(!russia->isHidden(), "both country languages and code match independently of UI language");
                            }
                            search->clear();
                        }
                        if (row == 0 && pass == 0) {
                            require(list->item(0)->checkState() == Qt::Checked, "manual selected group starts checked");
                            list->setCurrentRow(0); list->setFocus();
                            QKeyEvent space(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
                            QApplication::sendEvent(list, &space);
                            require(list->item(0)->checkState() == Qt::Unchecked, "Space toggles native check state");
                            search->setText("fixture-no-matching-group");
                            require(list->item(0)->isHidden() && list->item(1)->checkState() == Qt::Checked, "search preserves hidden selections");
                            search->clear();
                        } else if (row == 0) {
                            require(list->item(0)->data(Qt::UserRole).toString() == ids.at(1)
                                && list->item(1)->data(Qt::UserRole).toString() == ids.at(2)
                                && list->item(0)->checkState() == Qt::Checked
                                && list->item(1)->checkState() == Qt::Checked, "reopened picker places checked groups first");
                        }
                        if (row == 1 && pass == 0) {
                            for (int i = 0; i < list->count(); ++i)
                                if (list->item(i)->data(Qt::UserRole) == ids.first()) list->item(i)->setCheckState(Qt::Unchecked);
                        }
                        QApplication::processEvents();
                        const QString suffix = row == 0 ? ".groups.png" : ".geosite-groups.png";
                        if (pass == 0) require(popup->grab().save(destination + suffix), "styled native group popup screenshot");
                        popup->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
                    });
                    target->actions().first()->trigger();
                    require(target->property("groupIds").toString() == ids.mid(1).join(','), "picker acceptance preserves selected IDs");
                }
            }
            }
            captureGeoSite(&window, destination);
            QFile log(destination + ".json");
            require(log.open(QIODevice::WriteOnly), "geometry log");
            log.write(QJsonDocument(observations).toJson()); log.close();
            auto *manage = dialog->findChild<QPushButton *>("manageDatabases");
            require(manage, "database manager action");
            QTimer::singleShot(500, dialog, [&] {
                auto *manager = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                require(manager && manager->objectName() == "sharedGeoDataDialog", "actual database modal path");
                require(manager->width() >= 440 && manager->width() <= 540 && manager->height() <= 340,
                    "database manager must remain compact after native layout events");
                require(!manager->windowIcon().isNull(), "manager uses real application icon");
                require(manager->grab().save(destination + ".databases.png"), "database geometry screenshot");
                if (QApplication::platformName() == "windows") {
                    const QRect frame = manager->frameGeometry();
                    require(manager->screen()->grabWindow(0, frame.x(), frame.y(), frame.width(), frame.height())
                        .save(destination + ".native-frame.png"), "native decorated manager screenshot");
                }
                QFile sizes(destination + ".databases.json"); require(sizes.open(QIODevice::WriteOnly), "database geometry log");
                sizes.write(QJsonDocument(QJsonObject{{"width", manager->width()}, {"height", manager->height()},
                    {"dpr", manager->devicePixelRatioF()}, {"font", manager->font().family()},
                    {"pointSize", manager->font().pointSizeF()}, {"nativeTitleBar", !manager->windowFlags().testFlag(Qt::FramelessWindowHint)}}).toJson());
                auto *details = manager->findChild<QToolButton *>("geodataDetails");
                require(details && !details->isChecked(), "database details start collapsed");
                require(details->arrowType() == Qt::NoArrow && !details->icon().isNull() && details->iconSize() == QSize(9, 5), "compact product chevron");
                details->click(); QApplication::processEvents();
                require(details->isChecked() && details->arrowType() == Qt::NoArrow, "accessible expanded state retains product icon");
                require(manager->height() <= 500, "expanded details remain bounded");
                require(manager->grab().save(destination + ".details.png"), "database details screenshot");
                QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
                QApplication::sendEvent(manager, &tab);
                require(manager->focusWidget(), "manager keyboard focus remains accessible");
                QKeyEvent backtab(QEvent::KeyPress, Qt::Key_Backtab, Qt::ShiftModifier);
                QApplication::sendEvent(manager, &backtab);
                QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                QApplication::sendEvent(manager, &escape);
                require(manager->result() == QDialog::Rejected, "Escape cancels database manager");
            });
            manage->click();
            dialog->QDialog::reject();
        });
        window.show();
        window.editProfile(0);
        require(observations.size() == 2, "initial and settled geometry observations");
        for (const auto &value : observations) {
            const auto geometry = value.toObject();
            require(geometry.value("width").toInt() == 680 && geometry.value("height").toInt() == 520,
                "profile modal must retain 680x520 after queued native layout events");
        }
        window.core_.cancel();
        app.removeTranslator(&translator);
        return 0;
    }
    static int preview(QApplication &app, const QString &destination, const QString &language) {
        QSettings().setValue("app/theme", app.arguments().contains("--ui-dark") ? "dark" : "light");
        applyThemePalette(app);
        class Translator final : public QTranslator {
        public:
            QJsonObject strings;
            QString translate(const char *, const char *source, const char *, int) const override {
                return strings.value(QString::fromUtf8(source)).toString();
            }
            bool isEmpty() const override { return strings.isEmpty(); }
        } translator;
        if (language == "ru") {
            QFile translations(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../assets/translations/ru.json"));
            require(translations.open(QIODevice::ReadOnly), "preview translations");
            translator.strings = QJsonDocument::fromJson(translations.readAll()).object();
            app.installTranslator(&translator);
        }
        QSettings().setValue("app/theme", app.arguments().contains("--preview-dark") ? "dark" : "light");
        applyThemePalette(app);
        MainWindow window;
        if (app.arguments().contains("--shadowtls-preview")) {
            window.core_.cancel();
            for (auto *timer : window.findChildren<QTimer *>()) timer->stop();
            ProfileEditorDialog editor("[Profile]\nName = Example\n[AccessKey]\nType = shadowsocks\nServer = example.invalid\nPort = 443\nMethod = aes-256-gcm\nPassword = synthetic\n[ShadowTLS]\nServerName = www.example.com\nPassword = synthetic\n", {{"proxy", true}, {"vpn", true}}, &window);
            auto *tabs = editor.findChild<QTabWidget *>();
            tabs->setCurrentIndex(tabs->count() - 2);
            editor.resize(720, 600);
            editor.show();
            app.processEvents();
            require(editor.grab().save(destination), "styled ShadowTLS preview");
            return 0;
        }
        const int settingsPreview = app.arguments().indexOf("--settings-preview");
        if (settingsPreview >= 0) {
            // State: off | profile | long | stale | empty. Captures the Settings page and its open selector.
            const QString state = app.arguments().value(settingsPreview + 1);
            window.core_.cancel();
            for (auto *timer : window.findChildren<QTimer *>()) timer->stop();
            window.autoConnectAttempted_ = true;
            QTemporaryDir directory;
            QList<Profile> profiles;
            if (state != "empty") {
                const QStringList names{"a20a542725", "Home", "Home",
                    "Office through the very long backup tunnel name that must not overflow"};
                for (int i = 0; i < names.size(); ++i) {
                    const QString path = directory.filePath(QString("p%1.conf").arg(i));
                    QFile config(path);
                    require(config.open(QIODevice::WriteOnly), "settings preview profile");
                    config.write("[Profile]\nName = P\n[AccessKey]\nSOCKS5 = socks5://localhost:1080\n[Interface]\nMode = proxy\n");
                    profiles.append({ProfileStore::identity(path), names[i], path});
                }
            }
            QSettings().setValue("app/autoConnect", state != "off" && state != "empty");
            QSettings().setValue("app/autoConnectProfile", state == "stale" ? QStringLiteral("removed")
                : profiles.isEmpty() ? QString() : profiles[state == "long" ? 3 : 0].id);
            window.store_ = ProfileStore(directory.path());
            require(window.store_.save(profiles), "settings preview store");
            window.refreshProfiles();
            window.pages_->setCurrentIndex(1);
            window.settingsNavigation_->setChecked(true);
            const int size = app.arguments().indexOf("--settings-size");
            if (size >= 0) {
                const auto parts = app.arguments().value(size + 1).split('x');
                window.resize(parts.value(0).toInt(), parts.value(1).toInt());
            }
            window.show();
            QCoreApplication::processEvents();
            require(window.grab().save(destination), "settings preview image");
            window.raise();
            window.activateWindow();
            QTimer::singleShot(300, &window, [&] {
                window.autoConnectProfile_->setFocus();
                window.autoConnectProfile_->showPopup();
            });
            QTimer::singleShot(900, &window, [&] {
                QWidget *popup = window.autoConnectProfile_->view()->window();
                require(popup->isVisible(), "settings selector popup");
                require(popup->grab().save(destination + ".open.png"), "settings selector popup image");
                window.autoConnectProfile_->hidePopup();
                QCoreApplication::quit();
            });
            app.exec();
            return 0;
        }
        if (app.arguments().contains("--ssh-preview")) {
            window.state_ = "connecting";
            window.handleSSHAuthentication({{"id", "preview"}, {"kind", "keyboard_interactive"},
                {"prompts", QJsonArray{"Account password", "One-time authentication code"}}, {"echo", QJsonArray{false, false}}});
            if (app.arguments().contains("--ssh-eight")) {
                window.clearSSHAuthentication();
                QJsonArray prompts, echo;
                for (int i = 0; i < 8; ++i) {
                    prompts.append(QString("Untrusted server prompt %1: ").arg(i + 1) + QString(220, 'x'));
                    echo.append(false);
                }
                window.handleSSHAuthentication({{"id", "eight"}, {"kind", "keyboard_interactive"},
                    {"prompts", prompts}, {"echo", echo}});
            }
            QCoreApplication::processEvents();
            require(window.sshDialog_ && window.sshDialog_->grab().save(destination), "SSH preview image");
            return 0;
        }
        window.helloReady_ = true;
        window.statusNeeded_ = false;
        QTemporaryDir directory;
        const QString path = directory.filePath("preview.conf");
        QFile profile(path);
        require(profile.open(QIODevice::WriteOnly), "preview profile");
        if (app.arguments().contains("--native-preview")) {
            profile.write("[Profile]\nName = Native\n[IKEv2]\nWindowsProfile = Fixture\nServerName = vpn.invalid\n[Interface]\nMode = vpn\n");
            profile.close();
            window.profiles_.append({"native-preview", "Native IKEv2", path});
            window.profileList_->addItem("Native IKEv2");
            window.profileList_->setCurrentRow(0);
            window.handleStatus({{"State", "disconnected"}});
            window.updateProfileAppearance();
            window.updateConnectionButton();
            NativeIKEv2SessionTest::Fixture nativePreview;
            if (app.arguments().contains("--native-bound-preview")) {
                nativePreview.discover();
                nativePreview.observe(nativePreview.sessionReply("connected", true));
                window.bindNativeController(&nativePreview.controller);
                nativePreview.controller.cancelExchange();
            }
            window.show();
            QCoreApplication::processEvents();
            const int uiState = app.arguments().indexOf("--native-ui-state");
            if (uiState >= 0) {
                NativeIKEv2Session::Snapshot snapshot;
                snapshot.state = app.arguments().value(uiState + 1);
                if (snapshot.state == "fixture-775") { snapshot.state = "failed"; snapshot.safeError = "775"; }
                snapshot.authoritative = snapshot.state != "unknown";
                snapshot.wfp = snapshot.dns = snapshot.state == "connected" ? "active" : "unknown";
                snapshot.recoveryRequired = snapshot.state == "retained";
                if (snapshot.recoveryRequired) { snapshot.state = "failed"; snapshot.wfp = "retained"; }
                window.showNativeSnapshot(snapshot);
                require(!window.connectButton_->isEnabled(), "presentation never enables native connection");
                QCoreApplication::processEvents();
            }
            if (app.arguments().contains("--native-editor-preview")) {
                ProfileEditorDialog editor(QString::fromUtf8("[Profile]\nName = Native\n[IKEv2]\nWindowsProfile = Fixture\nServerName = vpn.invalid\n[Interface]\nMode = vpn\n"), {}, &window);
                editor.show();
                if (app.arguments().contains("--ui-minimum")) editor.resize(620, 460);
                QCoreApplication::processEvents();
                require(editor.grab().save(destination), "native editor capture");
                auto *authMethod = editor.findChild<QComboBox *>("nativeAuthMethod");
                auto *authSource = editor.findChild<QComboBox *>("nativeAuthSource");
                for (int method = 0; method < 3; ++method) {
                    authMethod->setCurrentIndex(method);
                    for (int source = 0; source < (method == 0 ? 2 : 1); ++source) {
                        authSource->setCurrentIndex(source);
                        QCoreApplication::processEvents();
                        require(editor.grab().save(destination + QString(".auth-%1-%2.png").arg(method).arg(source)), "authentication capture");
                    }
                }
                authMethod->setCurrentIndex(0); authSource->setCurrentIndex(0);
                auto *actions = editor.findChild<QWidget *>("nativeProfileActions");
                require(actions && actions->isVisible(), "native actions visible");
                for (auto *button : actions->findChildren<QPushButton *>()) require(!button->isEnabled(), "native actions disabled");
                for (const auto &action : {QString("create"), QString("remove"), QString("unchanged"), QString("invalid")}) {
                    auto *confirmation = ProfileEditorDialog::nativeConfirmation(&editor, "<b>Fixture</b>", "vpn.invalid", action, true);
                    confirmation->show();
                    QCoreApplication::processEvents();
                    auto *box = confirmation->findChild<QDialogButtonBox *>();
                    require(box, "confirmation buttons");
                    int accepts = 0;
                    for (auto *button : box->buttons()) if (box->buttonRole(button) == QDialogButtonBox::AcceptRole) ++accepts;
                    require(accepts == (action == "create" || action == "remove" ? 1 : 0), "only reviewed mutations can confirm");
                    for (auto *label : confirmation->findChildren<QLabel *>())
                        if (label->text().contains("<b>")) require(label->textFormat() == Qt::PlainText, "preview markup is literal");
                    require(confirmation->grab().save(destination + "." + action + ".png"), "confirmation capture");
                    confirmation->reject();
                    require(confirmation->result() == QDialog::Rejected, "local cancellation");
                    delete confirmation;
                }
                ProfileEditorDialog packet("[Profile]\nName = Packet\n[AccessKey]\nSOCKS5 = socks5://localhost:1080\n[Interface]\nMode = proxy\n", {{"proxy", true}}, &window);
                packet.show();
                QCoreApplication::processEvents();
                require(!packet.findChild<QWidget *>("nativeProfileActions")->isVisible(), "packet editor has no native actions");
                require(!packet.findChild<QWidget *>("nativeAuthentication")->isVisible(), "packet editor has no native authentication");
                return 0;
            }
            require(window.grab().save(destination), "native unavailable preview image");
            return 0;
        }
        profile.write("[Profile]\nName = Example proxy\n[AccessKey]\nSOCKS5 = socks5://localhost:1080\n[Interface]\nMode = proxy\n");
        profile.close();
        window.profiles_.append({"preview", "Example proxy", path});
        window.profileList_->addItem("Example proxy");
        window.profileList_->setCurrentRow(0);
        window.handleStatus({{"State", "connected"}, {"Session", QJsonObject{
            {"id", "preview"}, {"profile_id", "preview"}, {"active", true}, {"mode", "proxy"}}}});
        window.show();
        QCoreApplication::processEvents();
        auto *unknown = new QListWidgetItem("Unknown example\nUnknown protocol", window.profileList_);
        unknown->setData(Qt::UserRole + 1, true);
        unknown->setSizeHint(QSize(0, 50));
        unknown->setBackground(Qt::white);
        window.profileList_->setCurrentItem(unknown);
        QCoreApplication::processEvents();
        require(window.grab().save(destination), "connection preview image");
        return 0;
    }

    static int quitFixture(QApplication &app, bool stop) {
        QLocalServer server;
        require(server.listen("bimeron-exit-" + QUuid::createUuid().toString()), "exit fixture endpoint");
        MainWindow window;
        window.core_.setEndpoint(server.serverName());
        window.helloReady_ = true;
        window.statusNeeded_ = false;
        window.instance_ = "exit";
        int requests = 0;
        QObject::connect(&server, &QLocalServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [&, socket] {
                if (!socket->canReadLine()) return;
                const auto request = QJsonDocument::fromJson(socket->readLine()).object();
                require(request.value("command").toString() == "shutdown", "explicit exit policy command");
                ++requests;
                socket->write(QJsonDocument(QJsonObject{{"version", 1}, {"id", request.value("id")},
                    {"instance", "exit"}, {"status", QJsonObject{{"State", "disconnected"}}}}).toJson(QJsonDocument::Compact) + '\n');
            });
        });
        if (window.tray_ && window.tray_->isVisible()) {
            QCloseEvent close;
            window.closeEvent(&close);
            require(!close.isAccepted() && !window.quitRequested_ && requests == 0, "close to tray must leave Core alone");
        }
        QSettings().setValue("app/stopCoreOnExit", stop);
        QTimer::singleShot(0, &window, [&] { window.requestShutdown(); });
        QTimer::singleShot(3000, &window, [] { require(false, "explicit quit must complete"); });
        app.exec();
        require(requests == (stop ? 1 : 0), "exit must honor only the explicit stop policy");
        return 0;
    }

    static void transport() {
        QLocalServer server;
        require(server.listen("bimeron-framing-" + QUuid::createUuid().toString()), "framing fixture");
        CoreClient client;
        client.setEndpoint(server.serverName());
        int replies = 0, failures = 0, received = 0;
        int mode = 0;
        CoreClient::Failure reason = CoreClient::Failure::Transport;
        QObject::connect(&client, &CoreClient::replied, &client, [&](const QString &, const QJsonObject &) { ++replies; });
        QObject::connect(&client, &CoreClient::failed, &client, [&](const QString &, CoreClient::Failure value, const QString &) {
            reason = value; ++failures;
        });
        QObject::connect(&server, &QLocalServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [&, socket] {
                if (!socket->canReadLine()) return;
                const auto request = QJsonDocument::fromJson(socket->readLine()).object();
                ++received;
                QByteArray payload;
                if (mode == 1) payload = "not JSON\n";
                else if (mode == 2) payload = QByteArray(256 * 1024 + 1, 'x');
                else payload = QJsonDocument(QJsonObject{{"version", mode == 3 ? 2 : 1},
                    {"id", mode == 4 ? QJsonValue("wrong") : request.value("id")}}).toJson(QJsonDocument::Compact) + '\n';
                if (mode == 0) {
                    socket->write(payload.left(3));
                    QTimer::singleShot(10, socket, [socket, payload] { socket->write(payload.mid(3)); });
                } else socket->write(payload);
            });
        });
        require(client.request("hello"), "first request accepted");
        require(!client.request("status"), "busy client must reject conflicting request explicitly");
        require(waitFor([&] { return replies == 1; }) && received == 1, "fragmented response must be assembled");
        const CoreClient::Failure expected[]{CoreClient::Failure::InvalidResponse, CoreClient::Failure::TooLarge,
            CoreClient::Failure::Incompatible, CoreClient::Failure::InvalidResponse};
        for (mode = 1; mode <= 4; ++mode) {
            const int previous = failures;
            client.request("hello");
            require(waitFor([&] { return failures > previous; }), "invalid response must fail");
            require(reason == expected[mode - 1], "structured failure must identify the cause");
        }
        const int previous = received;
        require(!client.request("start", {{"config", QString(17000, 'x')}}), "oversized request must be rejected locally");
        require(reason == CoreClient::Failure::TooLarge && received == previous, "oversized request must never reach Core");
    }

    static void sshAuthentication() {
        MainWindow window;
        window.state_ = "connecting";
        const auto challenge = [](const QString &id, const QString &kind = "keyboard_interactive") {
            return QJsonObject{{"id", id}, {"kind", kind}, {"prompts", QJsonArray{"<b>OTP</b>", "Password"}},
                {"echo", QJsonArray{true, false}}};
        };
        window.handleSSHAuthentication(challenge("one"));
        require(window.sshDialog_, "SSH dialog must be nonmodal");
        auto inputs = window.sshDialog_->findChildren<QLineEdit *>();
        auto promptViews = window.sshDialog_->findChildren<QPlainTextEdit *>();
        require(promptViews.size() == 2 && promptViews[0]->isReadOnly()
            && promptViews[0]->toPlainText() == "<b>OTP</b>", "server prompt remains literal text");
        require(inputs.size() == 2 && inputs[0]->echoMode() == QLineEdit::Normal
            && inputs[1]->echoMode() == QLineEdit::Password, "SSH echo behavior");
        for (auto *label : window.sshDialog_->findChildren<QLabel *>())
            require(label->textFormat() == Qt::PlainText, "SSH prompts are untrusted plain text");
        inputs[0]->setText("OTP_SENTINEL");
        inputs[1]->setText("PASSWORD_SENTINEL");
        auto *buttons = window.sshDialog_->findChild<QDialogButtonBox *>();
        buttons->button(QDialogButtonBox::Ok)->click();
        require(window.sshReplyReady_ && window.sshAnswers_.size() == 2, "SSH queues one reply");
        require(inputs[0]->text().isEmpty() && inputs[1]->text().isEmpty(), "SSH input cleared on submit");
        window.clearSSHAuthentication();
        require(window.sshAnswers_.isEmpty() && !window.sshReplyReady_, "SSH cancellation discards pending reply");
        window.handleSSHAuthentication(challenge("one"));
        require(!window.sshDialog_, "SSH one-use challenge cannot reopen");
        window.handleSSHAuthentication(challenge("two", "password"));
        require(window.sshDialog_->findChildren<QLineEdit *>()[0]->echoMode() == QLineEdit::Password,
            "password remains masked despite untrusted echo");
        window.sshDialog_->reject();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        window.handleSSHAuthentication(challenge("two", "password"));
        require(!window.sshDialog_, "canceled challenge cannot reopen");
        auto bad = challenge("bad");
        bad.insert("echo", QJsonArray{false});
        window.handleSSHAuthentication(bad);
        require(!window.sshDialog_, "malformed SSH challenge rejected");
        bad = challenge("oversized");
        bad.insert("prompts", QJsonArray{QString(257, 'x'), "second"});
        window.handleSSHAuthentication(bad);
        require(!window.sshDialog_, "oversized SSH prompt rejected");
        window.handleSSHAuthentication(challenge("unicode"));
        auto unicodeInputs = window.sshDialog_->findChildren<QLineEdit *>();
        unicodeInputs[0]->setText(QString(600, QChar(0x044f)));
        window.sshDialog_->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        require(!window.sshReplyReady_ && window.sshDialog_, "SSH answer bound counts UTF-8 bytes");
        require(!window.sshDialog_->findChild<QLabel *>("sshAnswerError")->isHidden(), "SSH explains oversized answers safely");
        int heartbeat = 0;
        QTimer::singleShot(0, &window, [&] { ++heartbeat; });
        require(waitFor([&] { return heartbeat == 1; }), "SSH prompt leaves UI responsive");
        window.clearSSHAuthentication();
        auto many = challenge("many");
        QJsonArray eightPrompts, eightEchoes;
        for (int i = 0; i < 8; ++i) { eightPrompts.append("Prompt"); eightEchoes.append(false); }
        many.insert("prompts", eightPrompts);
        many.insert("echo", eightEchoes);
        window.handleSSHAuthentication(many);
        require(window.sshDialog_ && window.sshAuthExpiry_->interval() == 120000, "eight prompts and two-minute expiry");
        require(window.sshDialog_->findChild<QScrollArea *>(), "many prompts use a bounded scroll area");
        for (auto *input : window.sshDialog_->findChildren<QLineEdit *>()) input->setText(QString(1024, QChar(1)));
        window.sshDialog_->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        require(!window.sshReplyReady_, "serialized JSON request bound includes escaping");
        QMetaObject::invokeMethod(window.sshAuthExpiry_, "timeout", Qt::DirectConnection);
        require(!window.sshDialog_ && window.sshAnswers_.isEmpty(), "expiry closes and clears challenge");
        window.handleSSHAuthentication(many);
        require(!window.sshDialog_, "expired challenge cannot reopen");
        many.insert("id", "nine");
        eightPrompts.append("Ninth"); eightEchoes.append(false);
        many.insert("prompts", eightPrompts); many.insert("echo", eightEchoes);
        window.handleSSHAuthentication(many);
        require(!window.sshDialog_, "ninth SSH prompt rejected");
        window.handleSSHAuthentication(challenge("stopped"));
        window.handleStatus({{"State", "stopping"}});
        require(!window.sshDialog_ && window.sshAnswers_.isEmpty(), "stop closes SSH prompt");
        window.state_ = "connecting";
        QLocalServer server;
        require(server.listen("bimeron-ssh-" + QUuid::createUuid().toString()), "SSH private endpoint");
        window.core_.setEndpoint(server.serverName());
        window.helloReady_ = true;
        window.statusNeeded_ = false;
        window.instance_ = "ssh-fixture";
        window.sessionId_ = "ssh-session";
        window.nextStatus_ = window.nextEvents_ = window.nextSSHAuth_ = window.pollClock_.elapsed() + 60000;
        int sent = 0;
        QString expectedCommand = "auth_reply";
        QObject::connect(&server, &QLocalServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [&, socket] {
                if (!socket->canReadLine()) return;
                const auto request = QJsonDocument::fromJson(socket->readLine()).object();
                require(request.value("command") == expectedCommand, "SSH uses existing correlated control transport");
                if (expectedCommand == "auth_reply") {
                    require(request.value("challenge_id") == "wire", "SSH replies to current ID");
                    require(request.value("answers").toArray().size() == 2, "SSH one answer per prompt");
                }
                ++sent;
                socket->write(QJsonDocument(QJsonObject{{"version", 1}, {"id", request.value("id")},
                    {"instance", "ssh-fixture"}, {"status", QJsonObject{{"State", "connecting"},
                        {"Session", QJsonObject{{"id", "ssh-session"}, {"active", true}}}}}}).toJson(QJsonDocument::Compact) + '\n');
            });
        });
        window.handleSSHAuthentication(challenge("wire"));
        auto *wireDialog = window.sshDialog_.data();
        for (auto *input : wireDialog->findChildren<QLineEdit *>()) input->setText("WIRE_SENTINEL");
        wireDialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        window.poll();
        require(waitFor([&] { return sent == 1 && !window.core_.busy(); }), "SSH reply completes asynchronously");
        window.poll();
        require(sent == 1 && window.sshAnswers_.isEmpty(), "SSH reply is not retried or cached");
        require(!window.log_->toPlainText().contains("WIRE_SENTINEL"), "SSH answers absent from logs");
        expectedCommand = "auth_pending";
        window.nextSSHAuth_ = -2;
        window.nextStatus_ = window.nextEvents_ = -1;
        window.poll();
        require(waitFor([&] { return sent == 2 && !window.core_.busy(); }), "overdue status/events cannot starve auth polling");
        window.nextStatus_ = window.nextEvents_ = window.nextSSHAuth_ = window.pollClock_.elapsed() + 60000;
        window.handleSSHAuthentication(challenge("session-change"));
        window.handleStatus({{"State", "connecting"}, {"Session", QJsonObject{{"id", "different-session"}}}});
        require(!window.sshDialog_, "session change closes old prompt");
        window.handleSSHAuthentication(challenge("failure"));
        window.handleReply("auth_reply", {{"instance", "ssh-fixture"}, {"error", "invalid_auth_challenge"},
            {"status", QJsonObject{{"State", "connecting"}, {"Session", QJsonObject{{"id", "different-session"}}}}}});
        require(!window.sshDialog_ && window.statusNeeded_ && !window.sshReplyReady_, "invalid challenge resynchronizes without replay");
        window.handleReply("auth_pending", {{"instance", "ssh-fixture"}, {"error", "unknown_command"},
            {"status", QJsonObject{{"State", "connecting"}, {"Session", QJsonObject{{"id", "ssh-session"}}}}}});
        require(!window.sshAuthSupported_, "old Core disables interactive polling");
        window.handleSSHAuthentication(challenge("three", "key_passphrase"));
        window.invalidateConnection();
        require(!window.sshDialog_ && window.sshAnswers_.isEmpty(), "backend invalidation closes SSH prompt");
    }

    static void serviceProbeSurvivesIpcFailure() {
        QTemporaryDir temporary;
        const QString path = temporary.filePath("held-probe.conf");
        QFile file(path);
        require(file.open(QIODevice::WriteOnly), "held service probe fixture");
        file.write("fixture-delay\n");
        file.close();
        MainWindow window;
        window.core_.cancel();
        for (auto *timer : window.findChildren<QTimer *>()) timer->stop();
        window.invalidateConnection();
        const quint64 generation = window.serviceProbeGeneration_;
        auto *probe = new QProcess(&window);
        window.serviceStatusProcess_ = probe;
        bool completed = false;
        QObject::connect(probe, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), &window,
                         [&](int code, QProcess::ExitStatus status) {
            completed = code == 0 && status == QProcess::NormalExit
                && window.serviceProbeGeneration_ == generation;
            window.serviceStatusProcess_ = nullptr;
            probe->deleteLater();
        });
        probe->start(QCoreApplication::applicationFilePath(), {"validate", path});
        require(probe->waitForStarted(), "asynchronous service probe starts");
        window.connectionFailure("hello", CoreClient::Failure::Transport, "fixture IPC loss");
        require(window.serviceStatusProcess_ == probe && window.serviceProbeGeneration_ == generation,
                "IPC failure preserves independent service inspection");
        require(waitFor([&] { return completed; }), "preserved service observation reaches completion");
        auto *stale = new QProcess(&window);
        window.serviceStatusProcess_ = stale;
        stale->start(QCoreApplication::applicationFilePath(), {"validate", path});
        require(stale->waitForStarted(), "stale probe starts");
        window.invalidateConnection();
        require(!window.serviceStatusProcess_ && window.serviceProbeGeneration_ != generation,
                "backend invalidation cancels service probe and rejects its generation");
        window.core_.cancel();
    }

    static void startupPreparation() {
        MainWindow window;
        window.core_.cancel();
        for (auto *timer : window.findChildren<QTimer *>()) timer->stop();
        int starts = 0;
        const auto reset = [&] {
            window.startupPreparationPending_ = true;
            window.startupPreparationFailed_ = false;
            window.startupStartAttempted_ = false;
            window.startupIpcChecked_ = true;
            window.startupDeadline_ = window.pollClock_.elapsed() + 30000;
            window.autoConnectAttempted_ = false;
            window.helloReady_ = false;
            window.statusNeeded_ = true;
            window.availabilityFailure_ = CoreClient::Failure::Transport;
            window.serviceObservation_ = MainWindow::ServiceObservation::Stopped;
        };
        reset();
        window.connectionFailure("hello", CoreClient::Failure::Transport, "initial unavailable");
        require(!window.autoConnectAttempted_, "initial preparation retains configured auto-connect eligibility");
        window.advanceStartupPreparation([&] { ++starts; });
        window.advanceStartupPreparation([&] { ++starts; });
        require(starts == 1 && window.startupStartAttempted_, "stopped service gets only one startup attempt");
        window.finishStartupPreparation(false, "permission canceled");
        window.advanceStartupPreparation([&] { ++starts; });
        require(starts == 1 && window.autoConnectAttempted_, "cancellation never repeats authorization or auto-connects");
        reset();
        window.helloReady_ = true;
        window.statusNeeded_ = false;
        window.state_ = "connected";
        window.advanceStartupPreparation([&] { ++starts; });
        require(!window.startupPreparationPending_ && !window.startupPreparationFailed_ && starts == 1
            && window.state_ == "connected", "ready VPN survives normal GUI startup");
        for (const auto state : {MainWindow::ServiceObservation::Missing, MainWindow::ServiceObservation::Failed}) {
            reset(); window.serviceObservation_ = state;
            window.advanceStartupPreparation([&] { ++starts; });
            require(!window.startupPreparationPending_ && starts == 1, "missing or unknown service is not started/installed automatically");
        }
        reset(); window.developerMode_ = true;
        window.advanceStartupPreparation([&] { ++starts; });
        require(!window.startupPreparationPending_ && starts == 1, "developer session never auto-starts service");
        window.developerMode_ = false;
        reset(); window.cleanupBlocked_ = true;
        window.advanceStartupPreparation([&] { ++starts; });
        require(!window.startupPreparationPending_ && starts == 1, "cleanup gate prevents startup");
        window.cleanupBlocked_ = false;
        reset(); window.serviceObservation_ = MainWindow::ServiceObservation::Running;
        window.startupDeadline_ = 0;
        window.advanceStartupPreparation([&] { ++starts; });
        require(window.startupPreparationFailed_ && starts == 1, "unresponsive running service times out without restart");
        window.core_.cancel();
    }

    static void coreAvailabilityAssessment() {
        MainWindow window;
        window.developerMode_ = true;
        // Service-state scenarios assume components are available independently
        // of whether this GUI-only test build bundles a Core executable.
        window.repairSnapshotTest_ = [](ConnectionRepair::Snapshot &snapshot) {
            snapshot.helperAvailable = true;
            snapshot.helperTrusted = true;
        };
        window.core_.cancel();
        for (auto *timer : window.findChildren<QTimer *>()) timer->stop();
        window.startupPreparationPending_ = false;
        window.helloReady_ = false;
        window.statusNeeded_ = true;
        window.availabilityFailure_ = CoreClient::Failure::Transport;
        const auto assess = [&](MainWindow::ServiceObservation state, const QString &expected) {
            window.serviceObservation_ = state;
            window.updateCoreAvailability();
            require(window.settingsAvailability_->text().contains(expected), "availability explains the observed service state");
            require(window.connectionLabel_->text() == "Could not prepare connection",
                    "Profiles and Settings share an availability assessment");
            require(!window.settingsRetry_->isHidden() && !window.availabilityDetails_->isHidden(),
                    "unready Core offers retry and details");
        };
        assess(MainWindow::ServiceObservation::Missing, "Install and start");
        require(window.recoveryAction_ == MainWindow::RecoveryAction::Restore, "missing service offers installation");
        window.serviceStartBlock_ = DevelopmentCore::StartBlock::CleanupUnconfirmed;
        for (const auto observation : {MainWindow::ServiceObservation::Stopped, MainWindow::ServiceObservation::Failed}) {
            window.serviceObservation_ = observation;
            window.updateCoreAvailability();
            require(window.recoveryAction_ == MainWindow::RecoveryAction::Restore && window.settingsRetry_->isEnabled(),
                    "abandoned lock requires bounded privileged diagnosis");
        }
        window.serviceStartBlock_ = DevelopmentCore::StartBlock::None;
        for (const auto block : {DevelopmentCore::StartBlock::LockOccupied, DevelopmentCore::StartBlock::LockUnknown,
                                DevelopmentCore::StartBlock::EndpointOccupied, DevelopmentCore::StartBlock::EndpointAccessDenied}) {
            window.serviceStartBlock_ = block;
            window.updateCoreAvailability();
            require(window.recoveryAction_ == MainWindow::RecoveryAction::Restore,
                    "unknown owner or inspection failure never authorizes network recovery");
            require(window.settingsRetry_->text() == "Restore",
                    "unknown observations retain the one-click repair entry point");
        }
        window.serviceStartBlock_ = DevelopmentCore::StartBlock::None;
        assess(MainWindow::ServiceObservation::Stopped, "Start the service");
        assess(MainWindow::ServiceObservation::Running, "Restore verifies ownership");
        window.availabilityFailure_ = CoreClient::Failure::AccessDenied;
        window.updateCoreAvailability();
        require(window.coreManagementTitle_->text().contains("current permissions"),
                "permission failure takes precedence over service state");
        require(window.connectionLabel_->text() == "Could not prepare connection",
                "permission guidance agrees across pages");
        int mutations = 0;
        window.serviceInstallButton_->disconnect(&window);
        window.serviceStartButton_->disconnect(&window);
        QObject::connect(window.serviceInstallButton_, &QPushButton::clicked, &window, [&] { ++mutations; });
        QObject::connect(window.serviceStartButton_, &QPushButton::clicked, &window, [&] { ++mutations; });
        window.core_.setEndpoint("recovery-action-" + QUuid::createUuid().toString());
        for (const auto observation : {MainWindow::ServiceObservation::Missing, MainWindow::ServiceObservation::Stopped}) {
            for (const auto failure : {CoreClient::Failure::AccessDenied, CoreClient::Failure::Incompatible,
                                      CoreClient::Failure::PeerMismatch, CoreClient::Failure::InvalidResponse}) {
                window.availabilityVerificationDeadline_ = 0;
                window.availabilityVerificationFailed_ = failure == CoreClient::Failure::Timeout;
                window.serviceObservation_ = observation;
                window.availabilityFailure_ = failure;
                window.updateCoreAvailability();
                require(window.recoveryAction_ == MainWindow::RecoveryAction::Restore, "error remedy never implies install/start");
                require(window.settingsRetry_->text() == "Restore", "failure retains Restore action");
                window.core_.cancel();
                require(mutations == 0, "Check again never launches a privileged service action");
            }
            window.availabilityVerificationDeadline_ = 0;
            window.availabilityVerificationFailed_ = true;
            window.availabilityFailure_ = CoreClient::Failure::Timeout;
            window.updateCoreAvailability();
            require(window.recoveryAction_ == MainWindow::RecoveryAction::Restore,
                    "timeout does not hide a confirmed install or start remedy");
            require(window.serviceInstallButton_->isEnabled() && window.serviceStartButton_->isEnabled(), "hidden dispatch remains enabled");
            require(window.settingsRetry_->text() == "Restore" && mutations == 0, "primary repair does not dispatch legacy service buttons");
            mutations = 0;
        }
        window.manualStartInspection_ = true;
        window.helloReady_ = true; window.statusNeeded_ = false;
        require(window.controlBlocked(), "manual service inspection blocks profile activation");
        window.manualStartInspection_ = false;
        window.helloReady_ = true;
        window.statusNeeded_ = false;
        window.state_ = "disconnected";
        window.updateCoreAvailability();
        require(window.availabilityDetails_->isHidden() && window.settingsRetry_->isHidden(),
                "ready disconnected Core needs no repair banner");
        require(window.settingsAvailability_->text() == "The service is running and responding.",
                "VPN disconnection is independent of Core readiness");
        window.core_.setEndpoint("bimeron-verification-" + QUuid::createUuid().toString());
        window.beginAvailabilityVerification();
        window.core_.cancel();
        require(!window.helloReady_ && window.statusNeeded_ && window.availabilityVerificationDeadline_ > 0,
                "successful helper operation requires a new hello and status");
        require(window.settingsAvailability_->text().contains("Waiting for service response"),
                "helper completion alone is not reported ready");
        window.helloReady_ = true;
        window.updateCoreAvailability();
        require(window.availabilityVerificationDeadline_ > 0 && !window.availabilityDetails_->isHidden(),
                "hello without authoritative status does not complete verification");
        window.serviceObservation_ = MainWindow::ServiceObservation::Running;
        window.availabilityVerificationDeadline_ = window.pollClock_.elapsed() + 1;
        QThread::msleep(2);
        window.updateCoreAvailability();
        require(window.availabilityVerificationFailed_ && !window.availabilityVerificationDeadline_
                && !window.repairFailure_.isEmpty(),
                "verification deadline produces an actionable terminal failure");
        require(!window.settingsRetry_->isHidden(), "verification failure retains retry");
        window.statusNeeded_ = false;
        window.updateCoreAvailability();
        require(!window.availabilityVerificationFailed_ && window.availabilityDetails_->isHidden()
                && window.settingsAvailability_->text() == "The service is running and responding.",
                "later authoritative readiness clears obsolete verification failure");
    }

    static void repairQuitOwnership() {
        for (bool cleanup : {false, true}) {
            MainWindow window;
            window.core_.cancel();
            for (auto *timer : window.findChildren<QTimer *>()) timer->stop();
            window.repairSession_ = true;
            window.development_->restoreService_ = cleanup;
            window.development_->interruptedRecovery_ = false;
            require(window.development_->needsCleanup() == cleanup, "quit cleanup precondition");
            int launches = 0;
            window.repairCommandTest_ = [&](const QStringList &) { ++launches; };
            const auto phase = window.development_->phase_;
            window.requestShutdown();
            QElapsedTimer elapsed;
            elapsed.start();
            waitFor([&] { return elapsed.elapsed() >= 300; });
            require(window.repairSession_ && !window.quitRequested_ && !window.developmentExit_
                && !window.development_->busy() && window.development_->phase_ == phase
                && window.development_->needsCleanup() == cleanup && launches == 0,
                "quit during pending authorization preserves repair ownership without development transition or elevation");
            window.development_->restoreService_ = false;
        }
    }
    static void run() {
        repairQuitOwnership();
        {
            MainWindow window;
            window.core_.cancel();
            window.core_.setEndpoint("repair-terminal-probe-" + QUuid::createUuid().toString());
            for (auto *timer : window.findChildren<QTimer *>()) timer->stop();
            window.startupPreparationPending_ = false;
            window.serviceObservation_ = MainWindow::ServiceObservation::Stopped;
            window.probeTestMode_ = "valid";
            window.repairSession_ = true;
            window.repairSessionDone_ = true;
            window.repairSessionOK_ = true;
            window.finishServiceCommand(0);
            require(window.serviceStatusProcess_, "terminal repair starts fresh service inspection");
            require(!window.helloReady_ && window.availabilityVerificationDeadline_ > 0,
                "terminal repair requires independent IPC readiness");
            window.core_.cancel(); // This fixture exercises service observation, not IPC.
            require(waitFor([&] { return !window.serviceStatusProcess_; }), "terminal repair service probe completes");
            require(window.serviceObservation_ == MainWindow::ServiceObservation::Running,
                "terminal repair replaces stale stopped observation");
            require(window.serviceStatusLabel_->text() == "Running", "running service header is capitalized");
            require(!window.helloReady_,
                "observed running service still requires independent IPC readiness");
            window.core_.cancel();
            require(window.log_->toPlainText().contains("Repair steps completed; verifying service readiness.")
                && !window.log_->toPlainText().contains("Service repair completed: ready to connect."),
                "helper completion is logged without claiming readiness");
            window.helloReady_ = true;
            window.statusNeeded_ = false;
            window.updateCoreAvailability();
            require(window.log_->toPlainText().contains("Service repair completed: ready to connect."),
                "verified repair success reaches Journal");
            const QString previousLog = window.log_->toPlainText();
            window.updateCoreAvailability();
            require(window.log_->toPlainText() == previousLog, "identical service observations do not duplicate Journal entries");
            window.serviceObservation_ = MainWindow::ServiceObservation::Stopped;
            window.lastCoreError_ = "password=visible-diagnostic";
            window.updateCoreAvailability();
            require(window.log_->toPlainText().contains("Service status: Stopped"), "service changes reach Journal");
            require(window.coreManagementDetails_->text().contains("password=visible-diagnostic"),
                "credential-bearing diagnostic details remain visible");
            require(window.repairReport().value("coreDetails").toString() == "password=visible-diagnostic",
                "repair report retains displayed diagnostics");
        }
        {
            MainWindow window;
            window.core_.cancel();
            window.core_.setEndpoint("single-repair-" + QUuid::createUuid().toString());
            for (auto *timer : window.findChildren<QTimer *>()) timer->stop();
            window.startupPreparationPending_ = false;
            window.helloReady_ = false;
            window.statusNeeded_ = true;
            int launches = 0;
            window.repairCommandTest_ = [&](const QStringList &command) {
                require(command.size() == 3 && command[0] == "repair-session"
                    && command[1].size() == 32
                    && command[2] == QString::number(QCoreApplication::applicationPid()),
                    "one bounded repair request binds nonce and caller PID");
                ++launches;
            };
            for (auto state : {MainWindow::ServiceObservation::Missing, MainWindow::ServiceObservation::Stopped,
                               MainWindow::ServiceObservation::Unknown}) {
                window.availabilityVerificationDeadline_ = 0;
                window.serviceObservation_ = state;
                window.startRepairSession();
                const int before = launches;
                window.startRepairSession();
                require(launches == before && window.repairSession_, "repeated Restore never elevates twice");
                window.coreManagement_->hide();
                window.openCoreManagement();
                require(window.repairSession_ && !window.settingsRetry_->isEnabled(), "close and reopen retains session");
                window.coreManagement_->hide();
                window.repairSessionDone_ = true;
                window.repairSessionOK_ = true;
                window.finishServiceCommand(0);
                require(launches == before && window.availabilityVerificationDeadline_ > 0
                    && !window.helloReady_, "session completion verifies readiness without second elevation");
                window.core_.cancel();
            }
            window.availabilityVerificationDeadline_ = 0;
            window.startRepairSession();
            const int before = launches;
            window.repairSessionDone_ = true;
            window.repairSessionOK_ = false;
            window.finishServiceCommand(1);
            require(launches == before && !window.repairSession_ && !window.repairFailure_.isEmpty(),
                "failed repair or rollback never relaunches elevation");
            require(window.log_->toPlainText().contains("Service repair failed:"), "terminal repair failure reaches Journal");
            window.startRepairSession();
            const int consentLaunches = launches;
            QLocalServer consentServer;
            const QString consentEndpoint = "repair-consent-" + QUuid::createUuid().toString(QUuid::Id128);
            require(consentServer.listen(consentEndpoint), "private consent endpoint");
            window.repairSocket_ = new QLocalSocket(&window);
            window.repairSocket_->connectToServer(consentEndpoint);
            require(waitFor([&] { return consentServer.hasPendingConnections()
                && window.repairSocket_->state() == QLocalSocket::ConnectedState; }), "consent fixture connects");
            auto *consentPeer = consentServer.nextPendingConnection();
            // Authentication is explicitly injected only at this private fixture boundary.
            window.repairSocket_->setProperty("authenticated", true);
            consentPeer->write("{\"stage\":\"replace\"}\n");
            consentPeer->flush();
            require(waitFor([&] { return window.repairSocket_->bytesAvailable() > 0; }), "repair stage arrives");
            window.readRepairSession();
            require(window.log_->toPlainText().contains("Service repair: Replacing service component")
                && !window.repairSessionDone_, "replacement stage is accepted and logged");
            for (const QString kind : {QString("disruption"), QString("network-recovery")}) {
                consentPeer->write(QJsonDocument(QJsonObject{{"consent", kind}}).toJson(QJsonDocument::Compact) + '\n');
                consentPeer->flush();
                require(waitFor([&] { return window.repairSocket_->bytesAvailable() > 0; }), "consent event arrives");
                window.readRepairSession();
                require(window.repairConsentDialog_ && window.repairSession_ && launches == consentLaunches,
                    "fresh consent stays inside the original elevated session");
                require(!window.requestRepairConsent(kind), "duplicate or overlapping challenge rejected");
                const bool approve = kind == "disruption";
                window.repairConsentDialog_->button(approve ? QMessageBox::Yes : QMessageBox::No)->click();
                require(waitFor([&] { return consentPeer->canReadLine(); }), "consent reply arrives");
                const auto reply = QJsonDocument::fromJson(consentPeer->readLine()).object();
                require(reply.size() == 2 && reply.value("consent") == kind
                    && reply.value("approved") == approve && launches == consentLaunches,
                    "exact consent response preserves approval or denial without elevation");
            }
            window.repairSessionDone_ = true;
            window.repairSessionOK_ = false;
            window.finishServiceCommand(1);
            window.coreManagement_->hide();
            require(launches == consentLaunches && !window.repairSession_, "denial terminates without relaunch");
#ifdef Q_OS_WIN
            window.startRepairSession();
            QLocalServer impostor;
            require(impostor.listen("Bimeron.Repair." + window.repairNonce_), "private impostor endpoint");
            window.connectRepairSession();
            require(waitFor([&] { return window.repairFailure_.contains("identity could not be verified"); }),
                "GUI rejects a server unrelated to the elevated process");
            QCoreApplication::processEvents();
            auto *peer = impostor.nextPendingConnection();
            // Authentication can abort before Qt delivers the server's accept.
            require(!peer || peer->bytesAvailable() == 0, "unauthenticated server receives no repair request");
            window.finishServiceCommand(1);
#endif
        }
        {
            MainWindow window;
            window.core_.cancel();
            for (auto *timer : window.findChildren<QTimer *>()) timer->stop();
            window.startupPreparationPending_ = false;
            window.invalidateConnection();
            for (const QString mode : {QString("failed"), QString("malformed"), QString("valid")}) {
                window.probeTestMode_ = mode;
                window.refreshServiceStatus();
                require(waitFor([&] { return !window.serviceStatusProcess_; }), "fixture probe completes");
                const auto report = window.repairReport();
                const QString category = mode == "failed" ? "command-failure" : mode == "malformed" ? "malformed-response" : "observed";
                require(report.value("probeCategory").toString() == category, "actual probe outcome exported");
                const QString description = mode == "failed" ? "Service check command failed"
                    : mode == "malformed" ? "Invalid service check response" : "Service state received";
                require(window.coreManagementDetails_->text().contains(description), "readable probe outcome rendered");
                if (mode == "failed") require(QJsonDocument(report).toJson().contains("secret-probe-fixture")
                    && window.coreManagementDetails_->text().contains("secret-probe-fixture")
                    && window.log_->toPlainText().contains("secret-probe-fixture"), "actual probe diagnostics retained in details, report and Journal");
                if (mode == "valid") require(report.value("failedProbe").toObject().value("category").toString() == "malformed-response", "failed probe retained after successful refresh");
            }
            for (const QString mode : {QString("starting"), QString("stopping")}) {
                window.probeTestMode_ = mode;
                window.refreshServiceStatus();
                require(waitFor([&] { return !window.serviceStatusProcess_; }), "transitional service probe completes");
                require(window.log_->toPlainText().contains("Service status: " + mode),
                    "different pending service states each reach Journal");
            }
            for (const QString mode : {QString("missing"), QString("malformed"), QString("stopped"), QString("valid"), QString("missing")}) {
                window.probeTestMode_ = mode;
                window.refreshServiceStatus();
                require(waitFor([&] { return !window.serviceStatusProcess_; }), "advanced probe completes");
                window.updateCoreAvailability();
                require(window.serviceStopButton_->isEnabled() && window.serviceUpdateButton_->isEnabled() && window.serviceUninstallButton_->isEnabled(), "idle advanced controls enabled");
                window.repairCommandPending_ = true;
                window.updateCoreAvailability();
                require(!window.serviceStopButton_->isEnabled() && !window.serviceUpdateButton_->isEnabled() && !window.serviceUninstallButton_->isEnabled(), "pending command disables advanced controls");
                window.repairCommandPending_ = false;
                window.updateCoreAvailability();
                require(window.serviceStopButton_->isHidden() == (mode != "valid"), "stop follows observed state");
                require(window.serviceUpdateButton_->isHidden() == (mode != "valid" && mode != "stopped"), "update follows observed state");
                require(window.serviceUninstallButton_->isHidden() == (mode != "stopped"), "uninstall follows observed state");
                require(window.serviceInstallButton_->isHidden() && window.serviceStartButton_->isHidden(), "primary dispatch controls remain hidden");
                require(window.coreManagement_->findChildren<QDialog *>().isEmpty(), "no nested dialog after state change");
            }
            window.core_.cancel();
        }
        {
            MainWindow window;
            window.core_.cancel();
            window.core_.setEndpoint("repair-workflow-" + QUuid::createUuid().toString());
            for (auto *timer : window.findChildren<QTimer *>()) timer->stop();
            window.startupPreparationPending_ = false;
            window.helloReady_ = false;
            window.statusNeeded_ = true;
            QStringList commands;
            window.repairCommandTest_ = [&](const QStringList &command) { commands.append(command.join(' ')); };
            window.repairInspectionTest_ = [](auto done) { done({DevelopmentCore::StartBlock::None, {}}); };
            window.startServiceAfterInstall_ = true;
            require(window.runServiceCommand({"service", "install"}, "Installing service"), "fake install begins");
            require(!window.runServiceCommand({"service", "start"}, "Starting service"), "duplicate command blocked");
            window.repairWatchdogMs_ = 0;
            window.updateCoreAvailability();
            require(window.repairWatchdogExpired_ && window.repairCommandPending_, "never-exiting helper stays tracked after watchdog");
            window.coreManagement_->hide();
            window.openCoreManagement();
            window.updateCoreAvailability();
            require(window.coreManagementTitle_->text().contains("still running") && !window.settingsRetry_->isEnabled(), "reopened helper retains watchdog and exclusion");
            require(!window.runServiceCommand({"service", "start"}, "Starting service"), "watchdog never permits overlapping mutation");
            window.coreManagement_->hide();
            window.repairWatchdogMs_ = 120000;
            window.probeCategory_ = "malformed-response";
            window.probeExitCode_ = 7;
            window.probeDurationMs_ = 23;
            window.probeObservedAt_ = 123456;
            window.updateCoreAvailability();
            require(window.coreManagementDetails_->text().contains("Invalid service check response"), "readable failed probe category rendered");
            const auto report = window.repairReport();
            require(report.value("probeCategory").toString() == "malformed-response" && report.value("probeExitCode").toInt() == 7
                && report.value("probeObservedAt").toDouble() == 123456, "report preserves structured probe evidence");

            window.finishServiceCommand(0);
            require(commands == QStringList{"service install", "service start"}, "install performs guarded start");
            window.finishServiceCommand(0);
            require(window.availabilityVerificationDeadline_ > 0 && !window.helloReady_ && window.autoConnectAttempted_,
                    "start success verifies readiness without profile auto-connect");
            window.core_.cancel();
            window.availabilityVerificationDeadline_ = 0;
            commands.clear();
            window.startAfterRecovery_ = true;
            require(window.runServiceCommand({"recover"}, "Restoring network"), "authorized fake recovery begins");
            window.finishServiceCommand(0);
            require(commands == QStringList{"recover", "service start"}, "recovery performs fresh guarded start");
            window.finishServiceCommand(5);
            require(!window.repairFailure_.isEmpty() && !window.repairHistory_.isEmpty(), "failed start keeps step and history");
            const QString failure = window.repairFailure_;
            window.updateCoreAvailability();
            require(window.repairFailure_ == failure, "diagnostic refresh preserves failure");
            commands.clear();
            window.startServiceAfterInstall_ = true;
            window.runServiceCommand({"service", "install"}, "Installing service");
            window.finishServiceCommand(5);
            require(commands == QStringList{"service install"}, "failed install never starts service");
            commands.clear();
            window.runServiceCommand({"service", "stop"}, "Stopping service");
            window.finishServiceCommand(5);
            window.requestServiceStart();
            require(commands == QStringList{"service stop"}, "failed stop blocks replacement start");
            window.core_.cancel();
        }
        {
            using namespace ConnectionRepair;
            Snapshot facts;
            require(diagnose(facts).action == Action::Restore, "missing helper requires application repair");
            facts.helperAvailable = true;
            facts.helperTrusted = false;
            require(diagnose(facts).cause == Cause::ComponentsUntrusted && diagnose(facts).action == Action::Restore, "present untrusted helper is not missing and does not demand reinstall");
            facts.helperTrusted = true;
            for (const auto state : {Service::Unknown, Service::Failed, Service::Paused, Service::Running, Service::Transitional}) {
                facts.service = state;
                require(diagnose(facts).action == Action::Restore, "uncertain state retains bounded repair entry point");
            }
            facts.service = Service::Missing;
            require(diagnose(facts).action == Action::Restore, "confirmed missing service selects install workflow");
            facts.service = Service::Stopped;
            require(diagnose(facts).action == Action::Restore, "confirmed stopped service selects start workflow");
            facts.conflict = DevelopmentCore::StartBlock::LockOccupied;
            require(diagnose(facts).action == Action::Restore, "unknown lock never selects recover");
            facts.conflict = DevelopmentCore::StartBlock::None;
            facts.ipcFailure = CoreClient::Failure::Incompatible;
            require(diagnose(facts).action == Action::Restore, "incompatible IPC selects bounded repair");
            facts.ipcFailure = CoreClient::Failure::AccessDenied;
            require(diagnose(facts).action == Action::Restore, "access failure cannot authorize service mutation");
            facts.ipcFailure = CoreClient::Failure::Transport;
            facts.interrupted = true;
            require(diagnose(facts).action == Action::Restore, "interrupted journal selects reconciliation");
            facts.interrupted = false; facts.cleanup = true;
            require(diagnose(facts).action == Action::Restore, "confirmed cleanup failure retains repair with explicit consent");
            facts.cleanup = false; facts.verifying = true;
            require(diagnose(facts).phase == Phase::Verifying, "verification is distinct from helper completion");
            facts.verifying = false; facts.ready = true;
            require(diagnose(facts).cause == Cause::Ready, "authoritative readiness succeeds");
        }
        startupPreparation();
        coreAvailabilityAssessment();
        serviceProbeSurvivesIpcFailure();
        {
            MainWindow window;
            NativeIKEv2SessionTest::Fixture fixture;
            fixture.discover();
            window.bindNativeController(&fixture.controller);
            for (const auto &state : {QString("connecting"), QString("connected"), QString("stopping"), QString("failed")}) {
                fixture.observe(fixture.sessionReply(state));
                const QString expected = state == "connecting" ? MainWindow::tr("Connecting…")
                    : state == "connected" ? MainWindow::tr("Connected")
                    : state == "stopping" ? MainWindow::tr("Disconnecting…") : MainWindow::tr("Native connection failed.");
                require(window.connectionLabel_->text().startsWith(expected), "controller outcome reaches GUI");
                const auto before = fixture.calls.size();
                window.connectButton_->click();
                window.toggleConnection();
                require(!window.connectButton_->isEnabled() && !window.core_.busy()
                    && fixture.calls.size() == before && fixture.launches == 0,
                    "native outcome prevents repeated GUI Connect and packet dispatch");
            }
            const auto calls = fixture.calls.size();
            NativeIKEv2Session::Snapshot snapshot;
            snapshot.authoritative = true;
            snapshot.state = "failed";
            snapshot.safeError = "775"; // Private hypothetical fixture, not a transport contract.
            JsonTranslator translator;
            require(translator.load(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../assets/translations/ru.json")), "775 Russian catalog");
            for (bool russian : {false, true}) {
                if (russian) QCoreApplication::installTranslator(&translator);
                window.showNativeSnapshot(snapshot);
                const QString expected = russian
                    ? QString::fromUtf8("Соединение завершилось с ошибкой Windows 775. Локальная причина неизвестна.")
                    : QString("Connection failed with Windows error 775. The local cause is unknown.");
                const QString text = window.nativeStatusLabel_->text();
                require(text.contains(expected), "775 explicitly localized with unknown local cause");
                for (const auto &unsupported : {QString("EAP"), QString("password"), QString("server rejection"),
                        QString::fromUtf8("парол"), QString::fromUtf8("отказ сервер")})
                    require(!text.contains(unsupported, Qt::CaseInsensitive), "775 makes no authentication diagnosis");
                if (russian) QCoreApplication::removeTranslator(&translator);
            }
            snapshot.authoritative = false;
            window.showNativeSnapshot(snapshot);
            require(!window.nativeStatusLabel_->text().contains("775")
                && window.connectionLabel_->text().startsWith("Connection state is unknown."),
                "stale 775 never becomes an authoritative outcome");
            snapshot.authoritative = true;
            snapshot.safeError = "raw diagnostic EAP password server rejection";
            window.showNativeSnapshot(snapshot);
            require(!window.nativeStatusLabel_->text().contains(snapshot.safeError)
                && window.nativeStatusLabel_->text().contains("The native operation could not be completed."),
                "unknown diagnostic uses generic fallback");
            require(fixture.calls.size() == calls && fixture.launches == 0 && window.nativeOwnerBlocked_
                && !window.connectButton_->isEnabled(), "rendering never dispatches or readmits");
        }
        for (int scenario = 0; scenario < 3; ++scenario) {
            MainWindow reviewWindow;
            require(reviewWindow.nativeUnavailableReason().contains("Core/Desktop"), "concrete approval reason");
            NativeIKEv2SessionTest::Fixture provision;
            reviewWindow.bindNativeController(&provision.controller);
            reviewWindow.nativeActions_.select("fixture", "vpn.example", "remove");
            provision.controller.setLaunchGates({true,true,true,true});
            require(reviewWindow.nativeActions_.preview(), "modal fixture preview");
            NativeIKEv2SessionTest::startProvision(provision);
            QTimer::singleShot(0, [&] {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                require(dialog, "actual consent modal");
                const auto plan = NativeIKEv2SessionTest::publicPlan();
                for (const auto *key : {"authentication", "ipsec", "credentials", "trust"}) {
                    bool found = false;
                    for (auto *label : dialog->findChildren<QLabel *>())
                        if (label->text() == plan.value(key).toString()) {
                            require(label->textFormat() == Qt::PlainText, "plan plain text"); found = true;
                        }
                    require(found, "public plan field displayed");
                }
                if (scenario == 0) { dialog->reject(); return; }
                if (scenario == 1) reviewWindow.nativeActions_.select("other", "vpn.example", "remove");
                else {
                    auto reply = provision.reply(); auto changed = plan; changed.insert("trust", "changed");
                    reply.insert("operation", "previewed"); reply.insert("profile", changed); provision.observe(reply);
                }
                dialog->accept();
            });
            reviewWindow.confirmNativePreview();
            for (const auto &call : provision.calls)
                require(call.command != "remove" && call.command != "apply", "cancel or changed modal cannot mutate");
        }
        {
            MainWindow reviewWindow;
            NativeIKEv2SessionTest::Fixture provision;
            reviewWindow.bindNativeController(&provision.controller);
            reviewWindow.nativeActions_.select("fixture", "vpn.example", "remove");
            provision.controller.setLaunchGates({true,true,true,true});
            require(reviewWindow.nativeActions_.preview(), "UI adapter starts fixture preview");
            NativeIKEv2SessionTest::startProvision(provision);
            QTimer::singleShot(0, [&] {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                require(dialog, "adapter opens actual confirmation");
                auto *buttons = dialog->findChild<QDialogButtonBox *>();
                for (auto *button : buttons->buttons())
                    if (buttons->buttonRole(button) == QDialogButtonBox::AcceptRole) button->click();
            });
            reviewWindow.confirmNativePreview();
            require(provision.calls.back().command == "remove", "Qt consent reaches adapter once");
        }

        {
            QTemporaryDir directory;
            require(directory.isValid(), "native creation fixture directory");
            MainWindow window;
            window.store_ = ProfileStore(directory.filePath("profiles"));
            window.profiles_.clear();
            window.validationExecutable_ = QCoreApplication::applicationFilePath();
            window.developerMode_ = true;
            QTimer::singleShot(0, [&] {
                auto *add = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                require(add, "native creation Add dialog");
                auto *action = add->findChild<QPushButton *>("createNativeProfile");
                require(action && !action->autoDefault(), "distinct native creation action");
                QTimer::singleShot(0, [&] {
                    auto *editor = qobject_cast<ProfileEditorDialog *>(QApplication::activeModalWidget());
                    require(editor, "creation opens existing editor");
                    require(editor->findChild<QTabWidget *>()->isTabEnabled(0), "empty native draft opens visual form");
                    require(editor->findChild<QLineEdit *>("windowsProfile")->text().isEmpty()
                        && editor->findChild<QLineEdit *>("nativeServerName")->text().isEmpty(), "no lab defaults");
                    editor->reject();
                });
                action->click();
            });
            window.addProfile();
            require(window.profiles_.isEmpty() && QDir(directory.path()).entryList(QDir::Files).isEmpty(), "cancel creates no profile");
            for (int scenario = 0; scenario < 5; ++scenario) {
                const bool available = scenario != 0;
                const bool successful = scenario >= 3;
                const int before = window.profiles_.size();
                if (scenario == 2) require(QDir().mkdir(directory.filePath("profiles.json")), "index write failure fixture");
                window.validationExecutable_ = available ? QCoreApplication::applicationFilePath() : directory.filePath("missing-core");
                bool submitted = false;
                bool submitPending = false;
                bool errorShown = false;
                bool cancelling = false;
                QTimer timer;
                QObject::connect(&timer, &QTimer::timeout, [&] {
                    if (auto *message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                        require(!successful, "successful native creation has no error");
                        errorShown = true;
                        bool discarded = false;
                        for (auto *button : message->buttons()) {
                            if (message->buttonRole(button) == QMessageBox::DestructiveRole) {
                                button->click(); discarded = true; break;
                            }
                        }
                        if (!discarded) message->accept();
                    } else if (auto *editor = qobject_cast<ProfileEditorDialog *>(QApplication::activeModalWidget())) {
                        // Ticks that piled up while the editor opened may run before the
                        // queued Save click; wait until that click has actually happened.
                        if (cancelling || submitPending) return;
                        if (submitted) {
                            require(errorShown && editor->findChild<QLineEdit *>("windowsProfile")->text() == "Fixture", "failed save preserves draft");
                            cancelling = true;
                            QTimer::singleShot(0, editor, [editor] { editor->reject(); });
                        } else {
                            editor->findChild<QLineEdit *>("profileName")->setText(scenario == 1 ? "fixture-reject" : "Native fixture");
                            editor->findChild<QLineEdit *>("windowsProfile")->setText("Fixture");
                            editor->findChild<QLineEdit *>("nativeServerName")->setText("vpn.invalid");
                            submitted = true;
                            submitPending = true;
                            auto *save = editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save);
                            QTimer::singleShot(0, save, [&submitPending, save] {
                                submitPending = false;
                                save->click();
                            });
                        }
                    }
                });
                timer.start(20);
                window.createNativeProfile();
                timer.stop();
                require(window.profiles_.size() == before + (successful ? 1 : 0), "only validated profile registered");
                require(QDir(directory.filePath("profiles")).entryList({"*.conf"}, QDir::Files).size() == window.profiles_.size(), "failed save leaves no orphan profile");
                if (scenario == 2) require(QDir().rmdir(directory.filePath("profiles.json")), "remove index failure fixture");
            }
            require(window.profiles_[0].path != window.profiles_[1].path, "duplicate display names cannot overwrite another profile");
            QFile saved(window.profiles_.first().path);
            require(saved.open(QIODevice::ReadOnly), "created native profile persisted");
            const QByteArray bytes = saved.readAll();
            saved.close();
            require(bytes.contains("WindowsProfile = Fixture") && bytes.contains("ServerName = vpn.invalid"), "native creation round trip");
            QTimer::singleShot(0, [&] {
                auto *editor = qobject_cast<ProfileEditorDialog *>(QApplication::activeModalWidget());
                require(editor && editor->findChild<QLineEdit *>("windowsProfile")->text() == "Fixture", "reopen native profile");
                editor->findChild<QLineEdit *>("profileName")->setText("Renamed native fixture");
                editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
            });
            window.editProfile(0);
            require(window.profiles_[0].name == "Renamed native fixture", "native local rename");
            const QString removedPath = window.profiles_[0].path;
            for (const bool confirmed : {false, true}) {
                window.profileList_->setCurrentRow(0, QItemSelectionModel::ClearAndSelect);
                QTimer::singleShot(0, [&] {
                    auto *message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                    require(message && message->text().contains("Windows VPN profile"), "local deletion scope is explicit");
                    for (auto *button : message->buttons()) {
                        if (message->buttonRole(button) == (confirmed ? QMessageBox::DestructiveRole : QMessageBox::RejectRole)) {
                            button->click(); break;
                        }
                    }
                });
                window.removeProfile();
                require(QFileInfo::exists(removedPath) != confirmed, "local deletion honors confirmation");
            }
            require(window.profiles_.size() == 1 && QFileInfo::exists(window.profiles_[0].path), "other same-name profile retained");
        }
        {
            for (const bool reviewable : {false, true}) {
                for (const QString action : {"create", "remove", "unchanged", "invalid"}) {
                    auto *dialog = ProfileEditorDialog::nativeConfirmation(nullptr, "Fixture", "vpn.invalid", action, reviewable);
                    auto *buttons = dialog->findChild<QDialogButtonBox *>();
                    QPushButton *accept = nullptr;
                    for (auto *button : buttons->buttons())
                        if (buttons->buttonRole(button) == QDialogButtonBox::AcceptRole) accept = qobject_cast<QPushButton *>(button);
                    require(bool(accept) == (reviewable && (action == "create" || action == "remove")), "confirmation requires reviewed mutation");
                    if (accept) { accept->click(); require(dialog->result() == QDialog::Accepted && !accept->isEnabled(), "one local confirmation"); }
                    else { dialog->reject(); require(dialog->result() == QDialog::Rejected, "no mutation confirmation"); }
                    delete dialog;
                }
            }
        }
        {
            MainWindow window;
            QTimer::singleShot(0, [&] {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                require(dialog, "Add dialog opened");
                const QList<QLineEdit *> fields{dialog->findChild<QLineEdit *>("profileUri"), dialog->findChild<QLineEdit *>("profileName")};
                require(fields.size() == 2, "URI and name fields");
                QPushButton *paste = nullptr;
                for (auto *button : dialog->findChildren<QPushButton *>()) {
                    if (button->text().startsWith("Paste")) paste = button;
                }
                require(paste, "Paste control");
                require(fields[1]->isHidden() && fields[0]->echoMode() == QLineEdit::Normal, "Name initially hidden and URI visible");
                // The native clipboard is shared with every process in the session. Snapshot the
                // user's content and restore it before any failure, because require() aborts.
                QMimeData *previous = new QMimeData;
                if (const QMimeData *current = QApplication::clipboard()->mimeData()) {
                    if (current->hasImage()) previous->setImageData(current->imageData());
                    for (const QString &format : current->formats())
                        if (format != "application/x-qt-image") previous->setData(format, current->data(format));
                }
                const auto restoreClipboard = [&] {
                    if (previous) QApplication::clipboard()->setMimeData(previous);
                    previous = nullptr;
                };
                const auto check = [&](bool condition, const char *message) {
                    if (!condition) restoreClipboard();
                    require(condition, message);
                };
                // Another process can hold the clipboard open, so the OS may reject a write.
                // Retry within a deadline and report that apart from the dialog's reaction,
                // which arrives asynchronously through QClipboard::dataChanged.
                const auto setClipboard = [&](const QString &text) {
                    QElapsedTimer timer;
                    timer.start();
                    qint64 lastWrite = -1000;
                    const bool written = waitFor([&] {
                        if (QApplication::clipboard()->text() == text) return true;
                        if (timer.elapsed() - lastWrite >= 250) {
                            QApplication::clipboard()->setText(text);
                            lastWrite = timer.elapsed();
                        }
                        return false;
                    }, 5000);
                    check(written, "Fixture could not write the system clipboard; another process may hold it open");
                };
                setClipboard("unsupported clipboard");
                check(waitFor([&] { return !paste->isVisible() && !paste->isEnabled(); }, 5000), "Unsupported clipboard hides Paste");
                const QSize original = dialog->size();
                fields[0]->setText("typed text");
                QCoreApplication::processEvents();
                check(dialog->size() == original, "Typing retains geometry");
                setClipboard("https://example.invalid:443");
                check(waitFor([&] { return paste->isEnabled() && paste->isVisible(); }, 5000), "Supported clipboard shows Paste");
                paste->click();
                QCoreApplication::processEvents();
                check(fields[0]->text() == "https://example.invalid:443", "Paste fills the URI field");
                check(dialog->width() == original.width() && dialog->height() > original.height(), "Valid URI reveals name without width change");
                check(fields[1]->isVisible() && fields[1]->y() < fields[0]->y(), "Name appears above URI");
                fields[0]->setText("ssh://alice@example.invalid");
                QCoreApplication::processEvents();
                check(fields[1]->isVisible() && dialog->width() == original.width(), "SSH retains name and dialog width");
                check(dialog->findChildren<QLineEdit *>().size() == 4, "SSH identity and known hosts retained");
                check(dialog->findChild<QPushButton *>("browseProfileFile"), "File browse shares the add form");
                check(dialog->findChild<QPushButton *>("createProfileInEditor"), "Create editor action is available");
                check(!dialog->findChild<QPushButton *>("createNativeProfile"), "IKEv2 creation is limited to developer mode");
                fields[0]->setText(QDir::homePath() + "/profile.conf");
                check(fields[1]->isHidden(), "Typed absolute path hides profile name");
                check(dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->isEnabled(), "File import does not require a name");
                fields[0]->setText("relative.conf");
                check(!dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->isEnabled(), "Relative path is not accepted");
                for (const QString &path : {QUrl::fromLocalFile(QDir::homePath() + "/profile.conf").toString(), QString("~/profile.conf"), "\"" + QDir::homePath() + "/profile.conf\""}) {
                    fields[0]->setText(path);
                    check(fields[1]->isHidden() && dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->isEnabled(), "File URL, home path and quoted path accepted without name");
                }
                const QString retainedInput = fields[0]->text();
                const QString capture = qEnvironmentVariable("BIMERON_CREATION_CAPTURE");
                if (!capture.isEmpty()) {
                    QCoreApplication::processEvents();
                    check(dialog->grab().save(capture + ".add.png"), "capture actual add dialog");
                }
                QTimer::singleShot(0, [&] {
                    auto *editor = qobject_cast<ProfileEditorDialog *>(QApplication::activeModalWidget());
                    check(editor, "Create action opens profile editor");
                    auto *sources = editor->findChild<QComboBox *>("keySource");
                    check(sources && sources->isEnabled() && sources->findData("SS") < 0, "New editor has no URI source choices");
                    check(editor->findChild<QLineEdit *>("windowsProfile")->isHidden()
                        && editor->findChild<QLineEdit *>("nativeServerName")->isHidden()
                        && editor->findChild<QWidget *>("nativeProfileActions")->isHidden(), "New editor hides IKEv2 rows");
                    if (!capture.isEmpty()) {
                        QCoreApplication::processEvents();
                        check(editor->grab().save(capture + ".editor.png"), "capture actual creation editor");
                    }
                    editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel)->click();
                });
                dialog->findChild<QPushButton *>("createProfileInEditor")->click();
                check(fields[0]->text() == retainedInput, "Cancel editor retains add-dialog input");
                restoreClipboard();
                dialog->reject();
            });
            window.addProfile();
        }

        transport();
        sshAuthentication();
        for (const QString policy : {QStringLiteral("keep"), QStringLiteral("stop")}) {
            QProcess process;
            process.start(QCoreApplication::applicationFilePath(), {"--visual-check", "--connection-quit", policy});
            require(process.waitForFinished(5000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0,
                    "isolated exit-policy fixture");
        }
        QTemporaryDir directory;
        require(directory.isValid(), "connection fixture directory");
        QLocalServer server;
        require(server.listen("bimeron-connection-" + QUuid::createUuid().toString()), "connection fixture endpoint");
        MainWindow window;
        window.core_.setEndpoint(server.serverName());
        window.validationExecutable_ = QCoreApplication::applicationFilePath();
        const QByteArray config("[Profile]\nName = Test\n[AccessKey]\nSOCKS5 = socks5://localhost:1080\n[Interface]\nMode = proxy\n");
        const auto write = [](const QString &path, const QByteArray &bytes) {
            QFile file(path);
            require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "write connection fixture");
            require(file.write(bytes) == bytes.size(), "complete connection fixture");
        };
        for (const QString name : {QStringLiteral("A"), QStringLiteral("B")}) {
            const QString path = directory.filePath(name + ".conf");
            write(path, config);
            window.profiles_.append({ProfileStore::identity(path), name, path});
            window.profileList_->addItem(name);
        }
        window.profileList_->setCurrentRow(0);
        QSettings().setValue("app/autoConnect", true);
        QSettings().setValue("app/autoConnectProfile", window.profiles_[1].id);
        QString instance = "first";
        QString version = "0.2.0";
        QJsonObject status{{"State", "connected"}, {"Session", QJsonObject{
            {"id", "existing"}, {"active", true}, {"profile_id", window.profiles_[0].id}, {"mode", "proxy"}}}};
        int starts = 0, stops = 0, requests = 0;
        qint64 bytes = 100;
        bool holdStart = false, holdStop = false, dropped = false, invalidCursor = false, failStop = false, failStart = false;
        QPointer<QLocalSocket> stopSocket;
        QJsonObject stopRequest;
        const auto respond = [&](QLocalSocket *socket, const QJsonObject &request, QJsonObject extra = {}) {
            QJsonObject reply{{"version", 1}, {"id", request.value("id")}, {"instance", instance},
                              {"status", status}, {"core", QJsonObject{{"version", version}}},
                              {"capabilities", QJsonObject{{"vpn", true}, {"proxy", true}, {"ipv6", true}}}};
            for (auto it = extra.begin(); it != extra.end(); ++it) reply.insert(it.key(), it.value());
            socket->write(QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n');
            socket->flush();
        };
        QObject::connect(&server, &QLocalServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [&, socket] {
                if (!socket->canReadLine()) return;
                const auto request = QJsonDocument::fromJson(socket->readLine()).object();
                ++requests;
                const QString command = request.value("command").toString();
                QJsonObject extra;
                if (command == "start") {
                    ++starts;
                    if (failStart) {
                        status = {{"State", "disconnected"}};
                        respond(socket, request, {{"error", "invalid_configuration"}, {"detail", "OUTER_SENTINEL"}});
                        return;
                    }
                    status = {{"State", "connecting"}, {"Session", QJsonObject{
                        {"id", QString::number(starts)}, {"active", true}, {"mode", "proxy"},
                        {"profile_id", ProfileStore::identity(request.value("config").toString())}}}};
                    if (holdStart) return;
                } else if (command == "stop") {
                    ++stops;
                    if (failStop) {
                        status = {{"State", "failed"}, {"LastError", "cleanup_failed_run_recover"}};
                        respond(socket, request, {{"error", "session_stop_failed"}});
                        return;
                    }
                    if (holdStop) {
                        status.insert("State", "stopping");
                        stopSocket = socket;
                        stopRequest = request;
                        return;
                    }
                    auto session = status.value("Session").toObject();
                    session.insert("active", false);
                    status = {{"State", "disconnected"}, {"Session", session}};
                } else if (command == "events") {
                    extra = {{"cursor", 12}, {"dropped", dropped}};
                    if (invalidCursor) extra.insert("error", "invalid_cursor");
                } else if (command == "stats") {
                    extra.insert("stats", QJsonObject{{"bytes_up", bytes}, {"bytes_down", bytes}});
                }
                require(command != "shutdown" && command != "recover", "ordinary controls must not shutdown or recover");
                respond(socket, request, extra);
            });
        });
        const auto synchronized = [&] { return window.helloReady_ && !window.statusNeeded_ && !window.core_.busy(); };
        const auto request = [&](const QString &command) {
            require(!window.core_.busy(), "fixture request serialization");
            require(window.core_.request(command), "fixture request accepted");
            require(waitFor([&] { return !window.core_.busy(); }), "fixture response");
        };
        window.poll();
        require(waitFor(synchronized), "initial hello and status");
        require(window.state_ == "connected" && window.activeProfileId_ == window.profiles_[0].id,
                "startup must adopt authoritative session");
        require(starts == 0 && window.autoConnectAttempted_, "startup adoption must not start again");
        require(window.activeProxyMode(), "runtime session mode must be authoritative");
        require(window.connectButton_->isEnabled(), "active profile must allow disconnect");

        holdStop = true;
        window.profileList_->setCurrentRow(1);
        window.toggleConnection();
        window.toggleConnection();
        require(waitFor([&] { return stops == 1; }), "switch must send one stop");
        require(starts == 0 && window.operation_ == "stop", "switch must wait for cleanup");
        int heartbeats = 0;
        QTimer heartbeat;
        heartbeat.setInterval(5);
        QObject::connect(&heartbeat, &QTimer::timeout, &heartbeat, [&] { ++heartbeats; });
        heartbeat.start();
        QElapsedTimer delay;
        delay.start();
        require(waitFor([&] { return delay.elapsed() > 70; }), "delayed stop UI interval");
        require(heartbeats > 3 && starts == 0, "pending stop must leave UI responsive without starting");
        auto session = status.value("Session").toObject();
        session.insert("active", false);
        status = {{"State", "disconnected"}, {"Session", session}};
        respond(stopSocket, stopRequest);
        require(waitFor([&] { return starts == 1 && window.operation_.isEmpty() && !window.statusNeeded_ && !window.core_.busy(); }),
                "successful cleanup must start target exactly once");
        require(window.state_ == "connecting" && window.activeProfileId_ == window.profiles_[1].id,
                "start acceptance is connecting, not connected");

        status.insert("State", "connected");
        request("status");
        const int requestBaseline = requests;
        QTimer polling;
        polling.setInterval(10);
        QObject::connect(&polling, &QTimer::timeout, &polling, [&] { window.poll(); });
        polling.start();
        delay.restart();
        require(waitFor([&] { return delay.elapsed() > 1100; }), "polling observation interval");
        polling.stop();
        require(waitFor([&] { return !window.core_.busy(); }), "polling drain");
        require(requests - requestBaseline <= 8, "frequent ticks must coalesce background work without a request storm");
        request("stats");
        require(window.statsClock_.isValid(), "stats must establish monotonic baseline");
        bytes = 10;
        request("stats");
        require(!window.trafficLabel_->text().contains("/s"), "counter decrease must reset rate");
        auto changed = status.value("Session").toObject();
        changed.insert("id", "replacement");
        status.insert("Session", changed);
        request("stats");
        require(!window.trafficLabel_->text().contains("/s"), "session change in stats must reset rate");
        dropped = true;
        request("events");
        require(window.statusNeeded_, "dropped history must request authoritative status");
        require(waitFor(synchronized), "dropped history resync");
        dropped = false;
        invalidCursor = true;
        request("events");
        require(window.eventCursor_ == 0 && window.statusNeeded_, "invalid cursor must reset and resync");
        require(waitFor(synchronized), "cursor resync");
        invalidCursor = false;

        // Restart can be observed in stats, not only events.
        window.pendingStartPath_ = window.profiles_[0].path;
        const quint64 generation = window.connectionGeneration_;
        instance = "second";
        request("stats");
        require(window.pendingStartPath_.isEmpty() && window.connectionGeneration_ > generation, "instance change cancels old intentions");
        require(waitFor(synchronized), "restart handshake");
        require(starts == 1, "restart must not restore a profile automatically");

        // Accepted start with a lost reply must be adopted, never retried.
        holdStop = false;
        window.toggleConnection();
        require(waitFor([&] { return window.cleanDisconnected() && window.operation_.isEmpty() && !window.core_.busy(); }), "ordinary stop");
        holdStart = true;
        window.operation_ = "start";
        require(window.core_.request("start", {{"config", window.profiles_[0].path}}, 30), "ambiguous start request");
        require(waitFor([&] { return starts == 2 && !window.helloReady_; }), "ambiguous start timeout");
        require(window.connectionLabel_->text() == "Could not prepare connection", "IPC loss is not disconnected");
        window.retryAt_ = 0;
        window.poll();
        require(waitFor(synchronized), "ambiguous start status reconciliation");
        require(starts == 2 && window.state_ == "connecting", "accepted timed-out start must not be repeated");
        holdStart = false;

        // A stop timeout must discard switching, including after later cleanup.
        window.profileList_->setCurrentRow(1);
        window.pendingStartPath_ = window.profiles_[1].path;
        window.operation_ = "stop";
        holdStop = true;
        require(window.core_.request("stop", {}, 30), "stop timeout request");
        require(waitFor([&] { return !window.helloReady_; }), "stop timeout");
        require(window.pendingStartPath_.isEmpty(), "stop timeout must cancel target");
        status = {{"State", "disconnected"}};
        window.retryAt_ = 0;
        window.poll();
        require(waitFor(synchronized) && starts == 2, "later cleanup must not resurrect canceled start");

        // Cleanup failure cancels switching and never starts/recover automatically.
        bool cleanupDialog = false;
        QTimer dialogCloser;
        dialogCloser.setInterval(10);
        QObject::connect(&dialogCloser, &QTimer::timeout, &dialogCloser, [&] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                cleanupDialog = true;
                box->accept();
            }
        });
        dialogCloser.start();
        failStop = true;
        window.pendingStartPath_ = window.profiles_[1].path;
        window.operation_ = "stop";
        request("stop");
        require(cleanupDialog && window.pendingStartPath_.isEmpty() && window.cleanupBlocked_ && starts == 2,
                "failed stop must report cleanup and cancel switching");
        dialogCloser.stop();
        failStop = false;
        require(waitFor(synchronized), "failed stop status resync");
        // Cleanup failure remains visible even when the GUI did not initiate it.
        window.developerMode_ = true; // Isolated helper fixture is not a protected package.
        status = {{"State", "failed"}, {"LastError", "cleanup_failed_run_recover"}};
        request("status");
        window.toggleConnection();
        require(window.cleanupBlocked_ && window.operation_.isEmpty() && starts == 2,
                "cleanup failure must block a new session");
        require(window.connectionLabel_->text() == "Could not prepare connection"
                && window.recoveryAction_ == MainWindow::RecoveryAction::Restore,
                "cleanup failure offers recovery instead of disconnected state");
        status = {{"State", "connected"}, {"Session", QJsonObject{{"id", "native"}, {"active", true}, {"backend", "ikev2"}}}};
        request("status");
        require(!window.connectButton_->isEnabled(), "native IKEv2 must not expose packet control");

        // Independent idle fixture for asynchronous validation and startup policies.
        status = {{"State", "disconnected"}};
        window.cleanupBlocked_ = false;
        request("status");
        window.profileList_->setCurrentRow(0);
        write(window.profiles_[0].path, config + "# fixture-reject\n");
        window.toggleConnection();
        require(waitFor([&] { return window.operation_.isEmpty(); }), "rejected validation must complete");
        require(starts == 2, "rejected profile must not reach start");
        write(window.profiles_[0].path, config + "# fixture-delay\n");
        window.toggleConnection();
        require(window.operation_ == "validate", "validation must be asynchronous");
        window.toggleConnection();
        const int heartbeatBefore = heartbeats;
        write(window.profiles_[0].path, config);
        require(waitFor([&] { return window.operation_.isEmpty(); }), "changed profile validation must complete");
        require(starts == 2 && heartbeats > heartbeatBefore, "changed profile must cancel start without blocking UI");
        write(window.profiles_[0].path, config + "# fixture-delay\n");
        window.toggleConnection();
        window.invalidateConnection();
        write(window.profiles_[0].path, config);
        window.poll();
        require(waitFor(synchronized), "backend invalidation must resynchronize");
        require(starts == 2 && !window.validationProcess_, "stale CLI must not start after invalidation");
        // Core failures invalidate from event callbacks; the discarded CLI must
        // be deleted only after its exit is reported, never while still running.
        write(window.profiles_[0].path, config + "# fixture-delay\n");
        window.toggleConnection();
        const QPointer<QProcess> discarded = window.validationProcess_;
        require(discarded, "delayed validation must be running before callback invalidation");
        static QtMessageHandler previousHandler = nullptr;
        static int destroyedRunning = 0;
        destroyedRunning = 0;
        previousHandler = qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &message) {
            if (message.startsWith("QProcess: Destroyed while process")) ++destroyedRunning;
            if (previousHandler) previousHandler(type, context, message);
        });
        QTimer::singleShot(0, &window, [&window] { window.invalidateConnection(); });
        const bool reaped = waitFor([&] { return !discarded; });
        qInstallMessageHandler(previousHandler);
        require(reaped && destroyedRunning == 0, "callback invalidation must reap the discarded CLI before deleting it");
        write(window.profiles_[0].path, config);
        window.poll();
        require(waitFor(synchronized) && starts == 2 && !window.validationProcess_, "callback invalidation must resynchronize");

        QSettings().setValue("app/autoConnectProfile", "removed-profile");
        window.autoConnectAttempted_ = false;
        request("status");
        require(starts == 2 && window.autoConnectAttempted_, "missing startup profile must not choose a replacement");
        QSettings().setValue("app/autoConnectProfile", window.profiles_[1].id);
        window.autoConnectAttempted_ = false;
        request("status");
        require(waitFor([&] { return starts == 3 && window.operation_.isEmpty() && !window.statusNeeded_ && !window.core_.busy(); }),
                "opt-in auto-connect must start once after synchronized idle status");
        request("hello");
        require(waitFor(synchronized) && starts == 3, "repeated handshake must not auto-connect again");

        status = {{"State", "disconnected"}};
        version = "0.3.0";
        instance = "updated";
        window.coreUpdateAwaitingResult_ = true;
        window.expectedCoreVersionAfterUpdate_ = version;
        window.reconnectProfileAfterUpdate_ = window.profiles_[0].id;
        window.invalidateConnection();
        window.poll();
        require(waitFor([&] { return starts == 4 && window.operation_.isEmpty() && !window.statusNeeded_ && !window.core_.busy(); }),
                "verified update must restore prior profile once");
        require(!window.coreUpdateAwaitingResult_, "update verification must finish");
        status = {{"State", "disconnected"}};
        window.coreUpdateAwaitingResult_ = true;
        window.expectedCoreVersionAfterUpdate_ = "9.9.9";
        window.reconnectProfileAfterUpdate_ = window.profiles_[0].id;
        window.invalidateConnection();
        window.poll();
        require(waitFor(synchronized) && starts == 4, "mismatched update must not reconnect");

        write(window.profiles_[0].path, "[Profile]\nName = Unsupported\n[AccessKey]\nType = future\n");
        window.coreUpdateAwaitingResult_ = true;
        window.expectedCoreVersionAfterUpdate_ = version;
        window.reconnectProfileAfterUpdate_ = window.profiles_[0].id;
        window.invalidateConnection();
        window.poll();
        require(waitFor(synchronized) && starts == 4 && window.reconnectResultProfileId_.isEmpty(),
                "unconnectable update target must finish with cancellation, not hang reconnecting");

        const QByteArray wrapped("[Profile]\nName = Wrapped\n[AccessKey]\nType = shadowsocks\nServer = example.invalid\nPort = 443\nMethod = aes-256-gcm\nPassword = INNER_SENTINEL\n[ShadowTLS]\nServerName = outer.invalid\nPassword = OUTER_SENTINEL\n");
        write(window.profiles_[0].path, wrapped);
        window.profileList_->setCurrentRow(0);
        failStart = true;
        bool rejectionShown = false;
        QTimer dismiss;
        QObject::connect(&dismiss, &QTimer::timeout, [&] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                rejectionShown = box->text().contains("configuration is invalid") && box->detailedText().contains("OUTER_SENTINEL");
                box->accept();
            }
        });
        dismiss.start(10);
        window.toggleConnection();
        require(waitFor([&] { return rejectionShown && window.operation_.isEmpty(); }), "service rejection after CLI acceptance is displayed");
        dismiss.stop();
        QFile retainedProfile(window.profiles_[0].path);
        require(retainedProfile.open(QIODevice::ReadOnly) && retainedProfile.readAll() == wrapped, "service rejection preserves profile bytes");
        require(window.log_->toPlainText().contains("OUTER_SENTINEL"), "service rejection retains supplied diagnostic detail");

        // The selection checks below need row 0 to stay an unsupported packet profile.
        write(window.profiles_[0].path, "[Profile]\nName = Unsupported\n[AccessKey]\nType = future\n");
        const int nativeStarts = starts;
        // Native profiles remain displayable but can never use packet lifecycle paths.
        const QString nativePath = directory.filePath("native.conf");
        write(nativePath, "[Profile]\nName = Native\n[IKEv2]\nWindowsProfile = Fixture\nServerName = vpn.invalid\n[Interface]\nMode = vpn\n");
        const Profile native{ProfileStore::identity(nativePath), "Native", nativePath};
        window.profiles_.append(native);
        window.store_ = ProfileStore(directory.path());
        require(window.store_.save(window.profiles_), "save isolated native selector fixture");
        window.refreshProfiles();
        const int nativeRow = window.profiles_.size() - 1;
        require(window.profileInfo(nativeRow).backend == ProfileConnectionInfo::Backend::NativeIKEv2,
                "native backend discriminator");
        require(window.autoConnectProfile_->findData(native.id) < 0, "native excluded from startup selector");
        require(!window.profileList_->item(nativeRow)->data(Qt::UserRole + 1).toBool(),
                "native unavailable label is not replaced by unknown protocol");
        window.profileList_->setCurrentRow(nativeRow);
        window.updateConnectionButton();
        require(!window.connectButton_->isEnabled() && window.nativeStatusLabel_->text().contains("approved Core/Desktop artifact pair and fresh admission"),
                "native action disabled with unavailable explanation");
        const auto settingsKeys = QSettings().allKeys();
        const auto startupProfile = QSettings().value("app/autoConnectProfile");
        window.toggleConnection();
        require(!window.startProfile(nativeRow) && window.operation_.isEmpty(), "native never starts packet validation");
        require(QSettings().allKeys() == settingsKeys && QSettings().value("app/autoConnectProfile") == startupProfile,
                "blocked native route does not mutate settings");
        require(waitFor(synchronized), "native dispatch fixture synchronized");
        window.startReadyPath_ = nativePath;
        window.operation_ = "start";
        window.poll();
        require(window.operation_.isEmpty() && starts == nativeStarts, "stale dispatch cannot send native daemon start");
        QSettings().setValue("app/autoConnectProfile", native.id);
        QSettings().setValue("app/autoConnect", true);
        window.autoConnectAttempted_ = false;
        window.completeSynchronization();
        require(window.autoConnectAttempted_ && starts == nativeStarts && window.operation_.isEmpty(), "stale native autostart is ignored");
        window.coreUpdateAwaitingResult_ = true;
        window.expectedCoreVersionAfterUpdate_ = version;
        window.reconnectProfileAfterUpdate_ = native.id;
        window.completeSynchronization();
        require(!window.coreUpdateAwaitingResult_ && window.reconnectResultProfileId_.isEmpty() && starts == nativeStarts,
                "native never restored by packet updater");
        // Selection must not strand the authoritative packet session. Neither an
        // unsupported packet profile nor native selection is a switch target.
        holdStop = false;
        for (const int selectedRow : {0, nativeRow}) {
            for (const QString activeState : {QStringLiteral("connected"), QStringLiteral("connecting"), QStringLiteral("reconnecting")}) {
                status = {{"State", activeState}, {"Session", QJsonObject{
                    {"id", "packet-selection"}, {"active", true}, {"profile_id", window.profiles_[1].id}, {"mode", "proxy"}}}};
                request("status");
                window.profileList_->setCurrentRow(selectedRow);
                window.updateConnectionButton();
                require(window.connectButton_->isEnabled() && window.connectButton_->text() == "Disconnect",
                        "unavailable selection must allow authoritative packet disconnect");
                const int previousStops = stops;
                window.connectButton_->click();
                require(!window.connectButton_->isEnabled(), "pending packet stop disables duplicate action");
                window.toggleConnection();
                require(waitFor([&] { return stops == previousStops + 1 && synchronized() && window.operation_.isEmpty(); }),
                        "unavailable selection sends exactly one packet stop");
                require(starts == nativeStarts && window.pendingStartPath_.isEmpty() && !window.nativeSession_,
                        "disconnect never starts native or switches backend");
            }
        }
        for (const QString blockedState : {QStringLiteral("stopping"), QStringLiteral("unknown")}) {
            status = {{"State", blockedState}, {"Session", QJsonObject{{"id", "packet-selection"}, {"active", true}}}};
            request("status");
            const int previousStops = stops;
            require(!window.connectButton_->isEnabled(), "ambiguous or stopping state disables disconnect");
            window.toggleConnection();
            require(stops == previousStops && window.operation_.isEmpty(), "blocked state cannot duplicate stop");
        }
        status = {{"State", "disconnected"}};
        window.retryAt_ = 0;
        window.poll();
        require(waitFor(synchronized), "unknown state resynchronizes before polling test");

        // Prove the update guard independently, before the permanent native block.
        require(!window.nativeOwnerBlocked_ && synchronized(), "polling fixture has no native owner block");
        window.nextStatus_ = 0;
        const int liveBefore = requests;
        window.poll();
        require(waitFor([&] { return requests > liveBefore && synchronized(); }), "regular polling is live before update");
        window.nextStatus_ = 0;
        const int suspendedBefore = requests;
        window.coreUpdateCommand_ = true;
        window.poll();
        require(requests == suspendedBefore && !window.core_.busy(), "service update alone suspends polling");
        window.coreUpdateCommand_ = false;
        window.poll();
        require(waitFor([&] { return requests > suspendedBefore && synchronized(); }), "regular polling resumes after update");

        const int beforeUpdateRefresh = requests;
        window.refreshCoreAfterUpdate();
        require(waitFor([&] { return requests > beforeUpdateRefresh && synchronized(); }),
                "unbound update refresh still synchronizes the packet backend");
        if (!window.developmentStatus_) window.developmentPage()->setParent(&window);
        const quint64 beforeDevelopmentChange = window.connectionGeneration_;
        window.development_->select("relative-invalid-core");
        require(window.connectionGeneration_ > beforeDevelopmentChange
                && window.connectionLabel_->text() == "Could not prepare connection"
                && window.developmentStatus_->text() == window.development_->status()
                && !window.development_->busy() && window.development_->failed(),
                "unbound Development failure still resets packet state and updates controls");
        // Return to the system backend so the failed selection does not mask IPC diagnostics.
        window.development_->select({});
        require(!window.development_->busy() && !window.development_->failed(), "Development failure clears on system backend");
        window.core_.setEndpoint(server.serverName());

        window.repairSnapshotTest_ = [](ConnectionRepair::Snapshot &snapshot) {
            snapshot.helperAvailable = true;
            snapshot.helperTrusted = true;
        };
        window.connectionFailure("hello", CoreClient::Failure::AccessDenied, "Access to Core was denied");
        require(window.coreManagementTitle_->text() == "Cannot inspect the service with current permissions", "access denial must be distinct");
        window.connectionFailure("hello", CoreClient::Failure::Incompatible, "incompatible");
        require(window.coreManagementTitle_->text() == "Application and service versions differ", "incompatible IPC must be distinct");
        require(!window.nativeController_, "ordinary packet window never constructs a native controller");
        auto nativeFixture = std::make_unique<NativeIKEv2SessionTest::Fixture>();
        nativeFixture->discover();
        nativeFixture->observe(nativeFixture->sessionReply("connected", true));
        bool queuedRefreshDelivered = false, queuedDevelopmentDelivered = false;
        const auto queueLegacyCallbacks = [&] {
            queuedRefreshDelivered = queuedDevelopmentDelivered = false;
            QTimer::singleShot(0, &window, [&] {
                window.refreshCoreAfterUpdate();
                queuedRefreshDelivered = true;
            });
            QTimer::singleShot(0, &window, [&] {
                emit window.development_->changed();
                queuedDevelopmentDelivered = true;
            });
        };
        const auto checkLegacyCallbacks = [&] {
            const QString presentation = window.connectionLabel_->text();
            const quint64 generation = window.connectionGeneration_;
            const int previousRequests = requests;
            const auto previousCalls = nativeFixture ? nativeFixture->calls.size() : 0;
            if (!window.trayMenu_) window.trayMenu_ = new QMenu(&window);
            window.rebuildTrayMenu();
            const QString trayStatus = window.trayStatusAction_->text();
            window.developmentStatus_->setText("stale development status");
            require(waitFor([&] { return queuedRefreshDelivered && queuedDevelopmentDelivered; }),
                    "queued legacy callbacks delivered after native authority transition");
            require(window.nativeOwnerBlocked_ && window.connectionLabel_->text() == presentation
                    && window.nativeStatusLabel_->text() == presentation
                    && window.trayStatusAction_->text() == trayStatus
                    && window.connectionGeneration_ == generation,
                    "queued legacy callbacks preserve native presentation tray and authority");
            require(!window.core_.busy() && requests == previousRequests
                    && (!nativeFixture || nativeFixture->calls.size() == previousCalls),
                    "queued legacy callbacks send no daemon hello or native activation");
            require(window.developmentStatus_->text() == window.development_->status()
                    && window.developmentStopOrphan_->isHidden() == !window.development_->orphanAvailable(),
                    "native authority preserves Development status and cleanup controls");
        };
        queueLegacyCallbacks();
        window.bindNativeController(&nativeFixture->controller);
        checkLegacyCallbacks();
        require(window.nativeOwnerBlocked_ && !window.nativeSession_
                && window.nativeStatusLabel_->text().contains("Active"),
                "initial real controller snapshot binds independently of daemon flag");
        nativeFixture->observe(nativeFixture->sessionReply("stopping", true));
        require(window.nativeStatusLabel_->text().contains("Inactive"), "changed signal propagates stopping snapshot");
        auto retained = nativeFixture->sessionReply("stopping", true);
        auto ns = retained["status"].toObject();
        auto protection = ns["native"].toObject();
        protection["wfp"] = "retained"; protection["dns"] = "modified";
        ns["native"] = protection; retained["status"] = ns;
        nativeFixture->observe(retained);
        require(window.nativeStatusLabel_->text().contains("Retained")
                && window.nativeStatusLabel_->text().contains("Modified"), "retained signal reaches native UI");
        JsonTranslator applicationTranslator;
        require(applicationTranslator.load(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../assets/translations/ru.json")),
                "load actual Russian translator");
        QCoreApplication::installTranslator(&applicationTranslator);
        nativeFixture->observe(retained);
        require(!window.nativeStatusLabel_->text().contains("WFP: Retained"), "controller signal uses Russian translation");
        QCoreApplication::removeTranslator(&applicationTranslator);
        nativeFixture->observe(retained);
        const QString retainedText = window.nativeStatusLabel_->text();
        window.handleStatus({{"State", "disconnected"}});
        window.refreshProfiles();
        window.trayMenu_ = new QMenu(&window);
        window.rebuildTrayMenu();
        require(window.nativeStatusLabel_->text() == retainedText
                && window.connectionLabel_->text() == retainedText
                && window.trayStatusAction_->text().contains(retainedText),
                "packet replies profile refresh and tray rebuild preserve native uncertainty");
        const auto nativeCalls = nativeFixture->calls.size();
        QCloseEvent close;
        window.closeEvent(&close);
        window.trayMenu_->actions().last()->trigger();
        require(!window.quitRequested_ && nativeFixture->calls.size() == nativeCalls,
                "close and tray quit never stop or close native owner");
        queueLegacyCallbacks();
        nativeFixture.reset();
        checkLegacyCallbacks();
        require(!window.nativeController_ && window.nativeOwnerBlocked_
                && window.nativeStatusLabel_->text().contains("Unknown"),
                "controller destruction leaves guarded sticky unknown");
        require(!window.runServiceCommand({"service", "update"}, "fixture") && !window.coreUpdatePrerequisite(),
                "native unknown ownership blocks service mutation");
        const int nativeRequests = requests;
        QSettings().setValue("app/autoConnect", true);
        QSettings().setValue("app/autoConnectProfile", window.profiles_.first().id);
        window.autoConnectAttempted_ = false;
        window.completeSynchronization();
        window.restartApplication();
        require(!window.quitRequested_ && !window.canActivateProfile(0), "settings and restart cannot bypass native block");
        window.poll();
        window.toggleConnection();
        require(requests == nativeRequests && starts == nativeStarts && !window.connectButton_->isEnabled(),
                "native ownership blocks polling and packet activation");

        window.core_.cancel();
        QSettings().clear();
    }
};
