#include <QCoreApplication>
#include "clipboardcontroller.hpp"

#include "wlr-data-control-unstable-v1-client-protocol.h"
#include "ext-data-control-v1-client-protocol.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>

#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <QUrl>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <poll.h>
#include <thread>
#include <unordered_map>
#include <unistd.h>

namespace {
constexpr qsizetype maximumTextBytes = 1024 * 1024;
constexpr qsizetype maximumImageBytes = 64 * 1024 * 1024;
constexpr qsizetype maximumRetainedBytes = 128 * 1024 * 1024;

QString compactText(const QString &value)
{
    const QString flattened = value.simplified();
    return flattened.size() > 90 ? flattened.left(89) + QStringLiteral("…") : flattened;
}

bool looksLikeLink(const QString &value)
{
    const QString candidate = value.trimmed();
    if (candidate.isEmpty() || candidate.contains(QRegularExpression(QStringLiteral("\\s")))) return false;
    const QUrl url(candidate);
    const QString scheme = url.scheme().toLower();
    if (QStringList{QStringLiteral("http"), QStringLiteral("https"), QStringLiteral("ftp")}.contains(scheme))
        return !url.host().isEmpty();
    return QStringList{QStringLiteral("mailto"), QStringLiteral("tel"), QStringLiteral("sms")}.contains(scheme)
        && candidate.size() > scheme.size() + 1;
}

QString imageFormat(const QString &mime)
{
    QString value = mime.section('/', 1, 1).toUpper();
    if (value == QStringLiteral("JPG")) value = QStringLiteral("JPEG");
    return value;
}

QVariantList fileUris(const QByteArray &data)
{
    QVariantList result;
    const QString text = QString::fromUtf8(data);
    for (const QString &line : text.split(QRegularExpression(QStringLiteral("\\r?\\n")), Qt::SkipEmptyParts)) {
        const QString value = line.trimmed();
        if (!value.startsWith(QLatin1Char('#')) && QUrl(value).isLocalFile()) result.append(value);
    }
    return result;
}

QString filePreview(const QVariantList &uris)
{
    QStringList names;
    for (const auto &value : uris.mid(0, 3)) names.append(QFileInfo(QUrl(value.toString()).toLocalFile()).fileName());
    QString preview = names.join(QStringLiteral(", "));
    if (uris.size() > 3) preview += QStringLiteral(" (+%1)").arg(uris.size() - 3);
    return compactText(preview);
}

QString fileMeta(const QVariantList &uris)
{
    if (uris.isEmpty()) return {};
    if (uris.size() > 1) return QCoreApplication::translate("HyprEndfield", "%1 项").arg(uris.size());
    const QFileInfo info(QUrl(uris.constFirst().toString()).toLocalFile());
    if (info.isDir()) return QCoreApplication::translate("HyprEndfield", "文件夹");
    const qint64 bytes = info.size();
    if (bytes < 1000) return QCoreApplication::translate("HyprEndfield", "%1 字节").arg(bytes);
    static const QStringList units{QStringLiteral("KB"), QStringLiteral("MB"),
                                   QStringLiteral("GB"), QStringLiteral("TB")};
    double value = bytes;
    int unit = -1;
    do { value /= 1000.0; ++unit; } while (value >= 1000.0 && unit + 1 < units.size());
    const int decimals = unit == 0 ? 0 : unit == 1 ? 1 : 2;
    return QString::number(value, 'f', decimals).remove(QRegularExpression(QStringLiteral("\\.?0+$")))
        + QLatin1Char(' ') + units.at(unit);
}

void writeAllAndClose(int descriptor, QByteArray data)
{
    const int flags = fcntl(descriptor, F_GETFL);
    if (flags >= 0) fcntl(descriptor, F_SETFL, flags & ~O_NONBLOCK);
    qsizetype offset = 0;
    while (offset < data.size()) {
        const ssize_t written = ::write(descriptor, data.constData() + offset,
                                        static_cast<size_t>(data.size() - offset));
        if (written > 0) offset += written;
        else if (written < 0 && errno == EINTR) continue;
        else break;
    }
    ::close(descriptor);
}
}

struct ClipboardController::Entry
{
    QString id;
    QString kind;
    QString preview;
    QString meta;
    QString mime;
    QString thumbnail;
    QVariantList fileUris;
    QByteArray payload;
    qint64 createdAt = 0;
    bool pinned = false;
};

// wlr-data-control 与 ext-data-control 的请求/事件几乎一一对应（仅
// primary_selected/primary_selection 事件名不同），因此用一层 traits 隔离协议
// 前缀，运行期按合成器导出的 global 选择：先试 wlroots 系，再试 KWin 的 ext。

namespace {
namespace clipboard_dc {

struct Wlr
{
    using Manager = zwlr_data_control_manager_v1;
    using Device = zwlr_data_control_device_v1;
    using Source = zwlr_data_control_source_v1;
    using Offer = zwlr_data_control_offer_v1;
    using DeviceListener = zwlr_data_control_device_v1_listener;
    using OfferListener = zwlr_data_control_offer_v1_listener;
    using SourceListener = zwlr_data_control_source_v1_listener;

    static constexpr const char *managerName = "zwlr_data_control_manager_v1";

    static Manager *bindManager(wl_registry *registry, uint32_t name, uint32_t version)
    {
        return static_cast<Manager *>(wl_registry_bind(registry, name,
            &zwlr_data_control_manager_v1_interface, std::min(version, 2u)));
    }
    static Device *getDevice(Manager *manager, wl_seat *seat)
    {
        return zwlr_data_control_manager_v1_get_data_device(manager, seat);
    }
    static void addDeviceListener(Device *device, const DeviceListener *listener, void *data)
    {
        zwlr_data_control_device_v1_add_listener(device, listener, data);
    }
    static void destroyDevice(Device *device) { zwlr_data_control_device_v1_destroy(device); }
    static Source *createSource(Manager *manager)
    {
        return zwlr_data_control_manager_v1_create_data_source(manager);
    }
    static void addSourceListener(Source *source, const SourceListener *listener, void *data)
    {
        zwlr_data_control_source_v1_add_listener(source, listener, data);
    }
    static void sourceOffer(Source *source, const char *mime)
    {
        zwlr_data_control_source_v1_offer(source, mime);
    }
    static void destroySource(Source *source) { zwlr_data_control_source_v1_destroy(source); }
    static void setSelection(Device *device, Source *source)
    {
        zwlr_data_control_device_v1_set_selection(device, source);
    }
    static void addOfferListener(Offer *offer, const OfferListener *listener, void *data)
    {
        zwlr_data_control_offer_v1_add_listener(offer, listener, data);
    }
    static void offerReceive(Offer *offer, const char *mime, int32_t descriptor)
    {
        zwlr_data_control_offer_v1_receive(offer, mime, descriptor);
    }
    static void destroyOffer(Offer *offer) { zwlr_data_control_offer_v1_destroy(offer); }
    static void destroyManager(Manager *manager) { zwlr_data_control_manager_v1_destroy(manager); }
};

struct Ext
{
    using Manager = ext_data_control_manager_v1;
    using Device = ext_data_control_device_v1;
    using Source = ext_data_control_source_v1;
    using Offer = ext_data_control_offer_v1;
    using DeviceListener = ext_data_control_device_v1_listener;
    using OfferListener = ext_data_control_offer_v1_listener;
    using SourceListener = ext_data_control_source_v1_listener;

    static constexpr const char *managerName = "ext_data_control_manager_v1";

    static Manager *bindManager(wl_registry *registry, uint32_t name, uint32_t version)
    {
        return static_cast<Manager *>(wl_registry_bind(registry, name,
            &ext_data_control_manager_v1_interface, std::min(version, 1u)));
    }
    static Device *getDevice(Manager *manager, wl_seat *seat)
    {
        return ext_data_control_manager_v1_get_data_device(manager, seat);
    }
    static void addDeviceListener(Device *device, const DeviceListener *listener, void *data)
    {
        ext_data_control_device_v1_add_listener(device, listener, data);
    }
    static void destroyDevice(Device *device) { ext_data_control_device_v1_destroy(device); }
    static Source *createSource(Manager *manager)
    {
        return ext_data_control_manager_v1_create_data_source(manager);
    }
    static void addSourceListener(Source *source, const SourceListener *listener, void *data)
    {
        ext_data_control_source_v1_add_listener(source, listener, data);
    }
    static void sourceOffer(Source *source, const char *mime)
    {
        ext_data_control_source_v1_offer(source, mime);
    }
    static void destroySource(Source *source) { ext_data_control_source_v1_destroy(source); }
    static void setSelection(Device *device, Source *source)
    {
        ext_data_control_device_v1_set_selection(device, source);
    }
    static void addOfferListener(Offer *offer, const OfferListener *listener, void *data)
    {
        ext_data_control_offer_v1_add_listener(offer, listener, data);
    }
    static void offerReceive(Offer *offer, const char *mime, int32_t descriptor)
    {
        ext_data_control_offer_v1_receive(offer, mime, descriptor);
    }
    static void destroyOffer(Offer *offer) { ext_data_control_offer_v1_destroy(offer); }
    static void destroyManager(Manager *manager) { ext_data_control_manager_v1_destroy(manager); }
};

}
}

class ClipboardController::BackendImplBase : public QObject
{
public:
    explicit BackendImplBase(ClipboardController *controller) : owner(controller) {}
    virtual bool start() = 0;
    virtual void shutdown() = 0;
    virtual bool publish(const Entry &entry) = 0;

protected:
    void reportReady(bool ready, const QString &status = {})
    {
        QMetaObject::invokeMethod(owner, [controller = owner, ready, status] {
            controller->setBackendReady(ready, status);
        }, Qt::QueuedConnection);
    }

    ClipboardController *owner = nullptr;
};

template <class P>
struct ClipboardController::BackendImpl final : ClipboardController::BackendImplBase
{
    struct Offer {
        typename P::Offer *object = nullptr;
        QStringList mimeTypes;
    };
    struct Source {
        BackendImpl *backend = nullptr;
        typename P::Source *object = nullptr;
        QHash<QString, QByteArray> payloads;
    };

    explicit BackendImpl(ClipboardController *controller) : BackendImplBase(controller) {}
    ~BackendImpl() override { shutdown(); }

    bool start() override
    {
        display = wl_display_connect(nullptr);
        if (!display) return false;
        registry = wl_display_get_registry(display);
        wl_registry_add_listener(registry, &registryListener, this);
        if (wl_display_roundtrip(display) < 0 || !manager || !seat) {
            
            return false;
        }
        device = P::getDevice(manager, seat);
        P::addDeviceListener(device, &deviceListener, this);
        baselinePending = true;
        if (wl_display_roundtrip(display) < 0) return false;
        baselinePending = false;
        displayNotifier = std::make_unique<QSocketNotifier>(wl_display_get_fd(display), QSocketNotifier::Read, this);
        QObject::connect(displayNotifier.get(), &QSocketNotifier::activated, this, [this] { dispatch(); });
        connected = true;
        return true;
    }

    void shutdown() override
    {
        cancelRead();
        displayNotifier.reset();
        for (auto &source : sources) {
            if (source->object) P::destroySource(source->object);
            source->object = nullptr;
        }
        sources.clear();
        for (auto &[object, offer] : offers) {
            if (object) P::destroyOffer(object);
            offer->object = nullptr;
        }
        offers.clear();
        selection = nullptr;
        if (device) P::destroyDevice(device);
        if (seat) wl_seat_destroy(seat);
        if (manager) P::destroyManager(manager);
        if (registry) wl_registry_destroy(registry);
        if (display) wl_display_disconnect(display);
        device = nullptr; seat = nullptr; manager = nullptr; registry = nullptr; display = nullptr;
        connected = false;
    }

    bool publish(const Entry &entry) override
    {
        if (!connected || !manager || !device) return false;
        auto source = std::make_unique<Source>();
        source->backend = this;
        source->object = P::createSource(manager);
        P::addSourceListener(source->object, &sourceListener, source.get());
        if (entry.kind == QStringLiteral("text") || entry.kind == QStringLiteral("url")) {
            for (const auto &mime : {QStringLiteral("text/plain;charset=utf-8"), QStringLiteral("text/plain"),
                                     QStringLiteral("UTF8_STRING"), QStringLiteral("STRING"), QStringLiteral("TEXT")})
                source->payloads.insert(mime, entry.payload);
        } else {
            source->payloads.insert(entry.mime, entry.payload);
        }
        for (auto it = source->payloads.cbegin(); it != source->payloads.cend(); ++it)
            P::sourceOffer(source->object, it.key().toUtf8().constData());
        Source *raw = source.get();
        sources.push_back(std::move(source));
        ignoreNextSelection = true;
        P::setSelection(device, raw->object);
        const bool success = wl_display_flush(display) >= 0 || errno == EAGAIN;
        if (!success) ignoreNextSelection = false;
        return success;
    }

    void dispatch()
    {
        // A socket notification can be stale after pending events are handled.
        // Never enter wl_display_dispatch()'s blocking read in Qt's event loop.
        bool failed = !display;
        if (!failed) {
            while (wl_display_prepare_read(display) != 0) {
                if (wl_display_dispatch_pending(display) < 0) { failed = true; break; }
            }
            if (!failed) {
                pollfd descriptor{wl_display_get_fd(display), POLLIN, 0};
                const int result = ::poll(&descriptor, 1, 0);
                if (result > 0 && (descriptor.revents & (POLLIN | POLLHUP | POLLERR)))
                    failed = wl_display_read_events(display) < 0;
                else
                    wl_display_cancel_read(display);
                if (!failed) failed = wl_display_dispatch_pending(display) < 0;
            }
        }
        if (failed) {
            connected = false;
            displayNotifier.reset();
            reportReady(false, QCoreApplication::translate("HyprEndfield", "无法连接系统剪贴板"));
            return;
        }
        wl_display_flush(display);
    }

    void beginRead(Offer *offer)
    {
        cancelRead();
        if (!offer) return;
        for (const auto &type : offer->mimeTypes) {
            if (type.compare(QStringLiteral("x-kde-passwordManagerHint"), Qt::CaseInsensitive) == 0) return;
        }
        QString mime;
        if (offer->mimeTypes.contains(QStringLiteral("text/uri-list"))) {
            mime = QStringLiteral("text/uri-list");
        } else {
            static const QStringList imagePriority{QStringLiteral("image/png"), QStringLiteral("image/jpeg"),
                QStringLiteral("image/jpg"), QStringLiteral("image/tiff"), QStringLiteral("image/webp"),
                QStringLiteral("image/bmp")};
            for (const auto &candidate : imagePriority)
                if (offer->mimeTypes.contains(candidate)) { mime = candidate; break; }
            if (mime.isEmpty()) {
                static const QStringList textPriority{QStringLiteral("text/plain;charset=utf-8"),
                    QStringLiteral("text/plain"), QStringLiteral("UTF8_STRING"), QStringLiteral("TEXT"),
                    QStringLiteral("STRING")};
                for (const auto &candidate : textPriority)
                    if (offer->mimeTypes.contains(candidate)) { mime = candidate; break; }
            }
        }
        if (mime.isEmpty()) return;
        int descriptors[2];
        if (pipe2(descriptors, O_CLOEXEC | O_NONBLOCK) != 0) return;
        readDescriptor = descriptors[0];
        readMime = mime;
        readData.clear();
        readLimit = mime.startsWith(QStringLiteral("image/")) ? maximumImageBytes : maximumTextBytes;
        P::offerReceive(offer->object, mime.toUtf8().constData(), descriptors[1]);
        ::close(descriptors[1]);
        wl_display_flush(display);
        readNotifier = std::make_unique<QSocketNotifier>(readDescriptor, QSocketNotifier::Read, this);
        QObject::connect(readNotifier.get(), &QSocketNotifier::activated, this, [this] { drainRead(); });
        drainRead();
    }

    void drainRead()
    {
        if (readDescriptor < 0) return;
        char buffer[64 * 1024];
        for (;;) {
            const ssize_t count = ::read(readDescriptor, buffer, sizeof(buffer));
            if (count > 0) {
                if (readData.size() + count > readLimit) { cancelRead(); return; }
                readData.append(buffer, count);
                continue;
            }
            if (count == 0) {
                const QString mime = readMime;
                const QByteArray data = readData;
                cancelRead();
                QMetaObject::invokeMethod(owner, [controller = owner, mime, data] {
                    controller->capture(mime, data, false);
                }, Qt::QueuedConnection);
                return;
            }
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            cancelRead();
            return;
        }
    }

    void cancelRead()
    {
        readNotifier.reset();
        if (readDescriptor >= 0) ::close(readDescriptor);
        readDescriptor = -1;
        readMime.clear();
        readData.clear();
        readLimit = 0;
    }

    void removeOffer(typename P::Offer *object)
    {
        if (!object) return;
        const auto found = offers.find(object);
        if (found == offers.end()) return;
        P::destroyOffer(object);
        offers.erase(found);
    }

    void removeSource(Source *source)
    {
        const auto found = std::find_if(sources.begin(), sources.end(),
            [source](const auto &candidate) { return candidate.get() == source; });
        if (found == sources.end()) return;
        if ((*found)->object) P::destroySource((*found)->object);
        (*found)->object = nullptr;
        sources.erase(found);
    }

    static void registryGlobal(void *data, wl_registry *registryObject, uint32_t name,
                               const char *interface, uint32_t version)
    {
        auto *self = static_cast<BackendImpl *>(data);
        if (!self->manager && std::strcmp(interface, P::managerName) == 0)
            self->manager = P::bindManager(registryObject, name, version);
        else if (!self->seat && std::strcmp(interface, wl_seat_interface.name) == 0)
            self->seat = static_cast<wl_seat *>(wl_registry_bind(
                registryObject, name, &wl_seat_interface, std::min(version, 7u)));
    }
    static void registryRemoved(void *, wl_registry *, uint32_t) {}
    static void offered(void *data, typename P::Offer *, const char *mime)
    {
        static_cast<Offer *>(data)->mimeTypes.append(QString::fromUtf8(mime));
    }
    static void dataOffer(void *data, typename P::Device *, typename P::Offer *object)
    {
        auto *self = static_cast<BackendImpl *>(data);
        auto offer = std::make_unique<Offer>();
        offer->object = object;
        P::addOfferListener(object, &offerListener, offer.get());
        self->offers.emplace(object, std::move(offer));
    }
    static void selected(void *data, typename P::Device *, typename P::Offer *object)
    {
        auto *self = static_cast<BackendImpl *>(data);
        if (self->selection && self->selection != object) self->removeOffer(self->selection);
        self->selection = object;
        if (!object) { self->cancelRead(); return; }
        if (self->baselinePending) return;
        if (self->ignoreNextSelection) { self->ignoreNextSelection = false; return; }
        const auto found = self->offers.find(object);
        if (found != self->offers.end()) self->beginRead(found->second.get());
    }
    static void finished(void *data, typename P::Device *)
    {
        auto *self = static_cast<BackendImpl *>(data);
        self->connected = false;
        self->reportReady(false, QCoreApplication::translate("HyprEndfield", "系统剪贴板监视已终止"));
    }
    static void primarySelected(void *data, typename P::Device *, typename P::Offer *object)
    {
        static_cast<BackendImpl *>(data)->removeOffer(object);
    }
    static void sourceSend(void *data, typename P::Source *, const char *mime, int32_t descriptor)
    {
        auto *source = static_cast<Source *>(data);
        const QByteArray bytes = source->payloads.value(QString::fromUtf8(mime));
        std::thread(writeAllAndClose, descriptor, bytes).detach();
    }
    static void sourceCancelled(void *data, typename P::Source *)
    {
        auto *source = static_cast<Source *>(data);
        source->backend->removeSource(source);
    }

    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    wl_seat *seat = nullptr;
    typename P::Manager *manager = nullptr;
    typename P::Device *device = nullptr;
    std::unique_ptr<QSocketNotifier> displayNotifier;
    std::unordered_map<typename P::Offer *, std::unique_ptr<Offer>> offers;
    std::vector<std::unique_ptr<Source>> sources;
    typename P::Offer *selection = nullptr;
    bool connected = false;
    bool baselinePending = false;
    bool ignoreNextSelection = false;
    int readDescriptor = -1;
    qsizetype readLimit = 0;
    QString readMime;
    QByteArray readData;
    std::unique_ptr<QSocketNotifier> readNotifier;

    inline static const wl_registry_listener registryListener{registryGlobal, registryRemoved};
    inline static const typename P::OfferListener offerListener{offered};
    inline static const typename P::DeviceListener deviceListener{
        dataOffer, selected, finished, primarySelected};
    inline static const typename P::SourceListener sourceListener{sourceSend, sourceCancelled};
};

struct ClipboardController::Backend final : QObject
{
    explicit Backend(ClipboardController *controller) : owner(controller) {}
    ~Backend() { shutdown(); }

    bool start()
    {
        // wlroots 系（Hyprland/niri）走 wlr-data-control；KWin 只提供 ext-data-control。
        
        auto wlr = std::make_unique<BackendImpl<clipboard_dc::Wlr>>(owner);
        if (wlr->start()) { impl = std::move(wlr); return true; }
        wlr->shutdown();
        
        auto ext = std::make_unique<BackendImpl<clipboard_dc::Ext>>(owner);
        if (ext->start()) { impl = std::move(ext); return true; }
        ext->shutdown();
        return false;
    }

    void shutdown()
    {
        if (!impl) return;
        impl->shutdown();
        impl.reset();
    }

    bool publish(const Entry &entry) { return impl && impl->publish(entry); }

    void reportReady(bool ready, const QString &status = {})
    {
        QMetaObject::invokeMethod(owner, [controller = owner, ready, status] {
            controller->setBackendReady(ready, status);
        }, Qt::QueuedConnection);
    }

private:
    ClipboardController *owner = nullptr;
    std::unique_ptr<BackendImplBase> impl;
};


ClipboardController::ClipboardController(QObject *parent, bool automatic, int capacity)
    : QObject(parent), m_capacity(std::max(1, capacity)), m_status(QCoreApplication::translate("HyprEndfield", "正在连接系统剪贴板"))
{
    if (!automatic) {
        m_ready = true;
        m_status.clear();
        return;
    }
    m_backend = new Backend(this);
    m_backend->moveToThread(&m_backendThread);
    connect(&m_backendThread, &QThread::started, m_backend, [backend = m_backend] {
        const bool ready = backend->start();
        if (!ready) backend->shutdown();
        backend->reportReady(ready, ready ? QString() : QCoreApplication::translate("HyprEndfield", "无法连接系统剪贴板"));
    });
    connect(&m_backendThread, &QThread::finished, m_backend, &QObject::deleteLater);
    m_backendThread.setObjectName(QStringLiteral("WaylandClipboard"));
    m_backendThread.start();
}

ClipboardController::~ClipboardController()
{
    if (m_backend) {
        QMetaObject::invokeMethod(m_backend, [backend = m_backend] { backend->shutdown(); },
                                  Qt::BlockingQueuedConnection);
        m_backendThread.quit();
        m_backendThread.wait();
    }
}

void ClipboardController::setActive(bool active)
{
    if (m_active == active) return;
    m_active = active;
    emit activeChanged();
}

QVariantMap ClipboardController::state() const
{
    QVariantList items;
    items.reserve(static_cast<qsizetype>(m_entries.size()));
    int removable = 0;
    for (const auto &entry : m_entries) {
        QVariantMap value{{"id", entry.id}, {"kind", entry.kind}, {"preview", entry.preview},
                          {"meta", entry.meta}, {"createdAt", entry.createdAt}, {"pinned", entry.pinned}};
        if (!entry.thumbnail.isEmpty()) value["thumbnail"] = entry.thumbnail;
        if (!entry.pinned) ++removable;
        items.append(value);
    }
    return {{"ready", m_ready}, {"total", static_cast<int>(m_entries.size())}, {"capacity", m_capacity},
            {"removable", removable}, {"items", items}, {"status", m_status}};
}

bool ClipboardController::captureForTest(const QString &mime, const QByteArray &data, bool sensitive)
{
    return capture(mime, data, sensitive);
}

bool ClipboardController::capture(const QString &mime, const QByteArray &data, bool sensitive)
{
    if (sensitive || data.isEmpty()) return false;
    Entry entry;
    entry.mime = mime;
    entry.payload = data;
    entry.createdAt = QDateTime::currentMSecsSinceEpoch();
    if (mime == QStringLiteral("text/uri-list")) {
        entry.fileUris = fileUris(data);
        if (entry.fileUris.isEmpty()) return false;
        entry.kind = QStringLiteral("files");
        entry.preview = filePreview(entry.fileUris);
        entry.meta = fileMeta(entry.fileUris);
    } else if (mime.startsWith(QStringLiteral("image/"))) {
        if (data.size() > maximumImageBytes) return false;
        QBuffer source(&entry.payload);
        source.open(QIODevice::ReadOnly);
        QImageReader reader(&source);
        const QSize dimensions = reader.size();
        if (!dimensions.isValid() || dimensions.width() <= 0 || dimensions.height() <= 0
            || static_cast<qint64>(dimensions.width()) * dimensions.height() > 100'000'000) return false;
        reader.setAutoTransform(true);
        reader.setScaledSize(dimensions.scaled(QSize(128, 96), Qt::KeepAspectRatio));
        const QImage thumb = reader.read();
        if (thumb.isNull()) return false;
        entry.kind = QStringLiteral("image");
        entry.preview = QCoreApplication::translate("HyprEndfield", "图片");
        entry.meta = QStringLiteral("%1   %2 × %3").arg(imageFormat(mime))
            .arg(dimensions.width()).arg(dimensions.height());
        QByteArray bytes;
        QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); thumb.save(&buffer, "PNG");
        entry.thumbnail = QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
    } else {
        if (data.size() > maximumTextBytes) return false;
        const QString text = QString::fromUtf8(data);
        if (text.isEmpty()) return false;
        entry.kind = looksLikeLink(text) ? QStringLiteral("url") : QStringLiteral("text");
        entry.preview = compactText(text);
        entry.meta = entry.kind == QStringLiteral("url") ? QUrl(text.trimmed()).host()
            : QCoreApplication::translate("HyprEndfield", "%1 字符").arg(text.size());
    }
        const QByteArray digestInput = entry.kind.toUtf8() + '\0' + entry.payload;
    entry.id = QString::fromLatin1(QCryptographicHash::hash(digestInput, QCryptographicHash::Sha256).toHex());
    const auto existing = std::find_if(m_entries.begin(), m_entries.end(),
        [&entry](const Entry &candidate) { return candidate.id == entry.id; });
    if (existing != m_entries.end()) {
        Entry previous = std::move(*existing);
        m_entries.erase(existing);
        m_entries.insert(m_entries.begin(), std::move(previous));
        publishState();
        return true;
    }
    qsizetype retained = data.size();
    for (const auto &value : m_entries) retained += value.payload.size();
    while (static_cast<int>(m_entries.size()) >= m_capacity || retained > maximumRetainedBytes) {
        const auto removable = std::find_if(m_entries.rbegin(), m_entries.rend(),
            [](const Entry &candidate) { return !candidate.pinned; });
        if (removable == m_entries.rend()) {
            m_status = QCoreApplication::translate("HyprEndfield", "缓存已被固定项目占满");
            publishState();
            return false;
        }
        retained -= removable->payload.size();
        m_entries.erase(std::next(removable).base());
    }
    m_entries.insert(m_entries.begin(), std::move(entry));
    m_status.clear();
    publishState();
    return true;
}

bool ClipboardController::copyItem(const QString &id)
{
    const Entry *entry = findEntry(id);
    if (!entry || !m_backend || !m_ready) {
        emit actionFinished(QStringLiteral("copy"), false, QCoreApplication::translate("HyprEndfield", "无法写入系统剪贴板"));
        return false;
    }
    // Qt's own clipboard consumer may synchronously wait for this source.
    // The source must keep serving requests while the GUI thread is waiting.
    const Entry snapshot = *entry;
    QMetaObject::invokeMethod(m_backend, [backend = m_backend, controller = this, snapshot] {
        const bool success = backend->publish(snapshot);
        QMetaObject::invokeMethod(controller, [controller, success, kind = snapshot.kind] {
            if (success) emit controller->itemCopied(kind);
            emit controller->actionFinished(QStringLiteral("copy"), success,
                success ? QString() : QCoreApplication::translate("HyprEndfield", "无法写入系统剪贴板"));
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
    return true;
}

bool ClipboardController::togglePin(const QString &id)
{
    Entry *entry = findEntry(id);
    if (!entry) return false;
    entry->pinned = !entry->pinned;
    publishState();
    emit actionFinished(QStringLiteral("pin"), true, {});
    return true;
}

bool ClipboardController::removeItem(const QString &id)
{
    const auto found = std::find_if(m_entries.begin(), m_entries.end(),
        [&id](const Entry &entry) { return entry.id == id; });
    if (found == m_entries.end() || found->pinned) return false;
    m_entries.erase(found);
    publishState();
    emit actionFinished(QStringLiteral("remove"), true, {});
    return true;
}

bool ClipboardController::clearUnpinned()
{
    const auto oldSize = m_entries.size();
    std::erase_if(m_entries, [](const Entry &entry) { return !entry.pinned; });
    const bool changed = oldSize != m_entries.size();
    if (changed) publishState();
    emit actionFinished(QStringLiteral("clear"), changed,
        changed ? QString() : QCoreApplication::translate("HyprEndfield", "没有可以清空的记录"));
    return changed;
}

void ClipboardController::setBackendReady(bool ready, const QString &status)
{
    const QVariantMap before = state();
    m_ready = ready;
    m_status = status;
    if (before != state()) emit stateChanged();
}

void ClipboardController::publishState()
{
    emit stateChanged();
}

ClipboardController::Entry *ClipboardController::findEntry(const QString &id)
{
    const auto found = std::find_if(m_entries.begin(), m_entries.end(),
        [&id](const Entry &entry) { return entry.id == id; });
    return found == m_entries.end() ? nullptr : &*found;
}

const ClipboardController::Entry *ClipboardController::findEntry(const QString &id) const
{
    const auto found = std::find_if(m_entries.cbegin(), m_entries.cend(),
        [&id](const Entry &entry) { return entry.id == id; });
    return found == m_entries.cend() ? nullptr : &*found;
}
