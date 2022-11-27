#include "comiccreator.h"
#include <QLabel>

#include <QEventLoop>
#include <QTimer>
#include <QFutureWatcher>
#include <QFuture>
#include <QtConcurrent/QtConcurrentRun>

#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QFileInfo>


ComicCreator* ComicCreator::s_i = nullptr;
QMutex ComicCreator::s_mutex = QMutex();

ComicCreator* ComicCreator::instance() {
    QMutexLocker lock(&s_mutex);
    s_i = new ComicCreator;
    return s_i;
}

ComicCreatorDialog::ComicCreatorDialog(QWidget* parent, const QString& title)
        :QDialog(parent)
{
    this->setWindowTitle("Open Comicbook");
    this->m_progressbar = new QProgressBar(this);
    auto lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel("Loading..."));
    lay->addWidget(new QLabel(title));
    lay->addWidget(m_progressbar);

    auto btns = new QDialogButtonBox(QDialogButtonBox::Cancel);
    lay->addWidget(btns);
    this->setFixedSize(800, 400);
    connect(btns, &QDialogButtonBox::rejected,
            this, &ComicCreatorDialog::abort);
}

void ComicCreatorDialog::setProgress(int val, int total)
{
    this->m_progressbar->setRange(0, total);
    this->m_progressbar->setValue(val);
}

ComicSource* ComicCreator::createComicSource(QWidget *parent, const QString& path) {
    qDebug()<<__PRETTY_FUNCTION__<<__LINE__;
    auto dlg = new ComicCreatorDialog(parent, QFileInfo(path).fileName());
    QTimer taskTimer;  // timer for task.
    QFutureWatcher<ComicSource*> watcher;
    QObject::connect(&watcher, &QFutureWatcher<ComicSource*>::canceled, [&]() {
                         dlg->accept();
                         taskTimer.stop();
                         qDebug() << "comic created canceal, timer stop.";
                     });
    QObject::connect(&watcher, &QFutureWatcher<ComicSource*>::finished, [&]() {
                         dlg->accept();
                         taskTimer.stop();
                         qDebug() << "comic created done, timer stop.";
                     });
    int step = 0;
    taskTimer.setInterval(500);
    QObject::connect(&taskTimer, &QTimer::timeout, [&]() {
                         qDebug() << "timeout, step = " << step;
                         step++;
                     });

    taskTimer.start();

    auto future = QtConcurrent::run(createComicSource_inner, path);
    watcher.setFuture(future);

    QObject::connect(dlg, &ComicCreatorDialog::abort, [&]() {
                         future.cancel();
                         step++;
                         dlg->setProgress(step, 10);
                     });
    dlg->exec();
    if (future.isCanceled()) return nullptr;
    assert(future.isFinished());
    return future.result();
}

ComicSource* createComicSource(QWidget* parent, const QString& path) {
    return ComicCreator::instance()->createComicSource(parent, path);
}

