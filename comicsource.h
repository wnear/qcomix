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

#ifndef COMICSOURCE_H
#define COMICSOURCE_H

#include <QString>
#include <QFileInfoList>
#include <QPixmap>
#include <QPixmapCache>
#include <quazipfileinfo.h>
#include "metadata.h"

class QuaZip;
class QuaZipFile;

class ComicSource
{
    public:
        ComicSource() {}
        virtual int getPageCount() const = 0;
        virtual QPixmap getPage(int pageNum) = 0;
        virtual QString getPageFilePath(int pageNum) = 0;
        virtual QString getTitle() const = 0;
        virtual QString getFilePath() const = 0;
        virtual QString getPath() const = 0;
        virtual QString getPageCacheKey(int pageNum) const = 0;
        virtual ComicSource* nextComic() = 0;
        virtual ComicSource* previousComic() = 0;
        virtual bool hasNextComic() = 0;
        virtual bool hasPreviousComic() = 0;
        virtual ComicMetadata getComicMetadata() const = 0;
        virtual PageMetadata getPageMetadata(int pageNum) = 0;
        virtual ~ComicSource() {}
};

class ZipComicSource final : public ComicSource
{
public:
    ZipComicSource(const QString& path);
    virtual int getPageCount() const override;
    virtual QString getPageCacheKey(int pageNum) const override;
    virtual QPixmap getPage(int pageNum) override;
    virtual QString getPageFilePath(int pageNum) override;
    virtual QString getTitle() const override;
    virtual QString getFilePath() const override;
    virtual QString getPath() const override;
    virtual ComicSource* nextComic() override;
    virtual ComicSource* previousComic() override;
    virtual bool hasNextComic() override;
    virtual bool hasPreviousComic() override;
    virtual ComicMetadata getComicMetadata() const override;
    virtual PageMetadata getPageMetadata(int pageNum) override;
    virtual ~ZipComicSource();

private:
    QString getNextFilePath();
    QString getPrevFilePath();
    void readNeighborList();
    QList<QuaZipFileInfo> fileInfoList;
    QFileInfoList cachedNeighborList;
    QuaZip* zip = nullptr;
    QuaZipFile* currZipFile = nullptr;
    QString path;
    QPixmapCache pCache;
};

class DirectoryComicSource final : public ComicSource
{
public:
    DirectoryComicSource(const QString& path);
    virtual int getPageCount() const override;
    virtual QString getPageCacheKey(int pageNum) const override;
    virtual QPixmap getPage(int pageNum) override;
    virtual QString getPageFilePath(int pageNum) override;
    virtual QString getTitle() const override;
    virtual QString getFilePath() const override;
    virtual QString getPath() const override;
    virtual ComicSource* nextComic() override;
    virtual ComicSource* previousComic() override;
    virtual bool hasNextComic() override;
    virtual bool hasPreviousComic() override;
    virtual ComicMetadata getComicMetadata() const override;
    virtual PageMetadata getPageMetadata(int pageNum) override;

private:
    QString getNextFilePath();
    QString getPrevFilePath();
    void readNeighborList();
    QFileInfoList fileInfoList;
    QFileInfoList cachedNeighborList;
    QString path;
    QPixmapCache pCache;
};

ComicSource* createComicSource(const QString& path);

#endif // COMICSOURCE_H
