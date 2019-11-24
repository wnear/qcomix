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

#include <QPainter>
#include "pageviewwidget.h"
#include "mainwindow.h"
#include "comicsource.h"
#include <cmath>
#include <QDebug>
#include <QApplication>
#include <QClipboard>
#include <QTextStream>

#define CHECKERED_IMAGE_SIZE 1500
#define CHECKER_SIZE 10

QTextStream out(stdout);

PageViewWidget::FitMode PageViewWidget::stringToFitMode(const QString &str)
{
    if(str == "zoom") return FitMode::ManualZoom;
    if(str == "original-size") return FitMode::OriginalSize;
    if(str == "fixed-size") return FitMode::FixedSize;
    if(str == "best") return FitMode::FitBest;
    if(str == "width") return FitMode::FitWidth;
    if(str == "height") return FitMode::FitHeight;
    out << "Invalid fit mode, falling back to best fit" << endl;
    return FitMode::FitBest;
}

PageViewWidget::PageViewWidget(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    connect(&this->slideShowTimer, &QTimer::timeout, [this](){
        scrollNext(ScrollSource::SlideShowScroll);
    });

    this->pixmapCache.setCacheLimit(256000);
}

void PageViewWidget::initialize()
{
    active = true;
    mainViewBackground = MainWindow::getOption("mainViewBackground").toString();
    doublePageMode = MainWindow::getOption("doublePageMode").toBool();
    doublePageModeSingleStep = MainWindow::getOption("doublePageModeSingleStep").toBool();
    fitMode = stringToFitMode(MainWindow::getOption("fitMode").toString());
    stretchSmallImages = MainWindow::getOption("stretchSmallImages").toBool();
    magnificationFactor = MainWindow::getOption("magnificationFactor").toDouble();
    magnifyingLensSize = MainWindow::getOption("magnifyingLensSize").toInt();
    hqTransformMode = MainWindow::getOption("hqTransformMode").toBool();
    doNotShowFirstPageAsDouble = MainWindow::getOption("doNotShowFirstPageAsDouble").toBool();
    doNotShowWidePageAsDouble = MainWindow::getOption("doNotShowWidePageAsDouble").toBool();
    checkeredBackgroundForTransparency = MainWindow::getOption("checkeredBackgroundForTransparency").toBool();
    keepTransformationOnPageSwitch = MainWindow::getOption("keepTransformationOnPageSwitch").toBool();
    mangaMode = MainWindow::getOption("mangaMode").toBool();
    slideShowSeconds = MainWindow::getOption("slideShowInterval").toInt();
    slideShowAutoOpenNextComic = MainWindow::getOption("slideShowAutoOpenNextComic").toBool();
    fixedSizeWidth = MainWindow::getOption("fixedSizeWidth").toInt();
    fixedSizeHeight = MainWindow::getOption("fixedSizeHeight").toInt();
    wheelScrollPixels = MainWindow::getOption("wheelScrollPixels").toInt();
    slideShowScrollPixels = MainWindow::getOption("slideShowScrollPixels").toInt();
    autoOpenNextComic = MainWindow::getOption("autoOpenNextComic").toBool();
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
    checkeredBkg = QImage(QSize(1500,1500), QImage::Format_ARGB32);
    checkeredBkg.fill(Qt::white);
    QPainter painter(&checkeredBkg);
    for(int i = 0; i < CHECKERED_IMAGE_SIZE/CHECKER_SIZE; i++)
    {
        for(int j = 0; j < CHECKERED_IMAGE_SIZE/CHECKER_SIZE; j++)
        {
            if(i%2 != j%2)
            {
                painter.fillRect(i*CHECKER_SIZE,j*CHECKER_SIZE, CHECKER_SIZE, CHECKER_SIZE, QColor(204,204,204));
            }
        }
    }
    painter.end();
    setAutoFillBackground(false);
    emit this->pageViewConfigUINeedsToBeUpdated();
    update();
}

void PageViewWidget::rotate(int degree)
{
    rotationDegree += degree;
    emit this->pageViewConfigUINeedsToBeUpdated();
    doFullRedraw();
}

void PageViewWidget::flipHorizontally(bool flip)
{
    horizontalFlip = flip;
    emit this->pageViewConfigUINeedsToBeUpdated();
    doFullRedraw();
}

void PageViewWidget::flipVertically(bool flip)
{
    verticalFlip = flip;
    emit this->pageViewConfigUINeedsToBeUpdated();
    doFullRedraw();
}

void PageViewWidget::zoomIn()
{
    fitMode = FitMode::ManualZoom;
    emit this->fitModeChanged(FitMode::ManualZoom);
    zoomLevel++;
    currentX *= 1.1;
    currentY *= 1.1;
    this->cachedZoomedImage = QImage{};
    emit this->pageViewConfigUINeedsToBeUpdated();
    update();
}

void PageViewWidget::zoomOut()
{
    fitMode = FitMode::ManualZoom;
    emit this->fitModeChanged(FitMode::ManualZoom);
    zoomLevel--;
    currentX *= 1.0/1.1;
    currentY *= 1.0/1.1;
    this->cachedZoomedImage = QImage{};
    emit this->pageViewConfigUINeedsToBeUpdated();
    update();
}

void PageViewWidget::resetZoom()
{
    auto zFactor = calcZoomScaleFactor();
    currentX *= zFactor;
    currentY *= zFactor;
    zoomLevel = 0;
    this->cachedZoomedImage = QImage{};
    emit this->fitModeChanged(fitMode);
    emit this->pageViewConfigUINeedsToBeUpdated();
    update();
}

void PageViewWidget::goToPage(int page)
{
    if(comic && page != currentPage && page > 0 && page < comic->getPageCount() + 1)
    {
        this->setCurrentPageInternal(page);
        resetTransformation();
        doFullRedraw();
    }
    emit this->pageViewConfigUINeedsToBeUpdated();
}

void PageViewWidget::lastPage()
{
    if(comic) this->goToPage(comic->getPageCount());
    emit this->pageViewConfigUINeedsToBeUpdated();
}

void PageViewWidget::firstPage()
{
    this->goToPage(1);
    emit this->pageViewConfigUINeedsToBeUpdated();
}

void PageViewWidget::nextPage(bool slideShow)
{
    if(comic && currentPage < comic->getPageCount())
    {       
        if(doublePageMode && !doublePageModeSingleStep && currentPage + 1 < comic->getPageCount() && !currentPageIsSinglePageInDoublePageMode())
        {
            this->setCurrentPageInternal(currentPage+2);
        } else
        {
            this->setCurrentPageInternal(currentPage+1);
        }
        resetTransformation();
        doFullRedraw();
    } else if(comic && !slideShow && autoOpenNextComic && currentPage == comic->getPageCount() && comic->hasNextComic())
    {
        emit this->requestLoadNextComic();
    } else if(comic && slideShow && slideShowAutoOpenNextComic && currentPage == comic->getPageCount() && comic->hasNextComic())
    {
        emit this->requestLoadNextComic();
    } else if(slideShow)
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
    } else
    {
        this->slideShowTimer.setInterval(1000*slideShowSeconds);
        this->slideShowTimer.start();
    }
}

void PageViewWidget::setSlideShowSeconds(int seconds)
{
    this->slideShowSeconds = seconds;
}

void PageViewWidget::previousPage()
{
    if(comic && currentPage > 1)
    {
        if(doublePageMode && !doublePageModeSingleStep && currentPage > 2 && !currentPageIsSinglePageInDoublePageMode())
        {
            this->setCurrentPageInternal(currentPage-2);
        } else
        {
            this->setCurrentPageInternal(currentPage-1);
        }
        resetTransformation();
        doFullRedraw();
    } else if(comic && autoOpenNextComic && currentPage < 2 && comic->hasPreviousComic())
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
    } else
    {
        draggingXEnabled = false;
        draggingYEnabled = false;
    }
    fitMode = mode;
    if(mode != FitMode::ManualZoom)
    {
        doFullRedraw();
    } else
    {
        update();
    }
    emit this->pageViewConfigUINeedsToBeUpdated();
}

bool PageViewWidget::onFirstPage() const
{
    return !comic || currentPage < 2;
}

bool PageViewWidget::onLastPage() const
{
    return !comic || currentPage == comic->getPageCount() || currentPage == 0;
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
    doFullRedraw();
}

void PageViewWidget::setCheckeredBackgroundForTransparency(bool checkered)
{
    checkeredBackgroundForTransparency = checkered;
    emit this->pageViewConfigUINeedsToBeUpdated();
    doFullRedraw();
}

void PageViewWidget::setMangaMode(bool enabled)
{
    this->mangaMode = enabled;
    emit this->pageViewConfigUINeedsToBeUpdated();
    this->emitImageMetadataChangedSignal();
    doFullRedraw();
}

void PageViewWidget::setDoublePageMode(bool doublePage)
{
    doublePageMode = doublePage;
    emit this->pageViewConfigUINeedsToBeUpdated();
    doFullRedraw();
}

void PageViewWidget::setMagnifyingLensEnabled(bool enabled)
{
    this->magnify = enabled;
    emit this->pageViewConfigUINeedsToBeUpdated();
    if(enabled)
    {
        this->setCursor(Qt::BlankCursor);
    } else
    {
        this->setCursor(Qt::ArrowCursor);
    }
    update();
}

void PageViewWidget::currentPageToClipboard()
{
    QApplication::clipboard()->setPixmap(QPixmap::fromImage(this->cachedPageImage));
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

void PageViewWidget::setComicSource(ComicSource *src)
{
    if(comic) delete comic;

    comic = src;

    this->setCurrentPageInternal(0);

    if(comic)
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

        emit this->archiveMetadataUpdateNeeded(comic->getComicMetadata());
    } else
    {
        emit this->archiveMetadataUpdateNeeded(ComicMetadata{});
    }

    resetTransformation();

    emit this->pageViewConfigUINeedsToBeUpdated();
}

bool PageViewWidget::getDoublePageMode() const
{
    return this->doublePageMode;
}

bool PageViewWidget::getMangaMode() const
{
    return this->mangaMode;
}

bool PageViewWidget::getStretchSmallImages() const
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

ComicSource *PageViewWidget::comicSource()
{
    return this->comic;
}

int PageViewWidget::getCurrentPage() const
{
    return currentPage;
}

QString PageViewWidget::getCurrentPageFilePath()
{
    if(this->comic && currentPage > 0)
    {
        return this->comic->getPageFilePath(currentPage-1);
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
        if(!keepTransformationOnPageSwitch || force)
        {
            this->rotationDegree = 0;
            this->horizontalFlip = false;
            this->verticalFlip = false;
            this->zoomLevel = 0;
            this->cachedZoomBaseImageSize = QSize{};
            if(fitMode == FitMode::ManualZoom)
            {
                setFitMode(stringToFitMode(MainWindow::getOption("fitMode").toString()));
                emit this->fitModeChanged(fitMode);
            }
        }
        this->currentX = 0;
        this->currentXWasReset = true;
        this->currentY = 0;
    }
    this->doFullRedraw();
}

void PageViewWidget::doFullRedraw()
{
    this->cachedPageImage = QImage{};
    this->cachedZoomedImage = QImage{};
    this->dynamicBackground = QColor{};
    this->lastDrawnImageFullSize = QSize{};
    this->update();
}

void PageViewWidget::paintEvent(QPaintEvent *event)
{
    if(active)
    {
        QPainter painter(this);

        QColor finalBkgColor;
        if(mainViewBackground == "dynamic")
        {
            painter.fillRect(painter.viewport(), this->palette().color(QPalette::Window));
            if(dynamicBackground.isValid())
            {
                painter.fillRect(painter.viewport(), dynamicBackground);
            }
            finalBkgColor = dynamicBackground;
        } else if(auto color = QColor(mainViewBackground); color.isValid())
        {
            painter.fillRect(painter.viewport(), color);
            finalBkgColor = color;
        } else
        {
            out << "Invalid main view background color" << endl;
            painter.fillRect(painter.viewport(), this->palette().color(QPalette::Window));
            finalBkgColor = this->palette().color(QPalette::Window);
        }

        if(comic && currentPage > 0)
        {
            QPixmap px_left;
            QPixmap px_right;
            QImage px_combined;
            auto width = this->width();
            auto height = this->height();
            int targetX = 0;
            int targetY = 0;
            if(cachedPageImage.isNull())
            {
            auto cacheKey = comic->getPageCacheKey(currentPage-1);
            if(pixmapCache.find(cacheKey, &px_left); px_left.isNull())
            {
                px_left = comic->getPage(currentPage-1);
                pixmapCache.insert(cacheKey, px_left);
            }

            if(doublePageMode && currentPage < comic->getPageCount() && !(currentPage == 1 && doNotShowFirstPageAsDouble) && !(px_left.width() > px_left.height() && doNotShowWidePageAsDouble))
            {
                cacheKey = comic->getPageCacheKey(currentPage);
                if(pixmapCache.find(cacheKey, &px_right); px_right.isNull())
                {
                    px_right = comic->getPage(currentPage);
                    pixmapCache.insert(cacheKey, px_right);
                }

                if(mangaMode)
                {
                    px_left.swap(px_right);
                }
            }
            QTransform transform;
            transform.rotate(rotationDegree);
            transform.scale(horizontalFlip ? -1.0 : 1.0, verticalFlip ? -1.0 : 1.0);
            px_left = px_left.transformed(transform, hqTransformMode ? Qt::SmoothTransformation : Qt::FastTransformation);
            px_right = px_right.transformed(transform, hqTransformMode ? Qt::SmoothTransformation : Qt::FastTransformation);

            if(!dynamicBackground.isValid() && mainViewBackground == "dynamic")
            {
                dynamicBackground = MainWindow::getMostCommonEdgeColor(px_left, px_right);
                painter.fillRect(painter.viewport(), dynamicBackground);
                finalBkgColor = dynamicBackground;
            }

            QSize combined_size = QSize(px_left.width() + px_right.width(), qMax(px_left.height(), px_right.height()));
            if(checkeredBackgroundForTransparency)
            {
                px_combined = getCheckeredBackground(combined_size);
            } else
            {
                px_combined = QImage(combined_size, QImage::Format_ARGB32);
                px_combined.fill(Qt::transparent);
            }
            QPainter combined_painter(&px_combined);
            combined_painter.drawPixmap(0, (px_combined.height()-px_left.height())/2, px_left);
            if(!px_right.isNull()) combined_painter.drawPixmap(px_left.width(), (px_combined.height()-px_right.height())/2, px_right);
            combined_painter.end();

            cachedPageImage = px_combined;
            emit windowIconUpdateNeeded(cachedPageImage);
            } else
            {
            px_combined = cachedPageImage;
            }

            if(fitMode == FitMode::FitHeight)
            {
                if(px_combined.height() > height || stretchSmallImages)
                {
                    px_combined = px_combined.scaledToHeight(height, hqTransformMode ? Qt::SmoothTransformation : Qt::FastTransformation);
                }
                cachedZoomBaseImageSize = px_combined.size();
            } else if(fitMode == FitMode::FitWidth)
            {
                if(px_combined.width() > width || stretchSmallImages)
                {
                    px_combined = px_combined.scaledToWidth(width, hqTransformMode ? Qt::SmoothTransformation : Qt::FastTransformation);
                }
                cachedZoomBaseImageSize = px_combined.size();
            } else if(fitMode == FitMode::FitBest)
            {
                if(!(px_combined.width() < width && px_combined.height() < height) || stretchSmallImages)
                {
                    px_combined = px_combined.scaled(width, height, Qt::KeepAspectRatio, hqTransformMode ? Qt::SmoothTransformation : Qt::FastTransformation);
                }
                cachedZoomBaseImageSize = px_combined.size();
            } else if(fitMode == FitMode::OriginalSize)
            {
                cachedZoomBaseImageSize = px_combined.size();
            } else if(fitMode == FitMode::FixedSize)
            {
                if(!(px_combined.width() < fixedSizeWidth && px_combined.height() < fixedSizeHeight) || stretchSmallImages)
                {
                    px_combined = px_combined.scaled(fixedSizeWidth, fixedSizeHeight, Qt::KeepAspectRatio, hqTransformMode ? Qt::SmoothTransformation : Qt::FastTransformation);
                }
                cachedZoomBaseImageSize = px_combined.size();
            } else if(fitMode == FitMode::ManualZoom)
            {
                auto zoomScaleFactor = calcZoomScaleFactor();

                auto zoomBaseImageSize = px_combined.size();
                if(!cachedZoomBaseImageSize.isNull()) zoomBaseImageSize = cachedZoomBaseImageSize;

                if(!cachedZoomedImage.isNull())
                {
                    px_combined = cachedZoomedImage;
                } else
                {
                    px_combined = px_combined.scaled(zoomScaleFactor*zoomBaseImageSize.width(), zoomScaleFactor*zoomBaseImageSize.height(), Qt::KeepAspectRatio, hqTransformMode ? Qt::SmoothTransformation : Qt::FastTransformation);
                    cachedZoomedImage = px_combined;
                }
            }

            if(int w_diff = width - px_combined.width(); w_diff > 0)
            {
                targetX = w_diff/2;
                draggingXEnabled = false;
                allowedXDisplacement = 0;
            } else
            {
                draggingXEnabled = true;
                allowedXDisplacement = -w_diff;
            }

            if(int h_diff = height - px_combined.height(); h_diff > 0)
            {
                targetY = h_diff/2;
                draggingYEnabled = false;
                allowedYDisplacement = 0;
            } else
            {
                draggingYEnabled = true;
                allowedYDisplacement = -h_diff;
            }

            ensureDisplacementWithinAllowedBounds();

            if(mangaMode && currentXWasReset && currentX == 0)
            {
                currentX = allowedXDisplacement;
                currentXWasReset = false;
            } else if(currentXWasReset)
            {
                currentXWasReset = false;
            }

            emit this->updateHorizontalScrollBar(allowedXDisplacement, currentX, std::min(width, px_combined.width()));
            emit this->updateVerticalScrollBar(allowedYDisplacement, currentY, std::min(height, px_combined.height()));

            painter.drawImage(targetX-currentX, targetY-currentY, px_combined);

            lastDrawnImageFullSize = px_combined.size();

            if(magnify && mouseCurrentlyOverWidget)
            {
                auto sourceLength = magnificationFactor*magnifyingLensSize;
                QImage sourceImg = px_combined.copy(mousePos.x()-targetX+currentX-sourceLength/2.0, mousePos.y()-targetY+currentY-sourceLength/2.0, sourceLength, sourceLength).scaled(magnifyingLensSize, magnifyingLensSize, Qt::IgnoreAspectRatio, magnifyingLensHQScaling ? Qt::SmoothTransformation : Qt::FastTransformation);
                painter.fillRect(mousePos.x()-magnifyingLensSize/2.0, mousePos.y()-magnifyingLensSize/2.0, magnifyingLensSize, magnifyingLensSize, finalBkgColor);
                painter.drawImage(mousePos.x()-magnifyingLensSize/2.0, mousePos.y()-magnifyingLensSize/2.0, sourceImg);
                painter.setPen(Qt::black);
                painter.drawRect(mousePos.x()-magnifyingLensSize/2.0, mousePos.y()-magnifyingLensSize/2.0, magnifyingLensSize, magnifyingLensSize);
            }
        }
    } else
    {
        QWidget::paintEvent(event);
    }
}

void PageViewWidget::mousePressEvent(QMouseEvent *event)
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

void PageViewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    dragging = false;
    if(!magnify)
    {
        this->setCursor(Qt::ArrowCursor);
    }
}

void PageViewWidget::mouseMoveEvent(QMouseEvent *event)
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

void PageViewWidget::wheelEvent(QWheelEvent *event)
{
    if(event->modifiers() & Qt::ControlModifier)
    {
        if(event->angleDelta().y() > 0)
        {
            zoomIn();
        } else if(event->angleDelta().y() < 0)
        {
            zoomOut();
        }
    } else
    {
        if(event->angleDelta().y() > 0)
        {
            scrollPrev(ScrollSource::WheelScroll);
        } else if(event->angleDelta().y() < 0)
        {
            scrollNext(ScrollSource::WheelScroll);
        }
        if(event->angleDelta().x() > 0)
        {
            scrollInDirection(ScrollDirection::Left, ScrollSource::WheelScroll);
        } else if(event->angleDelta().x() < 0)
        {
            scrollInDirection(ScrollDirection::Right, ScrollSource::WheelScroll);
        }
    }
}

void PageViewWidget::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Up)
    {
        scrollPrev(ScrollSource::ArrowKeyScroll);
    } else if(event->key() == Qt::Key_Down)
    {
        scrollNext(ScrollSource::ArrowKeyScroll);
    } else if(event->key() == Qt::Key_Left)
    {
        scrollInDirection(ScrollDirection::Left, ScrollSource::ArrowKeyScroll);
    } else if(event->key() == Qt::Key_Right)
    {
        scrollInDirection(ScrollDirection::Right, ScrollSource::ArrowKeyScroll);
    } else if(event->key() == Qt::Key_Space)
    {
        scrollNext(ScrollSource::SpaceScroll);
    }
}

void PageViewWidget::enterEvent(QEvent *event)
{
    mouseCurrentlyOverWidget = true;
    update();
}

void PageViewWidget::leaveEvent(QEvent *event)
{
    mouseCurrentlyOverWidget = false;
    update();
}

void PageViewWidget::scrollInDirection(ScrollDirection direction, PageViewWidget::ScrollSource src)
{
    auto scrollPx = 0;
    currentXWasReset = false;
    if(src == ScrollSource::WheelScroll)
    {
        if(useAdaptiveWheelScroll)
        {
            scrollPx = getAdaptiveScrollPixels(direction);
        } else
        {
            scrollPx = wheelScrollPixels;
        }
    } else if(src == ScrollSource::ArrowKeyScroll)
    {
        if(useAdaptiveArrowKeyScroll)
        {

        } else
        {
            scrollPx = arrowKeyScrollPixels;
        }
    } else if(src == ScrollSource::SpaceScroll)
    {
        if(useAdaptiveSpaceScroll)
        {
        } else
        {
            scrollPx = lastDrawnImageFullSize.height()*spaceScrollFraction;
        }
    } else if(src == ScrollSource::SlideShowScroll)
    {
        if(useAdaptiveSlideShowScroll)
        {

        } else
        {
            scrollPx = slideShowScrollPixels;
        }
    }
    if(direction == ScrollDirection::Down && currentY <= allowedYDisplacement)
    {
        if(currentFlipSteps < 0) currentFlipSteps = 0;
        if(mangaMode && currentY >= allowedYDisplacement && currentX <= 0)
        {
            currentFlipSteps++;
            if(src == ScrollSource::SlideShowScroll) currentFlipSteps = stepsBeforePageFlip;
        } else if(!mangaMode && currentY >= allowedYDisplacement && currentX >= allowedXDisplacement)
        {
            currentFlipSteps++;
            if(src == ScrollSource::SlideShowScroll) currentFlipSteps = stepsBeforePageFlip;
        } else
        {
            currentFlipSteps = 0;
        }
        currentY += scrollPx;
        if(currentY > allowedYDisplacement) currentY = allowedYDisplacement;
    } else if(direction == ScrollDirection::Up && currentY >= 0)
    {
        if(currentFlipSteps > 0) currentFlipSteps = 0;
        if(mangaMode && currentY <= 0 && currentX >= allowedXDisplacement)
        {
            currentFlipSteps--;
        } else if(!mangaMode && currentY <= 0 && currentX <= 0)
        {
            currentFlipSteps--;
        } else
        {
            currentFlipSteps = 0;
        }
        currentY -= scrollPx;
        if(currentY < 0) currentY = 0;
    } else if(direction == ScrollDirection::Right && currentX <= allowedXDisplacement)
    {
        if(mangaMode && currentY <= 0 && currentX >= allowedXDisplacement)
        {
            if(currentFlipSteps > 0) currentFlipSteps = 0;
            currentFlipSteps--;
        } else if(!mangaMode && currentY >= allowedYDisplacement && currentX >= allowedXDisplacement)
        {
            if(currentFlipSteps < 0) currentFlipSteps = 0;
            currentFlipSteps++;
            if(src == ScrollSource::SlideShowScroll) currentFlipSteps = stepsBeforePageFlip;
        } else
        {
            currentFlipSteps = 0;
        }
        currentX += scrollPx;
        if(currentX > allowedXDisplacement) currentX = allowedXDisplacement;
    } else if(direction == ScrollDirection::Left && currentX >= 0)
    {
        if(mangaMode && currentY >= allowedYDisplacement && currentX <= 0)
        {
            if(currentFlipSteps < 0) currentFlipSteps = 0;
            currentFlipSteps++;
            if(src == ScrollSource::SlideShowScroll) currentFlipSteps = stepsBeforePageFlip;
        } else if(!mangaMode && currentY <= 0 && currentX <= 0)
        {
            if(currentFlipSteps > 0) currentFlipSteps = 0;
            currentFlipSteps--;
        } else
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
    } else if(currentFlipSteps == -stepsBeforePageFlip)
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
                } else
                {
                    if(canScrollH) currentY = 0;
                    scrollInDirection(ScrollDirection::Left, src);
                }
            } else
            {
                if(canScrollH)
                {
                    scrollInDirection(ScrollDirection::Left, src);
                } else
                {
                    if(canScrollV) currentX = allowedXDisplacement;
                    scrollInDirection(ScrollDirection::Down, src);
                }
            }
        } else
        {
            bool canScrollV = currentY < allowedYDisplacement;
            bool canScrollH = currentX < allowedXDisplacement;
            if(smartScrollVFirst)
            {
                if(canScrollV)
                {
                    scrollInDirection(ScrollDirection::Down, src);
                } else
                {
                    if(canScrollH) currentY = 0;
                    scrollInDirection(ScrollDirection::Right, src);
                }
            } else
            {
                if(canScrollH)
                {
                    scrollInDirection(ScrollDirection::Right, src);
                } else
                {
                    if(canScrollV) currentX = 0;
                    scrollInDirection(ScrollDirection::Down, src);
                }
            }
        }
    } else
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
                } else
                {
                    if(canScrollH) currentY = allowedYDisplacement;
                    scrollInDirection(ScrollDirection::Right, src);
                }
            } else
            {
                if(canScrollH)
                {
                    scrollInDirection(ScrollDirection::Right, src);
                } else
                {
                    if(canScrollV) currentX = 0;
                    scrollInDirection(ScrollDirection::Up, src);
                }
            }
        } else
        {
            bool canScrollV = currentY > 0;
            bool canScrollH = currentX > 0;
            if(smartScrollVFirst)
            {
                if(canScrollV)
                {
                    scrollInDirection(ScrollDirection::Up, src);
                } else
                {
                    if(canScrollH) currentY = allowedYDisplacement;
                    scrollInDirection(ScrollDirection::Left, src);
                }
            } else
            {
                if(canScrollH)
                {
                    scrollInDirection(ScrollDirection::Left, src);
                } else
                {
                    if(canScrollV) currentX = allowedXDisplacement;
                    scrollInDirection(ScrollDirection::Up, src);
                }
            }
        }
    } else
    {
        scrollInDirection(ScrollDirection::Up, src);
    }
}

bool PageViewWidget::currentPageIsSinglePageInDoublePageMode()
{
    if(doublePageMode)
    {
    bool singlePage = currentPage == 1 && doNotShowFirstPageAsDouble;
    if(!singlePage && doNotShowWidePageAsDouble)
            {
                QPixmap px;
                auto cacheKey = comic->getPageCacheKey(currentPage-1);
                if(pixmapCache.find(cacheKey, &px); px.isNull())
                {
                    px = comic->getPage(currentPage-1);
                    pixmapCache.insert(cacheKey, px);
                }
                singlePage = px.width() > px.height();
            }
    return singlePage;
    }
    return false;
}

int PageViewWidget::getAdaptiveScrollPixels(PageViewWidget::ScrollDirection d)
{
    if(d == ScrollDirection::Up || d == ScrollDirection::Down)
    {
        return (1.0 - adaptiveScrollOverhang ) * this->height();
    } else
    {
        return (1.0 - adaptiveScrollOverhang ) * this->width();
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
    if(comic && comic->getPageCount() > 0 && currentPage > 0)
    {
        metadata1 = comic->getPageMetadata(currentPage-1);
    if(doublePageMode && currentPage < comic->getPageCount())
    {
        if(!currentPageIsSinglePageInDoublePageMode()) metadata2 = comic->getPageMetadata(currentPage);
    }
    }

    if(metadata2.valid && mangaMode)
    {
        emit this->imageMetadataUpdateNeeded(metadata2, metadata1);
    } else
    {
        emit this->imageMetadataUpdateNeeded(metadata1, metadata2);
    }
}

void PageViewWidget::setCurrentPageInternal(int page)
{
    currentPage = page;
    if(comic)
    {
        emit this->currentPageChanged(comic->getFilePath(), currentPage, comic->getPageCount());
    } else
    {
        emit this->currentPageChanged({}, 0, 0);
    }
    this->emitImageMetadataChangedSignal();
    emit this->pageViewConfigUINeedsToBeUpdated();
}

double PageViewWidget::calcZoomScaleFactor()
{
    return std::pow(1.1, zoomLevel);
}

QImage PageViewWidget::getCheckeredBackground(const QSize &size)
{
    QImage res(size, QImage::Format_ARGB32);
    QPainter painter(&res);
    for(int i = 0; i < size.width(); i += CHECKERED_IMAGE_SIZE)
    {
        for(int j = 0; j < size.height(); j += CHECKERED_IMAGE_SIZE)
        {
            painter.drawImage(i*CHECKERED_IMAGE_SIZE,j*CHECKERED_IMAGE_SIZE,checkeredBkg);
        }
    }
    return res;
}

