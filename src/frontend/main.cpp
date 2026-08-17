// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// Linux port: entry point for the Qt6 grid frontend.

#include "TerminalWidget.h"

#include <QApplication>
#include <QMainWindow>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("openconsole_linux");

    QMainWindow window;
    auto* term = new TerminalWidget();
    window.setCentralWidget(term);
    window.setWindowTitle(QStringLiteral("openconsole_linux"));

    window.resize(term->sizeHint());
    window.show();

    return app.exec();
}
