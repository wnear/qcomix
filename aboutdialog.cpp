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

#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "version.h"

AboutDialog::AboutDialog(QWidget* parent) :
    QDialog(parent), ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    this->ui->aboutTabWidget->setCurrentIndex(0);
    QFile license(":/LICENSE.txt");
    if(license.open(QFile::ReadOnly)) this->ui->licenseText->setPlainText(license.readAll());
    this->ui->versionLabel->setText("version " + QString{QCOMIX_VERSION});
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::on_closeAboutDialogButton_clicked()
{
    this->close();
}
