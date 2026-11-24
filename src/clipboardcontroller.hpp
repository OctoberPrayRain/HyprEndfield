#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QThread>
#include <QVariantMap>

#include <memory>
#include <vector>

class ClipboardController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
public:
    explicit ClipboardController(QObject *parent = nullptr, bool automatic = true, int capacity = 10);
    ~ClipboardController() override;

    bool active() const { return m_active; }
    void setActive(bool active);
    QVariantMap state() const;

    Q_INVOKABLE bool copyItem(const QString &id);
    Q_INVOKABLE bool togglePin(const QString &id);
    Q_INVOKABLE bool removeItem(const QString &id);
    Q_INVOKABLE bool clearUnpinned();

    // Feeds an isolated offer through the same parser without touching the
    // user's clipboard. Used by the controller and page contract tests.
    bool captureForTest(const QString &mime, const QByteArray &data, bool sensitive = false);

signals:
    void itemCopied(const QString &kind);
    void activeChanged();
    void stateChanged();
    void actionFinished(const QString &action, bool success, const QString &message);

private:
    struct Entry;
    struct Backend;
    struct BackendImplBase;
    template <class P> struct BackendImpl;

    bool capture(const QString &mime, const QByteArray &data, bool sensitive);
    void setBackendReady(bool ready, const QString &status = {});
    void publishState();
    Entry *findEntry(const QString &id);
    const Entry *findEntry(const QString &id) const;

    bool m_active = false;
    bool m_ready = false;
    int m_capacity = 10;
    QString m_status;
    std::vector<Entry> m_entries;
    Backend *m_backend = nullptr;
    QThread m_backendThread;
};
