#include "appshortcutscontroller.hpp"
#include <utility>

#include <QBuffer>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QIcon>
#ifdef HAVE_KSERVICE
#include <KService>
#include <KServiceGroup>
#include <KSycoca>
#include <KSycocaEntry>
#endif
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QPainter>
#include <QProcess>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSet>
#include <QStandardPaths>
#include <QTextBoundaryFinder>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#pragma push_macro("signals")
#undef signals
#include <gio/gdesktopappinfo.h>
#pragma pop_macro("signals")

namespace {
constexpr qint64 maximumBytes = 2 * 1024 * 1024;
const QStringList presetIDs{"original", "bolt", "star", "terminal", "globe", "folder", "music", "play", "brush", "code", "game", "camera", "grid", "textBubble"};
QJsonArray presetData() {
    static const auto data = [] { QFile file(":/assets/app-shortcut/artwork.json"); if (!file.open(QIODevice::ReadOnly)) return QJsonArray(); return QJsonDocument::fromJson(file.readAll()).object()["presets"].toArray(); }();
    return data;
}
QString encodedIcon(const QIcon &icon, int size = 64, const QColor &tint = {})
{
    if (icon.isNull()) return {};
    QImage image = icon.pixmap(size, size).toImage();
    if (image.isNull()) return {};
    // 类别卡片统一深色单色化，避免浅色主题图标在浅色卡片上看不清。
    if (tint.isValid()) {
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&image);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(image.rect(), tint);
        painter.end();
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
}
QString iconData(const QString &name, int size = 64, const QColor &tint = {})
{
    if (name.isEmpty()) return {};
    return encodedIcon(QIcon::fromTheme(name), size, tint);
}

QColor averageColor(QImage image) {
    if (image.isNull()) return QColor("#2A2A2A");
    image = image.scaled(8, 8, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    double r = 0, g = 0, b = 0, a = 0;
    for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x) { const auto pixel = image.pixel(x, y); r += qRed(pixel); g += qGreen(pixel); b += qBlue(pixel); a += qAlpha(pixel); }
    if (!a) return QColor("#2A2A2A");
    r = r / a * .72; g = g / a * .72; b = b / a * .72;
    const double luminance = .2126 * r + .7152 * g + .0722 * b;
    if (luminance > .55) { r *= .55 / luminance; g *= .55 / luminance; b *= .55 / luminance; }
    return QColor::fromRgbF(r, g, b);
}
QString mainCategoryKey(const QString &token)
{
    static const QHash<QString, QString> alias{
        {QStringLiteral("AudioVideo"), QStringLiteral("AudioVideo")},
        {QStringLiteral("Audio"), QStringLiteral("AudioVideo")},
        {QStringLiteral("Video"), QStringLiteral("AudioVideo")},
        {QStringLiteral("Mixer"), QStringLiteral("AudioVideo")},
        {QStringLiteral("Sequencer"), QStringLiteral("AudioVideo")},
        {QStringLiteral("Tuner"), QStringLiteral("AudioVideo")},
        {QStringLiteral("TV"), QStringLiteral("AudioVideo")},
        {QStringLiteral("AudioVideoEditing"), QStringLiteral("AudioVideo")},
        {QStringLiteral("DiscBurning"), QStringLiteral("AudioVideo")},
        {QStringLiteral("Player"), QStringLiteral("AudioVideo")},
        {QStringLiteral("Recorder"), QStringLiteral("AudioVideo")},
        {QStringLiteral("Development"), QStringLiteral("Development")},
        {QStringLiteral("Building"), QStringLiteral("Development")},
        {QStringLiteral("Debugger"), QStringLiteral("Development")},
        {QStringLiteral("IDE"), QStringLiteral("Development")},
        {QStringLiteral("GUIDesigner"), QStringLiteral("Development")},
        {QStringLiteral("Profiling"), QStringLiteral("Development")},
        {QStringLiteral("RevisionControl"), QStringLiteral("Development")},
        {QStringLiteral("Translation"), QStringLiteral("Development")},
        {QStringLiteral("WebDevelopment"), QStringLiteral("Development")},
        {QStringLiteral("Education"), QStringLiteral("Education")},
        {QStringLiteral("Art"), QStringLiteral("Education")},
        {QStringLiteral("Construction"), QStringLiteral("Education")},
        {QStringLiteral("Music"), QStringLiteral("Education")},
        {QStringLiteral("Game"), QStringLiteral("Game")},
        {QStringLiteral("ActionGame"), QStringLiteral("Game")},
        {QStringLiteral("AdventureGame"), QStringLiteral("Game")},
        {QStringLiteral("ArcadeGame"), QStringLiteral("Game")},
        {QStringLiteral("BoardGame"), QStringLiteral("Game")},
        {QStringLiteral("BlocksGame"), QStringLiteral("Game")},
        {QStringLiteral("CardGame"), QStringLiteral("Game")},
        {QStringLiteral("KidsGame"), QStringLiteral("Game")},
        {QStringLiteral("LogicGame"), QStringLiteral("Game")},
        {QStringLiteral("RolePlaying"), QStringLiteral("Game")},
        {QStringLiteral("Shooter"), QStringLiteral("Game")},
        {QStringLiteral("Simulation"), QStringLiteral("Game")},
        {QStringLiteral("SportsGame"), QStringLiteral("Game")},
        {QStringLiteral("StrategyGame"), QStringLiteral("Game")},
        {QStringLiteral("Graphics"), QStringLiteral("Graphics")},
        {QStringLiteral("2DGraphics"), QStringLiteral("Graphics")},
        {QStringLiteral("3DGraphics"), QStringLiteral("Graphics")},
        {QStringLiteral("VectorGraphics"), QStringLiteral("Graphics")},
        {QStringLiteral("RasterGraphics"), QStringLiteral("Graphics")},
        {QStringLiteral("Photography"), QStringLiteral("Graphics")},
        {QStringLiteral("Publishing"), QStringLiteral("Graphics")},
        {QStringLiteral("Scanning"), QStringLiteral("Graphics")},
        {QStringLiteral("OCR"), QStringLiteral("Graphics")},
        {QStringLiteral("Viewer"), QStringLiteral("Graphics")},
        {QStringLiteral("Network"), QStringLiteral("Network")},
        {QStringLiteral("Dialup"), QStringLiteral("Network")},
        {QStringLiteral("InstantMessaging"), QStringLiteral("Network")},
        {QStringLiteral("Chat"), QStringLiteral("Network")},
        {QStringLiteral("IRCClient"), QStringLiteral("Network")},
        {QStringLiteral("Feed"), QStringLiteral("Network")},
        {QStringLiteral("FileTransfer"), QStringLiteral("Network")},
        {QStringLiteral("HamRadio"), QStringLiteral("Network")},
        {QStringLiteral("News"), QStringLiteral("Network")},
        {QStringLiteral("P2P"), QStringLiteral("Network")},
        {QStringLiteral("RemoteAccess"), QStringLiteral("Network")},
        {QStringLiteral("Telephony"), QStringLiteral("Network")},
        {QStringLiteral("TelephonyTools"), QStringLiteral("Network")},
        {QStringLiteral("VideoConference"), QStringLiteral("Network")},
        {QStringLiteral("WebBrowser"), QStringLiteral("Network")},
        {QStringLiteral("Office"), QStringLiteral("Office")},
        {QStringLiteral("Calendar"), QStringLiteral("Office")},
        {QStringLiteral("ContactManagement"), QStringLiteral("Office")},
        {QStringLiteral("Database"), QStringLiteral("Office")},
        {QStringLiteral("Dictionary"), QStringLiteral("Office")},
        {QStringLiteral("Chart"), QStringLiteral("Office")},
        {QStringLiteral("Email"), QStringLiteral("Office")},
        {QStringLiteral("Finance"), QStringLiteral("Office")},
        {QStringLiteral("FlowChart"), QStringLiteral("Office")},
        {QStringLiteral("PDA"), QStringLiteral("Office")},
        {QStringLiteral("Presentation"), QStringLiteral("Office")},
        {QStringLiteral("ProjectManagement"), QStringLiteral("Office")},
        {QStringLiteral("Spreadsheet"), QStringLiteral("Office")},
        {QStringLiteral("WordProcessor"), QStringLiteral("Office")},
        {QStringLiteral("Science"), QStringLiteral("Science")},
        {QStringLiteral("Astronomy"), QStringLiteral("Science")},
        {QStringLiteral("Biology"), QStringLiteral("Science")},
        {QStringLiteral("Chemistry"), QStringLiteral("Science")},
        {QStringLiteral("ComputerScience"), QStringLiteral("Science")},
        {QStringLiteral("DataVisualization"), QStringLiteral("Science")},
        {QStringLiteral("Economy"), QStringLiteral("Science")},
        {QStringLiteral("Electricity"), QStringLiteral("Science")},
        {QStringLiteral("Geography"), QStringLiteral("Science")},
        {QStringLiteral("Geology"), QStringLiteral("Science")},
        {QStringLiteral("Geoscience"), QStringLiteral("Science")},
        {QStringLiteral("History"), QStringLiteral("Science")},
        {QStringLiteral("Humanities"), QStringLiteral("Science")},
        {QStringLiteral("ImageProcessing"), QStringLiteral("Science")},
        {QStringLiteral("Languages"), QStringLiteral("Science")},
        {QStringLiteral("Literature"), QStringLiteral("Science")},
        {QStringLiteral("Maps"), QStringLiteral("Science")},
        {QStringLiteral("Math"), QStringLiteral("Science")},
        {QStringLiteral("NumericalAnalysis"), QStringLiteral("Science")},
        {QStringLiteral("MedicalSoftware"), QStringLiteral("Science")},
        {QStringLiteral("Physics"), QStringLiteral("Science")},
        {QStringLiteral("Robotics"), QStringLiteral("Science")},
        {QStringLiteral("ArtificialIntelligence"), QStringLiteral("Science")},
        {QStringLiteral("Electronics"), QStringLiteral("Science")},
        {QStringLiteral("Engineering"), QStringLiteral("Science")},
        {QStringLiteral("Settings"), QStringLiteral("Settings")},
        {QStringLiteral("Accessibility"), QStringLiteral("Settings")},
        {QStringLiteral("DesktopSettings"), QStringLiteral("Settings")},
        {QStringLiteral("HardwareSettings"), QStringLiteral("Settings")},
        {QStringLiteral("PackageManager"), QStringLiteral("Settings")},
        {QStringLiteral("Printing"), QStringLiteral("Settings")},
        {QStringLiteral("Security"), QStringLiteral("Settings")},
        {QStringLiteral("System"), QStringLiteral("System")},
        {QStringLiteral("Emulator"), QStringLiteral("System")},
        {QStringLiteral("FileTools"), QStringLiteral("System")},
        {QStringLiteral("FileManager"), QStringLiteral("System")},
        {QStringLiteral("TerminalEmulator"), QStringLiteral("System")},
        {QStringLiteral("Filesystem"), QStringLiteral("System")},
        {QStringLiteral("Monitor"), QStringLiteral("System")},
        {QStringLiteral("Utility"), QStringLiteral("Utility")},
        {QStringLiteral("Archiving"), QStringLiteral("Utility")},
        {QStringLiteral("Calculator"), QStringLiteral("Utility")},
        {QStringLiteral("Clock"), QStringLiteral("Utility")},
        {QStringLiteral("Compression"), QStringLiteral("Utility")},
        {QStringLiteral("TextEditor"), QStringLiteral("Utility")},
        {QStringLiteral("TextTools"), QStringLiteral("Utility")},
        {QStringLiteral("Core"), QStringLiteral("Utility")},
    };
    return alias.value(token);
}
QString categoryIconName(const QString &key)
{
    if (key == QLatin1String("AudioVideo")) return QStringLiteral("applications-multimedia");
    if (key == QLatin1String("Development")) return QStringLiteral("applications-development");
    if (key == QLatin1String("Education")) return QStringLiteral("applications-education");
    if (key == QLatin1String("Game")) return QStringLiteral("applications-games");
    if (key == QLatin1String("Graphics")) return QStringLiteral("applications-graphics");
    if (key == QLatin1String("Network")) return QStringLiteral("applications-internet");
    if (key == QLatin1String("Office")) return QStringLiteral("applications-office");
    if (key == QLatin1String("Science")) return QStringLiteral("applications-science");
    if (key == QLatin1String("Settings")) return QStringLiteral("preferences-system");
    if (key == QLatin1String("System")) return QStringLiteral("applications-system");
    if (key == QLatin1String("Utility")) return QStringLiteral("applications-utilities");
    return QStringLiteral("folder");
}
QIcon appIcon(GAppInfo *info)
{
    GIcon *icon = g_app_info_get_icon(info);
    if (!icon) return {};
    if (G_IS_THEMED_ICON(icon)) {
        const auto names = g_themed_icon_get_names(G_THEMED_ICON(icon));
        for (int i = 0; names && names[i]; ++i) {
            const QIcon theme = QIcon::fromTheme(QString::fromUtf8(names[i]));
            if (!theme.isNull()) return theme;
        }
    } else if (G_IS_FILE_ICON(icon)) {
        char *file = g_file_get_path(g_file_icon_get_file(G_FILE_ICON(icon)));
        QIcon result;
        if (file) result = QIcon(QString::fromUtf8(file));
        g_free(file);
        return result;
    }
    return {};
}
}
AppShortcutsController::AppShortcutsController(QObject *parent, QString directory, TestLauncher launcher)
    : QObject(parent), m_directory(directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/AppShortcuts" : directory),
      m_path(m_directory + "/shortcuts.json"), m_testLauncher(std::move(launcher))
{
    load();
    refresh();
    bindCatalogMonitor();
}
QString AppShortcutsController::normalizeName(const QString &value) {
    const auto text = value.trimmed();
    for (const auto c : text) if (c.category() == QChar::Other_Control) return {};
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    int count = 0, end = 0;
    for (int next = finder.toNextBoundary(); next >= 0 && count < 24; next = finder.toNextBoundary(), ++count) end = next;
    return text.left(end);
}
std::optional<AppShortcutsController::App> AppShortcutsController::inspect(const QString &path) const {
    const QFileInfo info(path);
    if (info.suffix().compare("desktop", Qt::CaseInsensitive) || !info.isFile() || !info.isReadable() || info.size() > 1024 * 1024) return {};
    auto *desktop = g_desktop_app_info_new_from_filename(QFile::encodeName(info.canonicalFilePath()).constData());
    if (!desktop) return {};
    App app;
    app.name = QString::fromUtf8(g_app_info_get_display_name(G_APP_INFO(desktop)));
    app.executable = QString::fromUtf8(g_app_info_get_executable(G_APP_INFO(desktop)));

    char *type = g_desktop_app_info_get_string(desktop, "Type");
    const bool valid = type && QByteArray(type) == "Application" && !g_desktop_app_info_get_boolean(desktop, "Hidden") && !app.name.isEmpty() && !app.executable.isEmpty();
    g_free(type);
    if (GIcon *icon = g_app_info_get_icon(G_APP_INFO(desktop))) {
        QIcon qtIcon;
        if (G_IS_THEMED_ICON(icon)) {
            const auto names = g_themed_icon_get_names(G_THEMED_ICON(icon));
            for (int i = 0; names && names[i] && qtIcon.isNull(); ++i) qtIcon = QIcon::fromTheme(QString::fromUtf8(names[i]));
        } else if (G_IS_FILE_ICON(icon)) {
            char *file = g_file_get_path(g_file_icon_get_file(G_FILE_ICON(icon))); if (file) qtIcon = QIcon(QString::fromUtf8(file)); g_free(file);
        }
        app.icon = qtIcon.pixmap(96, 96).toImage(); app.color = averageColor(app.icon);
    }
    g_object_unref(desktop);
    return valid ? std::optional(app) : std::nullopt;
}
void AppShortcutsController::load() {
    QFile file(m_path); if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly) || file.size() > maximumBytes) { m_readable = false; m_error = tr("无法读取应用快捷方式，原始数据已保留。"); return; }
    const auto bytes = file.readAll(); const auto document = QJsonDocument::fromJson(bytes);
    const auto object = document.object();
    if (document.isNull() || object["version"].toInt() != 1 || !object["items"].isArray()) { m_readable = false; m_error = tr("无法读取应用快捷方式，原始数据已保留。"); return; }
    QSet<QString> ids, paths;
    for (const auto &value : object["items"].toArray()) {
        const auto row = value.toObject();
        Entry entry{row["id"].toString(), row["name"].toString(), row["originalName"].toString(), row["path"].toString(), row["executable"].toString(), row["iconPreset"].toString(), qint64(row["created"].toDouble())};
        if (QUuid(entry.id).isNull() || ids.contains(entry.id) || paths.contains(entry.path) || entry.name.isEmpty() || entry.name != normalizeName(entry.name)
            || !QFileInfo(entry.path).isAbsolute() || !entry.path.endsWith(".desktop", Qt::CaseInsensitive) || entry.executable.isEmpty() || !presetIDs.contains(entry.iconPreset)) {
            m_entries.clear(); m_readable = false; m_error = tr("无法读取应用快捷方式，原始数据已保留。"); return;
        }
        ids.insert(entry.id); paths.insert(entry.path); m_entries.append(entry);
    }
    m_saved = bytes;
}
bool AppShortcutsController::fail(const QString &message) { m_error = message; emit changed(); return false; }
bool AppShortcutsController::commit(const QList<Entry> &next) {
    if (!m_readable) return false;
    QFile current(m_path);
    if (current.exists() && (!current.open(QIODevice::ReadOnly) || current.size() > maximumBytes)) return fail(tr("无法读取应用快捷方式，原始数据已保留。"));
    if ((current.isOpen() ? current.readAll() : QByteArray()) != m_saved) return fail(tr("应用快捷方式已在外部更改，请重启 HyprEndfield 后再编辑。"));
    QJsonArray rows;
    for (const auto &entry : next) rows.append(QJsonObject{{"id", entry.id}, {"name", entry.name}, {"originalName", entry.originalName}, {"path", entry.path},
        {"executable", entry.executable}, {"iconPreset", entry.iconPreset}, {"created", entry.created}});
    const auto bytes = QJsonDocument(QJsonObject{{"version", 1}, {"items", rows}}).toJson(QJsonDocument::Compact);
    if (bytes.size() > maximumBytes || !QDir().mkpath(m_directory)) return fail(tr("无法保存应用快捷方式。"));
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) return fail(tr("无法保存应用快捷方式。"));
    m_entries = next; m_saved = bytes; m_error.clear(); emit changed(); return true;
}
QVariantMap AppShortcutsController::state() const {
    QVariantList entries;
    for (const auto &entry : m_entries) entries.append(QVariantMap{{"id", entry.id}, {"name", entry.name}, {"iconPreset", entry.iconPreset},
        {"available", m_apps.contains(entry.id)}, {"icon", "image://shortcut-icons/" + entry.id + '/' + entry.iconPreset}});
    return {{"items", entries}, {"count", entries.size()}, {"error", m_error}, {"available", m_readable}, {"launching", m_launching}};
}
QVariantList AppShortcutsController::presets() const {
    QVariantList result;
    for (const auto &value : presetData()) { const auto entry = value.toObject(); result.append(QVariantMap{{"id", entry["id"].toString()}, {"title", tr(qPrintable(entry["simplifiedChinese"].toString()))}}); }
    return result;
}
void AppShortcutsController::refresh() {
    m_catalogReady = false;
    m_apps.clear();
    for (const auto &entry : m_entries) if (const auto app = inspect(entry.path); app && app->executable == entry.executable) m_apps.insert(entry.id, *app);
    emit changed();
}

QHash<QString, int> AppShortcutsController::favoriteOrder() const
{
    // 桥接 Kickoff 的“常用应用程序”标签：plasma 收藏列表存在 ResourceLink 里。
    QHash<QString, int> order;
    const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/kactivitymanagerd/resources/database");
    if (!QFile::exists(path)) return order;
    const QString connection = QStringLiteral("hyprendfield-favorites");
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(path);
        database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (database.open()) {
            QSqlQuery query(database);
            if (query.exec(QStringLiteral(
                    "SELECT targettedResource FROM ResourceLink "
                    "WHERE usedActivity=':global' AND initiatingAgent='org.kde.plasma.favorites.applications' "
                    "ORDER BY rowid"))) {
                int position = 0;
                while (query.next()) {
                    const QString uri = query.value(0).toString();
                    if (!uri.startsWith(QStringLiteral("applications:"))) continue;
                    const QString id = uri.section(QLatin1Char(':'), 1);
                    if (!id.isEmpty() && !order.contains(id)) order.insert(id, position++);
                }
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connection);
    return order;
}

void AppShortcutsController::updateFavorites()
{
    if (!m_catalogReady) return;
    const auto order = favoriteOrder();
    auto apps = m_catalog.value(QStringLiteral("apps")).toList();
    bool changed = false;
    for (auto &value : apps) {
        auto row = value.toMap();
        const int favorite = order.value(row.value(QStringLiteral("storageId")).toString(), -1);
        if (row.value(QStringLiteral("favorite")).toInt() != favorite) {
            row[QStringLiteral("favorite")] = favorite;
            value = row;
            changed = true;
        }
    }
    if (changed) m_catalog[QStringLiteral("apps")] = apps;
}

QVariantMap AppShortcutsController::applicationCatalog()
{
    if (m_catalogReady) {
        updateFavorites();
        return m_catalog;
    }
    if (m_catalogSource == QStringLiteral("kde") && kdeCatalogAvailable()) {
#ifdef HAVE_KSERVICE
        if (auto *sycoca = KSycoca::self()) sycoca->ensureCacheValid();
#endif
        m_catalog = kdeCatalog();
    } else {
        m_catalog = desktopCatalog();
    }
    m_catalogReady = true;
    return m_catalog;
}
QVariantMap AppShortcutsController::desktopCatalog()
{
    QVariantList apps;
    QVariantList categories;
    QHash<QString, int> index;
    QHash<QString, QStringList> membership;
    const auto favorites = favoriteOrder();
    const QStringList order{
        QStringLiteral("AudioVideo"), QStringLiteral("Development"), QStringLiteral("Education"),
        QStringLiteral("Game"), QStringLiteral("Graphics"), QStringLiteral("Network"),
        QStringLiteral("Office"), QStringLiteral("Science"), QStringLiteral("Settings"),
        QStringLiteral("System"), QStringLiteral("Utility")};
    GList *list = g_app_info_get_all();
    for (GList *item = list; item; item = item->next) {
        auto *info = static_cast<GAppInfo *>(item->data);
        if (!G_IS_DESKTOP_APP_INFO(info) || !g_app_info_should_show(info)) continue;
        auto *desktop = G_DESKTOP_APP_INFO(info);
        const char *filename = g_desktop_app_info_get_filename(desktop);
        const char *id = g_app_info_get_id(info);
        const char *display = g_app_info_get_display_name(info);
        if (!filename || !*filename || !id || !*id || !display || !*display) continue;
        const QString storageId = QString::fromUtf8(id);
        const QString path = QString::fromUtf8(filename);
        QStringList keys;
        if (const char *raw = g_desktop_app_info_get_categories(desktop)) {
            for (const auto &token : QString::fromUtf8(raw).split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
                const QString key = mainCategoryKey(token.trimmed());
                if (!key.isEmpty() && !keys.contains(key)) keys.append(key);
            }
        }
        if (!index.contains(storageId)) {
            index.insert(storageId, apps.size());
            const char *executable = g_app_info_get_executable(info);
            apps.append(QVariantMap{{"path", path}, {"name", QString::fromUtf8(display)},
                {"executable", executable ? QString::fromUtf8(executable) : QString()},
                {"icon", encodedIcon(appIcon(info))}, {"categories", keys}, {"storageId", storageId},
                {"favorite", favorites.value(storageId, -1)}});
            membership.insert(storageId, keys);
        } else if (membership.value(storageId) != keys) {
            auto row = apps.at(index.value(storageId)).toMap();
            auto merged = row.value(QStringLiteral("categories")).toStringList();
            for (const auto &key : keys) if (!merged.contains(key)) merged.append(key);
            row[QStringLiteral("categories")] = merged;
            apps[index.value(storageId)] = row;
            membership.insert(storageId, merged);
        }
    }
    g_list_free_full(list, g_object_unref);
    std::sort(apps.begin(), apps.end(), [](const QVariant &left, const QVariant &right) {
        return QString::localeAwareCompare(left.toMap().value(QStringLiteral("name")).toString(),
            right.toMap().value(QStringLiteral("name")).toString()) < 0;
    });
    for (const auto &key : order) {
        const bool used = std::any_of(apps.cbegin(), apps.cend(), [&](const QVariant &value) {
            return value.toMap().value(QStringLiteral("categories")).toStringList().contains(key);
        });
        if (!used) continue;
        QString icon = iconData(categoryIconName(key), 64, QColor(QStringLiteral("#2E2E2E")));
        if (icon.isEmpty()) icon = iconData(QStringLiteral("folder"), 64, QColor(QStringLiteral("#2E2E2E")));
        categories.append(QVariantMap{{"key", key}, {"title", categoryTitle(key)}, {"icon", icon}});
    }
    return {{"categories", categories}, {"apps", apps}};
}
QVariantMap AppShortcutsController::kdeCatalog()
{
#ifdef HAVE_KSERVICE
    // 仅在用户选择 KDE 菜单时使用。类别、名称与归组和 Kickoff 一致。
    QVariantList apps;
    QVariantList categories;
    QHash<QString, int> index;
    QHash<QString, int> groupSeen;
    const auto favorites = favoriteOrder();
    const auto appendService = [&](const KSycocaEntry::Ptr &entry, const QString &groupKey) {
        const auto *service = static_cast<const KService *>(entry.data());
        if (!service || service->noDisplay() || service->entryPath().isEmpty()) return;
        const QString id = service->storageId();
        if (id.isEmpty()) return;
        if (!index.contains(id)) {
            index.insert(id, apps.size());
            apps.append(QVariantMap{{"path", service->entryPath()}, {"name", service->name()},
                {"executable", service->exec()}, {"icon", iconData(service->icon())},
                {"categories", QStringList{groupKey}}, {"storageId", id},
                {"favorite", favorites.value(id, -1)}});
        } else {
            auto row = apps.at(index.value(id)).toMap();
            auto keys = row.value(QStringLiteral("categories")).toStringList();
            if (!keys.contains(groupKey)) keys.append(groupKey);
            row[QStringLiteral("categories")] = keys;
            apps[index.value(id)] = row;
        }
    };
    const auto gather = [](KServiceGroup *node, KSycocaEntry::List &out, auto &&self) -> void {
        for (const auto &entry : node->entries(true, true)) {
            if (entry->isType(KST_KServiceGroup)) self(static_cast<KServiceGroup *>(entry.data()), out, self);
            else if (entry->isType(KST_KService)) out.append(entry);
        }
    };
    if (const auto root = KServiceGroup::root()) {
        for (const auto &entry : root->entries(true, true)) {
            if (!entry->isType(KST_KServiceGroup)) continue;
            auto *group = static_cast<KServiceGroup *>(entry.data());
            if (!group || group->noDisplay()) continue;
            const QString key = group->relPath();
            if (key.isEmpty() || groupSeen.contains(key)) continue;
            KSycocaEntry::List services;
            gather(group, services, gather);
            services.removeIf([](const KSycocaEntry::Ptr &entry) {
                const auto *service = static_cast<const KService *>(entry.data());
                return !service || service->noDisplay() || service->entryPath().isEmpty();
            });
            if (services.isEmpty()) continue;
            groupSeen.insert(key, categories.size());
            categories.append(QVariantMap{{"key", key}, {"title", group->caption()}, {"icon", iconData(group->icon().isEmpty() ? QStringLiteral("folder") : group->icon(), 64, QColor(QStringLiteral("#2E2E2E")))}});
            for (const auto &service : services) appendService(service, key);
        }
    }
    return {{"categories", categories}, {"apps", apps}};
#else
    return {};
#endif
}
QString AppShortcutsController::categoryTitle(const QString &key) const
{
    if (key == QLatin1String("AudioVideo")) return tr("影音");
    if (key == QLatin1String("Development")) return tr("开发");
    if (key == QLatin1String("Education")) return tr("教育");
    if (key == QLatin1String("Game")) return tr("游戏");
    if (key == QLatin1String("Graphics")) return tr("图形");
    if (key == QLatin1String("Network")) return tr("网络");
    if (key == QLatin1String("Office")) return tr("办公");
    if (key == QLatin1String("Science")) return tr("科学");
    if (key == QLatin1String("Settings")) return tr("设置");
    if (key == QLatin1String("System")) return tr("系统");
    if (key == QLatin1String("Utility")) return tr("工具");
    return key;
}
QString AppShortcutsController::catalogAppName(const QString &path) const
{
    for (const auto &value : m_catalog.value(QStringLiteral("apps")).toList()) {
        const auto row = value.toMap();
        if (row.value(QStringLiteral("path")).toString() == path) return row.value(QStringLiteral("name")).toString();
    }
    return {};
}
bool AppShortcutsController::kdeCatalogAvailable() const
{
#ifdef HAVE_KSERVICE
    return true;
#else
    return false;
#endif
}
QVariantList AppShortcutsController::catalogSources() const
{
    QVariantList result{QVariantMap{{"key", QStringLiteral("desktop")}, {"title", tr("桌面文件分类")}}};
    if (kdeCatalogAvailable())
        result.append(QVariantMap{{"key", QStringLiteral("kde")}, {"title", tr("KDE 应用菜单")}});
    return result;
}
void AppShortcutsController::invalidateCatalog()
{
    m_catalogReady = false;
    ++m_catalogRevision;
    emit changed();
}
void AppShortcutsController::bindCatalogMonitor()
{
    if (m_sycocaConnection) {
        QObject::disconnect(m_sycocaConnection);
        m_sycocaConnection = {};
    }
    if (m_appWatcher) m_appWatcher->removePaths(m_appWatcher->directories());
#ifdef HAVE_KSERVICE
    if (m_catalogSource == QStringLiteral("kde")) {
        if (auto *sycoca = KSycoca::self())
            m_sycocaConnection = connect(sycoca, &KSycoca::databaseChanged, this, [this] { invalidateCatalog(); });
        return;
    }
#endif
    if (!m_appRefresh) {
        m_appRefresh = new QTimer(this);
        m_appRefresh->setSingleShot(true);
        m_appRefresh->setInterval(400);
        connect(m_appRefresh, &QTimer::timeout, this, [this] { invalidateCatalog(); });
    }
    if (!m_appWatcher) {
        m_appWatcher = new QFileSystemWatcher(this);
        connect(m_appWatcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_appRefresh->start(); });
    }
    QStringList dirs;
    const auto add = [&](const QString &dir) {
        if (dir.isEmpty() || dirs.contains(dir) || !QDir(dir).exists()) return;
        dirs.append(dir);
    };
    for (const auto &root : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
        add(root + QStringLiteral("/applications"));
    add(QStringLiteral("/usr/share/applications"));
    add(QStringLiteral("/usr/local/share/applications"));
    add(QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/share/applications"));
    add(QStringLiteral("/var/lib/flatpak/exports/share/applications"));
    add(QStringLiteral("/var/lib/snapd/desktop/applications"));
    for (const auto &root : qEnvironmentVariable("XDG_DATA_DIRS").split(QLatin1Char(':'), Qt::SkipEmptyParts))
        add(QDir(root).filePath(QStringLiteral("applications")));
    if (!dirs.isEmpty()) m_appWatcher->addPaths(dirs);
}
void AppShortcutsController::setCatalogSource(const QString &source)
{
    const QString requested = source == QStringLiteral("kde") ? QStringLiteral("kde") : QStringLiteral("desktop");
    const QString next = requested == QStringLiteral("kde") && kdeCatalogAvailable() ? QStringLiteral("kde") : QStringLiteral("desktop");
    const bool changed = next != m_catalogSource;
    m_catalogSource = next;
    if (changed) {
        invalidateCatalog();
        bindCatalogMonitor();
        emit catalogSourceChanged();
        clampHomeCategory();
    } else if (next != requested) {
        emit catalogSourceChanged();
    }
}
void AppShortcutsController::setHomeCategory(const QString &key)
{
    const QString next = key.trimmed().isEmpty() ? QStringLiteral("all") : key.trimmed();
    if (next == m_homeCategory) return;
    m_homeCategory = next;
    emit homeCategoryChanged();
}
void AppShortcutsController::clampHomeCategory()
{
    QString next = QStringLiteral("all");
    for (const auto &value : launchCategories())
        if (value.toMap().value(QStringLiteral("key")).toString() == m_homeCategory) { next = m_homeCategory; break; }
    if (next == m_homeCategory) return;
    m_homeCategory = next;
    emit homeCategoryChanged();
}
void AppShortcutsController::applyHomeCategory()
{
    clampHomeCategory();
    if (m_homeCategory == m_launchCategory) return;
    m_launchCategory = m_homeCategory;
    emit launchCategoryChanged();
}
QVariantList AppShortcutsController::launchCategories()
{
    const auto catalog = applicationCatalog();
    QVariantList result{
        QVariantMap{{"key", QStringLiteral("frequent")}, {"title", tr("常用应用程序")}, {"icon", iconData(QStringLiteral("view-history"), 64, QColor(QStringLiteral("#2E2E2E")))}},
        QVariantMap{{"key", QStringLiteral("all")}, {"title", tr("全部应用程序")}, {"icon", iconData(QStringLiteral("applications-all"), 64, QColor(QStringLiteral("#2E2E2E")))}}
    };
    for (const auto &value : catalog.value(QStringLiteral("categories")).toList()) result.append(value);
    return result;
}

void AppShortcutsController::setLaunchCategory(const QString &key)
{
    if (key.isEmpty() || key == m_launchCategory) return;
    bool valid = key == QStringLiteral("all") || key == QStringLiteral("frequent");
    if (!valid) {
        for (const auto &value : launchCategories())
            if (value.toMap().value(QStringLiteral("key")).toString() == key) { valid = true; break; }
    }
    if (!valid) return;
    m_launchCategory = key;
    emit launchCategoryChanged();
}

bool AppShortcutsController::requestOpenPath(const QString &path)
{
    if (m_launching || path.isEmpty()) return false;
    m_pending.clear();
    m_pendingPath = path;
    emit launchRequested();
    return true;
}

void AppShortcutsController::launchFile(const QString &path, const QString &name)
{
    const auto done = [this, path, name](bool success, const QString &error) {
        m_launching = false;
        if (success) { m_error.clear(); emit changed(); emit opened(name); }
        else { fail(tr("无法打开应用：") + error.left(500)); emit launchFailed(); }
    };
    if (m_testLauncher) { const auto error = m_testLauncher(path); done(error.isEmpty(), error); return; }
    auto *process = new QProcess(this); auto *timeout = new QTimer(process); timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, process, &QProcess::kill);
    connect(process, &QProcess::errorOccurred, this, [process, done](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) { done(false, process->errorString()); process->deleteLater(); }
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [process, timeout, done](int code, QProcess::ExitStatus status) {
        timeout->stop(); done(code == 0 && status == QProcess::NormalExit, QString::fromUtf8(process->readAllStandardError()).trimmed()); process->deleteLater();
    });
    m_launching = true; emit changed(); process->start("gio", {"launch", path}); timeout->start(15000);
}

bool AppShortcutsController::add(const QUrl &url) {
    if (!url.isLocalFile()) return fail(tr("请选择可用的 Linux 应用快捷方式（.desktop）。"));
    const auto path = QFileInfo(url.toLocalFile()).canonicalFilePath(); const auto app = inspect(path);
    if (!app) return fail(tr("请选择可用的 Linux 应用快捷方式（.desktop）。"));
    for (const auto &entry : m_entries) if (entry.path == path) return fail(tr("此应用已添加，请编辑现有快捷方式。"));
    Entry entry{QUuid::createUuid().toString(QUuid::WithoutBraces), normalizeName(app->name), app->name, path, app->executable, "original", QDateTime::currentMSecsSinceEpoch()};
    if (entry.name.isEmpty()) return fail(tr("请输入有效的快捷方式名称。"));
    auto next = m_entries; next.append(entry);
    if (!commit(next)) return false;
    m_apps.insert(entry.id, *app); emit changed(); emit added(entry.id); return true;
}
bool AppShortcutsController::rename(const QString &id, const QString &name) {
    const auto normalized = normalizeName(name); if (normalized.isEmpty()) return fail(tr("请输入有效的快捷方式名称。"));
    auto next = m_entries;
    for (auto &entry : next) if (entry.id == id) { if (entry.name == normalized) return true; entry.name = normalized; return commit(next); }
    return false;
}
bool AppShortcutsController::setIcon(const QString &id, const QString &preset) {
    if (!presetIDs.contains(preset)) return false;
    auto next = m_entries;
    for (auto &entry : next) if (entry.id == id) { if (entry.iconPreset == preset) return true; entry.iconPreset = preset; return commit(next); }
    return false;
}
bool AppShortcutsController::remove(const QString &id) {
    auto next = m_entries; if (!next.removeIf([&](const Entry &entry) { return entry.id == id; })) return false;
    if (!commit(next)) return false;
    m_apps.remove(id); return true;
}
void AppShortcutsController::paste() {
    // Match the shelf's asynchronous URI-list read; never block the UI waiting for another Wayland client.
    auto *process = new QProcess(this); auto *timeout = new QTimer(process); timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, process, &QProcess::kill);
    connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error) { if (error == QProcess::FailedToStart) process->deleteLater(); });
    connect(process, &QProcess::readyReadStandardOutput, this, [process] { if (process->bytesAvailable() > 1024 * 1024) process->kill(); });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, process, timeout](int code, QProcess::ExitStatus status) {
        timeout->stop(); const auto bytes = process->readAllStandardOutput(); process->deleteLater();
        if (code != 0 || status != QProcess::NormalExit || bytes.size() > 1024 * 1024) return;
        QList<QUrl> urls;
        for (const auto &line : bytes.split('\n')) if (!line.trimmed().isEmpty() && !line.startsWith('#')) urls.append(QUrl::fromEncoded(line.trimmed()));
        if (urls.size() == 1) add(urls.first());
    });
    process->start("wl-paste", {"--type", "text/uri-list", "--no-newline"}); timeout->start(3000);
}
bool AppShortcutsController::requestOpen(const QString &id) {
    if (m_launching) return false;
    for (const auto &entry : m_entries) if (entry.id == id) { m_pending = id; emit launchRequested(); return true; }
    return false;
}
void AppShortcutsController::launchPending()
{
    if (m_launching) return;
    if (!m_pendingPath.isEmpty()) {
        const auto path = std::exchange(m_pendingPath, {});
        const QFileInfo info(path);
        if (!info.isFile()) { fail(tr("应用已移除，请刷新列表。")); emit launchFailed(); return; }
        if (!m_catalogReady) applicationCatalog();
        const QString found = catalogAppName(path);
        launchFile(path, found.isEmpty() ? info.completeBaseName() : found);
        return;
    }
    if (m_pending.isEmpty()) return;
    const auto id = std::exchange(m_pending, {});
    for (const auto &entry : m_entries) if (entry.id == id) {
        const auto app = inspect(entry.path);
        if (!app || app->executable != entry.executable) { fail(tr("应用已移动、移除或更改，请重新选择应用。")); emit launchFailed(); return; }
        launchFile(entry.path, entry.name);
        return;
    }
}

QImage AppShortcutsController::artwork(const QString &id, const QString &preset, QColor accent, QSize size) const {
    if (size.isEmpty()) size = {80, 80}; size = size.boundedTo({512, 512});
    QImage result(size, QImage::Format_ARGB32_Premultiplied); result.fill(Qt::transparent);
    QPainter painter(&result); painter.setRenderHint(QPainter::SmoothPixmapTransform); painter.scale(size.width() / 40., size.height() / 40.);
    const auto app = m_apps.value(id); const bool original = preset == "original";
    painter.fillRect(QRectF(0, 0, 40, 40), original ? app.color : accent);
    if (original && !app.icon.isNull()) {
        const auto target = QSizeF(app.icon.size()).scaled({30, 30}, Qt::KeepAspectRatio);
        painter.drawImage(QRectF((40 - target.width()) / 2, (40 - target.height()) / 2, target.width(), target.height()), app.icon);
    } else for (const auto &value : presetData()) {
        const auto data = value.toObject(); if (data["id"].toString() != preset) continue;
        const auto frame = data["frame"].toArray();
        QImage icon = QImage(":/assets/app-shortcut/" + data["file"].toString()).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QPainter tint(&icon); tint.setCompositionMode(QPainter::CompositionMode_SourceIn); tint.fillRect(icon.rect(), original ? QColor("#EDEDED") : QColor("#111111")); tint.end();
        painter.drawImage(QRectF(frame[0].toDouble(), frame[1].toDouble(), frame[2].toDouble(), frame[3].toDouble()), icon); break;
    }
    painter.setPen(QColor(0, 0, 0, 64)); painter.drawRect(QRectF(.5, .5, 39, 39));
    return result;
}
