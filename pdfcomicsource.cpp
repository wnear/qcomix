#include "pdfcomicsource.h"
#include <QDebug>
#include <memory>
#include "imagecache.h"

PDFComicSource::PDFComicSource(const QString& path) : FileComicSource(path)
{
    signatureMimeStr = "application/pdf";
    m_document = Poppler::Document::load(this->path);

    if(!m_document || m_document->isLocked() ){
        qDebug()<<"faile to open";
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
PDFComicSource::~PDFComicSource() {
    delete m_document;
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

    // auto page = std::make_unique<Poppler::Page>(m_document->page(pageNum));
    std::unique_ptr<Poppler::Page> page ;
    QMutexLocker lock(&m_openLock);

    page.reset(m_document->page(pageNum));
    auto img = QPixmap::fromImage(page->renderToImage());
    // auto img = page->renderToImage();
    if(!img.isNull()){
        ImageCache::cache().addImage(cacheKey, img);
    }

    return img;
}

QString PDFComicSource::getPageFilePath(int pageNum)
{
    // assert(0);
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

