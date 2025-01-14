
/****************************************************************************
**
** Copyright (C) 2007-2009 Kevin Clague. All rights reserved.
** Copyright (C) 2015 - 2019 Trevor SANDY. All rights reserved.
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
 * The traverse function is the one function that traverses the LDraw model
 * higherarchy seaching for pages to render.  It tracks the partial assembly
 * contents, parts list contents, step group contents, and callouts.
 *
 * It can count pages in the design, gather page contents for translation
 * into graphical representation of pages for the user.  In the future it
 * will gather Bill of Materials contents.
 *
 * Please see lpub.h for an overall description of how the files in LPub
 * make up the LPub program.
 *
 ***************************************************************************/

#include "lpub.h"
#include <QtWidgets>
#include <QGraphicsItem>
#include <QString>
#include <QFileInfo>
#include "lpub_preferences.h"
#include "ranges.h"
#include "callout.h"
#include "pointer.h"
#include "range.h"
#include "reserve.h"
#include "step.h"
#include "paths.h"
#include "metaitem.h"
#include "pointer.h"
#include "pagepointer.h"
#include "ranges_item.h"

#include "QsLog.h"

// Set to enable PageSize trace logging
#ifndef SIZE_DEBUG
//#define SIZE_DEBUG
#endif

#define FIRST_STEP 1
#define FIRST_PAGE 1

static QString AttributeNames[] =
{
    "Line",
    "Border"
};

static QString PositionNames[] =
{
    "BASE_TOP",
    "BASE_BOTTOM",
    "BASE_LEFT",
    "BASE_RIGHT"
};

//struct PAMItem
//{
//    PointerAttribMeta pam;
//    Positions pos;
//};

/*********************************************
 *
 * remove_group
 *
 * this removes members of a group from the
 * ldraw file held in the the ldr string
 *
 ********************************************/

static void remove_group(
    QStringList  in,      // csiParts
    QString      group,   // steps->meta.LPub.remove.group.value()
    QStringList &out)     // newCSIParts
{
  QRegExp bgt(  "^\\s*0\\s+MLCAD\\s+BTG\\s+(.*)$");
  QRegExp ldcg( "^\\s*0\\s+!?LDCAD\\s+GROUP_NXT\\s+\\[ids=([\\d\\s]+)\\].*$");
  QRegExp leogb("^\\s*0\\s+!?LEOCAD\\s+GROUP\\s+BEGIN\\s+Group\\s+(#\\d+)$",Qt::CaseInsensitive);
  QRegExp leoge("^\\s*0\\s+!?LEOCAD\\s+GROUP\\s+END$");

  bool leoRemove = false;
  int leoNest = 0;
  for (int i = 0; i < in.size(); i++) {
    QString line = in.at(i);
      // MLCad Groups
      if (line.contains(bgt)) {
         if (bgt.cap(bgt.captureCount()) == group) {
           i++;
         } else {
           out << line;
         }
      }
      // LDCad Groups
      else
      if (line.contains(ldcg)) {
          QStringList lids = ldcg.cap(ldcg.captureCount()).split(" ");
          if (lids.size() && gui->ldcadGroupMatch(group,lids)) {
              i++;
          } else {
              out << line;
          }
      }
      // LeoCAD	Group Begin
      else
      if (line.contains(leogb)) {
          if ((leogb.cap(leogb.captureCount()) == group)){
            leoRemove = true;
            i++;
          }
          else
          if (leoRemove) {
              leoNest++;
              i++;
          }
          else {
             out << line;
          }
      }
      // LeoCAD	Group End
      else
      if (line.contains(leoge)) {
          if (leoRemove) {
              if (leoNest == 0) {
                leoRemove = false;
              } else {
                leoNest--;
              }
          }
          else {
              out << line;
          }
      }
      else
      if (leoRemove) {
             i++;
      }
      else {
         out << line;
      }
      // End groups
    }

  return;
}

/*********************************************
 *
 * remove_part
 *
 * this removes members of a part from the
 * ldraw file held in the the ldr string
 *
 ********************************************/

static void remove_parttype(
    QStringList  in,
    QString      model,
    QStringList &out)
{

  model = model.toLower();

  for (int i = 0; i < in.size(); i++) {
      QString line = in.at(i);
      QStringList tokens;

      split(line,tokens);

      if (tokens.size() == 15 && tokens[0] == "1") {
          QString type = tokens[14].toLower();
          if (type != model) {
              out << line;
            }
        } else {
          out << line;
        }
    }

  return;
}

/*********************************************
 *
 * remove_name
 *
 ********************************************/

static void remove_partname(
    QStringList  in,
    QString      name,
    QStringList &out)
{
  name = name.toLower();

  for (int i = 0; i < in.size(); i++) {
      QString line = in.at(i);
      QStringList tokens;

      split(line,tokens);

      if (tokens.size() == 4 && tokens[0] == "0" &&
          tokens[1] == "LPUB" &&
          tokens[2] == "NAME") {
          QString type = tokens[3].toLower();
          if (type == name) {
              for ( ; i < in.size(); i++) {
                  line = in.at(i);
                  split(line,tokens);
                  if (tokens.size() == 15 && tokens[0] == "1") {
                      break;
                    } else {
                      out << line;
                    }
                }
            } else {
              out << line;
            }
        } else {
          out << line;
        }
    }

  return;
}

/*********************************************
 *
 * set_divider_pointers
 *
 * this processes step_group or callout divider
 * pointers and pointer attributes
 *
 ********************************************/

static void set_divider_pointers(
        Meta &curMeta,
        Where &current,
        Range *range,
        LGraphicsView *view,
        DividerType dividerType,
        int stepNum,
        Rc rct){

    QString sType = (rct == CalloutDividerRc ? "CALLOUT" : "STEPGROUP");

    Rc pRc  = (rct == CalloutDividerRc ? CalloutDividerPointerRc :
                                         StepGroupDividerPointerRc);
    Rc paRc = (rct == CalloutDividerRc ? CalloutDividerPointerAttribRc :
                                         StepGroupDividerPointerAttribRc);
    PointerAttribMeta pam = (rct == CalloutDividerRc ? curMeta.LPub.callout.divPointerAttrib :
                                                       curMeta.LPub.multiStep.divPointerAttrib);

    Where walk(current.modelName,current.lineNumber);
    walk++;

    int numLines = gui->subFileSize(walk.modelName);

    bool rd = dividerType == RangeDivider;

    int sn  = stepNum - 1; // set the previous STEP's step number

    for ( ; walk.lineNumber < numLines; walk++) {
        QString stepLine = gui->readLine(walk);
        Rc mRc = curMeta.parse(stepLine,walk);
        if (mRc == StepRc || mRc == RotStepRc) {
            break;
        } else if (mRc == pRc) {
            PointerMeta pm = (rct == CalloutDividerRc ? curMeta.LPub.callout.divPointer :
                                                        curMeta.LPub.multiStep.divPointer);
            range->appendDividerPointer(walk,pm,pam,view,sn,rd);
        } else if (mRc == paRc) {
            QStringList argv;
            split(stepLine,argv);
            pam.setValueInches(pam.parseAttributes(argv,walk));
            Pointer          *p = nullptr;
            int i               = pam.value().id - 1;
            int validIndex      = rd ? range->rangeDividerPointerList.size() - 1 :
                                       range->stepDividerPointerList.size() - 1; /*0-index*/
            if (i <= validIndex) {
                p = rd ? range->rangeDividerPointerList[i] :
                         range->stepDividerPointerList[i];
            } else {
                gui->parseError(QString("Invalid Divider pointer attribute index.<br>"
                                        "Expected value &#60;= %1, received %2")
                                        .arg(validIndex).arg(i),current);
                break;
            }
            if (p && pam.value().id == p->id) {
                pam.setAltValueInches(p->getPointerAttribInches());
                p->setPointerAttribInches(pam);
                if (rd)
                    range->rangeDividerPointerList.replace(i,p);
                else
                    range->stepDividerPointerList.replace(i,p);
            }
        }
    }
}

Step *Gui::getCurStep()
{
    return gStep;
}

/*
 * This function, drawPage, is handed the parse state going into the page
 * that is to be displayed.  It gathers up a step group, or a single step,
 * including any called out models (think recursion), but ignores non-called
 * out submodels.  It stops parsing the LDraw files when it hits end of
 * page, at which point, it calls a function to convert the parsed and
 * retained results into Qt GraphicsItems for display to the user.
 *
 * This drawPage function is only called by the findPage function.  The findPage
 * function and this drawPage function used to be one function, but doing
 * this processing in one function was problematic.  The design issue is that
 * at start of step, or multistep, you do not know the page number, because
 * the step could contain submodels that are not called out, which produce at
 * least one page each.
 *
 * findPage (is below this drawPage function in this file), is lightweight
 * in that it is much smaller that the original combined function traverse.
 * Its design goal is to find the page the user wants displayed, and present
 * the parse state of the start of page to this function drawPage.
 *
 * depends on the current page number of the parse, and the page number the
 * user wants to see.  If the current page number is lower than the "display"
 * page number, the state of meta, the parts in the submodel, the filename
 * and linenumber of the first line of page is saved.  When findPage hits end
 * of page for the "display" page, it hands the saved start of page state to
 * drawPage.  drawPage parses from start of page, creating a tree of data
 * structures representing the content of the page.  At end of page, the
 * tree is converted into Qt GraphicsItems for display.
 *
 * One thing to note is that findPage does the bulk of the LDraw file parsing
 * and is as lightweight (e.g. small) as I could make it.  Since callouts do
 * not have pages of their own (they are on the page of their parent step),
 * findPage ignores callouts.  Since findPage deals with non-callout submodels,
 * drawPage ignores non-called out submodels, and only deals with callout
 * submodels.
 *
 * After drawPage finishes gathering the page and converting the tree to
 * graphics items, it returns to findPage, which discards the parse state,
 * but continues parsing through to the last page, so we know how many pages
 * are in the building instructions.
 *
 */

Range *newRange(
    Steps  *steps,
    bool    calledOut)
{
  Range *range;

  if (calledOut) {
      range = new Range(steps,
                        steps->meta.LPub.callout.alloc.value(),
                        steps->meta.LPub.callout.freeform);
    } else {
      range = new Range(steps,
                        steps->meta.LPub.multiStep.alloc.value(),
                        steps->meta.LPub.multiStep.freeform);
    }
  return range;
}

int Gui::drawPage(
    LGraphicsView  *view,
    LGraphicsScene *scene,
    Steps          *steps,
    int             stepNum,
    QString const  &addLine,
    Where          &current,
    QStringList    &csiParts,
    QStringList    &pliParts,
    bool            isMirrored,
    QHash<QString, QStringList> &bfx,
    QList<PliPartGroupMeta> &pliPartGroups,
    bool            printing,
    bool            bfxStore2,
    QStringList    &bfxParts,
    QStringList    &ldrStepFiles,
    QStringList    &csiKeys,
    bool            assembledCallout,
    bool            calledOut)
{
  QStringList saveCsiParts;
  bool     global = true;
  QString  line, csiName;
  Callout *callout         = nullptr;
  Range   *range           = nullptr;
  Step    *step            = nullptr;
  int      numLines        = ldrawFile.size(current.modelName);
  bool     pliIgnore       = false;
  bool     partIgnore      = false;
  bool     synthBegin      = false;
  bool     multiStep       = false;
  bool     partsAdded      = false;
  bool     coverPage       = false;
  bool     bfxStore1       = false;
  bool     bfxLoad         = false;
  bool     firstStep       = true;
  bool     noStep          = false;
  bool     rotateIcon      = false;
  bool     assemAnnotation = false;
  bool     displayCount    = false;
  bool     itemDirection   = false;
  int      countInstances  = steps->meta.LPub.countInstance.value();

  DividerType dividerType  = NoDivider;

  PagePointer *pagePointer = nullptr;
  QMap<Positions, PagePointer*> pagePointers;

  steps->isMirrored = isMirrored;
  steps->setTopOfSteps(current);

  QList<InsertMeta> inserts;

  Where topOfStep = current;
  Rc gprc = OkRc;
  Rc rc;
  int retVal = 0;

  page.coverPage = false;

  QStringList calloutParts;

  QElapsedTimer pageRenderTimer;
  pageRenderTimer.start();

  auto drawPageElapsedTime = [this, &partsAdded, &pageRenderTimer](){
    QString pageRenderMessage = QString("%1 ").arg(VER_PRODUCTNAME_STR);
    if (!page.coverPage && partsAdded) {
      pageRenderMessage += QString("using %1 ").arg(Render::getRenderer());
      QString renderAttributes;
      if (Render::getRenderer() == RENDERER_LDVIEW) {
        if (Preferences::enableLDViewSingleCall)
          renderAttributes += QString("Single Call");
        if (Preferences::enableLDViewSnaphsotList)
          renderAttributes += QString(", Snapshot List");
      }
      if (!renderAttributes.isEmpty())
        pageRenderMessage += QString("(%1) ").arg(renderAttributes);
      pageRenderMessage += QString("render ");
    }
    pageRenderMessage += QString("rendered page %1. %2")
                                 .arg(displayPageNum)
                                 .arg(gui->elapsedTime(pageRenderTimer.elapsed()));
    emit gui->messageSig(LOG_TRACE, pageRenderMessage);
  };

  // set page header/footer width
  float pW;
  int which;
  if (callout){
      which = callout->meta.LPub.page.orientation.value() == Landscape ? 1 : 0;
      pW = callout->meta.LPub.page.size.value(which);
      callout->meta.LPub.page.pageHeader.size.setValue(0,pW);
      callout->meta.LPub.page.pageFooter.size.setValue(0,pW);
    } else {
      which = steps->meta.LPub.page.orientation.value() == Landscape ? 1 : 0;
      pW = steps->meta.LPub.page.size.value(which);
      steps->meta.LPub.page.pageHeader.size.setValue(0,pW);
      steps->meta.LPub.page.pageFooter.size.setValue(0,pW);
    }

  emit messageSig(LOG_INFO, "Processing draw page for " + current.modelName + "...");

  /*
   * do until end of page
   */
  for ( ; current <= numLines; current++) {

      // load initial meta values

      Meta   &curMeta = callout ? callout->meta : steps->meta;

      QStringList tokens;

      // If we hit end of file we've got to note end of step

      if (current >= numLines) {
          line.clear();
          gprc = EndOfFileRc;
          tokens << "0";

          // not end of file, so get the next LDraw line

        } else {

          // read the line from the ldrawFile db

          line = ldrawFile.readLine(current.modelName,current.lineNumber);
          split(line,tokens);
        }

      // STEP - Process part type
      if (tokens.size() == 15 && tokens[0] == "1") {

          QString color = tokens[1];
          QString type  = tokens[tokens.size()-1];

          if (color == "16") {
              QStringList addTokens;
              split(addLine,addTokens);
              if (addTokens.size() == 15) {
                  tokens[1] = addTokens[1];
                }
              line = tokens.join(" ");
              color = tokens[1];
            }

          csiParts << line;
          partsAdded = true;

          // STEP - Allocate STEP

          /* since we have a part usage, we have a valid STEP */

          if (step == nullptr  && ! noStep) {
              if (range == nullptr) {
                  range = newRange(steps,calledOut);
                  steps->append(range);
                }

              step = new Step(topOfStep,
                              range,
                              stepNum,
                              curMeta,
                              calledOut,
                              multiStep);

              range->append(step);
            }

          // STEP - Allocate PLI

          /* check if part is on excludedPart.lst and set pliIgnore*/

          if (ExcludedParts::hasExcludedPart(type))
              pliIgnore = true;

          /* addition of ldraw parts */

          if (curMeta.LPub.pli.show.value()
              && ! pliIgnore
              && ! partIgnore
              && ! synthBegin) {
              QString colorType = color+type;

              if (! isSubmodel(type) || curMeta.LPub.pli.includeSubs.value()) {

                  /*  check if substitute part exist and replace */
                  if(PliSubstituteParts::hasSubstitutePart(type)) {

                      QStringList substituteToken;
                      split(line,substituteToken);
                      QString substitutePart = type;

                      if (PliSubstituteParts::getSubstitutePart(substitutePart)){
                          substituteToken[substituteToken.size()-1] = substitutePart;
                        }

                      line = substituteToken.join(" ");
                    }

                  if (bfxStore2 && bfxLoad) {
                      for (int i = 0; i < bfxParts.size(); i++) {
                          if (bfxParts[i] == colorType) {
                              bfxParts.removeAt(i);
                              break;
                            }
                        }

                      // Danny: the following condition should help LPUB to remove automatically from PLI the parts in the buffer,
                      // but does not work if two buffers are used one after another in a multi step page.
                      // Better to make the user use the !LPUB PLI BEGIN IGN / END

                      //if ( ! removed )  {
                      pliParts << Pli::partLine(line,current,steps->meta);
                      //}
                    } else {

                      pliParts << Pli::partLine(line,current,steps->meta);
                    }
                }

              // bfxStore1 goes true when we've seen BFX STORE the first time
              // in a sequence of steps.  This used to be commented out which
              // means it didn't work in some cases, but we need it in step
              // group cases, so.... bfxStore1 && multiStep (was just bfxStore1)
              if (bfxStore1 && (multiStep || calledOut)) {
                  bfxParts << colorType;
                }
            } // STEP - Process shown PLI parts

          /* if it is a called out sub-model, then process it */

          if (ldrawFile.isSubmodel(type) && callout && ! noStep) {

              CalloutBeginMeta::CalloutMode calloutMode = callout->meta.LPub.callout.begin.value();

//              logDebug() << "CALLOUT MODE: " << (calloutMode == CalloutBeginMeta::Unassembled ? "Unassembled" :
//                                                 calloutMode == CalloutBeginMeta::Rotated ? "Rotated" : "Assembled");

              /* we are a callout, so gather all the steps within the callout */
              /* start with new meta, but no rotation step */

              QString thisType = type;

             /* t.s. Rotated or assembled callout here (treated like a submodel) */
              if ((assembledCallout = calloutMode != CalloutBeginMeta::Unassembled)) {

                  /* So, we process these callouts in-line, not when we finally hit the STEP or
                     ROTSTEP that ends this processing, but for ASSEMBLED or ROTATED
                     callouts, the ROTSTEP state affects the results, so we have to search
                     forward until we hit STEP or ROTSTEP to know how the submodel might
                     want to be rotated.  Also, for submodel's who's scale is different
                     than their parent's scale, we want to scan ahead and find out the
                     parent's scale and "render" the submodels at the parent's scale */

                  Meta tmpMeta = curMeta;
                  Where walk = current;
                  for (++walk; walk < numLines; ++walk) {
                      QStringList tokens;
                      QString scanLine = ldrawFile.readLine(walk.modelName,walk.lineNumber);
                      split(scanLine,tokens);
                      if (tokens.size() > 0 && tokens[0] == "0") {
                          Rc rc = tmpMeta.parse(scanLine,walk,false);
                          if (rc == StepRc || rc == RotStepRc) {
                              break;
                            }
                        }
                    }

                  if (calloutMode == CalloutBeginMeta::Rotated) {
                      // When renderers apply CA rotation, set cameraAngles to 0 so only ROTSTEP is sent to renderers.
                      if (! Preferences::applyCALocally)
                          callout->meta.LPub.assem.cameraAngles.setValues(0.0, 0.0);
                  }
                  callout->meta.rotStep = tmpMeta.rotStep;
                  callout->meta.LPub.assem.modelScale = tmpMeta.LPub.assem.modelScale;
                  // The next command applies the rotation due to line, but not due to callout->meta.rotStep
                  thisType = callout->wholeSubmodel(callout->meta,type,line,0);
                }

              if (callout->bottom.modelName != thisType) {

                  Where current2(thisType,0);
                  skipHeader(current2);
                  if (calloutMode == CalloutBeginMeta::Assembled) {
                      // In this case, no additional rotation should be applied to the submodel
                      callout->meta.rotStep.clear();
                  }
                  SubmodelStack tos(current.modelName,current.lineNumber,stepNum);
                  callout->meta.submodelStack << tos;
                  Meta saveMeta = callout->meta;
                  callout->meta.LPub.pli.constrain.resetToDefault();

                  step->append(callout);

                  calloutParts.clear();
                  QStringList csiParts2;

                  QHash<QString, QStringList> calloutBfx;

                  int rc;
                  rc = drawPage(
                        view,
                        scene,
                        callout,
                        1,
                        line,
                        current2,
                        csiParts2,
                        calloutParts,
                        ldrawFile.mirrored(tokens),
                        calloutBfx,
                        pliPartGroups,
                        printing,
                        bfxStore2,
                        bfxParts,
                        ldrStepFiles,
                        csiKeys,
                        assembledCallout,
                        true);

                  callout->meta = saveMeta;

                  if (callout->meta.LPub.pli.show.value() &&
                      ! callout->meta.LPub.callout.pli.perStep.value() &&
                      ! pliIgnore && ! partIgnore && ! synthBegin &&
                      calloutMode == CalloutBeginMeta::Unassembled) {

                      pliParts += calloutParts;
                    }

                  if (rc != 0) {
                      steps->placement = steps->meta.LPub.assem.placement;
                      return rc;
                    }
                } else {
                  callout->instances++;
                  if (calloutMode == CalloutBeginMeta::Unassembled) {
                      pliParts += calloutParts;
                    }
                }

              /* remind user what file we're working on */
              emit messageSig(LOG_STATUS, "Processing " + current.modelName + "...");

            } // STEP - Process called out submodel

          // Set flag to display submodel at first submodel step
          if (step && steps->meta.LPub.subModel.show.value()) {
              bool topModel       = (topLevelFile() == topOfStep.modelName);
              bool showTopModel   = (steps->meta.LPub.subModel.showTopModel.value());
              bool showStepOk     = (steps->meta.LPub.subModel.showStepNum.value() == stepNum || stepNum == 1);
              step->placeSubModel = (showStepOk && !calledOut && (!topModel || showTopModel));
          }

        }
      // STEP - Process line, triangle, or polygon type
      else if ((tokens.size() == 8  && tokens[0] == "2") ||
               (tokens.size() == 11 && tokens[0] == "3") ||
               (tokens.size() == 14 && tokens[0] == "4") ||
               (tokens.size() == 14 && tokens[0] == "5")) {

          /* we've got a line, triangle or polygon, so add it to the list */
          /* and make sure we know we have a step */

          csiParts << line;
          partsAdded = true;

          if (step == nullptr && ! noStep) {
              if (range == nullptr) {
                  range = newRange(steps,calledOut);
                  steps->append(range);
                }

              step = new Step(topOfStep,
                              range,
                              stepNum,
                              steps->meta,
                              calledOut,
                              multiStep);

              range->append(step);
            }
        }
      // STEP - Process meta command
      else if ( (tokens.size() && tokens[0] == "0") || gprc == EndOfFileRc) {

          /* must be meta-command (or comment) */

          if (global && tokens.contains("!LPUB") && tokens.contains("GLOBAL")) {
              topOfStep = current;
            } else {
              global = false;
            }

          if (gprc == EndOfFileRc) {
              rc = gprc;
            } else {
              rc = curMeta.parse(line,current,true);
            }

          /* handle specific meta-commands */

          switch (rc) {
            /* toss it all out the window, per James' original plan */
            case ClearRc:
              pliParts.clear();
              csiParts.clear();
              //steps->freeSteps();  // had to remove this because it blows steps following clear
              // in step group.
              break;

              /* Buffer exchange */
            case BufferStoreRc:
              bfx[curMeta.bfx.value()] = csiParts;
              bfxStore1 = true;
              bfxParts.clear();
              break;

            case BufferLoadRc:
              csiParts = bfx[curMeta.bfx.value()];
              if (!partsAdded) {
                  ldrawFile.setPrevStepPosition(current.modelName,csiParts.size());
                  //qDebug() << "Model:" << current.modelName << ", Step:"  << stepNum << ", bfx Set Fade Position:" << csiParts.size();
              }
              bfxLoad = true;
              break;

            case MLCadGroupRc:
            case LDCadGroupRc:
            case LeoCadGroupBeginRc:
            case LeoCadGroupEndRc:
              csiParts << line;
              break;

            case IncludeRc:
              include(curMeta);
              break;

              /* substitute part/parts with this */

            case PliBeginSub1Rc:
              if (pliIgnore) {
                  parseError("Nested PLI BEGIN/ENDS not allowed",current);
                }
              if (steps->meta.LPub.pli.show.value() &&
                  ! pliIgnore &&
                  ! partIgnore &&
                  ! synthBegin) {

                  SubData subData = curMeta.LPub.pli.begin.sub.value();
                  QString addPart = QString("1 0  0 0 0  0 0 0 0 0 0 0 0 0 %1") .arg(subData.part);
                  pliParts << Pli::partLine(addPart,current,curMeta);
                }

              if (step == nullptr && ! noStep) {
                  if (range == nullptr) {
                      range = newRange(steps,calledOut);
                      steps->append(range);
                    }
                  step = new Step(topOfStep,
                                  range,
                                  stepNum,
                                  curMeta,
                                  calledOut,
                                  multiStep);

                  range->append(step);
                }
              pliIgnore = true;
              break;

              /* substitute part/parts with this */
            case PliBeginSub2Rc:
            case PliBeginSub3Rc:
            case PliBeginSub4Rc:
            case PliBeginSub5Rc:
            case PliBeginSub6Rc:
              if (pliIgnore) {
                  parseError("Nested PLI BEGIN/ENDS not allowed",current);
                }
              if (steps->meta.LPub.pli.show.value() &&
                  ! pliIgnore &&
                  ! partIgnore &&
                  ! synthBegin) {

                  SubData subData = curMeta.LPub.pli.begin.sub.value();
                  QString addPart = QString("1 %1  0 0 0  0 0 0 0 0 0 0 0 0 %2") .arg(subData.color) .arg(subData.part);
                  pliParts << Pli::partLine(addPart,current,curMeta);
                }

              if (step == nullptr && ! noStep) {
                  if (range == nullptr) {
                      range = newRange(steps,calledOut);
                      steps->append(range);
                    }
                  step = new Step(topOfStep,
                                  range,
                                  stepNum,
                                  curMeta,
                                  calledOut,
                                  multiStep);

                  range->append(step);
                }
              pliIgnore = true;
              break;

              /* do not put subsequent parts into PLI */
            case PliBeginIgnRc:
              if (pliIgnore) {
                  parseError("Nested PLI BEGIN/ENDS not allowed",current);
                }
              pliIgnore = true;
              break;
            case PliEndRc:
              if ( ! pliIgnore) {
                  parseError("PLI END with no PLI BEGIN",current);
                }
              pliIgnore = false;
              curMeta.LPub.pli.begin.sub.clearAttributes();
              break;

            case AssemAnnotationIconRc:
              if (assemAnnotation) {
                  parseError("Nested ASSEM ANNOTATION ICON not allowed",current);
              } else {
                  if (step && ! gui->exportingObjects())
                      step->appendCsiAnnotation(current,curMeta.LPub.assem.annotation/*,view*/);
                  assemAnnotation = false;
              }
              break;

              /* discard subsequent parts, and don't create CSI's for them */
            case PartBeginIgnRc:
            case MLCadSkipBeginRc:
              if (partIgnore) {
                  parseError("Nested BEGIN/ENDS not allowed",current);
                }
              partIgnore = true;
              break;

            case PartEndRc:
            case MLCadSkipEndRc:
              if (! partIgnore) {
                  parseError("Ignore ending with no ignore begin",current);
                }
              partIgnore = false;
              break;

            case SynthBeginRc:
              if (synthBegin) {
                  parseError("Nested LSynth BEGIN/ENDS not allowed",current);
                }
              synthBegin = true;
              break;

            case SynthEndRc:
              if ( ! synthBegin) {
                  parseError("LSynth ignore ending with no ignore begin",current);
                }
              synthBegin = false;
              break;

              /* remove a group or all instances of a part type */
            case GroupRemoveRc:
            case RemoveGroupRc:
            case RemovePartRc:
            case RemoveNameRc:
              {
                QStringList newCSIParts;

                if (rc == RemoveGroupRc) {
                    remove_group(csiParts,steps->meta.LPub.remove.group.value(),newCSIParts);
                  } else if (rc == RemovePartRc) {
                    remove_parttype(csiParts, steps->meta.LPub.remove.parttype.value(),newCSIParts);
                  } else {
                    remove_partname(csiParts, steps->meta.LPub.remove.partname.value(),newCSIParts);
                  }
                csiParts = newCSIParts;

                if (step == nullptr && ! noStep) {
                    if (range == nullptr) {
                        range = newRange(steps,calledOut);
                        steps->append(range);
                      }
                    step = new Step(topOfStep,
                                    range,
                                    stepNum,
                                    curMeta,
                                    calledOut,
                                    multiStep);

                    range->append(step);
                  }
              }
              break;

            case ReserveSpaceRc:
              /* since we have a part usage, we have a valid step */
              if (calledOut || multiStep) {
                  step = nullptr;
                  Reserve *reserve = new Reserve(current,steps->meta.LPub);
                  if (range == nullptr) {
                      range = newRange(steps,calledOut);
                      steps->append(range);
                    }
                  range->append(reserve);
                }
              break;

            case InsertFinalModelRc:
              {
                if (curMeta.LPub.fadeStep.fadeStep.value() || curMeta.LPub.highlightStep.highlightStep.value()){
                    // this is not a step but it's necessary to use the step object to place the model
                    // increment the step number down - so basically use previous number for step
                    // do this before creating the step so we can use in the file name during
                    // csi generation to indicate this step file is not an actual step - just a model display
                    stepNum--;
                    if (step == nullptr) {
                        if (range == nullptr) {
                            range = newRange(steps,calledOut);
                            steps->append(range);
                          }
                        step = new Step(topOfStep,
                                        range,
                                        stepNum,
                                        curMeta,
                                        calledOut,
                                        multiStep);

                        step->modelDisplayOnlyStep = true;

                        range->append(step);
                      }
                  }
              }
              break;

            case InsertCoverPageRc:
              {
                coverPage = true;
                page.coverPage = true;
                QRegExp backCoverPage("^\\s*0\\s+!LPUB\\s+.*BACK");
                if (line.contains(backCoverPage)){
                    page.backCover  = true;
                    page.frontCover = false;
                  } else {
                    page.frontCover = true;
                    page.backCover  = false;
                  }
                // nothing to display in 3D Window
                if (! exporting())
                  emit clearViewerWindowSig();
              }
            case InsertPageRc:
              {
                partsAdded = true;

                // nothing to display in 3D Window
                if (! exporting())
                  emit clearViewerWindowSig();
              }
              break;

            case InsertRc:
              {
                 InsertData insertData = curMeta.LPub.insert.value();
                 if (insertData.type == InsertData::InsertRotateIcon) { // indicate that we have a rotate icon for this step
                     rotateIcon = (calledOut && assembledCallout ? false : true);
                 }
                 if (insertData.type == InsertData::InsertBom) {
                     // nothing to display in 3D Window
                     if (! exporting())
                         emit clearViewerWindowSig();
                 }
                 inserts.append(curMeta.LPub.insert);                  // these are always placed before any parts in step
              }
              break;

           case SceneItemDirectionRc:
              itemDirection = true;
              break;

           case PliPartGroupRc:
                curMeta.LPub.pli.pliPartGroup.setWhere(current);
                pliPartGroups.append(curMeta.LPub.pli.pliPartGroup);
              break;

            case PagePointerRc:
              {
                  if (pagePointer) {
                      parseError("Nested page pointers not allowed within the same page",current);
                  } else {
                      Positions position    = PP_LEFT;
                      PointerMeta ppm       = curMeta.LPub.page.pointer;
                      PointerAttribMeta pam = curMeta.LPub.page.pointerAttrib;
                      PointerAttribData pad = pam.valueInches();
                      RectPlacement currRp  = curMeta.LPub.page.pointer.value().rectPlacement;

                      if (currRp == TopInside)
                          position = PP_TOP;
                      else
                      if (currRp == BottomInside)
                          position = PP_BOTTOM;
                      else
                      if (currRp == LeftInside)
                          position = PP_LEFT;
                      else
                      if (currRp == RightInside)
                          position = PP_RIGHT;

                      // DEBUG
                      bool newPP = true;
                      pagePointer = pagePointers.value(position);
                      if (pagePointer) {
                          newPP = false;
                          pad.id     = pagePointer->pointerList.size() + 1;
                          pad.parent = PositionNames[position];
                          pam.setValueInches(pad);
                          pagePointer->appendPointer(current,ppm,pam);
                          pagePointers.remove(position);
                          pagePointers.insert(position,pagePointer);
                      } else {
                          pagePointer = new PagePointer(&curMeta,view);
                          pagePointer->parentStep = step;
                          pagePointer->setTopOfPagePointer(current);
                          pagePointer->setBottomOfPagePointer(current);
                          if (multiStep){
                              pagePointer->parentRelativeType = StepGroupType;
                          } else
                            if (calledOut){
                              pagePointer->parentRelativeType = CalloutType;
                          } else {
                              pagePointer->parentRelativeType = step->relativeType;
                          }
                          PlacementMeta placementMeta;
                          placementMeta.setValue(currRp, PageType);
                          pagePointer->placement = placementMeta;

                          pad.id     = 1;
                          pad.parent = PositionNames[position];
                          pam.setValueInches(pad);
                          pagePointer->appendPointer(current,ppm,pam);
                          pagePointers.insert(position,pagePointer);
                      }
                      pagePointer = nullptr;
                  }
              }
              break;

            case PagePointerAttribRc:
              {
                  PointerAttribMeta pam = curMeta.LPub.page.pointerAttrib;
                  pam.setValueInches(pam.parseAttributes(tokens,current));

                  Positions position = PP_LEFT;
                  if (pam.value().parent == "BASE_TOP")
                      position = PP_TOP;
                  else
                  if (pam.value().parent == "BASE_BOTTOM")
                      position = PP_BOTTOM;
                  else
                  if (pam.value().parent == "BASE_LEFT")
                      position = PP_LEFT;
                  else
                  if (pam.value().parent == "BASE_RIGHT")
                      position = PP_RIGHT;

                  PagePointer *pp = pagePointers.value(position);
                  if (pp) {
                      Pointer          *p = nullptr;
                      int i               = pam.value().id - 1;
                      int validIndex      = pp->pointerList.size() - 1; /*0-index*/
                      if (i <= validIndex) {
                          p = pp->pointerList[i];
                      } else {
                          parseError(QString("Invalid Page pointer attribute index.<br>"
                                             "Expected value &#60;= %1, received %2")
                                             .arg(validIndex).arg(i),current);
                          break;
                      }
                      if (p && pam.value().id == p->id) {
                          pam.setAltValueInches(p->getPointerAttribInches());
                          p->setPointerAttribInches(pam);
                          pp->pointerList.replace(i,p);
                          pagePointers.remove(position);
                          pagePointers.insert(position,pp);
                      }
                  } else {
                      emit messageSig(LOG_ERROR, QString("Page Position %1 does not exist.").arg(PositionNames[position]));
                  }
              }
              break;

            case CalloutBeginRc:
              if (callout) {
                  parseError("Nested CALLOUT not allowed within the same file",current);
                } else {
                  callout = new Callout(curMeta,view);
                  callout->setTopOfCallout(current);
                }
              break;

            case CalloutPointerRc:
              if (callout) {
                  callout->appendPointer(current,
                                         curMeta.LPub.callout.pointer,
                                         curMeta.LPub.callout.pointerAttrib);
                }
              break;

          case CalloutPointerAttribRc:
              if (callout) {
                  PointerAttribMeta pam = curMeta.LPub.callout.pointerAttrib;
                  pam.setValueInches(pam.parseAttributes(tokens,current));
                  Pointer          *p = nullptr;
                  int i               = pam.value().id - 1;
                  int validIndex      = callout->pointerList.size() - 1; /*0-index*/
                  if (i <= validIndex) {
                      p = callout->pointerList[i];
                  } else {
                      parseError(QString("Invalid Callout pointer attribute index.<br>"
                                         "Expected value &#60;= %1, received %2")
                                         .arg(validIndex).arg(i),current);
                      break;
                  }
                  if (p && pam.value().id == p->id) {
                      pam.setAltValueInches(p->getPointerAttribInches());
                      p->setPointerAttribInches(pam);
                      callout->pointerList.replace(i,p);
                  }
              }
            break;

            case CalloutDividerRc:
              if (range) {
                  range->sepMeta = curMeta.LPub.callout.sep;
                  dividerType = (line.contains("STEPS") ? StepDivider : RangeDivider);
                  set_divider_pointers(curMeta,current,range,view,dividerType,stepNum,CalloutDividerRc);
                  if (dividerType != StepDivider) {
                      range = nullptr;
                      step = nullptr;
                  }
                }
              break;

            case CalloutEndRc:
              if ( ! callout) {
                  parseError("CALLOUT END without a CALLOUT BEGIN",current);
                }
              else
              if (! step) {
                  parseError("CALLOUT does not contain a valid STEP",current);
               }
              else
                {
                  callout->parentStep = step;
                  if (multiStep) {
                      callout->parentRelativeType = StepGroupType;
                    } else if (calledOut) {
                      callout->parentRelativeType = CalloutType;
                    } else {
                      callout->parentRelativeType = step->relativeType;
                    }
                  // set csi annotations - callout
//                  if (! gui->exportingObjects())
//                      callout->setCsiAnnotationMetas();
                  callout->pli.clear();
                  callout->placement = curMeta.LPub.callout.placement;
                  callout->margin = curMeta.LPub.callout.margin;
                  callout->setBottomOfCallout(current);
                  callout = nullptr;
                }
              break;

            case StepGroupBeginRc:
              if (calledOut) {
                  parseError("MULTI_STEP not allowed inside callout models",current);
              } else {
                  if (multiStep) {
                      parseError("Nested MULTI_STEP not allowed",current);
                  }
                  multiStep = true;
              }
              steps->relativeType = StepGroupType;
              break;

            case StepGroupDividerRc:
              if (range) {
                  range->sepMeta = steps->meta.LPub.multiStep.sep;
                  dividerType = (line.contains("STEPS") ? StepDivider : RangeDivider);
                  set_divider_pointers(curMeta,current,range,view,dividerType,stepNum,StepGroupDividerRc);
                  if (dividerType != StepDivider) {
                      range = nullptr;
                      step = nullptr;
                  }
                }
              break;

              /* finished off a multiStep */
            case StepGroupEndRc:
              if (multiStep && steps->list.size()) {
                  // save the current meta as the meta for step group
                  // PLI for non-pli-per-step
                  if (partsAdded) {
                      parseError("Expected STEP before MULTI_STEP END", current);
                    }
                  multiStep = false;

                  if (pliParts.size() && steps->meta.LPub.multiStep.pli.perStep.value() == false) {
                      PlacementData placementData = steps->stepGroupMeta.LPub.multiStep.pli.placement.value();
                      // Override default, which is for PliPerStep true
                      if (placementData.relativeTo != PageType &&
                          placementData.relativeTo != StepGroupType) {
                          placementData.relativeTo  = PageType;
                          placementData.placement   = TopLeft;
                          placementData.preposition = Inside;
                          placementData.offsets[0]  = 0;
                          placementData.offsets[1]  = 0;
                          steps->stepGroupMeta.LPub.multiStep.pli.placement.setValue(placementData);
                        }
                      steps->pli.bom = false;
                      steps->pli.setParts(pliParts,pliPartGroups,steps->stepGroupMeta);

                      emit messageSig(LOG_STATUS, "Add PLI images for multi-step page " + current.modelName);

                      steps->pli.sizePli(&steps->stepGroupMeta, StepGroupType, false);
                    }
                  pliParts.clear();
                  pliPartGroups.clear();

                  /* this is a page we're supposed to process */

                  steps->placement = steps->meta.LPub.multiStep.placement;

                  showLine(steps->topOfSteps());

                  bool endOfSubmodel =
                          steps->meta.LPub.contStepNumbers.value() ?
                              steps->meta.LPub.contModelStepNum.value() >= ldrawFile.numSteps(current.modelName) :
                              stepNum - 1 >= ldrawFile.numSteps(current.modelName);

                  // set csi annotations - multistep
//                  if (! gui->exportingObjects())
//                      steps->setCsiAnnotationMetas();

                  // get the number of submodel instances in the model file
                  int countInstanceOverride = steps->meta.LPub.page.countInstanceOverride.value();
                  int instances = countInstanceOverride ? countInstanceOverride :
                                                          ldrawFile.instances(current.modelName, isMirrored);
                  // count the instances accordingly
                  displayCount= countInstances && instances > 1;
                  if (displayCount && countInstances != CountAtTop && !countInstanceOverride) {
                      MetaItem mi;
                      if (countInstances == CountAtStep)
                          instances = mi.countInstancesInStep(&steps->meta, current.modelName);
                      else
                      if (countInstances > CountFalse && countInstances < CountAtStep)
                          instances = mi.countInstancesInModel(&steps->meta, current.modelName);
                  }

                  Page *page = dynamic_cast<Page *>(steps);
                  if (page) {
                      page->inserts              = inserts;
                      page->instances            = instances;
                      page->displayInstanceCount = displayCount;
                      page->pagePointers         = pagePointers;
                      page->setItemDirection     = itemDirection;
                    }

                  emit messageSig(LOG_STATUS, "Add CSI images for multi-step page " + current.modelName);

                  if (renderer->useLDViewSCall() && ldrStepFiles.size() > 0){

                      QElapsedTimer timer;
                      timer.start();
                      QString empty("");

                      // set camera
                      steps->meta.LPub.assem.cameraAngles            = step->csiCameraMeta.cameraAngles;
                      steps->meta.LPub.assem.cameraDistNative.factor = step->csiCameraMeta.cameraDistNative.factor;
                      steps->meta.LPub.assem.modelScale              = step->csiCameraMeta.modelScale;
                      steps->meta.LPub.assem.cameraFoV               = step->csiCameraMeta.cameraFoV;
                      steps->meta.LPub.assem.zfar                    = step->csiCameraMeta.zfar;
                      steps->meta.LPub.assem.znear                   = step->csiCameraMeta.znear;
                      // set the extra renderer parms
                      steps->meta.LPub.assem.ldviewParms =
                           Render::getRenderer() == RENDERER_LDVIEW ? step->ldviewParms :
                           Render::getRenderer() == RENDERER_LDGLITE ? step->ldgliteParms :
                                         /*POV scene file generator*/  step->ldviewParms ;
                      if (Preferences::preferredRenderer == RENDERER_POVRAY)
                          steps->meta.LPub.assem.povrayParms = step->povrayParms;

                      int rc = renderer->renderCsi(empty,ldrStepFiles,csiKeys,empty,steps->meta);
                      if (rc != 0) {
                          emit messageSig(LOG_ERROR,QMessageBox::tr("Render CSI images failed."));
                          return rc;
                        }

                      emit gui->messageSig(LOG_INFO,
                                          QString("%1 CSI (Single Call) render took "
                                                  "%2 milliseconds to render %3 [Step %4] %5 "
                                                  "for %6 step group on page %7.")
                                             .arg(Render::getRenderer())
                                             .arg(timer.elapsed())
                                             .arg(ldrStepFiles.size())
                                             .arg(stepNum)
                                             .arg(ldrStepFiles.size() == 1 ? "image" : "images")
                                             .arg(calledOut ? "called out," : "simple,")
                                             .arg(stepPageNum));
                    }

                  addGraphicsPageItems(steps, coverPage, endOfSubmodel, view, scene, printing);

                  drawPageElapsedTime();
                  return HitEndOfPage;
                }
              inserts.clear();
              pagePointers.clear();
              break;

            case NoStepRc:
              noStep = true;
              break;

              /* we've hit some kind of step, or implied step and end of file */
            case EndOfFileRc:
            case RotStepRc:
            case StepRc:

              // STEP - special case of no parts added, but BFX load and not NOSTEP
              if (! partsAdded && bfxLoad && ! noStep) {
                  if (step == nullptr) {
                      if (range == nullptr) {
                          range = newRange(steps,calledOut);
                          steps->append(range);
                        }
                      step = new Step(topOfStep,
                                      range,
                                      stepNum,
                                      curMeta,
                                      calledOut,
                                      multiStep);

                      range->append(step);
                    }

                  emit messageSig(LOG_INFO, "Processing CSI bfx load special case for " + topOfStep.modelName + "...");
                  csiName = step->csiName();
                  (void) step->createCsi(
                        isMirrored ? addLine : "1 color 0 0 0 1 0 0 0 1 0 0 0 1 foo.ldr",
                        saveCsiParts = configureModelStep(csiParts, stepNum, topOfStep),
                        &step->csiPixmap,
                        steps->meta,
                        bfxLoad);

                  if (renderer->useLDViewSCall() && ! step->ldrName.isNull()) {
                      ldrStepFiles << step->ldrName;
                      csiKeys << step->csiKey; // No parts to process
                    }

                  partsAdded = true; // OK, so this is a lie, but it works
                }

              // STEP - normal case of parts added, and not NOSTEP
              if (partsAdded && ! noStep) {

                  if (firstStep) {
                      steps->stepGroupMeta = curMeta;
                      firstStep = false;
                  }

                  if (pliIgnore) {
                      parseError("PLI BEGIN then STEP. Expected PLI END",current);
                      pliIgnore = false;
                  }
                  if (partIgnore) {
                      parseError("PART BEGIN then STEP. Expected PART END",current);
                      partIgnore = false;
                  }
                  if (synthBegin) {
                      parseError("SYNTH BEGIN then STEP. Expected SYNTH_END",current);
                      synthBegin = false;
                  }

                  bool pliPerStep;

                  if (multiStep && steps->meta.LPub.multiStep.pli.perStep.value()) {
                      pliPerStep = true;
                  } else if (calledOut && steps->meta.LPub.callout.pli.perStep.value()) {
                      pliPerStep = true;
                  } else if ( ! multiStep && ! calledOut && steps->meta.LPub.stepPli.perStep.value()) {
                      pliPerStep = true;
                  } else {
                      pliPerStep = false;
                  }

                  if (step) {

                      Page *page = dynamic_cast<Page *>(steps);
                      if (page) {
                          page->inserts              = inserts;
                          page->pagePointers         = pagePointers;
                          page->modelDisplayOnlyStep = step->modelDisplayOnlyStep;
                          page->setItemDirection     = itemDirection;
                      }

                      PlacementType relativeType = SingleStepType;
                      if (pliPerStep) {
                          if (multiStep) {
                              relativeType = StepGroupType;
                          } else if (calledOut) {
                              relativeType = CalloutType;
                          } else {
                              relativeType = SingleStepType;
                          }

                          step->pli.setParts(pliParts,pliPartGroups,steps->meta);
                          pliParts.clear();
                          pliPartGroups.clear();

                          emit messageSig(LOG_INFO, "Add step PLI for " + topOfStep.modelName + "...");

                          step->pli.sizePli(&steps->meta,relativeType,pliPerStep);
                      }

                      if (step->placeSubModel){
                          emit messageSig(LOG_INFO, "Set first step submodel display for " + topOfStep.modelName + "...");

                          // get the number of submodel instances in the model file
                          int countInstanceOverride = steps->meta.LPub.page.countInstanceOverride.value();
                          int instances = countInstanceOverride ? countInstanceOverride :
                                                                  ldrawFile.instances(current.modelName, isMirrored);
                          displayCount = steps->meta.LPub.subModel.showInstanceCount.value() && instances > 1;
                          if (displayCount && countInstances != CountAtTop && !countInstanceOverride) {
                              MetaItem mi;
                              if (countInstances == CountAtStep)
                                  instances = mi.countInstancesInStep(&steps->meta, current.modelName);
                              else
                              if (countInstances > CountFalse && countInstances < CountAtStep)
                                  instances = mi.countInstancesInModel(&steps->meta, current.modelName);
                          }
                          if (multiStep) {
                              relativeType = StepGroupType;
                          } else if (calledOut) {
                              relativeType = CalloutType;
                          } else {
                              relativeType = SingleStepType;
                          }
                          steps->meta.LPub.subModel.instance.setValue(instances);
                          step->subModel.setSubModel(current.modelName,steps->meta);
                          step->subModel.displayInstanceCount = displayCount;
                          if (step->subModel.sizeSubModel(&steps->meta,relativeType) != 0)
                              emit messageSig(LOG_ERROR, "Failed to set first step submodel display for " + topOfStep.modelName + "...");
                      } else {
                          step->subModel.clear();
                      }

                      switch (dividerType){
                      // for range divider, we set the dividerType for the last STEP of the previous RANGE.
                      case RangeDivider:
                          if (steps && steps->list.size() > 1) {
                              int i = steps->list.size()-2;           // previous range index
                              Range *previousRange = dynamic_cast<Range *>(steps->list[i]);
                              if (previousRange){
                                  i = previousRange->list.size()-1;   // last step index in previous range
                                  Step *lastStep = dynamic_cast<Step *>(previousRange->list[i]);
                                  if (lastStep)
                                    lastStep->dividerType = dividerType;
                              }
                          }
                          break;
                      // for steps divider, we set the dividerType for the previous STEP
                      case StepDivider:
                          if (range && range->list.size() > 1) {
                             int i = range->list.size()-2;            // previous step index
                             Step *previousStep = dynamic_cast<Step *>(range->list[i]);
                             if (previousStep)
                               previousStep->dividerType = dividerType;
                          }
                          break;
                      // no divider
                      default:
                          step->dividerType = dividerType;
                      break;
                      }

                      step->placeRotateIcon = rotateIcon;

                      emit messageSig(LOG_STATUS, "Processing CSI for " + topOfStep.modelName + "...");
                      csiName = step->csiName();
                      int rc = step->createCsi(
                                  isMirrored ? addLine : "1 color 0 0 0 1 0 0 0 1 0 0 0 1 foo.ldr",
                                  saveCsiParts = configureModelStep(csiParts, step->modelDisplayOnlyStep ? -1 : stepNum, topOfStep),
                                  &step->csiPixmap,
                                  steps->meta);

                      if (rc) {
                          emit messageSig(LOG_ERROR, QMessageBox::tr("Failed to create CSI file."));
                          return rc;
                      }

                      // set csi annotations - single step only
                      if (! gui->exportingObjects() &&  ! multiStep && ! calledOut)
                          step->setCsiAnnotationMetas(steps->meta);

                      if (renderer->useLDViewSCall() && ! step->ldrName.isNull()) {
                          ldrStepFiles << step->ldrName;
                          csiKeys << step->csiKey;
                      }

                  } else {

                      if (pliPerStep) {
                          pliParts.clear();
                          pliPartGroups.clear();
                      }

                      // Only pages or step can have inserts and pointers... not callouts
                      if ( ! multiStep && ! calledOut) {

                          Page *page = dynamic_cast<Page *>(steps);
                          if (page) {
                              page->inserts      = inserts;
                              page->pagePointers = pagePointers;
                          }
                      }
                  }

                  // STEP - Simple STEP
                  if ( ! multiStep && ! calledOut) {

                      steps->placement = steps->meta.LPub.assem.placement;

                      showLine(topOfStep);

                      int  numSteps = ldrawFile.numSteps(current.modelName);

                      bool endOfSubmodel =
                              numSteps == 0 ||
                              steps->meta.LPub.contStepNumbers.value() ?
                                  steps->meta.LPub.contModelStepNum.value() >= numSteps :
                                  stepNum >= numSteps;

                      int countInstanceOverride = steps->meta.LPub.page.countInstanceOverride.value();
                      int instances = countInstanceOverride ? countInstanceOverride :
                                                              ldrawFile.instances(current.modelName, isMirrored);
                      displayCount = countInstances && instances > 1;
                      if (displayCount && countInstances != CountAtTop && !countInstanceOverride) {
                          MetaItem mi;
                          if (countInstances == CountAtStep)
                              instances = mi.countInstancesInStep(&steps->meta, current.modelName);
                          else
                          if (countInstances > CountFalse && countInstances < CountAtStep)
                              instances = mi.countInstancesInModel(&steps->meta, current.modelName);
                      }

                      Page *page = dynamic_cast<Page *>(steps);
                      if (page && instances > 1) {
                          page->instances            = instances;
                          page->displayInstanceCount = displayCount;
                          page->setItemDirection     = itemDirection;

                          if (! steps->meta.LPub.stepPli.perStep.value()) {

                              PlacementType relativeType = SingleStepType;

                              QStringList instancesPliParts;
                              if (pliParts.size() > 0) {
                                  for (int index = 0; index < pliParts.size(); index++) {
                                      QString pliLine = pliParts[index];
                                      for (int i = 0; i < instances; i++) {
                                          instancesPliParts << pliLine;
                                      }
                                  }
                              }

                              step->pli.setParts(instancesPliParts,pliPartGroups,steps->meta);
                              instancesPliParts.clear();
                              pliParts.clear();
                              pliPartGroups.clear();

                              emit messageSig(LOG_INFO, "Add PLI images for single-step page...");

                              step->pli.sizePli(&steps->meta,relativeType,pliPerStep);
                          }
                      }

                      emit messageSig(LOG_INFO, "Add CSI image for single-step page...");

                      if (renderer->useLDViewSCall() && ldrStepFiles.size() > 0){

                          QElapsedTimer timer;
                          timer.start();
                          QString empty("");

                          // set camera
                          steps->meta.LPub.assem.cameraAngles            = step->csiCameraMeta.cameraAngles;
                          steps->meta.LPub.assem.cameraDistNative.factor = step->csiCameraMeta.cameraDistNative.factor;
                          steps->meta.LPub.assem.modelScale              = step->csiCameraMeta.modelScale;
                          steps->meta.LPub.assem.cameraFoV               = step->csiCameraMeta.cameraFoV;
                          steps->meta.LPub.assem.zfar                    = step->csiCameraMeta.zfar;
                          steps->meta.LPub.assem.znear                   = step->csiCameraMeta.znear;
                          // set the extra renderer parms
                          steps->meta.LPub.assem.ldviewParms =
                               Render::getRenderer() == RENDERER_LDVIEW ? step->ldviewParms :
                               Render::getRenderer() == RENDERER_LDGLITE ? step->ldgliteParms :
                                             /*POV scene file generator*/  step->ldviewParms ;
                          if (Preferences::preferredRenderer == RENDERER_POVRAY)
                              steps->meta.LPub.assem.povrayParms = step->povrayParms;

                          // render the partially assembled model
                          int rc = renderer->renderCsi(empty,ldrStepFiles,csiKeys,empty,steps->meta);
                          if (rc != 0) {
                              emit messageSig(LOG_ERROR,QMessageBox::tr("Render CSI images failed."));
                              return rc;
                          }

                          emit gui->messageSig(LOG_INFO,
                                               QString("%1 CSI (Single Call) render took "
                                                       "%2 milliseconds to render %3 [Step %4] %5 for %6 "
                                                       "single step on page %7.")
                                               .arg(Render::getRenderer())
                                               .arg(timer.elapsed())
                                               .arg(ldrStepFiles.size())
                                               .arg(stepNum)
                                               .arg(ldrStepFiles.size() == 1 ? "image" : "images")
                                               .arg(calledOut ? "called out," : "simple,")
                                               .arg(stepPageNum));
                      }

                      addGraphicsPageItems(steps,coverPage,endOfSubmodel,view,scene,printing);
                      stepPageNum += ! coverPage;
                      steps->setBottomOfSteps(current);
                      drawPageElapsedTime();

                      return HitEndOfPage;
                  }

                  dividerType = NoDivider;

                  steps->meta.pop();
                  stepNum += partsAdded;
                  topOfStep = current;

                  partsAdded    = false;
                  coverPage     = false;
                  rotateIcon    = false;
                  itemDirection = false;
                  step          = nullptr;
                  bfxStore2     = bfxStore1;
                  bfxStore1     = false;
                  bfxLoad       = false;
              }

              if ( ! multiStep) {
                  inserts.clear();
                  pagePointers.clear();
                }
              steps->setBottomOfSteps(current);
              noStep = false;
              break;
            case RangeErrorRc:
            {
              showLine(current);
              QString message;
              if (Preferences::usingNativeRenderer &&
                  line.indexOf("CAMERA_FOV") != -1)
                  message = QString("Native renderer CAMERA_FOV value is out of range [%1:%2]"
                                    "<br>Meta command: %3"
                                    "<br>Valid values: minimum 1.0, maximum 359.0")
                                        .arg(current.modelName)
                                        .arg(current.lineNumber)
                                        .arg(line);
              else
                  message = QString("Parameter(s) out of range: %1:%2<br>Meta command: %3")
                          .arg(current.modelName)
                          .arg(current.lineNumber)
                          .arg(line);

              emit gui->messageSig(LOG_ERROR,message);
              return RangeErrorRc;
            }
            default:
              break;
            }
        }
      // STEP - Process invalid line
      else if (line != "") {
          showLine(current);
          const QChar type = line.at(0);
          parseError(QString("Invalid LDraw type %1 line. Expected %2 elements, got \"%3\".")
                     .arg(type).arg(type == '1' ? 15 : type == '2' ? 8 : type == '3' ? 11 : 14).arg(line),current);
          retVal = InvalidLDrawLineRc;
      }
      /* if part is on excludedPart.lst, unset pliIgnore if still set */
      if (pliIgnore && tokens[0] == "1" &&
          ExcludedParts::hasExcludedPart(tokens[tokens.size()-1])) {
          pliIgnore = false;
      }
    } // for every line

  drawPageElapsedTime();
  return retVal;
}

int Gui::findPage(
    LGraphicsView  *view,
    LGraphicsScene *scene,
    int            &pageNum,      //maxPages
    QString const  &addLine,
    Where          &current,
    PgSizeData     &pageSize,
    bool            isMirrored,
    Meta            meta,
    bool            printing,
    int             contStepNumber,
    int             renderStepNumber,
    QString         renderParentModel)
{
  bool stepGroup  = false;
  bool partIgnore = false;
  bool coverPage  = false;
  bool stepPage   = false;
  bool bfxStore1  = false;
  bool bfxStore2  = false;
  bool callout    = false;
  bool noStep     = false;
  bool noStep2    = false;
  bool stepGroupBfxStore2 = false;
  bool pageSizeUpdate     = false;

  Rc rc;
  QStringList bfxParts;
  QStringList saveBfxParts;
  QStringList ldrStepFiles;
  QStringList csiKeys;
  int  partsAdded = 0;
  int  stepNumber = 1;

  skipHeader(current);

  if (pageNum == 1) {
      topOfPages.clear();
      topOfPages.append(current);
  }

  QStringList csiParts;
  QStringList saveCsiParts;
  Where       saveCurrent = current;
  Where       stepGroupCurrent;
  int         saveStepNumber = 1;

  saveStepPageNum = stepPageNum;

  Meta        saveMeta = meta;

  QHash<QString, QStringList> bfx;
  QHash<QString, QStringList> saveBfx;
  QList<PliPartGroupMeta> emptyPartGroups;

  int numLines = ldrawFile.size(current.modelName);

  int  countInstances = meta.LPub.countInstance.value();

  Where topOfStep = current;

  RotStepMeta saveRotStep = meta.rotStep;

  emit messageSig(LOG_STATUS, "Processing find page for " + current.modelName + "...");

  ldrawFile.setRendered(current.modelName, isMirrored, renderParentModel, renderStepNumber, countInstances);

  for ( ;
        current.lineNumber < numLines;
        current.lineNumber++) {

      // scan through the rest of the model counting pages
      // if we've already hit the display page, then do as little as possible

      QString line = ldrawFile.readLine(current.modelName,current.lineNumber).trimmed();

      if (line.startsWith("0 GHOST ")) {
          line = line.mid(8).trimmed();
      }

      QStringList tokens, addTokens;

      switch (line.toLatin1()[0]) {
      case '1':
          split(line,tokens);

          if (tokens.size() > 2 && tokens[1] == "16") {
              split(addLine,addTokens);
              if (addTokens.size() == 15) {
                  tokens[1] = addTokens[1];
              }
              line = tokens.join(" ");
          }

          if (! partIgnore) {

              // csiParts << line;

              if (firstStepPageNum == -1) {
                  firstStepPageNum = pageNum;
              }
              lastStepPageNum = pageNum;

              QStringList token;

              split(line,token);

              if (token.size() == 15) {

                  QString    type = token[token.size()-1];

                  bool contains   = ldrawFile.isSubmodel(type);
                  CalloutBeginMeta::CalloutMode calloutMode = meta.LPub.callout.begin.value();

                  // if submodel or assembled/rotated callout
                  if (contains && (!callout || (callout && calloutMode != CalloutBeginMeta::Unassembled))) {

                      // check if submodel was rendered
                      bool rendered = ldrawFile.rendered(type,ldrawFile.mirrored(token),current.modelName,stepNumber,countInstances);

                      // if the submodel was not rendered, and (is not in the buffer exchange call setRendered for the submodel.
                      if (! rendered && (! bfxStore2 || ! bfxParts.contains(token[1]+type))) {

                          isMirrored = ldrawFile.mirrored(token);

                          // add submodel to the model stack - it can't be a callout
                          SubmodelStack tos(current.modelName,current.lineNumber,stepNumber);
                          meta.submodelStack << tos;
                          Where current2(type,0);

                          ldrawFile.setModelStartPageNumber(current2.modelName,pageNum);

                          // save rotStep, clear it, and restore it afterwards
                          // since rotsteps don't affect submodels
                          RotStepMeta saveRotStep2 = meta.rotStep;
                          meta.rotStep.clear();

                          // save Default pageSize information
                          PgSizeData pageSize2;
                          if (exporting()) {
                              pageSize2       = pageSizes[DEF_SIZE];
                              pageSizeUpdate  = false;
#ifdef SIZE_DEBUG
                              logDebug() << "SM: Saving    Default Page size info at PageNumber:" << pageNum
                                         << "W:"    << pageSize2.sizeW << "H:"    << pageSize2.sizeH
                                         << "O:"    <<(pageSize2.orientation == Portrait ? "Portrait" : "Landscape")
                                         << "ID:"   << pageSize2.sizeID
                                         << "Model:" << current.modelName;
#endif
                          }

                          // set the step number and parent model where the submodel will be rendered
                          renderStepNumber = stepNumber;
                          renderParentModel = current.modelName;

                          findPage(view,
                                   scene,
                                   pageNum,
                                   line,
                                   current2,
                                   pageSize,
                                   isMirrored,
                                   meta,
                                   printing,
                                   contStepNumber,
                                   renderStepNumber,
                                   renderParentModel);

                          saveStepPageNum = stepPageNum;
                          meta.submodelStack.pop_back();
                          meta.rotStep = saveRotStep2;       // restore old rotstep
                          if (contStepNumber) {              // capture continuous step number from exited submodel
                              contStepNumber = saveContStepNum;
                          }

                          if (exporting()) {
                              pageSizes.remove(DEF_SIZE);
                              pageSizes.insert(DEF_SIZE,pageSize2);  // restore old Default pageSize information
#ifdef SIZE_DEBUG
                              logDebug() << "SM: Restoring Default Page size info at PageNumber:" << pageNum
                                         << "W:"    << pageSizes[DEF_SIZE].sizeW << "H:"    << pageSizes[DEF_SIZE].sizeH
                                         << "O:"    << (pageSizes[DEF_SIZE].orientation == Portrait ? "Portrait" : "Landscape")
                                         << "ID:"   << pageSizes[DEF_SIZE].sizeID
                                         << "Model:" << current.modelName;
#endif
                          }
                      }
                  }
                 if (bfxStore1) {
                     bfxParts << token[1]+type;
                 }
              }
          } else if (partIgnore){

              if (tokens.size() == 15){
                  QString lineItem = tokens[tokens.size()-1];

                  if (ldrawFile.isSubmodel(lineItem)){
                      Where model(lineItem,0);
                      QString message("Submodel " + lineItem + " is set to ignore (IGN)");
                      statusBarMsg(message + ".");
                      //                      ldrawFile.setModelStartPageNumber(model.modelName,pageNum);
                      //                      logTrace() << message << model.modelName << " @ Page: " << pageNum;
                  }
              }
          }
        case '2':
        case '3':
        case '4':
        case '5':
            ++partsAdded;
            csiParts << line;
            break;

        case '0':
          rc = meta.parse(line,current);
          switch (rc) {
            case StepGroupBeginRc:
              stepGroup = true;
              stepGroupCurrent = topOfStep;
              if (contStepNumber){    // save starting step group continuous step number to pass to drawPage for submodel preview
                  int showStepNum = contStepNumber == 1 ? stepNumber : contStepNumber;
                  if (pageNum == 1) {
                      meta.LPub.subModel.showStepNum.setValue(showStepNum);
                  } else {
                      saveMeta.LPub.subModel.showStepNum.setValue(showStepNum);
                  }
              }
              // Steps within step group modify bfxStore2 as they progress
              // so we must save bfxStore2 and use the saved copy when
              // we call drawPage for a step group.
              stepGroupBfxStore2 = bfxStore2;
              break;

            case StepGroupEndRc:
              if (stepGroup && ! noStep2) {
                  stepGroup = false;
                  if (pageNum < displayPageNum) {
                      saveCsiParts   = csiParts;
                      saveStepNumber = stepNumber;
                      saveMeta       = meta;
                      saveBfx        = bfx;
                      saveBfxParts   = bfxParts;
                      bfxParts.clear();
                      saveRotStep = meta.rotStep;
                    } else if (pageNum == displayPageNum) {
                      csiParts.clear();
                      savePrevStepPosition = saveCsiParts.size();
                      stepPageNum = saveStepPageNum;
                      if (pageNum == 1) {
                        page.meta = meta;
                      } else {
                        page.meta = saveMeta;
                      }
                      if (contStepNumber) {  // pass continuous step number to drawPage
                        page.meta.LPub.contModelStepNum.setValue(saveStepNumber);
                        saveStepNumber = saveContStepNum;
                      }
                      page.meta.pop();
                      page.meta.rotStep = saveRotStep;

                      QStringList pliParts;

                      (void) drawPage(view,
                                      scene,
                                      &page,
                                      saveStepNumber,
                                      addLine,
                                      stepGroupCurrent,
                                      saveCsiParts,
                                      pliParts,
                                      isMirrored,
                                      saveBfx,
                                      emptyPartGroups,
                                      printing,
                                      stepGroupBfxStore2,
                                      saveBfxParts,
                                      ldrStepFiles,
                                      csiKeys);

                      saveCurrent.modelName.clear();
                      saveCsiParts.clear();
                    }
                  if (exporting()) {
                      pageSizes.remove(pageNum);
                      if (pageSizeUpdate) {
                          pageSizeUpdate = false;
                          pageSizes.insert(pageNum,pageSize);
#ifdef SIZE_DEBUG
                          logTrace() << "SG: Inserting New Page size info     at PageNumber:" << pageNum
                                     << "W:"    << pageSize.sizeW << "H:"    << pageSize.sizeH
                                     << "O:"    <<(pageSize.orientation == Portrait ? "Portrait" : "Landscape")
                                     << "ID:"   << pageSize.sizeID
                                     << "Model:" << current.modelName;
#endif
                        } else {
                          pageSizes.insert(pageNum,pageSizes[DEF_SIZE]);
#ifdef SIZE_DEBUG
                          logTrace() << "SG: Inserting Default Page size info at PageNumber:" << pageNum
                                     << "W:"    << pageSizes[DEF_SIZE].sizeW << "H:"    << pageSizes[DEF_SIZE].sizeH
                                     << "O:"    << (pageSizes[DEF_SIZE].orientation == Portrait ? "Portrait" : "Landscape")
                                     << "ID:"   << pageSizes[DEF_SIZE].sizeID
                                     << "Model:" << current.modelName;
#endif
                        }
                    } // exporting
                  ++pageNum;
                  topOfPages.append(current);
                  saveStepPageNum = ++stepPageNum;
                }
              noStep2 = false;
              break;

            case RotStepRc:
            case StepRc:
              if (partsAdded && ! noStep) {
                  if (contStepNumber) {   // increment continuous step number until we hit the display page
                      if (pageNum < displayPageNum &&
                         (stepNumber > FIRST_STEP || displayPageNum > FIRST_PAGE)) { // skip the first step
                          contStepNumber += ! coverPage && ! stepPage;
                      }
                      if (! stepGroup && stepNumber == 1) {
                          if (pageNum == 1 && topOfStep.modelName == topLevelFile()) { // when pageNum is 1 and not multistep, persist contStepNumber to 'meta' only if we are in the main model
                              meta.LPub.subModel.showStepNum.setValue(contStepNumber);
                          } else {
                              saveMeta.LPub.subModel.showStepNum.setValue(contStepNumber);
                          }
                      }
                  }
                  stepNumber  += ! coverPage && ! stepPage;
                  stepPageNum += ! coverPage && ! stepGroup;
                  if (pageNum < displayPageNum) {
                      if ( ! stepGroup) {
                          saveStepNumber  = stepNumber;
                          saveCsiParts    = csiParts;
                          saveMeta        = meta;
                          saveBfx         = bfx;
                          saveBfxParts    = bfxParts;
                          saveStepPageNum = stepPageNum;
                          // bfxParts.clear();
                        }
                      if (contStepNumber) { // save continuous step number from current model
                          saveContStepNum = contStepNumber;
                      }
                      saveCurrent = current;
                      saveRotStep = meta.rotStep;
                    }
                  if ( ! stepGroup) {
                      if (pageNum == displayPageNum) {
                          csiParts.clear();
                          savePrevStepPosition = saveCsiParts.size();
                          stepPageNum = saveStepPageNum;
                          if (pageNum == 1) {
                              page.meta = meta;
                          } else {
                              page.meta = saveMeta;
                          }
                          if (contStepNumber) { // pass continuous step number to drawPage
                              page.meta.LPub.contModelStepNum.setValue(saveStepNumber);
                              saveStepNumber = contStepNumber;
                          }
                          page.meta.pop();
                          page.meta.rotStep = saveRotStep;
                          page.meta.rotStep = meta.rotStep;

                          QStringList pliParts;

                          (void) drawPage(view,
                                          scene,
                                          &page,
                                          saveStepNumber,
                                          addLine,
                                          saveCurrent,
                                          saveCsiParts,
                                          pliParts,
                                          isMirrored,
                                          saveBfx,
                                          emptyPartGroups,
                                          printing,
                                          bfxStore2,
                                          saveBfxParts,
                                          ldrStepFiles,
                                          csiKeys);

                          saveCurrent.modelName.clear();
                          saveCsiParts.clear();
                        }
                      if (exporting()) {
                          pageSizes.remove(pageNum);
                          if (pageSizeUpdate) {
                              pageSizeUpdate = false;
                              pageSizes.insert(pageNum,pageSize);
#ifdef SIZE_DEBUG
                              logTrace() << "ST: Inserting New Page size info     at PageNumber:" << pageNum
                                         << "W:"    << pageSize.sizeW << "H:"    << pageSize.sizeH
                                         << "O:"    <<(pageSize.orientation == Portrait ? "Portrait" : "Landscape")
                                         << "ID:"   << pageSize.sizeID
                                         << "Model:" << current.modelName;
#endif
                            } else {
                              pageSizes.insert(pageNum,pageSizes[DEF_SIZE]);
#ifdef SIZE_DEBUG
                              logTrace() << "ST: Inserting Default Page size info at PageNumber:" << pageNum
                                         << "W:"    << pageSizes[DEF_SIZE].sizeW << "H:"    << pageSizes[DEF_SIZE].sizeH
                                         << "O:"    << (pageSizes[DEF_SIZE].orientation == Portrait ? "Portrait" : "Landscape")
                                         << "ID:"   << pageSizes[DEF_SIZE].sizeID
                                         << "Model:" << current.modelName;
#endif
                            }
                        } // exporting
                      ++pageNum;
                      topOfPages.append(current);
                    }
                  topOfStep = current;
                  partsAdded = 0;
                  meta.pop();
                  coverPage = false;
                  stepPage = false;                 
                  bfxStore2 = bfxStore1;
                  bfxStore1 = false;
                  if ( ! bfxStore2) {
                      bfxParts.clear();
                    }
                } else if ( ! stepGroup) {
                  saveCurrent = current;  // so that draw page doesn't have to
                  // deal with steps that are not steps
                }
              noStep2 = noStep;
              noStep = false;
              break;

            case CalloutBeginRc:
              callout = true;
              break;

            case CalloutEndRc:
              callout = false;
              meta.LPub.callout.placement.clear();
              break;

            case InsertCoverPageRc:
              coverPage  = true;
              partsAdded = true;
              break;

            case InsertPageRc:
              stepPage   = true;
              partsAdded = true;
              break;

            case PartBeginIgnRc:
              partIgnore = true;
              break;
            case PartEndRc:
              partIgnore = false;
              break;

              // Any of the metas that can change csiParts needs
              // to be processed here

            case ClearRc:
              if (pageNum < displayPageNum) {
                  csiParts.clear();
                  saveCsiParts.clear();
                }
              break;

              /* Buffer exchange */
            case BufferStoreRc:
              if (pageNum < displayPageNum) {
                  bfx[meta.bfx.value()] = csiParts;
                }
              bfxStore1 = true;
              bfxParts.clear();
              break;

            case BufferLoadRc:
              if (pageNum < displayPageNum) {
                  csiParts = bfx[meta.bfx.value()];
                }
              partsAdded = true;
              break;

            case MLCadGroupRc:
            case LDCadGroupRc:
            case LeoCadGroupBeginRc:
            case LeoCadGroupEndRc:
              if (pageNum < displayPageNum) {
                  csiParts << line;
                  partsAdded = true;
                }
              break;

              /* remove a group or all instances of a part type */
            case GroupRemoveRc:
            case RemoveGroupRc:
            case RemovePartRc:
            case RemoveNameRc:
              if (pageNum < displayPageNum) {
                  QStringList newCSIParts;
                  if (rc == RemoveGroupRc) {
                      remove_group(csiParts,    meta.LPub.remove.group.value(),newCSIParts);
                    } else if (rc == RemovePartRc) {
                      remove_parttype(csiParts, meta.LPub.remove.parttype.value(),newCSIParts);
                    } else {
                      remove_partname(csiParts, meta.LPub.remove.partname.value(),newCSIParts);
                    }
                  csiParts = newCSIParts;
                  newCSIParts.empty();
                }
              break;

            case IncludeRc:
              include(meta);
              break;

            case PageSizeRc:
              {
                if (exporting()) {
                    pageSizeUpdate  = true;

                    pageSize.sizeW  = meta.LPub.page.size.valueInches(0);
                    pageSize.sizeH  = meta.LPub.page.size.valueInches(1);
                    pageSize.sizeID = meta.LPub.page.size.valueSizeID();

                    pageSizes.remove(DEF_SIZE);
                    pageSizes.insert(DEF_SIZE,pageSize);
#ifdef SIZE_DEBUG                
                    logTrace() << "1. New Page Size entry for Default  at PageNumber:" << pageNum
                               << "W:"  << pageSize.sizeW << "H:"    << pageSize.sizeH
                               << "O:"  << (pageSize.orientation == Portrait ? "Portrait" : "Landscape")
                               << "ID:" << pageSize.sizeID
                               << "Model:" << current.modelName;
#endif
                  }
              }
              break;

            case CountInstanceRc:
              countInstances = meta.LPub.countInstance.value();
              break;

            case ContStepNumRc:
              {
                  if (meta.LPub.contStepNumbers.value()) {
                      if (! contStepNumber)
                          contStepNumber++;
                  } else {
                      contStepNumber = 0;
                  }
              }
              break;

            case PageOrientationRc:
              {
                if (exporting()){
                    pageSizeUpdate      = true;

                    if (pageSize.sizeW == 0.0f)
                      pageSize.sizeW    = pageSizes[DEF_SIZE].sizeW;
                    if (pageSize.sizeH == 0.0f)
                      pageSize.sizeH    = pageSizes[DEF_SIZE].sizeH;
                    if (pageSize.sizeID.isEmpty())
                      pageSize.sizeID   = pageSizes[DEF_SIZE].sizeID;
                    pageSize.orientation= meta.LPub.page.orientation.value();

                    pageSizes.remove(DEF_SIZE);
                    pageSizes.insert(DEF_SIZE,pageSize);
#ifdef SIZE_DEBUG
                    logTrace() << "1. New Orientation entry for Default at PageNumber:" << pageNum
                               << "W:"  << pageSize.sizeW << "H:"    << pageSize.sizeH
                               << "O:"  << (pageSize.orientation == Portrait ? "Portrait" : "Landscape")
                               << "ID:" << pageSize.sizeID
                               << "Model:" << current.modelName;
#endif
                  }
              }
              break;

            case NoStepRc:
              noStep = true;
              break;

            default:
              break;
            } // switch
          break;
        }
    } // for every line
  csiParts.clear();

  // last step in submodel
  if (partsAdded && ! noStep) {
      // increment continuous step number
      // save continuous step number from current model
      // pass continuous step number to drawPage
      if (contStepNumber) {
          if (! countInstances && pageNum < displayPageNum &&
             (stepNumber > FIRST_STEP || displayPageNum > FIRST_PAGE)) {
              contStepNumber += ! coverPage && ! stepPage;
          }
          if (pageNum == displayPageNum) {
              saveMeta.LPub.contModelStepNum.setValue(saveStepNumber);
              saveStepNumber = contStepNumber;
          }
          saveContStepNum = contStepNumber;
      }

      if (pageNum == displayPageNum) {
          savePrevStepPosition = saveCsiParts.size();
          page.meta = saveMeta;
          QStringList pliParts;

          (void) drawPage(view,
                          scene,
                          &page,
                          saveStepNumber,
                          addLine,
                          saveCurrent,
                          saveCsiParts,
                          pliParts,
                          isMirrored,
                          saveBfx,
                          emptyPartGroups,
                          printing,
                          bfxStore2,
                          saveBfxParts,
                          ldrStepFiles,
                          csiKeys);
        }
      if (exporting()) {
          pageSizes.remove(pageNum);
          if (pageSizeUpdate) {
              pageSizeUpdate = false;
              pageSizes.insert(pageNum,pageSize);
#ifdef SIZE_DEBUG
              logTrace() << "PG: Inserting New Page size info     at PageNumber:" << pageNum
                         << "W:"    << pageSize.sizeW << "H:"    << pageSize.sizeH
                         << "O:"    <<(pageSize.orientation == Portrait ? "Portrait" : "Landscape")
                         << "ID:"   << pageSize.sizeID
                         << "Model:" << current.modelName;
#endif
            } else {
              pageSizes.insert(pageNum,pageSizes[DEF_SIZE]);
#ifdef SIZE_DEBUG
              logTrace() << "PG: Inserting Default Page size info at PageNumber:" << pageNum
                         << "W:"    << pageSizes[DEF_SIZE].sizeW << "H:"    << pageSizes[DEF_SIZE].sizeH
                         << "O:"    << (pageSizes[DEF_SIZE].orientation == Portrait ? "Portrait" : "Landscape")
                         << "ID:"   << pageSizes[DEF_SIZE].sizeID
                         << "Model:" << current.modelName;
#endif
            }
        } // exporting
      ++pageNum;
      topOfPages.append(current);
      ++stepPageNum;
    }  
  return 0;
}

int Gui::getBOMParts(
    Where        current,
    QString     &addLine,
    QStringList &pliParts,
    QList<PliPartGroupMeta> &bomPartGroups)
{
  bool partIgnore = false;
  bool pliIgnore = false;
  bool synthBegin = false;
  bool bfxStore1 = false;
  bool bfxStore2 = false;
  bool bfxLoad = false;
  bool partsAdded = false;
  bool excludedPart = false;
  QStringList bfxParts;

  Meta meta;

  skipHeader(current);

  QHash<QString, QStringList> bfx;

  int numLines = ldrawFile.size(current.modelName);

  Rc rc;

  for ( ;
        current.lineNumber < numLines;
        current.lineNumber++) {

      // scan through the rest of the model counting pages
      // if we've already hit the display page, then do as little as possible

      QString line = ldrawFile.readLine(current.modelName,current.lineNumber).trimmed();

      if (line.startsWith("0 GHOST ")) {
          line = line.mid(8).trimmed();
        }

      switch (line.toLatin1()[0]) {
        case '1':
          /* check if part is in excludedPart.lst*/
          excludedPart = ExcludedParts::lineHasExcludedPart(line);

          if ( !excludedPart && ! partIgnore && ! pliIgnore && ! synthBegin) {

              QStringList token,addToken;

              split(line,token);

              QString    type = token[token.size()-1];

              if (token[1] == "16") {
                  split(addLine,addToken);
                  if (addToken.size() == 15) {
                      token[1] = addToken[1];
                      line = token.join(" ");
                    }
                }

          /*
           * Automatically ignore parts added twice due to buffer exchange
           */
              bool removed = false;
              QString colorPart = token[1] + type;

              if (bfxStore2 && bfxLoad) {
                  int i;
                  for (i = 0; i < bfxParts.size(); i++) {
                      if (bfxParts[i] == colorPart) {
                          bfxParts.removeAt(i);
                          removed = true;
                          break;
                        }
                    }
                }

              if ( ! removed) {

                  if (ldrawFile.isSubmodel(type)) {

                      Where current2(type,0);

                      getBOMParts(current2,line,pliParts,bomPartGroups);

                    } else {

                      /*  check if substitute part exist and replace */
                      if(PliSubstituteParts::hasSubstitutePart(type)) {

                          QStringList substituteToken;
                          split(line,substituteToken);
                          QString substitutePart = type;

                          if (PliSubstituteParts::getSubstitutePart(substitutePart)){
                              substituteToken[substituteToken.size()-1] = substitutePart;
                            }
                          line = substituteToken.join(" ");
                        }

                      QString newLine = Pli::partLine(line,current,meta);

                      pliParts << newLine;
                    }
                }
              if (bfxStore1) {
                  bfxParts << colorPart;
                }
              partsAdded = true;
            }
          break;
        case '0':
          rc = meta.parse(line,current);

          /* substitute part/parts with this */

          switch (rc) {
            case PliBeginSub1Rc:
              if (! pliIgnore &&
                  ! partIgnore &&
                  ! synthBegin) {

                  QString line = QString("1 0 0 0 0 1 0 0 0 1 0 0 0 1 %1") .arg(meta.LPub.pli.begin.sub.value().part);
                  pliParts << Pli::partLine(line,current,meta);
                  pliIgnore = true;
                }
              break;

              /* substitute part/parts with this */
            case PliBeginSub2Rc:
            case PliBeginSub3Rc:
            case PliBeginSub4Rc:
            case PliBeginSub5Rc:
            case PliBeginSub6Rc:
              if (! pliIgnore &&
                  ! partIgnore &&
                  ! synthBegin) {
                  QString line = QString("1 %1 0 0 0 1 0 0 0 1 0 0 0 1 %2")
                      .arg(meta.LPub.pli.begin.sub.value().color)
                      .arg(meta.LPub.pli.begin.sub.value().part);
                  pliParts << Pli::partLine(line,current,meta);
                  pliIgnore = true;
                }
              break;

            case PliBeginIgnRc:
              pliIgnore = true;
              break;

            case PliEndRc:
              pliIgnore = false;
              meta.LPub.pli.begin.sub.clearAttributes();
              break;

            case PartBeginIgnRc:
              partIgnore = true;
              break;

            case PartEndRc:
              partIgnore = false;
              break;

            case SynthBeginRc:
              synthBegin = true;
              break;

            case SynthEndRc:
              synthBegin = false;
              break;

              /* Buffer exchange */
            case BufferStoreRc:
              bfxStore1 = true;
              bfxParts.clear();
              break;
            case BufferLoadRc:
              bfxLoad = true;
              break;
            case BomPartGroupRc:
              meta.LPub.bom.pliPartGroup.setWhere(current);
              meta.LPub.bom.pliPartGroup.setBomPart(true);
              bomPartGroups.append(meta.LPub.bom.pliPartGroup);
              break;

              // Any of the metas that can change pliParts needs
              // to be processed here

            case ClearRc:
              pliParts.empty();
              break;

            case MLCadGroupRc:
            case LDCadGroupRc:
            case LeoCadGroupBeginRc:
            case LeoCadGroupEndRc:
              pliParts << Pli::partLine(line,current,meta);
              break;

              /* remove a group or all instances of a part type */
            case GroupRemoveRc:
            case RemoveGroupRc:
            case RemovePartRc:
            case RemoveNameRc:
              {
                QStringList newCSIParts;
                if (rc == RemoveGroupRc) {
                    remove_group(pliParts,meta.LPub.remove.group.value(),newCSIParts);
                  } else if (rc == RemovePartRc) {
                    remove_parttype(pliParts, meta.LPub.remove.parttype.value(),newCSIParts);
                  } else {
                    remove_partname(pliParts, meta.LPub.remove.partname.value(),newCSIParts);
                  }
                pliParts = newCSIParts;
              }
              break;

            case StepRc:

              if (partsAdded) {
                  bfxStore2 = bfxStore1;
                  bfxStore1 = false;
                  bfxLoad = false;
                  if ( ! bfxStore2) {
                      bfxParts.clear();
                    }
                }
              partsAdded = false;
              break;

            default:
              break;
            } // switch
          break;
        }
    } // for every line
  return 0;
}

int Gui::getBOMOccurrence(Where	current) {		// start at top of ldrawFile

  // traverse content to find the number and location of BOM pages
  // key=modelName_LineNumber, value=occurrence
  QHash<QString, int> bom_Occurrence;
  Meta meta;

  skipHeader(current);

  int numLines        = ldrawFile.size(current.modelName);
  int occurrenceNum   = 0;
  boms                = 0;
  bomOccurrence       = 0;
  Rc rc;

  for ( ; current.lineNumber < numLines;
        current.lineNumber++) {

      QString line = ldrawFile.readLine(current.modelName,current.lineNumber).trimmed();
      switch (line.toLatin1()[0]) {
        case '1':
          {
            QStringList token;
            split(line,token);
            QString type = token[token.size()-1];

            if (ldrawFile.isSubmodel(type)) {
                Where current2(type,0);
                getBOMOccurrence(current2);
              }
            break;
          }
        case '0':
          {
            rc = meta.parse(line,current);
            switch (rc) {
              case InsertRc:
                {
                  InsertData insertData = meta.LPub.insert.value();
                  if (insertData.type == InsertData::InsertBom){

                      QString uniqueID = QString("%1_%2").arg(current.modelName).arg(current.lineNumber);
                      occurrenceNum++;
                      bom_Occurrence[uniqueID] = occurrenceNum;
                    }
                }
                break;
              default:
                break;
              } // switch metas
            break;
          }  // switch line type
        }
    } // for every line

  if (occurrenceNum > 1) {
      // now set the bom occurrance based on our current position
      Where here = gui->topOfPages[gui->displayPageNum-1];
      for (++here; here.lineNumber < ldrawFile.size(here.modelName); here++) {
          QString line = gui->readLine(here);
          Meta meta;
          Rc rc;

          rc = meta.parse(line,here);
          if (rc == InsertRc) {

              InsertData insertData = meta.LPub.insert.value();
              if (insertData.type == InsertData::InsertBom) {

                  QString bomID   = QString("%1_%2").arg(here.modelName).arg(here.lineNumber);
                  bomOccurrence   = bom_Occurrence[bomID];
                  boms            = bom_Occurrence.size();
                  break;
                }
            }
        }
    }
  return 0;
}

bool Gui::generateBOMPartsFile(const QString &bomFileName){
    QString addLine;
    QStringList tempParts, bomParts;
    QList<PliPartGroupMeta> bomPartGroups;
    Where current(ldrawFile.topLevelFile(),0);
    getBOMParts(current,addLine,tempParts,bomPartGroups);

    if (! tempParts.size()) {
        emit messageSig(LOG_ERROR,QMessageBox::tr("No BOM parts were detected."));
        return false;
    }

    foreach (QString bomPartsString, tempParts){
        if (bomPartsString.startsWith("1")) {
            QStringList partComponents = bomPartsString.split(";");
            bomParts << partComponents.at(0);
            emit messageSig(LOG_DEBUG,QMessageBox::tr("%1 added to export list.").arg(partComponents.at(0)));
        }
    }
    emit messageSig(LOG_INFO,QMessageBox::tr("%1 BOM parts processed.").arg(bomParts.size()));

    // create a BOM parts file
    QFile bomFile(bomFileName);
    if ( ! bomFile.open(QIODevice::WriteOnly)) {
        emit messageSig(LOG_ERROR,QMessageBox::tr("Cannot open BOM parts file for writing: %1, %2.")
                              .arg(bomFileName)
                              .arg(bomFile.errorString()));
        return false;
    }

    QTextStream out(&bomFile);
    out << QString("0 Name: %1").arg(QFileInfo(bomFileName).fileName()) << endl;
    foreach (QString bomPart, bomParts)
        out << bomPart << endl;
    bomFile.close();
    return true;
}

void Gui::attitudeAdjustment()
{
  Meta meta;
  bool callout = false;
  int numFiles = ldrawFile.subFileOrder().size();

  for (int i = 0; i < numFiles; i++) {
      QString fileName = ldrawFile.subFileOrder()[i];
      int numLines     = ldrawFile.size(fileName);

      QStringList pending;

      for (Where current(fileName,0);
           current.lineNumber < numLines;
           current.lineNumber++) {

          QString line = ldrawFile.readLine(current.modelName,current.lineNumber);
          QStringList argv;
          split(line,argv);

          if (argv.size() >= 4 &&
              argv[0] == "0" &&
              (argv[1] == "LPUB" || argv[1] == "!LPUB") &&
              argv[2] == "CALLOUT") {
              if (argv.size() == 4 && argv[3] == "BEGIN") {
                  callout = true;
                  pending.clear();
                } else if (argv[3] == "END") {
                  callout = false;
                  for (int i = 0; i < pending.size(); i++) {
                      ldrawFile.insertLine(current.modelName,current.lineNumber, pending[i]);
                      ++numLines;
                      ++current;
                    }
                  pending.clear();
                } else if (argv[3] == "ALLOC" ||
                           argv[3] == "BACKGROUND" ||
                           argv[3] == "BORDER" ||
                           argv[3] == "MARGINS" ||
                           argv[3] == "PLACEMENT") {
                  if (callout && argv.size() >= 5 && argv[4] != "GLOBAL") {
                      ldrawFile.deleteLine(current.modelName,current.lineNumber);
                      pending << line;
                      --numLines;
                      --current;
                    }
                }
            }
        }
    }
}


void Gui::countPages()
{
  if (maxPages < 1) {
      writeToTmp();
      statusBarMsg("Counting");
      Where current(ldrawFile.topLevelFile(),0);
      int savedDpn     = displayPageNum;
      displayPageNum   = 1 << 31;
      firstStepPageNum = -1;
      lastStepPageNum  = -1;
      maxPages         = 1;
      Meta meta;
      QString empty;
      PgSizeData empty1;
      stepPageNum = 1;
      findPage(KpageView,KpageScene,maxPages,/*addLine*/empty,current,/*pageSize*/empty1,
               /*isMirrored*/false,meta,/*printing*/false,/*contStepNumber*/0,0,empty);
      topOfPages.append(current);
      maxPages--;

      if (displayPageNum > maxPages) {
          displayPageNum = maxPages;
        } else {
          displayPageNum = savedDpn;
        }
      QString string = QString("%1 of %2") .arg(displayPageNum) .arg(maxPages);
      setPageLineEdit->setText(string);
      statusBarMsg("");
    }
}

void Gui::drawPage(
    LGraphicsView  *view,
    LGraphicsScene *scene,
    bool            printing)
{
  QApplication::setOverrideCursor(Qt::WaitCursor);

  ldrawFile.unrendered();
  ldrawFile.countInstances();
  writeToTmp();
  Where       current(ldrawFile.topLevelFile(),0);
  maxPages    = 1;
  stepPageNum = 1;
  ldrawFile.setModelStartPageNumber(current.modelName,maxPages);
  //logTrace() << "SET INITIAL Model: " << current.modelName << " @ Page: " << maxPages;
  QString empty;
  Meta    meta;
  firstStepPageNum = -1;
  lastStepPageNum  = -1;
  savePrevStepPosition = 0;
  saveContStepNum = 1;

  PgSizeData pageSize;
  if (exporting()) {
      pageSize.sizeW      = meta.LPub.page.size.valueInches(0);
      pageSize.sizeH      = meta.LPub.page.size.valueInches(1);
      pageSize.sizeID     = meta.LPub.page.size.valueSizeID();
      pageSize.orientation= meta.LPub.page.orientation.value();
      pageSizes.insert(     DEF_SIZE,pageSize);
#ifdef SIZE_DEBUG
      logTrace() << "0. Inserting INIT page size info    at PageNumber:" << DEF_SIZE
                 << "W:"  << pageSize.sizeW << "H:"    << pageSize.sizeH
                 << "O:"  << (pageSize.orientation == Portrait ? "Portrait" : "Landscape")
                 << "ID:" << pageSize.sizeID
                 << "Model:" << current.modelName;
#endif
    }

  findPage(view,scene,maxPages,empty,current,pageSize,false,meta,printing,0,0,empty);
  topOfPages.append(current);
  maxPages--;

  QString string = QString("%1 of %2") .arg(displayPageNum) .arg(maxPages);
  if (! exporting())
    setPageLineEdit->setText(string);

  QApplication::restoreOverrideCursor();
}

void Gui::skipHeader(Where &current)
{
  int numLines = ldrawFile.size(current.modelName);
  for ( ; current.lineNumber < numLines; current.lineNumber++) {
      QString line = gui->readLine(current);
      int p;
      for (p = 0; p < line.size(); ++p) {
          if (line[p] != ' ') {
              break;
            }
        }
      if (line[p] >= '1' && line[p] <= '5') {
          if (current.lineNumber == 0) {
              QString empty = "0 ";
              gui->insertLine(current,empty,nullptr);
            } else if (current > 0) {
              --current;
            }
          break;
        } else if ( ! isHeader(line)) {
          if (current.lineNumber != 0) {
              --current;
              break;
            }
        }
    }
}

void Gui::include(Meta &meta)
{
  QString fileName = meta.LPub.include.value();
  if (ldrawFile.isSubmodel(fileName)) {
      int numLines = ldrawFile.size(fileName);

      Where current(fileName,0);
      for (; current < numLines; current++) {
          QString line = ldrawFile.readLine(fileName,current.lineNumber);
          meta.parse(line,current);
        }
    } else {
      QFileInfo fileInfo(fileName);
      if (fileInfo.exists()) {
          QFile file(fileName);
          if ( ! file.open(QFile::ReadOnly | QFile::Text)) {
              QMessageBox::warning(nullptr,
                                   QMessageBox::tr(VER_PRODUCTNAME_STR),
                                   QMessageBox::tr("Cannot read file %1:\n%2.")
                                   .arg(fileName)
                                   .arg(file.errorString()));
              return;
            }

          /* Read it in the first time to put into fileList in order of
         appearance */

          QTextStream in(&file);
          QStringList contents;
          Where       current(fileName,0);

          while ( ! in.atEnd()) {
              QString line = in.readLine(0);
              meta.parse(line,current);
              ++current;
            }
          file.close();
        }
    }
}

static Where dummy;

Where &Gui::topOfPage()
{
  int pageNum = displayPageNum - 1;
  if (pageNum < topOfPages.size()) {
      return topOfPages[pageNum];
    } else {
      return dummy;
    }
}

Where &Gui::bottomOfPage()
{
  if (displayPageNum < topOfPages.size()) {
      return topOfPages[displayPageNum];
    } else {
      return dummy;
    }
}

/*
 * This function applies buffer exchange and LPub's remove
 * meta commands before writing them out for the renderers to use.
 * Fade, Highlight and COLOUR meta commands are preserved.
 * This eliminates the need for ghosting parts removed by buffer
 * exchange
 */

void Gui::writeToTmp(const QString &fileName,
                     const QStringList &contents)
{
  QString fname = QDir::toNativeSeparators(QDir::currentPath()) + QDir::separator() + Paths::tmpDir + QDir::separator() + fileName;
  QFileInfo fileInfo(fname);
  if(!fileInfo.dir().exists()) {
     fileInfo.dir().mkpath(".");
    }
  QFile file(fname);
  if ( ! file.open(QFile::WriteOnly|QFile::Text)) {
      QMessageBox::warning(nullptr,QMessageBox::tr("LPub3D"),
                           QMessageBox::tr("Failed to open %1 for writing: %2")
                           .arg(fname) .arg(file.errorString()));
    } else {
      QStringList csiParts;
      QHash<QString, QStringList> bfx;

      for (int i = 0; i < contents.size(); i++) {
          QString line = contents[i];
          QStringList tokens;

          split(line,tokens);
          if (tokens.size()) {
              if (tokens[0] != "0") {
                 csiParts << line;
              } else if (tokens.size() == 11 &&
                          tokens[0] == "0"    &&
                          tokens[1] == "!COLOUR") {
                 csiParts << line;
              } else if ((tokens.size() == 2 || tokens.size() == 3) &&
                         tokens[0] == "0"    &&
                        (tokens[1] == "!FADE")) {
                csiParts << line;
             } else if ((tokens.size() == 2 || tokens.size() == 4) &&
                          tokens[0] == "0"    &&
                         (tokens[1] == "!SILHOUETTE")) {
                 csiParts << line;
              } else {
                  Meta meta;
                  Rc   rc;
                  Where here(fileName,i);
                  rc = meta.parse(line,here,false);

                  switch (rc) {

                    /* Buffer exchange */
                    case BufferStoreRc:
                      bfx[meta.bfx.value()] = csiParts;
                      break;
                    case BufferLoadRc:
                      csiParts = bfx[meta.bfx.value()];
                      break;
                    case MLCadGroupRc:
                    case LDCadGroupRc:
                    case LeoCadGroupBeginRc:
                    case LeoCadGroupEndRc:
                      csiParts << line;
                      break;
                      /* remove a group or all instances of a part type */
                    case GroupRemoveRc:
                    case RemoveGroupRc:
                    case RemovePartRc:
                    case RemoveNameRc:
                      {
                        QStringList newCSIParts;
                        if (rc == RemoveGroupRc) {
                            remove_group(csiParts,meta.LPub.remove.group.value(),newCSIParts);
                          } else if (rc == RemovePartRc) {
                            remove_parttype(csiParts, meta.LPub.remove.parttype.value(),newCSIParts);
                          } else {
                            remove_partname(csiParts, meta.LPub.remove.partname.value(),newCSIParts);
                          }
                        csiParts = newCSIParts;
                      }
                      break;
                    default:
                      break;
                    }
                }
            }
        }

      QTextStream out(&file);
      for (int i = 0; i < csiParts.size(); i++) {
          out << csiParts[i] << endl;
        }
      file.close();
    }
}

void Gui::writeToTmp()
{
  if (Preferences::modeGUI && ! exporting()) {
      emit progressBarPermInitSig();
      emit progressPermRangeSig(1, ldrawFile._subFileOrder.size());
      emit progressPermMessageSig("Writing submodels...");
    }
  emit messageSig(LOG_STATUS, "Writing submodels to temp directory...");

  bool upToDate = true;
  bool doFadeStep  = page.meta.LPub.fadeStep.fadeStep.value();
  bool doHighlightStep = page.meta.LPub.highlightStep.highlightStep.value() && !suppressColourMeta();

  QString fadeColor = LDrawColor::ldColorCode(page.meta.LPub.fadeStep.fadeColor.value());

  QStringList content, configuredContent;

  for (int i = 0; i < ldrawFile._subFileOrder.size(); i++) {

      Where here(ldrawFile._subFileOrder[i],0);

      normalizeHeader(here);

      QString fileName = ldrawFile._subFileOrder[i].toLower();

      if (Preferences::modeGUI && ! exporting())
        emit progressPermSetValueSig(i);

      content = ldrawFile.contents(fileName);

      if (ldrawFile.changedSinceLastWrite(fileName)) {
          // write normal submodels...
          upToDate = false;
          emit messageSig(LOG_INFO, "Writing submodel to temp directory: " + fileName + "...");
          writeToTmp(fileName,content);

          // capture file name extensions
          QString fileNameStr;
          QString extension = QFileInfo(fileName).suffix().toLower();

          // write configured (Fade) submodels
          if (doFadeStep) {
             fileNameStr = fileName;
             if (extension.isEmpty()) {
               fileNameStr = fileNameStr.append(QString("%1.ldr").arg(FADE_SFX));
             } else {
               fileNameStr = fileNameStr.replace("."+extension, QString("%1.%2").arg(FADE_SFX).arg(extension));
             }

            /* Faded version of submodels */
            emit messageSig(LOG_INFO, "Writing fade submodels to temp directory: " + fileNameStr);
            configuredContent = configureModelSubFile(content, fadeColor, FADE_PART);
            writeToTmp(fileNameStr,configuredContent);
          }
          // write configured (Highlight) submodels
          if (doHighlightStep) {
            fileNameStr = fileName;
            if (extension.isEmpty()) {
              fileNameStr = fileNameStr.append(QString("%1.ldr").arg(HIGHLIGHT_SFX));
            } else {
              fileNameStr = fileNameStr.replace("."+extension, QString("%1.%2").arg(HIGHLIGHT_SFX).arg(extension));
            }
            /* Highlighted version of submodels */
            emit messageSig(LOG_INFO, "Writing highlight submodel to temp directory: " + fileNameStr);
            configuredContent = configureModelSubFile(content, fadeColor, HIGHLIGHT_PART);
            writeToTmp(fileNameStr,configuredContent);
          }
      }
  }

  bool generateSubModelImages = Preferences::modeGUI &&
                                gApplication->mPreferences.mViewPieceIcons &&
                                ! submodelIconsLoaded;
  if (generateSubModelImages) {
      if (Preferences::modeGUI && ! exporting())
          emit progressPermSetValueSig(ldrawFile._subFileOrder.size());

      // generate submodel icons...
      emit messageSig(LOG_INFO_STATUS, "Creating submodel icons...");
      Pli pli;
      int rc = pli.createSubModelIcons();
      if (rc == 0)
          gMainWindow->mSubmodelIconsLoaded = submodelIconsLoaded = true;
      else
          emit messageSig(LOG_ERROR, "Could not create submodel icons...");
      if (Preferences::modeGUI && ! exporting())
          emit progressPermStatusRemoveSig();
  } else
  if (Preferences::modeGUI && ! exporting()) {
      emit progressPermSetValueSig(ldrawFile._subFileOrder.size());
      emit progressPermStatusRemoveSig();
  }
  emit messageSig(LOG_STATUS, upToDate ? "No submodels written; temp directory up to date." : "Submodels written to temp directory.");
}

/*
 * Configure writeToTmp content - make fade or highlight copies of submodel files.
 */
QStringList Gui::configureModelSubFile(const QStringList &contents, const QString &fadeColour, const PartType partType)
{
  QString nameMod, colourPrefix;
  if (partType == FADE_PART){
    nameMod = LPUB3D_COLOUR_FADE_SUFFIX;
    colourPrefix = LPUB3D_COLOUR_FADE_PREFIX;
  } else if (partType == HIGHLIGHT_PART) {
    nameMod = LPUB3D_COLOUR_HIGHLIGHT_SUFFIX;
    colourPrefix = LPUB3D_COLOUR_HIGHLIGHT_PREFIX;
  }

  QStringList configuredContents, subfileColourList;
  bool FadeMetaAdded = false;
  bool SilhouetteMetaAdded = false;

  if (contents.size() > 0) {

      QStringList argv;

      for (int index = 0; index < contents.size(); index++) {

          QString contentLine = contents[index];
          split(contentLine, argv);
          if (argv.size() == 15 && argv[0] == "1") {
              // Insert opening fade meta
              if (!FadeMetaAdded && Preferences::enableFadeSteps && partType == FADE_PART){
                 configuredContents.insert(index,QString("0 !FADE %1").arg(Preferences::fadeStepsOpacity));
                 FadeMetaAdded = true;
              }
              // Insert opening silhouette meta
              if (!SilhouetteMetaAdded && Preferences::enableHighlightStep && partType == HIGHLIGHT_PART){
                 configuredContents.insert(index,QString("0 !SILHOUETTE %1 %2")
                                                         .arg(Preferences::highlightStepLineWidth)
                                                         .arg(Preferences::highlightStepColour));
                 SilhouetteMetaAdded = true;
              }
              if (argv[1] != LDRAW_EDGE_MATERIAL_COLOUR &&
                  argv[1] != LDRAW_MAIN_MATERIAL_COLOUR) {
                  QString colourCode;
                  // Insert color code for fade part
                  if (partType == FADE_PART)
                      colourCode = Preferences::fadeStepsUseColour ? fadeColour : argv[1];
                  // Insert color code for silhouette part
                  if (partType == HIGHLIGHT_PART)
                      colourCode = argv[1];
                  // generate fade color entry
                  if (!colourEntryExist(subfileColourList,argv[1], partType))
                      subfileColourList << createColourEntry(colourCode, partType);
                  // set color code - fade, highlight or both
                  argv[1] = QString("%1%2").arg(colourPrefix).arg(colourCode);
              }
              // process static colored parts
              QString fileNameStr = QString(argv[argv.size()-1]).toLower();
              if (ldrawColourParts.isLDrawColourPart(fileNameStr)){
                  fileNameStr = QDir::toNativeSeparators(fileNameStr.replace(".dat", "-" + nameMod + ".dat"));
                }
              // process subfile naming
              if (ldrawFile.isSubmodel(fileNameStr)) {
                  QString extension = QFileInfo(fileNameStr).suffix().toLower();
                  bool ldr = extension == "ldr";
                  bool mpd = extension == "mpd";
                  bool dat = extension == "dat";
                  if (ldr) {
                      fileNameStr = fileNameStr.replace(".ldr", "-" + nameMod + ".ldr");
                    } else if (mpd) {
                      fileNameStr = fileNameStr.replace(".mpd", "-" + nameMod + ".mpd");
                    } else if (dat) {
                      fileNameStr = fileNameStr.replace(".dat", "-" + nameMod + ".dat");
                    }
                }
              argv[argv.size()-1] = fileNameStr;
            }
          if (isGhost(contentLine))
              argv.prepend(GHOST_META);
          contentLine = argv.join(" ");
          configuredContents  << contentLine;

          // Insert closing fade and silhouette metas
          if (index+1 == contents.size()){
              if (FadeMetaAdded){
                 configuredContents.append(QString("0 !FADE"));
              }
              if (SilhouetteMetaAdded){
                 configuredContents.append(QString("0 !SILHOUETTE"));
              }
          }
      }
  } else {
    return contents;
  }
  // add the color list to the header of the configuredContents
  if (!subfileColourList.isEmpty()){
      subfileColourList.toSet().toList();  // remove dupes
      configuredContents.prepend("0");
      for (int i = 0; i < subfileColourList.size(); ++i)
          configuredContents.prepend(subfileColourList.at(i));
      configuredContents.prepend("0 // LPub3D step custom colours");
      configuredContents.prepend("0");
  }
  return configuredContents;
}

/*
 * Process csiParts list - fade previous step-parts and or highlight current step-parts.
 * To get the previous content position, take the previous cisFile file size.
 * The csiFile entries are only parts with not formatting or meta commands so it is
 * well suited to provide the delta between steps.
 */
QStringList Gui::configureModelStep(const QStringList &csiParts, const int &stepNum,  Where &current) {

  QStringList configuredCsiParts, stepColourList;
  bool doFadeStep  = page.meta.LPub.fadeStep.fadeStep.value();
  bool doHighlightStep = page.meta.LPub.highlightStep.highlightStep.value() && !suppressColourMeta();
  bool doHighlightFirstStep = Preferences::highlightFirstStep;
  bool FadeMetaAdded = false;
  bool SilhouetteMetaAdded = false;

  if (csiParts.size() > 0 && (doHighlightFirstStep ? true : stepNum > 1)) {

      QString fadeColour  = LDrawColor::ldColorCode(page.meta.LPub.fadeStep.fadeColor.value());

      // retrieve the previous step position
      int prevStepPosition = ldrawFile.getPrevStepPosition(current.modelName);
      if (prevStepPosition == 0 && savePrevStepPosition > 0)
          prevStepPosition = savePrevStepPosition;

      // save the current step position
      ldrawFile.setPrevStepPosition(current.modelName,csiParts.size());

      //qDebug() << "Model:" << current.modelName << ", Step:"  << stepNum << ", PrevStep Get Previous Step Position:" << prevStepPosition
      //         << ", CSI Size:" << csiParts.size() << ", Model Size:"  << ldrawFile.size(current.modelName);
      QStringList argv;

      for (int index = 0; index < csiParts.size(); index++) {

          bool ldr = false, mpd = false, dat= false;
          bool type_1_line = false;
          bool type_1_5_line = false;
          bool is_colour_part = false;
          bool is_submodel_file = false;

          int updatePosition = index+1;
          QString fileNameStr;
          QString csiLine = csiParts[index];
          split(csiLine, argv);

          // determine line type
          if (argv.size() && argv[0].size() == 1 &&
              argv[0] >= "1" && argv[0] <= "5") {
              type_1_5_line = true;
              if (argv.size() == 15 && argv[0] == "1")
                  type_1_line = true;
          }

          if (type_1_line){
              // process color parts naming
              fileNameStr = argv[argv.size()-1].toLower();

              // check if is color part
              is_colour_part = ldrawColourParts.isLDrawColourPart(fileNameStr);

              //if (is_colour_part)
              //    emit messageSig(LOG_NOTICE, "Static color part - " + fileNameStr);
          }

          // check if is submodel
          QString extension;
          if (ldrawFile.isSubmodel(fileNameStr)) {
                 is_submodel_file = true;
                 extension = QFileInfo(fileNameStr).suffix().toLower();
          }

          // write fade step entries
          if ((doHighlightFirstStep ? stepNum > 1 : true) && doFadeStep && (updatePosition <= prevStepPosition)) {
              if (type_1_5_line) {
                  // Insert opening fade meta
                  if (!FadeMetaAdded && Preferences::enableFadeSteps){
                     configuredCsiParts.insert(index,QString("0 !FADE %1").arg(Preferences::fadeStepsOpacity));
                     FadeMetaAdded = true;
                  }
                  if (argv[1] != LDRAW_EDGE_MATERIAL_COLOUR &&
                      argv[1] != LDRAW_MAIN_MATERIAL_COLOUR) {
                      // generate fade color entry
                      QString colourCode = Preferences::fadeStepsUseColour ? fadeColour : argv[1];
                      if (!colourEntryExist(stepColourList,argv[1], FADE_PART))
                        stepColourList << createColourEntry(colourCode, FADE_PART);
                      // set fade color code
                      argv[1] = QString("%1%2").arg(LPUB3D_COLOUR_FADE_PREFIX).arg(colourCode);
                  }
                  if (type_1_line) {
                        if (is_colour_part)
                               fileNameStr = QDir::toNativeSeparators(fileNameStr.replace(".dat", QString("%1.dat").arg(FADE_SFX)));
                        // process subfiles naming
                        if (is_submodel_file) {
                            if (extension.isEmpty()) {
                              fileNameStr = fileNameStr.append(QString("%1.ldr").arg(FADE_SFX));
                            } else {
                              fileNameStr = fileNameStr.replace("."+extension, QString("%1.%2").arg(FADE_SFX).arg(extension));
                            }
                        }
                        // assign fade part name
                        argv[argv.size()-1] = fileNameStr;
                  }
              }
          }
          // write highlight entries
          if (doHighlightStep && (updatePosition > prevStepPosition)) {
              if (type_1_5_line) {
                  // Insert opening silhouette meta
                  if (!SilhouetteMetaAdded && Preferences::enableHighlightStep){
                     configuredCsiParts.append(QString("0 !SILHOUETTE %1 %2")
                                                       .arg(Preferences::highlightStepLineWidth)
                                                       .arg(Preferences::highlightStepColour));
                     SilhouetteMetaAdded = true;
                  }
                  if (argv[1] != LDRAW_EDGE_MATERIAL_COLOUR &&
                      argv[1] != LDRAW_MAIN_MATERIAL_COLOUR) {
                      // generate fade color entry
                      QString colourCode = argv[1];
                      if (!colourEntryExist(stepColourList,argv[1], HIGHLIGHT_PART))
                        stepColourList << createColourEntry(colourCode, HIGHLIGHT_PART);
                      // set fade color code
                      argv[1] = QString("%1%2").arg(LPUB3D_COLOUR_HIGHLIGHT_PREFIX).arg(colourCode);
                  }
                  if (type_1_line) {
                        if (is_colour_part)
                               fileNameStr = QDir::toNativeSeparators(fileNameStr.replace(".dat", QString("%1.dat").arg(HIGHLIGHT_SFX)));
                        // process subfiles naming
                        if (is_submodel_file) {
                            if (extension.isEmpty()) {
                              fileNameStr = fileNameStr.append(QString("%1.ldr").arg(HIGHLIGHT_SFX));
                            } else {
                              fileNameStr = fileNameStr.replace("."+extension, QString("%1.%2").arg(HIGHLIGHT_SFX).arg(extension));
                            }
                        }
                        // assign fade part name
                        argv[argv.size()-1] = fileNameStr;
                  }
              }
          }

          if (isGhost(csiLine))
              argv.prepend(GHOST_META);

          // previous step parts
          csiLine = argv.join(" ");

          // current step parts
          configuredCsiParts  << csiLine;

          // Insert closing fade meta
          if (updatePosition == prevStepPosition) {
              if (FadeMetaAdded){
                 configuredCsiParts.append(QString("0 !FADE"));
              }
          }

          // Insert closing silhouette meta
          if (index+1 == csiParts.size()){
              if (SilhouetteMetaAdded){
                 configuredCsiParts.append(QString("0 !SILHOUETTE"));
              }
          }
        }
    } else {
      // save the current step position
      ldrawFile.setPrevStepPosition(current.modelName,csiParts.size());
      return csiParts;
    }

  // add the fade color list to the header of the CsiParts list
  if (!stepColourList.isEmpty()){
      stepColourList.toSet().toList();  // remove dupes
      configuredCsiParts.prepend("0");
      for (int i = 0; i < stepColourList.size(); ++i)
        configuredCsiParts.prepend(stepColourList.at(i));
      configuredCsiParts.prepend("0 // LPub3D step custom colours");
      configuredCsiParts.prepend("0");
  }

  return configuredCsiParts;
}

bool Gui::colourEntryExist(const QStringList &colourEntries, const QString &code, const PartType partType)
{
    bool fadePartType = partType == FADE_PART;

    if (Preferences::fadeStepsUseColour && fadePartType && colourEntries.size() > 0)
        return true;

    QStringList colourComponents;
    QString colourPrefix = fadePartType ? LPUB3D_COLOUR_FADE_PREFIX : LPUB3D_COLOUR_HIGHLIGHT_PREFIX;
    QString colourCode   = QString("%1%2").arg(colourPrefix).arg(code);
    for (int i = 0; i < colourEntries.size(); ++i){
        QString colourLine = colourEntries[i];
        split(colourLine,colourComponents);
        if (colourComponents.size() == 11 && colourComponents[4] == colourCode) {
            return true;
        }
    }
    return false;
}

QString Gui::createColourEntry(const QString &colourCode, const PartType partType)
{
  // Fade Step Alpha Percent (default = 100%) -  e.g. 50% of Alpha 255 rounded up we get ((255 * 50) + (100 - 1)) / 100

  bool fadePartType          = partType == FADE_PART;

  QString _colourPrefix      = fadePartType ? LPUB3D_COLOUR_FADE_PREFIX : LPUB3D_COLOUR_HIGHLIGHT_PREFIX;  // fade prefix 100, highlight prefix 110
  QString _fadeColour        = LDrawColor::ldColorCode(page.meta.LPub.fadeStep.fadeColor.value());
  QString _colourCode        = _colourPrefix + (fadePartType ? Preferences::fadeStepsUseColour ? _fadeColour : colourCode : colourCode);
  QString _mainColourValue   = "#" + ldrawColors.value(colourCode);
  QString _edgeColourValue   = fadePartType ? "#" + ldrawColors.edge(colourCode) : Preferences::highlightStepColour;
  QString _colourDescription = LPUB3D_COLOUR_TITLE_PREFIX + ldrawColors.name(colourCode);
  int _fadeAlphaValue        = ((ldrawColors.alpha(colourCode) * (100 - Preferences::fadeStepsOpacity)) + (100 - 1)) / 100;
  int _alphaValue            = fadePartType ? _fadeAlphaValue : ldrawColors.alpha(colourCode);             // use 100% opacity with highlight color

  return QString("0 !COLOUR %1 CODE %2 VALUE %3 EDGE %4 ALPHA %5")
                 .arg(_colourDescription)   // description
                 .arg(_colourCode)          // original color code
                 .arg(_mainColourValue)     // main color value
                 .arg(_edgeColourValue)     // edge color value
                 .arg(_alphaValue);         // color alpha value
}
