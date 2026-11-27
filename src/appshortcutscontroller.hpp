#pragma once
#include <QObject>
#include <QColor>
#include <QHash>
#include <QImage>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QQuickImageProvider>
#include <functional>
#include <optional>

class QFileSystemWatcher;
class QTimer;

// AppShortcutStore/Canvas rules, with .desktop references and GIO launching on Linux.
class AppShortcutsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(QVariantList presets READ presets NOTIFY changed)
    Q_PROPERTY(QString launchCategory READ launchCategory WRITE setLaunchCategory NOTIFY launchCategoryChanged)
    Q_PROPERTY(QString catalogSource READ catalogSource NOTIFY catalogSourceChanged)
    Q_PROPERTY(QString homeCategory READ homeCategory NOTIFY homeCategoryChanged)
    Q_PROPERTY(bool kdeCatalogAvailable READ kdeCatalogAvailable CONSTANT)
    Q_PROPERTY(int catalogRevision READ catalogRevision NOTIFY changed)
public:
    using TestLauncher = std::function<QString(const QString &)>;
    explicit AppShortcutsController(QObject *parent = nullptr, QString directory = {}, TestLauncher launcher = {});
    QVariantMap state() const;
    QVariantList presets() const;
    QImage artwork(const QString &id, const QString &preset, QColor accent, QSize size) const;
    Q_INVOKABLE QVariantMap applicationCatalog();
    Q_INVOKABLE QVariantList launchCategories();
    QString launchCategory() const { return m_launchCategory; }
    QString catalogSource() const { return m_catalogSource; }
    QString homeCategory() const { return m_homeCategory; }
    bool kdeCatalogAvailable() const;
    int catalogRevision() const { return m_catalogRevision; }
    Q_INVOKABLE QVariantList catalogSources() const;
    Q_INVOKABLE void setCatalogSource(const QString &source);
    Q_INVOKABLE void setHomeCategory(const QString &key);
    Q_INVOKABLE void clampHomeCategory();
    Q_INVOKABLE void applyHomeCategory();
    Q_INVOKABLE void invalidateCatalog();
    Q_INVOKABLE void setLaunchCategory(const QString &key);
    Q_INVOKABLE bool add(const QUrl &url);
    Q_INVOKABLE bool requestOpenPath(const QString &path);
    Q_INVOKABLE bool rename(const QString &id, const QString &name);
    Q_INVOKABLE bool setIcon(const QString &id, const QString &preset);
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE void paste();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool requestOpen(const QString &id);
    void launchPending();
signals:
    void changed();
    void launchRequested();
    void launchFailed();
    void opened(const QString &name);
    void added(const QString &id);
    void launchCategoryChanged();
    void catalogSourceChanged();
    void homeCategoryChanged();
private:
    struct Entry { QString id, name, originalName, path, executable, iconPreset; qint64 created = 0; };
    struct App { QString name, executable; QImage icon; QColor color = QColor("#2A2A2A"); };
    std::optional<App> inspect(const QString &path) const;
    QHash<QString, int> favoriteOrder() const;
    void updateFavorites();
    void launchFile(const QString &path, const QString &name);
    QVariantMap desktopCatalog();
    QVariantMap kdeCatalog();
    void bindCatalogMonitor();
    QString categoryTitle(const QString &key) const;
    QString catalogAppName(const QString &path) const;
    void load();
    bool commit(const QList<Entry> &next);
    bool fail(const QString &message);
    static QString normalizeName(const QString &value);
    QString m_directory, m_path, m_error, m_pending;
    QList<Entry> m_entries;
    QHash<QString, App> m_apps;
    QByteArray m_saved;
    int m_catalogRevision = 0;
    QVariantMap m_catalog;
    QString m_pendingPath, m_launchCategory = QStringLiteral("all");
    QString m_catalogSource = QStringLiteral("desktop");
    QString m_homeCategory = QStringLiteral("all");
    QFileSystemWatcher *m_appWatcher = nullptr;
    QTimer *m_appRefresh = nullptr;
    QMetaObject::Connection m_sycocaConnection;
    bool m_catalogReady = false;
    bool m_readable = true, m_launching = false;
    TestLauncher m_testLauncher;
};

class ShortcutIconProvider final : public QQuickImageProvider {
public:
    explicit ShortcutIconProvider(AppShortcutsController *controller) : QQuickImageProvider(QQuickImageProvider::Image), m_controller(controller) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requested) override {
        const auto parts = id.split('/');
        QColor accent("#" + parts.value(2)); if (!accent.isValid()) accent = QColor("#FFFA00");
        const auto image = m_controller->artwork(parts.value(0), parts.value(1, "original"), accent, requested);
        if (size) *size = image.size();
        return image;
    }
private:
    AppShortcutsController *m_controller;
};
