#include "pdfcomicsource.h"
#include <QDebug>
#include <memory>
#include "imagecache.h"

PDFComicSource::PDFComicSource(const QString& path) : FileComicSource(path)
{
    signatureMimeStr = "application/pdf";
    m_document = Poppler::Document::load(this->path);

    if(!m_document || m_document->isLocked() ){
        qDebug()<<"faile to open, may be it's not a valid pdf file.";
        return;
    }

    auto len = m_document->numPages();
    auto charlen = QString::number(len).size();
    for(int i = 0; i< len; i++){
        PageMetadata cur;
        cur.fileName = QString("%1").arg(i, charlen, 10, QLatin1Char('0'));
        m_pageMetaDataList.push_back(cur);
    }
}

PDFComicSource::~PDFComicSource()
{
    delete m_document;
    m_document = nullptr;
}

int PDFComicSource::getPageCount() const
{
    if (!m_document)
        return 0;
    return m_document->numPages();
}

QPixmap PDFComicSource::getPagePixmap(int pageNum)
{
    auto cacheKey = QPair{id, pageNum};
    if(auto img = ImageCache::cache().getImage(cacheKey); !img.isNull())
        return img;

    QMutexLocker lock(&m_openLock);
    std::unique_ptr<Poppler::Page> page ;
    page.reset(m_document->page(pageNum));
    //TODO:
    //renderToImage(xres, yres)
    auto img = QPixmap::fromImage(page->renderToImage(300, 300));
    if(!img.isNull()){
        ImageCache::cache().addImage(cacheKey, img);
    }

    return img;
}

QString PDFComicSource::getPageFilePath(int pageNum)
{
    return "virtual";
}

PageMetadata PDFComicSource::getPageMetadata(int pageNum)
{
    auto& meta = m_pageMetaDataList[pageNum];
    if(meta.valid){
        return meta;
    }
    auto px = getPagePixmap(pageNum);
    meta.width = px.width();
    meta.height = px.height();
    meta.fileSize = 0;
    meta.valid = true;
    return meta;
}

