#include "applicationcontroller.hpp"
#include "settingscontroller.hpp"
#include "facilitycontroller.hpp"
#include "storagecontroller.hpp"
#include "profilecontroller.hpp"
#include "accountcontroller.hpp"
#include "accountartwork.hpp"
#include "orbipomcontroller.hpp"
#include "eventlogcontroller.hpp"
#include "appshortcutscontroller.hpp"
#include "projectioncanvas.hpp"
#include "mediaeditor.hpp"
#include "centerlogo.hpp"
#include <QApplication>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QAction>
#include <KGlobalAccel>
#include "watchanimationcontroller.hpp"
#include "sourcedial.hpp"
#include "batterycontroller.hpp"
#include "nowplayingcontroller.hpp"
#include "splayerlyrics.hpp"
#include "audiocontroller.hpp"
#include "terminalsession.hpp"
#include "terminalview.hpp"
#include "activitycontroller.hpp"
#include "calendarcontroller.hpp"
#include "focusmodecontroller.hpp"
#include "clipboardcontroller.hpp"
#include "notescontroller.hpp"
#include "fileshelfcontroller.hpp"
#include "archivecontroller.hpp"
#include "readercontroller.hpp"

#include <QGuiApplication>
#include <QIcon>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QProcess>
#include <QQuickAsyncImageProvider>
#include <QQuickImageProvider>
#include <QQuickTextureFactory>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QtConcurrentRun>

#include <unistd.h>

#include <algorithm>

namespace
{
class LogoImageProvider final : public QQuickImageProvider {
public:
    LogoImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &) override {
        if (!cache.contains(id)) {
            const QStringList presets{"endfield", "rhodesIsland", "babel", "rhineLab"};
            const QString path = id.startsWith("custom/") ? QUrl::fromPercentEncoding(id.mid(7).toUtf8())
                               : ":/assets/settings/center-logos/" + presets.value(id.toInt(), "endfield") + ".png";
            QImageReader reader(path); const auto dimensions = reader.size();
            if (dimensions.isValid()) reader.setScaledSize(dimensions.scaled(768, 768, Qt::KeepAspectRatio));
            QImage image = reader.read();
            if (id == "0" && !image.isNull()) { const double unit = image.width() / 430.; image = image.copy(0, qRound(142 * unit), image.width(), qRound(83 * unit)); }
            if (id.startsWith("custom/")) for (auto it = cache.begin(); it != cache.end();) { if (it.key().startsWith("custom/")) it = cache.erase(it); else ++it; }
            cache[id] = centerLogoArtwork(image);
        }
        if (size) *size = cache.value(id).size();
        return cache.value(id);
    }
private:
    QHash<QString, QImage> cache;
};
class AudioIconProvider final : public QQuickImageProvider
{
public:
    AudioIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requested) override {
        const QSize target = requested.isValid() ? requested : QSize(64, 64);
        QImage image = QIcon::fromTheme(QUrl::fromPercentEncoding(id.toUtf8())).pixmap(target).toImage();
        if (image.isNull()) { image = QImage(target, QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent); }
        if (size) *size = image.size();
        return image;
    }
};
class ActivityIconProvider final : public QQuickImageProvider
{
public:
    ActivityIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requested) override {
        const QString name = QUrl::fromPercentEncoding(id.toUtf8());
        const QSize target = requested.isValid() ? requested : QSize(64, 64);
        const QIcon icon = QFileInfo(name).isAbsolute() ? QIcon(name) : QIcon::fromTheme(name);
        const auto image = icon.pixmap(target).toImage();
        if (size) *size = image.size();
        return image;
    }
};
class ArchiveThumbnailResponse final : public QQuickImageResponse
{
public:
    ArchiveThumbnailResponse(QString id, QSize requested)
    {
        connect(&m_watcher, &QFutureWatcherBase::finished, this, &QQuickImageResponse::finished);
        m_watcher.setFuture(QtConcurrent::run([id = std::move(id), requested] {
            const QByteArray encoded = id.toLatin1();
            const QString path = QString::fromUtf8(QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding));
            if (path.isEmpty()) return QImage();
            QImageReader reader(path);
            reader.setAutoTransform(true);
            if (reader.canRead()) {
                QSize size = reader.size();
                const QSize wanted = requested.isValid() ? requested : QSize(256, 256);
                if (size.isValid()) { size.scale(wanted, Qt::KeepAspectRatio); reader.setScaledSize(size); }
                return reader.read();
            }
            QProcess process;
            process.start(QStringLiteral("ffmpegthumbnailer"), {QStringLiteral("-i"), path, QStringLiteral("-o"), QStringLiteral("/dev/stdout"),
                QStringLiteral("-s"), QString::number(std::max(64, requested.isValid() ? std::max(requested.width(), requested.height()) : 256)), QStringLiteral("-q"), QStringLiteral("8")});
            if (!process.waitForStarted(2000) || !process.waitForFinished(8000)) { process.kill(); process.waitForFinished(); return QImage(); }
            return QImage::fromData(process.readAllStandardOutput());
        }));
    }
    QQuickTextureFactory *textureFactory() const override { return QQuickTextureFactory::textureFactoryForImage(m_watcher.result()); }
private:
    QFutureWatcher<QImage> m_watcher;
};
class ArchiveThumbnailProvider final : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override
    {
        return new ArchiveThumbnailResponse(id, requestedSize);
    }
};
QString requestedCommand(const QStringList &arguments, bool &valid)
{
    valid = true;
    if (arguments.size() <= 1) {
        return QStringLiteral("toggle");
    }

    const QString argument = arguments.at(1).toLower();
    if (argument == QStringLiteral("--background")) return QStringLiteral("background");
    if (argument == QStringLiteral("--show")) {
        return QStringLiteral("show");
    }
    if (argument == QStringLiteral("--hide")) {
        return QStringLiteral("hide");
    }
    if (argument == QStringLiteral("--toggle")) {
        return QStringLiteral("toggle");
    }
    if (argument == QStringLiteral("--quit")) {
        return QStringLiteral("quit");
    }

    valid = false;
    return {};
}

QString localServerName()
{
    return QStringLiteral("hyprendfield-%1").arg(static_cast<qulonglong>(geteuid()));
}

bool sendToExistingInstance(const QString &serverName, const QString &command)
{
    QLocalSocket socket;
    socket.connectToServer(serverName, QIODevice::WriteOnly);
    if (!socket.waitForConnected(250)) {
        return false;
    }

    socket.write(command.toUtf8());
    socket.flush();
    socket.waitForBytesWritten(250);
    socket.disconnectFromServer();
    return true;
}
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("wayland"));
    QSurfaceFormat surfaceFormat;
    surfaceFormat.setRenderableType(QSurfaceFormat::OpenGLES);
    surfaceFormat.setVersion(3, 0);
    surfaceFormat.setSamples(4);
    QSurfaceFormat::setDefaultFormat(surfaceFormat);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("HyprEndfield"));
    application.setApplicationDisplayName(QStringLiteral("HyprEndfield"));
    application.setOrganizationName(QStringLiteral("HyprEndfield"));
    application.setApplicationVersion(QStringLiteral(HYPR_VERSION));
    application.setQuitOnLastWindowClosed(false);

    bool commandValid = false;
    const QString command = requestedCommand(application.arguments(), commandValid);
    if (!commandValid) {
        qCritical("Usage: HyprEndfield [--show|--hide|--toggle|--quit|--background]");
        return 2;
    }

    const QString serverName = localServerName();
    if (sendToExistingInstance(serverName, command)) {
        return 0;
    }

    if (command == QStringLiteral("hide") || command == QStringLiteral("quit")) {
        return 0;
    }

    SettingsController settings;
    ApplicationController controller;
    controller.setSettings(&settings);
    // Hyprland 用 submap、niri 用快捷键抑制来接管按键；KDE 两者都没有，
    // 因此把 Alt+Esc 注册成桌面级快捷键，开机自启后也能直接开关 HUD。
    if (qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains(QStringLiteral("KDE"), Qt::CaseInsensitive)
        || qEnvironmentVariable("XDG_SESSION_DESKTOP").compare(QStringLiteral("KDE"), Qt::CaseInsensitive) == 0) {
        auto *toggleShortcut = new QAction(&application);
        toggleShortcut->setObjectName(QStringLiteral("toggle-hud"));
        toggleShortcut->setText(QObject::tr("打开或关闭 HUD"));
        KGlobalAccel::self()->setGlobalShortcut(toggleShortcut,
            QList<QKeySequence>{QKeySequence(QStringLiteral("Alt+Esc"))});
        QObject::connect(toggleShortcut, &QAction::triggered, &controller, &ApplicationController::toggleHud);
    }
    QMenu trayMenu;
    trayMenu.addAction(QObject::tr("打开 / 关闭 HUD"), &controller, &ApplicationController::toggleHud);
    trayMenu.addAction(QObject::tr("退出"), &controller, &ApplicationController::quitApplication);
    QSystemTrayIcon tray;
    tray.setToolTip(QStringLiteral("HyprEndfield · SUPER + Esc"));
    tray.setContextMenu(&trayMenu);
    const auto updateIcon = [&] {
        const QString id = settings.value("applicationIcon").toString();
        application.setWindowIcon(QIcon(":/assets/settings/application-icons/" + id + "-app.png"));
        tray.setIcon(QIcon(":/assets/settings/application-icons/" + id + "-tray.png"));
    };
    updateIcon();
    tray.show();
    QObject::connect(&tray, &QSystemTrayIcon::activated, &controller, [&controller](auto reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) controller.toggleHud();
    });
    QObject::connect(&settings, &SettingsController::preferenceChanged, &application, [&](const QString &key, const QVariant &) {
        if (key == "applicationIcon") updateIcon();
    });
    WatchAnimationController watchAnimation;
    BatteryController battery;
    QObject::connect(&settings, &SettingsController::languageChanged, &battery, &BatteryController::refresh);
    QObject::connect(&battery, &BatteryController::stateChanged, &settings, [&] { settings.updateBattery(battery.state()); });
    QObject::connect(&settings, &SettingsController::preferenceChanged, &battery, [&](const QString &key, const QVariant &) {
        if (key == "batteryMethod") battery.setMonitoring(settings.value("batteryMethod").toInt() != 2);
    });
    settings.updateBattery(battery.state());
    battery.setMonitoring(settings.value("batteryMethod").toInt() != 2);
    NowPlayingController nowPlaying;
    SPlayerLyrics lyrics(&nowPlaying);
    AudioController audio;
    TerminalSession terminal;
    ActivityController activity;
    FacilityController facility;
    facility.setEnabled(settings.value("facilityAlerts").toBool());
    QObject::connect(&settings, &SettingsController::preferenceChanged, &facility, [&](const QString &key, const QVariant &value) {
        if (key == "facilityAlerts") facility.setEnabled(value.toBool());
    });
    QObject::connect(&settings, &SettingsController::languageChanged, &facility, &FacilityController::alertsChanged);
    CalendarController calendar;
    FocusModeController focusMode;
    ClipboardController clipboard;
    NotesController notes;
    StorageController storage;
    ProfileController profile;
    ProfileController gameProfile(nullptr, QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/HyprEndfield/GameProfile");
    AccountController account(&gameProfile);
    account.setLanguage(settings.value("languageIndex").toInt());
    QObject::connect(&settings, &SettingsController::languageChanged, &account, [&] { account.setLanguage(settings.value("languageIndex").toInt()); });
    QObject::connect(&account, &AccountController::loginRequested, &controller, [&] {
        if (controller.hudVisible()) controller.hideHud();
        else account.showLogin();
    });
    QObject::connect(&controller, &ApplicationController::hudVisibleChanged, &account, [&] { if (!controller.hudVisible()) account.showLogin(); });
    QObject::connect(&account, &AccountController::loginFinished, &controller, &ApplicationController::showHud);
    OrbiPomController orbipom;
    FileShelfController fileShelf;
    ArchiveController archive;
    auto *readerImages = new ReaderImageProvider;
    ReaderController reader(nullptr, readerImages);
    EventLogController eventLog;
    QObject::connect(&account, &AccountController::eventOccurred, &eventLog, [&](const QString &action) { eventLog.recordAction("accountAction", action); });
    QObject::connect(&gameProfile, &ProfileController::cropChanged, &eventLog, [&](const QString &target) { eventLog.record("profileCropChanged", {{"target", target}}); });
    AppShortcutsController appShortcuts;
    QObject::connect(&settings, &SettingsController::languageChanged, &appShortcuts, &AppShortcutsController::changed);
    QObject::connect(&appShortcuts, &AppShortcutsController::launchRequested, &controller, [&] {
        if (controller.hudVisible()) controller.hideHud();
        else appShortcuts.launchPending();
    });
    QObject::connect(&controller, &ApplicationController::hudVisibleChanged, &appShortcuts, [&] { if (!controller.hudVisible()) appShortcuts.launchPending(); });
    QObject::connect(&appShortcuts, &AppShortcutsController::launchFailed, &controller, &ApplicationController::showHud);
    QObject::connect(&appShortcuts, &AppShortcutsController::opened, &eventLog, [&](const QString &name) { eventLog.record("appShortcutOpened", {{"app", name}}); });
    QObject::connect(&application, &QCoreApplication::aboutToQuit, &eventLog, &EventLogController::flush);
    QObject::connect(&settings, &SettingsController::languageChanged, &eventLog, &EventLogController::stateChanged);
    QObject::connect(&controller, &ApplicationController::hudOpenChanged, &eventLog, [&] { if (controller.hudOpen()) eventLog.record("overlayOpened"); });
    QObject::connect(&notes, &NotesController::eventOccurred, &eventLog, [&](const QString &action) { eventLog.recordAction("noteAction", action, action.startsWith("edited") || action == "drawingEdited"); });
    QObject::connect(&calendar, &CalendarController::eventOccurred, &eventLog, [&](const QString &action) { eventLog.recordAction("calendarAction", action); });
    QObject::connect(&archive, &ArchiveController::eventOccurred, &eventLog, [&](const QString &action) { eventLog.recordAction("archiveAction", action, action == "edited"); });
    QObject::connect(&orbipom, &OrbiPomController::actionFinished, &eventLog, [&](const QString &action, bool success, const QString &) { if (success) eventLog.recordAction("minigameAction", action); });
    QObject::connect(&nowPlaying, &NowPlayingController::eventOccurred, &eventLog, [&](const QString &action) { eventLog.record("playbackAction", {{"action", action}, {"source", "system"}}); });
    QObject::connect(&reader, &ReaderController::actionFinished, &eventLog, [&](const QString &action, bool success, const QString &) {
        const QString event = action == "import" ? "imported" : action == "remove" ? "deleted" : action == "bookmark" ? "bookmarked" : action == "settings" ? "settings" : "";
        if (success && !event.isEmpty()) eventLog.recordAction("readerAction", event, event == "settings");
    });
    QObject::connect(&clipboard, &ClipboardController::itemCopied, &eventLog, [&](const QString &kind) { eventLog.record("clipboardCopied", {{"kind", kind}}); });
    QObject::connect(&fileShelf, &FileShelfController::itemsAdded, &eventLog, [&](const QStringList &ids) {
        for (const auto &url : fileShelf.exportUrls(ids, nullptr)) eventLog.record("shelfAdded", {{"filename", QFileInfo(url.toLocalFile()).fileName()}});
    });
    QObject::connect(&fileShelf, &FileShelfController::itemRemoved, &eventLog, [&](const QString &name) { eventLog.record("shelfRemoved", {{"filename", name}}); });
    QObject::connect(&fileShelf, &FileShelfController::shelfCleared, &eventLog, [&](int count) { eventLog.record("shelfCleared", {{"count", count}}); });
    eventLog.observeWork(focusMode.state()); eventLog.observeBattery(battery.state());
    QObject::connect(&focusMode, &FocusModeController::stateChanged, &eventLog, [&] { eventLog.observeWork(focusMode.state()); });
    QObject::connect(&battery, &BatteryController::stateChanged, &eventLog, [&] { eventLog.observeBattery(battery.state()); });
    QObject::connect(&audio, &AudioController::stateChanged, &eventLog, [&] { eventLog.observeAudio(audio.state()); });
    QObject::connect(&application, &QGuiApplication::screenAdded, &eventLog, [&](QScreen *screen) { eventLog.record("displayConnected", {{"device", screen->model().isEmpty() ? screen->name() : screen->model()}}); });
    QObject::connect(&application, &QGuiApplication::screenRemoved, &eventLog, [&](QScreen *screen) { eventLog.record("displayDisconnected", {{"device", screen->model().isEmpty() ? screen->name() : screen->model()}}); });
    QObject::connect(&profile, &ProfileController::cropChanged, &eventLog, [&](const QString &target) { eventLog.record("profileCropChanged", {{"target", target}}); });
    QObject::connect(&settings, &SettingsController::preferenceChanged, &eventLog, [&](const QString &key, const QVariant &value) {
        QString field, setting;
        if (key == "clockStyleIndex") { field = "clockStyle"; setting = QStringList{"digital", "split", "dial", "rail", "stacked"}.value(value.toInt()); }
        else if (key == "centerLogoIndex") { field = "centerLogo"; setting = QStringList{"endfield", "rhodesIsland", "babel", "rhineLab", "custom"}.value(value.toInt()); }
        else if (key == "batteryMetric") { field = "alertMetric"; setting = QStringList{"battery", "ram", "cpu", "network", "disk", "gpu"}.value(value.toInt()); }
        else if (key == "customLogo") { field = "centerLogo"; setting = "customImported"; }
        if (!field.isEmpty()) eventLog.record("displaySettingsChanged", {{"field", field}, {"value", setting}});
    });
    qmlRegisterType<TerminalView>("HyprEndfield.Terminal", 1, 0, "TerminalView");
    qmlRegisterType<SourceDial>("HyprEndfield.Source", 1, 0, "SourceDial");
    qmlRegisterType<ProjectionCanvas>("HyprEndfield.Projection", 1, 0, "ProjectionCanvas");
    qmlRegisterType<ProjectionDots>("HyprEndfield.Projection", 1, 0, "ProjectionDots");
    qmlRegisterType<MediaEditor>("HyprEndfield.Media", 1, 0, "MediaEditor");
    qmlRegisterType<ProfileArtwork>("HyprEndfield.Profile", 1, 0, "ProfileArtwork");
    qmlRegisterType<GameProfileArtwork>("HyprEndfield.Account", 1, 0, "GameProfileArtwork");
    qmlRegisterType<AccountGaugeArtwork>("HyprEndfield.Account", 1, 0, "AccountGaugeArtwork");
    qmlRegisterType<OrbiPomBoard>("HyprEndfield.OrbiPom", 1, 0, "OrbiPomBoard");
    QObject::connect(&audio, &AudioController::commandFailed, [](const QString &error) {
        qWarning().noquote() << "Audio:" << error;
    });
    QObject::connect(&nowPlaying, &NowPlayingController::commandFailed, [](const QString &error) {
        qWarning().noquote() << "SPlayer:" << error;
    });
    if (!controller.listen(serverName)) {
        qCritical("Unable to create the HyprEndfield control socket");
        return 1;
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("settingsController"), &settings);
    engine.rootContext()->setContextProperty(QStringLiteral("facilityController"), &facility);
    QObject::connect(&settings, &SettingsController::languageChanged, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("batteryController"), &battery);
    engine.rootContext()->setContextProperty(QStringLiteral("sourceWatchAnimation"), &watchAnimation);
    engine.rootContext()->setContextProperty(QStringLiteral("nowPlayingController"), &nowPlaying);
    engine.rootContext()->setContextProperty(QStringLiteral("splayerLyrics"), &lyrics);
    engine.rootContext()->setContextProperty(QStringLiteral("audioController"), &audio);
    engine.rootContext()->setContextProperty(QStringLiteral("terminalSession"), &terminal);
    engine.rootContext()->setContextProperty(QStringLiteral("activityController"), &activity);
    engine.rootContext()->setContextProperty(QStringLiteral("calendarController"), &calendar);
    engine.rootContext()->setContextProperty(QStringLiteral("focusModeController"), &focusMode);
    engine.rootContext()->setContextProperty(QStringLiteral("clipboardController"), &clipboard);
    engine.rootContext()->setContextProperty(QStringLiteral("notesController"), &notes);
    engine.rootContext()->setContextProperty(QStringLiteral("storageController"), &storage);
    engine.rootContext()->setContextProperty(QStringLiteral("profileController"), &profile);
    engine.rootContext()->setContextProperty(QStringLiteral("gameProfileController"), &gameProfile);
    engine.rootContext()->setContextProperty(QStringLiteral("accountController"), &account);
    engine.rootContext()->setContextProperty(QStringLiteral("orbipomController"), &orbipom);
    engine.rootContext()->setContextProperty(QStringLiteral("eventLogController"), &eventLog);
    engine.rootContext()->setContextProperty(QStringLiteral("appShortcutsController"), &appShortcuts);
    engine.addImageProvider(QStringLiteral("shortcut-icons"), new ShortcutIconProvider(&appShortcuts));
    engine.rootContext()->setContextProperty(QStringLiteral("fileShelfController"), &fileShelf);
    engine.rootContext()->setContextProperty(QStringLiteral("archiveController"), &archive);
    engine.rootContext()->setContextProperty(QStringLiteral("readerController"), &reader);
    engine.addImageProvider(QStringLiteral("activity-icons"), new ActivityIconProvider);
    engine.addImageProvider(QStringLiteral("settings-logos"), new LogoImageProvider);
    engine.addImageProvider(QStringLiteral("audio-icons"), new AudioIconProvider);
    engine.addImageProvider(QStringLiteral("archive-thumbnails"), new ArchiveThumbnailProvider);
    engine.addImageProvider(QStringLiteral("reader-pages"), readerImages);
    engine.loadFromModule(QStringLiteral("HyprEndfield"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window) {
        qCritical("The HyprEndfield root object is not a window");
        return 1;
    }

    controller.setWindow(window);
    settings.setBatteryWindow(window->findChild<QQuickWindow *>(QStringLiteral("batteryAlertWindow")));
    QObject::connect(&application, &QCoreApplication::aboutToQuit,
                     &focusMode, &FocusModeController::shutdown);
    fileShelf.setWindow(window);
    QObject::connect(&fileShelf, &FileShelfController::externalOpened, &controller, &ApplicationController::hideHud);
    QObject::connect(&fileShelf, &FileShelfController::dragStarted, &controller, &ApplicationController::beginFileDrag);
    QObject::connect(&fileShelf, &FileShelfController::dragFinished, &controller, &ApplicationController::endFileDrag);
    QTimer::singleShot(0, &controller, [&controller, command] {
        if (command != QStringLiteral("background")) controller.executeCommand(command);
    });

    return application.exec();
}
