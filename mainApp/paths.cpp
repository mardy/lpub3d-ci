 

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

#include <QSettings>
#include <QDir>
#include "paths.h"
#include "lpub_preferences.h"

Paths paths;

QString Paths::lpubDir   = "LPub3D";
QString Paths::tmpDir    = "LPub3D/tmp";
QString Paths::assemDir  = "LPub3D/assem";
QString Paths::partsDir  = "LPub3D/parts";
QString Paths::viewerDir = "LPub3D/viewer";

QString Paths::fadeDir;
QString Paths::fadePartDir;
QString Paths::fadeSubDir;
QString Paths::fadePrimDir;
QString Paths::fadePrim8Dir;
QString Paths::fadePrim48Dir;

QStringList Paths::fadeDirs;


void Paths::mkdirs(){

    QDir dir;
    dir.mkdir(lpubDir);
    dir.mkdir(tmpDir);
    dir.mkdir(assemDir);
    dir.mkdir(partsDir);
    dir.mkdir(viewerDir);

}

void Paths::mkfadedirs(){

  QDir dir;

   if(! dir.exists(fadeDir)       ||
      ! dir.exists(fadePartDir)   ||
      ! dir.exists(fadeSubDir)    ||
      ! dir.exists(fadePrimDir)   ||
      ! dir.exists(fadePrim8Dir)  ||
      ! dir.exists(fadePrim48Dir)) {

      fadeDir = QDir::toNativeSeparators(Preferences::lpubDataPath + "/fade");
      if(! dir.exists(fadeDir))
        dir.mkdir(fadeDir);

      fadePartDir   = QDir::toNativeSeparators(fadeDir + "/parts");
      if (! dir.exists(fadePartDir))
        dir.mkdir(fadePartDir);

      fadeSubDir    = QDir::toNativeSeparators(fadeDir + "/parts/s");
      if (! dir.exists(fadeSubDir))
        dir.mkdir(fadeSubDir);

      fadePrimDir   = QDir::toNativeSeparators(fadeDir + "/p");
      if (! dir.exists(fadePrimDir))
        dir.mkdir(fadePrimDir);

      fadePrim8Dir  = QDir::toNativeSeparators(fadeDir + "/p/8");
      if (! dir.exists(fadePrim8Dir))
        dir.mkdir(fadePrim8Dir);

      fadePrim48Dir = QDir::toNativeSeparators(fadeDir + "/p/48");
      if (! dir.exists(fadePrim48Dir))
        dir.mkdir(fadePrim48Dir);

    } else {

      fadeDir       = QDir::toNativeSeparators(Preferences::lpubDataPath + "/fade");
      fadePartDir   = QDir::toNativeSeparators(fadeDir + "/parts");
      fadeSubDir    = QDir::toNativeSeparators(fadeDir + "/parts/s");
      fadePrimDir   = QDir::toNativeSeparators(fadeDir + "/p");
      fadePrim8Dir  = QDir::toNativeSeparators(fadeDir + "/p/8");
      fadePrim48Dir = QDir::toNativeSeparators(fadeDir + "/p/48");

    }

  fadeDirs << fadePartDir << fadeSubDir << fadePrimDir << fadePrim8Dir << fadePrim48Dir;
}

