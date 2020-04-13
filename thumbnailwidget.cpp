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
#include <cmath>
#include "thumbnailwidget.h"
#include "mainwindow.h"
#include "imagecache.h"
#include "comicsource.h"

// https://stackoverflow.com/questions/1855884/determine-font-color-based-on-background-color
QColor contrastColor(const QColor& color)
{
    int d = 0;

    // Counting the perceptive luminance - human eye favors green color...
    double luminance = (0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue()) / 255;

    if(luminance > 0.5)
    {
        d = 12; // bright colors - black font
    }
    else
    {
        d = 243; // dark colors - white font
    }

    return QColor(d, d, d);
}

ThumbnailWidget::ThumbnailWidget(QWidget* parent) :
    QWidget(parent)
{
    setMouseTracking(true);
}

void ThumbnailWidget::initialize()
{
    active = true;
    thumbBkg = MainWindow::getOption("thumbBackground").toString();
    showPageNums = MainWindow::getOption("showPageNumbersOnThumbnails").toBool();
    thumbWidth = MainWindow::getOption("thumbnailWidth").toInt();
    thumbHeight = MainWindow::getOption("thumbnailHeight").toInt();
    thumbSpacing = MainWindow::getOption("thumbnailSpacing").toInt();
    thumbnailBorder = MainWindow::getOption("thumbnailBorder").toBool();
    pageNumFontSize = MainWindow::getOption("thumbPageNumFontSize").toInt();
    automaticFixedWidth = MainWindow::getOption("thumbnailSidebarAutomaticWidth").toBool();
    thumbBorderColor = QColor{MainWindow::getOption("thumbnailBorderColor").toString()};
    QFont f = this->font();
    f.setPointSize(pageNumFontSize);
    this->setFont(f);
    pageNumSize = QFontMetrics(font()).boundingRect("999").width() + 4;
    if(automaticFixedWidth) this->setMinimumWidth(this->getThumbCellWidth());
}

void ThumbnailWidget::setShowPageNumbers(bool show)
{
    showPageNums = show;
    updateAllowedDisplacement();
    update();
}

void ThumbnailWidget::setThumbnailBackground(const QString& bkg)
{
    this->thumbBkg = bkg;
    update();
}

bool ThumbnailWidget::pageNumbersShown() const
{
    return this->showPageNums;
}

void ThumbnailWidget::setComicSource(ComicSource* src)
{
    this->comic = src;
    if(src)
    {
        pageNumSize = QFontMetrics(font()).boundingRect(QString::number(src->getPageCount())).width() + 4;
    }
    else
    {
        pageNumSize = QFontMetrics(font()).boundingRect("999").width() + 4;
    }
    currentX = 0;
    currentY = 0;
    this->setCurrentPage(0);
    this->updateAllowedDisplacement();
    if(automaticFixedWidth)
    {
        this->setMinimumWidth(this->getThumbCellWidth());
        emit automaticallyResizedSelf(this->width());
    }
}

void ThumbnailWidget::notifyPageThumbnailAvailable(const QString& srcID, int page)
{
    if(comic)
    {
        int thumbCellHeight = getThumbCellHeight();
        int visibleThumbRangeStart = currentY / thumbCellHeight;
        int visibleThumbRangeEnd = (currentY + height()) / thumbCellHeight + 1;
        if(page >= visibleThumbRangeStart && page <= visibleThumbRangeEnd && srcID == comic->getID()) update();
    }
}

ComicSource* ThumbnailWidget::comicSource()
{
    return this->comic;
}

void ThumbnailWidget::setHorizontalScrollPosition(int pos)
{
    this->currentX = pos;
    ensureDisplacementWithinAllowedBounds();
    update();
}

void ThumbnailWidget::setVerticalScrollPosition(int pos)
{
    this->currentY = pos;
    ensureDisplacementWithinAllowedBounds();
    update();
}

void ThumbnailWidget::setCurrentPage(int page)
{
    if(page > 0 && std::abs(page - this->currentPage) > 4) emit this->thumbnailerShouldBeRefocused(page);
    this->currentPage = page;
    this->ensurePageVisible(page);
    dynamicBackground = {};
    update();
}

void ThumbnailWidget::paintEvent(QPaintEvent* event)
{
    if(active)
    {
        QPainter painter(this);

        ensureDisplacementWithinAllowedBounds();

        QColor finalBkg;
        if(thumbBkg == "dynamic")
        {
            painter.fillRect(painter.viewport(), this->palette().color(QPalette::Window));
            if(dynamicBackground.isValid())
            {
                painter.fillRect(painter.viewport(), dynamicBackground);
                finalBkg = dynamicBackground;
            }
        }
        else if(auto color = QColor(thumbBkg); color.isValid())
        {
            painter.fillRect(painter.viewport(), color);
            finalBkg = color;
        }
        else
        {
            painter.fillRect(painter.viewport(), this->palette().color(QPalette::Window));
            finalBkg = this->palette().color(QPalette::Window);
        }

        if(comic)
        {
            auto srcID = comic->getID();
            auto width = painter.viewport().width();
            auto height = this->height();
            int thumbCellHeight = getThumbCellHeight();
            int thumbCellWidth = getThumbCellWidth();
            int startIndex = std::floor(static_cast<double>(currentY) / thumbCellHeight);
            int endIndex = std::min(std::ceil(static_cast<double>(currentY + height) / thumbCellHeight), double(comic->getPageCount() - 1));
            int offsetY = -(currentY % thumbCellHeight);
            int offsetX = width < thumbCellWidth ? -currentX : std::ceil((width - thumbCellWidth) / 2.0);
            int pageNumOffset = showPageNums ? pageNumSize : 0;

            if(currentPage > 0 && !dynamicBackground.isValid() && thumbBkg == "dynamic")
            {
                if(auto img = ThumbCache::cache().getPixmap({comic->getID(), currentPage}); !img.isNull())
                {
                    dynamicBackground = MainWindow::getMostCommonEdgeColor(img.toImage(), {});
                    painter.fillRect(painter.viewport(), dynamicBackground);
                    finalBkg = dynamicBackground;
                }
            }

            QPixmap loadingPixmap = QPixmap(thumbWidth, thumbHeight);
            loadingPixmap.fill(finalBkg.lighter());
            QPainter lPainter(&loadingPixmap);
            lPainter.setPen(contrastColor(finalBkg.lighter()));
            lPainter.drawText(lPainter.viewport(), "...", QTextOption{Qt::AlignCenter});
            lPainter.end();

            for(int i = startIndex; i <= endIndex; i++)
            {
                QPixmap thumb = ThumbCache::cache().getPixmap({srcID, i});
                if(thumb.isNull()) thumb = loadingPixmap;

                if(i == currentPage - 1)
                {
                    painter.fillRect(QRectF(0, offsetY, width, thumbCellHeight), finalBkg.darker());
                }
                if(showPageNums)
                {
                    painter.setPen(contrastColor(i == currentPage - 1 ? finalBkg.darker() : finalBkg));
                    painter.drawText(QRectF(offsetX, offsetY, pageNumSize, thumbCellHeight), QString::number(i + 1), QTextOption{Qt::AlignCenter});
                }
                painter.setPen(Qt::black);
                offsetY += thumbSpacing;
                if(thumbnailBorder)
                {
                    painter.setPen(thumbBorderColor);
                    painter.drawRect(offsetX + thumbSpacing + pageNumOffset + (thumbWidth - thumb.width()) / 2.0, offsetY + (thumbHeight - thumb.height()) / 2.0, thumb.width() + 1, thumb.height() + 1);
                    offsetY += 1;
                    painter.drawPixmap(offsetX + thumbSpacing + pageNumOffset + 1 + (thumbWidth - thumb.width()) / 2.0, offsetY + (thumbHeight - thumb.height()) / 2.0, thumb);
                    offsetY += 1;
                    painter.setPen(Qt::black);
                }
                else
                {
                    painter.drawPixmap(offsetX + thumbSpacing + pageNumOffset + (thumbWidth - thumb.width()) / 2.0, offsetY + (thumbHeight - thumb.height()) / 2.0, thumb);
                }
                offsetY += thumbHeight;
                offsetY += thumbSpacing;
            }

            emit this->updateHorizontalScrollBar(allowedXDisplacement, currentX, width, getThumbCellWidth());
            emit this->updateVerticalScrollBar(allowedYDisplacement, currentY, height, getThumbCellHeight());
        }
    }
    else
    {
        QWidget::paintEvent(event);
    }
}

void ThumbnailWidget::mousePressEvent(QMouseEvent* event)
{
    int clickIdx = (currentY + event->y()) / getThumbCellHeight();
    emit this->pageChangeRequested(clickIdx + 1);
}

void ThumbnailWidget::wheelEvent(QWheelEvent* event)
{
    if(event->angleDelta().y() > 0)
    {
        emit this->prevPageRequested();
    }
    else if(event->angleDelta().y() < 0)
    {
        emit this->nextPageRequested();
    }
}

void ThumbnailWidget::resizeEvent(QResizeEvent*)
{
    this->updateAllowedDisplacement();
}

void ThumbnailWidget::ensureDisplacementWithinAllowedBounds()
{
    if(currentX < 0) currentX = 0;
    if(currentX > allowedXDisplacement) currentX = allowedXDisplacement;
    if(currentY < 0) currentY = 0;
    if(currentY > allowedYDisplacement) currentY = allowedYDisplacement;
    emit this->updateHorizontalScrollBar(allowedXDisplacement, currentX, this->width(), getThumbCellWidth());
    emit this->updateVerticalScrollBar(allowedYDisplacement, currentY, this->height(), getThumbCellHeight());
}

void ThumbnailWidget::updateAllowedDisplacement()
{
    if(comic)
    {
        int thumbCellHeight = getThumbCellHeight();
        allowedYDisplacement = std::max(0, comic->getPageCount() * thumbCellHeight - height());
    }
    else
    {
        allowedYDisplacement = 0;
    }
    int thumbCellWidth = getThumbCellWidth();
    allowedXDisplacement = std::max(0, thumbCellWidth - this->width());
    this->ensureDisplacementWithinAllowedBounds();
}

void ThumbnailWidget::ensurePageVisible(int page)
{
    if(page <= 0)
    {
        currentY = 0;
    }
    else
    {
        int thumbCellHeight = getThumbCellHeight();
        int startY = std::max(0, (page - 1) * thumbCellHeight);
        if(startY < currentY || startY + thumbCellHeight > currentY + height()) currentY = startY;
    }
    updateAllowedDisplacement();
}

int ThumbnailWidget::getThumbCellWidth() const
{
    return thumbWidth + 2 * thumbSpacing * 2 * int(thumbnailBorder) + (showPageNums ? pageNumSize : 0);
}

int ThumbnailWidget::getThumbCellHeight() const
{
    return thumbHeight + 2 * thumbSpacing + 2 * int(thumbnailBorder);
}
