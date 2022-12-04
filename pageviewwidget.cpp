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

#include "pageviewwidget.h"
#include "comicsource.h"
#include "mainwindow.h"
#include "thumbnailwidget.h"
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QPainter>
#include <cmath>
#include <qnamespace.h>
#include <QMenu>

#include <QElapsedTimer>

constexpr int CHECKERED_IMAGE_SIZE = 1500;

PageViewWidget::FitMode PageViewWidget::stringToFitMode(const QString& str)
{
    if(str == "zoom")
        return FitMode::ManualZoom;
    if(str == "original-size")
        return FitMode::OriginalSize;
    if(str == "fixed-size")
        return FitMode::FixedSize;
    if(str == "best")
        return FitMode::FitBest;
    if(str == "width")
        return FitMode::FitWidth;
    if(str == "height")
        return FitMode::FitHeight;
    return FitMode::FitBest;
}
QString PageViewWidget::fitmodeToStr(PageViewWidget::FitMode x)
{
    QString res;
    switch(x){
        case FitMode::FitBest:
            res = "best";
            break;
        case FitMode::OriginalSize:
            res = "original-size";
            break;
        case FitMode::FitWidth:
            res = "width";
            break;
        case FitMode::FitHeight:
            res = "height";
            break;
        case FitMode::ManualZoom:
            res = "zoom";
            break;
        case FitMode::FixedSize:
            res = "fixed-size";
            break;
        default:
            res = "";
            break;
    }
    return res;
}

PageViewWidget::PageViewWidget(QWidget* parent) :
    QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(&this->slideShowTimer, &QTimer::timeout, [this]() {
        scrollNext(ScrollSource::SlideShowScroll);
    });
    connect(this, &PageViewWidget::zoomLevelChanged, [this](int val){
            qDebug()<< "zoom level changed to "<<val;
            });
    connect(this, &QWidget::customContextMenuRequested,
            this, &PageViewWidget::onCustomContextMenuRequested);
}

void PageViewWidget::onCustomContextMenuRequested(const QPoint& p)
{
    //@TODO: QPonter,
    auto *menu = new QMenu;

    /**
     * - save to disk
     * - rotate current.
     * - set as singlepage.
     * - single & rotate.
     * - shortcut to viewmode. //get from mainwindow.
     * - enhance.
     * -
     */
    return;
}

void PageViewWidget::initialize(ThumbnailWidget* w)
{
    active = true;
    mainViewBackground = MainWindow::getOption("mainViewBackground").toString();
    m_doublePageMode = MainWindow::getOption("doublePageMode").toBool();
    doublePageModeSingleStep = MainWindow::getOption("doublePageModeSingleStep").toBool();
    fitMode = stringToFitMode(MainWindow::getOption("fitMode").toString());
    stretchSmallImages = MainWindow::getOption("stretchSmallImages").toBool();
    stretchSmallImages = true;
    magnificationFactor = MainWindow::getOption("magnificationFactor").toDouble();
    magnifyingLensSize = MainWindow::getOption("magnifyingLensSize").toInt();
    hqTransformMode = MainWindow::getOption("hqTransformMode").toBool();
    showFirstPageAsCover = MainWindow::getOption("doNotShowFirstPageAsDouble").toBool();
    doNotShowWidePageAsDouble = MainWindow::getOption("doNotShowWidePageAsDouble").toBool();
    checkeredBackgroundForTransparency = MainWindow::getOption("checkeredBackgroundForTransparency").toBool();
    keepTransformationOnPageSwitch = MainWindow::getOption("keepTransformationOnPageSwitch").toBool();
    keepTransformationOnPageSwitch = false;
    mangaMode = MainWindow::getOption("mangaMode").toBool();
    slideShowSeconds = MainWindow::getOption("slideShowInterval").toInt();
    slideShowAutoOpenNextComic = MainWindow::getOption("slideShowAutoOpenNextComic").toBool();
    fixedSizeWidth = MainWindow::getOption("fixedSizeWidth").toInt();
    fixedSizeHeight = MainWindow::getOption("fixedSizeHeight").toInt();
    wheelScrollPixels = MainWindow::getOption("wheelScrollPixels").toInt();
    slideShowScrollPixels = MainWindow::getOption("slideShowScrollPixels").toInt();
    autoOpenNextComic = MainWindow::getOption("autoOpenNextComic").toBool();
    autoOpenNextComic = true;
    arrowKeyScrollPixels = MainWindow::getOption("arrowKeyScrollPixels").toInt();
    magnifyingLensHQScaling = MainWindow::getOption("magnifyingLensHQScaling").toBool();
    smartScroll = MainWindow::getOption("smartScroll").toBool();
    scrollsRequiredToFlip = MainWindow::getOption("stepsBeforePageFlip").toInt();
    flipPagesByScrolling = MainWindow::getOption("flipPagesByScrolling").toBool();
    smartScrollVFirst = MainWindow::getOption("smartScrollVerticalFirst").toBool();
    spaceScrollFraction = MainWindow::getOption("spaceScrollFraction").toDouble();
    stepsBeforePageFlip = MainWindow::getOption("stepsBeforePageFlip").toInt();
    useAdaptiveWheelScroll = MainWindow::getOption("useAdaptiveWheelScroll").toBool();
    useAdaptiveArrowKeyScroll = MainWindow::getOption("useAdaptiveArrowKeyScroll").toBool();
    adaptiveScrollOverhang = MainWindow::getOption("adaptiveScrollOverhang").toDouble();
    useAdaptiveSlideShowScroll = MainWindow::getOption("useAdaptiveSlideShowScroll").toBool();
    useAdaptiveSpaceScroll = MainWindow::getOption("useAdaptiveSpaceScroll").toBool();
    allowFreeDrag = MainWindow::getOption("allowFreeDrag").toBool();
    transparentBackgroundCheckerSize = MainWindow::getOption("checkerBoardPatternSize").toInt();

    QImage tmpCheckeredBkg = QImage(QSize(CHECKERED_IMAGE_SIZE, CHECKERED_IMAGE_SIZE), QImage::Format_ARGB32);
    tmpCheckeredBkg.fill(Qt::white);
    QPainter painter(&tmpCheckeredBkg);
    for(int i = 0; i < CHECKERED_IMAGE_SIZE / transparentBackgroundCheckerSize; i++)
    {
        for(int j = 0; j < CHECKERED_IMAGE_SIZE / transparentBackgroundCheckerSize; j++)
        {
            if(i % 2 != j % 2)
            {
                painter.fillRect(i * transparentBackgroundCheckerSize, j * transparentBackgroundCheckerSize, transparentBackgroundCheckerSize, transparentBackgroundCheckerSize, QColor(204, 204, 204));
            }
        }
    }
    painter.end();
    checkeredBkg = QPixmap::fromImage(tmpCheckeredBkg);

    setAutoFillBackground(false);
    this->thumbsWidget = w;
    if(this->thumbsWidget) this->thumbsWidget->initialize();

    emit this->pageViewConfigUINeedsToBeUpdated();
    emitStatusbarUpdateSignal();
    update();
}

void PageViewWidget::rotate(int degree)
{
    rotationDegree += degree;
    emit this->pageViewConfigUINeedsToBeUpdated();
    maintainCache(cacheKey::leftPageTransformed);
}

void PageViewWidget::flipHorizontally(bool flip)
{
    horizontalFlip = flip;
    emit this->pageViewConfigUINeedsToBeUpdated();
    maintainCache(cacheKey::leftPageTransformed);
}

void PageViewWidget::flipVertically(bool flip)
{
    verticalFlip = flip;
    emit this->pageViewConfigUINeedsToBeUpdated();
    maintainCache(cacheKey::leftPageTransformed);
}

void PageViewWidget::zoomIn()
{
    //fitMode = FitMode::ManualZoom;
    //emit this->fitModeChanged(FitMode::ManualZoom);
    zoomLevel++;
    currentX *= 1.1;
    currentY *= 1.1;
    maintainCache(cacheKey::leftPageFitted);
    emit this->pageViewConfigUINeedsToBeUpdated();
    emitStatusbarUpdateSignal();
}

void PageViewWidget::zoomOut()
{
    //fitMode = FitMode::ManualZoom;
    //emit this->fitModeChanged(FitMode::ManualZoom);
    zoomLevel--;
    currentX *= 1.0 / 1.1;
    currentY *= 1.0 / 1.1;
    maintainCache(cacheKey::leftPageFitted);
    emit this->pageViewConfigUINeedsToBeUpdated();
    emitStatusbarUpdateSignal();
}

void PageViewWidget::resetZoom()
{
    auto zFactor = calcZoomScaleFactor();
    currentX *= zFactor;
    currentY *= zFactor;
    zoomLevel = 0;
    maintainCache(cacheKey::leftPageFitted);
    emit this->pageViewConfigUINeedsToBeUpdated();
    emitStatusbarUpdateSignal();
}

void PageViewWidget::goToPage(int page, int doublePage)
{
    if(m_comic == nullptr || page == currPage )
        return;

    if(!m_comic->isValidPage(page-1))
        return;
    if(doublePage == -1){
        m_isDoublePage = false;
        do {
            if(!isDoublePageMode())
                break ;
            // if(doublePageModeSingleStep)
            //     break;

            int pn_a = page-1;
            int pn_b = pn_a+1;
            if(isSinglePageByPageMeta(pn_a))
                break;
            if(!m_comic->isValidPage(pn_b))
                break;
            if(isSinglePageByPageMeta(pn_b))
                break;
            m_isDoublePage = true;
        } while(0);
    } else {
        m_isDoublePage = doublePage == 1? true:false;
    }

    this->setCurrentPage_Internal(page);
    resetTransformation();
    emit this->pageViewConfigUINeedsToBeUpdated();
}

void PageViewWidget::lastPage()
{
    if(m_comic)
        this->goToPage(m_comic->getPageCount());
    emit this->pageViewConfigUINeedsToBeUpdated();
}

void PageViewWidget::firstPage()
{
    this->goToPage(1);
    // emit this->pageViewConfigUINeedsToBeUpdated();
}

void PageViewWidget::nextPage(bool slideShow)
{
    //TODO:
    bool isEnd = ( m_isDoublePage && currPage >= this->m_comic->getPageCount()-0)
               ||(!m_isDoublePage && currPage >= this->m_comic->getPageCount()-1);
    if(isEnd){
        emit requestLoadNextComic();
        return;
    }
    if(this->doublePageModeSingleStep){
        this->goToPage(currPage+1);
    } else
        this->goToPage(m_isDoublePage&&!this->doublePageModeSingleStep
                       ? currPage +2 : currPage +1);
}

void PageViewWidget::toggleSlideShow(bool enabled)
{
    if(!enabled) {
        this->slideShowTimer.stop();
    } else {
        this->slideShowTimer.setInterval(1000 * slideShowSeconds);
        this->slideShowTimer.start();
    }
}

void PageViewWidget::setSlideShowSeconds(int seconds)
{
    this->slideShowSeconds = seconds;
}

void PageViewWidget::previousPage()
{
    //TODO:
    //isStart page ,request for previous comic.
    if(currPage <=1){
        emit requestLoadPrevComic();
        return;
    }
    // bool isStart = ;
    int page = currPage -1;
    if(! m_comic->isValidPage(page))
        return;
    do {
        m_isDoublePage = false;
        if(m_doublePageMode == false)
            break;
        if(isSinglePageByPageMeta(page-1)){
            break;
        }
        // if(this->doublePageModeSingleStep)
        //     break;

        int secondPage = page-1;
        if(isSinglePageByPageMeta(secondPage-1))
            break;
        if(secondPage == 0 && showFirstPageAsCover)
            break;
        page = secondPage;
        m_isDoublePage = true;
    } while(0);

    if(doublePageModeSingleStep){
        this->goToPage(currPage - 1);
    } else
        this->goToPage(page, m_isDoublePage?1:0);
}

void PageViewWidget::setFitMode(FitMode mode)
{
    if(fitMode != mode)
    {
        currentX = 0;
        currentXWasReset = true;
        currentY = 0;
        zoomLevel = 0;
    }
    if(mode == FitMode::ManualZoom)
    {
        draggingXEnabled = true;
        draggingYEnabled = true;
    }
    else
    {
        draggingXEnabled = false;
        draggingYEnabled = false;
    }
    fitMode = mode;
    if(mode != FitMode::ManualZoom)
    {
        maintainCache(cacheKey::leftPageTransformed);
        fitModeJustChanged = true;
    }
    else
    {
        update();
    }
    emitStatusbarUpdateSignal();
    emit this->pageViewConfigUINeedsToBeUpdated();
}

bool PageViewWidget::onFirstPage() const
{
    return !m_comic || currPage < 2;
}

bool PageViewWidget::onLastPage() const
{
    return !m_comic || currPage == m_comic->getPageCount() || currPage == 0;
}

bool PageViewWidget::verticallyFlipped() const
{
    return this->verticalFlip;
}

bool PageViewWidget::magnifyingLensEnabled() const
{
    return this->magnify;
}

bool PageViewWidget::horizontallyFlipped() const
{
    return this->horizontalFlip;
}

void PageViewWidget::setKeepTransformation(bool keep)
{
    keepTransformationOnPageSwitch = keep;
    emit this->pageViewConfigUINeedsToBeUpdated();
}

void PageViewWidget::setStretchSmallImages(bool stretch)
{
    stretchSmallImages = stretch;
    emit this->pageViewConfigUINeedsToBeUpdated();
    maintainCache(cacheKey::leftPageFitted);
}

void PageViewWidget::setCheckeredBackgroundForTransparency(bool checkered)
{
    checkeredBackgroundForTransparency = checkered;
    emit this->pageViewConfigUINeedsToBeUpdated();
    maintainCache(cacheKey::leftPageTransformed);
}

void PageViewWidget::setMangaMode(bool enabled)
{
    this->mangaMode = enabled;
    emit this->pageViewConfigUINeedsToBeUpdated();
    this->updateImageMetadata();
    maintainCache(cacheKey::leftPageRaw);
}

void PageViewWidget::setDoublePageMode(bool doublePage)
{
    m_doublePageMode = doublePage;
    emit this->pageViewConfigUINeedsToBeUpdated();
    maintainCache(cacheKey::leftPageRaw);
}

void PageViewWidget::setMagnifyingLensEnabled(bool enabled)
{
    this->magnify = enabled;
    emit this->pageViewConfigUINeedsToBeUpdated();
    if(enabled)
    {
        this->setCursor(Qt::BlankCursor);
    }
    else
    {
        this->setCursor(Qt::ArrowCursor);
    }
    update();
}

void PageViewWidget::currentPageToClipboard()
{
    renderCombinedImage = true;
    this->repaint();
    renderCombinedImage = false;
    QApplication::clipboard()->setPixmap(this->cachedCombinedImage);
    this->cachedCombinedImage = {};
}

void PageViewWidget::setSmartScroll(bool enabled)
{
    this->smartScroll = enabled;
    emit this->pageViewConfigUINeedsToBeUpdated();
}

void PageViewWidget::setSmartScrollVerticalFirst(bool vfirst)
{
    this->smartScrollVFirst = vfirst;
    emit this->pageViewConfigUINeedsToBeUpdated();
}

ComicSource* PageViewWidget::setComicSource(ComicSource* src)
{
    auto oldComic = m_comic;

    m_comic = src;
    emit this->archiveMetadataUpdateNeeded(m_comic? m_comic->getComicMetadata():ComicMetadata{});

    maintainCache(cacheKey::dropAll);

    if(this->thumbsWidget)
        this->thumbsWidget->setComicSource(src);

    if(m_comic)
    {
        auto startPageNum = m_comic->startAtPage();
        if(startPageNum == -1)
        {
            if(MainWindow::getOption("rememberPage").toBool())
            {
                auto savedPos = MainWindow::getSavedPositionForFilePath(m_comic->getFilePath());
                if(m_comic->isValidPage(savedPos - 1))
                {
                    startPageNum = savedPos;
                }
            }
            else if(m_comic->getPageCount() > 0)
            {
                startPageNum = 1;
            }
        }
        this->goToPage(startPageNum);
    }

    return oldComic;
}

bool PageViewWidget::isDoublePageMode() const
{
    return this->m_doublePageMode;
}

bool PageViewWidget::isMangaMode() const
{
    return this->mangaMode;
}

bool PageViewWidget::stretchesSmallImages() const
{
    return this->stretchSmallImages;
}

bool PageViewWidget::smartScrollVerticalFirst() const
{
    return this->smartScrollVFirst;
}

bool PageViewWidget::smartScrollEnabled() const
{
    return this->smartScroll;
}

bool PageViewWidget::transformationKept() const
{
    return this->keepTransformationOnPageSwitch;
}

void PageViewWidget::setAllowFreeDrag(bool enabled)
{
    this->allowFreeDrag = enabled;
    this->ensureDisplacementWithinAllowedBounds();
    emit this->pageViewConfigUINeedsToBeUpdated();
    update();
}

bool PageViewWidget::freeDragAllowed() const
{
    return this->allowFreeDrag;
}

int PageViewWidget::slideShowInterval() const
{
    return slideShowSeconds;
}

ComicSource* PageViewWidget::comicSource()
{
    return this->m_comic;
}

int PageViewWidget::currentPage() const
{
    return currPage;
}

QString PageViewWidget::currentPageFilePath()
{
    if(this->m_comic && currPage > 0)
    {
        return this->m_comic->getPageFilePath(currPage - 1);
    }
    return {};
}

void PageViewWidget::setHorizontalScrollPosition(int pos)
{
    this->currentX = pos;
    ensureDisplacementWithinAllowedBounds();
    update();
}

void PageViewWidget::setVerticalScrollPosition(int pos)
{
    this->currentY = pos;
    ensureDisplacementWithinAllowedBounds();
    update();
}

void PageViewWidget::resetTransformation(bool force)
{
    if(active)
    {
        lastDrawnLeftHeight = 0;
        lastDrawnRightHeight = 0;
        if(!keepTransformationOnPageSwitch || force)
        {
            this->rotationDegree = 0;
            this->horizontalFlip = false;
            this->verticalFlip = false;
            this->zoomLevel = 0;
            this->cachedZoomBaseLeftImageSize = QSize{};
            this->cachedZoomBaseRightImageSize = QSize{};
            if(fitMode == FitMode::ManualZoom)
            {
                setFitMode(stringToFitMode(MainWindow::getOption("fitMode").toString()));
                emit this->fitModeChanged(fitMode);
            }
        }
        maintainCache(cacheKey::leftPageTransformed);
        this->currentX = 0;
        this->currentXWasReset = true;
        this->currentY = 0;
    }
}

QPixmap PageViewWidget::getRegionFromCombinedPixmap(const QPixmap& left, const QPixmap& right, int x, int y, int w, int h, const QColor& bkgColor)
{
    int combined_height = std::max(left.height(), right.height());
    int rightYOffset = (combined_height - right.height()) / 2.0;
    int leftYOffset = (combined_height - left.height()) / 2.0;
    int leftX = 0, leftY = 0, leftW = 0, leftH = 0, rightX = 0, rightY = 0, rightW = 0, rightH = 0;
    if(x < left.width())
    {
        leftX = x;
        leftW = std::min(left.width() - x, w);
        rightW = w - leftW;
    }
    else
    {
        rightX = x - left.width();
        rightW = w;
    }
    leftY = y - leftYOffset;
    leftH = y - leftYOffset + h;
    rightY = y - rightYOffset;
    rightH = y - rightYOffset + h;

    QPixmap res(w, h);
    QPainter p(&res);
    p.fillRect(res.rect(), bkgColor);
    if(leftW > 0 && leftH > 0) p.drawPixmap(QPointF(0, std::max(0, -leftY)), left, QRectF(leftX, leftY, leftW, leftH));
    if(rightW > 0 && rightH > 0) p.drawPixmap(QPointF(leftW, std::max(0, -rightY)), right, QRectF(rightX, rightY, rightW, rightH));
    p.end();
    return res;
}

void PageViewWidget::fitLeftRightImageToSize(int width, int height, int combined_width, int combined_height, double& leftScaledWidth, double& rightScaledWidth, double& leftScaledHeight, double& rightScaledHeight)
{
    double proportion = 1.0;
    bool shrunk = false;
    bool heightLimit = false;
    if(combined_width > width)
    {
        shrunk = true;
        proportion = width / double(combined_width);
        leftScaledWidth *= proportion;
        rightScaledWidth *= proportion;
        leftScaledHeight *= proportion;
        rightScaledHeight *= proportion;
    }
    if(proportion * combined_height > height)
    {
        shrunk = true;
        heightLimit = true;
        proportion = height / (proportion * combined_height);
        leftScaledWidth *= proportion;
        rightScaledWidth *= proportion;
        leftScaledHeight *= proportion;
        rightScaledHeight *= proportion;
    }

    if(stretchSmallImages && !shrunk)
    {
        proportion = width / double(combined_width);
        if(proportion * combined_height > height)
        {
            heightLimit = true;
            proportion = height / double(combined_height);
        }
        leftScaledWidth *= proportion;
        rightScaledWidth *= proportion;
        leftScaledHeight *= proportion;
        rightScaledHeight *= proportion;
    }

    leftScaledWidth = std::ceil(leftScaledWidth);
    rightScaledWidth = std::ceil(rightScaledWidth);

    if(heightLimit)
    {
        while(leftScaledHeight > height) leftScaledHeight--;
        while(rightScaledHeight > height) rightScaledHeight--;
        while(std::max(leftScaledHeight, rightScaledHeight) < height)
        {
            leftScaledHeight++;
            rightScaledHeight++;
        }
    }
    else
    {
        int c = 0;
        while(leftScaledWidth + rightScaledWidth < width)
        {
            if(rightScaledWidth > 0 && c % 2 == 0)
            {
                rightScaledWidth++;
            }
            else
            {
                leftScaledWidth++;
            }
            c++;
        }

        while(leftScaledWidth + rightScaledWidth > width)
        {
            if(rightScaledWidth > 0 && c % 2 == 0)
            {
                rightScaledWidth--;
            }
            else
            {
                leftScaledWidth--;
            }
            c++;
        }
    }
}

struct ImageInfo {
    QPixmap image;
    QPixmap imageScreen;
    int pagenum;
    QSize size;
    QPoint position;
    QPoint center;
};

//maybe onepage/two, or more.
void preparePageBeforeUpdate_getpage(void){
    QVector<int> cur;

    int curmax, curmin;
    bool doublepage;
    bool ltor; //mangaMode;
    bool forward = true;
    int pagecount;
    QVector<int> toconsider;
    if (ltor) {
    }
    if( forward ) {
        int cur = curmin;
        for(int i = 1; i<=2; i++){
            if(1){
                toconsider.append(cur-i);
            }
        }
    } else {
        int cur = curmax;
        for(int i = 1; i<=2; i++){
            if(1)
                toconsider.append(cur+i);
        }
    }

    if( toconsider[0] || toconsider[1] ){ //isSinglePage;
        toconsider.pop_back();
    }
    if( ltor && toconsider.size() > 1){
        std::reverse(toconsider.begin(), toconsider.end());
    }
    for(auto i: cur){

    }
    //transform:
}

void PageViewWidget::paintEvent(QPaintEvent* event)
{
    //check comicsource
    if(this->m_comic == nullptr || !m_comic->isValidPage(this->currPage - 1))
        return;
    //check this
    if(active == false){
        QWidget::paintEvent(event);
        return;
    }
    QElapsedTimer x;
    x.start();
    QPainter painter(this);

    // <<-- 1. calculate background color.
    QColor finalBkgColor;
    QColor color;
    auto a = dynamicBackground;
    auto b = QColor(mainViewBackground);
    auto c = this->palette().color(QPalette::Window);
    color = a.isValid()? a : (b.isValid()?b:c);
    painter.fillRect(painter.viewport(), color);
    finalBkgColor = color;

    // >>-- 1. calculate background color. ....end...

    int width = this->width();
    int height = this->height();
    int targetX = 0;
    int targetY = 0;

    // -- 2. get imgCache[pageRaw] data.
    if(imgCache[cacheKey::leftPageRaw].isNull()) {
        imgCache[cacheKey::leftPageRaw] = m_comic->getPagePixmap(currPage - 1);
    }

    bool doublePage = m_isDoublePage;
    if(doublePage && imgCache[cacheKey::rightPageRaw].isNull())
    {
        imgCache[cacheKey::rightPageRaw] = m_comic->getPagePixmap(currPage);

        if(mangaMode)
        {
            imgCache[cacheKey::rightPageRaw].swap(imgCache[cacheKey::leftPageRaw]);
        }
    }

    // -- 3. imgCache[pageTransformed], if rotation and flip needed.
    if(imgCache[cacheKey::leftPageTransformed].isNull() || (!dynamicBackground.isValid() && mainViewBackground == "dynamic"))
    {
        QTransform transform;
        transform.rotate(rotationDegree);
        transform.scale(horizontalFlip ? -1.0 : 1.0, verticalFlip ? -1.0 : 1.0);

        imgCache[cacheKey::leftPageTransformed] = imgCache[cacheKey::leftPageRaw].transformed(transform, hqTransformMode ? Qt::SmoothTransformation : Qt::FastTransformation);
        if(doublePage)
            imgCache[cacheKey::rightPageTransformed] = imgCache[cacheKey::rightPageRaw].transformed(transform, hqTransformMode ? Qt::SmoothTransformation : Qt::FastTransformation);

        if(!dynamicBackground.isValid() && mainViewBackground == "dynamic")
        {
            dynamicBackground = MainWindow::getMostCommonEdgeColor(imgCache[cacheKey::leftPageTransformed].toImage(), imgCache[cacheKey::rightPageTransformed].toImage());
            painter.fillRect(painter.viewport(), dynamicBackground);
            finalBkgColor = dynamicBackground;
        }

        QPixmap leftBkg, rightBkg;
        if(checkeredBackgroundForTransparency)
        {
            leftBkg = getCheckeredBackground(imgCache[cacheKey::leftPageTransformed].width(), imgCache[cacheKey::leftPageTransformed].height());
            if(doublePage) rightBkg = getCheckeredBackground(imgCache[cacheKey::rightPageTransformed].width(), imgCache[cacheKey::rightPageTransformed].height());
        }
        else
        {
            leftBkg = QPixmap(imgCache[cacheKey::leftPageTransformed].size());
            if(doublePage) rightBkg = QPixmap(imgCache[cacheKey::rightPageTransformed].size());
            leftBkg.fill(Qt::transparent);
            if(doublePage) rightBkg.fill(Qt::transparent);
        }
        QPainter leftPainter(&leftBkg);
        leftPainter.drawPixmap(0, 0, imgCache[cacheKey::leftPageTransformed]);
        leftPainter.end();

        if(doublePage)
        {
            QPainter rightPainter(&rightBkg);
            rightPainter.drawPixmap(0, 0, imgCache[cacheKey::rightPageTransformed]);
            rightPainter.end();
        }

        imgCache[cacheKey::leftPageTransformed] = leftBkg;
        imgCache[cacheKey::rightPageTransformed] = rightBkg;
        updtWindowIcon = true;
    }

    if(updtWindowIcon)
    {
        emit windowIconUpdateNeeded(imgCache[cacheKey::leftPageTransformed]);
        updtWindowIcon = false;
    }

    auto combined_width = imgCache[cacheKey::leftPageTransformed].width() + imgCache[cacheKey::rightPageTransformed].width();
    auto combined_height = std::max(imgCache[cacheKey::leftPageTransformed].height(), imgCache[cacheKey::rightPageTransformed].height());

    // -- extra, save current presented image to clipboard.
    if(renderCombinedImage)
    {
        QPixmap img_combined;

        if(checkeredBackgroundForTransparency)
        {
            img_combined = getCheckeredBackground(combined_width, combined_height);
        }
        else
        {
            img_combined = QPixmap(combined_width, combined_height);
            img_combined.fill(Qt::transparent);
        }

        QPainter combined_painter(&img_combined);

        combined_painter.drawPixmap(0,
                                    (img_combined.height() - imgCache[cacheKey::leftPageTransformed].height()) / 2.0,
                                    imgCache[cacheKey::leftPageTransformed]);
        if(!imgCache[cacheKey::rightPageTransformed].isNull()){
            //combined_painter.
            combined_painter.setPen(Qt::black);
            combined_painter.drawLine(  imgCache[cacheKey::leftPageTransformed].width(),
                                        (img_combined.height() - imgCache[cacheKey::rightPageTransformed].height()) / 2.0,
                                        imgCache[cacheKey::leftPageTransformed].width(),
                                        this->height());
            combined_painter.drawPixmap(imgCache[cacheKey::leftPageTransformed].width(),
                                        (img_combined.height() - imgCache[cacheKey::rightPageTransformed].height()) / 2.0,
                                        imgCache[cacheKey::rightPageTransformed]);
        }
        combined_painter.end();
        cachedCombinedImage = img_combined;
    }

    // -- 4. time to draw to the display.
    auto scaleMode = hqTransformMode ? Qt::SmoothTransformation : Qt::FastTransformation;

    if(imgCache[cacheKey::leftPageFitted].isNull())
    {
        if(fitMode == FitMode::FitHeight)
        {
            int temp = height;
            height *= calcZoomScaleFactor();
            if(imgCache[cacheKey::leftPageTransformed].height() > height || stretchSmallImages) {
                imgCache[cacheKey::leftPageFitted] = imgCache[cacheKey::leftPageTransformed].scaledToHeight(height, scaleMode);
            } else {
                imgCache[cacheKey::leftPageFitted] = imgCache[cacheKey::leftPageTransformed];
            }

            if(imgCache[cacheKey::rightPageTransformed].height() > height || stretchSmallImages) {
                imgCache[cacheKey::rightPageFitted] = imgCache[cacheKey::rightPageTransformed].scaledToHeight(height, scaleMode);
            } else {
                imgCache[cacheKey::rightPageFitted] = imgCache[cacheKey::rightPageTransformed];
            }
            cachedZoomBaseLeftImageSize = imgCache[cacheKey::leftPageFitted].size();
            cachedZoomBaseRightImageSize = imgCache[cacheKey::rightPageFitted].size();
            height = temp;
        }
        else if(fitMode == FitMode::FitWidth)
        {
            int temp = width;
            width *= calcZoomScaleFactor();
            if(combined_width > width || stretchSmallImages)
            {
                double leftProportion = double(imgCache[cacheKey::leftPageTransformed].width()) / double(combined_width);
                int leftScaledWidth = width * leftProportion;
                int rightScaledWidth = width - leftScaledWidth;
                if(imgCache[cacheKey::rightPageTransformed].isNull())
                {
                    rightScaledWidth = 0;
                    leftScaledWidth = width;
                }
                imgCache[cacheKey::leftPageFitted] = imgCache[cacheKey::leftPageTransformed].scaledToWidth(
                                                        leftScaledWidth, scaleMode);
                if(doublePage)
                    imgCache[cacheKey::rightPageFitted] = imgCache[cacheKey::rightPageTransformed].scaledToWidth(
                                                                rightScaledWidth, scaleMode);
            }
            else
            {
                imgCache[cacheKey::leftPageFitted] = imgCache[cacheKey::leftPageTransformed];
                imgCache[cacheKey::rightPageFitted] = imgCache[cacheKey::rightPageTransformed];
            }
            cachedZoomBaseLeftImageSize = imgCache[cacheKey::leftPageFitted].size();
            cachedZoomBaseRightImageSize = imgCache[cacheKey::rightPageFitted].size();
            width = temp;
        }
        else if(fitMode == FitMode::FitBest)
        {
            if(!(combined_width < width && combined_height < height) || stretchSmallImages)
            {
                double leftScaledWidth = imgCache[cacheKey::leftPageTransformed].width();
                double rightScaledWidth = imgCache[cacheKey::rightPageTransformed].width();
                double leftScaledHeight = imgCache[cacheKey::leftPageTransformed].height();
                double rightScaledHeight = imgCache[cacheKey::rightPageTransformed].height();

                fitLeftRightImageToSize(width, height,
                                        combined_width, combined_height,
                                        leftScaledWidth, rightScaledWidth,
                                        leftScaledHeight, rightScaledHeight);

                imgCache[cacheKey::leftPageFitted] = imgCache[cacheKey::leftPageTransformed].scaled(
                                                            leftScaledWidth,
                                                            leftScaledHeight,
                                                            Qt::IgnoreAspectRatio,
                                                            scaleMode);
                if(doublePage)
                    imgCache[cacheKey::rightPageFitted] = imgCache[cacheKey::rightPageTransformed].scaled(
                                                                rightScaledWidth,
                                                                rightScaledHeight,
                                                                Qt::IgnoreAspectRatio,
                                                                scaleMode);
            }
            else
            {
                imgCache[cacheKey::leftPageFitted] = imgCache[cacheKey::leftPageTransformed];
                imgCache[cacheKey::rightPageFitted] = imgCache[cacheKey::rightPageTransformed];
            }
            cachedZoomBaseLeftImageSize = imgCache[cacheKey::leftPageFitted].size();
            cachedZoomBaseRightImageSize = imgCache[cacheKey::rightPageFitted].size();
        }
        else if(fitMode == FitMode::OriginalSize)
        {
            imgCache[cacheKey::leftPageFitted] = imgCache[cacheKey::leftPageTransformed];
            imgCache[cacheKey::rightPageFitted] = imgCache[cacheKey::rightPageTransformed];
            cachedZoomBaseLeftImageSize = imgCache[cacheKey::leftPageFitted].size();
            cachedZoomBaseRightImageSize = imgCache[cacheKey::rightPageFitted].size();
        }
        else if(fitMode == FitMode::FixedSize)
        {
            if(!(combined_width < fixedSizeWidth && combined_height < fixedSizeHeight) || stretchSmallImages)
            {
                double leftScaledWidth = imgCache[cacheKey::leftPageTransformed].width();
                double rightScaledWidth = imgCache[cacheKey::rightPageTransformed].width();
                double leftScaledHeight = imgCache[cacheKey::leftPageTransformed].height();
                double rightScaledHeight = imgCache[cacheKey::rightPageTransformed].height();

                fitLeftRightImageToSize(fixedSizeWidth, fixedSizeHeight, combined_width, combined_height, leftScaledWidth, rightScaledWidth, leftScaledHeight, rightScaledHeight);

                imgCache[cacheKey::leftPageFitted] = imgCache[cacheKey::leftPageTransformed].scaled(leftScaledWidth, leftScaledHeight, Qt::IgnoreAspectRatio, scaleMode);
                if(doublePage)
                    imgCache[cacheKey::rightPageFitted] = imgCache[cacheKey::rightPageTransformed].scaled(rightScaledWidth, rightScaledHeight, Qt::IgnoreAspectRatio, scaleMode);
            }
            else
            {
                imgCache[cacheKey::leftPageFitted] = imgCache[cacheKey::leftPageTransformed];
                if(doublePage)
                    imgCache[cacheKey::rightPageFitted] = imgCache[cacheKey::rightPageTransformed];
            }
            cachedZoomBaseLeftImageSize = imgCache[cacheKey::leftPageFitted].size();
            cachedZoomBaseRightImageSize = imgCache[cacheKey::rightPageFitted].size();
        }
        else if(fitMode == FitMode::ManualZoom)
        {
            auto zoomScaleFactor = calcZoomScaleFactor();
            auto zoomBaseLeftImageSize = cachedZoomBaseLeftImageSize.isNull()
                                            ? imgCache[cacheKey::leftPageTransformed].size()
                                            : cachedZoomBaseLeftImageSize;
            auto zoomBaseRightImageSize = cachedZoomBaseRightImageSize.isNull()
                                    ? imgCache[cacheKey::rightPageTransformed].size()
                                    : cachedZoomBaseRightImageSize;

            imgCache[cacheKey::leftPageFitted] = imgCache[cacheKey::leftPageTransformed].scaled(
                                            zoomScaleFactor * zoomBaseLeftImageSize.width(),
                                            zoomScaleFactor * zoomBaseLeftImageSize.height(),
                                            Qt::KeepAspectRatio,
                                            hqTransformMode ? Qt::SmoothTransformation : Qt::FastTransformation);

            if(doublePage)
                imgCache[cacheKey::rightPageFitted] = imgCache[cacheKey::rightPageTransformed].scaled(
                                            zoomScaleFactor * zoomBaseRightImageSize.width(),
                                            zoomScaleFactor * zoomBaseRightImageSize.height(),
                                            Qt::KeepAspectRatio,
                                            hqTransformMode ? Qt::SmoothTransformation : Qt::FastTransformation);
        }
    }

    combined_width = imgCache[cacheKey::leftPageFitted].width() + imgCache[cacheKey::rightPageFitted].width();
    combined_height = std::max(imgCache[cacheKey::leftPageFitted].height(), imgCache[cacheKey::rightPageFitted].height());
    if(lastDrawnLeftHeight != imgCache[cacheKey::leftPageFitted].height())
    {
        lastDrawnLeftHeight = imgCache[cacheKey::leftPageFitted].height();
        emitStatusbarUpdateSignal();
    }
    if(lastDrawnRightHeight != imgCache[cacheKey::rightPageFitted].height())
    {
        lastDrawnRightHeight = imgCache[cacheKey::rightPageFitted].height();
        emitStatusbarUpdateSignal();
    }

    if(int w_diff = width - combined_width; w_diff > 0)
    {
        targetX = w_diff / 2;
        draggingXEnabled = false;
        allowedXDisplacement = 0;
    }
    else
    {
        draggingXEnabled = true;
        allowedXDisplacement = -w_diff;
    }

    if(int h_diff = height - combined_height; h_diff > 0)
    {
        targetY = h_diff / 2;
        draggingYEnabled = false;
        allowedYDisplacement = 0;
    }
    else
    {
        draggingYEnabled = true;
        allowedYDisplacement = -h_diff;
    }

    ensureDisplacementWithinAllowedBounds();

    if(mangaMode && currentXWasReset && currentX == 0)
    {
        currentX = allowedXDisplacement;
        currentXWasReset = false;
    }
    else if(currentXWasReset)
    {
        currentXWasReset = false;
    }

    emit this->updateHorizontalScrollBar(allowedXDisplacement, currentX, std::min(width, combined_width));
    emit this->updateVerticalScrollBar(allowedYDisplacement, currentY, std::min(height, combined_height));

    painter.drawPixmap(targetX - currentX, targetY - currentY + (combined_height - imgCache[cacheKey::leftPageFitted].height()) / 2.0, imgCache[cacheKey::leftPageFitted]);
    if(!imgCache[cacheKey::rightPageFitted].isNull())
    {
        painter.drawPixmap(targetX - currentX + imgCache[cacheKey::leftPageFitted].width(), targetY - currentY + (combined_height - imgCache[cacheKey::rightPageFitted].height()) / 2.0, imgCache[cacheKey::rightPageFitted]);
    }

    lastDrawnImageFullSize = QSize(combined_width, combined_height);

    if(magnify && mouseCurrentlyOverWidget)
    {
        auto sourceLength = magnificationFactor * magnifyingLensSize;
        QPixmap sourceImg = getRegionFromCombinedPixmap(imgCache[cacheKey::leftPageFitted], imgCache[cacheKey::rightPageFitted],
                                                        mousePos.x() - targetX + currentX - sourceLength / 2.0,
                                                        mousePos.y() - targetY + currentY - sourceLength / 2.0,
                                                        sourceLength, sourceLength, finalBkgColor)
                              .scaled(magnifyingLensSize, magnifyingLensSize, Qt::IgnoreAspectRatio,
                                      magnifyingLensHQScaling ? Qt::SmoothTransformation : Qt::FastTransformation);
        painter.fillRect(mousePos.x() - magnifyingLensSize / 2.0, mousePos.y() - magnifyingLensSize / 2.0, magnifyingLensSize, magnifyingLensSize, finalBkgColor);
        painter.drawPixmap(mousePos.x() - magnifyingLensSize / 2.0, mousePos.y() - magnifyingLensSize / 2.0, sourceImg);
        painter.setPen(Qt::black);
        painter.drawRect(mousePos.x() - magnifyingLensSize / 2.0, mousePos.y() - magnifyingLensSize / 2.0, magnifyingLensSize, magnifyingLensSize);
    }
}

void PageViewWidget::mousePressEvent(QMouseEvent* event)
{
    dragging = true;
    currentXWasReset = false;
    dragStartX = currentX + event->x();
    dragStartY = currentY + event->y();
    if(!magnify)
    {
        this->setCursor(Qt::DragMoveCursor);
    }
}

void PageViewWidget::mouseReleaseEvent(QMouseEvent*)
{
    dragging = false;
    if(!magnify)
    {
        this->setCursor(Qt::ArrowCursor);
    }
}

void PageViewWidget::mouseMoveEvent(QMouseEvent* event)
{
    if(dragging)
    {
        currentXWasReset = false;
        if(draggingXEnabled || allowFreeDrag)
        {
            currentX = dragStartX - event->x();
        }
        if(draggingYEnabled || allowFreeDrag)
        {
            currentY = dragStartY - event->y();
        }
        ensureDisplacementWithinAllowedBounds();
        if(draggingXEnabled || draggingYEnabled || allowFreeDrag) update();
    }
    if(magnify)
    {
        mousePos = event->pos();
        update();
    }
}

void PageViewWidget::wheelEvent(QWheelEvent* event)
{
    if(event->modifiers() & Qt::ControlModifier)
    {
        if(event->angleDelta().y() > 0)
        {
            zoomIn();
        }
        else if(event->angleDelta().y() < 0)
        {
            zoomOut();
        }
    }
    else
    {
        if(event->angleDelta().y() > 0)
        {
            scrollPrev(ScrollSource::WheelScroll);
        }
        else if(event->angleDelta().y() < 0)
        {
            scrollNext(ScrollSource::WheelScroll);
        }
        if(event->angleDelta().x() > 0)
        {
            scrollInDirection(ScrollDirection::Left, ScrollSource::WheelScroll);
        }
        else if(event->angleDelta().x() < 0)
        {
            scrollInDirection(ScrollDirection::Right, ScrollSource::WheelScroll);
        }
    }
}

void PageViewWidget::keyPressEvent(QKeyEvent* event)
{
    bool forceSingleByPressShift =!! (event->modifiers()&Qt::ShiftModifier);
    if(forceSingleByPressShift) { this->doublePageModeSingleStep = true; }
    if(event->key() == Qt::Key_Up)
    {
        scrollPrev(ScrollSource::ArrowKeyScroll);
    }
    else if(event->key() == Qt::Key_Down)
    {
        scrollNext(ScrollSource::ArrowKeyScroll);
    }
    else if(event->key() == Qt::Key_Left)
    {
        scrollInDirection(ScrollDirection::Left, ScrollSource::ArrowKeyScroll);
    }
    else if(event->key() == Qt::Key_Right)
    {
        scrollInDirection(ScrollDirection::Right, ScrollSource::ArrowKeyScroll);
    }
    else if(event->key() == Qt::Key_Space)
    {
        scrollNext(ScrollSource::SpaceScroll);
    }
    else if(event->key() == Qt::Key_Backspace)
    {
        scrollPrev(ScrollSource::SpaceScroll);
    }
    this->doublePageModeSingleStep = false;
}

void PageViewWidget::enterEvent(QEvent*)
{
    this->setFocus(Qt::MouseFocusReason);
    mouseCurrentlyOverWidget = true;
    update();
}

void PageViewWidget::leaveEvent(QEvent*)
{
    mouseCurrentlyOverWidget = false;
    update();
}

void PageViewWidget::resizeEvent(QResizeEvent*)
{
    if(fitModeJustChanged) maintainCache(cacheKey::leftPageFitted);
    fitModeJustChanged = false;
}

void PageViewWidget::scrollInDirection(ScrollDirection direction,
                                       PageViewWidget::ScrollSource src)
{
    auto scrollPx = 0;
    currentXWasReset = false;
    if(src == ScrollSource::WheelScroll)
    {
        if(useAdaptiveWheelScroll)
        {
            scrollPx = getAdaptiveScrollPixels(direction);
        }
        else
        {
            scrollPx = wheelScrollPixels;
        }
    }
    else if(src == ScrollSource::ArrowKeyScroll)
    {
        if(!useAdaptiveArrowKeyScroll) scrollPx = arrowKeyScrollPixels;
    }
    else if(src == ScrollSource::SpaceScroll)
    {
        if(!useAdaptiveSpaceScroll) scrollPx = lastDrawnImageFullSize.height() * spaceScrollFraction;
    }
    else if(src == ScrollSource::SlideShowScroll)
    {
        if(!useAdaptiveSlideShowScroll) scrollPx = slideShowScrollPixels;
    }
    if(direction == ScrollDirection::Down && currentY <= allowedYDisplacement)
    {
        if(currentFlipSteps < 0)
            currentFlipSteps = 0;
        if(mangaMode && currentY >= allowedYDisplacement && currentX <= 0)
        {
            currentFlipSteps++;
            if(src == ScrollSource::SlideShowScroll) currentFlipSteps = stepsBeforePageFlip;
        }
        else if(!mangaMode && currentY >= allowedYDisplacement && currentX >= allowedXDisplacement)
        {
            currentFlipSteps++;
            if(src == ScrollSource::SlideShowScroll) currentFlipSteps = stepsBeforePageFlip;
        }
        else
        {
            currentFlipSteps = 0;
        }
        currentY += scrollPx;
        if(currentY > allowedYDisplacement) currentY = allowedYDisplacement;
    }
    else if(direction == ScrollDirection::Up && currentY >= 0)
    {
        if(currentFlipSteps > 0) currentFlipSteps = 0;
        if(mangaMode && currentY <= 0 && currentX >= allowedXDisplacement)
        {
            currentFlipSteps--;
        }
        else if(!mangaMode && currentY <= 0 && currentX <= 0)
        {
            currentFlipSteps--;
        }
        else
        {
            currentFlipSteps = 0;
        }
        currentY -= scrollPx;
        if(currentY < 0) currentY = 0;
    }
    else if(direction == ScrollDirection::Right && currentX <= allowedXDisplacement)
    {
        if(mangaMode && currentY <= 0 && currentX >= allowedXDisplacement)
        {
            if(currentFlipSteps > 0) currentFlipSteps = 0;
            currentFlipSteps--;
        }
        else if(!mangaMode && currentY >= allowedYDisplacement && currentX >= allowedXDisplacement)
        {
            if(currentFlipSteps < 0) currentFlipSteps = 0;
            currentFlipSteps++;
            if(src == ScrollSource::SlideShowScroll) currentFlipSteps = stepsBeforePageFlip;
        }
        else
        {
            currentFlipSteps = 0;
        }
        currentX += scrollPx;
        if(currentX > allowedXDisplacement) currentX = allowedXDisplacement;
    }
    else if(direction == ScrollDirection::Left && currentX >= 0)
    {
        if(mangaMode && currentY >= allowedYDisplacement && currentX <= 0)
        {
            if(currentFlipSteps < 0) currentFlipSteps = 0;
            currentFlipSteps++;
            if(src == ScrollSource::SlideShowScroll) currentFlipSteps = stepsBeforePageFlip;
        }
        else if(!mangaMode && currentY <= 0 && currentX <= 0)
        {
            if(currentFlipSteps > 0) currentFlipSteps = 0;
            currentFlipSteps--;
        }
        else
        {
            currentFlipSteps = 0;
        }
        currentX -= scrollPx;
        if(currentX < 0) currentX = 0;
    }
    this->ensureDisplacementWithinAllowedBounds();
    if(currentFlipSteps == stepsBeforePageFlip)
    {
        currentFlipSteps = 0;
        if(flipPagesByScrolling || src == ScrollSource::SlideShowScroll)
            nextPage(src == ScrollSource::SlideShowScroll);
    }
    else if(currentFlipSteps == -stepsBeforePageFlip)
    {
        currentFlipSteps = 0;
        if(flipPagesByScrolling) previousPage();
    }
    update();
}

void PageViewWidget::scrollNext(PageViewWidget::ScrollSource src)
{
    if(smartScroll)
    {
        if(mangaMode)
        {
            bool canScrollV = currentY < allowedYDisplacement;
            bool canScrollH = currentX > 0;
            if(smartScrollVFirst)
            {
                if(canScrollV)
                {
                    scrollInDirection(ScrollDirection::Down, src);
                }
                else
                {
                    if(canScrollH) currentY = 0;
                    scrollInDirection(ScrollDirection::Left, src);
                }
            }
            else
            {
                if(canScrollH)
                {
                    scrollInDirection(ScrollDirection::Left, src);
                }
                else
                {
                    if(canScrollV) currentX = allowedXDisplacement;
                    scrollInDirection(ScrollDirection::Down, src);
                }
            }
        }
        else
        {
            bool canScrollV = currentY < allowedYDisplacement;
            bool canScrollH = currentX < allowedXDisplacement;
            if(smartScrollVFirst)
            {
                if(canScrollV)
                {
                    scrollInDirection(ScrollDirection::Down, src);
                }
                else
                {
                    if(canScrollH) currentY = 0;
                    scrollInDirection(ScrollDirection::Right, src);
                }
            }
            else
            {
                if(canScrollH)
                {
                    scrollInDirection(ScrollDirection::Right, src);
                }
                else
                {
                    if(canScrollV) currentX = 0;
                    scrollInDirection(ScrollDirection::Down, src);
                }
            }
        }
    }
    else
    {
        scrollInDirection(ScrollDirection::Down, src);
    }
}

void PageViewWidget::scrollPrev(PageViewWidget::ScrollSource src)
{
    if(smartScroll)
    {
        if(mangaMode)
        {
            bool canScrollV = currentY > 0;
            bool canScrollH = currentX < allowedXDisplacement;
            if(smartScrollVFirst)
            {
                if(canScrollV)
                {
                    scrollInDirection(ScrollDirection::Up, src);
                }
                else
                {
                    if(canScrollH) currentY = allowedYDisplacement;
                    scrollInDirection(ScrollDirection::Right, src);
                }
            }
            else
            {
                if(canScrollH)
                {
                    scrollInDirection(ScrollDirection::Right, src);
                }
                else
                {
                    if(canScrollV) currentX = 0;
                    scrollInDirection(ScrollDirection::Up, src);
                }
            }
        }
        else
        {
            bool canScrollV = currentY > 0;
            bool canScrollH = currentX > 0;
            if(smartScrollVFirst)
            {
                if(canScrollV)
                {
                    scrollInDirection(ScrollDirection::Up, src);
                }
                else
                {
                    if(canScrollH) currentY = allowedYDisplacement;
                    scrollInDirection(ScrollDirection::Left, src);
                }
            }
            else
            {
                if(canScrollH)
                {
                    scrollInDirection(ScrollDirection::Left, src);
                }
                else
                {
                    if(canScrollV) currentX = allowedXDisplacement;
                    scrollInDirection(ScrollDirection::Up, src);
                }
            }
        }
    }
    else
    {
        scrollInDirection(ScrollDirection::Up, src);
    }
}

// check the pageNo. the page width/height.
// wont check the next page.
bool PageViewWidget::isSinglePageByPageMeta(int pn)
{
    // currPage pageNum start from 1.
    // while comicSource pageNum start from 0.

    //first page.
    if(pn == 0 && showFirstPageAsCover)
        return true;

    //last page.
    // if(pn == m_comic->getPageCount() -1)
    //     return true;

    //criteria
    auto meta = m_comic->getPageMetadata(pn);
    int w{meta.width}, h{meta.height};
    //isbanner
    if( (w>h && w/h>2) || (w<h) && (h/w>2) )
        return true;

    /*
    criteria
    - witdth: set the width val.
    - ratio, set the ratio.
    - auto, will record the first few page, record the data, calculate the ratio or width.
    */

    //auto
    auto pagesquare = [](PageMetadata x){
        return x.width * x.height;
    };
    if(m_comic->isValidPage(pn-1) && m_comic->isValidPage(pn+1)){
        auto page_former = pagesquare(m_comic->getPageMetadata(pn-1));
        auto page_cur = pagesquare(m_comic->getPageMetadata(pn));
        auto page_latter = pagesquare(m_comic->getPageMetadata(pn+1));
        if(page_cur > page_former*1.3 && page_cur > page_latter * 1.3){
            return true;
        }
    }

    // criteria is width value.
    // if(w > 1000)
    //     return true;
    // crietria is by ration.

    // if(w*1.0/h > 1.2)
    //     return true;
    return false;

}

bool PageViewWidget::currentPageIsSinglePageInDoublePageMode()
{
    if(m_doublePageMode)
    {
        bool singlePage = currPage == 1 && showFirstPageAsCover;
        if(!singlePage && m_comic && m_comic->getPageCount() && doNotShowWidePageAsDouble)
        {
            QPixmap img;
            auto meta = m_comic->getPageMetadata(currPage-1);
            if(meta.width > meta.height)
                singlePage = true;
        }
        return singlePage;
    }
    return false;
}

int PageViewWidget::getAdaptiveScrollPixels(PageViewWidget::ScrollDirection d)
{
    if(d == ScrollDirection::Up || d == ScrollDirection::Down)
    {
        return (1.0 - adaptiveScrollOverhang) * this->height();
    }
    else
    {
        return (1.0 - adaptiveScrollOverhang) * this->width();
    }
}

void PageViewWidget::ensureDisplacementWithinAllowedBounds()
{
    if(!allowFreeDrag)
    {
        if(currentX < 0) currentX = 0;
        if(currentX > allowedXDisplacement) currentX = allowedXDisplacement;
        if(currentY < 0) currentY = 0;
        if(currentY > allowedYDisplacement) currentY = allowedYDisplacement;
    }
}

void PageViewWidget::updateImageMetadata()
{
    auto metadata1 = PageMetadata{};
    auto metadata2 = PageMetadata{};
    if(m_comic && m_comic->getPageCount() > 0 && currPage > 0)
    {
        metadata1 = m_comic->getPageMetadata(currPage - 1);
        if(m_isDoublePage){
            metadata2 = m_comic->getPageMetadata(currPage);
        }
    }
    if(metadata2.valid && mangaMode)
    {
        std::swap(metadata2, metadata1);
    }
    emit this->imageMetadataUpdated(metadata1, metadata2);
    emitStatusbarUpdateSignal();
}

void PageViewWidget::setCurrentPage_Internal(int page)
{
    maintainCache(cacheKey::leftPageRaw);
    currPage = page;
    emit this->currentPageChanged(m_comic->getFilePath(), currPage, m_comic->getPageCount());
    update();

    updtWindowIcon = true;
    if(this->thumbsWidget)
        this->thumbsWidget->setCurrentPage(page);
    this->updateImageMetadata();
    emit this->pageViewConfigUINeedsToBeUpdated();
}

double PageViewWidget::calcZoomScaleFactor()
{
    return std::pow(1.1, zoomLevel);
}

void PageViewWidget::emitStatusbarUpdateSignal()
{
    auto metadata1 = PageMetadata{};
    auto metadata2 = PageMetadata{};

    QElapsedTimer x;
    x.start();
    if(m_comic && m_comic->getPageCount() > 0 && currPage > 0)
    {
        metadata1 = m_comic->getPageMetadata(currPage - 1);
        if(m_doublePageMode && currPage < m_comic->getPageCount())
        {
            if(m_isDoublePage)
            {
                metadata2 = m_comic->getPageMetadata(currPage);
                if(mangaMode)
                    std::swap(metadata1, metadata2);
            }
        }
    }

    bool swappedLeftRight = mangaMode && m_doublePageMode && m_isDoublePage;
    if(swappedLeftRight) std::swap(lastDrawnLeftHeight, lastDrawnRightHeight);

    emit this->statusbarUpdate(fitMode, metadata1, metadata2, lastDrawnLeftHeight, lastDrawnRightHeight, swappedLeftRight);
}

QPixmap PageViewWidget::getCheckeredBackground(const int width, const int height)
{
    QPixmap res(width, height);
    QPainter painter(&res);
    for(int i = 0; i < width; i += CHECKERED_IMAGE_SIZE)
    {
        for(int j = 0; j < height; j += CHECKERED_IMAGE_SIZE)
        {
            painter.drawPixmap(i * CHECKERED_IMAGE_SIZE, j * CHECKERED_IMAGE_SIZE, checkeredBkg);
        }
    }
    return res;
}

void PageViewWidget::maintainCache(PageViewWidget::cacheKey dropKey)
{
    QMutableMapIterator<cacheKey, QPixmap> it(imgCache);
    switch(dropKey)
    {
        case cacheKey::dropAll:
        case cacheKey::leftPageRaw:
        case cacheKey::rightPageRaw:
            while(it.hasNext())
            {
                it.next();
                it.value() = {};
                it.remove();
            }
            break;
        case cacheKey::leftPageTransformed:
        case cacheKey::rightPageTransformed:
            while(it.hasNext())
            {
                it.next();
                if(static_cast<int>(it.key()) >= static_cast<int>(cacheKey::leftPageTransformed))
                {
                    it.value() = {};
                    it.remove();
                }
            }
            break;
        case cacheKey::leftPageFitted:
        case cacheKey::rightPageFitted:
            while(it.hasNext())
            {
                it.next();
                if(static_cast<int>(it.key()) >= static_cast<int>(cacheKey::leftPageFitted))
                {
                    it.value() = {};
                    it.remove();
                }
            }
            break;
        case cacheKey::dropNone:
            break;
    }

    this->dynamicBackground = QColor{};
    this->lastDrawnImageFullSize = QSize{};
    this->update();
}
