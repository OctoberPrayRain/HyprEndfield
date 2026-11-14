#include "activitycollector.hpp"

#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <unistd.h>

namespace Activity {
namespace {
QByteArray read(const QString &path) {
    QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QList<QByteArray> words(const QByteArray &text) { return text.simplified().split(' '); }
QVariant number(std::optional<double> value) { return value && std::isfinite(*value) && *value >= 0 ? QVariant(*value) : QVariant(); }
QVariant number(std::optional<quint64> value) { return value ? QVariant::fromValue(*value) : QVariant(); }
std::optional<double> difference(quint64 now, quint64 old, double seconds) {
    return now >= old && seconds > 0 ? std::optional<double>(double(now - old) / seconds) : std::nullopt;
}
std::optional<Counters> pair(const QList<QByteArray> &values, int first, int second) {
    if (values.size() <= std::max(first, second)) return {};
    bool a, b; const auto one = values[first].toULongLong(&a), two = values[second].toULongLong(&b);
    return a && b ? std::optional<Counters>({one, two}) : std::nullopt;
}
QByteArray query(const QString &program, const QStringList &args, int timeout) {
    if (program.isEmpty()) return {};
    QProcess process; process.start(program, args, QIODevice::ReadOnly);
    if (!process.waitForFinished(timeout)) { process.kill(); process.waitForFinished(250); return {}; }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0 ? process.readAllStandardOutput() : QByteArray();
}

// The compositor's open windows as (pid, application id), which is what maps a
// process to its desktop entry. Hyprland reports initialClass/class on `clients`;
// niri reports app_id on `windows`. Both are one JSON array per call and are
// only consulted when the corresponding integration is present, so a session
// runs one helper script, not both.
QList<QPair<int, QString>> compositorWindows() {
    QList<QPair<int, QString>> result;
    if (!qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")) {
        const auto bytes = query(QStandardPaths::findExecutable(QStringLiteral("hyprctl")),
            {QStringLiteral("clients"), QStringLiteral("-j")}, 800);
        for (const auto &value : QJsonDocument::fromJson(bytes).array()) {
            const auto window = value.toObject();
            result.append({window.value("pid").toInt(),
                window.value("initialClass").toString(window.value("class").toString())});
        }
        return result;
    }
    if (!qEnvironmentVariableIsEmpty("NIRI_SOCKET")) {
        const auto bytes = query(QStandardPaths::findExecutable(QStringLiteral("niri")),
            {QStringLiteral("msg"), QStringLiteral("--json"), QStringLiteral("windows")}, 800);
        // niri's app_id is the Wayland application id: desktop entries usually match
        // it directly, and the comparison below also accepts app_id + ".desktop".
        for (const auto &value : QJsonDocument::fromJson(bytes).array()) {
            const auto window = value.toObject();
            result.append({window.value("pid").toInt(), window.value("app_id").toString()});
        }
        return result;
    }
    return result;
}
QList<QVariant> rates(const QMap<QString, Counters> &now, const QMap<QString, Counters> &old, double elapsed) {
    if (now.isEmpty() || now.keys() != old.keys() || elapsed <= 0 || elapsed > 15) return {{}, {}};
    double a = 0, b = 0;
    for (auto it = now.cbegin(); it != now.cend(); ++it) {
        const auto before = old.value(it.key());
        const auto first = difference(it->first, before.first, elapsed), second = difference(it->second, before.second, elapsed);
        if (!first || !second) return {{}, {}};
        a += *first; b += *second;
    }
    return {a, b};
}
}

std::optional<Counters> parseCpu(const QByteArray &text) {
    const auto values = words(text.split('\n').first());
    if (values.size() < 9 || values[0] != "cpu") return {};
    quint64 total = 0, idle = 0;
    // guest and guest_nice are already included in user/nice, so stop at steal.
    for (int i = 1; i <= 8; ++i) {
        bool ok; const auto value = values[i].toULongLong(&ok); if (!ok) return {};
        total += value; if (i == 4 || i == 5) idle += value;
    }
    return Counters{total, idle};
}
std::optional<Process> parseProcess(const QByteArray &text, long pageSize) {
    const int open = text.indexOf('('), close = text.lastIndexOf(')');
    if (open < 1 || close < open || pageSize <= 0) return {};
    const auto fields = words(text.mid(close + 1)); if (fields.size() < 22) return {};
    Process result; bool ok;
    result.pid = text.left(open).trimmed().toInt(&ok); if (!ok || result.pid <= 0) return {};
    result.name = QString::fromLocal8Bit(text.mid(open + 1, close - open - 1));
    result.parent = fields[1].toInt(&ok); if (!ok) return {};
    const auto cpu = pair(fields, 11, 12); if (!cpu) return {};
    result.ticks = cpu->first + cpu->second;
    result.threads = fields[17].toInt(&ok); if (!ok || result.threads < 0) return {};
    result.start = fields[19].toULongLong(&ok); if (!ok) return {};
    const auto resident = fields[21].toLongLong(&ok); if (!ok || resident < 0) return {};
    result.memory = quint64(resident) * quint64(pageSize);
    return result;
}

void parseDrm(const QByteArray &text, Process &process) {
    // https://docs.kernel.org/gpu/drm-usage-stats.html
    QMap<QByteArray, QByteArray> fields;
    for (const auto &line : text.split('\n')) {
        const int split = line.indexOf(':');
        if (split > 0) fields[line.left(split)] = line.mid(split + 1).trimmed();
    }
    if (!fields.contains("drm-driver") || !fields.contains("drm-client-id")) return;
    const QString prefix = QString::fromLatin1(fields.value("drm-pdev", fields["drm-driver"]))
        + '/' + QString::fromLatin1(fields["drm-client-id"]) + '/';
    for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
        if (!it.key().startsWith("drm-engine-") || it.key().startsWith("drm-engine-capacity-")) continue;
        const auto values = words(it.value());
        bool valid = false; const auto counter = values.value(0).toULongLong(&valid);
        if (!valid || values.value(1) != "ns") continue;
        const auto engine = it.key().mid(11);
        const QString key = prefix + QString::fromLatin1(engine);
        process.gpuEngines[key] = std::max(process.gpuEngines.value(key), counter);
        const double capacity = fields.value("drm-engine-capacity-" + engine, "1").toDouble(&valid);
        process.gpuCapacities[key] = valid && capacity > 0 ? capacity : 1;
    }
}

QMap<int, double> parseNvidiaProcesses(const QByteArray &text) {
    // Read only named utilisation columns; unavailable driver readings remain absent.
    QMap<int, double> result; QList<QByteArray> header;
    for (auto line : text.split('\n')) {
        line = line.trimmed();
        if (line.startsWith('#')) {
            auto fields = words(line.mid(1));
            if (fields.contains("pid") && fields.contains("sm")) header = fields;
            continue;
        }
        if (header.isEmpty() || line.isEmpty()) continue;
        const auto fields = words(line); bool ok = false;
        const int pid = fields.value(header.indexOf("pid")).toInt(&ok);
        if (!ok || pid <= 0) continue;
        for (const auto &engine : {"sm", "enc", "dec", "jpg", "ofa"}) {
            const int index = header.indexOf(engine); if (index < 0) continue;
            const double value = fields.value(index).toDouble(&ok);
            if (ok && std::isfinite(value) && value >= 0 && value <= 100)
                result[pid] = std::max(result.value(pid), value);
        }
    }
    return result;
}

Collector::Collector(Paths paths) : m_paths(std::move(paths)) {
    if (m_paths.desktopDirs.isEmpty())
        for (const auto &path : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
            m_paths.desktopDirs.append(path + "/applications");
}
Reading Collector::sample(bool applications) {
    Reading result;
    result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    result.ticksPerSecond = ::sysconf(_SC_CLK_TCK);
    result.cpu = parseCpu(read(m_paths.proc + "/stat"));
    QMap<QByteArray, quint64> memory;
    for (const auto &line : read(m_paths.proc + "/meminfo").split('\n')) {
        const auto fields = words(line); if (fields.size() < 2) continue;
        bool ok; const auto value = fields[1].toULongLong(&ok); if (ok) memory[fields[0]] = value * 1024;
    }
    if (memory.contains("MemTotal:") && memory.contains("MemAvailable:") && memory["MemTotal:"] >= memory["MemAvailable:"]) {
        result.memoryTotal = memory["MemTotal:"]; result.memoryUsed = *result.memoryTotal - memory["MemAvailable:"];
    }
    QMap<QString, Counters> allNetwork, physicalNetwork;
    for (const auto &line : read(m_paths.proc + "/net/dev").split('\n')) {
        const int colon = line.indexOf(':'); if (colon < 0) continue;
        const auto name = QString::fromLatin1(line.left(colon).trimmed());
        if (name == "lo") continue;
        const auto value = pair(words(line.mid(colon + 1)), 8, 0); if (!value) continue;
        allNetwork[name] = *value;
        if (QFileInfo::exists(m_paths.sys + "/class/net/" + name + "/device")) physicalNetwork[name] = *value;
    }
    // Count physical links once, without counting the same packet again in a VPN/bridge.
    result.network = physicalNetwork.isEmpty() ? allNetwork : physicalNetwork;
    for (const auto &name : QDir(m_paths.sys + "/block").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto path = m_paths.sys + "/block/" + name;
        if (!QFileInfo::exists(path + "/device")) continue; // excludes partitions, dm, loop and zram
        if (auto value = pair(words(read(path + "/stat")), 2, 6))
            result.disks[name] = {value->first * 512, value->second * 512};
    }
    for (const auto &card : QDir(m_paths.sys + "/class/drm").entryList({"card*"}, QDir::Dirs)) {
        bool ok; const auto busy = read(m_paths.sys + "/class/drm/" + card + "/device/gpu_busy_percent").trimmed().toDouble(&ok);
        if (ok && busy >= 0 && busy <= 100) result.gpu = std::max(result.gpu.value_or(0), busy);
    }
    if (m_paths.helpers) {
        const auto output = query(QStandardPaths::findExecutable("nvidia-smi"),
            {"--query-gpu=utilization.gpu", "--format=csv,noheader,nounits"}, 1500);
        for (const auto &line : output.split('\n')) {
            bool ok; const auto value = line.trimmed().toDouble(&ok);
            if (ok && value >= 0 && value <= 100) result.gpu = std::max(result.gpu.value_or(0), value);
        }
    }
    result.applications = applications;
    if (applications) {
        collectApplications(result);
        if (m_paths.helpers) {
            const auto usage = parseNvidiaProcesses(query(QStandardPaths::findExecutable("nvidia-smi"),
                {"pmon", "-c", "1", "-s", "u"}, 2200));
            for (auto it = usage.cbegin(); it != usage.cend(); ++it)
                if (result.processes.contains(it.key())) result.processes[it.key()].gpuPercent = it.value();
        }
    }
    return result;
}

void Collector::loadDesktopEntries() {
    m_catalogLoaded = true; QSet<QString> seen;
    for (const auto &directory : m_paths.desktopDirs) {
        QDirIterator it(directory, {"*.desktop"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            QString id = QDir(directory).relativeFilePath(path); id.replace('/', '-');
            if (seen.contains(id)) continue;
            seen.insert(id);
            QMap<QString, QString> values; bool inEntry = false;
            for (const auto &raw : read(path).split('\n')) {
                const auto line = QString::fromUtf8(raw).trimmed();
                if (line.startsWith('[')) { inEntry = line == "[Desktop Entry]"; continue; }
                const int equal = line.indexOf('=');
                if (inEntry && equal > 0 && !line.startsWith('#')) values[line.left(equal)] = line.mid(equal + 1);
            }
            if (values.value("Hidden") == "true" || values.value("Type") != "Application") continue;
            const QString locale = QLocale().name();
            const auto name = values.value("Name[" + locale + "]", values.value("Name[" + locale.section('_', 0, 0) + "]", values.value("Name")));
            const auto command = QProcess::splitCommand(values.value("Exec"));
            QString binary = command.isEmpty() ? QString() : QFileInfo(command.first()).fileName();
            if (binary == "env") {
                binary.clear();
                for (int i = 1; i < command.size(); ++i)
                    if (!command[i].contains('=') && !command[i].startsWith('-')) { binary = QFileInfo(command[i]).fileName(); break; }
            }
            // Shared interpreters/launchers cannot identify an app on their own.
            if (QStringList{"bash", "sh", "python", "python3", "java", "electron", "flatpak", "uwsm"}.contains(binary)) binary.clear();
            if (!name.isEmpty()) m_desktops.append({id, name, values.value("Icon"), binary, values.value("StartupWMClass")});
        }
    }
}
void Collector::collectApplications(Reading &reading) {
    if (!m_catalogLoaded) loadDesktopEntries();
    QMap<int, QString> seeds;
    QMap<QString, App> identities;
    const long pageSize = ::sysconf(_SC_PAGESIZE);
    for (const auto &name : QDir(m_paths.proc).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool pidOk; const int pid = name.toInt(&pidOk); if (!pidOk || pid <= 0) continue;
        const auto path = m_paths.proc + '/' + name;
        if (QFileInfo(path).ownerId() != ::getuid()) continue;
        auto process = parseProcess(read(path + "/stat"), pageSize); if (!process) continue;
        process->executable = QFileInfo(path + "/exe").symLinkTarget();
        for (const auto &descriptor : QDir(path + "/fdinfo").entryList(QDir::Files))
            parseDrm(read(path + "/fdinfo/" + descriptor), *process);
        reading.processes[pid] = *process;
        const auto binary = QFileInfo(process->executable).fileName();
        for (const auto &desktop : m_desktops) {
            if (!desktop.binary.isEmpty() && binary == desktop.binary) {
                seeds[pid] = desktop.id;
                identities[desktop.id] = {desktop.id, desktop.name, desktop.icon, {}};
                break;
            }
        }
        if (binary == "HyprEndfield") {
            seeds[pid] = "HyprEndfield"; identities["HyprEndfield"] = {"HyprEndfield", "HyprEndfield", {}, {}};
        }
    }
    // Window classes name the desktop entry a process belongs to, which the
    // binary name alone cannot do (Electron apps, browsers, Flatpak). Each
    // compositor reports its own shape; both are reduced here to pid + app id.
    if (m_paths.helpers) {
        for (const auto &window : compositorWindows()) {
            const int pid = window.first;
            if (!reading.processes.contains(pid)) continue;
            const QString appClass = window.second;
            if (appClass.isEmpty()) continue;
            for (const auto &desktop : m_desktops) {
                if (desktop.windowClass.compare(appClass, Qt::CaseInsensitive) == 0 || desktop.id.compare(appClass + ".desktop", Qt::CaseInsensitive) == 0) {
                    seeds[pid] = desktop.id; identities[desktop.id] = {desktop.id, desktop.name, desktop.icon, {}}; break;
                }
            }
            if (!seeds.contains(pid)) {
                const QString id = "window:" + appClass.toLower();
                seeds[pid] = id; identities[id] = {id, appClass, {}, {}};
            }
        }
    }
    for (auto it = reading.processes.cbegin(); it != reading.processes.cend(); ++it) {
        int ancestor = it.key(); QSet<int> visited;
        while (ancestor > 0 && !visited.contains(ancestor) && !seeds.contains(ancestor)) {
            visited.insert(ancestor);
            if (!reading.processes.contains(ancestor)) { ancestor = 0; break; }
            ancestor = reading.processes[ancestor].parent;
        }
        if (seeds.contains(ancestor)) {
            const auto id = seeds[ancestor];
            if (!reading.apps.contains(id)) reading.apps[id] = identities[id];
            reading.apps[id].pids.append(it.key());
        }
    }
}

QVariantMap derive(const Reading &now, const Reading *previous) {
    const double elapsed = previous ? now.seconds - previous->seconds : 0;
    std::optional<double> cpu;
    if (elapsed > 0 && elapsed <= 15 && now.cpu && previous->cpu) {
        const auto total = difference(now.cpu->first, previous->cpu->first, 1), idle = difference(now.cpu->second, previous->cpu->second, 1);
        if (total && idle && *total > 0 && *idle <= *total) cpu = 100 * (*total - *idle) / *total;
    }
    const auto network = previous ? rates(now.network, previous->network, elapsed) : QList<QVariant>{{}, {}};
    const auto disks = previous ? rates(now.disks, previous->disks, elapsed) : QList<QVariant>{{}, {}};
    return {{"gpu", number(now.gpu)}, {"cpu", number(cpu)}, {"memoryUsed", number(now.memoryUsed)}, {"memoryTotal", number(now.memoryTotal)},
        {"upload", network[0]}, {"download", network[1]}, {"read", disks[0]}, {"write", disks[1]}};
}
QVariantList deriveApps(const Reading &now, const Reading *previous) {
    QVariantList result;
    const double elapsed = previous ? now.seconds - previous->seconds : 0;
    for (const auto &app : now.apps) {
        bool complete = !app.pids.isEmpty(); quint64 memory = 0, ticks = 0; int threads = 0;
        bool baseline = previous && previous->applications && elapsed > 0 && elapsed <= 5 && now.ticksPerSecond > 0
            && previous->apps.contains(app.id) && app.pids == previous->apps[app.id].pids;
        for (int pid : app.pids) {
            if (!now.processes.contains(pid)) { complete = false; baseline = false; continue; }
            const auto &process = now.processes[pid]; memory += process.memory; threads += process.threads;
            if (baseline) {
                const auto old = previous->processes.constFind(pid);
                if (old == previous->processes.cend() || old->start != process.start || old->ticks > process.ticks) baseline = false;
                else ticks += process.ticks - old->ticks;
            }
        }
        QMap<QString, double> engines; QSet<QString> clients;
        double nvidia = 0; bool gpuKnown = false;
        for (int pid : app.pids) {
            const auto process = now.processes.constFind(pid); if (process == now.processes.cend()) continue;
            if (process->gpuPercent) { nvidia += *process->gpuPercent; gpuKnown = true; }
            if (!previous || elapsed <= 0 || elapsed > 15) continue;
            const auto old = previous->processes.constFind(pid);
            if (old == previous->processes.cend() || old->start != process->start) continue;
            for (auto it = process->gpuEngines.cbegin(); it != process->gpuEngines.cend(); ++it) {
                if (clients.contains(it.key()) || !old->gpuEngines.contains(it.key()) || it.value() < old->gpuEngines[it.key()]) continue;
                clients.insert(it.key()); gpuKnown = true;
                const QString engine = it.key().section('/', 0, 0) + '/' + it.key().section('/', 2);
                engines[engine] += double(it.value() - old->gpuEngines[it.key()]) / (elapsed * 1e9)
                    * 100 / process->gpuCapacities.value(it.key(), 1);
            }
        }
        double gpu = nvidia; for (double value : engines) gpu = std::max(gpu, value);
        result.append(QVariantMap{{"id", app.id}, {"name", app.name}, {"icon", app.icon}, {"gpu", gpuKnown ? QVariant(std::clamp(gpu, 0., 100.)) : QVariant()},
            {"cpu", baseline && complete ? QVariant(double(ticks) / now.ticksPerSecond / elapsed * 100) : QVariant()},
            {"memory", complete ? QVariant::fromValue(memory) : QVariant()}, {"threads", complete ? QVariant(threads) : QVariant()}});
    }
    return result;
}
QVariantList sortedApps(QVariantList values, const QString &key, bool descending) {
    // Qt's C-locale collator ignores numericMode; preserve natural number order there too.
    const QLocale locale;
    QCollator collator(locale.language() == QLocale::C ? QLocale(QLocale::English, QLocale::UnitedStates) : locale);
    collator.setNumericMode(true); collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(values.begin(), values.end(), [&](const QVariant &a, const QVariant &b) {
        const auto left = a.toMap(), right = b.toMap();
        if (key != "name") {
            const auto x = left.value(key), y = right.value(key);
            const bool validX = x.isValid() && !x.isNull(), validY = y.isValid() && !y.isNull();
            if (validX != validY) return validX;
            if (validX && x.toDouble() != y.toDouble()) return descending ? x.toDouble() > y.toDouble() : x.toDouble() < y.toDouble();
        }
        const int names = collator.compare(left.value("name").toString(), right.value("name").toString());
        if (names) return key == "name" && descending ? names > 0 : names < 0;
        return left.value("id").toString() < right.value("id").toString();
    });
    return values;
}
}
