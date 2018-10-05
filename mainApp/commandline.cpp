/****************************************************************************
**
** Copyright (C) 2018 Trevor SANDY. All rights reserved.
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

#include "application.h"
#include "lpub.h"

int Gui::processCommandLine()
{
  // 3DViewer
  int viewerJob = gApplication->Process3DViewerCommandLine();
  if (viewerJob < 0)
     return 1;
  else
  if (viewerJob > 0)
    return 0;

  // Declarations
  int  fadeStepsOpacity    = FADE_OPACITY_DEFAULT;
  float highlightLineWidth = HIGHLIGHT_LINE_WIDTH_DEFAULT;
  bool processExport       = false;
  bool processFile         = false;
  bool fadeSteps           = false;
  bool highlightStep       = false;
  bool imageMatting        = false;
  bool resetSearchDirs     = false;

  QString pageRange, exportOption,
          commandlineFile, preferredRenderer,
          fadeStepsColour, highlightStepColour;

  // Process parameters
  QStringList Arguments = Application::instance()->arguments();

  const int NumArguments = Arguments.size();
  for (int ArgIdx = 1; ArgIdx < NumArguments; ArgIdx++)
    {
      const QString& Param = Arguments[ArgIdx];

      if (Param[0] != '-')
      {
          commandlineFile = Param;
          continue;
      }

      if (Param == QLatin1String("-icr") ||
          Param == QLatin1String("--ignore-console-redirect"))
      {
          continue;
      }

      auto ParseString = [&ArgIdx, &Arguments, NumArguments](QString& Value, bool Required)
      {
          if (ArgIdx < NumArguments - 1 && Arguments[ArgIdx + 1][0] != '-')
            {
              ArgIdx++;
              Value = Arguments[ArgIdx];
            }
          else if (Required)
            printf("Not enough parameters for the '%s' argument.\n", Arguments[ArgIdx].toLatin1().constData());
        };

      auto ParseInteger = [&ArgIdx, &Arguments, NumArguments](int& Value)
      {
          if (ArgIdx < NumArguments - 1 && Arguments[ArgIdx + 1][0] != '-')
          {
              bool Ok = false;
              ArgIdx++;
              int NewValue = Arguments[ArgIdx].toInt(&Ok);

              if (Ok)
                  Value = NewValue;
              else
                  printf("Invalid value specified for the '%s' argument.\n", Arguments[ArgIdx - 1].toLatin1().constData());
          }
          else
              printf("Not enough parameters for the '%s' argument.\n", Arguments[ArgIdx].toLatin1().constData());
      };

      auto ParseFloat = [&ArgIdx, &Arguments, NumArguments](float& Value)
      {
          if (ArgIdx < NumArguments - 1 && Arguments[ArgIdx + 1][0] != '-')
          {
              bool Ok = false;
              ArgIdx++;
              int NewValue = Arguments[ArgIdx].toFloat(&Ok);

	      if (Ok)
		  Value = NewValue;
	      else
		  printf("Invalid value specified for the '%s' argument.\n", Arguments[ArgIdx - 1].toLatin1().constData());
	  }
	  else
	      printf("Not enough parameters for the '%s' argument.\n", Arguments[ArgIdx].toLatin1().constData());
      };

      if (Param == QLatin1String("-pf") || Param == QLatin1String("--process-file"))
        processFile = true;
      else
      if (Param == QLatin1String("-pe") || Param == QLatin1String("--process-export"))
        processExport = true;
      else
      if (Param == QLatin1String("-fs") || Param == QLatin1String("--fade-steps"))
        fadeSteps = true;
      else
      if (Param == QLatin1String("-fo") || Param == QLatin1String("--fade-step-opacity"))
        ParseInteger(fadeStepsOpacity);
      else
      if (Param == QLatin1String("-fc") || Param == QLatin1String("--fade-steps-color"))
        ParseString(fadeStepsColour, false);
      else
      if (Param == QLatin1String("-hs") || Param == QLatin1String("--highlight-step"))
        highlightStep = true;
      else
      if (Param == QLatin1String("-hc") || Param == QLatin1String("--highlight-step-color"))
        ParseString(highlightStepColour, false);
      else
//      if (Param == QLatin1String("-im") || Param == QLatin1String("--image-matte"))
//        imageMatting = true;
//      else
      if (Param == QLatin1String("-of") || Param == QLatin1String("--pdf-output-file"))
        ParseString(saveFileName, false);
      else
      if (Param == QLatin1String("-rs") || Param == QLatin1String("--reset-search-dirs"))
          resetSearchDirs = true;
      else
      if (Param == QLatin1String("-x") || Param == QLatin1String("--clear-cache"))
        resetCache = true;
      else
      if (Param == QLatin1String("-p") || Param == QLatin1String("--preferred-renderer"))
        ParseString(preferredRenderer, false);
      else
      if (Param == QLatin1String("-o") || Param == QLatin1String("--export-option"))
        ParseString(exportOption, false);
      else
      if (Param == QLatin1String("-d") || Param == QLatin1String("--image-output-directory"))
        ParseString(saveFileName, false);
      else
      if (Param == QLatin1String("-r") || Param == QLatin1String("--range"))
        ParseString(pageRange, false);
      else
      if (Param == QLatin1String("--line-width"))
        ParseFloat(highlightLineWidth);
      else
        emit messageSig(LOG_INFO,QString("Unknown commandline parameter: '%1'.").arg(Param));
    }

  if (!preferredRenderer.isEmpty()){
      //QSettings Settings;
      QString renderer;
      if (preferredRenderer.toLower() == "native"){
          renderer = RENDERER_NATIVE;
      }
      else
      if (preferredRenderer.toLower() == "ldview"){
          renderer = RENDERER_LDVIEW;
      }
      else
      if (preferredRenderer.toLower() == "ldview-sc"){
          renderer = RENDERER_LDVIEW;
          Preferences::enableLDViewSingleCall = true;
      }
      else
      if (preferredRenderer.toLower() == "ldglite"){
          renderer = RENDERER_LDGLITE;
      }
      else
      if (preferredRenderer.toLower() == "povray"){
          renderer = RENDERER_POVRAY;
      }
      else {
          emit messageSig(LOG_INFO,QString("Invalid renderer specified: '%1'.").arg(renderer));
          return 1;
      }
      if (Preferences::preferredRenderer != renderer) {
          QString message = QString("Renderer preference changed from %1 to %2 %3.")
                            .arg(Preferences::preferredRenderer)
                            .arg(renderer)
                            .arg(renderer == RENDERER_POVRAY ? QString("(POV file generator is %1)").arg(Preferences::povFileGenerator) :
                                 renderer == RENDERER_LDVIEW ? Preferences::enableLDViewSingleCall ? "(Single Call)" : "" : "");
          emit messageSig(LOG_INFO,message);
          Preferences::preferredRenderer = renderer;
          Render::setRenderer(Preferences::preferredRenderer);
      }
    }

  if (Preferences::pageGuides)
      Preferences::setPageGuidesPreference(false);

  if (Preferences::pageRuler)
      Preferences::setPageRulerPreference(false);

  if (fadeSteps && fadeSteps != Preferences::enableFadeSteps) {
      Preferences::enableFadeSteps = fadeSteps;
      QString message = QString("Fade Previous Steps set to ON.");
      emit messageSig(LOG_INFO,message);
      if (fadeStepsColour.isEmpty()) {
          if (Preferences::fadeStepsUseColour){
              Preferences::fadeStepsUseColour = false;
              QString message = QString("Use Global Fade Color set to OFF.");
              emit messageSig(LOG_INFO,message);
            }
        }
    }

  if ((fadeStepsOpacity != Preferences::fadeStepsOpacity) &&
      Preferences::enableFadeSteps) {
          QString message = QString("Fade Step Transparency changed from %1 to %2 percent.")
              .arg(Preferences::fadeStepsOpacity)
              .arg(fadeStepsOpacity);
          emit messageSig(LOG_INFO,message);
          Preferences::fadeStepsOpacity = fadeStepsOpacity;
    }

  if (!fadeStepsColour.isEmpty() && Preferences::enableFadeSteps) {
      if (!Preferences::fadeStepsUseColour){
          Preferences::fadeStepsUseColour = true;
          QString message = QString("Use Global Fade Color set to ON.");
          emit messageSig(LOG_INFO,message);
          fadeStepsOpacity = 100;
          if (fadeStepsOpacity != Preferences::fadeStepsOpacity ) {
              QString message = QString("Fade Step Transparency changed from %1 to %2 percent.")
                  .arg(Preferences::fadeStepsOpacity)
                  .arg(fadeStepsOpacity);
              emit messageSig(LOG_INFO,message);
              Preferences::fadeStepsOpacity = fadeStepsOpacity;
            }
        }
      if (LDrawColor::name(fadeStepsColour) != Preferences::fadeStepsColour) {
          QString message = QString("Fade Step Color preference changed from %1 to %2.")
              .arg(QString(Preferences::fadeStepsColour).replace("_"," "))
              .arg(QString(LDrawColor::name(fadeStepsColour)).replace("_"," "));
          emit messageSig(LOG_INFO,message);
          Preferences::fadeStepsColour = LDrawColor::name(fadeStepsColour);
        }
    }

  /* [Experimental] LDView Image Matting */
//  if (imageMatting && Preferences::enableFadeSteps &&
//      (Preferences::preferredRenderer == RENDERER_LDVIEW)) {
//      Preferences::enableImageMatting = imageMatting;
//      QString message = QString("Enable Image matte is ON.");
//      emit messageSig(LOG_INFO,message);
//    } else {
//      QString message;
//      if (imageMatting && !Preferences::enableFadeSteps) {
//          message = QString("Image matte requires fade previous steps set to ON.");
//          emit messageSig(LOG_ERROR,message);
//        }

//      if (imageMatting && (Preferences::preferredRenderer != RENDERER_LDVIEW)) {
//          message = QString("Image matte requires LDView renderer.");
//          emit messageSig(LOG_ERROR,message);
//        }
//      if (imageMatting) {
//          message = QString("Image matte flag will be ignored.");
//          emit messageSig(LOG_ERROR,message);
//        }
//    }

  if (highlightStep && highlightStep != Preferences::enableHighlightStep) {
      Preferences::enableHighlightStep = highlightStep;
      QString message = QString("Highlight Current Step set to ON.");
      emit messageSig(LOG_INFO,message);
    }

  if ((highlightStepColour != Preferences::highlightStepColour) &&
      !highlightStepColour.isEmpty() &&
      Preferences::enableHighlightStep) {
      QString message = QString("Highlight Step Color preference changed from %1 to %2.")
          .arg(Preferences::highlightStepColour)
          .arg(highlightStepColour);
      emit messageSig(LOG_INFO,message);
      Preferences::highlightStepColour = highlightStepColour;
    }

  if ((highlightLineWidth != Preferences::highlightStepLineWidth ) &&
      Preferences::enableHighlightStep) {
      QString message = QString("Highlight Line Width preference changed from %1 to %2.")
          .arg(Preferences::highlightStepLineWidth)
          .arg(highlightLineWidth);
      emit messageSig(LOG_INFO,message);
      Preferences::highlightStepLineWidth = highlightLineWidth;
    }

  if (resetSearchDirs) {
      QString message = QString("Reset search directories requested..");
      emit messageSig(LOG_INFO,message);

      // set fade step setting
      if (fadeSteps && fadeSteps != Preferences::enableFadeSteps) {
          Preferences::enableFadeSteps = fadeSteps;
        }
      // set highlight step setting
      if (highlightStep && highlightStep != Preferences::enableHighlightStep) {
          Preferences::enableHighlightStep = highlightStep;
        }
      partWorkerLDSearchDirs.resetSearchDirSettings();
    }

  QElapsedTimer commandTimer;
  if (!commandlineFile.isEmpty()) {
      commandTimer.start();
      if (!loadFile(commandlineFile)) {
          return 1;
        }
    }

  if (processPageRange(pageRange)) {
      if (processFile){
          Preferences::pageDisplayPause = 1;
          continuousPageDialog(PAGE_NEXT);
        } else
        if (processExport) {
            if (exportOption == "pdf")
               exportAsPdfDialog();
            else
            if (exportOption == "png")
               exportAsPngDialog();
            else
            if (exportOption == "jpg")
               exportAsJpgDialog();
            else
            if (exportOption == "bmp")
               exportAsBmpDialog();
            else
               exportAsPdfDialog();
          } else {
            continuousPageDialog(PAGE_NEXT);
          }
    } else
    return 1;

  emit messageSig(LOG_INFO,QString("Model file '%1' processed. %2.")
                          .arg(QFileInfo(commandlineFile).fileName())
                          .arg(gui->elapsedTime(commandTimer.elapsed())));
  return 0;
}
