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

#ifndef METADATA_H
#define METADATA_H

#include <QString>

struct ComicMetadata {
    bool valid = false;
    QString title;
    QString fileName;
};

struct PageMetadata {
    bool valid = false;
    QString fileName;
    QString fileType;
    int fileSize = 0;
    int width = 0;
    int height = 0;
};

#endif // METADATA_H
