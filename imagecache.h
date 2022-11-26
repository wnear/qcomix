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

#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <QPixmap>
#include <QString>
#include <QMutex>
#include <QReadWriteLock>
#include <QMap>

struct imgCacheEntry
{
    QString id;
    int page = -1;
    QPixmap data;
};

class ImageCache
{
public:
    static ImageCache& cache();
    QPixmap getImage(const QPair<QString, int>& key);
    void addImage(const QPair<QString, int>& key, const QPixmap& img);
    int hasKey(const QPair<QString, int>& key);
    void initialize(int maxCount);

private:
    void maintain();
    int maxCount = 0;
    QVector<imgCacheEntry> storage;
    QReadWriteLock mut { QReadWriteLock::Recursive };
};

class ThumbCache
{
public:
    static ThumbCache& cache();
    QPixmap getPixmap(const QPair<QString, int>& key);
    void addImage(const QPair<QString, int>& key, QPixmap img);
    bool hasKey(const QPair<QString, int>& key);
    void initialize(int maxCount);
    void maintain();
    QList<QPair<QString, int>> keys;

private:
    int maxCount = 0;
    QMap<QPair<QString, int>, QPixmap> storage;
    QMutex mut;
};

#endif // IMAGECACHE_H
