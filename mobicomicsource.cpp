#include "comicsource.h"

MobiComicSource::MobiComicSource(const QString& path)
    :path(path)
{
    //parse libfile and 
    // get metadata:title author.
    // get id.
    //store file-data -> fileList[].
    

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
    img.loadFromData(fileList[pageNum]);
    ImageCache::cache().addImage(cacheKey, cacheKey, img);
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
    return this->meta->title;
}

QString MobiComicSource::getFilePath() const
{
    return this->path;
}
QString MobiComicSource::getPath() const
{
    return QFileInfo(this->path).path();
}
ComicSource* MobiComicSource::nextComic()
{
    if(auto path = getNextFilePath(); !path.isEmpty()){
        return createComicSource(path);
    }
    return nullptr;
}
ComicSource* MobiComicSource::previousComic()
{
    if(auto path = getPrevFilePath(); !path.isEmpty())
    {
        return createComicSource(path);
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
    return getComicMetadata{};
}
PageMetadata MobiComicSource::getPageMetadata(int pageNum)
{
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

MobiComicSource::~MobiComicSource()
{

}
QString MobiComicSource::getNextFilePath()
{

}
QString MobiComicSource::getPrevFilePath()
{

}
void MobiComicSource::readNeighborList()
{

}
