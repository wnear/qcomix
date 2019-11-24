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

#ifndef PAGEVIEWWIDGET_H
#define PAGEVIEWWIDGET_H

#include <QWidget>
#include <QPixmapCache>
#include <QMouseEvent>
#include <QTimer>
#include "metadata.h"

class ComicSource;

class PageViewWidget : public QWidget
{
    Q_OBJECT
public:

    enum class ScrollSource
    {
        ArrowKeyScroll,
        WheelScroll,
        SpaceScroll,
        SlideShowScroll
    };

    enum class FitMode
    {
        FitBest,
        OriginalSize,
        FitWidth,
        FitHeight,
        ManualZoom,
        FixedSize
    };

    enum class ScrollDirection
    {
        Up,
        Down,
        Left,
        Right
    };

    static FitMode stringToFitMode(const QString& str);

    explicit PageViewWidget(QWidget *parent = nullptr);
    void initialize();
    void rotate(int degree);
    void flipHorizontally(bool flip);
    void flipVertically(bool flip);
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void goToPage(int page);
    void lastPage();
    void firstPage();
    void nextPage(bool slideShow = false);
    void toggleSlideShow(bool enabled);
    void setSlideShowSeconds(int seconds);
    void previousPage();
    void setFitMode(FitMode mode);
    bool onFirstPage() const;
    bool onLastPage() const;
    bool verticallyFlipped() const;
    bool magnifyingLensEnabled() const;
    bool horizontallyFlipped() const;
    void setKeepTransformation(bool keep);
    void setStretchSmallImages(bool stretch);
    void setCheckeredBackgroundForTransparency(bool checkered);
    void setMangaMode(bool enabled);
    void setDoublePageMode(bool doublePage);
    void setMagnifyingLensEnabled(bool enabled);
    void currentPageToClipboard();
    void setSmartScroll(bool enabled);
    void setSmartScrollVerticalFirst(bool vfirst);
    void setComicSource(ComicSource* src);
    bool getDoublePageMode() const;
    bool getMangaMode() const;
    bool getStretchSmallImages() const;
    bool smartScrollVerticalFirst() const;
    bool smartScrollEnabled() const;
    bool transformationKept() const;
    void setAllowFreeDrag(bool enabled);
    bool freeDragAllowed() const;
    int slideShowInterval() const;
    ComicSource* comicSource();
    int getCurrentPage() const;
    QString getCurrentPageFilePath();

signals:
    void windowIconUpdateNeeded(QImage);
    void archiveMetadataUpdateNeeded(ComicMetadata);
    void imageMetadataUpdateNeeded(PageMetadata, PageMetadata);
    void currentPageChanged(const QString&, int, int);
    void fitModeChanged(FitMode);
    void updateHorizontalScrollBar(int, int, int);
    void updateVerticalScrollBar(int, int, int);
    void requestLoadNextComic();
    void requestLoadPrevComic();
    void pageViewConfigUINeedsToBeUpdated();

public slots:
    void setHorizontalScrollPosition(int pos);
    void setVerticalScrollPosition(int pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void scrollInDirection(ScrollDirection direction, ScrollSource src);
    void scrollNext(ScrollSource src);
    void scrollPrev(ScrollSource src);
    bool currentPageIsSinglePageInDoublePageMode();
    int getAdaptiveScrollPixels(ScrollDirection d);
    void ensureDisplacementWithinAllowedBounds();
    void emitImageMetadataChangedSignal();
    void setCurrentPageInternal(int page);
    double calcZoomScaleFactor();
    void resetTransformation(bool force = false);
    void doFullRedraw();
    bool active = false;
    int rotationDegree = 0;
    bool horizontalFlip = false;
    bool verticalFlip = false;
    int zoomLevel = 0;
    int currentFlipSteps = 0;
    int currentX = 0;
    int currentY = 0;
    int dragStartX = 0;
    int dragStartY = 0;
    bool dragging = false;
    bool draggingXEnabled = false;
    bool draggingYEnabled = false;
    bool useAdaptiveWheelScroll = false;
    bool useAdaptiveArrowKeyScroll = false;
    bool magnifyingLensHQScaling = false;
    double adaptiveScrollOverhang = 0;
    bool useAdaptiveSlideShowScroll = false;
    bool useAdaptiveSpaceScroll = false;
    int allowedXDisplacement = 0;
    int allowedYDisplacement = 0;
    bool mangaMode = false;
    int wheelScrollPixels = 0;
    int arrowKeyScrollPixels = 0;
    int slideShowScrollPixels = 0;
    double spaceScrollFraction = 0;
    int stepsBeforePageFlip = 0;
    int slideShowSeconds = 1;
    ComicSource* comic = nullptr;
    int currentPage = 0;
    int fixedSizeWidth = 0;
    int fixedSizeHeight = 0;
    QSize lastDrawnImageFullSize;
    QPixmapCache pixmapCache;
    FitMode fitMode;
    QString mainViewBackground;
    bool stretchSmallImages = false, hqTransformMode = false, checkeredBackgroundForTransparency = false,
    doNotShowFirstPageAsDouble = false, doNotShowWidePageAsDouble = false, keepTransformationOnPageSwitch = false,
    doublePageMode = false, doublePageModeSingleStep = false;
    bool smartScroll = false;
    bool currentXWasReset = true;
    bool flipPagesByScrolling = false;
    int scrollsRequiredToFlip = 1;
    int magnifyingLensSize = 0;
    double magnificationFactor = 1.0;
    bool smartScrollVFirst = false;
    bool autoOpenNextComic = false;
    bool magnify = false;
    bool allowFreeDrag = false;
    bool slideShowAutoOpenNextComic = false;
    bool mouseCurrentlyOverWidget = false;
    QPoint mousePos;
    QImage getCheckeredBackground(const QSize& size);
    QImage checkeredBkg;
    QImage cachedPageImage;
    QSize cachedZoomBaseImageSize;
    QImage cachedZoomedImage;
    QColor dynamicBackground;
    QTimer slideShowTimer;
};

#endif // PAGEVIEWWIDGET_H
