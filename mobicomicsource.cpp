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
    :FileComicSource(path)
{
    signatureMimeStr = "application/x-mobipocket-ebook";
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

