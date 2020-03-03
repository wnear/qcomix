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

#ifndef THUMBNAILSWIDGET_H
#define THUMBNAILSWIDGET_H

#include <QWidget>
#include <QTimer>

class ComicSource;

class ThumbnailWidget : public QWidget
{
        Q_OBJECT
    public:
        explicit ThumbnailWidget(QWidget* parent = nullptr);
        void initialize();
        bool pageNumbersShown() const;
        ComicSource* comicSource();

    signals:
        void pageSwitchRequested(int);
        void updateHorizontalScrollBar(int, int, int, int);
        void updateVerticalScrollBar(int, int, int, int);
        void thumbnailerShouldBeRefocused(int);
        void pageChangeRequested(int);
        void nextPageRequested();
        void prevPageRequested();
        void automaticallyResizedSelf(int);

    public slots:
        void setHorizontalScrollPosition(int pos);
        void setVerticalScrollPosition(int pos);
        void setCurrentPage(int page);
        void setComicSource(ComicSource* src);
        void notifyPageThumbnailAvailable(const QString& srcID, int page);
        void setShowPageNumbers(bool show);
        void setThumbnailBackground(const QString& bkg);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private:
        void ensureDisplacementWithinAllowedBounds();
        void updateAllowedDisplacement();
        void ensurePageVisible(int page);
        bool showPageNums = false;
        bool automaticFixedWidth = false;
        bool active = false;
        ComicSource* comic = nullptr;
        int currentPage = 0;
        int getThumbCellWidth() const;
        int getThumbCellHeight() const;
        int currentX = 0;
        int currentY = 0;
        int pageNumFontSize = 0;
        int pageNumSize = 0;
        int thumbWidth = 0;
        int thumbHeight = 0;
        int allowedXDisplacement = 0;
        int allowedYDisplacement = 0;
        int thumbSpacing = 0;
        bool thumbnailBorder = false;
        QString thumbBkg;
        QColor dynamicBackground;
        QColor thumbBorderColor;
};

#endif // THUMBNAILSWIDGET_H
