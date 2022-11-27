#include "mobicomicsource.h"
#include "imagecache.h"

//TODO
#include <QCryptographicHash>
#include <QMimeDatabase>
#include <QDir>
#include <QTemporaryFile>
#include <QCollator>
#include <QDebug>

/**
 * @TODO
 * libmobi filetype matches Qt mimetype: filter more image types.
 */
MobiComicSource::MobiComicSource(const QString& path)
{
    this->meta.f = nullptr;
    this->meta.mobi = nullptr;
    this->meta.rawml = nullptr;
    this->meta.path = QFileInfo(path).absoluteFilePath();

    this->meta.mobi = mobi_init();
    this->meta.f = fopen(path.toStdString().data(), "rb");
    assert(this->meta.f != nullptr);
    auto ret1 = mobi_load_file(this->meta.mobi, this->meta.f);

    this->meta.title = mobi_meta_get_title(this->meta.mobi);
    this->meta.author = mobi_meta_get_author(this->meta.mobi);

    this->meta.rawml = mobi_init_rawml(this->meta.mobi);
    auto ret2 = mobi_parse_rawml(this->meta.rawml, this->meta.mobi);
    if(this->meta.rawml != nullptr) {
        MOBIPart *curr = this->meta.rawml->resources;
        size_t imagePN{1};
        size_t anyPN{1};
        while(curr != nullptr){
            auto file_meta = mobi_get_filemeta_by_type(curr->type);
            //qDebug()<<QString("page of %1 is %2").arg(anyPN++).arg(type2str(curr->type));
            switch(curr->type){
                case T_JPG:
                case T_PNG:
                case T_BMP:
                case T_GIF:
                    this->fileList.push_back({
                            QString("page %1").arg(imagePN++),
                            curr->data,
                            curr->size
                            });
                    break;
                default:
                    break;
            }
            curr = curr->next;
        }
    }


    this->id = QString::fromUtf8(QCryptographicHash::hash((path + +"!/\\++&" + QString::number(this->fileList.count())).toUtf8(), QCryptographicHash::Md5).toHex());
}

int MobiComicSource::getPageCount() const
{
    return this->fileList.length();
}
QPixmap MobiComicSource::getPagePixmap(int pageNum)
{
    auto cacheKey = QPair{id, pageNum};
    if(auto img = ImageCache::cache().getImage(cacheKey); !img.isNull())
        return img;

    QPixmap img;
    img.loadFromData(fileList[pageNum].data, fileList[pageNum].size);
    ImageCache::cache().addImage(cacheKey, img);
    return img;
}

QString MobiComicSource::getPageFilePath(int pageNum)
{
    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    return "why";
}
QString MobiComicSource::getTitle() const
{
    return this->meta.title;
}

QString MobiComicSource::getFilePath() const
{
    return this->meta.path;
}
QString MobiComicSource::getPath() const
{
    return QFileInfo(this->meta.path).path();
}
ComicSource* MobiComicSource::nextComic()
{
    if(auto path = getNextFilePath(); !path.isEmpty()){
        // return createComicSource(path);
    }
    return nullptr;
}
ComicSource* MobiComicSource::previousComic()
{
    if(auto path = getPrevFilePath(); !path.isEmpty())
    {
        // return createComicSource(path);
    }
    return nullptr;
}
QString MobiComicSource::getID() const
{
    return this->id;
}
bool MobiComicSource::hasNextComic()
{
    return !(getNextFilePath().isEmpty());
}
bool MobiComicSource::hasPreviousComic()
{
    return !(getPrevFilePath().isEmpty());
}
ComicMetadata MobiComicSource::getComicMetadata() const
{
    ComicMetadata meta;
    meta.title = this->meta.title;
    meta.fileName = this->meta.path;
    meta.valid = (this->meta.f != nullptr)? true:false;
    return meta;
}
PageMetadata MobiComicSource::getPageMetadata(int pageNum)
{
    QMimeDatabase mdb;
    PageMetadata res;
    auto px = getPagePixmap(pageNum);
    res.width = px.width();
    res.height = px.height();
    res.fileName = this->fileList[pageNum].name;
    res.fileSize = this->fileList[pageNum].size;
    res.fileType = mdb.mimeTypeForFile(this->fileList[pageNum].name + ".jpg").name();
    res.valid = true;
    return res;
}

MobiComicSource::~MobiComicSource()
{
    if(this->meta.rawml){
        mobi_free_rawml(this->meta.rawml);
    }
    if(this->meta.mobi){
        mobi_free(this->meta.mobi);
    }
    if(this->meta.f){
        fclose(this->meta.f);
    }
}
QString MobiComicSource::getNextFilePath()
{
    qDebug()<<"get next file";
    readNeighborList();
    auto length = cachedNeighborList.length();

    qDebug()<<"neighbour files length: "<<length;
    qDebug()<<"test for current path:"<<this->getFilePath();
    for(int i = 0; i < length - 1; i++)
    {
        if(cachedNeighborList[i].absoluteFilePath() == this->getFilePath())
            return cachedNeighborList[i + 1].absoluteFilePath();
    }
    qDebug()<<"not found";

    return {};
}
QString MobiComicSource::getPrevFilePath()
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
void MobiComicSource::readNeighborList()
{
    if(cachedNeighborList.isEmpty())
    {
        QDir dir(this->getPath());
        dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);

        QCollator collator;
        collator.setNumericMode(true);

        QMimeDatabase mimeDb;
        qDebug()<<"current dir files count: "<<dir.entryInfoList().length();
        for(const auto& entry: dir.entryInfoList())
        {
            if(mimeDb.mimeTypeForFile(entry).inherits("application/x-mobipocket-ebook"))
            {
                cachedNeighborList.append(entry);
            }
        }

        std::sort(cachedNeighborList.begin(), cachedNeighborList.end(),
                  [&collator](const QFileInfo& file1, const QFileInfo& file2) {
                      return collator.compare(file1.fileName(), file2.fileName()) < 0;
                  });
    }
}
