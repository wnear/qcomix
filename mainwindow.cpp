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

#include "mainwindow.h"
#include "aboutdialog.h"
#include "comicsource.h"
#include "imagecache.h"
#include "thumbnailer.h"
#include "ui_mainwindow.h"
#include <QActionGroup>
#include <QClipboard>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QGraphicsScene>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonValue>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QMimeDatabase>
#include <QProcess>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QStyledItemDelegate>

QSettings* MainWindow::userProfile = nullptr;
QSettings* MainWindow::defaultSettings = nullptr;
QJsonObject MainWindow::rememberedPages = QJsonObject {};

class NoEditDelegate : public QStyledItemDelegate
{
    public:
        NoEditDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}
        virtual QWidget* createEditor(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const override
        {
            return nullptr;
        }
};

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(QApplication::instance(), &QApplication::aboutToQuit, this, &MainWindow::exitCleanup);

    scr_h = new QScrollBar;
    scr_h->setOrientation(Qt::Horizontal);
    static_cast<QGridLayout*>(this->ui->viewPage->layout())->addWidget(scr_h, 1, 0);

    scr_v = new QScrollBar;
    scr_v->setOrientation(Qt::Vertical);
    static_cast<QGridLayout*>(this->ui->viewPage->layout())->addWidget(scr_v, 0, 1);

    scr_h_t = new QScrollBar;
    scr_h_t->setOrientation(Qt::Horizontal);
    static_cast<QGridLayout*>(this->ui->thumbnailsTab->layout())->addWidget(scr_h_t, 1, 0);

    scr_v_t = new QScrollBar;
    scr_v_t->setOrientation(Qt::Vertical);
    static_cast<QGridLayout*>(this->ui->thumbnailsTab->layout())->addWidget(scr_v_t, 0, 1);

    for (auto& action : ui->menuBar->actions()) this->addAction(action);

    auto viewModeGroup = new QActionGroup(this);
    viewModeGroup->addAction(this->ui->actionBest_fit_mode);
    viewModeGroup->addAction(this->ui->actionFit_original_size_mode);
    viewModeGroup->addAction(this->ui->actionFit_to_fixed_size_mode);
    viewModeGroup->addAction(this->ui->actionFit_width_mode);
    viewModeGroup->addAction(this->ui->actionFit_height_mode);
    viewModeGroup->addAction(this->ui->actionManual_zoom_mode);

    this->ui->sidePanel->setCurrentIndex(0);

    fileSystemModel.setRootPath("");
    fileSystemFilterModel.setSourceModel(&fileSystemModel);
    this->ui->fileSystemView->setModel(&fileSystemFilterModel);
    this->ui->fileSystemView->setAnimated(true);
    this->ui->fileSystemView->setIndentation(4);
    this->ui->fileSystemView->setSortingEnabled(true);
    for (int i = 1; i < this->ui->fileSystemView->header()->count(); i++) this->ui->fileSystemView->setColumnHidden(i, true);
    this->ui->fileSystemView->setSelectionMode(QAbstractItemView::SingleSelection);
    this->ui->fileSystemView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    this->ui->fileSystemView->header()->setStretchLastSection(false);
    this->ui->fileSystemView->setExpandsOnDoubleClick(false);
    this->ui->bookmarksTreeWidget->setItemDelegateForColumn(0, new NoEditDelegate(this));
    this->ui->bookmarksTreeWidget->setSortingEnabled(true);
    this->ui->bookmarksTreeWidget->setIndentation(0);
    this->ui->bookmarksTreeWidget->setTextElideMode(Qt::ElideMiddle);
    this->ui->bookmarksTreeWidget->setEditTriggers(QTreeWidget::SelectedClicked);
    this->ui->bookmarksTreeWidget->setSelectionMode(QTreeWidget::ExtendedSelection);
    this->ui->bookmarksTreeWidget->installEventFilter(new BookmarksTreeWidgetEventFilter(this));
    this->ui->sidePanel->setFocusPolicy(Qt::NoFocus);
    this->ui->mainToolBar->setFocusPolicy(Qt::NoFocus);
}

void MainWindow::init(const QString& profile, const QString& openFileName)
{
    auto userConfigLocation = QStandardPaths::locate(QStandardPaths::AppConfigLocation, profile + ".conf");
    if (profile == "default" && !QFile::exists(userConfigLocation))
    {
        if(QFile defaultConf{":/default.conf"}; defaultConf.open(QFile::ReadOnly))
        {
            if(auto configDir = QStandardPaths::standardLocations(QStandardPaths::AppConfigLocation).first(); QDir{}.mkpath(configDir))
            {
                if(auto f = QFile{configDir+"/default.conf"}; f.open(QFile::WriteOnly | QFile::Text))
                {
                    userConfigLocation = configDir+"/default.conf";
                    f.write(defaultConf.readAll());
                    f.close();
                }
            }
        }
    }
    if (QFile::exists(userConfigLocation))
    {
        userProfile = new QSettings(userConfigLocation, QSettings::IniFormat);
    }
    defaultSettings = new QSettings(":/default.conf", QSettings::IniFormat);

    if (auto theme = getOption("theme").toString(); theme != "system")
    {
        static_cast<QApplication*>(QApplication::instance())->setStyle(QStyleFactory::create("fusion"));
        if (theme.toLower().endsWith(".css") && QFile::exists(theme))
        {
            QFile f(theme);
            f.open(QFile::ReadOnly);
            QString css(f.readAll());
            dynamic_cast<QApplication*>(QApplication::instance())->setStyleSheet(css);
            this->setStyleSheet(css);
        }
        else if (theme == "default")
        {
            QFile f(":/default.css");
            f.open(QFile::ReadOnly);
            QString css(f.readAll());
            dynamic_cast<QApplication*>(QApplication::instance())->setStyleSheet(css);
            this->setStyleSheet(css);
        }
        else qFatal("Invalid theme");
    }

    ImageCache::cache().initialize(getOption("mainImageCacheLimit").toInt());
    ThumbCache::cache().initialize(getOption("thumbnailCacheLimit").toInt());

    imagePreloader = new ImagePreloader{getOption("preloadedPageCount").toInt(), getOption("enableNearbyPagePreloader").toBool(), this};
    imagePreloader->start();

    int thumbnail_thread_count = std::max(1, MainWindow::getOption("thumbnailerThreadCount").toInt());
    int thumbX = getOption("thumbnailWidth").toInt();
    int thumbY = getOption("thumbnailHeight").toInt();
    bool cacheThumbsToDisk = getOption("cacheThumbnailsOnDisk").toBool();
    bool fastThumbScaling = !getOption("thumbHQScaling").toBool();

    for (int i = 0; i < thumbnail_thread_count; i++)
    {
        this->thumbnailerThreads.push_back(new Thumbnailer{i, thumbnail_thread_count, thumbX, thumbY, cacheThumbsToDisk, fastThumbScaling, this});
        connect(this->thumbnailerThreads.back(), &Thumbnailer::thumbnailReady, this->ui->thumbnails, &ThumbnailWidget::notifyPageThumbnailAvailable);
        connect(this->ui->thumbnails, &ThumbnailWidget::thumbnailerShouldBeRefocused, this->thumbnailerThreads.back(), &Thumbnailer::refocus);
        this->thumbnailerThreads.back()->start();
    }

    if (MainWindow::hasOption("shortcutOpen")) this->ui->actionOpen->setShortcut(QKeySequence{MainWindow::getOption("shortcutOpen").toString()});
    if (MainWindow::hasOption("shortcutRecent")) this->ui->actionRecent->setShortcut(QKeySequence{MainWindow::getOption("shortcutRecent").toString()});
    if (MainWindow::hasOption("shortcutReload")) this->ui->actionReload->setShortcut(QKeySequence{MainWindow::getOption("shortcutReload").toString()});
    if (MainWindow::hasOption("shortcutMinimize")) this->ui->actionMinimize->setShortcut(QKeySequence{MainWindow::getOption("shortcutMinimize").toString()});
    if (MainWindow::hasOption("shortcutQuit")) this->ui->actionQuit->setShortcut(QKeySequence{MainWindow::getOption("shortcutQuit").toString()});
    if (MainWindow::hasOption("shortcutFullscreen")) this->ui->actionFullscreen->setShortcut(QKeySequence{MainWindow::getOption("shortcutFullscreen").toString()});
    if (MainWindow::hasOption("shortcutDouble_page_mode")) this->ui->actionDouble_page_mode->setShortcut(QKeySequence{MainWindow::getOption("shortcutDouble_page_mode").toString()});
    if (MainWindow::hasOption("shortcutManga_mode")) this->ui->actionManga_mode->setShortcut(QKeySequence{MainWindow::getOption("shortcutManga_mode").toString()});
    if (MainWindow::hasOption("shortcutBest_fit_mode")) this->ui->actionBest_fit_mode->setShortcut(QKeySequence{MainWindow::getOption("shortcutBest_fit_mode").toString()});
    if (MainWindow::hasOption("shortcutFit_width_mode")) this->ui->actionFit_width_mode->setShortcut(QKeySequence{MainWindow::getOption("shortcutFit_width_mode").toString()});
    if (MainWindow::hasOption("shortcutFit_height_mode")) this->ui->actionFit_height_mode->setShortcut(QKeySequence{MainWindow::getOption("shortcutFit_height_mode").toString()});
    if (MainWindow::hasOption("shortcutFit_original_size_mode")) this->ui->actionFit_original_size_mode->setShortcut(QKeySequence{MainWindow::getOption("shortcutFit_original_size_mode").toString()});
    if (MainWindow::hasOption("shortcutManual_zoom_mode")) this->ui->actionManual_zoom_mode->setShortcut(QKeySequence{MainWindow::getOption("shortcutManual_zoom_mode").toString()});
    if (MainWindow::hasOption("shortcutSlideshow")) this->ui->actionSlideshow->setShortcut(QKeySequence{MainWindow::getOption("shortcutSlideshow").toString()});
    if (MainWindow::hasOption("shortcutMagnifying_lens")) this->ui->actionMagnifying_lens->setShortcut(QKeySequence{MainWindow::getOption("shortcutMagnifying_lens").toString()});
    if (MainWindow::hasOption("shortcutZoom_in")) this->ui->actionZoom_in->setShortcut(QKeySequence{MainWindow::getOption("shortcutZoom_in").toString()});
    if (MainWindow::hasOption("shortcutZoom_out")) this->ui->actionZoom_out->setShortcut(QKeySequence{MainWindow::getOption("shortcutZoom_out").toString()});
    if (MainWindow::hasOption("shortcutReset_zoom")) this->ui->actionReset_zoom->setShortcut(QKeySequence{MainWindow::getOption("shortcutReset_zoom").toString()});
    if (MainWindow::hasOption("shortcutNext_page")) this->ui->actionNext_page->setShortcut(QKeySequence{MainWindow::getOption("shortcutNext_page").toString()});
    if (MainWindow::hasOption("shortcutPrevious_page")) this->ui->actionPrevious_page->setShortcut(QKeySequence{MainWindow::getOption("shortcutPrevious_page").toString()});
    if (MainWindow::hasOption("shortcutGo_to_page")) this->ui->actionGo_to_page->setShortcut(QKeySequence{MainWindow::getOption("shortcutGo_to_page").toString()});
    if (MainWindow::hasOption("shortcutFirst_page")) this->ui->actionFirst_page->setShortcut(QKeySequence{MainWindow::getOption("shortcutFirst_page").toString()});
    if (MainWindow::hasOption("shortcutLast_page")) this->ui->actionLast_page->setShortcut(QKeySequence{MainWindow::getOption("shortcutLast_page").toString()});
    if (MainWindow::hasOption("shortcutNext_comic")) this->ui->actionNext_comic->setShortcut(QKeySequence{MainWindow::getOption("shortcutNext_comic").toString()});
    if (MainWindow::hasOption("shortcutPrevious_comic")) this->ui->actionPrevious_comic->setShortcut(QKeySequence{MainWindow::getOption("shortcutPrevious_comic").toString()});
    if (MainWindow::hasOption("shortcutShow_menu")) this->ui->actionShow_menu->setShortcut(QKeySequence{MainWindow::getOption("shortcutShow_menu").toString()});
    if (MainWindow::hasOption("shortcutShow_main_toolbar")) this->ui->actionShow_main_toolbar->setShortcut(QKeySequence{MainWindow::getOption("shortcutShow_main_toolbar").toString()});
    if (MainWindow::hasOption("shortcutAdd_bookmark")) this->ui->actionAdd_bookmark->setShortcut(QKeySequence{MainWindow::getOption("shortcutAdd_bookmark").toString()});
    if (MainWindow::hasOption("shortcutAbout_QComix")) this->ui->actionAbout_QComix->setShortcut(QKeySequence{MainWindow::getOption("shortcutAbout_QComix").toString()});
    if (MainWindow::hasOption("shortcutAbout_Qt")) this->ui->actionAbout_Qt->setShortcut(QKeySequence{MainWindow::getOption("shortcutAbout_Qt").toString()});
    if (MainWindow::hasOption("shortcutClose_comic")) this->ui->actionClose_comic->setShortcut(QKeySequence{MainWindow::getOption("shortcutClose_comic").toString()});
    if (MainWindow::hasOption("shortcutRotate_clockwise")) this->ui->actionRotate_clockwise->setShortcut(QKeySequence{MainWindow::getOption("shortcutRotate_clockwise").toString()});
    if (MainWindow::hasOption("shortcutRotate_counterclockwise")) this->ui->actionRotate_counterclockwise->setShortcut(QKeySequence{MainWindow::getOption("shortcutRotate_counterclockwise").toString()});
    if (MainWindow::hasOption("shortcutFlip_horizontally")) this->ui->actionFlip_horizontally->setShortcut(QKeySequence{MainWindow::getOption("shortcutFlip_horizontally").toString()});
    if (MainWindow::hasOption("shortcutFlip_vertically")) this->ui->actionFlip_vertically->setShortcut(QKeySequence{MainWindow::getOption("shortcutFlip_vertically").toString()});
    if (MainWindow::hasOption("shortcutKeep_transformation")) this->ui->actionKeep_transformation->setShortcut(QKeySequence{MainWindow::getOption("shortcutKeep_transformation").toString()});
    if (MainWindow::hasOption("shortcutCopy_image_to_clipboard")) this->ui->actionCopy_image_to_clipboard->setShortcut(QKeySequence{MainWindow::getOption("shortcutCopy_image_to_clipboard").toString()});
    if (MainWindow::hasOption("shortcutCopy_image_file_to_clipboard")) this->ui->actionCopy_image_file_to_clipboard->setShortcut(QKeySequence{MainWindow::getOption("shortcutCopy_image_file_to_clipboard").toString()});
    if (MainWindow::hasOption("shortcutRotate_180_degrees")) this->ui->actionRotate_180_degrees->setShortcut(QKeySequence{MainWindow::getOption("shortcutRotate_180_degrees").toString()});
    if (MainWindow::hasOption("shortcutShow_scrollbars")) this->ui->actionShow_scrollbars->setShortcut(QKeySequence{MainWindow::getOption("shortcutShow_scrollbars").toString()});
    if (MainWindow::hasOption("shortcutShow_side_panels")) this->ui->actionShow_side_panels->setShortcut(QKeySequence{MainWindow::getOption("shortcutShow_side_panels").toString()});
    if (MainWindow::hasOption("shortcutHide_all")) this->ui->actionHide_all->setShortcut(QKeySequence{MainWindow::getOption("shortcutHide_all").toString()});
    if (MainWindow::hasOption("shortcutOpen_with")) this->ui->actionOpen_with->setShortcut(QKeySequence{MainWindow::getOption("shortcutOpen_with").toString()});
    if (MainWindow::hasOption("shortcutOpen_image_with")) this->ui->actionOpen_image_with->setShortcut(QKeySequence{MainWindow::getOption("shortcutOpen_image_with").toString()});
    if (MainWindow::hasOption("shortcutStretch_small_images")) this->ui->actionStretch_small_images->setShortcut(QKeySequence{MainWindow::getOption("shortcutStretch_small_images").toString()});
    if (MainWindow::hasOption("shortcutSmart_scroll_vertical_first")) this->ui->actionSmart_scroll_vertical_first->setShortcut(QKeySequence{MainWindow::getOption("shortcutSmart_scroll_vertical_first").toString()});
    if (MainWindow::hasOption("shortcutOpen_directory")) this->ui->actionOpen_directory->setShortcut(QKeySequence{MainWindow::getOption("shortcutOpen_directory").toString()});
    if (MainWindow::hasOption("shortcutSet_slideshow_interval")) this->ui->actionSet_slideshow_interval->setShortcut(QKeySequence{MainWindow::getOption("shortcutSet_slideshow_interval").toString()});
    if (MainWindow::hasOption("shortcutFit_to_fixed_size_mode")) this->ui->actionFit_to_fixed_size_mode->setShortcut(QKeySequence{MainWindow::getOption("shortcutFit_to_fixed_size_mode").toString()});
    if (MainWindow::hasOption("shortcutSmart_scroll")) this->ui->actionSmart_scroll->setShortcut(QKeySequence{MainWindow::getOption("shortcutSmart_scroll").toString()});
    if (MainWindow::hasOption("shortcutAllow_free_drag")) this->ui->actionAllow_free_drag->setShortcut(QKeySequence{MainWindow::getOption("shortcutAllow_free_drag").toString()});
    if (MainWindow::hasOption("shortcutHydrus_search_query")) this->ui->actionHydrus_search_query->setShortcut(QKeySequence{MainWindow::getOption("shortcutHydrus_search_query").toString()});

    if (!MainWindow::getOption("enableHydrusIntegration").toBool())
    {
        this->ui->actionHydrus_search_query->setEnabled(false);
        this->ui->actionHydrus_search_query->setVisible(false);
    }

    this->ui->splitter->setStretchFactor(0, 0);
    this->ui->splitter->setStretchFactor(1, 1);
    adjustSidePanelWidth(MainWindow::getOption("sidePanelWidth").toInt());

    this->ui->actionFit_original_size_mode->setIcon(QIcon(getIconFileName("fit-original-size")));
    this->ui->actionFit_to_fixed_size_mode->setIcon(QIcon(getIconFileName("fit-fixed-size")));
    this->ui->actionBest_fit_mode->setIcon(QIcon(getIconFileName("fit-best")));
    this->ui->actionManual_zoom_mode->setIcon(QIcon(getIconFileName("fit-manual-zoom")));
    this->ui->actionFit_width_mode->setIcon(QIcon(getIconFileName("fit-width")));
    this->ui->actionFit_height_mode->setIcon(QIcon(getIconFileName("fit-height")));
    this->ui->actionPrevious_comic->setIcon(QIcon(getIconFileName("prev-archive")));
    this->ui->actionNext_comic->setIcon(QIcon(getIconFileName("next-archive")));
    this->ui->actionPrevious_page->setIcon(QIcon(getIconFileName("prev-page")));
    this->ui->actionNext_page->setIcon(QIcon(getIconFileName("next-page")));
    this->ui->actionFirst_page->setIcon(QIcon(getIconFileName("first-page")));
    this->ui->actionLast_page->setIcon(QIcon(getIconFileName("last-page")));
    this->ui->actionGo_to_page->setIcon(QIcon(getIconFileName("go-to-page")));

    auto expandWidget = new QWidget;
    expandWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->ui->mainToolBar->insertWidget(this->ui->actionBest_fit_mode, expandWidget);

    if (getOption("startMaximized").toBool()) this->showMaximized();
    if (getOption("startFullscreen").toBool()) this->ui->actionFullscreen->toggle();
    if (!getOption("showMenubar").toBool()) this->ui->actionShow_menu->toggle();
    if (!getOption("showToolbar").toBool()) this->ui->actionShow_main_toolbar->toggle();
    if (!getOption("showSidePanel").toBool()) this->ui->actionShow_side_panels->toggle();

    scr_h->setVisible(getOption("showScrollbars").toBool() && getOption("alwaysShowScrollbars").toBool());
    scr_v->setVisible(getOption("showScrollbars").toBool() && getOption("alwaysShowScrollbars").toBool());
    this->ui->actionShow_scrollbars->setChecked(getOption("showScrollbars").toBool());

    if (getOption("quitOnEscape").toBool())
    {
        auto esc = new QShortcut(Qt::Key_Escape, this);
        connect(esc, &QShortcut::activated, QApplication::instance(), &QApplication::quit);
        connect(esc, &QShortcut::activatedAmbiguously, QApplication::instance(), &QApplication::quit);
    }

    if (getOption("useCurrentPageAsWindowIcon").toBool())
    {
        connect(this->ui->view, &PageViewWidget::windowIconUpdateNeeded, this, &MainWindow::updateWindowIcon);
    }
    connect(this->ui->view, &PageViewWidget::archiveMetadataUpdateNeeded, this, &MainWindow::updateArchiveMetadata);
    connect(this->ui->view, &PageViewWidget::fitModeChanged, [this](PageViewWidget::FitMode fitMode)
    {
        this->ui->actionManual_zoom_mode->setChecked(fitMode == PageViewWidget::FitMode::ManualZoom);
        this->ui->actionFit_width_mode->setChecked(fitMode == PageViewWidget::FitMode::FitWidth);
        this->ui->actionFit_height_mode->setChecked(fitMode == PageViewWidget::FitMode::FitHeight);
        this->ui->actionBest_fit_mode->setChecked(fitMode == PageViewWidget::FitMode::FitBest);
        this->ui->actionFit_original_size_mode->setChecked(fitMode == PageViewWidget::FitMode::OriginalSize);
        this->ui->actionFit_to_fixed_size_mode->setChecked(fitMode == PageViewWidget::FitMode::FixedSize);
    });
    connect(this->ui->mainToolBar, &QToolBar::visibilityChanged, [this](bool visible)
    {
        if (!visible) this->ui->actionShow_main_toolbar->setChecked(false);
    });
    connect(this->ui->view, &PageViewWidget::currentPageChanged, [this](const QString & filePath, int current, int max)
    {
        if (getOption("useComicNameAsWindowTitle").toBool())
        {
            currentPageInWindowTitle = current;
            maxPageInWindowTitle = max;
        }

        if (!filePath.isEmpty() && getOption("rememberPage").toBool() && current > 0)
        {
            MainWindow::savePositionForFilePath(filePath, current);
        }

        updateWindowTitle();
    });
    connect(this->ui->view, &PageViewWidget::currentPageChanged, [this](const QString&, int current, int)
    {
        imagePreloader->preloadPages(this->ui->view->comicSource(), current);
    });
    connect(this->ui->view, &PageViewWidget::requestLoadNextComic, this, &MainWindow::on_actionNext_comic_triggered);
    connect(this->ui->view, &PageViewWidget::requestLoadPrevComic, this, &MainWindow::on_actionPrevious_comic_triggered);
    connect(this->ui->view, &PageViewWidget::imageMetadataUpdateNeeded, [this](const PageMetadata & m1, const PageMetadata & m2)
    {
        this->ui->labelim1v2->setVisible(m1.valid);
        this->ui->labelim1v3->setVisible(m1.valid);
        this->ui->labelim1v4->setVisible(m1.valid);
        this->ui->labelim1v5->setVisible(m1.valid);
        this->ui->im1vMime->setVisible(m1.valid);
        this->ui->im1vTitle->setVisible(m1.valid);
        this->ui->im1vFileName->setVisible(m1.valid);
        this->ui->im1vFileSize->setVisible(m1.valid);
        this->ui->im1vDimensions->setVisible(m1.valid);
        this->ui->labelim2v1->setVisible(m2.valid);
        this->ui->labelim2v2->setVisible(m2.valid);
        this->ui->labelim2v3->setVisible(m2.valid);
        this->ui->labelim2v4->setVisible(m2.valid);
        this->ui->labelim2v5->setVisible(m2.valid);
        this->ui->im2vMime->setVisible(m2.valid);
        this->ui->im2vFileName->setVisible(m2.valid);
        this->ui->im2vFileSize->setVisible(m2.valid);
        this->ui->im2vDimensions->setVisible(m2.valid);

        if (m2.valid)
        {
            this->ui->im1vTitle->setText("Image metadata (left)");
        }
        else
        {
            this->ui->im1vTitle->setText("Image metadata");
        }

        this->ui->im1vMime->setText(m1.fileType);
        this->ui->im1vFileName->setText(m1.fileName);
        this->ui->im1vFileSize->setText(this->locale().formattedDataSize(m1.fileSize));
        this->ui->im1vDimensions->setText(QString("%1x%2").arg(QString::number(m1.width), QString::number(m1.height)));

        this->ui->im2vMime->setText(m2.fileType);
        this->ui->im2vFileName->setText(m2.fileName);
        this->ui->im2vFileSize->setText(this->locale().formattedDataSize(m2.fileSize));
        this->ui->im2vDimensions->setText(QString("%1x%2").arg(QString::number(m2.width), QString::number(m2.height)));
    });

    connect(this->ui->view, &PageViewWidget::updateHorizontalScrollBar, [this](int max, int current, int visible)
    {
        const static int arrowKeyScrollPixels = MainWindow::getOption("arrowKeyScrollPixels").toInt();
        scr_h->blockSignals(true);
        scr_h->setMinimum(0);
        scr_h->setMaximum(max);
        scr_h->setValue(current);
        scr_h->setSingleStep(arrowKeyScrollPixels);
        scr_h->setPageStep(visible);
        this->scr_h->setVisible(this->ui->actionShow_scrollbars->isChecked() && (getOption("alwaysShowScrollbars").toBool() || max > 0));
        scr_h->blockSignals(false);
    });
    connect(scr_h, &QScrollBar::valueChanged, this->ui->view, &PageViewWidget::setHorizontalScrollPosition);

    connect(this->ui->thumbnails, &ThumbnailWidget::updateHorizontalScrollBar, [this](int max, int current, int visible, int singleStep)
    {
        scr_h_t->blockSignals(true);
        scr_h_t->setMinimum(0);
        scr_h_t->setMaximum(max);
        scr_h_t->setValue(current);
        scr_h_t->setSingleStep(singleStep);
        scr_h_t->setPageStep(visible);
        scr_h_t->setVisible(max > 0);
        scr_h_t->blockSignals(false);
    });
    connect(scr_h_t, &QScrollBar::valueChanged, this->ui->thumbnails, &ThumbnailWidget::setHorizontalScrollPosition);

    connect(this->ui->view, &PageViewWidget::updateVerticalScrollBar, [this](int max, int current, int visible)
    {
        const static int arrowKeyScrollPixels = MainWindow::getOption("arrowKeyScrollPixels").toInt();
        scr_v->blockSignals(true);
        scr_v->setMinimum(0);
        scr_v->setMaximum(max);
        scr_v->setValue(current);
        scr_v->setSingleStep(arrowKeyScrollPixels);
        scr_v->setPageStep(visible);
        this->scr_v->setVisible(this->ui->actionShow_scrollbars->isChecked() && (getOption("alwaysShowScrollbars").toBool() || max > 0));
        scr_v->blockSignals(false);
    });
    connect(scr_v, &QScrollBar::valueChanged, this->ui->view, &PageViewWidget::setVerticalScrollPosition);

    connect(this->ui->thumbnails, &ThumbnailWidget::updateVerticalScrollBar, [this](int max, int current, int visible, int singleStep)
    {
        scr_v_t->blockSignals(true);
        scr_v_t->setMinimum(0);
        scr_v_t->setMaximum(max);
        scr_v_t->setValue(current);
        scr_v_t->setSingleStep(singleStep);
        scr_v_t->setPageStep(visible);
        scr_v_t->setVisible(max > 0);
        scr_v_t->blockSignals(false);
    });
    connect(scr_v_t, &QScrollBar::valueChanged, this->ui->thumbnails, &ThumbnailWidget::setVerticalScrollPosition);

    connect(this->ui->fileSystemView, &QTreeView::doubleClicked,
            [this](const QModelIndex & index)
    {
        auto filePath = this->fileSystemModel.fileInfo(this->fileSystemFilterModel.mapToSource(index)).absoluteFilePath();
        this->ui->fileSystemView->scrollTo(fileSystemFilterModel.mapToSource(index));
        if (auto comic = createComicSource(filePath))
        {
            this->loadComic(comic);
        }
    });

    connect(this->ui->thumbnails, &ThumbnailWidget::pageChangeRequested, this->ui->view, &PageViewWidget::goToPage);
    connect(this->ui->thumbnails, &ThumbnailWidget::nextPageRequested, [&]()
    {
        this->ui->view->nextPage();
    });
    connect(this->ui->thumbnails, &ThumbnailWidget::prevPageRequested, this->ui->view, &PageViewWidget::previousPage);
    connect(this->ui->view, &PageViewWidget::pageViewConfigUINeedsToBeUpdated, this, &MainWindow::updateUIState);
    this->ui->bookmarksTreeWidget->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    connect(this->ui->bookmarksTreeWidget, &QTreeWidget::itemChanged, [this](QTreeWidgetItem*, int)
    {
        this->saveBookmarksFromTreeWidget();
    });
    connect(this->ui->bookmarksTreeWidget, &QTreeWidget::itemDoubleClicked, [this](QTreeWidgetItem * item, int)
    {
        auto filePath = item->text(0);
        auto page = item->text(1).toInt();
        if (QFileInfo::exists(filePath))
        {
            if (auto comic = createComicSource(filePath))
            {
                this->loadComic(comic);
                this->ui->view->goToPage(page);
            }
        }
    });

    loadBookmarks();
    loadRecentFiles();
    rebuildOpenImageWithMenu();

    this->ui->view->initialize(this->ui->thumbnails);

    if (MainWindow::getOption("thumbnailSidebarAutomaticWidth").toBool())
    {
        connect(this->ui->thumbnails, &ThumbnailWidget::automaticallyResizedSelf, this, &MainWindow::adjustSidePanelWidth);
        adjustSidePanelWidth(this->ui->thumbnails->minimumWidth());
    }

    if (MainWindow::getOption("openLastViewedOnStartup").toBool() && openFileName.isEmpty())
    {
        this->loadComic(createComicSource(readLastViewedFilePath()));
    }
    else
    {
        this->loadComic(createComicSource(openFileName));
    }
}

int MainWindow::getSavedPositionForFilePath(const QString& id)
{
    if (MainWindow::rememberedPages.isEmpty() && MainWindow::getOption("rememberPage").toBool())
    {
        auto path = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
        auto fileName = MainWindow::getOption("pageStorageFile").toString();
        auto filePath = path + "/" + fileName;
        if (!QFileInfo(filePath).exists())
        {
            QJsonObject empty;
            QJsonDocument doc;
            doc.setObject(empty);
            if (!QDir().mkpath(path))
            {
                QMessageBox::critical(nullptr, "Error", "Failed to create the remembered pages file!");
            }
            QFile f(filePath);
            if (f.open(QFile::WriteOnly))
            {
                f.write(doc.toJson());
                f.close();
            }
            else
            {
                QMessageBox::critical(nullptr, "Error", "Failed to create the remembered pages file!");
            }
        }

        QFile f(filePath);
        if (f.open(QFile::ReadOnly))
        {
            QJsonParseError err;
            auto doc = QJsonDocument::fromJson(f.readAll(), &err);
            if (err.error == QJsonParseError::NoError)
            {
                MainWindow::rememberedPages = doc.object();
            }
            else
            {
                QMessageBox::critical(nullptr, "Error", "The content of the remembered pages file is invalid!");
            }
            f.close();
        }
        else
        {
            QMessageBox::critical(nullptr, "Error", "Failed to load bookmarks, can't open the remembered pages file!");
        }
    }
    if (auto val = MainWindow::rememberedPages.value(id); !val.isUndefined())
    {
        return val.toInt(1);
    }
    return 1;
}

void MainWindow::savePositionForFilePath(const QString& p, int page)
{
    MainWindow::rememberedPages[p] = QJsonValue(page);

    auto path = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation)
                .first();
    auto fileName = MainWindow::getOption("pageStorageFile").toString();

    QJsonDocument doc;
    doc.setObject(MainWindow::rememberedPages);
    QFile f(path + "/" + fileName);
    if (f.open(QFile::WriteOnly))
    {
        f.write(doc.toJson());
        f.close();
    }
    else
    {
        QMessageBox::critical(nullptr, "Error", "Failed to save remembered pages, can't open the remembered pages file!");
    }
}

QVariant MainWindow::getOption(const QString& key)
{
    if (MainWindow::userProfile && MainWindow::userProfile->contains(key))
    {
        return MainWindow::userProfile->value(key);
    }
    else if (MainWindow::defaultSettings->contains(key))
    {
        return MainWindow::defaultSettings->value(key);
    }
    QMessageBox::critical(nullptr, "Error", QString("Missing/invalid configuration key: %1").arg(key));
    qFatal("Invalid key: %s", qPrintable(key));
}

bool MainWindow::hasOption(const QString& key)
{
    if (MainWindow::userProfile && MainWindow::userProfile->contains(key))
    {
        return true;
    }
    else return MainWindow::defaultSettings->contains(key);
}

QColor MainWindow::getMostCommonEdgeColor(const QImage& left_img, const QImage& right_img)
{
    bool fast = MainWindow::getOption("fasterDynamicBackgroundDetection").toBool();
    const int step = fast ? 4 : 1;

    QMap<QRgb, int> colorCountMap;

    auto left_im_w = left_img.width();
    auto left_im_h = left_img.height();

    auto right_im_w = right_img.width();
    auto right_im_h = right_img.height();

    // Left image - left side
    if (left_im_w > 0)
    {
        for (int i = 0; i < left_im_h; i += step) colorCountMap[left_img.pixel(0, i)]++;
    }

    // Left image - top
    if (left_im_h > 0)
    {
        for (int i = 0; i < left_im_w; i += step) colorCountMap[left_img.pixel(i, 0)]++;
    }

    // Left image - bottom
    if (left_im_h > 0)
    {
        for (int i = 0; i < left_im_w; i += step) colorCountMap[left_img.pixel(i, left_im_h - 1)]++;
    }

    // Left image - right side
    if (left_im_w > 0 && left_img.isNull())
    {
        for (int i = 0; i < left_im_h; i += step) colorCountMap[left_img.pixel(left_im_w - 1, i)]++;
    }

    if (!right_img.isNull())
    {
        // Right image - top
        if (right_im_h > 0)
        {
            for (int i = 0; i < right_im_w; i += step) colorCountMap[right_img.pixel(i, 0)]++;
        }

        // Right image - bottom
        if (right_im_h > 0)
        {
            for (int i = 0; i < right_im_w; i += step) colorCountMap[right_img.pixel(i, right_im_h - 1)]++;
        }

        // Right image - right side
        if (right_im_w > 0 && right_img.isNull())
        {
            for (int i = 0; i < right_im_h; i += step) colorCountMap[right_img.pixel(right_im_w - 1, i)]++;
        }
    }

    int maxcnt = 0;
    QRgb maxval = 0;
    bool ok = false;
    for (const auto& k : colorCountMap.keys())
    {
        if (colorCountMap[k] > maxcnt)
        {
            maxcnt = colorCountMap[k];
            maxval = k;
            ok = true;
        }
    }

    if (!ok) return Qt::transparent;
    return QColor::fromRgba(maxval);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionAbout_Qt_triggered()
{
    QApplication::aboutQt();
}

void MainWindow::on_actionAbout_QComix_triggered()
{
    AboutDialog d(this);
    d.exec();
}

void MainWindow::on_actionQuit_triggered()
{
    this->close();
}

void MainWindow::on_actionMinimize_triggered()
{
    this->showMinimized();
}

void MainWindow::on_actionFullscreen_toggled(bool on)
{
    if (on)
    {
        this->showFullScreen();
    }
    else
    {
        this->showNormal();
    }
}

void MainWindow::on_actionHide_all_toggled(bool arg1)
{
    if (arg1)
    {
        prevMenuVisible = this->ui->actionShow_menu->isChecked();
        prevScrollBarsVisible = this->ui->actionShow_scrollbars->isChecked();
        prevSidePanelVisible = this->ui->actionShow_side_panels->isChecked();
        prevToolBarVisible = this->ui->actionShow_main_toolbar->isChecked();
        this->ui->actionShow_menu->setChecked(false);
        this->ui->actionShow_main_toolbar->setChecked(false);
        this->ui->actionShow_side_panels->setChecked(false);
        this->ui->actionShow_scrollbars->setChecked(false);
    }
    else
    {
        if (prevMenuVisible && !this->ui->actionShow_menu->isChecked()) this->ui->actionShow_menu->toggle();
        if (prevToolBarVisible && !this->ui->actionShow_main_toolbar->isChecked()) this->ui->actionShow_main_toolbar->toggle();
        if (prevSidePanelVisible && !this->ui->actionShow_side_panels->isChecked()) this->ui->actionShow_side_panels->toggle();
        if (prevScrollBarsVisible && !this->ui->actionShow_scrollbars->isChecked()) this->ui->actionShow_scrollbars->toggle();
    }
}

void MainWindow::on_actionShow_side_panels_toggled(bool arg1)
{
    this->ui->sidePanel->setVisible(arg1);
    if (this->ui->sidePanel->isVisible()) this->ui->actionHide_all->setChecked(false);
}

void MainWindow::on_actionShow_scrollbars_toggled(bool arg1)
{
    this->scr_h->setVisible(arg1 && (getOption("alwaysShowScrollbars").toBool() || this->scr_h->maximum() > 0));
    this->scr_v->setVisible(arg1 && (getOption("alwaysShowScrollbars").toBool() || this->scr_v->maximum() > 0));
    if (this->scr_h->isVisible() || this->scr_v->isVisible()) this->ui->actionHide_all->setChecked(false);
}

void MainWindow::on_actionShow_main_toolbar_toggled(bool arg1)
{
    this->ui->mainToolBar->setVisible(arg1);
    if (this->ui->mainToolBar->isVisible()) this->ui->actionHide_all->setChecked(false);
}

void MainWindow::on_actionShow_menu_toggled(bool arg1)
{
    this->ui->menuBar->setVisible(arg1);
    if (this->ui->menuBar->isVisible()) this->ui->actionHide_all->setChecked(false);
}

void MainWindow::loadComic(ComicSource* src)
{
    nameInWindowTitle.clear();
    currentPageInWindowTitle = 0;
    maxPageInWindowTitle = 0;

    if (src)
    {
        if (getOption("useComicNameAsWindowTitle").toBool())
        {
            nameInWindowTitle = src->getTitle();
        }
        else
        {
            nameInWindowTitle = "qcomix";
        }

        if (getOption("useFirstPageAsWindowIcon").toBool() && src->getPageCount() > 0)
        {
            if (src->getPageCount() > 0)
            {
                setWindowIcon(src->getPagePixmap(0).scaled(256, 256, Qt::KeepAspectRatio));
            }
            else
            {
                setWindowIcon(QIcon(":/icon.png"));
            }
        }
        else
        {
            setWindowIcon(QIcon(":/icon.png"));
        }

        const QModelIndex rootIndex = fileSystemModel.index(src->getFilePath());
        this->ui->fileSystemView->setCurrentIndex(fileSystemFilterModel.mapFromSource(rootIndex));
        this->ui->fileSystemView->setExpanded(fileSystemFilterModel.mapFromSource(rootIndex), false);
        this->ui->fileSystemView->scrollTo(fileSystemFilterModel.mapFromSource(rootIndex));

        this->saveLastViewedFilePath(src->getFilePath());
        this->addToRecentFiles(src->getFilePath());
    }
    else
    {
        nameInWindowTitle = "qcomix";
        setWindowIcon(QIcon(":/icon.png"));

        const QModelIndex rootIndex = fileSystemModel.index("");
        this->ui->fileSystemView->setCurrentIndex(fileSystemFilterModel.mapFromSource(rootIndex));
        this->ui->fileSystemView->setExpanded(fileSystemFilterModel.mapFromSource(rootIndex), true);
        this->ui->fileSystemView->scrollTo(fileSystemFilterModel.mapFromSource(rootIndex));

        this->saveLastViewedFilePath({});
    }

    this->rebuildOpenWithMenu(src);

    updateWindowTitle();

    auto oldComic = this->ui->view->setComicSource(src);

    imagePreloader->stopCurrentWork();

    for (auto t : thumbnailerThreads)
    {
        t->stopCurrentWork();
    }

    delete oldComic;

    if (src)
    {
        for (auto t : thumbnailerThreads)
        {
            t->startWorking(src);
            if (this->ui->view->currentPage() > 6)
            {
                t->refocus(this->ui->view->currentPage());
            }
        }
    }

    this->ui->view->setFocus(Qt::OtherFocusReason);
}

void MainWindow::stopThreads()
{
    imagePreloader->exit();
    for (auto& thread : thumbnailerThreads)
    {
        thread->exit();
    }
}

void MainWindow::updateWindowTitle()
{
    QString title;
    if (currentPageInWindowTitle != 0 && maxPageInWindowTitle != 0 && MainWindow::getOption("useComicNameAsWindowTitle").toBool())
    {
        title = QString("[%1/%2] ").arg(QString::number(currentPageInWindowTitle), QString::number(maxPageInWindowTitle));
    }
    if (!nameInWindowTitle.isEmpty() && MainWindow::getOption("useComicNameAsWindowTitle").toBool())
    {
        title += nameInWindowTitle;
    }
    else
    {
        title = "qcomix";
    }
    setWindowTitle(title);
}

void MainWindow::saveBookmarks(const QJsonArray& bookmarks)
{
    auto path = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation)
                .first();
    auto fileName = MainWindow::getOption("bookmarksFileName").toString();

    QJsonDocument doc;
    doc.setArray(bookmarks);
    QFile f(path + "/" + fileName);
    if (f.open(QFile::WriteOnly))
    {
        f.write(doc.toJson());
        f.close();
    }
    else
    {
        QMessageBox::critical(this, "Error", "Failed to save bookmarks, can't open the bookmark file!");
    }
}

void MainWindow::loadBookmarks()
{
    auto path = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
    auto fileName = MainWindow::getOption("bookmarksFileName").toString();
    auto filePath = path + "/" + fileName;
    if (!QFileInfo::exists(filePath))
    {
        QJsonArray empty;
        QJsonDocument doc;
        doc.setArray(empty);
        if (!QDir().mkpath(path))
        {
            QMessageBox::critical(this, "Error", "Failed to create the bookmark file!");
        }
        QFile f(filePath);
        if (f.open(QFile::WriteOnly))
        {
            f.write(doc.toJson());
            f.close();
        }
        else
        {
            QMessageBox::critical(this, "Error", "Failed to create the bookmark file!");
        }
    }

    QFile f(filePath);
    if (f.open(QFile::ReadOnly))
    {
        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(f.readAll(), &err);
        if (err.error == QJsonParseError::NoError)
        {
            updateBookmarkSideBar(doc.array());
        }
        else
        {
            QMessageBox::critical(this, "Error", "The content of the bookmark file is invalid!");
        }
        f.close();
    }
    else
    {
        QMessageBox::critical(this, "Error", "Failed to load bookmarks, can't open the bookmark file!");
    }
}

void MainWindow::loadRecentFiles()
{
    if (MainWindow::getOption("storeRecentFiles").toBool())
    {
        auto path = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
        auto fileName = MainWindow::getOption("recentFilesStorage").toString();
        auto filePath = path + "/" + fileName;
        if (!QFileInfo::exists(filePath))
        {
            QJsonArray empty;
            QJsonDocument doc;
            doc.setArray(empty);
            if (!QDir().mkpath(path))
            {
                QMessageBox::critical(this, "Error", "Failed to create the recent files storage file!");
            }
            QFile f(filePath);
            if (f.open(QFile::WriteOnly))
            {
                f.write(doc.toJson());
                f.close();
            }
            else
            {
                QMessageBox::critical(this, "Error", "Failed to create the recent files storage file!");
            }
        }

        QFile f(filePath);
        if (f.open(QFile::ReadOnly))
        {
            QJsonParseError err{};
            auto doc = QJsonDocument::fromJson(f.readAll(), &err);
            if (err.error == QJsonParseError::NoError)
            {
                auto arr = doc.array();
                this->recentFiles.clear();
                for (int i = 0; i < arr.size(); i++)
                {
                    this->recentFiles.append(arr[i].toString());
                }
                this->rebuildRecentFilesMenu();
            }
            else
            {
                QMessageBox::critical(this, "Error", "The content of the recent files storage file is invalid!");
            }
            f.close();
        }
        else
        {
            QMessageBox::critical(this, "Error", "Failed to load recent files, can't open the recent files storage file!");
        }
    }
}

void MainWindow::addToRecentFiles(const QString& path)
{
    auto maxNum = MainWindow::getOption("numberOfRecentFiles").toInt();
    this->recentFiles.removeAll(path);
    this->recentFiles.push_front(path);
    while (maxNum >= 0 && recentFiles.length() > maxNum) recentFiles.pop_back();
    this->rebuildRecentFilesMenu();
    this->saveRecentFiles();
}

void MainWindow::rebuildRecentFilesMenu()
{
    if (MainWindow::getOption("storeRecentFiles").toBool())
    {
        bool hydrusEnabled = MainWindow::getOption("enableHydrusIntegration").toBool();
        this->ui->actionRecent->setVisible(this->recentFiles.length());
        if (!this->ui->actionRecent->menu()) this->ui->actionRecent->setMenu(new QMenu {});
        this->ui->actionRecent->menu()->clear();
        for (const auto& f : this->recentFiles)
        {
            auto info = QFileInfo(f);
            if (info.exists() || (hydrusEnabled && f.startsWith("hydrus://")))
            {
                auto openRecent = new QAction { this->ui->actionRecent->menu() };
                openRecent->setText((hydrusEnabled && f.startsWith("hydrus://")) ? f : info.fileName());
                connect(openRecent, &QAction::triggered, [this, f]()
                {
                    this->loadComic(createComicSource(f));
                });
                this->ui->actionRecent->menu()->addAction(openRecent);
            }
        }
    }
    else
    {
        this->ui->actionRecent->setVisible(false);
    }
}

void MainWindow::saveRecentFiles()
{
    if (MainWindow::getOption("storeRecentFiles").toBool())
    {
        auto path = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation)
                    .first();
        auto fileName = MainWindow::getOption("recentFilesStorage").toString();

        QJsonDocument doc;
        doc.setArray(QJsonArray::fromStringList(this->recentFiles));
        QFile f(path + "/" + fileName);
        if (f.open(QFile::WriteOnly))
        {
            f.write(doc.toJson());
            f.close();
        }
        else
        {
            QMessageBox::critical(this, "Error", "Failed to save recent files, can't open the recent files storage file!");
        }
    }
}

QString MainWindow::readLastViewedFilePath()
{
    auto path = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation)
                .first();
    auto fileName = MainWindow::getOption("lastViewedFileStorage").toString();
    auto filePath = path + "/" + fileName;
    if (!QFileInfo::exists(filePath))
    {
        if (!QDir().mkpath(path))
        {
            QMessageBox::critical(this, "Error", "Failed to create the file storing the last viewed comic!");
        }
        QFile f(filePath);
        if (f.open(QFile::WriteOnly | QFile::Text))
        {
            f.write("");
            f.close();
        }
        else
        {
            QMessageBox::critical(this, "Error", "Failed to create the file storing the last viewed comic!");
        }
    }

    QFile f(filePath);
    if (f.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream stream(&f);
        auto res = stream.readAll();
        f.close();
        return res;
    }
    else
    {
        QMessageBox::critical(this, "Error", "Failed to load bookmarks, can't open the bookmark file!");
    }

    return {};
}

void MainWindow::saveLastViewedFilePath(const QString& p)
{
    if (MainWindow::getOption("openLastViewedOnStartup").toBool())
    {
        auto path = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
        auto fileName = MainWindow::getOption("lastViewedFileStorage").toString();
        auto filePath = path + "/" + fileName;

        QFile f(filePath);
        if (f.open(QFile::WriteOnly | QFile::Text))
        {
            QTextStream stream(&f);
            stream << p;
            f.close();
        }
        else
        {
            QMessageBox::critical(this, "Error", "Failed to create the file storing the last viewed comic!");
        }
    }
}

void MainWindow::updateBookmarkSideBar(const QJsonArray& bookmarks)
{
    this->ui->bookmarksTreeWidget->blockSignals(true);
    this->ui->bookmarksTreeWidget->clear();
    auto bsize = bookmarks.size();
    if (bookmarks.size() % 3 != 0)
    {
        QMessageBox::critical(this, "Error", "Can't load bookmarks, invalid bookmark array!");
    }
    else
    {
        for (int i = 0; i < bsize; i += 3)
        {
            auto item = new QTreeWidgetItem{};
            item->setText(0, bookmarks[i].toString());
            item->setText(1, bookmarks[i + 1].toString());
            item->setText(2, bookmarks[i + 2].toString());
            item->setToolTip(0, bookmarks[i].toString());
            item->setToolTip(1, bookmarks[i + 1].toString());
            item->setToolTip(2, bookmarks[i + 2].toString());
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            this->ui->bookmarksTreeWidget->addTopLevelItem(item);
        }
    }
    this->ui->bookmarksTreeWidget->blockSignals(false);
}

void MainWindow::rebuildOpenImageWithMenu()
{
    this->rebuildOpenMenu(this->ui->actionOpen_image_with, MainWindow::getOption("openImageWith").toStringList(), true);
}

void MainWindow::adjustSidePanelWidth(int w)
{
    this->ui->splitter->setSizes(QList<int>() << w << 1);
    this->ui->bookmarksTreeWidget->setColumnWidth(0, w / 3);
    this->ui->bookmarksTreeWidget->setColumnWidth(2, w / 3);
    this->ui->bookmarksTreeWidget->setColumnWidth(1, 40);
}

void MainWindow::rebuildOpenWithMenu(ComicSource* src)
{
    if (src)
    {
        if (dynamic_cast<DirectoryComicSource*>(src))
        {
            this->rebuildOpenMenu(this->ui->actionOpen_with, MainWindow::getOption("openDirectoryWith").toStringList(), false);
        }
        else if (dynamic_cast<ZipComicSource*>(src))
        {
            this->rebuildOpenMenu(this->ui->actionOpen_with, MainWindow::getOption("openZipWith").toStringList(), false);
        }
    }
    else
    {
        this->ui->actionOpen_with->setVisible(false);
    }
}

void MainWindow::rebuildOpenMenu(QAction* action, const QStringList& strList,
                                 bool image)
{
    QString sep = ":";
    auto l = strList.length();
    if (!action->menu()) action->setMenu(new QMenu {});
    action->menu()->clear();
    bool actionAdded = false;
    for (int i = 0; i < l; ++i)
    {
        QString programName;
        if (strList[i].startsWith(sep))
        {
            programName = strList[i].mid(sep.length());
        }
        if (!programName.isEmpty())
        {
            ++i;
            QStringList commandList;
            while (i < l)
            {
                if (!strList[i].startsWith(sep))
                {
                    commandList.append(strList[i]);
                    i++;
                }
                else break;
            }

            auto openImg = new QAction { action->menu() };
            openImg->setText(programName);
            connect(openImg, &QAction::triggered, [this, programName, commandList, image]() mutable
            {
                auto l = commandList.size();
                if (!l)
                    return;
                QString fileName;
                if (image)
                {
                    fileName = this->ui->view->currentPageFilePath();
                }
                else if (auto src = this->ui->view->comicSource())
                {
                    fileName = src->getFilePath();
                }
                for (int i = 0; i < l; i++)
                {
                    commandList[i] = commandList[i].replace("%FILEPATH%", fileName);
                }
                QProcess::startDetached(commandList[0], commandList.mid(1));
            });
            action->menu()->addAction(openImg);
            actionAdded = true;

            programName.clear();
            if (i < l && strList[i].startsWith(sep)) i--;
        }
    }
    action->setVisible(l && actionAdded);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (getOption("confirmQuit").toBool())
    {
        if (QMessageBox::question(this, "Quitting qcomix", "Are you sure you want to quit?") == QMessageBox::Yes)
        {
            event->accept();
        }
        else
        {
            event->ignore();
        }
    }
    else
    {
        event->accept();
    }
}

QString getDefaultIconFileName(const QString& iconName)
{
    if (QFileInfo::exists(":/" + iconName + ".svg"))
    {
        return ":/" + iconName + ".svg";
    }
    if (QFileInfo::exists(":/" + iconName + ".png"))
    {
        return ":/" + iconName + ".png";
    }
    return {};
}

QString MainWindow::getIconFileName(const QString& iconName)
{
    if (auto iconPath = MainWindow::getOption("iconPath").toString(); iconPath == "default")
    {
        return getDefaultIconFileName(iconName);
    }
    else
    {
        if (QFileInfo::exists(iconPath + "/" + iconName + ".svg"))
        {
            return iconPath + "/" + iconName + ".svg";
        }
        if (QFileInfo::exists(iconPath + "/" + iconName + ".png"))
        {
            return iconPath + "/" + iconName + ".png";
        }

        // Fall back to default
        return getDefaultIconFileName(iconName);
    }
    return {};
}

void MainWindow::deleteSelectedBookmarks()
{
    auto sel = this->ui->bookmarksTreeWidget->selectedItems();
    if (sel.size())
    {
        QString question = "Are you sure you want to delete the selected bookmark?";
        if (sel.size() > 1) question = "Are you sure you want to delete the selected bookmarks?";
        if (QMessageBox::question(this, "Delete bookmark" + (sel.size() > 1 ? QString("s") : QString {}), question) == QMessageBox::Yes)
        {
            for (int i = 0; i < sel.size(); i++)
            {
                delete this->ui->bookmarksTreeWidget->takeTopLevelItem(this->ui->bookmarksTreeWidget->indexOfTopLevelItem(sel[i]));
            }
            this->saveBookmarksFromTreeWidget();
        }
    }
}

void MainWindow::on_actionFit_width_mode_triggered()
{
    this->ui->view->setFitMode(PageViewWidget::FitMode::FitWidth);
}

void MainWindow::on_actionNext_page_triggered()
{
    this->ui->view->nextPage();
}

void MainWindow::updateWindowIcon(const QPixmap& page)
{
    setWindowIcon(page.scaled(256, 256, Qt::KeepAspectRatio));
}

void MainWindow::on_actionPrevious_page_triggered()
{
    this->ui->view->previousPage();
}

void MainWindow::on_actionFit_height_mode_triggered()
{
    this->ui->view->setFitMode(PageViewWidget::FitMode::FitHeight);
}

void MainWindow::on_actionFit_original_size_mode_triggered()
{
    this->ui->view->setFitMode(PageViewWidget::FitMode::OriginalSize);
}

void MainWindow::on_actionBest_fit_mode_triggered()
{
    this->ui->view->setFitMode(PageViewWidget::FitMode::FitBest);
}

void MainWindow::updateArchiveMetadata(const ComicMetadata& m)
{
    this->ui->amvTitle->setVisible(m.valid);
    this->ui->amvFileName->setVisible(m.valid);
    this->ui->labelamv1->setVisible(m.valid);
    this->ui->labelamv2->setVisible(m.valid);
    this->ui->labelamv3->setVisible(m.valid);

    if (m.valid)
    {
        this->ui->amvTitle->setText(m.title);
        this->ui->amvFileName->setText(m.fileName);
    }
    else
    {
        this->ui->amvTitle->clear();
        this->ui->amvFileName->clear();
    }
}

void MainWindow::on_actionManual_zoom_mode_triggered()
{
    this->ui->view->setFitMode(PageViewWidget::FitMode::ManualZoom);
}

void MainWindow::on_actionClose_comic_triggered()
{
    this->loadComic(nullptr);
}

void MainWindow::on_actionOpen_directory_triggered()
{
    auto res = QFileDialog::getExistingDirectory(this);
    if (!res.isEmpty()) this->loadComic(createComicSource(res));
}

void MainWindow::on_actionOpen_triggered()
{
    auto res = QFileDialog::getOpenFileName(
                   this, QString {}, QString {}, "Zip archives (*.zip *.cbz);;Any file (*.*)");
    if (!res.isEmpty()) this->loadComic(createComicSource(res));
}

void MainWindow::on_actionReload_triggered()
{
    auto src = this->ui->view->comicSource();
    int currentPage = this->ui->view->currentPage();
    if (src)
    {
        this->loadComic(createComicSource(src->getFilePath()));
        this->ui->view->goToPage(currentPage);
    }
}

void MainWindow::on_actionGo_to_page_triggered()
{
    if (ui->view->comicSource())
    {
        bool ok = false;
        int page = QInputDialog::getInt(this, "Go to page", "Page:", ui->view->currentPage(), 1, ui->view->comicSource()->getPageCount(), 1, &ok);
        if (ok) ui->view->goToPage(page);
    }
}

bool FileSystemFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    auto fileInfo = static_cast<QFileSystemModel*>(this->sourceModel())->fileInfo(static_cast<QFileSystemModel*>(this->sourceModel())->index(sourceRow, 0, sourceParent));
    if (fileInfo.isDir()) return true;
    QMimeDatabase mimeDb;
    if (mimeDb.mimeTypeForFile(fileInfo).inherits("application/zip")) return true;
    return false;
}

void MainWindow::on_actionFirst_page_triggered()
{
    this->ui->view->firstPage();
}

void MainWindow::on_actionLast_page_triggered()
{
    this->ui->view->lastPage();
}

void MainWindow::on_actionNext_comic_triggered()
{
    if (auto comic = this->ui->view->comicSource())
    {
        if (comic->hasNextComic()) this->loadComic(comic->nextComic());
    }
}

void MainWindow::on_actionPrevious_comic_triggered()
{
    if (auto comic = this->ui->view->comicSource())
    {
        if (comic->hasPreviousComic()) this->loadComic(comic->previousComic());
    }
}

void MainWindow::on_actionRotate_clockwise_triggered()
{
    this->ui->view->rotate(90);
}

void MainWindow::on_actionRotate_counterclockwise_triggered()
{
    this->ui->view->rotate(-90);
}

void MainWindow::on_actionFlip_horizontally_triggered(bool arg1)
{
    this->ui->view->flipHorizontally(arg1);
}

void MainWindow::on_actionFlip_vertically_triggered(bool arg1)
{
    this->ui->view->flipVertically(arg1);
}

void MainWindow::on_actionCopy_image_to_clipboard_triggered()
{
    this->ui->view->currentPageToClipboard();
}

void MainWindow::on_actionCopy_image_file_to_clipboard_triggered()
{
    QMimeData* data = new QMimeData;
    auto path = this->ui->view->currentPageFilePath();
    if (!path.isEmpty()) data->setUrls({ QUrl::fromLocalFile(path) });
    QApplication::clipboard()->setMimeData(data);
}

void MainWindow::on_actionRotate_180_degrees_triggered()
{
    this->ui->view->rotate(180);
}

void MainWindow::on_actionSmart_scroll_vertical_first_triggered(bool checked)
{
    this->ui->view->setSmartScrollVerticalFirst(checked);
}

void MainWindow::on_actionManga_mode_triggered(bool checked)
{
    this->ui->view->setMangaMode(checked);
}

void MainWindow::on_actionDouble_page_mode_triggered(bool checked)
{
    this->ui->view->setDoublePageMode(checked);
}

void MainWindow::on_actionZoom_in_triggered()
{
    this->ui->view->zoomIn();
}

void MainWindow::on_actionZoom_out_triggered()
{
    this->ui->view->zoomOut();
}

void MainWindow::on_actionReset_zoom_triggered()
{
    this->ui->view->resetZoom();
}

void MainWindow::on_actionStretch_small_images_triggered(bool checked)
{
    this->ui->view->setStretchSmallImages(checked);
}

void MainWindow::on_actionKeep_transformation_triggered(bool checked)
{
    this->ui->view->setKeepTransformation(checked);
}

void MainWindow::on_actionSet_slideshow_interval_triggered()
{
    if (ui->view->comicSource())
    {
        bool ok = false;
        int seconds = QInputDialog::getInt(this, "Slideshow interval", "Switch page after this many seconds:", ui->view->slideShowInterval(), 1, 900, 1, &ok);
        if (ok) ui->view->setSlideShowSeconds(seconds);
    }
}

void MainWindow::on_actionSlideshow_triggered(bool checked)
{
    this->ui->view->toggleSlideShow(checked);
}

void MainWindow::updateUIState()
{
    bool autoOpenNextComic = MainWindow::getOption("autoOpenNextComic").toBool();
    auto src = this->ui->view->comicSource();
    this->ui->actionNext_comic->setEnabled(src && src->hasNextComic());
    this->ui->actionPrevious_comic->setEnabled(src && src->hasPreviousComic());
    this->ui->actionFirst_page->setEnabled(src && !this->ui->view->onFirstPage());
    this->ui->actionLast_page->setEnabled(src && !this->ui->view->onLastPage());
    this->ui->actionPrevious_page->setEnabled(src && (!this->ui->view->onFirstPage() || (autoOpenNextComic && src->hasPreviousComic())));
    this->ui->actionNext_page->setEnabled(src && (!this->ui->view->onLastPage() || (autoOpenNextComic && src->hasNextComic())));
    this->ui->actionGo_to_page->setEnabled(src && src->getPageCount() > 0);
    this->ui->actionOpen_image_with->setEnabled(src && this->ui->view->currentPage() > 0);
    this->ui->actionDouble_page_mode->setChecked(this->ui->view->isDoublePageMode());
    this->ui->actionManga_mode->setChecked(this->ui->view->isMangaMode());
    this->ui->actionStretch_small_images->setChecked(this->ui->view->stretchesSmallImages());
    this->ui->actionSmart_scroll_vertical_first->setChecked(this->ui->view->smartScrollVerticalFirst());
    this->ui->actionKeep_transformation->setChecked(this->ui->view->transformationKept());
    this->ui->actionSmart_scroll_vertical_first->setEnabled(this->ui->view->smartScrollEnabled());
    this->ui->actionFlip_horizontally->setChecked(this->ui->view->horizontallyFlipped());
    this->ui->actionFlip_vertically->setChecked(this->ui->view->verticallyFlipped());
    this->ui->actionReload->setEnabled(src);
    this->ui->actionClose_comic->setEnabled(src);
    this->ui->actionAdd_bookmark->setEnabled(src && this->ui->view->currentPage() > 0);
    this->ui->actionReset_zoom->setEnabled(src && this->ui->view->currentPage() > 0);
    this->ui->actionZoom_in->setEnabled(src && this->ui->view->currentPage() > 0);
    this->ui->actionZoom_out->setEnabled(src && this->ui->view->currentPage() > 0);
    this->ui->actionFlip_vertically->setEnabled(src && this->ui->view->currentPage() > 0);
    this->ui->actionFlip_horizontally->setEnabled(src && this->ui->view->currentPage() > 0);
    this->ui->actionRotate_clockwise->setEnabled(src && this->ui->view->currentPage() > 0);
    this->ui->actionRotate_counterclockwise->setEnabled(src && this->ui->view->currentPage() > 0);
    this->ui->actionRotate_180_degrees->setEnabled(src && this->ui->view->currentPage() > 0);
    this->ui->actionSmart_scroll->setChecked(this->ui->view->smartScrollEnabled());
    this->ui->actionAllow_free_drag->setChecked(this->ui->view->freeDragAllowed());
    this->ui->actionMagnifying_lens->setChecked(this->ui->view->magnifyingLensEnabled());
}

void MainWindow::on_actionAdd_bookmark_triggered()
{
    int page = this->ui->view->currentPage();
    if (auto src = this->ui->view->comicSource(); src && page > 0)
    {
        auto item = new QTreeWidgetItem;
        item->setText(0, src->getFilePath());
        item->setText(1, QString::number(page));
        item->setText(2, src->getTitle());
        item->setToolTip(0, src->getFilePath());
        item->setToolTip(1, QString::number(page));
        item->setToolTip(2, src->getTitle());
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        this->ui->bookmarksTreeWidget->addTopLevelItem(item);
        this->saveBookmarksFromTreeWidget();
    }
}

void MainWindow::saveBookmarksFromTreeWidget()
{
    auto itemCount = this->ui->bookmarksTreeWidget->topLevelItemCount();
    bool fail = false;
    QJsonArray arr;
    for (int i = 0; i < itemCount; i++)
    {
        auto item = this->ui->bookmarksTreeWidget->topLevelItem(i);
        arr.append(QJsonValue(item->text(0)));
        arr.append(QJsonValue(item->text(1)));
        if (item->text(1).toInt() < 1)
        {
            fail = true;
            QMessageBox::critical(this, "Error",
                                  "Can't save bookmarks: invalid page number!");
            break;
        }
        arr.append(QJsonValue(item->text(2)));
    }
    this->saveBookmarks(arr);
}

void MainWindow::on_actionFit_to_fixed_size_mode_triggered()
{
    this->ui->view->setFitMode(PageViewWidget::FitMode::FixedSize);
}

void MainWindow::on_actionSmart_scroll_triggered(bool checked)
{
    this->ui->view->setSmartScroll(checked);
}

void MainWindow::on_actionAllow_free_drag_triggered(bool checked)
{
    if (checked && MainWindow::getOption("freeDragWarning").toBool())
    {
        QMessageBox::warning(this, "Free drag",
                             "Free drag is not compatible with some advanced "
                             "scrolling features. Proceed at your own risk.");
    }
    this->ui->view->setAllowFreeDrag(checked);
}

void MainWindow::on_actionMagnifying_lens_triggered(bool checked)
{
    this->ui->view->setMagnifyingLensEnabled(checked);
}

void MainWindow::exitCleanup()
{
    for (auto t : thumbnailerThreads)
    {
        t->stopCurrentWork();
    }
    if (MainWindow::getOption("cacheThumbnailsOnDisk").toBool())
    {
        int limit = MainWindow::getOption("diskThumbnailFileCacheLimit").toInt();
        if (limit > 0)
        {
            auto cacheLocation = QStandardPaths::standardLocations(QStandardPaths::CacheLocation).first() + "/qcomix/";
            QDir d(cacheLocation, "qc_thumb_*", QDir::Time, QDir::Files);
            auto files = d.entryList();
            while (files.size() > limit)
            {
                QFile::remove(cacheLocation + "/" + files[0]);
                files.pop_front();
            }
        }
    }
    this->stopThreads();
}

void MainWindow::on_actionHydrus_search_query_triggered()
{
    QString query = QInputDialog::getMultiLineText(this, "Hydrus search query", "Write each tag in a new line");
    QString finalQuery;
    for (const auto& t : query.split("\n"))
    {
        QString clearedTag = t.trimmed();
        if (!t.isEmpty()) finalQuery += "," + t;
    }
    if (finalQuery.startsWith(",")) finalQuery = finalQuery.mid(1);
    if (!finalQuery.isEmpty())
    {
        finalQuery = "hydrus://" + finalQuery;
        this->loadComic(createComicSource(finalQuery));
    }
}

bool BookmarksTreeWidgetEventFilter::eventFilter(QObject* target, QEvent* event)
{
    if (auto tree = dynamic_cast<QTreeWidget*>(target))
    {
        if (event->type() == QEvent::KeyPress)
        {
            auto keyEv = static_cast<QKeyEvent*>(event);
            if (keyEv->key() == Qt::Key_Delete)
            {
                window->deleteSelectedBookmarks();
                return true;
            }
        }
    }
    return false;
}
