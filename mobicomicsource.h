#pragma once

#include "comicsource.h"

class MobiComicSource final : public FileComicSource
{
public:
    MobiComicSource(const QString& path);
    virtual QString getTitle() const override;
    virtual int getPageCount() const override;

    virtual ComicMetadata getComicMetadata() const override;
    virtual PageMetadata getPageMetadata(int pageNum) override;
    virtual QPixmap getPagePixmap(int pageNum) override;
    virtual QString getPageFilePath(int pageNum) override;
    virtual ~MobiComicSource();

private:
    struct mobiMetadata{
        QString title;
        QString author;
        QString path;
        FILE *f;
        MOBIData *mobi;
        MOBIRawml *rawml;
    } meta;
    struct pagefile {
        QString name;
        const unsigned char * data;
        size_t size;
    };
    QList<pagefile> fileList;
};
