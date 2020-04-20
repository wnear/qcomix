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

#include "metadata.h"
#include <QMouseEvent>
#include <QTimer>
#include <QWidget>

class ComicSource;
class ThumbnailWidget;

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

    explicit PageViewWidget(QWidget* parent = nullptr);
    void initialize(ThumbnailWidget* w);
    bool onFirstPage() const;
    bool onLastPage() const;
    bool verticallyFlipped() const;
    bool magnifyingLensEnabled() const;
    bool horizontallyFlipped() const;
    bool isDoublePageMode() const;
    bool isMangaMode() const;
    bool stretchesSmallImages() const;
    bool smartScrollVerticalFirst() const;
    bool smartScrollEnabled() const;
    bool transformationKept() const;
    bool freeDragAllowed() const;
    int slideShowInterval() const;
    ComicSource* comicSource();
    int currentPage() const;
    QString currentPageFilePath();

signals:
    void windowIconUpdateNeeded(QPixmap);
    void archiveMetadataUpdateNeeded(ComicMetadata);
    void imageMetadataUpdateNeeded(PageMetadata, PageMetadata);
    void currentPageChanged(const QString&, int, int);
    void fitModeChanged(PageViewWidget::FitMode);
    void updateHorizontalScrollBar(int, int, int);
    void updateVerticalScrollBar(int, int, int);
    void requestLoadNextComic();
    void requestLoadPrevComic();
    void pageViewConfigUINeedsToBeUpdated();
    void statusbarUpdate(PageViewWidget::FitMode, const PageMetadata&, const PageMetadata&, int, int, bool);

public slots:
    void setHorizontalScrollPosition(int pos);
    void setVerticalScrollPosition(int pos);
    void goToPage(int page);
    void nextPage(bool slideShow = false);
    void previousPage();
    void rotate(int degree);
    void flipHorizontally(bool flip);
    void flipVertically(bool flip);
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void lastPage();
    void firstPage();
    void toggleSlideShow(bool enabled);
    void setSlideShowSeconds(int seconds);
    void setFitMode(PageViewWidget::FitMode mode);
    void setKeepTransformation(bool keep);
    void setStretchSmallImages(bool stretch);
    void setCheckeredBackgroundForTransparency(bool checkered);
    void setMangaMode(bool enabled);
    void setDoublePageMode(bool doublePage);
    void setMagnifyingLensEnabled(bool enabled);
    void currentPageToClipboard();
    void setSmartScroll(bool enabled);
    void setSmartScrollVerticalFirst(bool vfirst);
    void setAllowFreeDrag(bool enabled);
    ComicSource* setComicSource(ComicSource* src);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void enterEvent(QEvent*) override;
    void leaveEvent(QEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    void scrollInDirection(ScrollDirection direction, ScrollSource src);
    void scrollNext(ScrollSource src);
    void scrollPrev(ScrollSource src);
    bool currentPageIsSinglePageInDoublePageMode();
    int getAdaptiveScrollPixels(ScrollDirection d);
    void fitLeftRightImageToSize(int width, int height, int combined_width, int combined_height, double& leftScaledWidth, double& rightScaledWidth, double& leftScaledHeight, double& rightScaledHeight);
    void ensureDisplacementWithinAllowedBounds();
    void emitImageMetadataChangedSignal();
    void setCurrentPageInternal(int page);
    double calcZoomScaleFactor();
    void emitStatusbarUpdateSignal();
    void resetTransformation(bool force = false);
    static QPixmap getRegionFromCombinedPixmap(const QPixmap& left, const QPixmap& right, int x, int y, int w, int h, const QColor& bkgColor);
    void doFullRedraw();
    bool active = false;
    bool updtWindowIcon = false;
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
    int transparentBackgroundCheckerSize = 10;
    ComicSource* comic = nullptr;
    int currPage = 0;
    int fixedSizeWidth = 0;
    int fixedSizeHeight = 0;
    QSize lastDrawnImageFullSize;
    FitMode fitMode = FitMode::FitBest;
    QString mainViewBackground;
    bool stretchSmallImages = false;
    bool hqTransformMode = false;
    bool checkeredBackgroundForTransparency = false;
    bool doNotShowFirstPageAsDouble = false;
    bool doNotShowWidePageAsDouble = false;
    bool keepTransformationOnPageSwitch = false;
    bool doublePageMode = false;
    bool doublePageModeSingleStep = false;
    bool smartScroll = false;
    bool currentXWasReset = true;
    bool flipPagesByScrolling = false;
    int scrollsRequiredToFlip = 1;
    int magnifyingLensSize = 0;
    int lastDrawnLeftHeight = 0;
    int lastDrawnRightHeight = 0;
    double magnificationFactor = 1.0;
    bool smartScrollVFirst = false;
    bool autoOpenNextComic = false;
    bool magnify = false;
    bool allowFreeDrag = false;
    bool slideShowAutoOpenNextComic = false;
    bool mouseCurrentlyOverWidget = false;
    QPoint mousePos;
    QPixmap getCheckeredBackground(const int width, const int height);
    QPixmap checkeredBkg;
    enum class cacheKey //in order, invalidating an entry should invalidate all following entries too
    {
        dropAll = 0,
        leftPageRaw = 1,
        rightPageRaw = 2,
        leftPageTransformed = 3,
        rightPageTransformed = 4,
        leftPageFitted = 5,
        rightPageFitted = 6,
        dropNone = 7
    };
    QMap<cacheKey, QPixmap> imgCache;
    bool renderCombinedImage = false;
    QPixmap cachedCombinedImage;
    void maintainCache(cacheKey dropKey);
    bool fitModeJustChanged = false;
    QSize cachedZoomBaseLeftImageSize;
    QSize cachedZoomBaseRightImageSize;
    QColor dynamicBackground;
    QTimer slideShowTimer;
    ThumbnailWidget* thumbsWidget = nullptr;
};

#endif // PAGEVIEWWIDGET_H
