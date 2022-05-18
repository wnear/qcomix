
#include <QDebug>
#include <quazip.h>
#include <quazipfile.h>
#include "comicsource.h"
#include <QDomDocument>
#include <QDomElement>

int readZipFileToDom(QDomDocument &domfile, QuaZip* zip, const QString& filename)
{
    auto zipfile = QuaZipFile(zip);
    zip->setCurrentFile(filename);
    zipfile.open(QIODevice::ReadOnly);
    domfile.setContent(zipfile.readAll());
    zipfile.close();
    return 0;
}

/**
 * @TODO
 * - parse <spine></spine> get idref, use idref as index for html page, then get image src from there.
 *
 * mimetype detection not needed. (maybe not needed, only image order is used in epub parsing)
 * zipsource is used as image scanner, epub/cbr, is used to order imgs or add metadata.
 */ 
EpubComicSource::EpubComicSource(const QString& path):ZipComicSource(path)
{
    QString protocolRootPath, realRootPath;
    
    protocolRootPath = "META-INF/container.xml";
    QDomDocument contentPage;
    readZipFileToDom(contentPage, this->zip, protocolRootPath);

    realRootPath  = contentPage.documentElement().firstChildElement().firstChildElement().attribute("full-path");

    if(1) {
        readZipFileToDom(contentPage, this->zip, realRootPath);
        auto item = contentPage.documentElement().firstChildElement("manifest").firstChildElement("item");

        QList<QuaZipFileInfo> li;
        while(!item.isNull()){
            if(!item.attribute("id").startsWith("Page")){
                item = item.nextSiblingElement();
                continue;
            }
            auto htmlPath = item.attribute("href");
            QDomDocument htmlPage;
            readZipFileToDom(htmlPage, this->zip, htmlPath);
            QString imgPath = htmlPage.documentElement().firstChildElement("body").firstChildElement().firstChildElement().firstChildElement().attribute("src").mid(3);
            auto _info = std::find_if(this->fileInfoList.begin(), this->fileInfoList.end(),
                    [imgPath](QuaZipFileInfo f) { return imgPath == f.name; });
            if(_info != this->fileInfoList.end()){
                li.push_back(*_info);
            }
            item = item.nextSiblingElement();
        }
        this->fileInfoList.clear();
        this->fileInfoList = li;
    } else {
        qDebug()<<"error to get contents file";
    }
}

// parsing:
// epub option: selected wanted image, gif/jpeg/png.
void EpubComicSource::resortFiles()
{

}

EpubComicSource::~EpubComicSource()
{

}