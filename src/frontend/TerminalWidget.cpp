// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// Linux port: S4 Qt6 grid frontend. Owns a pty + engine instance
// (StateMachine + AdaptDispatch on a TextBuffer) and paints a
// conhost-style black grid with a block cursor.

#include "TerminalWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QSocketNotifier>
#include <QTimer>
#include <QWheelEvent>

#include <fcntl.h>
#include <unistd.h>

namespace
{
    DWORD ControlKeyStateFromKey(const QKeyEvent* event)
    {
        DWORD cks = 0;
        const auto mods = event->modifiers();
        if (mods & Qt::ShiftModifier)
        {
            cks |= SHIFT_PRESSED;
        }
        if (mods & Qt::ControlModifier)
        {
            cks |= CTRL_PRESSED;
        }
        if (mods & Qt::AltModifier)
        {
            cks |= ALT_PRESSED;
        }
        if (mods & Qt::KeypadModifier)
        {
            cks |= ENHANCED_KEY;
        }
        return cks;
    }

    // Console-format modifier flags (SHIFT/ALT/CTRL) from Qt modifiers.
    short ModifierKeyStateFromQt(const Qt::KeyboardModifiers mods)
    {
        short ks = 0;
        if (mods & Qt::ShiftModifier)
        {
            ks |= SHIFT_PRESSED;
        }
        if (mods & Qt::ControlModifier)
        {
            ks |= CTRL_PRESSED;
        }
        if (mods & Qt::AltModifier)
        {
            ks |= ALT_PRESSED;
        }
        return ks;
    }

    // Windows distinguishes the "extended key" cluster (physical cursor/nav
    // keys, 0xE0 prefix) from the keypad. The engine needs ENHANCED_KEY to
    // encode these as the main cursor sequences (e.g. under the Kitty
    // keyboard protocol, a non-enhanced VK_LEFT would become the keypad code
    // KP_Left 0xE049 instead of the plain Left arrow).
    bool IsExtendedKey(const WORD vk)
    {
        switch (vk)
        {
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
            return true;
        default:
            return false;
        }
    }

    // Maps a Qt::Key to a Windows virtual key code. Returns 0 for printable
    // character keys (letters, digits, punctuation, space) which are delivered
    // through their Unicode text instead. Crucially, we do NOT map printable
    // ASCII to its numeric value: '.'=0x2E would collide with VK_DELETE,
    // '\''=0x27 with VK_RIGHT, etc., making the engine emit the wrong sequence.
    WORD VkFromQtKey(const int qtKey)
    {
        switch (qtKey)
        {
        case Qt::Key_Escape: return VK_ESCAPE;
        case Qt::Key_Tab: return VK_TAB;
        case Qt::Key_Backtab: return VK_TAB;
        case Qt::Key_Backspace: return VK_BACK;
        case Qt::Key_Return: return VK_RETURN;
        case Qt::Key_Enter: return VK_RETURN;
        case Qt::Key_Insert: return VK_INSERT;
        case Qt::Key_Delete: return VK_DELETE;
        case Qt::Key_Pause: return VK_PAUSE;
        case Qt::Key_Home: return VK_HOME;
        case Qt::Key_End: return VK_END;
        case Qt::Key_Left: return VK_LEFT;
        case Qt::Key_Up: return VK_UP;
        case Qt::Key_Right: return VK_RIGHT;
        case Qt::Key_Down: return VK_DOWN;
        case Qt::Key_PageUp: return VK_PRIOR;
        case Qt::Key_PageDown: return VK_NEXT;
        case Qt::Key_Print: return VK_SNAPSHOT;
        case Qt::Key_CapsLock: return VK_CAPITAL;
        case Qt::Key_NumLock: return VK_NUMLOCK;
        case Qt::Key_ScrollLock: return VK_SCROLL;
        case Qt::Key_F1: return VK_F1;
        case Qt::Key_F2: return VK_F2;
        case Qt::Key_F3: return VK_F3;
        case Qt::Key_F4: return VK_F4;
        case Qt::Key_F5: return VK_F5;
        case Qt::Key_F6: return VK_F6;
        case Qt::Key_F7: return VK_F7;
        case Qt::Key_F8: return VK_F8;
        case Qt::Key_F9: return VK_F9;
        case Qt::Key_F10: return VK_F10;
        case Qt::Key_F11: return VK_F11;
        case Qt::Key_F12: return VK_F12;
        case Qt::Key_F13: return VK_F13;
        case Qt::Key_F14: return VK_F14;
        case Qt::Key_F15: return VK_F15;
        case Qt::Key_F16: return VK_F16;
        case Qt::Key_F17: return VK_F17;
        case Qt::Key_F18: return VK_F18;
        case Qt::Key_F19: return VK_F19;
        case Qt::Key_F20: return VK_F20;
        case Qt::Key_F21: return VK_F21;
        case Qt::Key_F22: return VK_F22;
        case Qt::Key_F23: return VK_F23;
        case Qt::Key_F24: return VK_F24;
        default: return 0;
        }
    }

    // Installed emoji and Nerd Font families, appended after the text font so
    // flag emoji / powerline glyphs render instead of tofu. The text font must
    // stay first so cell metrics are derived from it.
    QStringList fallbackFontFamilies()
    {
        const auto installed = QFontDatabase::families();
        QStringList result;

        static const QStringList emojiCandidates = {
            QStringLiteral("Noto Color Emoji"),
            QStringLiteral("Noto Emoji"),
            QStringLiteral("Symbola"),
            QStringLiteral("Apple Color Emoji"),
            QStringLiteral("Segoe UI Emoji"),
        };
        for (const auto& family : emojiCandidates)
        {
            if (installed.contains(family, Qt::CaseInsensitive))
            {
                result.push_back(family);
            }
        }

        // Nerd Fonts provide the Powerline/devicons glyphs shell prompts use.
        for (const auto& family : installed)
        {
            if (family.contains(QStringLiteral("Nerd Font"), Qt::CaseInsensitive))
            {
                result.push_back(family);
            }
        }

        return result;
    }
}

TerminalWidget::TerminalWidget() :
    QWidget(nullptr)
{
    // Engine responses and translated input both write to the pty.
    if (!_pty.Open())
    {
        qFatal("failed to open pty");
    }

    _api.SetWriteCallback([this](const std::string_view data) {
        _pty.Write(data);
    });
    _api.SetTitleCallback([this](const std::wstring_view title) {
        // Window title = "<program title> - openconsole_linux" (the app name follows the
        // program-set title). When no title is set, fall back to just "openconsole_linux".
        // Use window()->setWindowTitle so the top-level window (QMainWindow when
        // embedded) gets the title, not this central widget.
        const auto t = QString::fromWCharArray(title.data(), static_cast<int>(title.size()));
        const auto full = t.isEmpty() ? QStringLiteral("openconsole_linux") : t + QStringLiteral(" - openconsole_linux");
        if (auto* w = window())
        {
            w->setWindowTitle(full);
        }
    });
    _api.SetBellCallback([this]() {
        QApplication::beep();
    });

    // Default title before the shell sets one.
    setWindowTitle(QStringLiteral("openconsole_linux"));

    // Prime the engine so shells that probe the terminal see VT100+.
    _api.Feed(L"\x1b[?1;2c");

    // Pick a monospace font and derive cell metrics from it.
    // Bypass the fontconfig "monospace" alias (which resolves to Noto Sans Mono
    // CJK KR, whose space glyph renders as a visible diamond) by naming the
    // families explicitly: a user-selectable primary family for ASCII, with
    // Noto Sans CJK SC + emoji/Nerd fonts as fallback.
    _fontFamily = QStringLiteral("Liberation Mono");
    _fallbackFamilies << QStringLiteral("Noto Sans CJK SC");
    _fallbackFamilies << fallbackFontFamilies();
    _fallbackFamilies << QStringLiteral("monospace");
    _applyFont();

    resize(InitialCols * _cellW, InitialRows * _cellH);

    // pty readability drives engine feeding.
    _ptyNotifier = new QSocketNotifier(_pty.master, QSocketNotifier::Read, this);
    connect(_ptyNotifier, &QSocketNotifier::activated, this, [this](int socket) {
        Q_UNUSED(socket);
        _feedPty();
    });

    // Cursor blink.
    _cursorTimer = new QTimer(this);
    _cursorTimer->setInterval(530);
    connect(_cursorTimer, &QTimer::timeout, this, [this]() {
        _cursorVisible = !_cursorVisible;
        update();
    });
    _cursorTimer->start();

    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setMouseTracking(true);
    setCursor(Qt::IBeamCursor);
}

TerminalWidget::~TerminalWidget()
{
    _pty.Shutdown();
}

QSize TerminalWidget::sizeHint() const
{
    return QSize{ InitialCols * _cellW, InitialRows * _cellH };
}

void TerminalWidget::_applyFont()
{
    _font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    QStringList families;
    families << _fontFamily;
    families << _fallbackFamilies;
    _font.setFamilies(families);
    _font.setStyleHint(QFont::Monospace);
    _font.setPixelSize(_fontPixelSize);
    const QFontMetricsF metrics(_font);
    _cellW = std::max(1, static_cast<int>(std::round(metrics.horizontalAdvance(QLatin1Char('M')))));
    _cellH = std::max(1, static_cast<int>(std::ceil(metrics.lineSpacing())));
    _fontAscent = static_cast<int>(std::ceil(metrics.ascent()));
    setFont(_font);
    update();
}

void TerminalWidget::setFontFamily(const QString& family)
{
    if (family.isEmpty() || family == _fontFamily)
    {
        return;
    }
    _fontFamily = family;
    const auto cols = std::max(1, width() / _cellW);
    const auto rows = std::max(1, height() / _cellH);
    _applyFont();
    resize(cols * _cellW, rows * _cellH);
}

void TerminalWidget::setFontSize(const int pixelSize)
{
    const auto clamped = std::clamp(pixelSize, 4, 72);
    if (clamped == _fontPixelSize)
    {
        return;
    }
    _fontPixelSize = clamped;
    const auto cols = std::max(1, width() / _cellW);
    const auto rows = std::max(1, height() / _cellH);
    _applyFont();
    resize(cols * _cellW, rows * _cellH);
}

void TerminalWidget::_feedPty()
{
    char buf[8192];
    std::wstring decoded;
    int n;
    while ((n = ::read(_pty.master, buf, sizeof(buf))) > 0)
    {
        decoded.clear();
        til::u8u16(std::string_view{ buf, static_cast<size_t>(n) }, decoded, _pty.decodeState);
        if (!decoded.empty())
        {
            _api.Feed(decoded);
            _handleEngineResize();
            // New output invalidates the selection (matches Windows Terminal).
            _selection.hasSelection = false;
        }
    }
    // EOF/error closes the master; keep running (like conhost starving).
    update();
}

void TerminalWidget::_handleEngineResize()
{
    til::size pendingSize;
    if (_api.ConsumeResizeRequested(pendingSize))
    {
        const auto px = QSize{ pendingSize.width * _cellW, pendingSize.height * _cellH };
        if (px != size())
        {
            resize(px);
        }
    }
}

void TerminalWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    const auto cols = std::max(1, width() / _cellW);
    const auto rows = std::max(1, height() / _cellH);
    _api.Resize(til::size{ cols, rows });
    _pty.SetSize(til::size{ cols, rows });
    _selection = {};
    update();
}

bool TerminalWidget::_flushInput(const std::optional<std::wstring>& output)
{
    if (output)
    {
        _pty.Write(til::u16u8(*output));
        update();
        return true;
    }
    return false;
}

void TerminalWidget::_sendKeyInput(const INPUT_RECORD& record)
{
    _flushInput(_api.Input().HandleKey(record));
}

void TerminalWidget::keyPressEvent(QKeyEvent* event)
{
    _sendKeyEvent(event, true);
}

void TerminalWidget::keyReleaseEvent(QKeyEvent* event)
{
    _sendKeyEvent(event, false);
}

void TerminalWidget::_sendKeyEvent(const QKeyEvent* event, const bool keyDown)
{
    const auto cks = ControlKeyStateFromKey(event);
    const auto key = event->key();

    // Named keys (arrows, F-keys, Enter, Backspace, Tab, Escape, ...) map to a
    // Windows VK code; the engine encodes them (and handles their modes).
    const auto vk = VkFromQtKey(key);
    if (vk != 0)
    {
        auto effectiveCks = cks;
        if (IsExtendedKey(vk))
        {
            effectiveCks |= ENHANCED_KEY;
        }
        INPUT_RECORD rec{};
        rec.EventType = KEY_EVENT;
        rec.Event.KeyEvent.bKeyDown = keyDown ? TRUE : FALSE;
        rec.Event.KeyEvent.wRepeatCount = 1;
        rec.Event.KeyEvent.wVirtualKeyCode = vk;
        rec.Event.KeyEvent.dwControlKeyState = effectiveCks;
        _sendKeyInput(rec);
        return;
    }

    if (!keyDown)
    {
        return;
    }

    const auto text = event->text();

    // Ctrl+letter (A-Z) -> control character (Ctrl+C = 0x03). Computed from the
    // key so it doesn't depend on Qt's text(), which may be empty or literal.
    const bool ctrl = (cks & CTRL_PRESSED) != 0;
    const bool alt = (cks & ALT_PRESSED) != 0;
    const bool altGr = ctrl && alt && !text.isEmpty() && text[0].unicode() > 0x20 && text[0].unicode() != 0x7f;
    if (ctrl && !altGr && key >= Qt::Key_A && key <= Qt::Key_Z)
    {
        INPUT_RECORD rec{};
        rec.EventType = KEY_EVENT;
        rec.Event.KeyEvent.bKeyDown = TRUE;
        rec.Event.KeyEvent.wRepeatCount = 1;
        rec.Event.KeyEvent.uChar.UnicodeChar = static_cast<wchar_t>(key - Qt::Key_A + 1);
        rec.Event.KeyEvent.dwControlKeyState = 0;
        _sendKeyInput(rec);
        return;
    }

    // Remaining printable characters (letters, digits, punctuation, space,
    // AltGr compositions) are delivered as Unicode text with vk=0, so the
    // engine emits them verbatim.
    if (!text.isEmpty())
    {
        const auto utf16 = text.utf16();
        for (int i = 0; i < text.size(); ++i)
        {
            INPUT_RECORD rec{};
            rec.EventType = KEY_EVENT;
            rec.Event.KeyEvent.bKeyDown = TRUE;
            rec.Event.KeyEvent.wRepeatCount = 1;
            rec.Event.KeyEvent.uChar.UnicodeChar = utf16[i];
            rec.Event.KeyEvent.dwControlKeyState = cks;
            _sendKeyInput(rec);
        }
    }
}

void TerminalWidget::inputMethodEvent(QInputMethodEvent* event)
{
    // IME committed text: deliver each code unit as a key event with vk=0 so
    // the engine transmits the codepoint verbatim.
    const auto committed = event->commitString();
    if (!committed.isEmpty())
    {
        const auto utf16 = committed.utf16();
        for (int i = 0; i < committed.size(); ++i)
        {
            INPUT_RECORD rec{};
            rec.EventType = KEY_EVENT;
            rec.Event.KeyEvent.bKeyDown = TRUE;
            rec.Event.KeyEvent.wRepeatCount = 1;
            rec.Event.KeyEvent.uChar.UnicodeChar = utf16[i];
            // IME committed text is delivered without modifier state.
            rec.Event.KeyEvent.dwControlKeyState = 0;
            _sendKeyInput(rec);
        }
    }
    event->accept();
}

QColor TerminalWidget::_fromColorref(COLORREF c) const
{
    return QColor{ GetRValue(c), GetGValue(c), GetBValue(c), 255 };
}

void TerminalWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    painter.fillRect(rect(), QColor{ 0x0C, 0x0C, 0x0C, 255 });

    auto& buffer = _api.CurrentBuffer();
    const auto size = buffer.GetSize();
    const auto width = size.Width();
    const auto height = size.Height();
    const auto viewRect = _api.ViewportRect();
    auto& settings = _api.Settings();

    const auto defaultBg = _fromColorref(settings.GetAttributeColors(TextAttribute{}).second);

    painter.setFont(_font);

    for (til::CoordType y = 0; y < height; ++y)
    {
        const auto& row = buffer.GetRowByOffset(viewRect.top + y);
        const auto yPx = static_cast<int>(y * _cellH);

        til::CoordType x = 0;
        while (x < width)
        {
            const auto glyph = row.GlyphAt(x);
            const auto attr = row.GetAttrByColumn(x);
            // A leading cell (followed by a trailer) is a wide glyph spanning
            // two columns. Use the buffer's DBCS info instead of guessing from
            // the codepoint (which would treat narrow scripts > U+02FF as wide).
            const bool wide = row.DbcsAttrAt(x) == DbcsAttribute::Leading;
            const auto colors = settings.GetAttributeColors(attr);
            const auto fg = _fromColorref(colors.first);
            const auto bg = _fromColorref(colors.second);

            // Background: only paint cells that differ from the clear color.
            if (bg != defaultBg)
            {
                painter.fillRect(x * _cellW, yPx, (wide ? 2 : 1) * _cellW, _cellH, bg);
            }

            if (!glyph.empty() && !(glyph.size() == 1 && glyph.front() == L' '))
            {
                const QString text = QString::fromWCharArray(glyph.data(), static_cast<int>(glyph.size()));
                painter.setPen(fg);
                const auto drawX = static_cast<qreal>(x * _cellW);
                painter.drawText(QPointF{ drawX, yPx + _fontAscent }, text);

                x += wide ? 2 : 1;
            }
            else
            {
                ++x;
            }
        }
    }

    // Cursor.
    auto& cursor = buffer.GetCursor();
    if (cursor.IsVisible() && _cursorVisible)
    {
        const auto pos = cursor.GetPosition();
        const auto colors = settings.GetAttributeColors(TextAttribute{});
        const auto fg = _fromColorref(colors.first);
        painter.fillRect(pos.x * _cellW, static_cast<int>(pos.y * _cellH), _cellW, _cellH, fg);
    }

    _paintSelection(painter);
}

til::point TerminalWidget::_cellAt(const QPoint& pos)
{
    const auto& buf = _api.CurrentBuffer();
    const auto w = buf.GetSize().Width();
    const auto h = buf.GetSize().Height();
    return til::point{
        std::clamp(pos.x() / _cellW, 0, w - 1),
        std::clamp(pos.y() / _cellH, 0, h - 1),
    };
}

void TerminalWidget::_paintSelection(QPainter& painter)
{
    if (!(_selection.active || _selection.hasSelection))
    {
        return;
    }
    // Normalize so beg <= end (lexicographic: row, then column) before painting.
    // For streaming (non-block) selections the start row already extends to the
    // line end and the end row starts at column 0, so endpoint x must come from
    // beg/end respectively, not from global min/max of the two.
    auto beg = _selection.start;
    auto end = _selection.end;
    if (beg.y > end.y || (beg.y == end.y && beg.x > end.x))
    {
        std::swap(beg, end);
    }

    const auto minY = beg.y;
    const auto maxY = end.y;
    const auto minX = std::min(beg.x, end.x);
    const auto maxX = std::max(beg.x, end.x);
    const auto lastX = _api.CurrentBuffer().GetSize().Width() - 1;
    for (til::CoordType y = minY; y <= maxY; ++y)
    {
        til::CoordType x0;
        til::CoordType x1;
        if (_selection.block || minY == maxY)
        {
            x0 = minX;
            x1 = maxX;
        }
        else if (y == minY)
        {
            x0 = beg.x;
            x1 = lastX;
        }
        else if (y == maxY)
        {
            x0 = 0;
            x1 = end.x;
        }
        else
        {
            x0 = 0;
            x1 = lastX;
        }
        painter.fillRect(x0 * _cellW, y * _cellH, (x1 - x0 + 1) * _cellW, _cellH, _selectionOverlay);
    }
}

void TerminalWidget::mousePressEvent(QMouseEvent* event)
{
    // When an application has enabled mouse tracking, report the event to it
    // unless Shift is held (Shift forces selection, matches Windows Terminal).
    const bool reportToApp = _api.Input().IsTrackingMouseInput() && !(event->modifiers() & Qt::ShiftModifier);

    switch (event->button())
    {
    case Qt::LeftButton:
        if (reportToApp)
        {
            _reportMouse(WM_LBUTTONDOWN, event->position(), event->buttons(), event->modifiers(), 0);
        }
        else
        {
            _selection.active = true;
            _selection.hasSelection = false;
            _selection.block = (event->modifiers() & Qt::AltModifier) != 0;
            _selection.start = _cellAt(event->pos());
            _selection.end = _selection.start;
            update();
        }
        break;
    case Qt::RightButton:
        if (reportToApp)
        {
            _reportMouse(WM_RBUTTONDOWN, event->position(), event->buttons(), event->modifiers(), 0);
        }
        else
        {
            _copyOrPaste();
        }
        break;
    case Qt::MiddleButton:
        if (reportToApp)
        {
            _reportMouse(WM_MBUTTONDOWN, event->position(), event->buttons(), event->modifiers(), 0);
        }
        break;
    default:
        break;
    }
}

void TerminalWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (_api.Input().IsTrackingMouseInput() && !(event->modifiers() & Qt::ShiftModifier))
    {
        _reportMouse(WM_MOUSEMOVE, event->position(), event->buttons(), event->modifiers(), 0);
        return;
    }
    if (_selection.active && (event->buttons() & Qt::LeftButton))
    {
        _selection.end = _cellAt(event->pos());
        update();
    }
}

void TerminalWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (_api.Input().IsTrackingMouseInput() && !(event->modifiers() & Qt::ShiftModifier))
    {
        switch (event->button())
        {
        case Qt::LeftButton:
            _reportMouse(WM_LBUTTONUP, event->position(), event->buttons(), event->modifiers(), 0);
            return;
        case Qt::RightButton:
            _reportMouse(WM_RBUTTONUP, event->position(), event->buttons(), event->modifiers(), 0);
            return;
        case Qt::MiddleButton:
            _reportMouse(WM_MBUTTONUP, event->position(), event->buttons(), event->modifiers(), 0);
            return;
        default:
            return;
        }
    }
    if (event->button() == Qt::LeftButton && _selection.active)
    {
        _selection.active = false;
        _selection.hasSelection = _selection.start != _selection.end;
        update();
    }
}

void TerminalWidget::wheelEvent(QWheelEvent* event)
{
    const auto delta = event->angleDelta();
    if (delta.y() != 0)
    {
        _reportMouse(WM_MOUSEWHEEL, event->position(), event->buttons(), event->modifiers(), static_cast<short>(delta.y()));
    }
    if (delta.x() != 0)
    {
        _reportMouse(WM_MOUSEHWHEEL, event->position(), event->buttons(), event->modifiers(), static_cast<short>(delta.x()));
    }
}

void TerminalWidget::_reportMouse(const unsigned int wmButton, const QPointF& pos, const Qt::MouseButtons buttons, const Qt::KeyboardModifiers mods, const short delta)
{
    const auto cell = _cellAt(pos.toPoint());
    const auto state = Microsoft::Console::VirtualTerminal::TerminalInput::MouseButtonState{
        (buttons & Qt::LeftButton) != 0,
        (buttons & Qt::MiddleButton) != 0,
        (buttons & Qt::RightButton) != 0,
    };
    _flushInput(_api.Input().HandleMouse(cell, wmButton, ModifierKeyStateFromQt(mods), delta, state));
}

void TerminalWidget::_copySelection()
{
    if (!_selection.hasSelection)
    {
        return;
    }
    // Normalize so beg <= end (lexicographic: row, then column).
    auto beg = _selection.start;
    auto end = _selection.end;
    if (beg.y > end.y || (beg.y == end.y && beg.x > end.x))
    {
        const auto tmp = beg;
        beg = end;
        end = tmp;
    }
    // GetPlainText/_RowCopyHelper treat end.x as EXCLUSIVE ([begin, end)), so
    // advance one past the last selected column.
    end.x += 1;

    auto& buf = _api.CurrentBuffer();
    const TextBuffer::CopyRequest req{
        buf,
        beg,
        end,
        /*blockSelection=*/_selection.block,
        /*includeLineBreak=*/true,
        /*trimTrailingWhitespace=*/true,
        /*formatWrappedRows=*/false,
        /*bufferCoordinates=*/false,
    };
    const auto utf8 = til::u16u8(buf.GetPlainText(req));
    QGuiApplication::clipboard()->setText(QString::fromUtf8(utf8.data(), static_cast<int>(utf8.size())));
    _selection.hasSelection = false;
    update();
}

void TerminalWidget::_paste()
{
    const auto text = QGuiApplication::clipboard()->text();
    if (text.isEmpty())
    {
        return;
    }
    const auto utf8 = text.toUtf8();
    const bool bracketed = _api.GetSystemMode(Microsoft::Console::VirtualTerminal::ITerminalApi::Mode::BracketedPaste);
    if (bracketed)
    {
        _pty.Write("\x1b[200~");
    }
    _pty.Write(std::string_view{ utf8.data(), static_cast<size_t>(utf8.size()) });
    if (bracketed)
    {
        _pty.Write("\x1b[201~");
    }
}

void TerminalWidget::_copyOrPaste()
{
    if (_selection.hasSelection)
    {
        _copySelection();
    }
    else
    {
        _paste();
    }
}

bool TerminalWidget::Pty::Open()
{
    winsize ws{};
    ws.ws_col = InitialCols;
    ws.ws_row = InitialRows;

    const char* shell = getenv("SHELL");
    if (!shell || *shell == '\0')
    {
        shell = "/bin/bash";
    }

    child = forkpty(&master, nullptr, nullptr, &ws);
    if (child < 0)
    {
        return false;
    }
    if (child == 0)
    {
        const char* argv[] = { shell, "-i", nullptr };
        setenv("TERM", "xterm-256color", 1);
        execv(shell, const_cast<char**>(argv));
        _exit(127);
    }

    const int flags = fcntl(master, F_GETFL, 0);
    if (flags >= 0)
    {
        fcntl(master, F_SETFL, flags | O_NONBLOCK);
    }
    return true;
}

void TerminalWidget::Pty::Shutdown()
{
    if (child > 0)
    {
        kill(child, SIGHUP);
        waitpid(child, nullptr, 0);
    }
    if (master >= 0)
    {
        ::close(master);
    }
}

void TerminalWidget::Pty::SetSize(const til::size cells)
{
    if (master < 0)
    {
        return;
    }
    winsize ws{};
    ws.ws_row = static_cast<unsigned short>(cells.height);
    ws.ws_col = static_cast<unsigned short>(cells.width);
    ioctl(master, TIOCSWINSZ, &ws);
}

void TerminalWidget::Pty::Write(const std::string_view data)
{
    if (master < 0)
    {
        return;
    }
    const char* p = data.data();
    size_t remaining = data.size();
    while (remaining > 0)
    {
        const auto n = ::write(master, p, remaining);
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                usleep(1000);
                continue;
            }
            break;
        }
        p += n;
        remaining -= static_cast<size_t>(n);
    }
}