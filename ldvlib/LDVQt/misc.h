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

#ifndef __MISC_H__
#define __MISC_H__

#include <string.h>
#ifdef _AIX
#include <strings.h>
#endif

#include <QtCore/qstring.h>
#include <TCFoundation/mystring.h>

void wcstoqstring(QString &dst, const wchar_t *src, int length = -1);
QString wcstoqstring(const wchar_t *src, int length = -1);
void wstringtoqstring(QString &dst, const std::wstring &src);
void ucstringtoqstring(QString &dst, const ucstring &src);
void qstringtoucstring(ucstring &dst, const QString &src);

#endif