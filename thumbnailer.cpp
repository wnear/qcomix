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

#include "thumbnailer.h"
#include "comicsource.h"
#include "imagecache.h"
#include "imagepreloader.h"
#include <QDir>
#include <QStandardPaths>
#include <QMutexLocker>

Thumbnailer::Thumbnailer(int num, int threadCount, int cellSizeX, int cellSizeY, bool cacheOnDisk, bool fastScaling, QObject* parent) :
    QThread{parent}, c_num(num), c_threadCount(threadCount), c_cellSizeX(cellSizeX), c_cellSizeY(cellSizeY), m_cacheOnDisk(cacheOnDisk), m_fastScaling(fastScaling)
{
    if(cacheOnDisk) {
        m_thumbLocation = QStandardPaths::standardLocations(QStandardPaths::CacheLocation).first() + "/qcomix/";
        if(!QFile::exists(m_thumbLocation)) {
            QDir{}.mkpath(m_thumbLocation);
        }
    }
}

void Thumbnailer::stopCurrentWork()
{
    this->stopFlag = true;

    QMutexLocker lock(&waitMutex);

    this->m_PageNums.clear();
    this->waitCondition.wakeAll();
}

void Thumbnailer::exit()
{
    exitFlag = true;
    waitCondition.wakeAll();
}

void Thumbnailer::startWorking(ComicSource* src)
{
    if(!src)
        return;

    this->stopCurrentWork();
    QMutexLocker lock(&waitMutex);
    this->m_comicSource = src;
    //TODO:
    // auto start = std::min(src->getPageCount(), 20);
    auto start = src->getPageCount();
    start = 4;
    for(int i = c_num; i < start; i += c_threadCount)
        m_PageNums.enqueue(i);
    this->stopFlag = false;
    this->waitCondition.wakeAll();
}

void Thumbnailer::makeThumbForPage(int page)
{
    if(!m_comicSource)
        return;

    auto srcID = m_comicSource->getID();
    auto cacheKey = QPair{srcID, page};
    if(!ThumbCache::cache().hasKey(cacheKey))
    {
        if(m_cacheOnDisk)
        {
            QString fname = "qc_thumb_" + srcID + "_" + QString::number(page) + "_" + QString::number(c_cellSizeX) + "_" + QString::number(c_cellSizeY) + ".png";
            if(QFile::exists(m_thumbLocation + "/" + fname))
            {
                auto thumbFromDisk = QPixmap(m_thumbLocation + "/" + fname);
                if(!thumbFromDisk.isNull())
                {
                    ThumbCache::cache().addImage(cacheKey, thumbFromDisk);
                }
                else
                {
                    QPixmap thumb = createThumb(page);
                    ThumbCache::cache().addImage(cacheKey, thumb);
                }
                emit this->thumbnailReady(srcID, page);
            }
            else
            {
                QPixmap thumb = createThumb(page);
                ThumbCache::cache().addImage(cacheKey, thumb);
                emit this->thumbnailReady(srcID, page);
                if(!m_comicSource->ephemeral()) thumb.save(m_thumbLocation + "/" + fname, "PNG");
            }
        }
        else
        {
            QPixmap thumb = createThumb(page);
            ThumbCache::cache().addImage(cacheKey, thumb);
            emit this->thumbnailReady(srcID, page);
        }
    }
}

void Thumbnailer::run()
{
    while(!exitFlag)
    {
        waitMutex.lock();

        while(!(stopFlag || exitFlag) && m_comicSource)
        {
            auto page = checkQueue();
            if(page == -1) break;
            makeThumbForPage(page);
        }

        waitCondition.wait(&waitMutex);

        while(!(stopFlag || exitFlag) && m_comicSource)
        {
            auto page = checkQueue();
            if(page == -1) break;
            makeThumbForPage(page);
        }

        waitMutex.unlock();
    }
}

void Thumbnailer::refocus(int pageNum)
{
    pageNum--;
    QMutexLocker lock(&workMutex);
    int c = m_PageNums.size();
    while(c > 0)
    {
        c--;
        auto it = m_PageNums.head();
        if(it > pageNum - 8 && it <= pageNum)
        {
            break;
        }
        else
        {
            m_PageNums.dequeue();
            m_PageNums.enqueue(it);
        }
    }
}

QPixmap Thumbnailer::createThumb(int page)
{
    return m_comicSource->getPagePixmap(page).scaled(c_cellSizeX, c_cellSizeY, Qt::KeepAspectRatio, m_fastScaling ? Qt::FastTransformation : Qt::SmoothTransformation);
}

int Thumbnailer::checkQueue()
{
    int page = -1;
    QMutexLocker lock(&workMutex);
    if(!m_PageNums.isEmpty())
        page = m_PageNums.dequeue();
    return page;
}
