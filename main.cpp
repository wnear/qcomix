/*
qcomix: Qt-based comic viewer
Copyright (C) 2019 qcomixdev

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "comicsource.h"
#include "mainwindow.h"
#include <QApplication>
#include <QDebug>

void customMsgHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    const char* file = context.file ? context.file : "";
    const char* function = context.function ? context.function : "";
    switch (type)
    {
        case QtDebugMsg:
            fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
            break;
        case QtInfoMsg:
            fprintf(stderr, "Info: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
            break;
        case QtWarningMsg:
            fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
            break;
        case QtCriticalMsg:
            fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
            break;
        case QtFatalMsg:
            fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
            break;
    }
}

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("qcomix");
    a.setApplicationDisplayName("qcomix");
    a.setQuitOnLastWindowClosed(true);

    //qInstallMessageHandler(&customMsgHandler);

    MainWindow w;

    QString openFileName;
    bool skipNext = true;
    for (const auto& arg : a.arguments())
    {
        if (skipNext)
        {
            skipNext = false;
            continue;
        }
        if (arg == "--profile")
        {
            skipNext = true;
            continue;
        }
        if (!arg.isEmpty())
        {
            openFileName = arg;
            break;
        }
    }

    if (auto idx = a.arguments().indexOf("--profile"); idx != -1 && idx + 1 < a.arguments().length())
    {
        w.init(a.arguments().at(idx + 1), openFileName);
    }
    else
    {
        w.init("default", openFileName);
    }

    w.show();

    return a.exec();
}
