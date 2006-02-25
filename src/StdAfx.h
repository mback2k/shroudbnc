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

#include "../config.h"

#ifdef _MSC_VER
#pragma warning ( disable : 4996 )

#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#define _USE_32BIT_TIME_T
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <typeinfo>

#ifdef _WIN32
#	include "win32.h"
#else
#	include "unix.h"
#endif

#ifndef _MSC_VER
#	ifdef DLL_EXPORT
#		undef DLL_EXPORT
#		define WAS_DLL_EXPORT
#	endif
#	include "libltdl/ltdl.h"
#	ifdef WAS_DLL_EXPORT
#		define DLL_EXPORT
#	endif
#endif

#ifndef _WIN32
typedef lt_dlhandle HMODULE;
#endif

#ifdef USESSL
#	include <openssl/ssl.h>
#	include <openssl/err.h>
#else
	typedef void SSL;
	typedef void SSL_CTX;
	typedef void X509;
	typedef void X509_STORE_CTX;
#endif

#if defined(HAVE_AF_INET6) && defined(HAVE_STRUCT_IN6_ADDR) && defined(HAVE_STRUCT_SOCKADDR_IN6)
#	define IPV6
#endif

#include "snprintf.h"

#ifndef SWIG
#	include "c-ares/ares.h"
#endif

#ifdef SBNC
#ifdef _DEBUG
void *DebugMalloc(size_t Size, const char *File);
void DebugFree(void *Pointer, const char *File);
void *DebugReAlloc(void *Pointer, size_t NewSize, const char *File);
char *DebugStrDup(const char *String, const char *File);

#define malloc(Size) DebugMalloc(Size, __FILE__)
#define free(Pointer) DebugFree(Pointer, __FILE__)
#define realloc(Pointer, NewSize) DebugReAlloc(Pointer, NewSize, __FILE__)
#define strdup(String) DebugStrDup(String, __FILE__)
#endif
#endif

#ifdef LEAKLEAK
#define GC_DEBUG
#include "gc.h"
#define malloc(n) GC_MALLOC(n)
#define calloc(m,n) GC_MALLOC((m)*(n))
#define free(p) GC_FREE(p)
#define realloc(p,n) GC_REALLOC((p),(n))
#define CHECK_LEAKS() GC_gcollect()
#endif

// Do NOT use sprintf.
#define sprintf __evil_function

#include "sbncloader/AssocArray.h"

#include "sbnc.h"
#include "Result.h"
#include "Zone.h"
#include "Vector.h"
#include "Debug.h"
#include "Hashtable.h"
#include "utility.h"
#include "OwnedObject.h"
#include "SocketEvents.h"
#include "DnsEvents.h"
#include "Timer.h"
#include "Connection.h"
#include "ClientConnection.h"
#include "IRCConnection.h"
#include "User.h"
#include "Core.h"
#include "Log.h"
#include "Config.h"
#include "ModuleFar.h"
#include "Module.h"
#include "Banlist.h"
#include "Channel.h"
#include "Nick.h"
#include "Keyring.h"
#include "IdentSupport.h"
#include "Match.h"
#include "TrafficStats.h"
#include "FIFOBuffer.h"
#include "Queue.h"
#include "FloodControl.h"
#include "Listener.h"
#include "Persistable.h"
