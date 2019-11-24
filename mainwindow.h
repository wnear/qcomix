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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVariant>
#include <QScrollBar>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QTreeWidget>
#include <QKeyEvent>
#include <QJsonArray>
#include <QJsonObject>
#include "metadata.h"

namespace Ui
{
    class MainWindow;
}

class ComicSource;
class QSettings;

class FileSystemFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    FileSystemFilterProxyModel(QObject *parent = 0) : QSortFilterProxyModel(parent) {}

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void init(const QString& profile, const QString &openFileName);
    static int getSavedPositionForFilePath(const QString& id);
    static void savePositionForFilePath(const QString& p, int page);
    void deleteSelectedBookmarks();
    ~MainWindow() override;

private slots:
    void on_actionAbout_Qt_triggered();
    void on_actionAbout_QComix_triggered();
    void on_actionQuit_triggered();
    void on_actionMinimize_triggered();
    void on_actionFullscreen_toggled(bool on);
    void on_actionHide_all_toggled(bool arg1);
    void on_actionShow_side_panels_toggled(bool arg1);
    void on_actionShow_scrollbars_toggled(bool arg1);
    void on_actionShow_main_toolbar_toggled(bool arg1);
    void on_actionShow_menu_toggled(bool arg1);
    void on_actionFit_width_mode_triggered();
    void on_actionNext_page_triggered();
    void updateWindowIcon(QImage page);
    void on_actionPrevious_page_triggered();
    void on_actionFit_height_mode_triggered();
    void on_actionFit_original_size_mode_triggered();
    void on_actionBest_fit_mode_triggered();
    void updateArchiveMetadata(ComicMetadata m);
    void on_actionManual_zoom_mode_triggered();
    void on_actionClose_comic_triggered();
    void on_actionOpen_directory_triggered();
    void on_actionOpen_triggered();
    void on_actionReload_triggered();
    void on_actionGo_to_page_triggered();
    void on_actionFirst_page_triggered();
    void on_actionLast_page_triggered();
    void on_actionNext_comic_triggered();
    void on_actionPrevious_comic_triggered();
    void on_actionRotate_clockwise_triggered();
    void on_actionRotate_counterclockwise_triggered();
    void on_actionFlip_horizontally_triggered(bool arg1);
    void on_actionFlip_vertically_triggered(bool arg1);
    void on_actionCopy_image_to_clipboard_triggered();
    void on_actionCopy_image_file_to_clipboard_triggered();
    void on_actionRotate_180_degrees_triggered();
    void on_actionSmart_scroll_vertical_first_triggered(bool checked);
    void on_actionManga_mode_triggered(bool checked);
    void on_actionDouble_page_mode_triggered(bool checked);
    void on_actionZoom_in_triggered();
    void on_actionZoom_out_triggered();
    void on_actionReset_zoom_triggered();
    void on_actionStretch_small_images_triggered(bool checked);
    void on_actionKeep_transformation_triggered(bool checked);
    void on_actionSet_slideshow_interval_triggered();
    void on_actionSlideshow_triggered(bool checked);
    void updateUIState();
    void on_actionAdd_bookmark_triggered();
    void saveBookmarksFromTreeWidget();
    void on_actionFit_to_fixed_size_mode_triggered();
    void on_actionSmart_scroll_triggered(bool checked);
    void on_actionAllow_free_drag_triggered(bool checked);

    void on_actionMagnifying_lens_triggered(bool checked);

private:
    void loadComic(ComicSource* src);
    void nextPage();
    void previousPage();
    int currentPageInWindowTitle = 0;
    int maxPageInWindowTitle = 0;
    QString nameInWindowTitle;
    void updateWindowTitle();
    Ui::MainWindow *ui;
    QScrollBar * scr_h = nullptr;
    QScrollBar * scr_v = nullptr;
    QFileSystemModel fileSystemModel;
    FileSystemFilterProxyModel fileSystemFilterModel;
    bool prevMenuVisible = false;
    bool prevToolBarVisible = false;
    bool prevSidePanelVisible = false;
    bool prevScrollBarsVisible = false;
    void saveBookmarks(const QJsonArray &bookmarks);
    void loadBookmarks();
    void loadRecentFiles();
    void addToRecentFiles(const QString& path);
    void rebuildRecentFilesMenu();
    void saveRecentFiles();
    QString readLastViewedFilePath();
    void saveLastViewedFilePath(const QString& p);
    void updateBookmarkSideBar(const QJsonArray &bookmarks);
    void rebuildOpenImageWithMenu();
    void rebuildOpenWithMenu(ComicSource* src);
    void rebuildOpenMenu(QAction* action, const QStringList& strList, bool image);
    static QJsonObject rememberedPages;
    QStringList recentFiles;

protected:
    void closeEvent(QCloseEvent* event) override;

public:
    static QString getIconFileName(const QString& iconName);
    static QVariant getOption(const QString& key);
    static QSettings* userProfile;
    static QSettings* defaultSettings;
    static QColor getMostCommonEdgeColor(const QPixmap& left_pixmap, const QPixmap& right_pixmap);
};

class BookmarksTreeWidgetEventFilter : public QObject
{
    Q_OBJECT

public:
    BookmarksTreeWidgetEventFilter(MainWindow* window) : window(window) {}
    virtual bool eventFilter(QObject *target, QEvent *event) override
    {
        if(auto tree = dynamic_cast<QTreeWidget*>(target))
        {
            if (event->type() == QEvent::KeyPress)
            {
                auto keyEv = static_cast<QKeyEvent*>(event);
                if(keyEv->key() == Qt::Key_Delete)
                {
                    window->deleteSelectedBookmarks();
                    return true;
                }
            }
        }
        return false;
    }

private:
    MainWindow* window = nullptr;
};

#endif // MAINWINDOW_H
