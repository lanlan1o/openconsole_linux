// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// Linux port: S4 Qt6 grid frontend. Owns a pty + engine instance
// (StateMachine + AdaptDispatch on a TextBuffer) and paints a
// conhost-style black grid with a block cursor.

#pragma once

#include "til.h"
#include "TerminalApi.hpp"

#include <QWidget>

#include <pty.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

class QSocketNotifier;
class QTimer;

static constexpr til::CoordType InitialCols = 80;
static constexpr til::CoordType InitialRows = 24;

class TerminalWidget final : public QWidget
{
public:
    TerminalWidget();
    ~TerminalWidget() override;

    QSize sizeHint() const override;
    void setFontFamily(const QString& family);
    void setFontSize(int pixelSize);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void _feedPty();
    void _handleEngineResize();
    void _sendKeyInput(const INPUT_RECORD& record);
    void _sendKeyEvent(const QKeyEvent* event, bool keyDown);
    bool _flushInput(const std::optional<std::wstring>& output);
    QColor _fromColorref(COLORREF c) const;
    void _applyFont();

    til::point _cellAt(const QPoint& pos);
    void _paintSelection(QPainter& painter);
    void _copySelection();
    void _paste();
    void _copyOrPaste();
    void _reportMouse(unsigned int wmButton, const QPointF& pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers mods, short delta);

    struct Pty
    {
        int master = -1;
        pid_t child = -1;
        til::u8state decodeState;

        bool Open();
        void Shutdown();
        void SetSize(const til::size cells);
        void Write(const std::string_view data);
    };

    Pty _pty;
    FrontendTerminalApi _api;
    QSocketNotifier* _ptyNotifier = nullptr;
    QTimer* _cursorTimer = nullptr;

    QFont _font;
    QString _fontFamily;
    int _fontPixelSize = 14;
    QStringList _fallbackFamilies;
    int _cellW = 8;
    int _cellH = 16;
    int _fontAscent = 0;

    bool _cursorVisible = true;

    struct Selection
    {
        bool active = false; // mouse button held, dragging
        bool hasSelection = false; // a completed selection exists
        bool block = false; // Alt-drag = rectangular
        til::point start{}; // viewport coords (col, visible row)
        til::point end{};
    } _selection;
    QColor _selectionOverlay{ 0xCC, 0xCC, 0xCC, 0x44 };
};