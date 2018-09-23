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

#include "archiveparts.h"
#include "lpub_preferences.h"
#include "lpub.h"
#include "paths.h"

#include "lc_application.h"


ArchiveParts::ArchiveParts(QObject *parent) : QObject(parent)
{

}

/*
 * Insert static coloured fade parts into unofficial ldraw library
 *
 */
bool ArchiveParts::Archive(
    const QString &zipArchive,
    const QDir &dir,
    QString &result,
    const QString &comment,
    bool overwriteCustomPart) {

  //qDebug() << QString("\nProcessing %1 with comment: %2").arg(dir.absolutePath()).arg(comment);

  // Confirm archive exist or create new instance
  QFileInfo zipInfo(zipArchive);
  if (!zipInfo.exists()) {
      if (!QFile::copy(QString("%1/%2").arg(Preferences::dataLocation).arg(zipInfo.fileName()),zipInfo.absoluteFilePath())) {
         logError() << QString("%1 not found. But could not copy new instance.").arg(zipInfo.fileName());
      } else {
         logInfo() << QString("%1 not found. New instance of created.").arg(zipInfo.fileName());
      }
   }

  // Initialize the zip file
  QuaZip zip(zipArchive);
  zip.setFileNameCodec("IBM866");

  // Check if directory exists
  if (!dir.exists()) {
      result = QString("! Archive directory does not exist: %1").arg(dir.absolutePath());
      logError() << result;
      return false;
    }

  // We get the list of directory files and folders recursively
  QStringList dirFileList;
  RecurseAddDir(dir, dirFileList);

  // Check if file list is empty
  if (dirFileList.isEmpty()) {
      result = QString("! File directory is empty: %1").arg(dir.absolutePath());
      return false;
    }

  // Create an array of file objects consisting of QFileInfo
  QFileInfoList files;
  foreach (QString fileName, dirFileList) files << QFileInfo(fileName);

  // Initialize some variables
  QString parts           = "parts";
  QString primitives      = "p";
  QString fileNameWithRelativePath;
  bool setPartsDir        = false;
  bool setPrimDir         = false;

  // If input directory is 'p' use 'p' (primitive) else if directory is 'parts' (parts) use 'parts'
  if (dir.dirName().toLower() == primitives){
      setPrimDir = true;
    } else if (dir.dirName().toLower() == parts){
      setPartsDir = true;
    }

  // Check if the zip file exist; if yes, set to add content, and if no create
  QFileInfo zipFileInfo(zipArchive);
  QFileInfoList zipFiles;
  if (zipFileInfo.exists()){
      if (!zip.open(QuaZip::mdAdd)) {
          result = QString("! Cannot add to zip archive: %1").arg(zip.getZipError());
          return false;
        }

      // We get the list of files already in the archive.
      QStringList zipDirPaths;
      QStringList zipFileList;
      QStringList subDirs = dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs, QDir::SortByMask);
      // First check if we have any subdirs in the specified dir...
      if (subDirs.count() > 0 && (!setPartsDir && !setPrimDir)){
          //qDebug() << "---FIRST LEVEL SUBDIR LIST:        " << subDirs;

          foreach(QString subDirName, subDirs){

              QDir subDir(QString("%1/%2").arg(dir.absolutePath()).arg(subDirName));
              //qDebug() << "---PROCESSING FIRST LEVEL SUBDIR:  " << subDir.absolutePath();

              QDir excludeUnoffPartsDir(QString("%1/%2").arg(Preferences::ldrawPath).arg("unofficial/parts"));
              QDir excludeUnoffPrimDir(QString("%1/%2").arg(Preferences::ldrawPath).arg("unofficial/p"));
              QDir excludeOffPartsDir(QString("%1/%2").arg(Preferences::ldrawPath).arg("parts"));
              QDir excludeOffPrimDir(QString("%1/%2").arg(Preferences::ldrawPath).arg("p"));

              if ((subDir == excludeUnoffPartsDir) || (subDir == excludeUnoffPrimDir) ||
                  (subDir == excludeOffPartsDir) || (subDir == excludeOffPrimDir)) {
                  //qDebug() << "SKIPPING " << subDir.absolutePath();
                  continue;
                }

              QStringList subSubDirs = subDir.entryList(QDir::NoDotAndDotDot | QDir::Dirs, QDir::SortByMask);
              if (subSubDirs.count() > 0) {
                  //qDebug() << "---SECOND LEVEL SUBDIR LIST:       " << subSubDirs;

                  foreach(QString subSubDirName, subSubDirs) {
                      // reset prtsInDir flags
                      setPartsDir  = false;
                      setPrimDir   = false;

                      QDir subSubDir(QString("%1/%2").arg(subDir.absolutePath()).arg(subSubDirName));
                      //qDebug() << "---PROCESSING SECOND LEVEL SUBDIR: " << subSubDir.absolutePath();

                      setPartsDir = subSubDir.dirName().toLower() == parts;
                      setPrimDir  = subSubDir.dirName().toLower() == primitives;

                      if (setPrimDir)
                        zipDirPaths << primitives;
                      else if (setPartsDir)
                        zipDirPaths << parts;
                      else
                        zipDirPaths << parts;

//                      qDebug() << "\nCHECK IF ARCHIVE EXIST (SECOND LEVEL DIR - SUBS (P/PART DIRS)): "
//                                 << "\npartsInDir:               " << setPartsDir
//                                 << "\nprimInDir:                " << setPrimDir
//                                 << "\nsubSubDir.absolutePath(): " << subSubDir.absolutePath()
//                                 << "\nzipDirPaths:              " << zipDirPaths
//                                    ;

                      foreach (QString zipDirPath, zipDirPaths){
                          RecurseZipArchive(zipFileList, zipDirPath, zipArchive, subSubDir/*dirRelativePath*/);
                        }

                      //Create an array of archive file objects consisting of QFileInfo
                      foreach (QString zipFile, zipFileList) zipFiles << QFileInfo(zipFile);

                      zipDirPaths.clear();
                    }
                }
              // No second level sub directories detected - default to zipDir 'parts'
              zipDirPaths << parts;

//              qDebug() << "\nCHECK IF ARCHIVE EXIST (FIRST LEVEL DIR - SUB (W/O P/PART DIR)): "
//                         << "\nsetPartsDir:           " << (setPartsDir?"True":"False")
//                         << "\nsetPrimDir:            " << (setPrimDir?"True":"False")
//                         << "\nsubDir.absolutePath(): " << subDir.absolutePath()
//                         << "\nzipDirPaths:           " << zipDirPaths
//                            ;

              foreach (QString zipDirPath, zipDirPaths){
                  RecurseZipArchive(zipFileList, zipDirPath, zipArchive, subDir/*dirRelativePath*/);
                }
              //Create an array of archive file objects consisting of QFileInfo
              foreach (QString zipFile, zipFileList) zipFiles << QFileInfo(zipFile);

              zipDirPaths.clear();
            }
        }

      // No sub directories detected - use parts
      if (setPrimDir)
        zipDirPaths << primitives;
      else if (setPartsDir)
        zipDirPaths << parts;
      else
        zipDirPaths << parts;

//      qDebug() << "\nCHECK IF ARCHIVE EXIST (ROOT LEVEL DIR - NO SUBS): "
//                 << "\nsetPartsDir:   " << setPartsDir
//                 << "\nsetPrimDir:    " << setPrimDir
//                 << "\ndir:           " << dir.absolutePath()
//                 << "\nzipDirPaths:   " << zipDirPaths
//                    ;

      foreach (QString zipDirPath, zipDirPaths){
          RecurseZipArchive(zipFileList, zipDirPath, zipArchive, dir);
        }

      //Create an array of archive file objects consisting of QFileInfo
      foreach (QString zipFile, zipFileList) zipFiles << QFileInfo(zipFile);

      zipDirPaths.clear();

    } else {

      if (!zip.open(QuaZip::mdCreate)) {
          result = QString("! Cannot create zip archive: %1").arg(zip.getZipError());
          return false;
        }
    }

  // Archive each disk file as necessary
//  qDebug() << "PROCESSING DISK FILES FOR ARCHIVE";
  QFile inFile;
  QuaZipFile outFile(&zip);
  int archivedPartCount = 0;

  char c;
  foreach(QFileInfo fileInfo, files) {

      //qDebug() << "Processing Disk File Name: " << fileInfo.absoluteFilePath();
      if (!fileInfo.isFile())
        continue;

      bool alreadyArchived = false;
      QString partStatus = "Archive entry";

      foreach (QFileInfo zipFileInfo, zipFiles) {
          if (fileInfo == zipFileInfo) {
              bool okToOverwrite = (zipFileInfo.fileName().contains("-fade.dat") || zipFileInfo.fileName().contains("-highlight.dat"));
              if (overwriteCustomPart && okToOverwrite) {
                  partStatus = "Archive entry (Overwritten)";
//                    qDebug() << "FileMatch - Overwriting Fade File !! " << fileInfo.absoluteFilePath();
              } else {
                  alreadyArchived = true;
//                    qDebug() << "FileMatch - Skipping !! " << fileInfo.absoluteFilePath();
              }
              break;
          }
      }

      if (alreadyArchived)
        continue;
      else
         archivedPartCount++;

      logInfo() << QString("Detected %1 part #%2: %3").arg(partStatus).arg(archivedPartCount).arg(fileInfo.fileName());

      int partsDirIndex    = fileInfo.absoluteFilePath().indexOf("/parts/",0,Qt::CaseInsensitive);
      int primDirIndex     = fileInfo.absoluteFilePath().indexOf("/p/",0,Qt::CaseInsensitive);

      setPartsDir    = partsDirIndex != -1;
      setPrimDir     = primDirIndex != -1;

      if (setPartsDir){
          fileNameWithRelativePath = fileInfo.absoluteFilePath().remove(0, partsDirIndex + 1);
          //qDebug() << "Adjusted Parts fileNameWithRelativePath: " << fileNameWithRelativePath;
        } else if (setPrimDir){
          fileNameWithRelativePath = fileInfo.absoluteFilePath().remove(0, primDirIndex + 1);
          //qDebug() << "Adjusted Primitive fileNameWithRelativePath: " << fileNameWithRelativePath;
        } else {
          fileNameWithRelativePath = fileInfo.fileName();
          //qDebug() << "Adjusted Root fileNameWithRelativePath: " << fileNameWithRelativePath;
        }

      QString fileNameWithCompletePath;

      if (setPartsDir || setPrimDir){
          fileNameWithCompletePath = fileNameWithRelativePath;
//          qDebug() << "fileNameWithCompletePath (ROOT PART/PRIMITIVE) " << fileNameWithCompletePath;
        } else {
          fileNameWithCompletePath = QString("%1/%2").arg(parts).arg(fileNameWithRelativePath);
//            qDebug() << "fileNameWithCompletePath (PART - DEFAULT)" << fileNameWithCompletePath;
        }

      inFile.setFileName(fileInfo.filePath());

      if (!inFile.open(QIODevice::ReadOnly)) {
          result = QString("inFile open error: %1").arg(inFile.errorString().toLocal8Bit().constData());
          return false;
      }

      if (!outFile.open(QIODevice::WriteOnly, QuaZipNewInfo(fileNameWithCompletePath, fileInfo.filePath()))) {
          result = QString("outFile open error: %1").arg(outFile.getZipError());
          return false;
        }

      while (inFile.getChar(&c) && outFile.putChar(c));

      if (outFile.getZipError() != UNZ_OK) {
          result = QString("outFile error: %1").arg(outFile.getZipError());
          return false;
        }

      outFile.close();

      if (outFile.getZipError() != UNZ_OK) {
          result = QString("outFile close error: %1").arg(outFile.getZipError());
          return false;
        }

      inFile.close();
    }

  if (!comment.isEmpty())
    zip.setComment(comment);

  zip.close();

  if (zip.getZipError() != 0) {
      result = QString("zip error: %1").arg(zip.getZipError());
      return false;
    }

  result = QString::number(archivedPartCount);
  return true;
}

/* Recursively searches files in the archive for a given directory \ a, and adds to the list of \ b */
bool ArchiveParts::RecurseZipArchive(QStringList &zipDirFileList, QString &zipDirPath, const QString &zipArchive, const QDir &dir) {

  QuaZip zip(zipArchive);
  QString parts      = "parts";
  QString primitives = "p";

  if (!zip.open(QuaZip::mdUnzip)) {
      logError() << QString("zip open error: %1 @ %2").arg(zip.getZipError()).arg(zipArchive);
      return false;
    }

  zip.setFileNameCodec("IBM866");

  QString entryPoint;
  if (zipDirPath.toLower() == parts || zipDirPath.toLower() == primitives)
    entryPoint = QString("/%1").arg(zipDirPath);
  else
    entryPoint = zipDirPath;

  QuaZipDir zipDir(&zip,entryPoint);

//  logInfo() << QString("Global zip archive entries: %1").arg(zip.getEntriesCount());
//  logInfo() << QString("Global zip comment: %1").arg(zip.getComment().toLocal8Bit().constData());
//  logInfo() << QString("Archive Subfolder %1: %2").arg(zipDir.dirName()).arg(zipDir.exists() ? "Found" : "Not Found");
//  logInfo() << QString("Disk File Absolute Path: %1").arg(dir.absolutePath());

  bool zipDirOk = zipDir.exists();

  QString zipDirMsg = QString("Unofficial archive subfolder '%1' %2.")
                      .arg(zipDir.dirName())
                      .arg(zipDirOk ? QString(QString("contains %1 %2")
                                      .arg(zipDir.count())
                                      .arg(zipDir.count() > 1 ? "entries" : "entry"))
                                      : "not found - zip may be corrupt.");
  if (zipDirOk) {
    logInfo() << zipDirMsg;
  } else {
    logError() << zipDirMsg;
  }

  if (zipDirOk) {

      zipDir.cd(zipDirPath);

//      logInfo() << QString("%1 Archive Subfolder Entries in '%2'").arg(zipDir.count()).arg(zipDirPath);

      QStringList qsl = zipDir.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files, QDir::SortByMask);

      foreach (QString zipFile, qsl) {

          if (!dir.exists())
            continue;

          QFileInfo zipFileInfo(QString("%1/%2").arg(dir.absolutePath()).arg(zipFile));

          if (!zipFileInfo.exists())
            continue;

          if (zipFileInfo.isSymLink()) {
            logError() << QString("Unofficial archive subfolder entrylist returned a symbolic link.");
            return false;
          }

          if(zipFileInfo.isDir()){

              QString subDirPath = QString("%1/%2").arg(zipDirPath).arg(zipFile);

              QDir subDir(zipFileInfo.filePath());

              RecurseZipArchive(zipDirFileList, subDirPath, zipArchive, subDir);

            } else {

              zipDirFileList << zipFileInfo.filePath();
              //logNotice() << "VALID FILE zipDirFileList: " << zipFileInfo.filePath();

            }
        }
    }

  zip.close();

  if (zip.getZipError() != UNZ_OK) {
      logError() << QString("zip close error: %1").arg(zip.getZipError());
      return false;
    }

  return true;
}

/* Recursively searches for all files on the disk \ a, and adds to the list of \ b */
void ArchiveParts::RecurseAddDir(const QDir &dir, QStringList &list) {

  QString offPartsDir = QDir::toNativeSeparators(QString("%1/%2").arg(Preferences::ldrawPath).arg("parts"));
  QString offPrimsDir = QDir::toNativeSeparators(QString("%1/%2").arg(Preferences::ldrawPath).arg("p"));
  QString unoffPartsDir = QDir::toNativeSeparators(QString("%1/%2").arg(Preferences::ldrawPath).arg("unofficial/parts"));
  QString unoffPrimsDir = QDir::toNativeSeparators(QString("%1/%2").arg(Preferences::ldrawPath).arg("unofficial/p"));

  QStringList qsl = dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);

  foreach (QString file, qsl) {

      QString filePath = QDir::toNativeSeparators(QString("%1/%2").arg(dir.absolutePath()).arg(file));
      if (
          filePath.toLower().contains(offPartsDir.toLower()) ||
          filePath.toLower().contains(offPrimsDir.toLower()) ||
          filePath.toLower().contains(unoffPartsDir.toLower()) ||
          filePath.toLower().contains(unoffPrimsDir.toLower())
          ) {
          //qDebug() << "\nLDRAW EXCLUDED DIR FILES: " << filePath;
          logError() << QString("Encountered excluded LDraw directory: %1.").arg(filePath);
          continue;
        }

      QFileInfo finfo(filePath);

      if (finfo.isSymLink()) {
        logError() << QString("Encountered a symbolic link: %1").arg(finfo.absoluteFilePath());
        continue;
      }

      if (finfo.isDir()) {

          //logInfo() << "FILE INFO DIR PATH: " << finfo.fileName();
          QDir subDir(finfo.filePath());
          RecurseAddDir(subDir, list);

        } else if (finfo.suffix().toLower() == "dat") {

          //qDebug() << "\nLDRAW INCLUDED DIR FILES: " << finfo.filePath();
          list << finfo.filePath();

       }

    }
}
