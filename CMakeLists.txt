cmake_minimum_required(VERSION 3.0.0)

project(qcomix)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} /usr/share/cmake/Modules)

find_package(Qt5Widgets)
find_package(Qt5Network)
find_package(Qt5Gui)
find_package(Qt5Xml)
find_package(QuaZip-Qt5)

set(SOURCES
  main.cpp
  mainwindow.cpp
  mainwindow.h
  aboutdialog.cpp
  aboutdialog.h
  comicsource.cpp
  comicsource.h
  epubcomicsource.cpp
  mobicomicsource.cpp
  rarcomicsource.cpp
  pdfcomicsource.cpp
  pageviewwidget.cpp
  pageviewwidget.h
  metadata.h
  version.h
  thumbnailer.cpp
  thumbnailer.h
  imagecache.cpp
  imagecache.h
  imagepreloader.cpp
  thumbnailwidget.cpp
  thumbnailwidget.h
  3rdparty/ksqueezedtextlabel.h
  3rdparty/ksqueezedtextlabel.cpp
)

set(FORMS
  mainwindow.ui
  aboutdialog.ui
)

set(RESOURCES
  resources.qrc
)

qt5_wrap_ui(FORM_H ${FORMS})
qt5_add_resources(RCC_SOURCES ${RESOURCES})

add_executable(qcomix ${SOURCES} ${FORM_H} ${RCC_SOURCES})

# libunrar link error, use external rar for now
target_link_libraries(qcomix Qt5::Widgets Qt5::Gui Qt5::Network QuaZip::QuaZip Qt5::Xml mobi poppler poppler-qt5)

install(TARGETS qcomix RUNTIME DESTINATION bin)
