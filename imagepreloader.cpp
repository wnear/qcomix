
#include "imagepreloader.h"
#include "comicsource.h"
#include "imagecache.h"

ImagePreloader::ImagePreloader(int count, bool enabled, QObject* parent) :
    QThread{parent}, count(count), enabled(enabled)
{
}

void ImagePreloader::preloadPages(ComicSource* src, int currPage)
{
    this->stopCurrentWork();
    this->waitMutex.lock();
    this->workMutex.lock();
    this->m_comicSource = src;
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

        while(!(stopFlag || exitFlag) && m_comicSource)
        {
            auto page = checkQueue();
            if(page == -1) break;
            preloadPage(page);
        }

        waitCondition.wait(&waitMutex);

        while(!(stopFlag || exitFlag) && m_comicSource)
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
    int limit = n + 3;
    for(; n <= limit; n++){
        if(n < m_comicSource->getPageCount()){
            // if(auto img = ImageCache::cache().getImage({src->getID(), n}); img.isNull())
            // cache check is doing inside comicsource,
            // it's a try-get.

            m_comicSource->getPagePixmap(n);
        } else {
            break;
        }

    }
}


int ImagePreloader::checkQueue()
{
    int page = -1;
    workMutex.lock();
    if(!pageNums.isEmpty()) page = pageNums.dequeue();
    workMutex.unlock();
    return page;
}
