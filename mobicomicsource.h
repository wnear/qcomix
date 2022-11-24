#pragma once

#include "comicsource.h"

class MobiComicSource final : public ComicSource
{
public:
    MobiComicSource(const QString& path);
    virtual int getPageCount() const override;
    virtual QPixmap getPagePixmap(int pageNum) override;
    virtual QString getPageFilePath(int pageNum) override;
    virtual QString getTitle() const override;
    virtual QString getFilePath() const override;
    virtual QString getPath() const override;
    virtual ComicSource* nextComic() override;
    virtual ComicSource* previousComic() override;
    virtual QString getID() const override;
    virtual bool hasNextComic() override;
    virtual bool hasPreviousComic() override;
    virtual ComicMetadata getComicMetadata() const override;
    virtual PageMetadata getPageMetadata(int pageNum) override;
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
    QString getNextFilePath();
    QString getPrevFilePath();
    QMutex zipM;
    void readNeighborList();
    QList<pagefile> fileList;
    QFileInfoList cachedNeighborList;
    QuaZip* zip = nullptr;
    QuaZipFile* currZipFile = nullptr;
    QString id;
    QHash<int, PageMetadata> metaDataCache;
};
