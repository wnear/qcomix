#pragma once

#include "comicsource.h"
#include <QDialog>
#include <QProgressBar>
#include <QMutex>


class ComicCreatorDialog : public QDialog
{
    Q_OBJECT
public:
    ComicCreatorDialog(QWidget* parent, const QString &title);
    void setProgress(int val, int total = -1);
    ~ComicCreatorDialog() {}
signals:
    void abort();
private:
    QProgressBar *m_progressbar;
};

class ComicCreator
{
public:
    ComicSource* createComicSource(QWidget *parent, const QString& path);
    static ComicCreator* instance();

  private:
    ComicCreator(){}
    ~ComicCreator(){}
    static ComicCreator *s_i;
    static QMutex s_mutex;
    ComicSource* m_result;
};

ComicSource* createComicSource(QWidget* parent, const QString& path);
