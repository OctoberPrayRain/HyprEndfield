#include "terminalview.hpp"
#include <QClipboard>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace {
VTermModifier modifiers(Qt::KeyboardModifiers value)
{
    return VTermModifier((value.testFlag(Qt::ShiftModifier) ? VTERM_MOD_SHIFT : 0)
        | (value.testFlag(Qt::AltModifier) ? VTERM_MOD_ALT : 0)
        | (value.testFlag(Qt::ControlModifier) ? VTERM_MOD_CTRL : 0));
}
QString cellText(const VTermScreenCell &cell)
{
    QString result;
    for (uint32_t code : cell.chars) {
        if (code == 0 || code == UINT32_MAX) break;
        const char32_t character = code;
        result += QString::fromUcs4(&character, 1);
    }
    return result;
}
QColor color(VTermColor value) { return QColor(value.rgb.red, value.rgb.green, value.rgb.blue); }
bool hudShortcut(QKeyEvent *event) { return event->key() == Qt::Key_Escape && event->modifiers().testFlag(Qt::MetaModifier); }
}

TerminalView::TerminalView(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setFlag(ItemAcceptsInputMethod, true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton | Qt::RightButton);
    setClip(true); setAntialiasing(true); setOpaquePainting(true);
    // 优先 Nerd Font（zsh/p10k 的图标需要），缺失时按顺序回退到已安装等宽字体。
    m_font = QFont();
    m_font.setFamilies({QStringLiteral("JetBrainsMono Nerd Font Mono"),
                        QStringLiteral("JetBrainsMono Nerd Font"),
                        QStringLiteral("MesloLGS NF"),
                        QStringLiteral("Symbols Nerd Font Mono"),
                        QStringLiteral("Noto Sans Mono")});
    m_font.setStyleHint(QFont::Monospace); m_font.setPixelSize(13);
    const QFontMetricsF metrics(m_font);
    m_cellWidth = metrics.horizontalAdvance(QLatin1Char('M'));
    m_cellHeight = std::ceil(metrics.height()) + 2; m_ascent = metrics.ascent();
}
void TerminalView::setSession(TerminalSession *session)
{
    if (m_session == session) return;
    if (m_session) disconnect(m_session, nullptr, this, nullptr);
    m_session = session; m_scrollOffset = 0; m_scrollSerial = 0;
    if (session) connect(session, &TerminalSession::screenChanged, this, [this] {
        if (m_scrollOffset > 0) m_scrollOffset += int(m_session->scrollSerial() - m_scrollSerial);
        m_scrollSerial = m_session->scrollSerial();
        m_selectionStart = m_selectionEnd = -1;
        refresh();
    });
    resizeTerminal(); refresh(); emit sessionChanged();
}
void TerminalView::setRenderScale(qreal scale)
{
    scale = std::clamp(scale, 1.0, 4.0);
    if (qFuzzyCompare(scale, m_renderScale)) return;
    m_renderScale = scale; resizeTerminal(); emit renderScaleChanged();
}
void TerminalView::geometryChange(const QRectF &geometry, const QRectF &old)
{
    QQuickPaintedItem::geometryChange(geometry, old); resizeTerminal();
}
void TerminalView::resizeTerminal()
{
    m_columns = std::clamp(int(width() / m_cellWidth), 2, 500);
    m_rows = std::clamp(int(height() / m_cellHeight), 2, 300);
    setTextureSize(QSize(std::max(1, int(std::ceil(width() * m_renderScale))),
                         std::max(1, int(std::ceil(height() * m_renderScale)))));
    if (m_session) m_session->resize(m_rows, m_columns);
    refresh();
}
void TerminalView::refresh()
{
    m_cells.clear(); m_cursorVisible = false;
    if (m_session) {
        m_scrollOffset = m_session->alternateScreen() ? 0 : std::clamp(m_scrollOffset, 0, m_session->historySize());
        m_topRow = m_session->historySize() - m_scrollOffset;
        m_cursor = m_session->cursor();
        m_cursorVisible = m_session->cursorVisible() && m_scrollOffset == 0;
        m_cursorShape = m_session->cursorShape();
        m_cells.reserve(m_rows * m_columns);
        for (int row = 0; row < m_rows; ++row) {
            for (int column = 0; column < m_columns; ++column) {
                const auto cell = m_session->cell(m_topRow + row, column);
                m_cells.append({cellText(cell), color(m_session->rgb(cell.fg)),
                    color(m_session->rgb(cell.bg)), cell.attrs, cell.chars[0] == UINT32_MAX ? 0 : int(cell.width)});
            }
        }
    }
    if (hasActiveFocus()) QGuiApplication::inputMethod()->update(Qt::ImCursorRectangle);
    update();
}
QRectF TerminalView::cursorRect() const
{
    return {m_cursor.col * m_cellWidth, m_cursor.row * m_cellHeight, m_cellWidth, m_cellHeight};
}
void TerminalView::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), QColor("#171717"));
    painter->setRenderHint(QPainter::TextAntialiasing);
    const int begin = std::min(m_selectionStart, m_selectionEnd), end = std::max(m_selectionStart, m_selectionEnd);
    for (int row = 0; row < m_rows; ++row) {
        for (int column = 0; column < m_columns; ++column) {
            const int index = row * m_columns + column;
            if (index >= m_cells.size()) continue;
            const auto &cell = m_cells[index];
            if (cell.width == 0) continue;
            const QRectF rect(column * m_cellWidth, row * m_cellHeight, std::max(1, cell.width) * m_cellWidth, m_cellHeight);
            QColor fg = cell.foreground, bg = cell.background;
            if (cell.attributes.reverse) std::swap(fg, bg);
            const int absolute = (m_topRow + row) * m_columns + column;
            if (begin >= 0 && absolute >= begin && absolute <= end) { bg = QColor("#686530"); fg = QColor("#ffffff"); }
            painter->fillRect(rect, bg);
            if (!cell.attributes.conceal && !cell.text.isEmpty()) {
                QFont font = m_font;
                font.setBold(cell.attributes.bold); font.setItalic(cell.attributes.italic);
                font.setUnderline(cell.attributes.underline != 0); font.setStrikeOut(cell.attributes.strike);
                painter->setFont(font); painter->setPen(fg);
                painter->drawText(QPointF(rect.x(), rect.y() + m_ascent + 1), cell.text);
            }
        }
    }
    if (m_cursorVisible) {
        QRectF rect = cursorRect();
        painter->setPen(QColor("#FFFA00"));
        if (!hasActiveFocus()) painter->drawRect(rect.adjusted(0.5, 0.5, -0.5, -0.5));
        else if (m_cursorShape == VTERM_PROP_CURSORSHAPE_UNDERLINE) painter->fillRect(QRectF(rect.x(), rect.bottom() - 2, rect.width(), 2), QColor("#FFFA00"));
        else if (m_cursorShape == VTERM_PROP_CURSORSHAPE_BAR_LEFT) painter->fillRect(QRectF(rect.x(), rect.y(), 2, rect.height()), QColor("#FFFA00"));
        else painter->fillRect(rect, QColor(255, 250, 0, 105));
        if (!m_preedit.isEmpty()) {
            const qreal textWidth = QFontMetricsF(m_font).horizontalAdvance(m_preedit);
            rect.setWidth(textWidth); painter->fillRect(rect, QColor("#343434"));
            painter->setFont(m_font); painter->drawText(rect.topLeft() + QPointF(0, m_ascent + 1), m_preedit);
            painter->drawLine(rect.bottomLeft(), rect.bottomRight());
        }
    }
}
void TerminalView::activate() { forceActiveFocus(Qt::OtherFocusReason); }
void TerminalView::deactivate() { setFocus(false); m_preedit.clear(); update(); }
void TerminalView::focusInEvent(QFocusEvent *event)
{
    QQuickPaintedItem::focusInEvent(event); if (m_session) m_session->focus(true); update();
}
void TerminalView::focusOutEvent(QFocusEvent *event)
{
    QQuickPaintedItem::focusOutEvent(event); if (m_session) m_session->focus(false); m_preedit.clear(); update();
}
bool TerminalView::event(QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride && hasActiveFocus() && !hudShortcut(static_cast<QKeyEvent *>(event))) {
        event->accept(); return true;
    }
    return QQuickPaintedItem::event(event);
}
void TerminalView::keyPressEvent(QKeyEvent *event)
{
    if (!m_session || hudShortcut(event)) { event->ignore(); return; }
    // Modifier presses arrive before the C/V key. Keep the mouse selection
    // intact while the user holds Ctrl+Shift to copy it.
    switch (event->key()) {
    case Qt::Key_Control: case Qt::Key_Shift: case Qt::Key_Alt:
    case Qt::Key_Meta: case Qt::Key_AltGr: case Qt::Key_CapsLock:
        event->accept(); return;
    default: break;
    }
    const auto mods = event->modifiers();
    if (mods == (Qt::ControlModifier | Qt::ShiftModifier)) {
        if (event->key() == Qt::Key_C) { copySelection(); event->accept(); return; }
        if (event->key() == Qt::Key_V) { pasteClipboard(); event->accept(); return; }
    }
    if (mods == Qt::ShiftModifier && (event->key() == Qt::Key_PageUp || event->key() == Qt::Key_PageDown)) {
        scroll(event->key() == Qt::Key_PageUp ? m_rows - 1 : 1 - m_rows); event->accept(); return;
    }
    if (!m_session->running() && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        m_session->startDefault(); event->accept(); return;
    }
    m_scrollOffset = 0; m_selectionStart = m_selectionEnd = -1;
    VTermKey key = VTERM_KEY_NONE;
    switch (event->key()) {
    case Qt::Key_Return: case Qt::Key_Enter: key = VTERM_KEY_ENTER; break;
    case Qt::Key_Tab: case Qt::Key_Backtab: key = VTERM_KEY_TAB; break;
    case Qt::Key_Backspace: key = VTERM_KEY_BACKSPACE; break;
    case Qt::Key_Escape: key = VTERM_KEY_ESCAPE; break;
    case Qt::Key_Up: key = VTERM_KEY_UP; break;
    case Qt::Key_Down: key = VTERM_KEY_DOWN; break;
    case Qt::Key_Left: key = VTERM_KEY_LEFT; break;
    case Qt::Key_Right: key = VTERM_KEY_RIGHT; break;
    case Qt::Key_Insert: key = VTERM_KEY_INS; break;
    case Qt::Key_Delete: key = VTERM_KEY_DEL; break;
    case Qt::Key_Home: key = VTERM_KEY_HOME; break;
    case Qt::Key_End: key = VTERM_KEY_END; break;
    case Qt::Key_PageUp: key = VTERM_KEY_PAGEUP; break;
    case Qt::Key_PageDown: key = VTERM_KEY_PAGEDOWN; break;
    default:
        if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F35) key = VTermKey(VTERM_KEY_FUNCTION(event->key() - Qt::Key_F1 + 1));
    }
    if (key != VTERM_KEY_NONE) m_session->key(key, modifiers(mods));
    else if (mods.testFlag(Qt::ControlModifier) && event->key() >= Qt::Key_A && event->key() <= Qt::Key_Z)
        m_session->character(U'a' + event->key() - Qt::Key_A, modifiers(mods & ~Qt::ShiftModifier));
    else if (mods.testFlag(Qt::ControlModifier) && event->key() == Qt::Key_Space) m_session->character(U' ', modifiers(mods));
    else for (char32_t value : event->text().toUcs4()) m_session->character(value, modifiers(mods & ~Qt::ShiftModifier));
    refresh(); event->accept();
}
void TerminalView::inputMethodEvent(QInputMethodEvent *event)
{
    if (m_session) {
        m_scrollOffset = 0;
        for (char32_t value : event->commitString().toUcs4()) m_session->character(value, VTERM_MOD_NONE);
    }
    m_preedit = event->preeditString(); refresh(); event->accept();
}
QVariant TerminalView::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImEnabled: return isEnabled() && hasActiveFocus();
    case Qt::ImCursorRectangle: return cursorRect();
    case Qt::ImFont: return m_font;
    case Qt::ImHints: return int(Qt::ImhNoPredictiveText);
    case Qt::ImSurroundingText: case Qt::ImCurrentSelection: return QString();
    case Qt::ImCursorPosition: case Qt::ImAnchorPosition: return 0;
    default: return QQuickPaintedItem::inputMethodQuery(query);
    }
}
QPoint TerminalView::cellAt(QPointF position) const
{
    return {std::clamp(int(position.x() / m_cellWidth), 0, m_columns - 1),
            std::clamp(int(position.y() / m_cellHeight), 0, m_rows - 1)};
}
int TerminalView::selectionPosition(QPointF position) const
{
    const auto point = cellAt(position); return (m_topRow + point.y()) * m_columns + point.x();
}
bool TerminalView::reportMouse(QMouseEvent *event, bool pressed, bool release)
{
    if (!m_session || !m_session->mouseReporting() || event->modifiers().testFlag(Qt::ShiftModifier)) return false;
    const auto point = cellAt(event->position());
    m_session->mouseMove(point.y(), point.x(), modifiers(event->modifiers()));
    const int button = event->button() == Qt::LeftButton ? 1 : event->button() == Qt::MiddleButton ? 2 : 3;
    if (pressed || release) m_session->mouseButton(button, pressed, modifiers(event->modifiers()));
    return true;
}
void TerminalView::mousePressEvent(QMouseEvent *event)
{
    activate();
    if (!reportMouse(event, true) && event->button() == Qt::LeftButton) {
        m_selecting = true; m_selectionStart = m_selectionEnd = selectionPosition(event->position()); update();
    }
    event->accept();
}
void TerminalView::mouseMoveEvent(QMouseEvent *event)
{
    if (!reportMouse(event, false) && m_selecting) { m_selectionEnd = selectionPosition(event->position()); update(); }
    event->accept();
}
void TerminalView::mouseReleaseEvent(QMouseEvent *event)
{
    reportMouse(event, false, true); m_selecting = false; event->accept();
}
void TerminalView::scroll(int rows)
{
    m_scrollOffset += rows; refresh();
}
void TerminalView::wheelEvent(QWheelEvent *event)
{
    if (m_session && m_session->mouseReporting() && !event->modifiers().testFlag(Qt::ShiftModifier)) {
        const auto point = cellAt(event->position()); const int button = event->angleDelta().y() > 0 ? 4 : 5;
        m_session->mouseMove(point.y(), point.x(), modifiers(event->modifiers()));
        for (int count = 0; count < std::max(1, std::abs(event->angleDelta().y()) / 120); ++count) {
            m_session->mouseButton(button, true, modifiers(event->modifiers()));
            m_session->mouseButton(button, false, modifiers(event->modifiers()));
        }
    } else scroll(event->angleDelta().y() / 40);
    event->accept();
}
void TerminalView::copySelection()
{
    if (!m_session || m_selectionStart < 0 || m_selectionEnd < 0) return;
    const int begin = std::min(m_selectionStart, m_selectionEnd), end = std::max(m_selectionStart, m_selectionEnd);
    QString text;
    for (int row = begin / m_columns; row <= end / m_columns; ++row) {
        QString line;
        for (int col = row == begin / m_columns ? begin % m_columns : 0;
             col <= (row == end / m_columns ? end % m_columns : m_columns - 1); ++col) {
            const auto cell = m_session->cell(row, col);
            if (cell.width == 0 || cell.chars[0] == UINT32_MAX) continue;
            const QString value = cellText(cell); line += value.isEmpty() ? QStringLiteral(" ") : value;
        }
        while (line.endsWith(' ')) line.chop(1);
        if (row > begin / m_columns) text += '\n';
        text += line;
    }
    QGuiApplication::clipboard()->setText(text);
}
void TerminalView::pasteClipboard()
{
    if (!m_session) return;
    m_scrollOffset = 0; m_session->paste(QGuiApplication::clipboard()->text()); refresh();
}
