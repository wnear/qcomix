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

#include "metadata.h"
#include <QFileInfoList>
#include <QPixmap>
#include <QString>
#include <QMutex>
#include <QHash>
#include <quazipfileinfo.h>
#include <mobi.h>

class QuaZip;
class QuaZipFile;

//QUrl, QString, QIODevice.
class ComicSource
{
public:
    ComicSource() {}
    virtual int getPageCount() const = 0;
    virtual QPixmap getPagePixmap(int pageNum) = 0;
    virtual QString getPageFilePath(int pageNum) = 0;
    virtual QString getTitle() const = 0;
    virtual QString getFilePath() const = 0;
    virtual QString getPath() const = 0;
    virtual QString getID() const = 0;
    virtual ComicSource* nextComic() = 0;
    virtual ComicSource* previousComic() = 0;
    virtual bool hasNextComic() = 0;
    virtual bool hasPreviousComic() = 0;
    virtual ComicMetadata getComicMetadata() const = 0;
    virtual PageMetadata getPageMetadata(int pageNum) = 0;
    virtual bool ephemeral() const;
    virtual int startAtPage() const;
    virtual void resortFiles() {}
    virtual ~ComicSource() {}
};

class FileComicSource : public ComicSource
{
public:
    FileComicSource(const QString& path);
    virtual QString getID() const override;
    virtual QString getTitle() const override;
    virtual ComicMetadata getComicMetadata() const override;

    virtual QString getPath() const override;
    virtual QString getFilePath() const override;

    virtual ComicSource* nextComic() override;
    virtual ComicSource* previousComic() override;
    virtual bool hasNextComic() override;
    virtual bool hasPreviousComic() override;
    virtual void readNeighborList();

    QString getNextFilePath();
    QString getPrevFilePath();

protected:
    QString signatureMimeStr{};
    QFileInfoList cachedNeighborList;
    QString path;
    QString id;
};

class ZipComicSource : public FileComicSource
{
public:
    ZipComicSource(const QString& path);
    virtual int getPageCount() const override;
    virtual QPixmap getPagePixmap(int pageNum) override;
    virtual QString getPageFilePath(int pageNum) override;
    virtual PageMetadata getPageMetadata(int pageNum) override;
    virtual ~ZipComicSource();

protected:

    QMutex zipM;
    QList<QuaZipFileInfo> m_zipFileInfoList;
    QuaZip* zip = nullptr;
    QuaZipFile* currZipFile = nullptr;
    QHash<int, PageMetadata> metaDataCache;
};

class EpubComicSource final : public ZipComicSource
{
    public:
        EpubComicSource(const QString& path);
        void resortFiles();
        virtual ~EpubComicSource();
};

class DirectoryComicSource final : public ComicSource
{
public:
    DirectoryComicSource(const QString& filePath);
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
    virtual int startAtPage() const override;
    static bool fileSupported(const QFileInfo &info);

private:
    QString getNextFilePath();
    QString getPrevFilePath();
    void readNeighborList();
    QFileInfoList fileInfoList;
    QFileInfoList cachedNeighborList;
    QString path;
    QString id;
    int startPage = 1;
};

class QNetworkAccessManager;

class HydrusSearchQuerySource final : public ComicSource
{
public:
    HydrusSearchQuerySource(const QString& path);
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
    virtual bool ephemeral() const override;
    virtual ~HydrusSearchQuerySource();

private:
    QJsonDocument doGet(const QString& endpoint, const QMap<QString, QString>& args);
    QNetworkAccessManager* nam = nullptr;
    QVector<PageMetadata> data;
    QString id;
    QString title;
    QStringList dbPaths;
    QStringList filePaths;
};

ComicSource* createComicSource_inner(const QString &path);
ComicSource* createComicSource_fn(const QString& path);
bool isImage(const QString &filename);

#endif // COMICSOURCE_H
