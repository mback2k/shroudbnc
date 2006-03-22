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

#ifndef SBNC
#define SBNC
#endif

#include "StdAfx.h"

CCore *g_Bouncer = NULL;
loaderparams_t *g_LoaderParameters;

#if defined(IPV6) && defined(__MINGW32__)
const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
#endif

/**
 * sbncLoad
 *
 * Used by "sbncloader" to start shroudBNC
 */
extern "C" EXPORT int sbncLoad(loaderparams_t *Parameters) {
	CConfig *Config;

	if (Parameters->Version < 201) {
		printf("Incompatible loader version. Expected version 201, got %d.\n", Parameters->Version);

		return 1;
	}

	srand((unsigned int)time(NULL));

#ifndef _WIN32
	if (getuid() == 0 || geteuid() == 0 || getgid() == 0 || getegid() == 0) {
		printf("You cannot run shroudBNC as 'root'. Use an ordinary user account and remove the suid bit if it is set.\n");
		return 1;
	}

	rlimit core_limit = { INT_MAX, INT_MAX };
	setrlimit(RLIMIT_CORE, &core_limit);
#endif

#if !defined(_WIN32 ) || defined(__MINGW32__)
	lt_dlinit();
#endif

	time(&g_CurrentTime);

	Config = new CConfig(Parameters->BuildPath("sbnc.conf", NULL));

	if (Config == NULL) {
		printf("Fatal: could not create config object.");

#if !defined(_WIN32 ) || defined(__MINGW32__)
		lt_dlexit();
#endif

		return 1;
	}

	g_LoaderParameters = Parameters;

	// constructor sets g_Bouncer
	new CCore(Config, Parameters->argc, Parameters->argv);

	if (Parameters->Box) {
		g_Bouncer->Thaw(Parameters->Box);
	}

#if !defined(_WIN32)
	signal(SIGPIPE, SIG_IGN);
#endif

	g_Bouncer->StartMainLoop();

	if (g_Bouncer != NULL) {
		if (g_Bouncer->GetStatus() == STATUS_FREEZE) {
			CAssocArray *Box;

			Parameters->GetBox(&Box);

			g_Bouncer->Freeze(Box);
		} else {
			delete g_Bouncer;
		}
	}

	delete Config;

#if !defined(_WIN32 ) || defined(__MINGW32__)
	lt_dlexit();
#endif

	return 0;
}

/**
 * sbncSetStatus
 *
 * Used by "sbncloader" to notify shroudBNC of status changes.
 */
extern "C" EXPORT bool sbncSetStatus(int Status) {
	if (g_Bouncer != NULL) {
		g_Bouncer->SetStatus(Status);
	}

	return true;
}

/* for debugging */
const char *debugGetModulePath(void) {
	return "./lib-0/";
}

const char *debugBuildPath(const char *Filename, const char *Base) {
	static char *Path = NULL;

	if (Filename[0] == '/') {
		return Filename;
	}

	if (Base == NULL) {
		return Filename;
	}

	free(Path);

	Path = (char *)malloc(strlen(Filename) + strlen(Base) + 2);
	strcpy(Path, Base);
	strcat(Path, "/");
	strcat(Path, Filename);

	return Path;
}

void debugSigEnable(void) {}

int main(int argc, char **argv) {
	loaderparams_s p;

	p.Version = 201;
	p.argc = argc;
	p.argv = argv;
	p.basepath = ".";
	p.BuildPath = debugBuildPath;
	p.GetModulePath = debugGetModulePath;
	p.SigEnable = debugSigEnable;
	p.Box = NULL;

	sbncLoad(&p);

	return 0;
}
