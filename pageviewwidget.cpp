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
    doublePageMode = MainWindow::getOption("doublePageMode").toBool();
    doublePageModeSingleStep = MainWindow::getOption("doublePageModeSingleStep").toBool();
    fitMode = stringToFitMode(MainWindow::getOption("fitMode").toString());
    stretchSmallImages = MainWindow::getOption("stretchSmallImages").toBool();
    stretchSmallImages = true;
    magnificationFactor = MainWindow::getOption("magnificationFactor").toDouble();
    magnifyingLensSize = MainWindow::getOption("magnifyingLensSize").toInt();
    hqTransformMode = MainWindow::getOption("hqTransformMode").toBool();
    doNotShowFirstPageAsDouble = MainWindow::getOption("doNotShowFirstPageAsDouble").toBool();
    doNotShowWidePageAsDouble = MainWindow::getOption("doNotShowWidePageAsDouble").toBool();
    checkeredBackgroundForTransparency = MainWindow::getOption("checkeredBackgroundForTransparency").toBool();
    keepTransformationOnPageSwitch = MainWindow::getOption("keepTransformationOnPageSwitch").toBool();
    keepTransformationOnPageSwitch = true;
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
    update();
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
    update();
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
    update();
}

void PageViewWidget::goToPage(int page)
{
    if(comic && page != currPage && page > 0 && page < comic->getPageCount() + 1)
    {
        this->setCurrentPageInternal(page);
        resetTransformation();
    }
    emit this->pageViewConfigUINeedsToBeUpdated();
}

void PageViewWidget::lastPage()
{
    if(comic)
        this->goToPage(comic->getPageCount());
    emit this->pageViewConfigUINeedsToBeUpdated();
}

void PageViewWidget::firstPage()
{
    this->goToPage(1);
    emit this->pageViewConfigUINeedsToBeUpdated();
}

void PageViewWidget::nextPage(bool slideShow)
{
    if(comic && currPage < comic->getPageCount())
    {
        if(doublePageMode && !doublePageModeSingleStep && currPage + 1 < comic->getPageCount() && !currentPageIsSinglePageInDoublePageMode())
        {
            
            this->setCurrentPageInternal(currPage + 2);
        }
        else
        {
            this->setCurrentPageInternal(currPage + 1);
        }
        resetTransformation();
        updtWindowIcon = true;
    }
    else if(comic && !slideShow && autoOpenNextComic && currPage == comic->getPageCount() && comic->hasNextComic())
    {
        emit this->requestLoadNextComic();
    }
    else if(comic && slideShow && slideShowAutoOpenNextComic && currPage == comic->getPageCount() && comic->hasNextComic())
    {
        emit this->requestLoadNextComic();
    }
    else if(slideShow)
    {
        this->toggleSlideShow(false);
    }
    emit this->pageViewConfigUINeedsToBeUpdated();
}

void PageViewWidget::toggleSlideShow(bool enabled)
{
    if(!enabled)
    {
        this->slideShowTimer.stop();
    }
    else
    {
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
    if(comic && currPage > 1)
    {
        if(doublePageMode && !doublePageModeSingleStep && currPage > 2 && !currentPageIsSinglePageInDoublePageMode())
        {
            this->setCurrentPageInternal(currPage - 2);
        }
        else
        {
            this->setCurrentPageInternal(currPage - 1);
        }
        resetTransformation();
    }
    else if(comic && autoOpenNextComic && currPage < 2 && comic->hasPreviousComic())
    {
        emit this->requestLoadPrevComic();
    }
    emit this->pageViewConfigUINeedsToBeUpdated();
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
    return !comic || currPage < 2;
}

bool PageViewWidget::onLastPage() const
{
    return !comic || currPage == comic->getPageCount() || currPage == 0;
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
    this->emitImageMetadataChangedSignal();
    maintainCache(cacheKey::leftPageRaw);
}

void PageViewWidget::setDoublePageMode(bool doublePage)
{
    doublePageMode = doublePage;
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
    auto oldComic = comic;

    comic = src;

    maintainCache(cacheKey::dropAll);

    if(this->thumbsWidget) this->thumbsWidget->setComicSource(src);

    this->setCurrentPageInternal(0);

    if(comic)
    {
        auto startAtPage = comic->startAtPage();
        if(startAtPage == -1)
        {
            if(MainWindow::getOption("rememberPage").toBool())
            {
                auto savedPos = MainWindow::getSavedPositionForFilePath(comic->getFilePath());
                if(savedPos <= comic->getPageCount() && savedPos > 0)
                {
                    this->setCurrentPageInternal(savedPos);
                }
            }
            else if(comic->getPageCount() > 0)
            {
                this->setCurrentPageInternal(1);
            }
        } else
        {
            this->setCurrentPageInternal(startAtPage);
        }

        emit this->archiveMetadataUpdateNeeded(comic->getComicMetadata());
    }
    else
    {
        emit this->archiveMetadataUpdateNeeded(ComicMetadata{});
    }

    resetTransformation();

    emit this->pageViewConfigUINeedsToBeUpdated();

    return oldComic;
}

bool PageViewWidget::isDoublePageMode() const
{
    return this->doublePageMode;
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
    return this->comic;
}

int PageViewWidget::currentPage() const
{
    return currPage;
}

QString PageViewWidget::currentPageFilePath()
{
    if(this->comic && currPage > 0)
    {
        return this->comic->getPageFilePath(currPage - 1);
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

void PageViewWidget::paintEvent(QPaintEvent* event)
{
    if(active)
    {
        QPainter painter(this);

        // <<-- 1. calculate background color.
        QColor finalBkgColor;
        if(mainViewBackground == "dynamic")
        {
            painter.fillRect(painter.viewport(), this->palette().color(QPalette::Window));
            if(dynamicBackground.isValid())
            {
                painter.fillRect(painter.viewport(), dynamicBackground);
            }
            finalBkgColor = dynamicBackground;
        }
        else if(auto color = QColor(mainViewBackground); color.isValid())
        {
            painter.fillRect(painter.viewport(), color);
            finalBkgColor = color;
        }
        else
        {
            painter.fillRect(painter.viewport(), this->palette().color(QPalette::Window));
            finalBkgColor = this->palette().color(QPalette::Window);
        }
        // >>-- 1. calculate background color. ....end...

        if(comic && currPage > 0)
        {
            int width = this->width();
            int height = this->height();
            int targetX = 0;
            int targetY = 0;

            // -- 2. get imgCache[pageRaw] data.
            if(imgCache[cacheKey::leftPageRaw].isNull())
            { imgCache[cacheKey::leftPageRaw] = comic->getPagePixmap(currPage - 1); }

            bool doublePage = doublePageMode && currPage < comic->getPageCount() && !(currPage == 1 && doNotShowFirstPageAsDouble) && !(imgCache[cacheKey::leftPageRaw].width() > imgCache[cacheKey::leftPageRaw].height() && doNotShowWidePageAsDouble);
            if(doublePage)
            {
                if(imgCache[cacheKey::rightPageRaw].isNull())
                {
                    imgCache[cacheKey::rightPageRaw] = comic->getPagePixmap(currPage);

                    if(mangaMode)
                    {
                        imgCache[cacheKey::rightPageRaw].swap(imgCache[cacheKey::leftPageRaw]);
                    }
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
                
                combined_painter.drawPixmap(0, (img_combined.height() - imgCache[cacheKey::leftPageTransformed].height()) / 2.0, imgCache[cacheKey::leftPageTransformed]);
                if(!imgCache[cacheKey::rightPageTransformed].isNull()){
                    //combined_painter.
                    combined_painter.setPen(Qt::black);
                    combined_painter.drawLine(imgCache[cacheKey::leftPageTransformed].width(), (img_combined.height() - imgCache[cacheKey::rightPageTransformed].height()) / 2.0,imgCache[cacheKey::leftPageTransformed].width(),this->height());
                    combined_painter.drawPixmap(imgCache[cacheKey::leftPageTransformed].width(), (img_combined.height() - imgCache[cacheKey::rightPageTransformed].height()) / 2.0, imgCache[cacheKey::rightPageTransformed]);
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
                        imgCache[cacheKey::leftPageFitted] = imgCache[cacheKey::leftPageTransformed].scaledToWidth(leftScaledWidth, scaleMode);
                        if(doublePage) imgCache[cacheKey::rightPageFitted] = imgCache[cacheKey::rightPageTransformed].scaledToWidth(rightScaledWidth, scaleMode);
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

                        fitLeftRightImageToSize(width, height, combined_width, combined_height, leftScaledWidth, rightScaledWidth, leftScaledHeight, rightScaledHeight);

                        imgCache[cacheKey::leftPageFitted] = imgCache[cacheKey::leftPageTransformed].scaled(leftScaledWidth, leftScaledHeight, Qt::IgnoreAspectRatio, scaleMode);
                        if(doublePage) imgCache[cacheKey::rightPageFitted] = imgCache[cacheKey::rightPageTransformed].scaled(rightScaledWidth, rightScaledHeight, Qt::IgnoreAspectRatio, scaleMode);
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
                    auto zoomBaseLeftImageSize = cachedZoomBaseLeftImageSize.isNull() ? imgCache[cacheKey::leftPageTransformed].size() : cachedZoomBaseLeftImageSize;
                    auto zoomBaseRightImageSize = cachedZoomBaseRightImageSize.isNull() ? imgCache[cacheKey::rightPageTransformed].size() : cachedZoomBaseRightImageSize;

                    imgCache[cacheKey::leftPageFitted] = imgCache[cacheKey::leftPageTransformed].scaled(zoomScaleFactor * zoomBaseLeftImageSize.width(),
                                                                                                        zoomScaleFactor * zoomBaseLeftImageSize.height(), Qt::KeepAspectRatio,
                                                                                                        hqTransformMode ? Qt::SmoothTransformation : Qt::FastTransformation);

                    if(doublePage)
                        imgCache[cacheKey::rightPageFitted] = imgCache[cacheKey::rightPageTransformed].scaled(zoomScaleFactor * zoomBaseRightImageSize.width(),
                                                                                                              zoomScaleFactor * zoomBaseRightImageSize.height(), Qt::KeepAspectRatio,
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
    }
    else
    {
        QWidget::paintEvent(event);
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
    bool isSingleStep =!! (event->modifiers()&Qt::ShiftModifier);
    if(isSingleStep) { this->doublePageModeSingleStep = true; }
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
        if(flipPagesByScrolling || src == ScrollSource::SlideShowScroll) nextPage(src == ScrollSource::SlideShowScroll);
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

bool PageViewWidget::isSinglePage(int pagenumber)
{
    auto isWidPage = [this](int pn){
        pn = std::max(0, pn -1);
        QPixmap img = comic->getPagePixmap(pn);
        if(img.width() > img.height()) return true;
        auto getWidth = [this](int n){
            return (n <0 || n >= comic->getPageCount())?
                0: comic->getPagePixmap(n).width();
        };
        auto width =std::max(getWidth(pn-1), getWidth(pn+1));
        return img.height() > width * 1.75? true: false;
    };
    if(doublePageMode){
        // first page.
        if(pagenumber == 1 && doNotShowFirstPageAsDouble)
            return true;
        // last page.
        if(pagenumber == comic->getPageCount() -1)
            return true;
        if(doNotShowWidePageAsDouble)
            return false;
    }
    return false;
}

bool PageViewWidget::currentPageIsSinglePageInDoublePageMode()
{
    if(doublePageMode)
    {
        bool singlePage = currPage == 1 && doNotShowFirstPageAsDouble;
        if(!singlePage && comic && comic->getPageCount() && doNotShowWidePageAsDouble)
        {
            QPixmap img;
            if(img = imgCache.value(cacheKey::leftPageRaw); img.isNull())
            {
                img = comic->getPagePixmap(std::max(0, currPage-1));
                //img = currPage == 0 ? comic->getPagePixmap(0) : comic->getPagePixmap(currPage - 1);
            }
            singlePage = img.width() > img.height();
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

void PageViewWidget::emitImageMetadataChangedSignal()
{
    auto metadata1 = PageMetadata{};
    auto metadata2 = PageMetadata{};
    if(comic && comic->getPageCount() > 0 && currPage > 0)
    {
        metadata1 = comic->getPageMetadata(currPage - 1);
        if(doublePageMode && currPage < comic->getPageCount())
        {
            if(!currentPageIsSinglePageInDoublePageMode())
                metadata2 = comic->getPageMetadata(currPage);
        }
    }
    if(metadata2.valid && mangaMode)
    {
        emit this->imageMetadataUpdateNeeded(metadata2, metadata1);
    }
    else
    {
        emit this->imageMetadataUpdateNeeded(metadata1, metadata2);
    }
    emitStatusbarUpdateSignal();
}

void PageViewWidget::setCurrentPageInternal(int page)
{
    if(currPage != page)
    {
        maintainCache(cacheKey::leftPageRaw);
    }
    currPage = page;
    if(comic)
    {
        emit this->currentPageChanged(comic->getFilePath(), currPage, comic->getPageCount());
    }
    else
    {
        emit this->currentPageChanged({}, 0, 0);
    }
    updtWindowIcon = true;
    if(this->thumbsWidget) this->thumbsWidget->setCurrentPage(page);
    this->emitImageMetadataChangedSignal();
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
    if(comic && comic->getPageCount() > 0 && currPage > 0)
    {
        metadata1 = comic->getPageMetadata(currPage - 1);
        if(doublePageMode && currPage < comic->getPageCount())
        {
            if(!currentPageIsSinglePageInDoublePageMode())
            {
                metadata2 = comic->getPageMetadata(currPage);
                if(mangaMode) std::swap(metadata1, metadata2);
            }
        }
    }

    bool swappedLeftRight = mangaMode && doublePageMode && !currentPageIsSinglePageInDoublePageMode();
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
