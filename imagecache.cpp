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

#include "imagecache.h"

ImageCache& ImageCache::cache()
{
    static ImageCache cache;
    return cache;
}

QPixmap ImageCache::getImage(const QPair<QString, int>& key)
{
    for (const auto& s : storage)
    {
        if (s.page == key.second && s.id == key.first)
        {
            return s.data;
        }
    }
    return {};
}

void ImageCache::addImage(const QPair<QString, int>& key, const QPixmap& img)
{
    mut.lock();
    if (!hasKey(key))
    {
        maintain();
        storage.push_front(imgCacheEntry{key.first, key.second, img});
    }
    mut.unlock();
}

int ImageCache::hasKey(const QPair<QString, int>& key)
{
    for (const auto& s : storage)
    {
        if (s.page == key.second && s.id == key.first)
        {
            return 2;
        }
    }
    return 0;
}

void ImageCache::initialize(int maxCount)
{
    this->maxCount = maxCount;
}

void ImageCache::maintain()
{
    if (storage.size() > maxCount)
    {
        while (storage.size() > (maxCount * 2) / 3)
        {
            storage.back().data = QPixmap{};
            storage.pop_back();
        }
    }
}

ThumbCache& ThumbCache::cache()
{
    static ThumbCache cache;
    return cache;
}

QPixmap ThumbCache::getPixmap(const QPair<QString, int>& key)
{
    mut.lock();
    if (storage.count(key))
    {
        auto res = storage.value(key);
        mut.unlock();
        return res;
    }
    mut.unlock();
    return {};
}

void ThumbCache::addImage(const QPair<QString, int>& key, QPixmap img)
{
    mut.lock();
    storage[key] = img;
    keys.push_back(key);
    mut.unlock();
}

bool ThumbCache::hasKey(const QPair<QString, int>& key)
{
    mut.lock();
    auto res = storage.count(key);
    mut.unlock();
    return res;
}

void ThumbCache::initialize(int maxCount)
{
    this->maxCount = maxCount;
}

void ThumbCache::maintain()
{
    if (keys.size() > maxCount)
    {
        while (keys.size() > (maxCount * 2) / 3)
        {
            storage.remove(keys.front());
            keys.pop_front();
        }
    }
}
