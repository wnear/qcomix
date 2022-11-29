#pragma once
#include "comicsource.h"

class RarComicSource : public FileComicSource
{
public:
    RarComicSource(const QString& path);
    virtual int getPageCount() const override;
    virtual QPixmap getPagePixmap(int pageNum) override;
    virtual QString getPageFilePath(int pageNum) override;
    virtual PageMetadata getPageMetadata(int pageNum) override;
    virtual ~RarComicSource();

protected:
    QList<PageMetadata> m_rarFileInfoList;
};
