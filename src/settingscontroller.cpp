#include "settingscontroller.hpp"

#include <QCoreApplication>
#include <LayerShellQt/Window>
#include <QQuickWindow>
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QNetworkReply>
#include <QSaveFile>
#include <QScreen>
#include <QStandardPaths>
#include <QStyleHints>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace {
class HudTranslator final : public QTranslator {
public:
    int language = 2;
    struct Entry { const char *english, *chinese, *traditional, *japanese, *korean; };
    static constexpr Entry entries[] = {
#include "localization_catalog.inc"
#include "localization_status.inc"
        {"Quick terminal", "快捷终端", "快捷終端", "クイックターミナル", "빠른 터미널"},
        {"Focus mode", "专注模式", "專注模式", "集中モード", "집중 모드"},
        {"01  Digital", "01  数字", "01  數字", "01  デジタル", "01  디지털"},
        {"02  Split", "02  分栏", "02  分欄", "02  分割", "02  분할"},
        {"03  Dial", "03  表盘", "03  錶盤", "03  文字盤", "03  다이얼"},
        {"04  Rail", "04  轨道", "04  軌道", "04  レール", "04  레일"},
        {"05  Stacked", "05  堆叠", "05  堆疊", "05  スタック", "05  스택"},
        {"Per-HUD adjustment unavailable; uses compositor settings", "暂不支持单独调整，沿用合成器设置", "暫不支援單獨調整，沿用合成器設定", "個別調整は未対応。コンポジタの設定を使用", "개별 조정 미지원 · 컴포지터 설정 사용"},
        {"Monitor disk and memory", "监测磁盘和内存状态", "監測磁碟和記憶體狀態", "ディスクとメモリの状態を監視", "디스크와 메모리 상태 감시"},
        {"Redact notes, clipboard content and file names", "隐藏便笺、剪贴板和文件名称等隐私内容", "隱藏便箋、剪貼簿和檔案名稱等隱私內容", "メモ、クリップボード、ファイル名などを非表示", "메모, 클립보드 내용 및 파일 이름 숨기기"},
        {"Keep this layout?", "保留此界面布局？", "保留此介面配置？", "この配置を保存しますか？", "이 화면 배치를 유지할까요?"},
        {"Scale and position revert when the countdown ends.", "倒计时结束后恢复原来的缩放与位置。", "倒數結束後恢復原來的縮放與位置。", "カウントダウン終了後、元の拡大率と位置に戻ります。", "카운트다운이 끝나면 원래 크기와 위치로 복원됩니다."},
        {"Original UI, animation and assets", "原始界面、动画与资源", "原始介面、動畫與資源", "元の UI・アニメーション・素材", "원본 UI, 애니메이션 및 리소스"},
        {"Linux UI and terminal components", "Linux 图形界面与终端组件", "Linux 圖形介面與終端元件", "Linux の UI とターミナル", "Linux UI 및 터미널 구성 요소"},
        {"Check failed", "检查失败", "檢查失敗", "確認に失敗", "확인 실패"},
        {"Different revision found", "发现不同提交", "發現不同提交", "異なるリビジョンを検出", "다른 리비전 발견"},
        {"Latest remote revision", "远端最新提交", "遠端最新提交", "リモートの最新リビジョン", "원격 최신 리비전"},
        {"Source build: update and rebuild manually", "源码构建，请手动更新并重新编译", "原始碼建置，請手動更新並重新編譯", "ソースビルド：手動で更新して再ビルド", "소스 빌드: 수동 업데이트 후 다시 빌드"},
        {"Unable to save preferences", "无法保存设置", "無法儲存設定", "設定を保存できません", "설정을 저장할 수 없습니다"},
        {"Layout restored", "已恢复界面布局", "已恢復介面配置", "配置を復元しました", "화면 배치를 복원했습니다"},
        {"Preferences restored", "已恢复默认设置", "已恢復預設設定", "初期設定を復元しました", "기본 설정을 복원했습니다"},
    };
    bool isEmpty() const override { return false; }
    QString translate(const char *, const char *source, const char *, int) const override {
        if (language == 2) return {};
        const QByteArray key(source);
        for (const auto &e : entries) {
            if (key != e.chinese && key != e.english) continue;
            const char *text = language == 3 ? e.traditional : language == 4 ? e.japanese
                              : language == 5 ? e.korean : e.english;
            return QString::fromUtf8(text);
        }
        return {};
    }
};
QString preferenceFile(const QString &path) { return path.isEmpty() ? QSettings().fileName() : path; }
QString quotedExec(QString text) {
    text.replace('\\', "\\\\").replace('"', "\\\"").replace('`', "\\`").replace('$', "\\$").replace('%', "%%");
    return '"' + text + '"';
}
const QStringList accents{"#FAD41F", "#6EDFE8", "#A8E58B", "#C9A2FF", "#FF8F78"};
}

QStringList SettingsController::defaultModules() {
    return {"notes", "fileShelf", "clipboard", "archive", "nowPlaying", "volume", "reader", "workMode", "calendar", "projection", "mediaAssembly", "minigame", "eventLog", "account"};
}

SettingsController::SettingsController(QObject *parent, QString preferences, QString autostart, QString data)
    : QQmlPropertyMap(this, parent), m_store(preferenceFile(preferences), QSettings::IniFormat),
      m_autostart(autostart.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart/hyprendfield.desktop" : autostart),
      m_data(data.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) : data)
{
    m_defaults = {{"uiScale", 1.0}, {"hudOffsetX", 0.0}, {"hudOffsetY", 0.0},
        {"parallaxIntensity", 1.0}, {"perspectiveIntensity", 1.0}, {"backgroundBrightness", .37}, {"blurAmount", .75},
        {"themeIndex", 0}, {"clock24Hour", true}, {"clockStyleIndex", 0}, {"centerLogoIndex", 0}, {"accentIndex", 0},
        {"customAccent", "#FAD41F"}, {"customLogo", ""}, {"applicationIcon", "endfield"},
        {"ambientAnimation", true}, {"reduceMotion", false}, {"lowPowerVisualMode", false},
        {"recordingMode", false}, {"dailyBootLog", true}, {"facilityAlerts", true},
        {"languageIndex", 0}, {"launchAtLogin", false}, {"closeOnFocusLost", false}, {"displayChoice", "pointer"}, {"soundVolume", 50},
        {"moduleOrder", defaultModules()}, {"hiddenModules", QStringList{}},
        {"batteryMethod", 0}, {"batteryDuration", 10.0}, {"batteryPlacement", 0}, {"batteryScale", 1.0},
        {"batteryX", .5}, {"batteryY", .12}, {"batteryMetric", 0}};
    QFile manifest(":/assets/settings/application-icons/manifest.json");
    if (manifest.open(QIODevice::ReadOnly)) {
        const auto entries = QJsonDocument::fromJson(manifest.readAll()).object()["icons"].toArray();
        for (const auto &v : entries) {
            const auto icon = v.toObject();
            if (icon["offered"].toBool()) m_icons.append(QVariantMap{{"id", icon["id"].toString()}, {"title", icon["title"].toString()},
                {"source", "qrc:/assets/settings/application-icons/" + icon["app"].toObject()["file"].toString()}});
        }
    }
    for (auto it = m_defaults.cbegin(); it != m_defaults.cend(); ++it) insert(it.key(), normalized(it.key(), m_store.value(storageKey(it.key()), it.value())));
    // Reflect the real startup entry, not an obsolete UI-only switch.
    QFile startup(m_autostart);
    insert("launchAtLogin", startup.open(QIODevice::ReadOnly) && startup.readAll().contains("X-HyprEndfield=true"));
    m_translator = new HudTranslator;
    m_translator->setParent(this);
    QCoreApplication::installTranslator(m_translator);
    applyLanguage();
    connect(this, &QQmlPropertyMap::valueChanged, this, &SettingsController::changed);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, &SettingsController::displaysChanged);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, &SettingsController::displaysChanged);
    connect(qGuiApp->styleHints(), &QStyleHints::colorSchemeChanged, this, &SettingsController::appearanceChanged);
    m_layoutTimer.setInterval(250);
    m_alertTimer.setSingleShot(true);
    connect(&m_alertTimer, &QTimer::timeout, this, [this] { m_alertVisible = false; emit batteryAlertChanged(); });
    connect(&m_layoutTimer, &QTimer::timeout, this, [this] {
        if (layoutRemaining() == 0) { revertLayout(); setNotice(tr("已恢复界面布局")); }
        else emit layoutChanged();
    });
}

QString SettingsController::storageKey(const QString &key) const {
    static const QStringList system{"languageIndex", "launchAtLogin", "closeOnFocusLost", "displayChoice", "moduleOrder", "hiddenModules", "soundVolume"};
    return (system.contains(key) ? "system/" : "display/") + key;
}
QVariant SettingsController::normalized(const QString &key, const QVariant &input) const {
    if (!m_defaults.contains(key)) return {};
    static const QHash<QString, QPair<double, double>> ranges{
        {"uiScale", {.2, 2}}, {"hudOffsetX", {-.5, .5}}, {"hudOffsetY", {-.5, .5}},
        {"parallaxIntensity", {0, 2}}, {"perspectiveIntensity", {0, 2}}, {"backgroundBrightness", {0, 1}}, {"blurAmount", {0, 1}},
        {"themeIndex", {0, 2}}, {"clockStyleIndex", {0, 4}}, {"centerLogoIndex", {0, 4}}, {"accentIndex", {0, 5}},
        {"languageIndex", {0, 5}}, {"soundVolume", {0, 100}}, {"batteryMethod", {0, 2}}, {"batteryDuration", {1, 60}}, {"batteryPlacement", {0, 1}},
        {"batteryScale", {.65, 1.6}}, {"batteryX", {0, 1}}, {"batteryY", {0, 1}}, {"batteryMetric", {0, 5}}};
    if (ranges.contains(key)) {
        bool ok = false; double v = input.toDouble(&ok);
        if (!ok || !std::isfinite(v)) return m_defaults.value(key);
        const auto [lo, hi] = ranges.value(key); v = std::clamp(v, lo, hi);
        return m_defaults.value(key).metaType().id() == QMetaType::Int ? QVariant(qRound(v)) : QVariant(v);
    }
    if (m_defaults.value(key).metaType().id() == QMetaType::Bool) return input.toBool();
    if (key == "moduleOrder" || key == "hiddenModules") {
        QStringList out;
        for (const auto &v : input.toStringList()) if (defaultModules().contains(v) && !out.contains(v)) out.append(v);
        if (key == "moduleOrder") for (const auto &v : defaultModules()) if (!out.contains(v)) out.append(v);
        if (key == "hiddenModules" && out.size() == defaultModules().size()) out.removeLast();
        return out;
    }
    if (key == "customAccent") {
        const QColor c(input.toString()); return c.isValid() ? c.name(QColor::HexRgb) : m_defaults.value(key);
    }
    if (key == "applicationIcon" && !m_icons.isEmpty()) {
        for (const auto &i : m_icons) if (i.toMap().value("id") == input) return input.toString();
        return "endfield";
    }
    return input.toString();
}
QVariant SettingsController::updateValue(const QString &key, const QVariant &input) {
    if (!m_defaults.contains(key)) return value(key);
    const auto next = normalized(key, input);
    if (next == value(key)) return next;
    if (key == "launchAtLogin" && !saveAutostart(next.toBool())) return value(key);
    m_store.setValue(storageKey(key), next); m_store.sync();
    if (m_store.status() != QSettings::NoError) { setNotice(tr("无法保存设置")); return value(key); }
    return next;
}
bool SettingsController::set(const QString &key, const QVariant &input) {
    if (!m_defaults.contains(key)) return false;
    const auto next = updateValue(key, input);
    if (next == value(key)) return next == normalized(key, input);
    insert(key, next); changed(key, next); return true;
}
void SettingsController::changed(const QString &key, const QVariant &v) {
    if (key == "languageIndex") applyLanguage();
    if (key.startsWith("battery")) {
        if (key == "batteryMethod") {
            m_alertTimer.stop(); m_alertVisible = v.toInt() == 1 && m_hasBattery;
            emit batteryAlertChanged();
        }
        positionBattery();
    }
    emit preferenceChanged(key, v); emit appearanceChanged();
}
void SettingsController::applyLanguage() {
    int language = value("languageIndex").toInt();
    if (!language) {
        const auto locale = QLocale::system();
        language = locale.language() == QLocale::Chinese ? (locale.script() == QLocale::TraditionalHanScript ? 3 : 2)
                 : locale.language() == QLocale::Japanese ? 4 : locale.language() == QLocale::Korean ? 5 : 1;
    }
    static_cast<HudTranslator *>(m_translator)->language = language;
    emit languageChanged(); emit displaysChanged();
}
bool SettingsController::saveAutostart(bool enabled) {
    QFile existing(m_autostart);
    if (existing.exists() && (!existing.open(QIODevice::ReadOnly) || !existing.readAll().contains("X-HyprEndfield=true"))) {
        setNotice(tr("启动项已存在，未覆盖原文件")); return false;
    }
    existing.close();
    if (!enabled) {
        if (QFile::exists(m_autostart) && !QFile::remove(m_autostart)) { setNotice(tr("无法移除登录启动项")); return false; }
        return true;
    }
    QDir().mkpath(QFileInfo(m_autostart).absolutePath());
    QSaveFile file(m_autostart);
    const QByteArray body = ("[Desktop Entry]\nType=Application\nName=HyprEndfield\nExec=" + quotedExec(QCoreApplication::applicationFilePath())
        + " --background\nTerminal=false\nX-HyprEndfield=true\n").toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(body) != body.size() || !file.commit()) {
        setNotice(tr("无法保存登录启动项")); return false;
    }
    return true;
}
QVariantList SettingsController::displays() const {
    QVariantList out{QVariantMap{{"id", "pointer"}, {"title", tr("鼠标所在显示器")}}, QVariantMap{{"id", "primary"}, {"title", tr("主显示器")}}};
    bool found = value("displayChoice") == "pointer" || value("displayChoice") == "primary";
    for (auto *screen : QGuiApplication::screens()) {
        out.append(QVariantMap{{"id", screen->name()}, {"title", screen->name()}});
        found |= value("displayChoice") == screen->name();
    }
    if (!found) out.append(QVariantMap{{"id", value("displayChoice")}, {"title", value("displayChoice").toString() + tr("（未连接）")}});
    return out;
}
int SettingsController::layoutRemaining() const { return m_layout.isEmpty() ? 0 : std::max(0, 12 - int(m_layoutClock.elapsed() / 1000)); }
void SettingsController::previewLayout(const QString &key, double input) {
    if (key != "uiScale" && key != "hudOffsetX" && key != "hudOffsetY") return;
    if (m_layout.isEmpty()) for (const auto &k : {"uiScale", "hudOffsetX", "hudOffsetY"}) m_layout[k] = value(k);
    insert(key, normalized(key, input));
    m_layoutClock.restart(); m_layoutTimer.start(); emit layoutChanged();
}
void SettingsController::confirmLayout() {
    if (m_layout.isEmpty()) return;
    if (!layoutRemaining()) { revertLayout(); return; }
    for (auto it = m_layout.cbegin(); it != m_layout.cend(); ++it) m_store.setValue(storageKey(it.key()), value(it.key()));
    m_store.sync();
    if (m_store.status() != QSettings::NoError) { revertLayout(); setNotice(tr("无法保存设置")); return; }
    m_layout.clear(); m_layoutTimer.stop(); emit layoutChanged();
}
void SettingsController::revertLayout() {
    for (auto it = m_layout.cbegin(); it != m_layout.cend(); ++it) insert(it.key(), it.value());
    m_layout.clear(); m_layoutTimer.stop(); emit layoutChanged();
}
void SettingsController::restoreDefaults() {
    revertLayout();
    if (!set("launchAtLogin", false)) return;
    for (auto it = m_defaults.cbegin(); it != m_defaults.cend(); ++it) set(it.key(), it.value());
    setNotice(tr("已恢复默认设置"));
}
void SettingsController::resetModules() { set("moduleOrder", defaultModules()); set("hiddenModules", QStringList{}); }
void SettingsController::moveModule(const QString &key, int destination) {
    auto order = value("moduleOrder").toStringList(); const int source = order.indexOf(key);
    if (source < 0) return;
    order.move(source, std::clamp(destination, 0, int(order.size()) - 1)); set("moduleOrder", order);
}
void SettingsController::showModule(const QString &key, bool visible) {
    if (!defaultModules().contains(key)) return;
    auto hidden = value("hiddenModules").toStringList();
    if (visible) hidden.removeAll(key);
    else if (!hidden.contains(key) && hidden.size() < defaultModules().size() - 1) hidden.append(key);
    set("hiddenModules", hidden);
}
bool SettingsController::light() const { return value("themeIndex").toInt() == 1 || (value("themeIndex").toInt() == 2 && qGuiApp->styleHints()->colorScheme() == Qt::ColorScheme::Light); }
QColor SettingsController::accent() const { const int i = value("accentIndex").toInt(); return QColor(i < accents.size() ? accents[i] : value("customAccent").toString()); }
QColor SettingsController::tone(const QColor &c) const {
    if (!light()) return c;
    const double high = std::max({c.redF(), c.greenF(), c.blueF()}), low = std::min({c.redF(), c.greenF(), c.blueF()});
    if (high - low > .1) return c;
    const double v = (c.redF() + c.greenF() + c.blueF()) / 3, mapped = v < .32 ? 1 - 4.5 * v * v : .52 - (v - .32) * .68;
    return QColor::fromRgbF(std::clamp(c.redF() + mapped - v, 0., 1.), std::clamp(c.greenF() + mapped - v, 0., 1.), std::clamp(c.blueF() + mapped - v, 0., 1.), c.alphaF());
}
QColor SettingsController::color(const QColor &c) const {
    const auto base = accent();
    if (base.name() == "#fad41f") return c;
    const QString hex = c.name(); QColor out = c;
    if (hex == "#fffa00" || hex == "#fad41f") out = base;
    else if (hex == "#fffc6b") out = QColor::fromRgbF(base.redF() + (1 - base.redF()) * .42, base.greenF() + (1 - base.greenF()) * .42, base.blueF() + (1 - base.blueF()) * .42);
    else if (hex == "#e9cf12" || hex == "#f1dd10" || hex == "#d7bc18") {
        const bool deep = hex == "#f1dd10";
        out = QColor::fromHsvF(std::fmod(std::max(0.f, base.hsvHueF()) + 1 - (deep ? 4.16 : 6.08) / 360, 1.),
            base.hsvSaturationF() * (deep ? .934 : .923), base.valueF() * (deep ? .945 : .914));
    }
    out.setAlphaF(c.alphaF()); return out;
}
QUrl SettingsController::centerLogo() const {
    switch (value("centerLogoIndex").toInt()) {
    case 1: return QUrl("qrc:/assets/settings/center-logos/rhodesIsland.png");
    case 2: return QUrl("qrc:/assets/settings/center-logos/babel.png");
    case 3: return QUrl("qrc:/assets/settings/center-logos/rhineLab.png");
    case 4: return QUrl::fromLocalFile(value("customLogo").toString());
    default: return {};
    }
}
void SettingsController::importLogo(const QUrl &url) {
    const QString path = url.toLocalFile(); QImageReader reader(path); reader.setAutoTransform(true);
    const auto size = reader.size();
    if (path.isEmpty() || QFileInfo(path).size() > 32 * 1024 * 1024 || !size.isValid() || size.width() > 16384 || size.height() > 16384 || qint64(size.width()) * size.height() > 64000000) {
        setNotice(tr("请选择不超过 32 MB、6400 万像素的图片。")); return;
    }
    reader.setScaledSize(size.scaled(768, 768, Qt::KeepAspectRatio));
    auto image = reader.read(); if (image.isNull()) { setNotice(tr("请选择可读取的图片文件。")); return; }
    const QString dir = m_data + "/CenterLogo"; QDir().mkpath(dir);
    const QString destination = dir + '/' + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".png";
    QSaveFile file(destination);
    if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit()) { setNotice(tr("无法保存中心标志")); return; }
    set("customLogo", destination); set("centerLogoIndex", 4);
}
void SettingsController::setNotice(const QString &message) { m_notice = message; emit noticeChanged(); }
void SettingsController::clearNotice() { setNotice({}); }
QString SettingsController::version() const { return QStringLiteral(HYPR_VERSION); }
QString SettingsController::buildId() const { return QStringLiteral(HYPR_BUILD_ID); }
void SettingsController::checkUpdates() {
    if (m_update.value("phase") == "checking") return;
    m_update = {{"phase", "checking"}, {"detail", tr("连接中")}, {"latest", ""}}; emit updateStateChanged();
    QNetworkRequest request(QUrl("https://api.github.com/repos/OctoberPrayRain/HyprEndfield/commits/main"));
    request.setRawHeader("Accept", "application/vnd.github+json"); request.setRawHeader("User-Agent", "HyprEndfield"); request.setTransferTimeout(12000);
    auto *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const auto object = QJsonDocument::fromJson(reply->readAll()).object();
        const QString sha = object.value("sha").toString();
        if (reply->error() != QNetworkReply::NoError || sha.size() != 40)
            m_update = {{"phase", "failed"}, {"detail", tr("检查失败：") + reply->errorString()}, {"latest", ""}};
        else {
            const bool current = sha.startsWith(buildId()) && !buildId().endsWith('+');
            m_update = {{"phase", current ? "current" : "available"}, {"detail", current ? tr("已是最新") : tr("远端提交与当前构建不同；请更新源码后重新构建")}, {"latest", sha.left(8)}};
        }
        reply->deleteLater(); emit updateStateChanged();
    });
}
void SettingsController::openLink(const QString &key) {
    static const QHash<QString, QString> urls{{"repository", "https://github.com/OctoberPrayRain/HyprEndfield"},
        {"feedback", "https://github.com/OctoberPrayRain/HyprEndfield/issues"}, {"latest", "https://github.com/OctoberPrayRain/HyprEndfield/commits/main"},
        {"source", "https://github.com/DDDuoDuo/EndfieldHUD"}, {"inspiration", "https://www.bilibili.com/video/BV1DBaP6yEHs/"},
        {"charge", "https://github.com/QinAnze/zmd-charge"}, {"audio", "https://github.com/ronitsingh10/FineTune"}};
    if (urls.contains(key) && !QDesktopServices::openUrl(QUrl(urls.value(key)))) setNotice(tr("无法打开链接"));
}
bool SettingsController::beginBootLog() {
    if (!value("dailyBootLog").toBool() || value("reduceMotion").toBool()) return false;
    const auto day = QDate::currentDate().toString(Qt::ISODate);
    if (m_store.value("system/lastBootLogDay").toString() == day) return false;
    m_store.setValue("system/lastBootLogDay", day); m_store.sync(); return true;
}

void SettingsController::setBatteryWindow(QQuickWindow *window) {
    m_batteryWindow = window;
    if (!window) return;
    connect(window, &QWindow::screenChanged, this, &SettingsController::positionBattery);
    positionBattery();
}
void SettingsController::updateBattery(const QVariantMap &state) {
    m_hasBattery = state.value("available").toBool();
    const bool plugged = state.value("plugged").toBool();
    const int method = value("batteryMethod").toInt();
    if (!m_hasBattery || method == 2) { m_alertVisible = false; m_alertTimer.stop(); }
    else if (method == 1) { m_alertVisible = true; m_alertTimer.stop(); }
    else if (m_plugged && *m_plugged != plugged) {
        m_alertVisible = true; m_alertTimer.start(qRound(value("batteryDuration").toDouble() * 1000));
    }
    m_plugged = m_hasBattery ? std::optional(plugged) : std::nullopt;
    emit batteryAlertChanged();
}
void SettingsController::editBatteryPosition() {
    m_editBattery = true; emit batteryAlertChanged(); positionBattery();
    setNotice(tr("拖动电池提示，松开鼠标保存位置。"));
}
void SettingsController::placeBattery(double x, double y) {
    set("batteryX", x);
    set("batteryY", y);
    set("batteryPlacement", 1);
    m_editBattery = false; emit batteryAlertChanged(); positionBattery();
}
void SettingsController::cancelBatteryPosition() { m_editBattery = false; emit batteryAlertChanged(); }
void SettingsController::positionBattery() {
    if (!m_batteryWindow || !m_batteryWindow->screen()) return;
    auto *layer = LayerShellQt::Window::get(m_batteryWindow);
    const QSize screen = m_batteryWindow->screen()->size();
    const double scale = value("batteryScale").toDouble();
    const QSize size(qRound(300 * scale), qRound(84 * scale));
    layer->setDesiredSize(size);
    if (value("batteryPlacement").toInt() == 0) {
        layer->setAnchors(LayerShellQt::Window::AnchorTop);
        layer->setMargins(QMargins(0, 12, 0, 0));
    } else {
        layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop) | LayerShellQt::Window::AnchorLeft);
        const int x = std::clamp(qRound(screen.width() * value("batteryX").toDouble() - size.width() / 2.), 0, std::max(0, screen.width() - size.width()));
        const int y = std::clamp(qRound(screen.height() * value("batteryY").toDouble() - size.height() / 2.), 0, std::max(0, screen.height() - size.height()));
        layer->setMargins(QMargins(x, y, 0, 0));
    }
}
