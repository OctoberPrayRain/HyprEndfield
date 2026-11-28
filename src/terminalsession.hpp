#pragma once

#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSocketNotifier>
#include <QVector>
#include <deque>
#include <vterm.h>

// One real PTY + Bash session. Hiding its view does not stop the child process.
// libvterm owns terminal parsing, alternate screen, key encoding and colours.
class TerminalSession final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY statusChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
public:
    explicit TerminalSession(QObject *parent = nullptr);
    ~TerminalSession() override;
    bool running() const { return m_process.state() != QProcess::NotRunning; }
    QString status() const { return m_status; }
    int rows() const { return m_rows; }
    int columns() const { return m_columns; }
    int historySize() const { return int(m_history.size()); }
    qint64 scrollSerial() const { return m_scrollSerial; }
    bool alternateScreen() const { return m_alternate; }
    bool cursorVisible() const { return m_cursorVisible; }
    int cursorShape() const { return m_cursorShape; }
    bool mouseReporting() const { return m_mouseMode != VTERM_PROP_MOUSE_NONE; }
    VTermPos cursor() const;
    VTermScreenCell cell(int absoluteRow, int column) const;
    VTermColor rgb(VTermColor color) const;
    QString screenText() const;
    Q_INVOKABLE void startDefault();
    void start(const QString &program, const QStringList &arguments, const QString &directory,
               const QProcessEnvironment &environment = QProcessEnvironment::systemEnvironment());
    void resize(int rows, int columns);
    void key(VTermKey key, VTermModifier modifiers);
    void character(char32_t character, VTermModifier modifiers);
    void paste(const QString &text);
    void focus(bool focused);
    void mouseMove(int row, int column, VTermModifier modifiers);
    void mouseButton(int button, bool pressed, VTermModifier modifiers);
    // Also used by parser tests; does not write to a process.
    void receive(const QByteArray &bytes);
signals:
    void screenChanged();
    void statusChanged();
    void outputWritten(const QByteArray &bytes);
private:
    void readPty();
    void writePty(const QByteArray &bytes);
    void flushWrite();
    void closePty();
    void closeSlave();
    void setStatus(const QString &status);
    static void output(const char *, size_t, void *);
    static int damage(VTermRect, void *);
    static int moveCursor(VTermPos, VTermPos, int, void *);
    static int termProperty(VTermProp, VTermValue *, void *);
    static int pushLine(int, const VTermScreenCell *, void *);
    static int popLine(int, VTermScreenCell *, void *);
    static int clearHistory(void *);
    VTerm *m_terminal = nullptr;
    VTermScreen *m_screen = nullptr;
    QProcess m_process;
    QSocketNotifier *m_reader = nullptr;
    QSocketNotifier *m_writer = nullptr;
    int m_master = -1;
    int m_slave = -1;
    int m_rows = 24, m_columns = 80;
    bool m_alternate = false, m_cursorVisible = true;
    QString m_shellLabel;
    int m_cursorShape = VTERM_PROP_CURSORSHAPE_BLOCK;
    int m_mouseMode = VTERM_PROP_MOUSE_NONE;
    qint64 m_scrollSerial = 0;
    QString m_status;
    QByteArray m_pendingWrite;
    std::deque<QVector<VTermScreenCell>> m_history;
};
