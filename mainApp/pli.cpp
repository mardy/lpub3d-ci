
/****************************************************************************
**
** Copyright (C) 2007-2009 Kevin Clague. All rights reserved.
** Copyright (C) 2015 - 2018 Trevor SANDY. All rights reserved.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

/****************************************************************************
 *
 * This class implements part list images.  It gathers and maintains a list
 * of part/colors that need to be displayed.  It provides mechanisms for
 * rendering the parts.  It provides methods for organizing the parts into
 * a reasonable looking box for display in your building instructions.
 *
 * Please see lpub.h for an overall description of how the files in LPub
 * make up the LPub program.
 *
 ***************************************************************************/
#include <QMenu>
#include <QGraphicsSceneContextMenuEvent>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>

#include "pli.h"
#include "step.h"
#include "ranges.h"
#include "callout.h"
#include "resolution.h"
#include "render.h"
#include "paths.h"
#include "ldrawfiles.h"
#include "placementdialog.h"
#include "metaitem.h"
#include "color.h"
#include "lpub.h"
#include "commonmenus.h"
#include "lpub_preferences.h"
#include "ranges_element.h"
#include "range_element.h"

#include "lc_category.h"
#include "lc_library.h"
#include "pieceinf.h"

QCache<QString,QString> Pli::orientation;

QString PartTypeNames[NUM_PART_TYPES] = { "Fade Previous Steps", "Highlight Current Step", "Normal" };

const Where &Pli::topOfStep()
{
  if (bom || step == nullptr || steps == nullptr) {
      return gui->topOfPage();
    } else {
      if (step) {
          return step->topOfStep();
        } else {
          return steps->topOfSteps();
        }
    }
}
const Where &Pli::bottomOfStep()
{
  if (bom || step == nullptr || steps == nullptr) {
      return gui->bottomOfPage();
    } else {
      if (step) {
          return step->bottomOfStep();
        } else {
          return steps->bottomOfSteps();
        }
    }
}
const Where &Pli::topOfSteps()
{
  return steps->topOfSteps();
}
const Where &Pli::bottomOfSteps()
{
  return steps->bottomOfSteps();
}
const Where &Pli::topOfCallout()
{
  return step->callout()->topOfCallout();
}
const Where &Pli::bottomOfCallout()
{
  return step->callout()->bottomOfCallout();
}

Pli::Pli(bool _bom) : bom(_bom)
{
  relativeType = PartsListType;
  initAnnotationString();
  steps = nullptr;
  step = nullptr;
  meta = nullptr;
  widestPart = 1;
  tallestPart = 1;
  background = nullptr;
  splitBom = false;

  ptn.append( { FADE_PART, "-fade" } );
  ptn.append( { HIGHLIGHT_PART, "-highlight" } );
  ptn.append( { NORMAL_PART, QString() } );

  isSubModel = false;
}

/****************************************************************************
 * Part List Images routines
 ***************************************************************************/

PliPart::~PliPart()
{
  instances.clear();
  leftEdge.clear();
  rightEdge.clear();
}


float PliPart::maxMargin()
{
  float margin1 = qMax(instanceMeta.margin.valuePixels(XX),
                       csiMargin.valuePixels(XX));

  // Use BOM Element margin
  if (styleMeta.style.value() != AnnotationStyle::none){
      float margin2 = styleMeta.margin.valuePixels(XX);
      margin1 = qMax(margin1,margin2);
  }

  if (annotWidth) {
      float margin2 = styleMeta.margin.valuePixels(XX);
      margin1 = qMax(margin1,margin2);
    }

  return margin1;
}

int Pli::pageSizeP(Meta *meta, int which){
  int _size;

  // flip orientation for landscape
  if (meta->LPub.page.orientation.value() == Landscape){
      which == 0 ? _size = 1 : _size = 0;
    } else {
      _size = which;
    }
  return meta->LPub.page.size.valuePixels(_size);
}

QString Pli::partLine(QString &line, Where &here, Meta & /*meta*/)
{
  return line + QString(";%1;%2").arg(here.modelName).arg(here.lineNumber);
}

void Pli::setParts(
    QStringList &pliParts,
    Meta        &meta,
    bool         _bom,
    bool         _split)
{
  bom      = _bom;
  splitBom = _split;
  pliMeta  = _bom ? meta.LPub.bom : meta.LPub.pli;

  bool styleExtended = pliMeta.annotation.extendedStyle.value();

  for (int i = 0; i < pliParts.size(); i++) {
      QString part = pliParts[i];
      QStringList sections = part.split(";");
      QString line = sections[0];
      Where here(sections[1],sections[2].toInt());

      QStringList tokens;

      split(line,tokens);

      if (tokens.size() == 15 && tokens[0] == "1") {
          QString &color = tokens[1];
          QString &type = tokens[14];

          QFileInfo info(type);

          QString key = info.baseName() + "_" + color;

          QString sortCategory;
          partClass(type,sortCategory);  // populate sort category using part class and

          float modelScale = 1.0f;
          if (Preferences::usingNativeRenderer) {
              modelScale = pliMeta.cameraDistNative.factor.value();
          } else {
              modelScale = pliMeta.modelScale.value();
          }

          // set default annotation style settings
          AnnotationStyleMeta styleMeta;
          styleMeta.margin = pliMeta.annotate.margin;
          styleMeta.font   = pliMeta.annotate.font;
          styleMeta.color  = pliMeta.annotate.color;

          // get part annotation style flag - either cirle(1) or square(2)
          AnnotationStyle style = (AnnotationStyle)Annotations::getAnnotationStyle(type);

          // set style meta settings
          if (style) {
              // get style category
              bool styleCategory = false;
              AnnotationCategory annotationCategory = (AnnotationCategory)Annotations::getAnnotationCategory(type);
              switch (annotationCategory)
              {
              case AnnotationCategory::axle:
                  styleCategory = pliMeta.annotation.axleStyle.value();
                  break;
              case AnnotationCategory::beam:
                  styleCategory = pliMeta.annotation.beamStyle.value();
                  break;
              case AnnotationCategory::cable:
                  styleCategory = pliMeta.annotation.cableStyle.value();
                  break;
              case AnnotationCategory::connector:
                  styleCategory = pliMeta.annotation.connectorStyle.value();
                  break;
              case AnnotationCategory::hose:
                  styleCategory = pliMeta.annotation.hoseStyle.value();
                  break;
              case AnnotationCategory::panel:
                  styleCategory = pliMeta.annotation.panelStyle.value();
                  break;
              default:
                  break;
              }
              // set style if category enabled
              if (styleCategory) {
                  if (style == AnnotationStyle::circle)
                      styleMeta       = pliMeta.circleStyle;
                  else
                  if (style == AnnotationStyle::square)
                     styleMeta        = pliMeta.squareStyle;
              }
          }
          else
          if (styleExtended) {
              styleMeta = pliMeta.rectangleStyle;
          }

          // assemble image name key
          QString nameKey = QString("%1_%2_%3_%4_%5_%6_%7")
              .arg(key)
              .arg(gui->pageSize(meta.LPub.page, 0))
              .arg(resolution())
              .arg(resolutionType() == DPI ? "DPI" : "DPCM")
              .arg(modelScale)
              .arg(pliMeta.cameraAngles.value(0))
              .arg(pliMeta.cameraAngles.value(1));
          // assemble image name
          QString imageName = QDir::currentPath() + "/" +
              Paths::partsDir + "/" + nameKey + ".png";

          if (bom && splitBom){
              if ( ! tempParts.contains(key)) {
                  PliPart *part = new PliPart(type,color);
                  part->styleMeta       = styleMeta;
                  part->instanceMeta    = pliMeta.instance;
                  part->csiMargin       = pliMeta.part.margin;
                  part->sortColour      = QString("%1").arg(color,5,'0');
                  part->sortCategory    = QString("%1").arg(sortCategory,80,' ');
                  part->nameKey         = nameKey;
                  part->imageName       = imageName;
                  tempParts.insert(key,part);
                }
              tempParts[key]->instances.append(here);
            } else {
              if ( ! parts.contains(key)) {
                  PliPart *part = new PliPart(type,color);
                  part->styleMeta       = styleMeta;
                  part->instanceMeta    = pliMeta.instance;
                  part->csiMargin       = pliMeta.part.margin;
                  part->sortColour      = QString("%1").arg(color,5,'0');
                  part->sortCategory    = QString("%1").arg(sortCategory,80,' ');
                  part->nameKey         = nameKey;
                  part->imageName       = imageName;
                  parts.insert(key,part);
                }
              parts[key]->instances.append(here);
            }
        }
    } //instances

  // now sort then divide the list based on BOM occurrence
  //TODO - Add BOM Element to sort
  if (bom && splitBom){

      //sort
      sortedKeys = tempParts.keys();

      if (pliMeta.sortBy.value() == SortOptionName[PartColour]){
          // Sort tempParts by color
          for (int i = 0; i < tempParts.size() - 1; i++) {
              for (int j = i+1; j < tempParts.size(); j++) {
                  if (tempParts[sortedKeys[i]]->sortColour < tempParts[sortedKeys[j]]->sortColour) {
                      QString t = sortedKeys[i];
                      sortedKeys[i] = sortedKeys[j];
                      sortedKeys[j] = t;
                    }
                }
            }
        } else if (pliMeta.sortBy.value() == SortOptionName[PartCategory]){
          // Sort tempParts by category
          for (int i = 0; i < tempParts.size() - 1; i++) {
              for (int j = i+1; j < tempParts.size(); j++) {
                  if (tempParts[sortedKeys[i]]->sortCategory < tempParts[sortedKeys[j]]->sortCategory) {
                      QString t = sortedKeys[i];
                      sortedKeys[i] = sortedKeys[j];
                      sortedKeys[j] = t;
                    }
                }
            }
        }

      int quotient    = tempParts.size() / gui->boms;
      int remainder   = tempParts.size() % gui->boms;
      int maxParts    = 0;
      int startIndex  = 0;
      int partIndex   = 0;   // using 0-based index

      if (gui->bomOccurrence == gui->boms){
          maxParts = gui->bomOccurrence * quotient + remainder;
          startIndex = maxParts - quotient - remainder;
        } else {
          maxParts = gui->bomOccurrence * quotient;
          startIndex = maxParts - quotient;
        }

      QString key;
      foreach(key,sortedKeys){
          PliPart *part;
          part = tempParts[key];

          if (partIndex >= startIndex && partIndex < maxParts) {
              parts.insert(key,part);
            }
          partIndex++;
        }
      tempParts.clear();
      sortedKeys.clear();
    }
}

void Pli::clear()
{
  parts.clear();
}

QHash<int, QString>     annotationString;
QList<QString>          titleAnnotations;

bool Pli::initAnnotationString()
{
  if (annotationString.empty()) {
      annotationString[1] = "B";  // blue
      annotationString[2] = "G";  // green
      annotationString[3] = "DC"; // dark cyan
      annotationString[4] = "R";  // red
      annotationString[5] = "M";  // magenta
      annotationString[6] = "Br"; // brown
      annotationString[9] = "LB"; // light blue
      annotationString[10]= "LG"; // light green
      annotationString[11]= "C";  // cyan
      annotationString[12]= "LR"; // cyan
      annotationString[13]= "P";  // pink
      annotationString[14]= "Y";  // yellow
      annotationString[22]= "Ppl";// purple
      annotationString[25]= "O";  // orange

      annotationString[32+1] = "TB";  // blue
      annotationString[32+2] = "TG";  // green
      annotationString[32+3] = "TDC"; // dark cyan
      annotationString[32+4] = "TR";  // red
      annotationString[32+5] = "TM";  // magenta
      annotationString[32+6] = "TBr"; // brown
      annotationString[32+9] = "TLB"; // light blue
      annotationString[32+10]= "TLG"; // light green
      annotationString[32+11]= "TC";  // cyan
      annotationString[32+12]= "TLR"; // cyan
      annotationString[32+13]= "TP";  // pink
      annotationString[32+14]= "TY";  // yellow
      annotationString[32+22]= "TPpl";// purple
      annotationString[32+25]= "TO";  // orange

      titleAnnotations = Annotations::getTitleAnnotations();
    }
  return true;
}

void Pli::getAnnotation(
    QString &type,
    QString &annotateStr)
{
  annotateStr.clear();

  bool enable = pliMeta.annotation.display.value();
  if (! enable)
    return;

  bool title = pliMeta.annotation.titleAnnotation.value();
  bool freeform = pliMeta.annotation.freeformAnnotation.value();
  bool titleAndFreeform = pliMeta.annotation.titleAndFreeformAnnotation.value();

  // pick up annotations
  annotateStr = titleDescription(type);

  if(title || titleAndFreeform){
      if (titleAnnotations.size() == 0 && !titleAndFreeform) {
          qDebug() << "Annotations enabled but no annotation source found.";
          return;
        }
      if (titleAnnotations.size() > 0) {
          QString annotation,sClean;
          for (int i = 0; i < titleAnnotations.size(); i++) {
              annotation = titleAnnotations[i];
              QRegExp rx(annotation);
              if (annotateStr.contains(rx)) {
                  sClean = rx.cap(1);
                  sClean.remove(QRegExp("\\s"));            //remove spaces
                  annotateStr = sClean;
                  return;
                }
            }
        }
      if (titleAndFreeform) {
          annotateStr = Annotations::freeformAnnotation(type.toLower());
          return;
        }
    } else if (freeform) {
      annotateStr = Annotations::freeformAnnotation(type.toLower());
      return;
    }
  annotateStr.clear();
  return;
}

QString Pli::orient(QString &color, QString type)
{
  type = type.toLower();

  float a = 1, b = 0, c = 0;
  float d = 0, e = 1, f = 0;
  float g = 0, h = 0, i = 1;

  QString *cached = orientation[type];

  if ( ! cached) {
      QString name(Preferences::pliControlFile);
      QFile file(name);

      if (file.open(QFile::ReadOnly | QFile::Text)) {
          QTextStream in(&file);

          while ( ! in.atEnd()) {
              QString line = in.readLine(0);
              QStringList tokens;

              split(line,tokens);

              if (tokens.size() != 15) {
                  continue;
                }

              QString token14 = tokens[14].toLower();

              if (tokens.size() == 15 && tokens[0] == "1" && token14 == type) {
                  cached = new QString(line);
                  orientation.insert(type,cached);
                  break;
                }
            }
          file.close();
        }
    }

  if (cached) {
      QStringList tokens;

      split(*cached, tokens);

      if (tokens.size() == 15 && tokens[0] == "1") {
          a = tokens[5].toFloat();
          b = tokens[6].toFloat();
          c = tokens[7].toFloat();
          d = tokens[8].toFloat();
          e = tokens[9].toFloat();
          f = tokens[10].toFloat();
          g = tokens[11].toFloat();
          h = tokens[12].toFloat();
          i = tokens[13].toFloat();
        }
    }

  return QString ("1 %1 0 0 0 %2 %3 %4 %5 %6 %7 %8 %9 %10 %11")
      .arg(color)
      .arg(a) .arg(b) .arg(c)
      .arg(d) .arg(e) .arg(f)
      .arg(g) .arg(h) .arg(i)
      .arg(type);
}

int Pli::createSubModelIcons()
{
    int rc = 0;
    QString key   = "";
    QString color = "0"; // static submodel color code

    Meta         _meta;
    meta       = &_meta;
    bom        = false;
    splitBom   = false;
    isSubModel = true;
    pliMeta    = meta->LPub.pli;

    if (renderer->useLDViewSCall()) {

        for (int i = 0; i < gui->fileList().size(); i++) {

            QString type = gui->fileList()[i];

            key = QString("%1_%2").arg(QFileInfo(type).baseName()).arg(color);

            float modelScale = 1.0f;
            if (Preferences::usingNativeRenderer) {
                modelScale = pliMeta.cameraDistNative.factor.value();
            } else {
                modelScale = pliMeta.modelScale.value();
            }

            // assemble icon name key
            QString nameKey = QString("%1_%2_%3_%4_%5_%6_%7")
                    .arg(key)
                    .arg(gui->pageSize(meta->LPub.page, 0))
                    .arg(resolution())
                    .arg(resolutionType() == DPI ? "DPI" : "DPCM")
                    .arg(modelScale)
                    .arg(pliMeta.cameraAngles.value(0))
                    .arg(pliMeta.cameraAngles.value(1));

            if ( ! parts.contains(key)) {
                AnnotationStyleMeta styleMeta;
                styleMeta.margin = pliMeta.annotate.margin;
                styleMeta.font   = pliMeta.annotate.font;
                styleMeta.color  = pliMeta.annotate.color;

                PliPart *part = new PliPart(type,color);
                part->styleMeta    = styleMeta;
                part->instanceMeta = pliMeta.instance;
                part->csiMargin    = pliMeta.part.margin;
                part->sortColour   = QString("%1").arg(color,5,'0');
                part->sortCategory = QString();
                part->nameKey      = nameKey;
                part->imageName    = QString();
                parts.insert(key,part);
            }
        }
        rc = partSizeLDViewSCall();

    } else {

        if (Preferences::modeGUI && ! gui->exporting()) {
            emit gui->progressPermResetSig();
            emit gui->progressPermMessageSig("Rendering submodel icons...");
            emit gui->progressPermRangeSig(1, gui->fileList().size());
          }

        for (int i = 0; i < gui->fileList().size(); i++) {

            if (Preferences::modeGUI && ! gui->exporting())
                emit gui->progressPermSetValueSig(i);

            QString type = gui->fileList()[i];
            key = QString("%1_%2").arg(QFileInfo(type).baseName()).arg(color);

            if ((rc = createPartImage(key,type,color,nullptr)) != 0) {
                emit gui->messageSig(LOG_ERROR, QString("Failed to create submodel icon for key %1").arg(key));
            }
        }
        if (Preferences::modeGUI && ! gui->exporting()) {
          emit gui->progressPermSetValueSig(gui->fileList().size());
        }

    }

    return rc;
}

int Pli::createPartImage(
    QString  &partialKey,
    QString  &type,
    QString  &color,
    QPixmap  *pixmap)
{

    fadeSteps = Preferences::enableFadeSteps ;
    displayIcons = gApplication->mPreferences.mViewPieceIcons;
    fadeColour = LDrawColor::ldColorCode(Preferences::validFadeStepsColour);
    highlightStep = Preferences::enableHighlightStep && !gui->suppressColourMeta();
    bool fadePartOK = fadeSteps && !highlightStep && displayIcons;
    bool highlightPartOK = highlightStep && !fadeSteps && displayIcons;

    bool isColorPart = gui->ldrawColourParts.isLDrawColourPart(type);

    for (int pT = 0; pT < ptn.size(); pT++ ) {

#ifdef QT_DEBUG_MODE
        QString CurrentPartType = PartTypeNames[pT];
#endif
        if (((pT == FADE_PART) && !fadePartOK) || ((pT == HIGHLIGHT_PART) && !highlightPartOK))
            continue;

        QElapsedTimer timer;
        timer.start();
        bool showElapsedTime = false;

        // moved from enum to save weight
        ia.baseName[pT] = QFileInfo(type).baseName();
        ia.partColor[pT] = (pT == FADE_PART && fadeSteps && Preferences::fadeStepsUseColour) ? fadeColour : color;

        emit gui->messageSig(LOG_STATUS, QString("Render PLI image for [%1] parts...").arg(PartTypeNames[pT]));

        float modelScale = 1.0f;
        if (Preferences::usingNativeRenderer) {
            modelScale = pliMeta.cameraDistNative.factor.value();
        } else {
            modelScale = pliMeta.modelScale.value();
        }

        QString key = QString("%1_%2_%3_%4_%5_%6_%7_%8%9")
                .arg(partialKey)
                .arg(pageSizeP(meta, 0))
                .arg(resolution())
                .arg(resolutionType() == DPI ? "DPI" : "DPCM")
                .arg(modelScale)
                .arg(pliMeta.cameraFoV.value())
                .arg(pliMeta.cameraAngles.value(0))
                .arg(pliMeta.cameraAngles.value(1))
                .arg(ptn[pT].typeName);

        QString imageDir = isSubModel ? Paths::submodelDir : Paths::partsDir;
        imageName = QDir::currentPath() + "/" + imageDir + "/" + key + ".png";
        ldrNames  = (QStringList() << QDir::currentPath() + "/" + Paths::tmpDir + "/pli.ldr");

        QFile part(imageName);

        if ( ! part.exists()) {

            showElapsedTime = true;

            // create a temporary DAT to feed the renderer
            part.setFileName(ldrNames.first());

            if ( ! part.open(QIODevice::WriteOnly)) {
                emit gui->messageSig(LOG_ERROR,QMessageBox::tr("Cannot open file for writing %1:\n%2.")
                                     .arg(ldrNames.first())
                                     .arg(part.errorString()));
                return -1;
            }

            // define ldr file name
            QFileInfo typeInfo = QFileInfo(type);
            QString iconType = typeInfo.fileName();
            if (pT != NORMAL_PART && (isSubModel || isColorPart))
                iconType = typeInfo.baseName() + ptn[pT].typeName + "." + typeInfo.suffix();

            // generate PLI Part file
            QStringList pliFile;
            pliFile = configurePLIPart(
                      ia.partColor[pT],
                      iconType,
                      (PartType)pT,
                      fadeSteps,
                      highlightStep);

            QTextStream out(&part);
            if (Preferences::usingNativeRenderer) {
                QString modelName = QFileInfo(type).baseName();
                modelName = modelName.replace(modelName.at(0),modelName.at(0).toUpper());
                out << QString("0 %1").arg(modelName) << endl;
                out << QString("0 Name: %1").arg(type) << endl;
                out << QString("0 !LEOCAD MODEL NAME %1").arg(modelName) << endl;
                out << renderer->getRotstepMeta(meta->rotStep) << endl;
            }
            foreach (QString line, pliFile)
                out << line << endl;
            part.close();

            // feed DAT to renderer
            PliType pliType = isSubModel ? SUBMODEL: bom ? BOM : PART;
            int rc = renderer->renderPli(ldrNames,imageName,*meta,pliType);

            if (rc != 0) {
                emit gui->messageSig(LOG_ERROR,QMessageBox::tr("Render failed for %1").arg(imageName));
                return -1;
             }
        }

        // create icon path key - using actual color code
        QString colourCode, imageKey;
        if (pT != NORMAL_PART) {
            colourCode = QString("%1").arg(pT == FADE_PART ?
                                           QString("%1%2").arg(LPUB3D_COLOUR_FADE_PREFIX).arg(Preferences::fadeStepsUseColour ? fadeColour : ia.partColor[pT]) :
                                           QString("%1%2").arg(LPUB3D_COLOUR_HIGHLIGHT_PREFIX ).arg(ia.partColor[pT]));
            if (isSubModel || isColorPart) {
                imageKey = QString("%1%2_%3").arg(ia.baseName[pT]).arg(ptn[pT].typeName).arg(colourCode);
            } else {
                imageKey = QString("%1_%2").arg(ia.baseName[pT]).arg(colourCode);
            }
        } else {
            colourCode = ia.partColor[pT];
            imageKey = QString("%1_%2").arg(ia.baseName[pT]).arg(colourCode);
        }
        imageKey = imageKey;

        emit gui->setPliIconPathSig(imageKey,imageName);

        if (pixmap && (pT == NORMAL_PART))
            pixmap->load(imageName);

        if (showElapsedTime)
            emit gui->messageSig(LOG_INFO, QString("%1 PLI [%2] render took %3 milliseconds "
                                                   "to render image [%4].")
                                                   .arg(Render::getRenderer())
                                                   .arg(PartTypeNames[pT])
                                                   .arg(timer.elapsed())
                                                   .arg(imageName));
    }

  return 0;
}

// LDView performance improvement
int Pli::createPartImagesLDViewSCall(QStringList &ldrNames, bool isNormalPart) {

    emit gui->messageSig(LOG_STATUS, "Render PLI images using LDView Single Call...");

    if (! ldrNames.isEmpty()) {

        // feed DAT to renderer
        PliType pliType = isSubModel ? SUBMODEL: bom ? BOM : PART;
        int rc = renderer->renderPli(ldrNames,QString(),*meta,pliType);
        if (rc != 0) {
            emit gui->messageSig(LOG_ERROR,QMessageBox::tr("Render failed for Pli images."));
            return -1;
        }

    }

    if (isNormalPart) {

        QString key;
        // 3. populate parts with image pixmap and size
        foreach(key,parts.keys()) {

            PliPart *part;
            // get part info
            part = parts[key];
            // load image files into pixmap
            // instantiate pixmps //ERROR
            QPixmap *pixmap = new QPixmap();
            if (pixmap == nullptr) {
                return -1;
            }

            if (! pixmap->load(part->imageName)) {
                emit gui->messageSig(LOG_ERROR,QMessageBox::tr("Cannot load PLI pixmap image %1 is not a file.")
                                     .arg(part->imageName));
                return -1;
            }

            // transfer image info to part
            QImage image = pixmap->toImage();

            part->pixmap = new PGraphicsPixmapItem(this,part,*pixmap,parentRelativeType,part->type, part->color);

            delete pixmap;

            // size the PLI
            part->pixmapWidth  = image.width();
            part->pixmapHeight = image.height();

            part->width  = image.width();

            /* Add instance count area */

            QString descr;

            descr = QString("%1x") .arg(part->instances.size(),0,10);

            QString font = pliMeta.instance.font.valueFoo();
            QString color = pliMeta.instance.color.value();

            part->instanceText =
                    new InstanceTextItem(this,part,descr,font,color,parentRelativeType);

            int textWidth, textHeight;

            part->instanceText->size(textWidth,textHeight);

            part->textHeight = textHeight;

            // if text width greater than image width
            // the bounding box is wider

            if (textWidth > part->width) {
                part->width = textWidth;
            }

            /* Add annotation area */

            if (part->styleMeta.style.value() == AnnotationStyle::circle ||
                part->styleMeta.style.value() == AnnotationStyle::square)
                descr = Annotations::getStyleAnnotation(part->type);
            else
                getAnnotation(part->type,descr);

            if (descr.size()) {

                font   = part->styleMeta.font.valueFoo();
                color  = part->styleMeta.color.value();

                part->annotateText =
                        new AnnotateTextItem(this,part,descr,font,color,parentRelativeType);

                part->annotateText->size(part->annotWidth,part->annotHeight);

                if (part->annotWidth > part->width) {
                    part->width = part->annotWidth;
                }

                part->partTopMargin = part->styleMeta.margin.valuePixels(YY);

                int hMax = int(part->annotHeight + part->partTopMargin);
                for (int h = 0; h < hMax; h++) {
                    part->leftEdge  << part->width - part->annotWidth;
                    part->rightEdge << part->width;
                }
            } else {
                part->annotateText = nullptr;
                part->annotWidth  = 0;
                part->annotHeight = 0;
                part->partTopMargin = 0;
            }

            part->topMargin = part->csiMargin.valuePixels(YY);
            getLeftEdge(image,part->leftEdge);
            getRightEdge(image,part->rightEdge);

            bool overlapped = false;

            int overlap = 0;
            int hMax = 0;
            int elementWidth = 0, elementHeight = 0;

            if (bom && pliMeta.partElements.display.value()) {

                /* Add BOM Elements area */

                descr.clear();
                QString _colorid = part->color;
                QString _typeid  = QFileInfo(part->type).baseName();

                int which = 0;   // 0 = BL, 1 = LEGO
                if ( pliMeta.partElements.legoElements.value())
                    which = 1;

                if (pliMeta.partElements.localLegoElements.value()) {
                    QString elementKey = QString("%1%2").arg(_typeid).arg(_colorid);
                    descr = Annotations::getLEGOElement(elementKey.toLower());
                }
                else
                {
                    if (!Annotations::loadBLElements()){
                        QString URL(VER_LPUB3D_BLELEMENTS_DOWNLOAD_URL);
                        gui->downloadFile(URL, "BrickLink Elements");
                        QByteArray Buffer = gui->getDownloadedFile();
                        Annotations::loadBLElements(Buffer);
                    }
                    descr = Annotations::getBLElement(_colorid,_typeid,which);
                }

                if (descr.size()) {

                    if (pliMeta.annotation.elementStyle.value()){
                        font   = pliMeta.rectangleStyle.font.valueFoo();
                        color  = pliMeta.rectangleStyle.color.value();
                        part->elementTopMargin = pliMeta.rectangleStyle.margin.valuePixels(YY);
                    } else {
                        font   = pliMeta.annotate.font.valueFoo();
                        color  = pliMeta.annotate.color.value();
                        part->elementTopMargin = pliMeta.annotate.margin.valuePixels(YY);
                    }

                    part->annotateElement =
                            new AnnotateTextItem(this,part,descr,font,color,parentRelativeType,true);

                    part->annotateElement->size(elementWidth,elementHeight);

                    part->elementHeight = elementHeight;

                    if (elementWidth > part->width) {
                        part->width = elementWidth;
                    }

                    /*
                     * Lets see if we can slide the BOM Element up in the bottom left corner of
                     * part image
                     */

                    overlapped = false;

                    for (overlap = 1; overlap < elementHeight && ! overlapped; overlap++) {
                        if (part->leftEdge[part->leftEdge.size() - overlap] < elementWidth) {
                            overlapped = true;
                        }
                    }

                    hMax = elementHeight + part->elementTopMargin;
                    for (int h = overlap; h < hMax; h++) {
                        part->leftEdge << 0;
                        part->rightEdge << elementWidth;
                    }

                } else {
                    part->annotateElement = nullptr;
                    part->elementHeight  = 0;
                }
            }

            /*
             * Lets see if we can slide the text up in the bottom left corner of
             * part image (or part element if display option selected)
             */

            overlapped = false;

            for (overlap = 1; overlap < textHeight && ! overlapped; overlap++) {
                if (part->leftEdge[part->leftEdge.size() - overlap] < textWidth) {
                    overlapped = true;
                  }
              }

            part->partBotMargin = part->instanceMeta.margin.valuePixels(YY);

            //int hMax = int(textHeight + part->instanceMeta.margin.valuePixels(YY)); // seems to be a problem here with instanceMeta.margin.value(YY)
            hMax = int(textHeight + part->partBotMargin);
            for (int h = overlap; h < hMax; h++) {
                part->leftEdge << 0;
                part->rightEdge << textWidth;
              }

            part->height = part->leftEdge.size();

            part->sortSize = QString("%1%2")
                    .arg(part->width, 8,10,QChar('0'))
                    .arg(part->height,8,10,QChar('0'));

            if (part->width > widestPart) {
                widestPart = part->width;
            }
            if (part->height > tallestPart) {
                tallestPart = part->height;
            }
        }
    }
    return 0;
}

QStringList Pli::configurePLIPart(
        QString &partColor,
        QString &type,
        PartType pT,
        bool fadeSteps,
        bool highlightStep){

    QString updatedColour = partColor;
    QStringList out;

    if (fadeSteps && (pT == FADE_PART)) {
        updatedColour = QString("%1%2").arg(LPUB3D_COLOUR_FADE_PREFIX).arg(partColor);
        out << QString("0 // LPub3D custom colours");
        out << gui->createColourEntry(partColor, pT);
        out << QString("0 !FADE %1").arg(Preferences::fadeStepsOpacity);
    }
    if (highlightStep && (pT == HIGHLIGHT_PART)) {
        updatedColour = QString("%1%2").arg(LPUB3D_COLOUR_HIGHLIGHT_PREFIX).arg(partColor);
        out << QString("0 // LPub3D custom colours");
        out << gui->createColourEntry(partColor, pT);
        out << QString("0 !SILHOUETTE %1 %2")
                       .arg(Preferences::highlightStepLineWidth)
                       .arg(Preferences::highlightStepColour);
    }

//  // For POV generation, do not use pli.mpd orientation
//  if (Preferences::preferredRenderer == RENDERER_POVRAY)
//  {
//      out << QString("1 %1 0 0 0 1 0 0 0 1 0 0 0 1 %2").arg(partColor).arg(type) << endl;
//  }
    out << orient(updatedColour, type);

    if (highlightStep && (pT == HIGHLIGHT_PART))
        out << QString("0 !SILHOUETTE");
    if (fadeSteps && (pT == FADE_PART))
        out << QString("0 !FADE");

    if ((pT == FADE_PART) || (pT == HIGHLIGHT_PART))
        out << QString("0 NOFILE");

    return out;
}

void Pli::partClass(
    QString &type,
    QString &pclass)
{
  pclass = titleDescription(type);

  if (pclass.length()) {
      QRegExp rx("^(\\w+)\\s+([0-9a-zA-Z]+).*$");
      if (pclass.contains(rx)) {
          pclass = rx.cap(1);
          if (rx.captureCount() == 2 && rx.cap(1) == "Technic") {
              pclass += rx.cap(2);
          }
        } else {
          pclass = "NoCat";
        }
    } else {
      pclass = "NoCat";
    }
}

int Pli::placePli(
    QList<QString> &keys,
    int    xConstraint,
    int    yConstraint,
    bool   packSubs,
    bool   sortType,
    int   &cols,
    int   &pliWidth,
    int   &pliHeight)
{
  
  // Place the first row
  BorderData borderData;
  borderData = pliMeta.border.valuePixels();
  int left = 0;
  int nPlaced = 0;
  int tallest = 0;
  int topMargin = int(borderData.margin[1]+borderData.thickness);
  int botMargin = topMargin;

  cols = 0;

  pliWidth = 0;
  pliHeight = 0;

  for (int i = 0; i < keys.size(); i++) {
      parts[keys[i]]->placed = false;
      if (parts[keys[i]]->height > yConstraint) {
          yConstraint = parts[keys[i]]->height;
          // return -2;
        }
    }

  QList< QPair<int, int> > margins;

  while (nPlaced < keys.size()) {

      int i;
      PliPart *part = nullptr;

      for (i = 0; i < keys.size(); i++) {
          QString key = keys[i];
          part = parts[key];
          if ( ! part->placed && left + part->width < xConstraint) {
              break;
            }
        }

      if (i == keys.size()) {
          return -1;
        }

      /* Start new col */

      PliPart *prevPart = parts[keys[i]];

      cols++;

      int width = prevPart->width /* + partMarginX */;
      int widest = i;

      prevPart->left = left;
      prevPart->bot  = 0;
      prevPart->placed = true;
      prevPart->col = cols;
      nPlaced++;

      QPair<int, int> margin;


      margin.first = qMax(prevPart->instanceMeta.margin.valuePixels(XX),
                          prevPart->csiMargin.valuePixels(XX));

//      TODO_DONE - Compare BOM Element Margin

      // Compare BOM Element Margin
      if (bom && pliMeta.partElements.display.value()) {
          part->elementTopMargin = qMax(prevPart->styleMeta.margin.valuePixels(XX),
                                   prevPart->csiMargin.valuePixels(XX));
          if (part->elementTopMargin > margin.first)
              margin.first = part->elementTopMargin;
      }

      tallest = qMax(tallest,prevPart->height);

      int right = left + prevPart->width;
      int bot = prevPart->height;

      botMargin = qMax(botMargin,prevPart->csiMargin.valuePixels(YY));

      // leftEdge is the number of pixels between the left edge of the image
      // and the leftmost pixel in the image

      // rightEdge is the number of pixels between the left edge of the image
      // and the rightmost pixel in the image

      // lets see if any unplaced part fits under the right side
      // of the first part of the column

      bool fits = false;
      for (i = 0; i < keys.size() && ! fits; i++) {
          part = parts[keys[i]];

          if ( ! part->placed) {
              int xMargin = qMax(prevPart->csiMargin.valuePixels(XX),
                                 part->csiMargin.valuePixels(XX));
              int yMargin = qMax(prevPart->csiMargin.valuePixels(YY),
                                 part->csiMargin.valuePixels(YY));

              // Do they overlap?

              int top;
              for (top = 0; top < part->height; top++) {
                  int ltop  = prevPart->height - part->height - yMargin + top;
                  if (ltop >= 0 && ltop < prevPart->height) {
                      if (prevPart->rightEdge[ltop] + xMargin >
                          prevPart->width - part->width + part->leftEdge[top]) {
                          break;
                        }
                    }
                }
              if (top == part->height) {
                  fits = true;
                  break;
                }
            }
        }
      if (fits) {
          part->left = prevPart->left + prevPart->width - part->width;
          part->bot  = 0;
          part->placed = true;
          part->col = cols;
          nPlaced++;
        }

      // allocate new row

      while (nPlaced < parts.size()) {

          int overlap = 0;

          bool overlapped = false;

          // new possible upstairs neighbors

          for (i = 0; i < keys.size() && ! overlapped; i++) {
              PliPart *part = parts[keys[i]];

              if ( ! part->placed) {

                  int splitMargin = qMax(prevPart->topMargin,part->csiMargin.valuePixels(YY));

                  // dropping part down into prev part (top part is right edge, bottom left)

                  for (overlap = 1; overlap < prevPart->height && ! overlapped; overlap++) {
                      if (overlap > part->height) { // in over our heads?

                          // slide the part from the left to right until it bumps into previous
                          // part
                          for (int right = 0, left = 0;
                               right < part->height;
                               right++,left++) {
                              if (part->rightEdge[right] + splitMargin >
                                  prevPart->leftEdge[left+overlap-part->height]) {
                                  overlapped = true;
                                  break;
                                }
                            }
                        } else {
                          // slide the part from the left to right until it bumps into previous
                          // part
                          for (int right = part->height - overlap - 1, left = 0;
                               right < part->height && left < overlap;
                               right++,left++) {
                              if (right >= 0 && part->rightEdge[right] + splitMargin >
                                  prevPart->leftEdge[left]) {
                                  overlapped = true;
                                  break;
                                }
                            }
                        }
                    }

                  // overlap = 0;

                  if (bot + part->height + splitMargin - overlap <= yConstraint) {
                      bot += splitMargin;
                      break;
                    } else {
                      overlapped = false;
                    }
                }
            }

          if (i == keys.size()) {
              break; // we can't go more Vertical in this column
            }

          PliPart *part = parts[keys[i]];

          margin.first    = part->csiMargin.valuePixels(XX);
          int splitMargin = qMax(prevPart->topMargin,part->csiMargin.valuePixels(YY));

          prevPart = parts[keys[i]];

          prevPart->left = left;
          prevPart->bot  = bot - overlap;
          prevPart->placed = true;
          prevPart->col = cols;
          nPlaced++;

          if (sortType) {
              if (prevPart->width > width) {
                  widest = i;
                  width = prevPart->width;
                }
            }

          int height = prevPart->height + splitMargin;

          // try to do sub columns

          if (packSubs && 0 /* && overlap == 0 */ ) {
              int subLeft = left + prevPart->width;
              int top = bot + prevPart->height - overlap + prevPart->topMargin;

              // allocate new sub_col

              while (nPlaced < keys.size() && i < parts.size()) {

                  PliPart *part = parts[keys[i]];
                  int subMargin = 0;
                  for (i = 0; i < keys.size(); i++) {
                      part = parts[keys[i]];
                      if ( ! part->placed) {
                          subMargin = qMax(prevPart->csiMargin.valuePixels(XX),part->csiMargin.valuePixels(XX));
                          if (subLeft + subMargin + part->width <= right &&
                              bot + part->height + part->topMargin <= top) {
                              break;
                            }
                        }
                    }

                  if (i == parts.size()) {
                      break;
                    }

                  int subWidth = part->width;
                  part->left = subLeft + subMargin;
                  part->bot  = bot;
                  part->placed = true;
                  nPlaced++;

                  int subBot = bot + part->height + part->topMargin;

                  // try to place sub_row

                  while (nPlaced < parts.size()) {

                      for (i = 0; i < parts.size(); i++) {
                          part = parts[keys[i]];
                          subMargin = qMax(prevPart->csiMargin.valuePixels(XX),part->csiMargin.valuePixels(XX));
                          if ( ! part->placed &&
                               subBot + part->height + splitMargin <= top &&
                               subLeft + subMargin + part->width <= right) {
                              break;
                            }
                        }

                      if (i == parts.size()) {
                          break;
                        }

                      part->left = subLeft + subMargin;
                      part->bot  = subBot;
                      part->placed = true;
                      nPlaced++;

                      subBot += part->height + splitMargin;
                    }
                  subLeft += subWidth;
                }
            }

          bot -= overlap;

          // FIMXE:: try to pack something under bottom of the row.

          bot += height;
          if (bot > tallest) {
              tallest = bot;
            }
        }
      topMargin = qMax(topMargin,part->topMargin);

      left += width;

      part = parts[keys[widest]];
      if (part->annotWidth) {
          margin.second = qMax(part->styleMeta.margin.valuePixels(XX),part->csiMargin.valuePixels(XX));
        } else {
          margin.second = part->csiMargin.valuePixels(XX);
        }
      margins.append(margin);
    }

  pliWidth = left;

  int margin;
  int totalCols = margins.size();
  int lastMargin = 0;
  for (int col = 0; col < totalCols; col++) {
      lastMargin = margins[col].second;
      if (col == 0) {
          int bmargin = int(borderData.thickness + borderData.margin[0]);
          margin = qMax(bmargin,margins[col].first);
        } else {
          margin = qMax(margins[col].first,margins[col].second);
        }
      for (int i = 0; i < parts.size(); i++) {
          if (parts[keys[i]]->col >= col+1) {
              parts[keys[i]]->left += margin;
            }
        }
      pliWidth += margin;
    }
  if (lastMargin < borderData.margin[0]+borderData.thickness) {
      lastMargin = int(borderData.margin[0]+borderData.thickness);
    }
  pliWidth += lastMargin;

  pliHeight = tallest;

  for (int i = 0; i < parts.size(); i++) {
      parts[keys[i]]->bot += botMargin;
    }

  pliHeight += botMargin + topMargin;

  return 0;
}

void Pli::placeCols(
    QList<QString> &keys)
{
  QList< QPair<int, int> > margins;

  // Place the first row
  BorderData borderData;
  borderData = pliMeta.border.valuePixels();

  float topMargin = parts[keys[0]]->topMargin;
  topMargin = qMax(borderData.margin[1]+borderData.thickness,topMargin);
  float botMargin = parts[keys[0]]->csiMargin.valuePixels(YY);
  botMargin = qMax(borderData.margin[1]+borderData.thickness,botMargin);

  int height = 0;
  int width;

  PliPart *part = parts[keys[0]];

  float borderMargin = borderData.thickness + borderData.margin[XX];

  width = int(qMax(borderMargin, part->maxMargin()));

  for (int i = 0; i < keys.size(); i++) {
      part = parts[keys[i]];
      part->left = width;
      part->bot  = int(botMargin);
      part->col = i;

      width += part->width;

      if (part->height > height) {
          height = part->height;
        }

      if (i < keys.size() - 1) {
          PliPart *nextPart = parts[keys[i+1]];
          width += int(qMax(part->maxMargin(),nextPart->maxMargin()));
        }
    }
  part = parts[keys[keys.size()-1]];
  width += int(qMax(part->maxMargin(),borderMargin));
  
  size[0] = width;
  size[1] = int(topMargin + height + botMargin);
}

void Pli::getLeftEdge(
    QImage     &image,
    QList<int> &edge)
{
  QImage alpha = image.alphaChannel();

  for (int y = 0; y < alpha.height(); y++) {
      int x;
      for (x = 0; x < alpha.width(); x++) {
          QColor c = alpha.pixel(x,y);
          if (c.blue()) {
              edge << x;
              break;
            }
        }
      if (x == alpha.width()) {
          edge << x - 1;
        }
    }
}

void Pli::getRightEdge(
    QImage     &image,
    QList<int> &edge)
{
  QImage alpha = image.alphaChannel();

  for (int y = 0; y < alpha.height(); y++) {
      int x;
      for (x = alpha.width() - 1; x >= 0; x--) {
          QColor c = alpha.pixel(x,y);
          if (c.blue()) {
              edge << x;
              break;
            }
        }
      if (x < 0) {
          edge << 0;
        }
    }
}

//TODO - Add BOM LEGO/BrickLink PartID Sort
int Pli::sortPli()
{
  // populate part size
  partSize();

  sortedKeys = parts.keys();

  if (pliMeta.sortBy.value() == SortOptionName[PartColour]){

      if (! bom)
        pliMeta.sort.setValue(true);

      // Sort parts by color
      for (int i = 0; i < parts.size() - 1; i++) {
          for (int j = i+1; j < parts.size(); j++) {
              if (parts[sortedKeys[i]]->sortColour < parts[sortedKeys[j]]->sortColour) {
                  QString t = sortedKeys[i];
                  sortedKeys[i] = sortedKeys[j];
                  sortedKeys[j] = t;
                }
            }
        }

      // Sort color-sorted parts by size
      for (int i = 0; i < parts.size() - 1; i++) {
          for (int j = i+1; j < parts.size(); j++) {
              if (parts[sortedKeys[i]]->sortColour == parts[sortedKeys[j]]->sortColour) {
                  if (parts[sortedKeys[i]]->sortSize < parts[sortedKeys[j]]->sortSize) {
                      QString t = sortedKeys[i];
                      sortedKeys[i] = sortedKeys[j];
                      sortedKeys[j] = t;
                    }
                }
            }
        }

    } else if (pliMeta.sortBy.value() == SortOptionName[PartCategory]){

      if (! bom)
        pliMeta.sort.setValue(true);

      // Sort parts by category
      for (int i = 0; i < parts.size() - 1; i++) {
          for (int j = i+1; j < parts.size(); j++) {
              if (parts[sortedKeys[i]]->sortCategory < parts[sortedKeys[j]]->sortCategory) {
                  QString t = sortedKeys[i];
                  sortedKeys[i] = sortedKeys[j];
                  sortedKeys[j] = t;
                }
            }
        }

      // Sort category-sorted parts by size
      for (int i = 0; i < parts.size() - 1; i++) {
          for (int j = i+1; j < parts.size(); j++) {
              if (parts[sortedKeys[i]]->sortCategory == parts[sortedKeys[j]]->sortCategory) {
                  if (parts[sortedKeys[i]]->sortSize < parts[sortedKeys[j]]->sortSize) {
                      QString t = sortedKeys[i];
                      sortedKeys[i] = sortedKeys[j];
                      sortedKeys[j] = t;
                    }
                }
            }
        }

    } else {

      // Sort parts by size
      for (int i = 0; i < parts.size() - 1; i++) {
          for (int j = i+1; j < parts.size(); j++) {
              if (parts[sortedKeys[i]]->sortSize < parts[sortedKeys[j]]->sortSize) {
                  QString t = sortedKeys[i];
                  sortedKeys[i] = sortedKeys[j];
                  sortedKeys[j] = t;
                }
            }
        }
    }
  return 0;
}

int Pli::partSize()
{
    isSubModel = false; // not sizing icon images

    if (renderer->useLDViewSCall()) {
      int rc = partSizeLDViewSCall();
      if (rc != 0)
        return rc;
    } else {

      QString key;
      widestPart = 0;
      tallestPart = 0;

      foreach(key,parts.keys()) {
          PliPart *part;

          // get part info
          part = parts[key];
          QFileInfo info(part->type);
          PieceInfo* pieceInfo = lcGetPiecesLibrary()->FindPiece(info.fileName().toUpper().toLatin1().constData(), nullptr, false, false);

          if (pieceInfo ||
              gui->isUnofficialPart(part->type) ||
              gui->isSubmodel(part->type)) {

              if (part->color == "16") {
                  part->color = "0";
                }

              QPixmap *pixmap = new QPixmap();
              if (pixmap == nullptr) {
                  return -1;
                }

              if (createPartImage(key,part->type,part->color,pixmap)) {
                  emit gui->messageSig(LOG_ERROR, QMessageBox::tr("Failed to create PLI part for key %1")
                                       .arg(key));
                  return -1;
              }

              QImage image = pixmap->toImage();

              part->pixmap = new PGraphicsPixmapItem(this,part,*pixmap,parentRelativeType,part->type, part->color);

              delete pixmap;

              part->pixmapWidth  = image.width();
              part->pixmapHeight = image.height();

              part->width  = image.width();

              /* Add instance count area */

              QString descr;

              descr = QString("%1x") .arg(part->instances.size(),0,10);

              QString font = pliMeta.instance.font.valueFoo();
              QString color = pliMeta.instance.color.value();

              part->instanceText =
                  new InstanceTextItem(this,part,descr,font,color,parentRelativeType);

              int textWidth, textHeight;

              part->instanceText->size(textWidth,textHeight);

              part->textHeight = textHeight;

              // if text width greater than image width
              // the bounding box is wider

              if (textWidth > part->width) {
                  part->width = textWidth;
                }

              /* Add annotation area */

              if (part->styleMeta.style.value() == AnnotationStyle::circle ||
                  part->styleMeta.style.value() == AnnotationStyle::square)
                  descr = Annotations::getStyleAnnotation(part->type);
              else
                  getAnnotation(part->type,descr);

              if (descr.size()) {

                  font   = part->styleMeta.font.valueFoo();
                  color  = part->styleMeta.color.value();

                  part->annotateText =
                      new AnnotateTextItem(this,part,descr,font,color,parentRelativeType);

                  part->annotateText->size(part->annotWidth,part->annotHeight);

                  if (part->annotWidth > part->width) {
                      part->width = part->annotWidth;
                    }

                  part->partTopMargin = part->styleMeta.margin.valuePixels(YY);           // annotationStyle margin

                  //int hMax = int(part->annotHeight + part->styleMeta.margin.value(YY));   // seems to be a problem here with styleMeta.margin.value(YY)
                  int hMax = int(part->annotHeight + part->partTopMargin);
                  for (int h = 0; h < hMax; h++) {
                      part->leftEdge  << part->width - part->annotWidth;
                      part->rightEdge << part->width;
                    }
                } else {
                  part->annotateText = nullptr;
                  part->annotWidth  = 0;
                  part->annotHeight = 0;
                  part->partTopMargin = 0;
                }

              part->topMargin = part->csiMargin.valuePixels(YY);
              getLeftEdge(image,part->leftEdge);
              getRightEdge(image,part->rightEdge);

              bool overlapped = false;

              int overlap = 0;
              int hMax = 0;
              int elementWidth = 0, elementHeight = 0;

              if (bom && pliMeta.partElements.display.value()) {

                  /* Add BOM Elements area */

                  descr.clear();
                  QString _colorid = part->color;
                  QString _typeid  = QFileInfo(part->type).baseName();

                  int which = 0;   // 0 = BL, 1 = LEGO
                  if (pliMeta.partElements.legoElements.value())
                      which = 1;

                  if (pliMeta.partElements.localLegoElements.value()) {
                      QString elementKey = QString("%1%2").arg(_typeid).arg(_colorid);
                      descr = Annotations::getLEGOElement(elementKey.toLower());
                  } else {
                      if (!Annotations::loadBLElements()){
                          QString URL(VER_LPUB3D_BLELEMENTS_DOWNLOAD_URL);
                          gui->downloadFile(URL, "BrickLink Elements");
                          QByteArray Buffer = gui->getDownloadedFile();
                          Annotations::loadBLElements(Buffer);
                      }
                      descr = Annotations::getBLElement(_colorid,_typeid,which);
                  }

                  if (descr.size()) {

                      if (pliMeta.annotation.elementStyle.value()){
                          font   = pliMeta.rectangleStyle.font.valueFoo();
                          color  = pliMeta.rectangleStyle.color.value();
                          part->elementTopMargin = pliMeta.rectangleStyle.margin.valuePixels(YY);
                      } else {
                          font   = pliMeta.annotate.font.valueFoo();
                          color  = pliMeta.annotate.color.value();
                          part->elementTopMargin = pliMeta.annotate.margin.valuePixels(YY);
                      }

                      part->annotateElement =
                              new AnnotateTextItem(this,part,descr,font,color,parentRelativeType,true);

                      part->annotateElement->size(elementWidth,elementHeight);

                      part->elementHeight = elementHeight;

                      if (elementWidth > part->width) {
                          part->width = elementWidth;
                      }

                      /*
                       * Lets see if we can slide the BOM Element up in the bottom left corner of
                       * part image
                       */
                      overlapped = false;

                      for (overlap = 1; overlap < elementHeight && ! overlapped; overlap++) {
                          if (part->leftEdge[part->leftEdge.size() - overlap] < elementWidth) {
                              overlapped = true;
                          }
                      }

                      hMax = elementHeight + part->elementTopMargin;
                      for (int h = overlap; h < hMax; h++) {
                          part->leftEdge << 0;
                          part->rightEdge << elementWidth;
                      }

                  } else {
                      part->annotateElement = nullptr;
                      part->elementHeight  = 0;
                  }
              }

              /*
               * Lets see if we can slide the text up in the bottom left corner of
               * part image (or part element if display option selected)
               */

              overlapped = false;

              for (overlap = 1; overlap < textHeight && ! overlapped; overlap++) {
                  if (part->leftEdge[part->leftEdge.size() - overlap] < textWidth) {
                      overlapped = true;
                    }
                }

              part->partBotMargin = part->instanceMeta.margin.valuePixels(YY);

              //int hMax = int(textHeight + part->instanceMeta.margin.valuePixels(YY)); // seems to be a problem here with instanceMeta.margin.value(YY)
              hMax = int(textHeight + part->partBotMargin);
              for (int h = overlap; h < hMax; h++) {
                  part->leftEdge << 0;
                  part->rightEdge << textWidth;
                }

              part->height = part->leftEdge.size();

              part->sortSize = QString("%1%2")
                  .arg(part->width, 8,10,QChar('0'))
                  .arg(part->height,8,10,QChar('0'));

              if (part->width > widestPart) {
                  widestPart = part->width;
                }
              if (part->height > tallestPart) {
                  tallestPart = part->height;
                }
            } else {
              delete parts[key];
              parts.remove(key);
            }
        }
    }

  return 0;
}

//LDView performance improvement
int Pli::partSizeLDViewSCall() {

    int rc = 0;
    QString key;
    widestPart = 0;
    tallestPart = 0;

    fadeSteps = Preferences::enableFadeSteps ;
    displayIcons = gApplication->mPreferences.mViewPieceIcons;
    fadeColour = LDrawColor::ldColorCode(Preferences::validFadeStepsColour);
    highlightStep = Preferences::enableHighlightStep && !gui->suppressColourMeta();
    bool fadePartOK = fadeSteps && !highlightStep && displayIcons;
    bool highlightPartOK = highlightStep && !fadeSteps && displayIcons;

    // 1. generate ldr files
    foreach(key,parts.keys()) {
        PliPart *pliPart;

        // get part info
        pliPart = parts[key];
        QFileInfo info(pliPart->type);
        PieceInfo* pieceInfo = lcGetPiecesLibrary()->FindPiece(info.fileName().toUpper().toLatin1().constData(), nullptr, false, false);

        if (pieceInfo ||
            gui->isUnofficialPart(pliPart->type) ||
            gui->isSubmodel(pliPart->type)) {

            if (pliPart->color == "16" || isSubModel) {
                pliPart->color = "0";
            }

            bool isColorPart = gui->ldrawColourParts.isLDrawColourPart(pliPart->type);

            emit gui->messageSig(LOG_INFO, QString("Processing PLI part for nameKey [%1]").arg(pliPart->nameKey));

            for (int pT = 0; pT < ptn.size(); pT++ ) {

#ifdef QT_DEBUG_MODE
        QString CurrentPartType = PartTypeNames[pT];
#endif
                if (((pT == FADE_PART) && !fadePartOK) || ((pT == HIGHLIGHT_PART) && !highlightPartOK))
                     continue;

                // moved from enum to save weight
                ia.baseName[pT] = QFileInfo(pliPart->type).baseName();
                ia.partColor[pT] = (pT == FADE_PART && fadeSteps && Preferences::fadeStepsUseColour) ? fadeColour : pliPart->color;

                // assemble ldr name
                QString key = !ptn[pT].typeName.isEmpty() ? pliPart->nameKey + ptn[pT].typeName : pliPart->nameKey;
                QString ldrName = QDir::currentPath() + "/" + Paths::tmpDir + "/" + key + ".ldr";
                QString imageDir = isSubModel ? Paths::submodelDir : Paths::partsDir;
                QString imageName = QDir::currentPath() + "/" + imageDir + "/" + key + ".png";

                // create icon path key - using actual color code
                QString colourCode, imageKey;
                if (pT != NORMAL_PART) {
                    colourCode = QString("%1").arg(pT == FADE_PART ?
                                                   QString("%1%2").arg(LPUB3D_COLOUR_FADE_PREFIX).arg(ia.partColor[pT]) :
                                                   QString("%1%2").arg(LPUB3D_COLOUR_HIGHLIGHT_PREFIX ).arg(ia.partColor[pT]));
                    if (isSubModel || isColorPart) {
                        imageKey = QString("%1%2_%3").arg(ia.baseName[pT]).arg(ptn[pT].typeName).arg(colourCode);
                    } else {
                        imageKey = QString("%1_%2").arg(ia.baseName[pT]).arg(colourCode);
                    }
                } else {
                    colourCode = ia.partColor[pT];
                    imageKey = QString("%1_%2").arg(ia.baseName[pT]).arg(colourCode);
                }

                // store imageName
                ia.imageKeys[pT] << imageKey;
                ia.imageNames[pT] << imageName;

                QFile part(imageName);

                if ( ! part.exists()) {

                    // create a DAT files to feed the renderer
                    part.setFileName(ldrName);
                    if ( ! part.open(QIODevice::WriteOnly)) {
                        QMessageBox::critical(nullptr,QMessageBox::tr(VER_PRODUCTNAME_STR),
                                              QMessageBox::tr("Cannot open ldr DAT file for writing part:\n%1:\n%2.")
                                              .arg(ldrName)
                                              .arg(part.errorString()));
                        return -1;
                    }

                    // store ldrName - long name includes key
                    ia.ldrNames[pT] << ldrName;

                    // define ldr file name
                    QFileInfo typeInfo = QFileInfo(pliPart->type);
                    QString iconType = typeInfo.fileName();
                    bool isColorPart = gui->ldrawColourParts.isLDrawColourPart(typeInfo.fileName());
                    if (pT != NORMAL_PART && (isSubModel || isColorPart))
                        iconType = typeInfo.baseName() + ptn[pT].typeName + "." + typeInfo.suffix();

                    // generate PLI Part file
                    QStringList pliFile;
                    pliFile = configurePLIPart(
                              ia.partColor[pT],
                              iconType,
                              (PartType)pT,
                              fadeSteps,
                              highlightStep);

                    QTextStream out(&part);
                    foreach (QString line, pliFile)
                        out << line << endl;
                    part.close();

                } else { ia.ldrNames[pT] << QStringList(); } // part already exist

            }     // for every part type

        }         // part is valid
        else
        {
            delete parts[key];
            parts.remove(key);
        }
    }            // for every part

    if (isSubModel && Preferences::modeGUI && ! gui->exporting()) {
        emit gui->progressPermResetSig();
        emit gui->progressPermMessageSig("Rendering submodel images...");
        emit gui->progressPermRangeSig(1, ptn.size());
    }

    for (int pT = 0; pT < ptn.size(); pT++ ) {   // for every part type

        if (isSubModel && Preferences::modeGUI && ! gui->exporting())
            emit gui->progressPermSetValueSig(pT);

#ifdef QT_DEBUG_MODE
        QString CurrentPartType = PartTypeNames[pT];
#endif

        if (((pT == FADE_PART) && !fadePartOK) || ((pT == HIGHLIGHT_PART) && !highlightPartOK))
             continue;

        QElapsedTimer timer;
        timer.start();

        if ((rc = createPartImagesLDViewSCall(ia.ldrNames[pT],(isSubModel ? false : pT == NORMAL_PART))) != 0) {
            emit gui->messageSig(LOG_ERROR, QMessageBox::tr("Failed to create PLI part images using LDView Single Call"));
            continue;
        }

        for (int i = 0; i < ia.imageKeys[pT].size() && displayIcons; i++) {                      // normal, fade, highlight image full paths
            emit gui->setPliIconPathSig(ia.imageKeys[pT][i],ia.imageNames[pT][i]);
        }

        if (!ia.ldrNames[pT].isEmpty())
            emit gui->messageSig(LOG_INFO, QString("%1 PLI (Single Call) for [%2] render took "
                                                   "%3 milliseconds to render %4.")
                                                   .arg(Render::getRenderer())
                                                   .arg(PartTypeNames[pT])
                                                   .arg(timer.elapsed())
                                                   .arg(QString("%1 %2")
                                                                .arg(ia.ldrNames[pT].size())
                                                                .arg(ia.ldrNames[pT].size() == 1 ? "image" : "images")));
    }

    if (isSubModel && Preferences::modeGUI && ! gui->exporting()) {
      emit gui->progressPermSetValueSig(ptn.size());
    }

    if (rc != 0)
        return rc;

    return 0;
}

int Pli::sizePli(Meta *_meta, PlacementType _parentRelativeType, bool _perStep)
{
  int rc;

  parentRelativeType = _parentRelativeType;
  perStep = _perStep;
  
  if (parts.size() == 0) {
      return 1;
    }

  meta = _meta;

  rc = sortPli();
  if (rc != 0) {
      return rc;
    }
  
  ConstrainData constrainData = pliMeta.constrain.value();
  
  return resizePli(meta,constrainData);
}

int Pli::sizePli(ConstrainData::PliConstrain constrain, unsigned height)
{
  if (parts.size() == 0) {
      return 1;
    }
  
  if (meta) {
      ConstrainData constrainData;
      constrainData.type = constrain;
      constrainData.constraint = height;

      return resizePli(meta,constrainData);
    }
  return 1;
}

int Pli::resizePli(
    Meta *meta,
    ConstrainData &constrainData)
{
  
  switch (parentRelativeType) {
    case StepGroupType:
      placement = meta->LPub.multiStep.pli.placement;
      break;
    case CalloutType:
      placement = meta->LPub.callout.pli.placement;
      break;
    default:
      placement = meta->LPub.pli.placement;
      break;
    }

  // Fill the part list image using constraint
  //   Constrain Height
  //   Constrain Width
  //   Constrain Columns
  //   Constrain Area
  //   Constrain Square

  int cols, height;
  bool packSubs = pliMeta.pack.value();
  bool sortType = pliMeta.sort.value();
  int pliWidth = 0,pliHeight = 0;

  if (constrainData.type == ConstrainData::PliConstrainHeight) {
      int cols;
      int rc;
      rc = placePli(sortedKeys,
                    10000000,
                    int(constrainData.constraint),
                    packSubs,
                    sortType,
                    cols,
                    pliWidth,
                    pliHeight);
      if (rc == -2) {
          constrainData.type = ConstrainData::PliConstrainArea;
        }
    } else if (constrainData.type == ConstrainData::PliConstrainColumns) {
      if (parts.size() <= constrainData.constraint) {
          placeCols(sortedKeys);
          pliWidth = Placement::size[0];
          pliHeight = Placement::size[1];
          cols = parts.size();
        } else {
          int bomCols = int(constrainData.constraint);

          int maxHeight = 0;
          for (int i = 0; i < parts.size(); i++) {
              maxHeight += parts[sortedKeys[i]]->height + parts[sortedKeys[i]]->csiMargin.valuePixels(1);
            }

          maxHeight += maxHeight;

          if (bomCols) {
              for (height = maxHeight/(4*bomCols); height <= maxHeight; height++) {
                  int rc = placePli(sortedKeys,10000000,
                                    height,
                                    packSubs,
                                    sortType,
                                    cols,
                                    pliWidth,
                                    pliHeight);
                  if (rc == 0 && cols == bomCols) {
                      break;
                    }
                }
            }
        }
    } else if (constrainData.type == ConstrainData::PliConstrainWidth) {

      int height = 0;
      for (int i = 0; i < parts.size(); i++) {
          height += parts[sortedKeys[i]]->height;
        }

      int cols;
      int good_height = height;

      for ( ; height > 0; height -= 4) {

          int rc = placePli(sortedKeys,10000000,
                            height,
                            packSubs,
                            sortType,
                            cols,
                            pliWidth,
                            pliHeight);
          if (rc) {
              break;
            }

          int w = 0;

          for (int i = 0; i < parts.size(); i++) {
              int t;
              t = parts[sortedKeys[i]]->left + parts[sortedKeys[i]]->width;
              if (t > w) {
                  w = t;
                }
            }
          if (w < constrainData.constraint) {
              good_height = height;
            }
        }
      placePli(sortedKeys,10000000,
               good_height,
               packSubs,
               sortType,
               cols,
               pliWidth,
               pliHeight);
    } else if (constrainData.type == ConstrainData::PliConstrainArea) {

      int height = 0;
      for (int i = 0; i < parts.size(); i++) {
          height += parts[sortedKeys[i]]->height;
        }

      int cols;
      int min_area = height*height;
      int good_height = height;

      // step by 1/10 of inch or centimeter

      int step = int(toPixels(0.1,DPI));

      for ( ; height > 0; height -= step) {

          int rc = placePli(sortedKeys,10000000,
                            height,
                            packSubs,
                            sortType,
                            cols,
                            pliWidth,
                            pliHeight);

          if (rc) {
              break;
            }

          int h = 0;
          int w = 0;

          for (int i = 0; i < parts.size(); i++) {
              int t;
              t = parts[sortedKeys[i]]->bot + parts[sortedKeys[i]]->height;
              if (t > h) {
                  h = t;
                }
              t = parts[sortedKeys[i]]->left + parts[sortedKeys[i]]->width;
              if (t > w) {
                  w = t;
                }
            }
          if (w*h < min_area) {
              min_area = w*h;
              good_height = height;
            }
        }
      placePli(sortedKeys,10000000,
               good_height,
               packSubs,
               sortType,
               cols,
               pliWidth,
               pliHeight);
    } else if (constrainData.type == ConstrainData::PliConstrainSquare) {

      int height = 0;
      for (int i = 0; i < parts.size(); i++) {
          height += parts[sortedKeys[i]]->height;
        }

      int cols;
      int min_delta = height;
      int good_height = height;
      int step = int(toPixels(0.1,DPI));

      for ( ; height > 0; height -= step) {

          int rc = placePli(sortedKeys,10000000,
                            height,
                            packSubs,
                            sortType,
                            cols,
                            pliWidth,
                            pliHeight);

          if (rc) {
              break;
            }

          int h = pliWidth;
          int w = pliHeight;

          int delta = 0;
          if (w < h) {
              delta = h - w;
            } else if (h < w) {
              delta = w - h;
            }
          if (delta < min_delta) {
              min_delta = delta;
              good_height = height;
            }
        }
      placePli(sortedKeys,10000000,
               good_height,
               packSubs,
               sortType,
               cols,
               pliWidth,
               pliHeight);
    }

  size[0] = pliWidth;
  size[1] = pliHeight;

  return 0;
}

void Pli::positionChildren(
    int height,
    qreal scaleX,
    qreal scaleY)
{
  QString key;

  foreach (key, sortedKeys) {
      PliPart *part = parts[key];
      if (part == nullptr) {
          continue;
        }
      float x,y;

      x = part->left;
      y = height - part->bot;

      if (part->annotateText) {
          part->annotateText->setParentItem(background);
          part->annotateText->setPos(
                (x + part->width - part->annotWidth)/scaleX,
                (y - part->height /*+ part->annotHeight*/)/scaleY);
        }

      part->pixmap->setParentItem(background);
      part->pixmap->setPos(
            x/scaleX,
            (y - part->height + part->annotHeight + part->partTopMargin)/scaleY);
      part->pixmap->setTransformationMode(Qt::SmoothTransformation);

      // Position the BOM Element
      if (bom && pliMeta.partElements.display.value() && part->annotateElement) {
          part->annotateElement->setParentItem(background);
          part->annotateElement->setPos(
                x/scaleX,
                (y - part->elementHeight - part->textHeight + part->elementTopMargin)/scaleY);
      }

      part->instanceText->setParentItem(background);
      part->instanceText->setPos(
            x/scaleX,
            (y - part->textHeight)/scaleY);
      bool foo = true;
    }
}

int Pli::addPli(
    int       submodelLevel,
    QGraphicsItem *parent)
{
  if (parts.size()) {
      background =
          new PliBackgroundItem(
            this,
            size[0],
          size[1],
          parentRelativeType,
          submodelLevel,
          parent);

      if ( ! background) {
          return -1;
        }

      background->size[0] = size[0];
      background->size[1] = size[1];

      positionChildren(size[1],1.0,1.0);
    } else {
      background = nullptr;
    }
  return 0;
}

void Pli::setPos(float x, float y)
{
  if (background) {
      background->setPos(x,y);
    }
}
void Pli::setFlag(QGraphicsItem::GraphicsItemFlag flag, bool value)
{
  if (background) {
      background->setFlag(flag,value);
    }
}

/*
 * Single step per page                   case 3 top/bottom of step
 * step in step group pli per step = true case 3 top/bottom of step
 * step in callout                        case 3 top/bottom of step
 * step group global pli                  case 2 topOfSteps/bottomOfSteps
 * BOM on single step per page            case 2 topOfSteps/bottomOfSteps
 * BOM on step group page                 case 1
 * BOM on cover page                      case 2 topOfSteps/bottomOfSteps
 * BOM on numbered page
 */

bool Pli::autoRange(Where &top, Where &bottom)
{
  if (bom || ! perStep) {
      top = topOfSteps();
      bottom = bottomOfSteps();
      return steps->list.size() && (perStep || bom);
    } else {
      top = topOfStep();
      bottom = bottomOfStep();
      return false;
    }
}

QString PGraphicsPixmapItem::pliToolTip(
    QString type,
    QString color)
{
  QString title = Pli::titleDescription(type);
  if (title == "") {
      Where here(type,0);
      title = gui->readLine(here);
      title = title.right(title.length() - 2);
    }
  QString toolTip;
  toolTip = LDrawColor::name(color) + " (" + LDrawColor::ldColorCode(LDrawColor::name(color)) + ") " + type + " \"" + title + "\" - right-click to modify";
  return toolTip;
}

const QString Pli::titleDescription(QString &part)
{
  QString    titleDescription;
  QFileInfo  info(part);
  PieceInfo* pieceInfo = lcGetPiecesLibrary()->FindPiece(info.fileName().toUpper().toLatin1().constData(), nullptr, false, false);

  if (pieceInfo) {
      titleDescription = pieceInfo->m_strDescription;
    }
  return titleDescription;
}

PliBackgroundItem::PliBackgroundItem(
    Pli           *_pli,
    int            width,
    int            height,
    PlacementType  _parentRelativeType,
    int            submodelLevel,
    QGraphicsItem *parent)
{
  pli       = _pli;
  grabHeight = height;
  grabber = nullptr;

  parentRelativeType = _parentRelativeType;

  QPixmap *pixmap = new QPixmap(width,height);

  QString toolTip;

  if (_pli->bom) {
      toolTip = "Bill Of Materials - right-click to modify";
    } else {
      toolTip = "Part List - right-click to modify";
    }

  if (parentRelativeType == StepGroupType /* && pli->perStep == false */) {
      if (pli-bom) {
          placement = pli->meta->LPub.bom.placement;
        } else {
          placement = pli->meta->LPub.multiStep.pli.placement;
        }
    } else {
      placement = pli->pliMeta.placement;
    }

  //gradient settings
  if (pli->pliMeta.background.value().gsize[0] == 0 &&
      pli->pliMeta.background.value().gsize[1] == 0) {
      pli->pliMeta.background.value().gsize[0] = pixmap->width();
      pli->pliMeta.background.value().gsize[1] = pixmap->width();
      QSize gSize(pli->pliMeta.background.value().gsize[0],
          pli->pliMeta.background.value().gsize[1]);
      int h_off = gSize.width() / 10;
      int v_off = gSize.height() / 8;
      pli->pliMeta.background.value().gpoints << QPointF(gSize.width() / 2, gSize.height() / 2)
                                              << QPointF(gSize.width() / 2 - h_off, gSize.height() / 2 - v_off);
    }
  setBackground( pixmap,
                 PartsListType,
                 pli->meta,
                 pli->pliMeta.background,
                 pli->pliMeta.border,
                 pli->pliMeta.margin,
                 pli->pliMeta.subModelColor,
                 submodelLevel,
                 toolTip);

  setPixmap(*pixmap);
  setParentItem(parent);
  if (parentRelativeType != SingleStepType && pli->perStep) {
      setFlag(QGraphicsItem::ItemIsMovable,false);
    }
}

void PliBackgroundItem::placeGrabbers()
{
  QRectF rect = currentRect();
  point = QPointF(rect.left() + rect.width()/2,rect.bottom());
  if (grabber == nullptr) {
      grabber = new Grabber(BottomInside,this,myParentItem());
    }
  grabber->setPos(point.x()-grabSize()/2,point.y()-grabSize()/2);
}

void PliBackgroundItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{     
  position = pos();
  positionChanged = false;
  QGraphicsItem::mousePressEvent(event);
  placeGrabbers();
} 

void PliBackgroundItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{ 
  positionChanged = true;
  QGraphicsItem::mouseMoveEvent(event);
  if (isSelected() && (flags() & QGraphicsItem::ItemIsMovable)) {
      placeGrabbers();
    }
}

void PliBackgroundItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
  QGraphicsItem::mouseReleaseEvent(event);

  if (isSelected() && (flags() & QGraphicsItem::ItemIsMovable)) {

      QPointF newPosition;

      // back annotate the movement of the PLI into the LDraw file.
      newPosition = pos() - position;
      if (newPosition.x() || newPosition.y()) {
          positionChanged = true;
          PlacementData placementData = placement.value();
          placementData.offsets[0] += newPosition.x()/pli->relativeToSize[0];
          placementData.offsets[1] += newPosition.y()/pli->relativeToSize[1];
          placement.setValue(placementData);

          Where here, bottom;
          bool useBot;

          useBot = pli->autoRange(here,bottom);

          if (useBot) {
              here = bottom;
            }
          changePlacementOffset(here,&placement,pli->parentRelativeType);
        }
    }
}

void PliBackgroundItem::contextMenuEvent(
    QGraphicsSceneContextMenuEvent *event)
{
  if (pli) {
      QMenu menu;
      bool showCameraDistFactorItem = (Preferences::usingNativeRenderer);

      PlacementData placementData = pli->placement.value();

      QString pl = pli->bom ? "Bill Of Materials" : "Parts List";
      QAction *placementAction  = commonMenus.placementMenu(menu,pl,
                                                            commonMenus.naturalLanguagePlacementWhatsThis(PartsListType,placementData,pl));
      QAction *cameraDistFactorAction = nullptr;
      QAction *scaleAction = nullptr;
      if (showCameraDistFactorItem){
          cameraDistFactorAction  = commonMenus.cameraDistFactorrMenu(menu, pl);
      } else {
          scaleAction             = commonMenus.scaleMenu(menu, pl);
      }
      QAction *constrainAction     = commonMenus.constrainMenu(menu,pl);
      QAction *backgroundAction    = commonMenus.backgroundMenu(menu,pl);
      QAction *subModelColorAction = commonMenus.subModelColorMenu(menu,pl);
      QAction *borderAction        = commonMenus.borderMenu(menu,pl);
      QAction *marginAction        = commonMenus.marginMenu(menu,pl);
      QAction *sortAction          = commonMenus.sortMenu(menu,pl);
      QAction *annotationAction    = commonMenus.annotationMenu(menu,pl);
      QAction *cameraFoVAction     = commonMenus.cameraFoVMenu(menu,pl);
      QAction *cameraAnglesAction  = commonMenus.cameraAnglesMenu(menu,pl);

      QAction *splitBomAction  = nullptr;
      QAction *deleteBomAction = nullptr;

      if (pli->bom) {
          splitBomAction = menu.addAction("Split Bill of Materials");
          splitBomAction->setIcon(QIcon(":/resources/splitbom.png"));

          deleteBomAction = menu.addAction("Delete Bill of Materials");
          deleteBomAction->setIcon(QIcon(":/resources/delete.png"));

          annotationAction->setIcon(QIcon(":/resources/bomannotation.png"));
        }

      QAction *selectedAction   = menu.exec(event->screenPos());

      if (selectedAction == nullptr) {
          return;
        }

      Where top;
      Where bottom;

      switch (parentRelativeType) {
        case CalloutType:
          top    = pli->topOfCallout();
          bottom = pli->bottomOfCallout();
          break;
        default:
          if (pli->step) {
              if (pli->bom) {
                  top = pli->topOfSteps();
                  bottom = pli->bottomOfSteps();
                } else {
                  top = pli->topOfStep();
                  bottom = pli->bottomOfStep();
                }
            } else {
              top = pli->topOfSteps();
              bottom = pli->bottomOfSteps();
            }
          break;
        }

      QString me = pli->bom ? "BOM" : "PLI";
      if (selectedAction == sortAction) {
          changePliSort(me+" Sort",
                        top,
                        bottom,
                        &pli->pliMeta.sortBy.sortOption);
        } else if (selectedAction == annotationAction) {
          changePliAnnotation(me+" Annotaton",
                              top,
                              bottom,
                              &pli->pliMeta.annotation);
        } else if (selectedAction == constrainAction) {
          changeConstraint(me+" Constraint",
                           top,
                           bottom,
                           &pli->pliMeta.constrain);
        } else if (selectedAction == placementAction) {
          if (pli->bom) {
              changePlacement(parentRelativeType,
                              pli->perStep,
                              PartsListType,
                              me+" Placement",
                              top,
                              bottom,
                              &pli->pliMeta.placement,true,1,0,false);
            } else if (pli->perStep) {
              changePlacement(parentRelativeType,
                              pli->perStep,
                              PartsListType,
                              me+" Placement",
                              top,
                              bottom,
                              &pli->placement);
            } else {
              changePlacement(parentRelativeType,
                              pli->perStep,
                              PartsListType,
                              me+" Placement",
                              top,
                              bottom,
                              &pli->placement,true,1,0,false);
            }
        } else if (selectedAction == marginAction) {
          changeMargins(me+" Margins",
                        top,
                        bottom,
                        &pli->pliMeta.margin);
        } else if (selectedAction == backgroundAction) {
          changeBackground(me+" Background",
                           top,
                           bottom,
                           &pli->pliMeta.background);
        } else if (selectedAction == subModelColorAction) {
          changeSubModelColor(me+" Background Color",
                           top,
                           bottom,
                           &pli->pliMeta.subModelColor);

        } else if (selectedAction == borderAction) {
          changeBorder(me+" Border",
                       top,
                       bottom,
                       &pli->pliMeta.border);
        } else if (selectedAction == cameraDistFactorAction) {
              changeCameraDistFactor(pl+" Camera Distance",
                                     "Native Camera Distance",
                                     top,
                                     bottom,
                                     &pli->pliMeta.cameraDistNative.factor);
        } else if (selectedAction == scaleAction){
              changeFloatSpin(pl+" Scale",
                              "Model Size",
                              top,
                              bottom,
                              &pli->pliMeta.modelScale);
        } else if (selectedAction == cameraFoVAction) {
          changeFloatSpin(me+" Camera Angle",
                          "Camera FOV",
                          top,
                          bottom,
                          &pli->pliMeta.cameraFoV);
        } else if (selectedAction == cameraAnglesAction) {
            changeCameraAngles(me+" Camera Angles",
                            top,
                            bottom,
                            &pli->pliMeta.cameraAngles);
        } else if (selectedAction == deleteBomAction) {
          deleteBOM();
        } else if (selectedAction == splitBomAction){
          insertSplitBOM();
        }
    }
}

/*
 * Code for resizing the PLI - part of the resize class described in
 * resize.h
 */

void PliBackgroundItem::resize(QPointF grabbed)
{
  // recalculate corners Y
  
  point = grabbed;

  // Figure out desired height of PLI
  
  if (pli && pli->parentRelativeType == CalloutType) {
      QPointF absPos = pos();
      absPos = mapToScene(absPos);
      grabHeight = int(grabbed.y() - absPos.y());
    } else {
      grabHeight = int(grabbed.y() - pos().y());
    }

  ConstrainData constrainData;
  constrainData.type = ConstrainData::PliConstrainHeight;
  constrainData.constraint = grabHeight;

  pli->resizePli(pli->meta, constrainData);

  qreal width = pli->size[0];
  qreal height = pli->size[1];
  qreal scaleX = width/size[0];
  qreal scaleY = height/size[1];

  pli->positionChildren(int(height),scaleX,scaleY);
  
  point = QPoint(int(pos().x()+width/2),int(pos().y()+height));
  grabber->setPos(point.x()-grabSize()/2,point.y()-grabSize()/2);

  resetTransform();
  setTransform(QTransform::fromScale(scaleX,scaleY),true);

  QList<QGraphicsItem *> kids = childItems();

  for (int i = 0; i < kids.size(); i++) {
      kids[i]->resetTransform();
      kids[i]->setTransform(QTransform::fromScale(1.0/scaleX,1.0/scaleY),true);
    }

  sizeChanged = true;
}

void PliBackgroundItem::change()
{
  ConstrainData constrainData;

  constrainData.type = ConstrainData::PliConstrainHeight;
  constrainData.constraint = int(grabHeight);
  
  pli->pliMeta.constrain.setValue(constrainData);

  Where top, bottom;
  bool useBot;

  // for single step with BOM, we have to do something special

  useBot = pli->autoRange(top,bottom);
  int append = 1;
  if (pli->bom && pli->steps->relativeType == SingleStepType && pli->steps->list.size() == 1) {
      Range *range = dynamic_cast<Range *>(pli->steps->list[0]);
      if (range->list.size() == 1) {
          append = 0;
        }
    }

  changeConstraint(top,bottom,&pli->pliMeta.constrain,append,useBot);
}

QRectF PliBackgroundItem::currentRect()
{
  if (pli->parentRelativeType == CalloutType) {
      QRectF foo (pos().x(),pos().y(),size[0],size[1]);
      return foo;
    } else {
      return sceneBoundingRect();
    }
}

void AnnotateTextItem::contextMenuEvent(
    QGraphicsSceneContextMenuEvent *event)
{
  QMenu menu;

  QString pl = "Part Annotation";

  QAction *fontAction       = commonMenus.fontMenu(menu,pl);
  QAction *colorAction      = commonMenus.colorMenu(menu,pl);
  QAction *marginAction     = commonMenus.marginMenu(menu,pl);
  QAction *borderAction     = commonMenus.borderMenu(menu,pl);
  QAction *backgroundAction = commonMenus.backgroundMenu(menu,pl);
  QAction *sizeAction       = nullptr; // commonMenus.sizeMenu(menu,pl);


  QAction *selectedAction   = menu.exec(event->screenPos());

  if (selectedAction == nullptr) {
      return;
    }
  
  Where top;
  Where bottom;
  switch (parentRelativeType) {
    case CalloutType:
      top    = pli->topOfCallout();
      bottom = pli->bottomOfCallout();
      break;
    default:
      top    = pli->topOfStep();
      bottom = pli->bottomOfStep();
      break;
    }

  if (selectedAction == fontAction) {
      changeFont(top,
                 bottom,
                 &styleMeta->font);
    } else if (selectedAction == colorAction) {
      changeColor(top,
                  bottom,
                  &styleMeta->color);
    } else if (selectedAction == marginAction) {
      changeMargins(pl + " Margins",
                    top,
                    bottom,
                    &styleMeta->margin);
    } else if (selectedAction == backgroundAction) {
      changeBackground(pl+" Background",
                       top,
                       bottom,
                       &styleMeta->background,
                       true,1,true,false);  // no picture
    } else if (selectedAction == borderAction) {
      bool corners = styleMeta->style.value() == circle;
      changeBorder(pl + " Border",
                   top,
                   bottom,
                   &styleMeta->border,
                   true,1,true,false,corners);
    } else if (selectedAction == sizeAction) {
//      changeSize(pl + " Size",
//                   "Width",
//                   "Height",
//                   top,
//                   bottom,
//                   &styleMeta->size);
    }
}

void InstanceTextItem::contextMenuEvent(
    QGraphicsSceneContextMenuEvent *event)
{
  QMenu menu;

  QString pl = "Parts Count ";

  QAction *fontAction   = commonMenus.fontMenu(menu,pl);
  QAction *colorAction  = commonMenus.colorMenu(menu,pl);
  QAction *marginAction = commonMenus.marginMenu(menu,pl);

  QAction *selectedAction   = menu.exec(event->screenPos());

  if (selectedAction == nullptr) {
      return;
    }
  
  Where top;
  Where bottom;
  
  switch (parentRelativeType) {
    case CalloutType:
      top    = pli->topOfCallout();
      bottom = pli->bottomOfCallout();
      break;
    default:
      top    = pli->topOfStep();
      bottom = pli->bottomOfStep();
      break;
    }

  if (selectedAction == fontAction) {
      changeFont(top,bottom,&pli->pliMeta.instance.font,1,false);
    } else if (selectedAction == colorAction) {
      changeColor(top,bottom,&pli->pliMeta.instance.color,1,false);
    } else if (selectedAction == marginAction) {
      changeMargins(pl + " Margins",top,bottom,&pli->pliMeta.instance.margin,true,1,false);
    }
}

void PGraphicsPixmapItem::contextMenuEvent(
    QGraphicsSceneContextMenuEvent *event)
{
  QMenu menu;
  QString pl = "Part";
  QAction *hideAction = menu.addAction("Hide Part from Parts List");
  hideAction->setIcon(QIcon(":/resources/display.png"));

  QAction *marginAction      = commonMenus.marginMenu(menu,pl);

  /*
  QAction *scaleAction        = commonMenus.scaleMenu(menu,pl);
  QAction *cameraFoVAction    = commonMenus.cameraFoVMenu(menu,pl);
  QAction *cameraAnglesAction = commonMenus.cameraAnglesMenu(menu,pl);
  */

  Where top;
  Where bottom;
  switch (parentRelativeType) {
    case CalloutType:
      top    = pli->topOfCallout();
      bottom = pli->bottomOfCallout();
      break;
    default:
      top    = pli->topOfStep();
      bottom = pli->bottomOfStep();
      break;
    }

  QAction *selectedAction   = menu.exec(event->screenPos());

  if (selectedAction == nullptr) {
      return;
    }

  if (selectedAction == marginAction) {

      changeMargins(pl+" Margins",
                    top,
                    bottom,
                    &pli->pliMeta.part.margin);
    } else if (selectedAction == hideAction) {
      hidePLIParts(this->part->instances);
    } /* else if (selectedAction == scaleAction) {
        changeFloatSpin(pl,
                        "Model Size",
                        top,
                        bottom,
                        &pli->pliMeta.modelScale);
  } else if (selectedAction == cameraFoVAction) {
    changeFloatSpin(pl,
                    "Camera FOV",
                    top,
                    bottom,
                    &pli->pliMeta.cameraFoV);
  } else if (selectedAction == cameraAnglesAction) {
      changeCameraAngles(pl+" Camera Angles",
                      top,
                      bottom,
                      &pli->pliMeta.cameraAngles);
   } */
}


//-----------------------------------------
//-----------------------------------------
//-----------------------------------------

#include <QGraphicsRectItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsPixmapItem>

AnnotateTextItem::AnnotateTextItem(
  Pli     *_pli,
  PliPart *_part,
  QString &_text,
  QString &_fontString,
  QString &_colorString,
  PlacementType _parentRelativeType,
 bool           _element)
{
  parentRelativeType = _parentRelativeType;

  QString fontString = _fontString;

  bool useDocSize = false;

  QString toolTip;

  if (_element){
      if (_pli->pliMeta.annotation.elementStyle.value()){
          styleMeta  = &_pli->pliMeta.rectangleStyle;
      }
      toolTip = QString("%1 Element Annotation - right-click to modify")
                       .arg(_pli->pliMeta.partElements.legoElements.value() ? "LEGO" : "BrickLink");
  } else {
      styleMeta  = &_part->styleMeta;
      useDocSize = styleMeta->style.value() == AnnotationStyle::none;
      toolTip    = "Part Annotation - right-click to modify";
      // adjust font size for 5.5 parts - set Arial to 17 (Interim solution)
      // TODO - automatically resize text until it fits
      if ((_part->styleMeta.style.value() == AnnotationStyle::circle ||
           _part->styleMeta.style.value() == AnnotationStyle::square) &&
           _text.size() > 2) {
         fontString = "Arial,17,-1,5,50,0,0,0,0,0";
      }
  }

  setText(_pli,_part,_text,fontString,toolTip);

  QRectF docSize  = QRectF(0,0,document()->size().width(),document()->size().height());

  if (useDocSize) {
      annotateRect = docSize;
  } else {
      bool dw = part->styleMeta.style.value() == AnnotationStyle::rectangle || _element;
      QRectF styleSize = QRectF(0,0,dw ? docSize.width() : styleMeta->size.valuePixels(0),styleMeta->size.valuePixels(1));
      annotateRect = boundingRect().adjusted(0,0,styleSize.width()-docSize.width(),styleSize.height()-docSize.height());
      // center the document on the new size
      setTextWidth(-1);
      setTextWidth(annotateRect.width());
      QTextBlockFormat format;
      format.setAlignment(Qt::AlignCenter);
      QTextCursor cursor = textCursor();
      cursor.select(QTextCursor::Document);
      cursor.mergeBlockFormat(format);
      cursor.clearSelection();
      setTextCursor(cursor);
  }

  QColor color(_colorString);
  setDefaultTextColor(color);

  setAttributes();
}

void AnnotateTextItem::size(int &x, int &y)
{
  x = int(annotateRect.width());
  y = int(annotateRect.height());
}

void AnnotateTextItem::setAttributes(){

    subModelColor = pli->pliMeta.subModelColor;
    if (pli->background)
      submodelLevel = pli->background->submodelLevel;

    //gradient settings
    if (styleMeta->background.value().gsize[0] == 0 &&
        styleMeta->background.value().gsize[1] == 0) {
        styleMeta->background.value().gsize[0] = annotateRect.width();
        styleMeta->background.value().gsize[1] = annotateRect.width();
        QSize gSize(styleMeta->background.value().gsize[0],
            styleMeta->background.value().gsize[1]);
        int h_off = gSize.width() / 10;
        int v_off = gSize.height() / 8;
        styleMeta->background.value().gpoints << QPointF(gSize.width() / 2, gSize.height() / 2)
                                   << QPointF(gSize.width() / 2 - h_off, gSize.height() / 2 - v_off);
    }

    setZValue(98);
    setFlag(QGraphicsItem::ItemIsSelectable,true);
}

QGradient AnnotateTextItem::setGradient(){

    BackgroundData backgroundData = styleMeta->background.value();
    QPolygonF pts;
    QGradientStops stops;

    QSize gSize(backgroundData.gsize[0],backgroundData.gsize[1]);

    for (int i=0; i<backgroundData.gpoints.size(); i++)
        pts.append(backgroundData.gpoints.at(i));

    QGradient::CoordinateMode mode = QGradient::LogicalMode;
    switch (backgroundData.gmode){
    case BackgroundData::LogicalMode:
        mode = QGradient::LogicalMode;
        break;
    case BackgroundData::StretchToDeviceMode:
        mode = QGradient::StretchToDeviceMode;
        break;
    case BackgroundData::ObjectBoundingMode:
        mode = QGradient::ObjectBoundingMode;
        break;
    }

    QGradient::Spread spread = QGradient::RepeatSpread;
    switch (backgroundData.gspread){
    case BackgroundData::PadSpread:
        spread = QGradient::PadSpread;
        break;
    case BackgroundData::RepeatSpread:
        spread = QGradient::RepeatSpread;
        break;
    case BackgroundData::ReflectSpread:
        spread = QGradient::ReflectSpread;
        break;
    }

    QGradient g = QLinearGradient(pts.at(0), pts.at(1));
    switch (backgroundData.gtype){
    case BackgroundData::LinearGradient:
        g = QLinearGradient(pts.at(0), pts.at(1));
        break;
    case BackgroundData::RadialGradient:
    {
        QLineF line(pts[0], pts[1]);
        if (line.length() > 132){
            line.setLength(132);
        }
        g = QRadialGradient(line.p1(),  qMin(gSize.width(), gSize.height()) / 3.0, line.p2());
    }
        break;
    case BackgroundData::ConicalGradient:
    {
        qreal angle = backgroundData.gangle;
        g = QConicalGradient(pts.at(0), angle);
    }
        break;
    case BackgroundData::NoGradient:
        break;
    }

    for (int i=0; i<backgroundData.gstops.size(); ++i) {
        stops.append(backgroundData.gstops.at(i));
    }

    g.setStops(stops);
    g.setSpread(spread);
    g.setCoordinateMode(mode);

    return g;
}

void AnnotateTextItem::setBackground(QPainter *painter)
{

    // set border and background parameters
    BorderData     borderData     = styleMeta->border.valuePixels();
    BackgroundData backgroundData = styleMeta->background.value();

    // set defaults for using a background image only
    if ((backgroundData.type == BackgroundData::BgImage) &&
            !backgroundData.stretch) {
        backgroundData.stretch = true;
    }

    // set rectangle size and dimensions parameters
    int ibt = int(borderData.thickness);
    QRectF irect(ibt/2,ibt/2,annotateRect.width()-ibt,annotateRect.height()-ibt);

    // set painter and render hints (initialized with pixmap)
    painter->setRenderHints(QPainter::TextAntialiasing | QPainter::HighQualityAntialiasing);

    // set the background then set the border and paint both in one go.

    /* BACKGROUND */
    bool useGradient = false;
    QColor brushColor;
    switch(backgroundData.type) {
    case BackgroundData::BgTransparent:
        brushColor = Qt::transparent;
        break;
    case BackgroundData::BgGradient:
        useGradient = true;
        brushColor = Qt::transparent;
        break;
    case BackgroundData::BgColor:
    case BackgroundData::BgSubmodelColor:
        if (backgroundData.type == BackgroundData::BgColor) {
            brushColor = LDrawColor::color(backgroundData.string);
        } else {
            brushColor = LDrawColor::color(subModelColor.value(0));
        }
        break;
    case BackgroundData::BgImage:
        break;
    }

    useGradient ? painter->setBrush(QBrush(setGradient())) :
                  painter->setBrush(brushColor);

    /* BORDER */
    QPen borderPen;
    QColor borderPenColor;
    if (borderData.type == BorderData::BdrNone) {
        borderPenColor = Qt::transparent;
    } else {
        borderPenColor =  LDrawColor::color(borderData.color);
    }
    borderPen.setColor(borderPenColor);
    borderPen.setCapStyle(Qt::RoundCap);
    borderPen.setJoinStyle(Qt::RoundJoin);
    if (borderData.line == BorderData::BdrLnSolid){
        borderPen.setStyle(Qt::SolidLine);
    }
    else if (borderData.line == BorderData::BdrLnDash){
        borderPen.setStyle(Qt::DashLine);
    }
    else if (borderData.line == BorderData::BdrLnDot){
        borderPen.setStyle(Qt::DotLine);
    }
    else if (borderData.line == BorderData::BdrLnDashDot){
        borderPen.setStyle(Qt::DashDotLine);
    }
    else if (borderData.line == BorderData::BdrLnDashDotDot){
        borderPen.setStyle(Qt::DashDotDotLine);
    }
    borderPen.setWidth(ibt);

    painter->setPen(borderPen);

    // set icon border dimensions
    qreal rx = borderData.radius;
    qreal ry = borderData.radius;
    qreal dx = annotateRect.width();
    qreal dy = annotateRect.height();

    if (dx && dy) {
        if (dx > dy) {
            rx *= dy;
            rx /= dx;
        } else {
            ry *= dx;
            ry /= dy;
        }
    }

    // draw icon rectangle - background and border
    if (styleMeta->style.value() != AnnotationStyle::circle) {
        if (borderData.type == BorderData::BdrRound) {
            painter->drawRoundRect(irect,int(rx),int(ry));
        } else {
            painter->drawRect(irect);
        }
    } else {
        painter->drawEllipse(irect);
    }
}

void AnnotateTextItem::paint( QPainter *painter, const QStyleOptionGraphicsItem *o, QWidget *w)
{
    if (styleMeta->style.value() != AnnotationStyle::none)
        setBackground(painter);
    QGraphicsTextItem::paint(painter, o, w);
}
