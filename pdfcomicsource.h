#pragma once

#include "comicsource.h"
#include <poppler/qt5/poppler-qt5.h>
#include <poppler/qt5/poppler-version.h>

class PDFComicSource : public FileComicSource
{
public:
  PDFComicSource(const QString& path);
  virtual int getPageCount() const override;
  virtual QPixmap getPagePixmap(int pageNum) override;
  virtual QString getPageFilePath(int pageNum) override;
  virtual PageMetadata getPageMetadata(int pageNum) override;
  virtual void readNeighborList() override;
  virtual ~PDFComicSource();

private:
    QList<PageMetadata> m_pageMetaDataList{};
    Poppler::Document *m_document{nullptr};
    QMutex m_openLock;
    // QString
};
