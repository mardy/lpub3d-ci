
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
 * This class reads in, manages and writes out LDraw files.  While being
 * edited an MPD file, or a top level LDraw files and any sub-model files
 * are maintained in memory using this class.
 *
 * Please see lpub.h for an overall description of how the files in LPub
 * make up the LPub program.
 *
 ***************************************************************************/

#ifndef LDRAWFILES_H
#define LDRAWFILES_H

#include <QStringList>
#include <QString>
#include <QMap>
#include <QDateTime>
#include <QList>

#include "excludedparts.h"
#include "QsLog.h"

extern QList<QRegExp> LDrawHeaderRegExp;
extern QList<QRegExp> LDrawUnofficialPartRegExp;
extern QList<QRegExp> LDrawUnofficialSubPartRegExp;
extern QList<QRegExp> LDrawUnofficialPrimitiveRegExp;
extern QList<QRegExp> LDrawUnofficialOtherRegExp;

class LDrawSubFile {
  public:
    QStringList _contents;
    QString     _subFilePath;
    bool        _modified;
    QDateTime   _datetime;
    QStringList _renderedKeys;
    QStringList _mirrorRenderedKeys;
    int         _numSteps;
    bool        _beenCounted;
    int         _instances;
    int         _mirrorInstances;
    bool        _rendered;
    bool        _mirrorRendered;
    bool        _changedSinceLastWrite;
    bool        _generated;
    int         _prevStepPosition;
    int         _startPageNumber;
    int         _unofficialPart;

    LDrawSubFile()
    {
      _unofficialPart = 0;
    }
    LDrawSubFile(
            const QStringList &contents,
            QDateTime         &datetime,
            int                unofficialPart,
            bool               generated = false,
            const QString     &subFilePath = QString());
    ~LDrawSubFile()
    {
      _contents.clear();
    }
};

class ViewerStep {
  public:
    QStringList _rotatedContents;
    QStringList _unrotatedContents;
    QString   	_filePath;
    QString     _csiKey;
    bool        _modified;
    bool        _multiStep;
    bool        _calledOut;

    ViewerStep()
    {
      _modified = false;
    }
    ViewerStep(
      const QStringList &rotatedContents,
      const QStringList &unrotatedContents,
      const QString     &filePath,
      const QString     &csiKey,
      bool               multiStep,
      bool               calledOut);
    ~ViewerStep()
    {
      _rotatedContents.clear();
      _unrotatedContents.clear();
    }
};

class LDrawFile {
  private:
    QMap<QString, LDrawSubFile> _subFiles;
    QMap<QString, ViewerStep>   _viewerSteps;
    QMultiHash<QString, int>    _ldcadGroups;
    QStringList                 _emptyList;
    QString                     _emptyString;
    bool                        _mpd;
    static int                  _emptyInt;

    ExcludedParts               excludedParts; // internal list of part count excluded parts

    bool topLevelFileNotCaptured;
    bool topLevelNameNotCaptured;
    bool topLevelAuthorNotCaptured;
    bool topLevelDescriptionNotCaptured;
    bool topLevelCategoryNotCaptured;
    bool unofficialPart;
    bool topLevelModel;
    bool ldcadGroupsLoaded;
    int  descriptionLine;

  public:
    LDrawFile();
    ~LDrawFile()
    {
      _subFiles.empty();
      _viewerSteps.empty();
      _ldcadGroups.empty();
    }

    QStringList                 _subFileOrder;
    static QStringList          _loadedParts;
    static QString              _file;
    static QString              _name;
    static QString              _author;
    static QString              _description;
    static QString              _category;
    static bool                 _currFileIsUTF8;
    static int                  _partCount;
    static bool                 _showLoadMessages;
    static bool                 _loadAborted;

    int getPartCount(){
      return _partCount;
    }

    static void showLoadMessages();

    bool saveFile(const QString &fileName);
    bool saveMPDFile(const QString &filename);
    bool saveLDRFile(const QString &filename);

    void insert(const QString       &fileName,
                      QStringList   &contents,
                      QDateTime     &datetime,
                      bool           unofficialPart,
                      bool           generated = false,
                      const QString &subFilePath = QString());

    int  size(const QString &fileName);
    void empty();

    QStringList getSubFilePaths();
    QStringList contents(const QString &fileName);
    void setSubFilePath(const QString &mcFileName,
                     const QString &subFilePath);
    void setContents(const QString     &fileName, 
                     const QStringList &contents);
    void setPrevStepPosition(const QString &mcFileName,
                         const int     &prevStepPosition);
    int getPrevStepPosition(const QString &mcFileName);
    void clearPrevStepPositions();
    void setModelStartPageNumber(const QString &mcFileName,
                         const int     &startPageNumber);
    int getModelStartPageNumber(const QString &mcFileName);
    void subFileLevels(QStringList &contents, int &level);
    int loadFile(const QString &fileName);
    void loadMPDFile(const QString &fileName, QDateTime &datetime);
    void loadLDRFile(const QString &path, const QString &fileName);
    QStringList subFileOrder();
    
    QString readLine(const QString &fileName, int lineNumber);
    void insertLine( const QString &fileName, int lineNumber, const QString &line);
    void replaceLine(const QString &fileName, int lineNumber, const QString &line);
    void deleteLine( const QString &fileName, int lineNumber);
    void changeContents(const QString &fileName, 
                              int      position, 
                              int      charsRemoved, 
                        const QString &charsAdded);

    bool isMpd();
    QString topLevelFile();
    int isUnofficialPart(const QString &name);
    int numSteps(const QString &fileName);
    QDateTime lastModified(const QString &fileName);
    int fileOrderIndex(const QString &file);
    bool contains(const QString &file);
    bool isSubmodel(const QString &file);
    bool modified();
    bool modified(const QString &fileName);
    bool older(const QStringList &parsedStack,
               const QDateTime &lastModified);
    static bool mirrored(const QStringList &tokens);
    void unrendered();
    void setRendered(
            const QString &fileName,
            bool           mirrored,
            const QString &renderParentModel,
            int            renderStepNumber,
            int            howCounted);
    bool rendered(
            const QString &fileName,
            bool           mirrored,
            const QString &renderParentModel,
            int            renderStepNumber,
            int            howCounted);
    int instances(const QString &fileName, bool mirrored);
    void countParts(const QString &fileName);
    void countInstances();
    void countInstances(const QString &fileName, bool mirrored, const bool callout = false);
    bool changedSinceLastWrite(const QString &fileName);
    void tempCacheCleared();

    void insertLDCadGroup(const QString &name, int lid);
    bool ldcadGroupMatch(const QString &name, const QStringList &lids);

    /* ViewerStep functions */
    void insertViewerStep(const QString     &fileName,
                          const QStringList &rotatedContents,
                          const QStringList &unrotatedContents,
                          const QString     &filePath,
                          const QString     &csiKey,
                          bool               multiStep,
                          bool               calledOut);
    void updateViewerStep(const QString     &fileName,
                          const QStringList &contents,
                          bool rotated = true);
    QStringList getViewerStepRotatedContents(const QString &fileName);
    QStringList getViewerStepUnrotatedContents(const QString &fileName);
    QString     getViewerStepFilePath(const QString &fileName);
    QString     getViewerConfigKey(const QString &fileName);
    bool        isViewerStepMultiStep(const QString &fileName);
    bool        isViewerStepCalledOut(const QString &fileName);
    bool        viewerStepContentExist(      const QString &fileName);
    void        clearViewerSteps();
};

int split(const QString &line, QStringList &argv);
int validSoQ(const QString &line, int soq);
int  getUnofficialFileType(QString &line);
bool isHeader(QString &line);
bool isComment(QString &line);
bool isGhost(QString &line);
bool isSubstitute(QString &line, QString &lineOut);

#endif
