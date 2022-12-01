#pragma once

#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>


class ComicSource;
class ImagePreloader : public QThread
{
public:
    explicit ImagePreloader(int count, bool enabled, QObject* parent = nullptr);
    void preloadPages(ComicSource* src, int currPage);
    void stopCurrentWork();
    void exit();
    void run() override;
    virtual ~ImagePreloader() {}

private:
    void preloadPage(int n);
    const int count;
    std::atomic_bool stopFlag = false;
    ComicSource* m_comicSource = nullptr;
    QMutex waitMutex;
    QMutex workMutex;
    bool enabled = false;
    int checkQueue();
    QWaitCondition waitCondition;
    QQueue<int> pageNums;
    std::atomic_bool exitFlag = false;
};
