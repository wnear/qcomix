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

#include "comicsource.h"

#include "mobicomicsource.h"
#include "rarcomicsource.h"
#include "pdfcomicsource.h"

#include <QFileInfo>
#include <QCollator>
#include <QDebug>
#include <QDir>
#include <QImageReader>
#include <QMimeDatabase>
#include <QTemporaryFile>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonObject>
#include <algorithm>
#include <quazip.h>
#include <quazipfile.h>
#include "imagecache.h"
#include "mainwindow.h"
#include <QDebug>
#include <cstdio>
#include <QThread>

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QTimer>

bool isImage(const QString &filename){

    QMimeDatabase mimeDb;
    auto supportedImageFormats = QImageReader::supportedMimeTypes();
    bool fileOK = false;
    auto possibleMimes = mimeDb.mimeTypesForFileName(filename);
    for(const auto& format: supportedImageFormats) {
        for(const auto& possibleMime: possibleMimes) {
            if(possibleMime.inherits(format)) {
                return true;
            }
        }
    }
    return false;
}

bool ComicSource::hasPagePixmap(int pageNum) const
{
    assert(isValidPage(pageNum));
    auto cacheKey = QPair{id, pageNum};
    auto img = ImageCache::cache().getImage(cacheKey);
    return !img.isNull();
}

DirectoryComicSource::DirectoryComicSource(const QString& path)
{
    QFileInfo fInfo(path);
    QDir dir;
    if(fInfo.isFile()){
        dir = fInfo.dir();
    } else {
        dir = QDir(path);
    }
    // auto dir = fInfo.dir();


    this->path = dir.absolutePath();
    qDebug()<<"directory, path is "<<this->path;
    this->id = QString::fromUtf8(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Md5).toHex());

    dir.setFilter(QDir::Files | QDir::Hidden);
    auto allFiles = dir.entryInfoList();
    for(const auto& file: allFiles) {
        if(fileSupported(file))
            this->fileInfoList.append(file);
    }

    QCollator collator;
    collator.setNumericMode(true);

    std::sort(this->fileInfoList.begin(), this->fileInfoList.end(), [&collator](const QFileInfo& file1, const QFileInfo& file2) {
        return collator.compare(file1.fileName(), file2.fileName()) < 0;
    });

    if(!fInfo.isDir())
    {
        for(int i = 0; i < this->fileInfoList.size(); i++)
        {
            if(fInfo == this->fileInfoList[i])
            {
                startPage = i+1;
                break;
            }
        }
    }
}

int DirectoryComicSource::getPageCount() const
{
    return this->fileInfoList.length();
}

QPixmap DirectoryComicSource::getPagePixmap(int pageNum)
{
    assert(pageNum >=0 && pageNum < this->getPageCount());
    auto cacheKey = QPair{id, pageNum};
    if(auto img = ImageCache::cache().getImage(cacheKey); !img.isNull()) return img;

    QPixmap img(this->fileInfoList[pageNum].absoluteFilePath());
    ImageCache::cache().addImage(cacheKey, img);
    return img;
}

QString DirectoryComicSource::getPageFilePath(int pageNum)
{
    assert(pageNum >=0 && pageNum < this->getPageCount());
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
    // assert(0);
    QDir d(this->path);
    // d.cdUp();
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

ComicSource* DirectoryComicSource::previousComic()
{
    // if(auto path = getPrevFilePath(); !path.isEmpty()) return createComicSource(path);
    return nullptr;
}

QString DirectoryComicSource::getID() const
{
    return id;
}

ComicSource* DirectoryComicSource::nextComic()
{
    // if(auto path = getNextFilePath(); !path.isEmpty()) return createComicSource(path);
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
    assert(pageNum >=0 && pageNum < this->getPageCount());
    QMimeDatabase mdb;
    PageMetadata res;
    auto px = getPagePixmap(pageNum);
    res.width = px.width();
    res.height = px.height();
    res.fileName = this->fileInfoList[pageNum].fileName();
    res.fileSize = this->fileInfoList[pageNum].size();
    res.fileType = mdb.mimeTypeForFile(this->fileInfoList[pageNum]).name();
    res.valid = true;
    return res;
}

int DirectoryComicSource::startAtPage() const
{
    return this->startPage;
}

bool DirectoryComicSource::fileSupported(const QFileInfo& info)
{
    return info.isFile() && isImage(info.absoluteFilePath());
}

QString DirectoryComicSource::getNextFilePath()
{
    readNeighborList();

    auto length = cachedNeighborList.length();
    for(int i = 0; i < length - 1; i++)
    {
        if(cachedNeighborList[i].absoluteFilePath() == this->getFilePath())
            return cachedNeighborList[i + 1].absoluteFilePath();
    }

    return {};
}

QString DirectoryComicSource::getPrevFilePath()
{
    readNeighborList();

    auto length = cachedNeighborList.length();
    for(int i = 1; i < length; i++)
    {
        if(cachedNeighborList[i].absoluteFilePath() == this->getFilePath())
            return cachedNeighborList[i - 1].absoluteFilePath();
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

        std::sort(cachedNeighborList.begin(), cachedNeighborList.end(),
                  [&collator](const QFileInfo& file1, const QFileInfo& file2) {
                      return collator.compare(file1.fileName(), file2.fileName()) < 0;
                  });
    }
}

ComicSource* createComicSource_inner(const QString& path)
{
    ComicSource *result = nullptr;
    if(path.isEmpty())
        return nullptr;

    if(path.toLower().startsWith("hydrus://"))
    {
        if(!MainWindow::getOption("enableHydrusIntegration").toBool()) return nullptr;
        return new HydrusSearchQuerySource{path.toLower()};
    }

    auto fileInfo = QFileInfo(path);
    do {
        if(fileInfo.exists() == false)
            break;
        if(fileInfo.isDir()){
            return new DirectoryComicSource(path);
        }

        QMimeDatabase mimeDb;
        auto mime = mimeDb.mimeTypeForFile(path);
        qDebug()<<"current file mimetype is: "<<mime.name();
        if(mime.inherits("applicaton/vnd.comicbook+zip")){
            return new ZipComicSource(path);
        } else if (mime.inherits("application/vnd.comicbook-rar")){
            qDebug()<<"cbr file";
            return new RarComicSource(path);
        } else if (mime.inherits("application/vnd.rar")){
            qDebug()<<"cbr file";
            return new RarComicSource(path);
        } else if (mime.inherits("application/epub+zip")) {
            qDebug()<<"epub file";
            return new EpubComicSource(path);
        } else if (mime.inherits("application/x-mobipocket-ebook")) {
            return new MobiComicSource(path);
        } else if(mime.inherits("application/pdf")) {
            qDebug()<<".pdf file";
            return new PDFComicSource(path);
        } else if(mimeDb.mimeTypeForFile(fileInfo).inherits("application/zip")) {
            return new ZipComicSource(path);
        } else if(mime.inherits("application/rar")) {
            qDebug()<<"rar file";
            return new RarComicSource(path);
        } else if(DirectoryComicSource::fileSupported(fileInfo)) {
            return new DirectoryComicSource{path};
        }

    } while(0);
    return nullptr;
}

ComicSource* createComicSource_fn(const QString& path)
{
    QEventLoop evlp;
    QTimer taskTimer; // timer for task.
    QFutureWatcher<ComicSource*> watcher;
    QObject::connect(&watcher, &QFutureWatcher<ComicSource *>::canceled, [&](){
                evlp.quit();
                taskTimer.stop();
                qDebug() << "comic created canceal, timer stop.";
            });
    QObject::connect(&watcher, &QFutureWatcher<ComicSource *>::finished, [&](){
                evlp.quit();
                taskTimer.stop();
                qDebug() << "comic created done, timer stop.";
            });
    int step = 0;
    taskTimer.setInterval(500);
    QObject::connect(&taskTimer, &QTimer::timeout, [&](){
                         qDebug()<<"timeout, step = "<<step;
                         step++;
                     });
    taskTimer.start();

    auto future = QtConcurrent::run(
                          createComicSource_inner,
                          path
                      );
    watcher.setFuture(future);
    evlp.exec();
    if(future.isCanceled())
        return nullptr;
    assert(future.isFinished());
    return future.result();
}

FileComicSource::FileComicSource(const QString& path) {
    this->path = QFileInfo(path).absoluteFilePath();
    this->id = QString::fromUtf8(QCryptographicHash::hash((this->getFilePath() + +"!/\\++&" + QString::number(QFileInfo(path).size())).toUtf8(), QCryptographicHash::Md5).toHex());
}

QString FileComicSource::getID() const
{
    return id;
}

QString FileComicSource::getTitle() const
{
    return QFileInfo(this->path).completeBaseName();
}

QString FileComicSource::getPath() const
{
    return QFileInfo(this->path).path();
}

QString FileComicSource::getFilePath() const
{
    return this->path;
}

ComicSource* FileComicSource::nextComic()
{
    if(auto path = getNextFilePath(); !path.isEmpty())
    {
        // return createComicSource(path);
    }
    return nullptr;
}

ComicSource* FileComicSource::previousComic()
{
    if(auto path = getPrevFilePath(); !path.isEmpty())
    {
        // return createComicSource(path);
    }
    return nullptr;
}

bool FileComicSource::hasNextComic()
{
    return !getNextFilePath().isEmpty();
}

bool FileComicSource::hasPreviousComic()
{
    return !getPrevFilePath().isEmpty();
}

ComicMetadata FileComicSource::getComicMetadata() const
{
    ComicMetadata res;
    res.title = this->getTitle();
    res.fileName = path;
    res.valid = true;
    return res;
}

void FileComicSource::readNeighborList()
{

    if(!cachedNeighborList.isEmpty()){
        return;
    }

    QDir dir(this->getPath());
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);

    QCollator collator;
    collator.setNumericMode(true);

    QMimeDatabase mimeDb;
    assert(!signatureMimeStr.isEmpty());
    for(const auto& entry: dir.entryInfoList())
    {
        if(mimeDb.mimeTypeForFile(entry).inherits(signatureMimeStr))
        {
            cachedNeighborList.append(entry);
        }
    }

    std::sort(cachedNeighborList.begin(), cachedNeighborList.end(),
              [&collator](const QFileInfo& file1, const QFileInfo& file2) {
                  return collator.compare(file1.fileName(), file2.fileName()) < 0;
              });
}

QString FileComicSource::getNextFilePath()
{
    readNeighborList();

    auto length = cachedNeighborList.length();
    for(int i = 0; i < length - 1; i++)
    {
        if(cachedNeighborList[i].absoluteFilePath() == this->getFilePath())
            return cachedNeighborList[i + 1].absoluteFilePath();
    }

    return {};
}

QString FileComicSource::getPrevFilePath()
{
    readNeighborList();

    auto length = cachedNeighborList.length();
    for(int i = 1; i < length; i++)
    {
        if(cachedNeighborList[i].absoluteFilePath() == this->getFilePath())
            return cachedNeighborList[i - 1].absoluteFilePath();
    }

    return {};
}

ZipComicSource::ZipComicSource(const QString& path)
    :FileComicSource(path)
{
    signatureMimeStr = "application/zip";

    QFileInfo f_info(path);

    this->zip = new QuaZip(this->path);
    if(zip->open(QuaZip::mdUnzip)) {
        this->currZipFile = new QuaZipFile(zip);

        auto fInfo = zip->getFileInfoList();
        QMimeDatabase mimeDb;
        auto supportedImageFormats = QImageReader::supportedMimeTypes();
        for(const auto& file: fInfo) {
            bool fileOK = false;
            auto possibleMimes = mimeDb.mimeTypesForFileName(file.name);
            for(const auto& format: supportedImageFormats) {
                for(const auto& possibleMime: possibleMimes) {
                    if(possibleMime.inherits(format)) {
                        this->m_zipFileInfoList.append(file);
                        fileOK = false;
                        break;
                    }
                }
                if(fileOK)
                    break;
            }
        }

        QCollator collator;
        collator.setNumericMode(true);

        std::sort(
          this->m_zipFileInfoList.begin(), this->m_zipFileInfoList.end(),
          [&collator](const QuaZipFileInfo& file1, const QuaZipFileInfo& file2) {
              return collator.compare(file1.name, file2.name) < 0;
          });
    }
}

int ZipComicSource::getPageCount() const
{
    return this->m_zipFileInfoList.length();
}

QPixmap ZipComicSource::getPagePixmap(int pageNum)
{
    auto cacheKey = QPair{id, pageNum};
    if(auto img = ImageCache::cache().getImage(cacheKey); !img.isNull()) return img;

    zipM.lock();
    this->zip->setCurrentFile(this->m_zipFileInfoList[pageNum].name);
    if(this->currZipFile->open(QIODevice::ReadOnly))
    {
        QPixmap img;
        img.loadFromData(this->currZipFile->readAll());
        this->currZipFile->close();
        zipM.unlock();
        ImageCache::cache().addImage(cacheKey, img);
        return img;
    }
    zipM.unlock();
    return {};
}

QString ZipComicSource::getPageFilePath(int pageNum)
{
    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    if(tmp.open())
    {
        this->zip->setCurrentFile(this->m_zipFileInfoList[pageNum].name);
        if(this->currZipFile->open(QIODevice::ReadOnly))
        {
            tmp.write(this->currZipFile->readAll());
            this->currZipFile->close();
        }
        tmp.close();
    }
    return tmp.fileName();
}



PageMetadata ZipComicSource::getPageMetadata(int pageNum)
{
    if(metaDataCache.count(pageNum)) return metaDataCache[pageNum];

    PageMetadata res;
    auto px = getPagePixmap(pageNum);
    res.width = px.width();
    res.height = px.height();
    res.fileName = this->m_zipFileInfoList[pageNum].name;
    res.fileSize = this->m_zipFileInfoList[pageNum].uncompressedSize;

    QMimeDatabase mdb;
    auto possibleMimes = mdb.mimeTypesForFileName(res.fileName);
    if(!possibleMimes.empty()) {
        res.fileType = possibleMimes[0].name();
    } else {
        res.fileType = "unknown";
    }

    res.valid = true;
    metaDataCache[pageNum] = res;
    return res;
}

ZipComicSource::~ZipComicSource()
{
    if(this->currZipFile)
    {
        if(this->currZipFile->isOpen()) this->currZipFile->close();
        this->currZipFile->deleteLater();
    }
    if(this->zip)
    {
        if(this->zip->isOpen()) this->zip->close();
        delete this->zip;
    }
}

HydrusSearchQuerySource::HydrusSearchQuerySource(const QString& path)
{
    this->nam = new QNetworkAccessManager{};

    QStringList query;
    const QString hydrusProtocol = "hydrus://";
    if(path.toLower().startsWith(hydrusProtocol))
    {
        query = path.mid(hydrusProtocol.size()).split(",", Qt::SkipEmptyParts);
        for(auto& tag: query) tag = tag.trimmed();
    }
    else
        return;

    if(query.isEmpty()) return;

    title = query.join(", ");
    id = path;

    dbPaths = MainWindow::getOption("hydrusClientFilesLocations").toStringList();

    QMap<QString, QString> searchParams;
    searchParams["system_inbox"] = "true";
    searchParams["system_archive"] = "true";

    QJsonArray tagsArr = QJsonArray::fromStringList(query);
    QJsonDocument tagsDoc;
    tagsDoc.setArray(tagsArr);

    searchParams["tags"] = QString::fromUtf8(QUrl::toPercentEncoding(tagsDoc.toJson()));
    QJsonDocument searchRes = doGet("/get_files/search_files", searchParams);

    if(!searchRes.isEmpty())
    {
        QStringList ids;
        for(const auto& v: searchRes["file_ids"].toArray())
        {
            ids.push_back(QString::number(v.toInt()));
        }
        QMap<QString, QString> metadataParams;
        metadataParams["file_ids"] = QString::fromUtf8(QUrl::toPercentEncoding("[" + ids.join(",") + "]"));
        auto metadata = doGet("/get_files/file_metadata", metadataParams);

        if(!metadata.isEmpty())
        {
            auto data = metadata["metadata"].toArray();
            const QSet<QString> supportedHydrusMimes = {"image/jpg", "image/png"};
            const QString sortNamespace = MainWindow::getOption("hydrusResultsSortNamespace").toString() + ":";
            QVector<QPair<QString, PageMetadata>> results;
            for(const auto& v: data)
            {
                auto obj = v.toObject();
                if(supportedHydrusMimes.contains(obj["mime"].toString()))
                {
                    PageMetadata fileMetaData;
                    fileMetaData.width = obj["width"].toInt();
                    fileMetaData.height = obj["height"].toInt();
                    fileMetaData.fileSize = obj["size"].toInt();
                    fileMetaData.fileType = obj["mime"].toString();
                    fileMetaData.fileName = obj["hash"].toString() + "." + fileMetaData.fileType.split("/")[1];
                    auto tagRepos = obj["service_names_to_statuses_to_tags"].toObject();
                    QStringList tags;
                    QString sortTag;
                    for(const auto& tagRepo: tagRepos.keys())
                    {
                        if(tagRepos[tagRepo].toObject().contains("0"))
                        {
                            auto tagsList = tagRepos[tagRepo].toObject()["0"].toArray();
                            for(const auto& t: tagsList)
                            {
                                auto tagStr = t.toString();
                                if(tagStr.startsWith(sortNamespace)) sortTag = tagStr;
                                tags.append(tagStr);
                            }
                        }
                        if(tagRepos[tagRepo].toObject().contains("1"))
                        {
                            auto tagsList = tagRepos[tagRepo].toObject()["1"].toArray();
                            for(const auto& t: tagsList)
                            {
                                auto tagStr = t.toString();
                                if(tagStr.startsWith(sortNamespace)) sortTag = tagStr;
                                tags.append(tagStr);
                            }
                        }
                    }
                    fileMetaData.tags = tags;
                    results.push_back({sortTag, fileMetaData});
                }
            }
            if(sortNamespace.size() > 1)
            {
                auto sortKey = [](const QPair<QString, PageMetadata>& a, const QPair<QString, PageMetadata>& b) {
                    return a.first.localeAwareCompare(b.first) < 0;
                };
                std::sort(results.begin(), results.end(), sortKey);
            }
            for(const auto& r: results)
            {
                QString pathFragment = "/f" + r.second.fileName.left(2) + "/" + r.second.fileName;
                for(const auto& dbPath: std::as_const(dbPaths))
                {
                    if(QFile::exists(dbPath + pathFragment))
                    {
                        this->filePaths.push_back(dbPath + pathFragment);
                        this->data.push_back(r.second);
                    }
                }
            }
        }
    }
}

int HydrusSearchQuerySource::getPageCount() const
{
    return this->data.size();
}

QPixmap HydrusSearchQuerySource::getPagePixmap(int pageNum)
{
    auto cacheKey = QPair{id, pageNum};
    if(auto img = ImageCache::cache().getImage(cacheKey); !img.isNull()) return img;

    QPixmap img(filePaths[pageNum]);
    ImageCache::cache().addImage(cacheKey, img);
    return img;
}

QString HydrusSearchQuerySource::getPageFilePath(int pageNum)
{
    return filePaths[pageNum];
}

QString HydrusSearchQuerySource::getTitle() const
{
    return title;
}

QString HydrusSearchQuerySource::getFilePath() const
{
    return id;
}

QString HydrusSearchQuerySource::getPath() const
{
    return id;
}

ComicSource* HydrusSearchQuerySource::nextComic()
{
    return nullptr;
}

ComicSource* HydrusSearchQuerySource::previousComic()
{
    return nullptr;
}

QString HydrusSearchQuerySource::getID() const
{
    return id;
}

bool HydrusSearchQuerySource::hasNextComic()
{
    return false;
}

bool HydrusSearchQuerySource::hasPreviousComic()
{
    return false;
}

ComicMetadata HydrusSearchQuerySource::getComicMetadata() const
{
    ComicMetadata res;
    res.title = title;
    res.fileName = "n/a";
    res.valid = true;
    return res;
}

PageMetadata HydrusSearchQuerySource::getPageMetadata(int pageNum)
{
    return this->data[pageNum];
}

bool HydrusSearchQuerySource::ephemeral() const
{
    return true;
}

HydrusSearchQuerySource::~HydrusSearchQuerySource()
{
    if(this->nam) this->nam->deleteLater();
}

QJsonDocument HydrusSearchQuerySource::doGet(const QString& endpoint, const QMap<QString, QString>& args)
{
    auto apiURL = QUrl{MainWindow::getOption("hydrusAPIURL").toString() + endpoint};
    QUrlQuery query{apiURL};
    for(const auto& k: args) query.addQueryItem(k, args[k]);
    apiURL.setQuery(query);

    QNetworkRequest req{apiURL};
    req.setRawHeader("Hydrus-Client-API-Access-Key", MainWindow::getOption("hydrusAPIKey").toString().toUtf8());

    auto reply = nam->get(req);

    QEventLoop loop;
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    if(!reply->isFinished()) loop.exec();

    if(reply->error())
    {
        QTextStream out(stdout);
        out << "Network error: " << reply->errorString() << Qt::endl;
        return {};
    }

    return QJsonDocument::fromJson(reply->readAll());
}

bool ComicSource::ephemeral() const
{
    return false;
}

int ComicSource::startAtPage() const
{
    return -1;
}


