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

#ifndef __LDVALERTHANDLER_H__
#define __LDVALERTHANDLER_H__

#include <TCFoundation/TCObject.h>

class TCAlert;
class LDrawModelViewer;
class LDSnapshotTaker;

class LDVWidget;
class LDVAlertHandler : public TCObject
{
public:
	LDVAlertHandler(LDVWidget *ldvw);
protected:
	~LDVAlertHandler(void);
	virtual void dealloc(void);

	void snapshotTakerAlertCallback(TCAlert *alert);
	void modelViewerAlertCallback(TCAlert *alert);

	LDVWidget *m_ldvw;
};

#if defined(__APPLE__)

class LDVSnapshotTaker;
class LDVSAlertHandler : public TCObject
{
public:
	LDVSAlertHandler(LDVSnapshotTaker *ldvsw);
protected:
	~LDVSAlertHandler(void);
	virtual void dealloc(void);

	void snapshotTakerAlertCallback(TCAlert *alert);

	LDVSnapshotTaker *m_ldvsw;
};

#endif

#endif // __LDVALERTHANDLER_H__
