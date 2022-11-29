#include "rarcomicsource.h"
#include "imagecache.h"

#include <QCollator>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QMimeData>
#include <QMimeDatabase>
#include <QObject>
#include <QProcess>
#include <QTemporaryFile>
#include <cassert>
#include <QImageReader>
#include <QDebug>

RarComicSource::RarComicSource(const QString& path)
            :FileComicSource(path)
{
    signatureMimeStr = "application/rar";
    //rar vt, list file info.

    // QObject::connect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [=](int exitCode, QProcess::ExitStatus status){ });
    QEventLoop evlp;
    QProcess proc;
    proc.start("unrar", {"vt", this->path});
    QObject::connect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     &evlp, &QEventLoop::quit);
    evlp.exec();
    QByteArray s;
    QString text;
    s = proc.readAllStandardOutput();
    text.append(s);

    bool isList{false};
    bool isImg{false};
    PageMetadata cur;
    for (auto s: text.split("\n")){
        auto l = s.trimmed();
        if(l.startsWith("Details")){
            isList = true;
            continue;
        }
        if(isList){
            auto fnPattern = QStringLiteral("Name: ");
            auto sizePattern = QStringLiteral("Size: ");
            //starts with fn, ends wit filesize.
            if (l.startsWith(fnPattern)){
                auto fn = l.mid(fnPattern.size());
                isImg = isImage(fn);
                cur.fileName = fn;
            }
            if (l.startsWith(sizePattern)){
                auto size = l.mid(sizePattern.size());
                cur.fileSize = size.toInt();
                cur.width = -1;
                cur.height = -1;
                if(isImg)
                    m_rarFileInfoList.push_back(cur);
            }
            //sort
        }
    }
    // auto len = m_rarFileInfoList.size();
    // for(auto i = 0; i< len; i++){
    //     auto cur = m_rarFileInfoList[i];
    // }
    QCollator collator;
    collator.setNumericMode(true);

    std::sort(
      this->m_rarFileInfoList.begin(), this->m_rarFileInfoList.end(),
      [&collator](const PageMetadata& file1, const PageMetadata& file2) {
          return collator.compare(file1.fileName, file2.fileName) < 0;
      });
}

RarComicSource::~RarComicSource() {}

int RarComicSource::getPageCount() const {
    return m_rarFileInfoList.count();
}

QPixmap RarComicSource::getPagePixmap(int pageNum) {

    auto cacheKey = QPair{id, pageNum};
    if(auto img = ImageCache::cache().getImage(cacheKey); !img.isNull())
        return img;

    QString imageFileName = this->m_rarFileInfoList[pageNum].fileName;
    QEventLoop evlp;
    QProcess proc;
    proc.start("unrar", {"p", "-inul", "-@", "--", this->path,imageFileName});
    QObject::connect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     &evlp, &QEventLoop::quit);
    evlp.exec();

    auto res = proc.waitForFinished();
    auto out = proc.readAllStandardOutput();
    QPixmap img;
    img.loadFromData(out);
    ImageCache::cache().addImage(cacheKey, img);
    return img;
    return {};
}

QString RarComicSource::getPageFilePath(int pageNum) {
    //extract file.
    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    if(tmp.open())
    {
        QPixmap img;
        QString imageFileName = this->getPageMetadata(pageNum).fileName;
        QEventLoop evlp;
        QProcess proc;
        proc.start("unrar", {"p", "-inul", "-@", "--", this->path,imageFileName});
        QObject::connect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         &evlp, &QEventLoop::quit);
        evlp.exec();
        auto out = proc.readAllStandardOutput();
        tmp.write(out);
        // tmp.write();
        tmp.close();
    }
    return tmp.fileName();
}

PageMetadata RarComicSource::getPageMetadata(int pageNum) {
    auto incache = [this](int page) -> bool{
        auto&& data = this->m_rarFileInfoList.at(page);
        if(data.width == -1 && data.height == -1)
            return false;
        return true;
    };
    assert(pageNum >= 0 &&
           pageNum < this->m_rarFileInfoList.count()
           );
    auto& res = m_rarFileInfoList[pageNum];
    if(res.width != -1 && res.height != -1)
        return res;

    auto px = getPagePixmap(pageNum);
    res.width = px.width();
    res.height = px.height();
    QMimeDatabase mdb;
    auto possibleMimes = mdb.mimeTypesForFileName(res.fileName);
    if(!possibleMimes.empty()) {
        res.fileType = possibleMimes[0].name();
    } else {
        res.fileType = "unknown";
    }

    res.valid = true;
    return res;
}

