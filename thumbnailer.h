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

#ifndef THUMBNAILER_H
#define THUMBNAILER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <atomic>
#include <QWaitCondition>
#include <QQueue>

class ComicSource;

class Thumbnailer : public QThread
{
    Q_OBJECT

public:
    explicit Thumbnailer(int num, int threadCount, int cellSizeX, int cellSizeY, bool cacheOnDisk, bool fastScaling, QObject* parent = nullptr);
    void stopCurrentWork();
    void exit();
    void startWorking(ComicSource* src);
    void run() override;
    void refocus(int pageNum);
    virtual ~Thumbnailer() {}

signals:
    void thumbnailReady(const QString& srcID, int pageNum);

private:
    void makeThumbForPage(int page);
    QPixmap createThumb(int page);
    const int c_num = 0;
    const int c_threadCount = 0;
    const int c_cellSizeX = 0;
    const int c_cellSizeY = 0;
    QString m_thumbLocation;
    bool m_cacheOnDisk = false;
    bool m_fastScaling = false;
    std::atomic_bool stopFlag = false;
    ComicSource* m_comicSource = nullptr;
    QMutex waitMutex;
    QMutex workMutex;
    int checkQueue();
    QWaitCondition waitCondition;
    QQueue<int> m_PageNums;
    std::atomic_bool exitFlag = false;
};


#endif // THUMBNAILER_H
