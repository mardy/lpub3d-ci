/****************************************************************************
**
** Copyright (C) 2018 Trevor SANDY. All rights reserved.
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

#ifndef LGRAPHICSVIEW_H
#define LGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QGridLayout>

#include "qgraphicsscene.h"
#include "lgraphicsscene.h"
#include "name.h"

enum FitMode { FitNone, FitWidth, FitVisible, FitTwoPages, FitContinuousScroll };

enum zValues {
  PageBackgroundZValue = 0,
  PageNumberZValue = 10,
  PagePLIZValue = 20,
  PageInstanceZValue = 30,
  AssemZValue = 30,
  StepGroupZValue = 30,
  CalloutPointerZValue = 45,
  CalloutBackgroundZValue = 50,
  CalloutAssemZValue = 55,
  CalloutInstanceZValue = 60,
};

class PageBackgroundItem;

class LGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
  LGraphicsView(LGraphicsScene *scene);
  PageBackgroundItem *pageBackgroundItem;   // page background used for sizing
  FitMode             fitMode;              // how to fit the scene into the view

  void fitVisible(const QRectF rect);
  void fitWidth(const QRectF rect);
  void actualSize();
  void zoomIn();
  void zoomOut();

public slots:
  void setPageGuides(Theme t = ThemeDefault);
  void setPageRuler(Theme t = ThemeDefault);
  void setSceneBackground(Theme t = ThemeDefault);

signals:
    void setPageRulerSig(bool);
    void setPageGuidesSig(bool);
    void setGuidePenSig(Theme t);
    void setSceneThemeSig(Theme t);

protected:
    // toggle ruler
    void removeGridItem(QGridLayout *layout, int row, int column, bool deleteWidgets = false);
    void deleteGridWidgets(QLayoutItem *item);

    // drag and drop
    virtual void dragMoveEvent(QDragMoveEvent *event);
    virtual void dragEnterEvent(QDragEnterEvent *event);
    virtual void dragLeaveEvent(QDragLeaveEvent *event);
    virtual void dropEvent(QDropEvent *event);

    virtual void resizeEvent(QResizeEvent *event);

private:
  QRectF mPageRect;
  bool mGridLayoutSet;
  QGridLayout* mGridLayout;
};

// View Ruler Class
#include <QGridLayout>
#define RULER_BREADTH 20

class LRuler : public QWidget
{
Q_OBJECT
Q_ENUMS(RulerType)
Q_PROPERTY(qreal origin READ origin WRITE setOrigin)
Q_PROPERTY(qreal rulerUnit READ rulerUnit WRITE setRulerUnit)
Q_PROPERTY(qreal rulerZoom READ rulerZoom WRITE setRulerZoom)

public:
  enum RulerType { Horizontal, Vertical };
LRuler(LRuler::RulerType rulerType, Theme t, QWidget* parent)
: QWidget(parent),
  mRulerType(rulerType),
  mOrigin(0.),
  mRulerUnit(1.),
  mRulerZoom(1.),
  mMouseTracking(false),
  mDrawText(false)
{
  setMouseTracking(true);
  QFont txtFont("Helvetica",7,0);
  txtFont.setHintingPreference(QFont::PreferDefaultHinting);
  txtFont.setStyleHint(QFont::SansSerif,QFont::PreferOutline);
  txtFont.setStyleStrategy(QFont::PreferAntialias);
  setFont(txtFont);
  setRulerColor(t);
  setRulerNMLPen(t);
  setRulerTickPen(t);
}

QSize minimumSizeHint() const
{
  return QSize(RULER_BREADTH,RULER_BREADTH);
}

LRuler::RulerType rulerType() const
{
  return mRulerType;
}

qreal origin() const
{
  return mOrigin;
}

qreal rulerUnit() const
{
  return mRulerUnit;
}

qreal rulerZoom() const
{
  return mRulerZoom;
}

public slots:
  void setOrigin(const qreal origin);
  void setRulerUnit(const qreal rulerUnit);
  void setRulerZoom(const qreal rulerZoom);
  void setCursorPos(const QPoint cursorPos);
  void setMouseTrack(const bool track);
  void setRulerTickPen(Theme = ThemeDefault);
  void setRulerNMLPen(Theme = ThemeDefault);
  void setRulerColor(Theme = ThemeDefault);

protected:
  void mouseMoveEvent(QMouseEvent* event);
  void paintEvent(QPaintEvent* event);

private:
  void drawAScaleMeter(QPainter* painter, QRectF rulerRect, qreal scaleMeter, qreal startPositoin);
  void drawFromOriginTo(QPainter* painter, QRectF rulerRect, qreal startMark, qreal endMark, int startTickNo, qreal step, qreal startPosition);
  void drawMousePosTick(QPainter* painter);

  RulerType mRulerType;
  qreal mOrigin;
  qreal mRulerUnit;
  qreal mRulerZoom;
  QPoint mCursorPos;
  bool mMouseTracking;
  bool mDrawText;
  QPen mRulerTickPen;
  QPen mRulerNMLPen;
  QColor mRulerColor;
};

#endif // LGRAPHICSVIEW_H
