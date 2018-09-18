/****************************************************************************
**
** Copyright (C) 2015 - 2018 Trevor SANDY. All rights reserved.
**
** This file may be used under the terms of the
** GNU General Public Liceense (GPL) version 3.0
** which accompanies this distribution, and is
** available at http://www.gnu.org/licenses/gpl.html
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#ifndef DIALOGEXPORTPAGES_H
#define DIALOGEXPORTPAGES_H

#include <QDialog>

namespace Ui {
  class DialogExportPages;
}

class DialogExportPages : public QDialog
{
  Q_OBJECT

public:
  explicit DialogExportPages(QWidget *parent = 0);
  ~DialogExportPages();

  bool allPages();
  bool currentPage();
  bool pageRange();
  bool resetCache();
  bool ignoreMixedPageSizesMsg();
  bool doNotShowPageProcessDlg();
  int pageDisplayPause();
  QString const pageRangeText();

private slots:
  void on_lineEditPageRange_textChanged(const QString &arg1);

  void on_lineEditPageRange_selectionChanged();

private:
  Ui::DialogExportPages *ui;
  QString linePageRange;
};

#endif // DIALOGEXPORTPAGES_H
