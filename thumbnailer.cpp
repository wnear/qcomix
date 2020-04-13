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
#include "mainwindow.h"
#include <QStandardPaths>

Thumbnailer::Thumbnailer(int num, int threadCount, int cellSizeX, int cellSizeY, bool cacheOnDisk, bool fastScaling, QObject* parent) :
    QThread{parent}, num(num), threadCount(threadCount), cellSizeX(cellSizeX), cellSizeY(cellSizeY), cacheOnDisk(cacheOnDisk), fastScaling(fastScaling)
{
    if(cacheOnDisk)
    {
        thumbLocation = QStandardPaths::standardLocations(QStandardPaths::CacheLocation).first() + "/qcomix/";
        if(!QFile::exists(thumbLocation))
        {
            QDir{}.mkpath(thumbLocation);
        }
    }
}

void Thumbnailer::stopCurrentWork()
{
    this->stopFlag = true;
    this->waitMutex.lock();
    this->pageNums.clear();
    this->waitCondition.wakeAll();
    this->waitMutex.unlock();
}

void Thumbnailer::exit()
{
    exitFlag = true;
    waitCondition.wakeAll();
}

void Thumbnailer::startWorking(ComicSource* src)
{
    this->stopCurrentWork();
    this->waitMutex.lock();
    this->src = src;
    if(src)
        for(int i = num; i < src->getPageCount(); i += threadCount) pageNums.enqueue(i);
    this->stopFlag = false;
    this->waitCondition.wakeAll();
    this->waitMutex.unlock();
}

void Thumbnailer::makeThumbForPage(int page)
{
    if(src)
    {
        auto srcID = src->getID();
        auto cacheKey = QPair{srcID, page};
        if(!ThumbCache::cache().hasKey(cacheKey))
        {
            if(cacheOnDisk)
            {
                QString fname = "qc_thumb_" + srcID + "_" + QString::number(page) + "_" + QString::number(cellSizeX) + "_" + QString::number(cellSizeY) + ".png";
                if(QFile::exists(thumbLocation + "/" + fname))
                {
                    auto thumbFromDisk = QPixmap(thumbLocation + "/" + fname);
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
                    if(!src->ephemeral()) thumb.save(thumbLocation + "/" + fname, "PNG");
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
}

void Thumbnailer::run()
{
    while(!exitFlag)
    {
        waitMutex.lock();

        while(!(stopFlag || exitFlag) && src)
        {
            auto page = checkQueue();
            if(page == -1) break;
            makeThumbForPage(page);
        }

        waitCondition.wait(&waitMutex);

        while(!(stopFlag || exitFlag) && src)
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
    workMutex.lock();
    int c = pageNums.size();
    while(c > 0)
    {
        c--;
        auto it = pageNums.head();
        if(it > pageNum - 8 && it <= pageNum)
        {
            break;
        }
        else
        {
            pageNums.dequeue();
            pageNums.enqueue(it);
        }
    }
    workMutex.unlock();
}

ImagePreloader::ImagePreloader(int count, bool enabled, QObject* parent) :
    QThread{parent}, count(count), enabled(enabled)
{
}

void ImagePreloader::preloadPages(ComicSource* src, int currPage)
{
    this->stopCurrentWork();
    this->waitMutex.lock();
    this->workMutex.lock();
    this->src = src;
    this->pageNums.clear();
    if(src)
    {
        int maxPageCount = src->getPageCount();
        for(int i = currPage - 1 - count; i <= currPage - 1 + count; i++)
            if(i >= 0 && i < maxPageCount) pageNums.enqueue(i);
    }
    this->stopFlag = false;
    workMutex.unlock();
    this->waitCondition.wakeAll();
    this->waitMutex.unlock();
}

void ImagePreloader::stopCurrentWork()
{
    this->stopFlag = true;
    this->waitMutex.lock();
    this->pageNums.clear();
    this->waitCondition.wakeAll();
    this->waitMutex.unlock();
}

void ImagePreloader::exit()
{
    exitFlag = true;
    this->waitCondition.wakeAll();
}

void ImagePreloader::run()
{
    if(!enabled) return;
    while(!exitFlag)
    {
        waitMutex.lock();

        while(!(stopFlag || exitFlag) && src)
        {
            auto page = checkQueue();
            if(page == -1) break;
            preloadPage(page);
        }

        waitCondition.wait(&waitMutex);

        while(!(stopFlag || exitFlag) && src)
        {
            auto page = checkQueue();
            if(page == -1) break;
            preloadPage(page);
        }

        waitMutex.unlock();
    }
}

void ImagePreloader::preloadPage(int n)
{
    if(!enabled) return;
    if(auto img = ImageCache::cache().getImage({src->getID(), n}); img.isNull()) src->getPagePixmap(n);
}

QPixmap Thumbnailer::createThumb(int page)
{
    return src->getPagePixmap(page).scaled(cellSizeX, cellSizeY, Qt::KeepAspectRatio, fastScaling ? Qt::FastTransformation : Qt::SmoothTransformation);
}

int Thumbnailer::checkQueue()
{
    int page = -1;
    workMutex.lock();
    if(!pageNums.isEmpty()) page = pageNums.dequeue();
    workMutex.unlock();
    return page;
}

int ImagePreloader::checkQueue()
{
    int page = -1;
    workMutex.lock();
    if(!pageNums.isEmpty()) page = pageNums.dequeue();
    workMutex.unlock();
    return page;
}
