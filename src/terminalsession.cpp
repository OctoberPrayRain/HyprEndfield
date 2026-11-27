#include <QCoreApplication>
#include "terminalsession.hpp"

#include <QDir>
#include <QFileInfo>
#include <pwd.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
constexpr int historyLimit = 2000;

// chsh 修改的登录 shell 就是 /etc/passwd 里的 pw_shell；$SHELL 作为回退。
QString loginShell() {
    if (const auto *pw = getpwuid(getuid()); pw && pw->pw_shell && *pw->pw_shell) {
        const QFileInfo info(QString::fromLocal8Bit(pw->pw_shell));
        if (info.isExecutable()) return info.absoluteFilePath();
    }
    const auto environment = qEnvironmentVariable("SHELL");
    if (!environment.isEmpty()) {
        const QFileInfo info(environment);
        if (info.isExecutable()) return info.absoluteFilePath();
    }
    return QStringLiteral("/bin/sh");
}
}

TerminalSession::TerminalSession(QObject *parent) : QObject(parent)
{
    VTERM_CHECK_VERSION;
    m_terminal = vterm_new(m_rows, m_columns);
    vterm_set_utf8(m_terminal, 1);
    m_screen = vterm_obtain_screen(m_terminal);
    vterm_screen_enable_altscreen(m_screen, 1);
    vterm_screen_enable_reflow(m_screen, true);
    static const VTermScreenCallbacks callbacks{
        .damage = damage, .moverect = nullptr, .movecursor = moveCursor,
        .settermprop = termProperty, .bell = nullptr, .resize = nullptr,
        .sb_pushline = pushLine, .sb_popline = popLine, .sb_clear = clearHistory
    };
    vterm_screen_set_callbacks(m_screen, &callbacks, this);
    vterm_output_set_callback(m_terminal, output, this);
    vterm_screen_set_damage_merge(m_screen, VTERM_DAMAGE_SCREEN);
    VTermColor foreground, background;
    vterm_color_rgb(&foreground, 230, 230, 230);
    vterm_color_rgb(&background, 23, 23, 23);
    vterm_screen_set_default_colors(m_screen, &foreground, &background);
    vterm_screen_reset(m_screen, 1);
    connect(&m_process, &QProcess::started, this, [this] {
        closeSlave();
        setStatus(m_shellLabel.isEmpty() ? QStringLiteral("SHELL") : m_shellLabel);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            closePty(); setStatus(QCoreApplication::translate("HyprEndfield", "终端启动失败：") + m_process.errorString());
        }
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        readPty(); closePty();
        setStatus(QCoreApplication::translate("HyprEndfield", "会话已结束（%1） · Enter 重新启动").arg(code));
    });
}

TerminalSession::~TerminalSession()
{
    disconnect(&m_process, nullptr, this, nullptr);
    const auto child = m_process.processId();
    closePty(); // PTY hangup reaches the foreground process group.
    if (child > 0 && running()) {
        ::kill(-pid_t(child), SIGHUP);
        m_process.terminate();
        if (!m_process.waitForFinished(500)) {
            ::kill(-pid_t(child), SIGKILL);
            m_process.kill(); m_process.waitForFinished(500);
        }
    }
    vterm_free(m_terminal);
}

void TerminalSession::startDefault()
{
    const auto shell = loginShell();
    m_shellLabel = QFileInfo(shell).fileName().toUpper();
    start(shell, {QStringLiteral("-i")}, QDir::homePath());
}
void TerminalSession::start(const QString &program, const QStringList &arguments, const QString &directory,
                            const QProcessEnvironment &environment)
{
    if (running()) return;
    closePty();
    m_history.clear(); m_scrollSerial = 0; m_alternate = false; m_mouseMode = VTERM_PROP_MOUSE_NONE;
    vterm_screen_reset(m_screen, 1);
    struct winsize size{};
    size.ws_row = m_rows; size.ws_col = m_columns;
    if (::openpty(&m_master, &m_slave, nullptr, nullptr, &size) < 0) {
        setStatus(QCoreApplication::translate("HyprEndfield", "无法创建终端：") + QString::fromLocal8Bit(std::strerror(errno))); return;
    }
    ::fcntl(m_master, F_SETFD, FD_CLOEXEC); ::fcntl(m_slave, F_SETFD, FD_CLOEXEC);
    ::fcntl(m_master, F_SETFL, ::fcntl(m_master, F_GETFL) | O_NONBLOCK);
    auto env = environment;
    env.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    env.insert(QStringLiteral("COLORTERM"), QStringLiteral("truecolor"));
    env.remove(QStringLiteral("COLUMNS")); env.remove(QStringLiteral("LINES")); env.remove(QStringLiteral("TERMCAP"));
    m_process.setProcessEnvironment(env);
    m_process.setWorkingDirectory(directory);
    const int master = m_master, slave = m_slave;
    m_process.setChildProcessModifier([this, master, slave] {
        // Child side: only async-signal-safe system calls before exec.
        if (::setsid() < 0 || ::ioctl(slave, TIOCSCTTY, 0) < 0)
            m_process.failChildProcessModifier("controlling terminal", errno);
        for (int fd = 0; fd <= 2; ++fd)
            if (::dup2(slave, fd) < 0) m_process.failChildProcessModifier("terminal stdio", errno);
        if (slave > 2) ::close(slave);
        if (master > 2) ::close(master);
    });
    m_reader = new QSocketNotifier(m_master, QSocketNotifier::Read, this);
    connect(m_reader, &QSocketNotifier::activated, this, &TerminalSession::readPty);
    m_writer = new QSocketNotifier(m_master, QSocketNotifier::Write, this);
    m_writer->setEnabled(false);
    connect(m_writer, &QSocketNotifier::activated, this, &TerminalSession::flushWrite);
    m_process.start(program, arguments);
    setStatus(QCoreApplication::translate("HyprEndfield", "正在启动…"));
    emit screenChanged();
}
void TerminalSession::closeSlave()
{
    if (m_slave >= 0) { ::close(m_slave); m_slave = -1; }
}
void TerminalSession::closePty()
{
    delete m_reader; m_reader = nullptr;
    delete m_writer; m_writer = nullptr;
    closeSlave();
    if (m_master >= 0) { ::close(m_master); m_master = -1; }
    m_pendingWrite.clear();
}
void TerminalSession::setStatus(const QString &status)
{
    m_status = status; emit statusChanged();
}
void TerminalSession::readPty()
{
    if (m_master < 0) return;
    QByteArray bytes;
    char buffer[16384];
    while (bytes.size() < 262144) {
        const auto count = ::read(m_master, buffer, sizeof(buffer));
        if (count > 0) { bytes.append(buffer, count); continue; }
        if (count < 0 && errno == EINTR) continue;
        if (count == 0 || (count < 0 && errno == EIO)) m_reader->setEnabled(false);
        break;
    }
    if (!bytes.isEmpty()) receive(bytes);
}
void TerminalSession::receive(const QByteArray &bytes)
{
    vterm_input_write(m_terminal, bytes.constData(), bytes.size());
    vterm_screen_flush_damage(m_screen);
    emit screenChanged();
}
void TerminalSession::output(const char *bytes, size_t count, void *data)
{
    static_cast<TerminalSession *>(data)->writePty(QByteArray(bytes, qsizetype(count)));
}
void TerminalSession::writePty(const QByteArray &bytes)
{
    emit outputWritten(bytes);
    if (m_master < 0) return;
    m_pendingWrite.append(bytes);
    flushWrite();
}
void TerminalSession::flushWrite()
{
    while (m_master >= 0 && !m_pendingWrite.isEmpty()) {
        const auto count = ::write(m_master, m_pendingWrite.constData(), m_pendingWrite.size());
        if (count > 0) { m_pendingWrite.remove(0, count); continue; }
        if (count < 0 && errno == EINTR) continue;
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        m_pendingWrite.clear(); break;
    }
    if (m_writer) m_writer->setEnabled(!m_pendingWrite.isEmpty());
}
void TerminalSession::resize(int rows, int columns)
{
    rows = std::clamp(rows, 2, 300); columns = std::clamp(columns, 2, 500);
    if (rows == m_rows && columns == m_columns) return;
    m_rows = rows; m_columns = columns;
    vterm_set_size(m_terminal, rows, columns);
    if (m_master >= 0) {
        struct winsize size{}; size.ws_row = rows; size.ws_col = columns;
        ::ioctl(m_master, TIOCSWINSZ, &size);
    }
    vterm_screen_flush_damage(m_screen);
    emit screenChanged();
}
void TerminalSession::key(VTermKey key, VTermModifier modifiers) { vterm_keyboard_key(m_terminal, key, modifiers); }
void TerminalSession::character(char32_t character, VTermModifier modifiers) { vterm_keyboard_unichar(m_terminal, character, modifiers); }
void TerminalSession::paste(const QString &text)
{
    // Escape cannot terminate bracketed paste through pasted control sequences.
    QString safe = text; safe.remove(QChar(0x1b)); safe.remove(QChar(0));
    safe.replace("\r\n", "\n"); safe.replace('\r', '\n'); safe.replace('\n', '\r');
    vterm_keyboard_start_paste(m_terminal);
    writePty(safe.toUtf8());
    vterm_keyboard_end_paste(m_terminal);
}
void TerminalSession::focus(bool focused)
{
    if (focused) vterm_state_focus_in(vterm_obtain_state(m_terminal));
    else vterm_state_focus_out(vterm_obtain_state(m_terminal));
}
void TerminalSession::mouseMove(int row, int column, VTermModifier modifiers) { vterm_mouse_move(m_terminal, row, column, modifiers); }
void TerminalSession::mouseButton(int button, bool pressed, VTermModifier modifiers) { vterm_mouse_button(m_terminal, button, pressed, modifiers); }
VTermPos TerminalSession::cursor() const
{
    VTermPos result{}; vterm_state_get_cursorpos(vterm_obtain_state(m_terminal), &result); return result;
}
VTermScreenCell TerminalSession::cell(int absoluteRow, int column) const
{
    VTermScreenCell result{};
    if (absoluteRow < 0 || column < 0) return result;
    if (absoluteRow < historySize()) {
        const auto &line = m_history[absoluteRow];
        return column < line.size() ? line[column] : result;
    }
    const int row = absoluteRow - historySize();
    if (row < m_rows && column < m_columns) vterm_screen_get_cell(m_screen, {row, column}, &result);
    return result;
}
VTermColor TerminalSession::rgb(VTermColor color) const { vterm_screen_convert_color_to_rgb(m_screen, &color); return color; }
QString TerminalSession::screenText() const
{
    QByteArray bytes(m_rows * m_columns * 24, '\0');
    const auto length = vterm_screen_get_text(m_screen, bytes.data(), bytes.size(), {0, m_rows, 0, m_columns});
    bytes.resize(qsizetype(length)); return QString::fromUtf8(bytes);
}
int TerminalSession::damage(VTermRect, void *) { return 1; }
int TerminalSession::moveCursor(VTermPos, VTermPos, int visible, void *data)
{
    static_cast<TerminalSession *>(data)->m_cursorVisible = visible; return 1;
}
int TerminalSession::termProperty(VTermProp prop, VTermValue *value, void *data)
{
    auto *self = static_cast<TerminalSession *>(data);
    switch (prop) {
    case VTERM_PROP_CURSORVISIBLE: self->m_cursorVisible = value->boolean; break;
    case VTERM_PROP_ALTSCREEN: self->m_alternate = value->boolean; break;
    case VTERM_PROP_CURSORSHAPE: self->m_cursorShape = value->number; break;
    case VTERM_PROP_MOUSE: self->m_mouseMode = value->number; break;
    default: break;
    }
    return 1;
}
int TerminalSession::pushLine(int cols, const VTermScreenCell *cells, void *data)
{
    auto *self = static_cast<TerminalSession *>(data);
    self->m_history.emplace_back(cells, cells + cols);
    if (self->historySize() > historyLimit) self->m_history.pop_front();
    ++self->m_scrollSerial;
    return 1;
}
int TerminalSession::popLine(int cols, VTermScreenCell *cells, void *data)
{
    auto *self = static_cast<TerminalSession *>(data);
    if (self->m_history.empty()) return 0;
    const auto line = std::move(self->m_history.back()); self->m_history.pop_back();
    std::fill(cells, cells + cols, VTermScreenCell{});
    std::copy_n(line.begin(), std::min(cols, int(line.size())), cells);
    --self->m_scrollSerial;
    return 1;
}
int TerminalSession::clearHistory(void *data)
{
    auto *self = static_cast<TerminalSession *>(data);
    self->m_history.clear(); self->m_scrollSerial = 0; return 1;
}
