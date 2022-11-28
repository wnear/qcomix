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

#include <QCommandLineOption>
#include <QCommandLineParser>

void customMsgHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    const char* file = context.file ? context.file : "";
    const char* function = context.function ? context.function : "";
    switch(type)
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
    a.setApplicationVersion("1.0b6~~");
    a.setQuitOnLastWindowClosed(true);

    //qInstallMessageHandler(&customMsgHandler);

    MainWindow w;

    QCommandLineParser parser;
    parser.setApplicationDescription("A Qt-based comic viewer, support rar, cbr, zip, cbr, epub, mobi.");

    QCommandLineOption configOption(QStringList() << "c" << "profile",
                                  QCoreApplication::translate("main", "Optional configuration file."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("<files>",
                                  QCoreApplication::translate("main", "Comic books to open."));
    parser.addOption(configOption);
    parser.process(a);
    auto books = parser.positionalArguments();
    auto configfile = parser.value(configOption);

    QString profile{"default"};
    if(!configfile.isEmpty()){
        profile = configfile;
    }
    w.initSettings(profile);
    w.loadSettings();
    w.init_openFiles(books);

    // qDebug()<<"config file is:"<<configfile;
    // qDebug()<<"files to open: "<<args;
    // return 1;

    w.show();

    return a.exec();
}
