/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005 Gunnar Beutner                                           *
 *                                                                             *
 * This program is free software; you can redistribute it and/or               *
 * modify it under the terms of the GNU General Public License                 *
 * as published by the Free Software Foundation; either version 2              *
 * of the License, or (at your option) any later version.                      *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with this program; if not, write to the Free Software                 *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. *
 *******************************************************************************/

#if !defined(AFX_SOCKETEVENTS_H__E3357DBD_BBB7_4F6D_B406_08F652675369__INCLUDED_)
#define AFX_SOCKETEVENTS_H__E3357DBD_BBB7_4F6D_B406_08F652675369__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

struct CSocketEvents {
public:
	virtual void Destroy(void) = 0;

	virtual bool Read(void) = 0;
	virtual void Write(void) = 0;
	virtual void Error(void) = 0;
	virtual bool HasQueuedData(void) = 0;
};

#endif // !defined(AFX_SOCKETEVENTS_H__E3357DBD_BBB7_4F6D_B406_08F652675369__INCLUDED_)
