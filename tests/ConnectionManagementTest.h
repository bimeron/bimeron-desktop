#pragma once
#include "GeoSitePicker.h"

#include <QListWidget>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QPlainTextEdit>
#include <QClipboard>
#include <QPointer>
#include <QCloseEvent>
#include <QSystemTrayIcon>
#include <QTranslator>
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
        downloadFeedback(destination);
        MainWindow window;
        window.core_.cancel();
        if (app.arguments().contains("--geosite-only")) {
            require(!qEnvironmentVariable("BIMERON_GEOSITE_IDS").isEmpty(), "real catalog for isolated picker captures");
            captureGeoSite(&window, destination);
            app.removeTranslator(&translator);
            return 0;
        }
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

    static void run() {
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
                const QString previous = QApplication::clipboard()->text();
                QApplication::clipboard()->setText("unsupported clipboard");
                QCoreApplication::processEvents();
                require(waitFor([&] { return !paste->isVisible(); }), "Unsupported clipboard hides Paste");
                const QSize original = dialog->size();
                fields[0]->setText("typed text");
                QCoreApplication::processEvents();
                require(dialog->size() == original, "Typing retains geometry");
                QApplication::clipboard()->setText("https://example.invalid:443");
                QCoreApplication::processEvents();
                require(waitFor([&] { return paste->isEnabled() && paste->isVisible(); }), "Supported clipboard shows Paste");
                paste->click();
                QCoreApplication::processEvents();
                require(dialog->width() == original.width() && dialog->height() > original.height(), "Valid URI reveals name without width change");
                require(fields[1]->isVisible() && fields[1]->y() < fields[0]->y(), "Name appears above URI");
                fields[0]->setText("ssh://alice@example.invalid");
                QCoreApplication::processEvents();
                require(fields[1]->isVisible() && dialog->width() == original.width(), "SSH retains name and dialog width");
                require(dialog->findChildren<QLineEdit *>().size() == 4, "SSH identity and known hosts retained");
                QApplication::clipboard()->setText(previous);
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
        require(window.connectionLabel_->text() == "Core unavailable", "IPC loss is not disconnected");
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
        status = {{"State", "failed"}, {"LastError", "cleanup_failed_run_recover"}};
        request("status");
        window.toggleConnection();
        require(window.cleanupBlocked_ && window.operation_.isEmpty() && starts == 2,
                "cleanup failure must block a new session");
        require(window.connectionLabel_->text().contains("failed", Qt::CaseInsensitive), "failed is not disconnected");
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
                rejectionShown = box->text().contains("configuration is invalid") && !box->text().contains("SENTINEL");
                box->accept();
            }
        });
        dismiss.start(10);
        window.toggleConnection();
        require(waitFor([&] { return rejectionShown && window.operation_.isEmpty(); }), "service rejection after CLI acceptance is displayed");
        dismiss.stop();
        QFile retained(window.profiles_[0].path);
        require(retained.open(QIODevice::ReadOnly) && retained.readAll() == wrapped, "service rejection preserves profile bytes");
        require(!window.log_->toPlainText().contains("SENTINEL"), "service rejection redacts secrets");

        const int before = requests;
        window.coreUpdateCommand_ = true;
        window.poll();
        require(requests == before, "service update must suspend regular polling");
        window.coreUpdateCommand_ = false;
        window.connectionFailure("hello", CoreClient::Failure::AccessDenied, "Access to Core was denied");
        require(window.connectionLabel_->text() == "Access to Core was denied", "access denial must be distinct");
        window.connectionFailure("hello", CoreClient::Failure::Incompatible, "incompatible");
        require(window.connectionLabel_->text().contains("incompatible"), "incompatible IPC must be distinct");
        window.core_.cancel();
        QSettings().clear();
    }
};
