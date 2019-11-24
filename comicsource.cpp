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

#include <QDir>
#include <QMimeDatabase>
#include <QCollator>
#include <QImageReader>
#include <algorithm>
#include <quazip.h>
#include <quazipfile.h>
#include <QTemporaryFile>
#include <QDebug>
#include "comicsource.h"

DirectoryComicSource::DirectoryComicSource(const QString &path)
{
    this->pCache.setCacheLimit(256000);

    QDir dir(path);
    this->path = dir.absolutePath();
    dir.setFilter(QDir::Files | QDir::Hidden);
    auto allFiles = dir.entryInfoList();
    QMimeDatabase mimeDb;
    auto supportedImageFormats = QImageReader::supportedMimeTypes();
    for(const auto & file : allFiles)
    {
        for(const auto& format : supportedImageFormats)
        {
            if(mimeDb.mimeTypeForFile(file).inherits(format))
            {
                this->fileInfoList.append(file);
                break;
            }
        }
    }

    QCollator collator;
    collator.setNumericMode(true);

    std::sort(this->fileInfoList.begin(), this->fileInfoList.end(), [&collator](const QFileInfo &file1, const QFileInfo &file2)
    {
        return collator.compare(file1.fileName(), file2.fileName()) < 0;
    });
}

int DirectoryComicSource::getPageCount() const
{
    return this->fileInfoList.length();
}

QPixmap DirectoryComicSource::getPage(int pageNum)
{
    QPixmap* cached = nullptr;
    if(this->pCache.find(this->fileInfoList[pageNum].absoluteFilePath(), cached); cached)
    {
        return cached->copy();
    } else
    {
        auto px = QPixmap(this->fileInfoList[pageNum].absoluteFilePath());
        pCache.insert(this->fileInfoList[pageNum].absoluteFilePath(), px);
        return px;
    }
}

QString DirectoryComicSource::getPageFilePath(int pageNum)
{
    return this->fileInfoList[pageNum].absoluteFilePath();
}

QString DirectoryComicSource::getTitle() const
{
    return QFileInfo(this->path).fileName();
}

QString DirectoryComicSource::getFilePath() const
{
    return this->path;
}

QString DirectoryComicSource::getPath() const
{
    QDir d(this->path);
    d.cdUp();
    return d.absolutePath();
}

bool DirectoryComicSource::hasPreviousComic()
{
    return !getPrevFilePath().isEmpty();
}

bool DirectoryComicSource::hasNextComic()
{
    return !getNextFilePath().isEmpty();
}

ComicSource *DirectoryComicSource::previousComic()
{
    if(auto path = getPrevFilePath(); !path.isEmpty())
    {
        return createComicSource(path);
    }
    return nullptr;
}

ComicSource *DirectoryComicSource::nextComic()
{
    if(auto path = getNextFilePath(); !path.isEmpty())
    {
        return createComicSource(path);
    }
    return nullptr;
}

ComicMetadata DirectoryComicSource::getComicMetadata() const
{
    ComicMetadata res;
    res.title = QFileInfo(path).fileName();
    res.fileName = path;
    res.valid = true;
    return res;
}

PageMetadata DirectoryComicSource::getPageMetadata(int pageNum)
{
    QMimeDatabase mdb;
    PageMetadata res;
    auto px = getPage(pageNum);
    res.width = px.width();
    res.height = px.height();
    res.fileName = this->fileInfoList[pageNum].fileName();
    res.fileSize = this->fileInfoList[pageNum].size();
    res.fileType = mdb.mimeTypeForFile(this->fileInfoList[pageNum]).name();
    res.valid = true;
    return res;
}

QString DirectoryComicSource::getNextFilePath()
{
    readNeighborList();

    auto length = cachedNeighborList.length();
    for(int i = 0; i < length - 1; i++)
    {
        if(cachedNeighborList[i].absoluteFilePath() == this->getFilePath()) return cachedNeighborList[i+1].absoluteFilePath();
    }

    return {};
}

QString DirectoryComicSource::getPrevFilePath()
{
    readNeighborList();

    auto length = cachedNeighborList.length();
    for(int i = 1; i < length; i++)
    {
        if(cachedNeighborList[i].absoluteFilePath() == this->getFilePath()) return cachedNeighborList[i-1].absoluteFilePath();
    }

    return {};
}

void DirectoryComicSource::readNeighborList()
{
    if(cachedNeighborList.isEmpty())
    {
    QDir dir(this->getPath());
    dir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);

    QCollator collator;
    collator.setNumericMode(true);

    cachedNeighborList = dir.entryInfoList();

    std::sort(cachedNeighborList.begin(), cachedNeighborList.end(), [&collator](const QFileInfo &file1, const QFileInfo &file2)
    {
        return collator.compare(file1.fileName(), file2.fileName()) < 0;
    });
    }
}

ComicSource *createComicSource(const QString &path)
{
    if(path.isEmpty()) return nullptr;

    auto fileInfo = QFileInfo(path);
    if(fileInfo.exists())
    {
        if(fileInfo.isDir())
        {
            return new DirectoryComicSource(path);
        } else
        {
            QMimeDatabase mimeDb;
            if(mimeDb.mimeTypeForFile(fileInfo).inherits("application/zip")) return new ZipComicSource(path);
        }
    }

    return nullptr;
}

QString DirectoryComicSource::getPageCacheKey(int pageNum) const
{
    return "DIRECTORY___" + this->path + QStringLiteral("#!@.-~\\/") + QString::number(pageNum);
}

ZipComicSource::ZipComicSource(const QString &path)
{
    this->pCache.setCacheLimit(256000);

    QFileInfo f_inf(path);
    this->path = f_inf.absoluteFilePath();

    this->zip = new QuaZip(this->path);
    if(zip->open(QuaZip::mdUnzip))
    {
        this->currZipFile = new QuaZipFile(zip);
        auto fInfo = zip->getFileInfoList();
        QMimeDatabase mimeDb;
        auto supportedImageFormats = QImageReader::supportedMimeTypes();
        for(const auto & file : fInfo)
        {
            bool fileOK = false;
            auto possibleMimes = mimeDb.mimeTypesForFileName(file.name);
            for(const auto& format : supportedImageFormats)
            {
                for(const auto& possibleMime : possibleMimes)
                {
                    if(possibleMime.inherits(format))
                    {
                        this->fileInfoList.append(file);
                        fileOK = false;
                        break;
                    }
                }
                if(fileOK) break;
            }
        }

        QCollator collator;
        collator.setNumericMode(true);

        std::sort(this->fileInfoList.begin(), this->fileInfoList.end(), [&collator](const QuaZipFileInfo &file1, const QuaZipFileInfo &file2)
        {
            return collator.compare(file1.name, file2.name) < 0;
        });
    }
}

int ZipComicSource::getPageCount() const
{
    return this->fileInfoList.length();
}

QString ZipComicSource::getPageCacheKey(int pageNum) const
{
    return "ZIP___" + this->path + QStringLiteral("#!@.-~\\/") + QString::number(pageNum);
}

QPixmap ZipComicSource::getPage(int pageNum)
{
    QPixmap* cached = nullptr;
    if(this->pCache.find(this->fileInfoList[pageNum].name, cached); cached)
    {
        return cached->copy();
    } else
    {
        this->zip->setCurrentFile(this->fileInfoList[pageNum].name);
        if(this->currZipFile->open(QIODevice::ReadOnly))
        {
            QPixmap px;
            px.loadFromData(this->currZipFile->readAll());
            pCache.insert(this->fileInfoList[pageNum].name, px);
            this->currZipFile->close();
            return px;
        }
    }
    return QPixmap{};
}

QString ZipComicSource::getPageFilePath(int pageNum)
{
    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    if(tmp.open())
    {
        this->zip->setCurrentFile(this->fileInfoList[pageNum].name);
        if(this->currZipFile->open(QIODevice::ReadOnly))
        {
            tmp.write(this->currZipFile->readAll());
            this->currZipFile->close();
        }
        tmp.close();
    }
    return tmp.fileName();
}

QString ZipComicSource::getTitle() const
{
    return QFileInfo(this->path).completeBaseName();
}

QString ZipComicSource::getFilePath() const
{
    return this->path;
}

QString ZipComicSource::getPath() const
{
    return QFileInfo(this->path).path();
}

ComicSource *ZipComicSource::nextComic()
{
    if(auto path = getNextFilePath(); !path.isEmpty())
    {
        return createComicSource(path);
    }
    return nullptr;
}

ComicSource *ZipComicSource::previousComic()
{
    if(auto path = getPrevFilePath(); !path.isEmpty())
    {
        return createComicSource(path);
    }
    return nullptr;
}

bool ZipComicSource::hasNextComic()
{
    return !getNextFilePath().isEmpty();
}

bool ZipComicSource::hasPreviousComic()
{
    return !getPrevFilePath().isEmpty();
}

ComicMetadata ZipComicSource::getComicMetadata() const
{
    ComicMetadata res;
    res.title = this->getTitle();
    res.fileName = path;
    res.valid = true;
    return res;
}

PageMetadata ZipComicSource::getPageMetadata(int pageNum)
{
    QMimeDatabase mdb;
    PageMetadata res;
    auto px = getPage(pageNum);
    res.width = px.width();
    res.height = px.height();
    res.fileName = this->fileInfoList[pageNum].name;
    res.fileSize = this->fileInfoList[pageNum].uncompressedSize;
    auto possibleMimes = mdb.mimeTypesForFileName(this->fileInfoList[pageNum].name);
    res.fileType = "unknown";
    if(possibleMimes.size())
    {
        res.fileType = possibleMimes[0].name();
    }
    res.valid = true;
    return res;
}

ZipComicSource::~ZipComicSource()
{
    if(this->currZipFile)
    {
        this->currZipFile->close();
        this->currZipFile->deleteLater();
    }
    if(this->zip)
    {
        this->zip->close();
        delete this->zip;
    }
}

QString ZipComicSource::getNextFilePath()
{
    readNeighborList();

    auto length = cachedNeighborList.length();
    for(int i = 0; i < length - 1; i++)
    {
        if(cachedNeighborList[i].absoluteFilePath() == this->getFilePath()) return cachedNeighborList[i+1].absoluteFilePath();
    }

    return {};
}

QString ZipComicSource::getPrevFilePath()
{
    readNeighborList();

    auto length = cachedNeighborList.length();
    for(int i = 1; i < length; i++)
    {
        if(cachedNeighborList[i].absoluteFilePath() == this->getFilePath()) return cachedNeighborList[i-1].absoluteFilePath();
    }

    return {};
}

void ZipComicSource::readNeighborList()
{
    if(cachedNeighborList.isEmpty())
    {
    QDir dir(this->getPath());
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);

    QCollator collator;
    collator.setNumericMode(true);

    QMimeDatabase mimeDb;
    for(const auto& entry: dir.entryInfoList())
    {
        if(mimeDb.mimeTypeForFile(entry).inherits("application/zip"))
        {
            cachedNeighborList.append(entry);
        }
    }

    std::sort(cachedNeighborList.begin(), cachedNeighborList.end(), [&collator](const QFileInfo &file1, const QFileInfo &file2)
    {
        return collator.compare(file1.fileName(), file2.fileName()) < 0;
    });
    }
}
