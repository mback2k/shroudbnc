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

#include "StdAfx.h"
#include "SocketEvents.h"
#include "Connection.h"
#include "ClientConnection.h"
#include "IRCConnection.h"
#include "BouncerUser.h"
#include "BouncerConfig.h"
#include "BouncerLog.h"
#include "BouncerCore.h"
#include "ModuleFar.h"
#include "Module.h"
#include "Channel.h"
#include "Nick.h"
#include "utility.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CIRCConnection::CIRCConnection(SOCKET Socket, sockaddr_in Peer, CBouncerUser* Owning) : CConnection(Socket, Peer) {
	m_State = State_Connecting;

	m_CurrentNick = NULL;

	WriteLine("NICK %s", Owning->GetNick());
	WriteLine("USER %s \"\" \"fnords\" :%s", Owning->GetUsername(), Owning->GetRealname());

	m_Owner = Owning;

	m_Channels = NULL;
	m_ChannelCount = 0;

	m_Server = NULL;

	m_ServerVersion = NULL;
	m_ServerFeat = NULL;

	m_LastBurst = time(NULL);

	m_ISupport = new CBouncerConfig(NULL);

	m_ISupport->WriteString("CHANMODES", "b,l,ntis");
	m_ISupport->WriteString("CHANTYPES", "#&");
	m_ISupport->WriteString("PREFIX", "(ohv)@%+");

	g_Bouncer->RegisterSocket(Socket, (CSocketEvents*)this);
}

CIRCConnection::~CIRCConnection() {
	free(m_CurrentNick);

	for (int i = 0; i < m_ChannelCount; i++)
		delete m_Channels[i];

	free(m_Channels);
}

connection_role_e CIRCConnection::GetRole(void) {
	return Role_IRC;
}

bool CIRCConnection::ParseLineArgV(int argc, const char** argv) {
	const char* Reply = argv[0];
	const char* Raw = argv[1];
	char* Nick = ::NickFromHostmask(Reply);

	bool b_Me = false;
	if (m_CurrentNick && Nick && strcmpi(Nick, m_CurrentNick) == 0)
		b_Me = true;

	free(Nick);

	if (!GetOwningClient()->GetClientConnection() && atoi(Raw) == 433) {
		WriteLine("NICK :%s`", GetOwningClient()->GetNick());
		return false;
	} else if (argc > 3 && (strcmpi(Raw, "privmsg") == 0 && !GetOwningClient()->GetClientConnection())) {
		const char* Nick = argv[2];

		if (Nick && strcmpi(Nick, m_CurrentNick) == 0) {
			GetOwningClient()->Log("%s: %s", Reply, argv[3]);
		}

		UpdateHostHelper(Reply);
	} else if (argc > 2 && strcmpi(Raw, "join") == 0) {
		if (b_Me) {
			AddChannel(argv[2]);

			if (!m_Owner->GetClientConnection())
				WriteLine("MODE %s", argv[2]);
		} else {
			CChannel* Chan = GetChannel(argv[2]);

			if (Chan) {
				Nick = NickFromHostmask(Reply);
				Chan->AddUser(Nick, '\0');
				free(Nick);
			}
		}

		UpdateHostHelper(Reply);
	} else if (argc > 2 && strcmpi(Raw, "part") == 0) {
		if (b_Me)
			RemoveChannel(argv[2]);
		else {
			CChannel* Chan = GetChannel(argv[2]);

			if (Chan) {
				Nick = NickFromHostmask(Reply);
				Chan->RemoveUser(Nick);
				free(Nick);
			}
		}

		UpdateHostHelper(Reply);
	} else if (argc > 2 && strcmpi(Raw, "kick") == 0) {
		if (strcmpi(argv[3], m_CurrentNick) == 0) {
			RemoveChannel(argv[2]);

			if (GetOwningClient()->GetClientConnection() == NULL) {
				WriteLine("JOIN %s", argv[2]);
			}
		} else {
			CChannel* Chan = GetChannel(argv[2]);

			if (Chan)
				Chan->RemoveUser(argv[3]);
		}

		UpdateHostHelper(Reply);
	} else if (argc > 2 && atoi(Raw) == 1) {
		CClientConnection* Client = GetOwningClient()->GetClientConnection();

		if (Client) {
			if (strcmp(Client->GetNick(), argv[2]) != 0) {
				Client->WriteLine(":%s!ident@sbnc NICK :%s", Client->GetNick(), argv[2]);
			}
		}

		free(m_CurrentNick);
		m_CurrentNick = strdup(argv[2]);

		free(m_Server);
		m_Server = strdup(Reply);
	} else if (argc > 1 && strcmpi(Raw, "nick") == 0) {
		if (b_Me) {
			free(m_CurrentNick);
			m_CurrentNick = strdup(argv[2]);
		}

		Nick = NickFromHostmask(argv[0]);

		for (int i = 0; i < m_ChannelCount; i++) {
			if (m_Channels[i]) {
				CHashtable<CNick*, false>* Nicks = m_Channels[i]->GetNames();

				CNick* NickObj;

				NickObj = Nicks->Get(argv[2]);

				if (NickObj) {
					Nicks->Add(argv[2], NickObj);

					if (strcmpi(Nick, argv[2]) != 0)
						Nicks->Remove(Nick);
				}
			}
		}

		free(Nick);

		UpdateHostHelper(Reply);
	} else if (argc > 1 && strcmpi(Raw, "quit") == 0) {
		Nick = NickFromHostmask(argv[0]);

		for (int i = 0; i < m_ChannelCount; i++) {
			if (m_Channels[i])
				m_Channels[i]->GetNames()->Remove(Nick);
		}

		free(Nick);
	} else if (argc > 1 && (atoi(Raw) == 422 || atoi(Raw) == 376)) {
		const char* Chans = GetOwningClient()->GetConfig()->ReadString("user.channels");

		if (Chans)
			WriteLine("JOIN %s", Chans);

		GetOwningClient()->Notice("Connected to an IRC server.");

		g_Bouncer->Log("Connected to an IRC server. (%s)", m_Owner->GetUsername());
	} else if (argc > 1 && strcmpi(Reply, "error") == 0) {
		if (strstr(Raw, "throttle") != NULL)
			GetOwningClient()->ScheduleReconnect(50);
		else
			GetOwningClient()->ScheduleReconnect(5);
	} else if (argc > 3 && atoi(Raw) == 351) {
		free(m_ServerVersion);
		m_ServerVersion = strdup(argv[3]);

		free(m_ServerFeat);
		m_ServerFeat = strdup(argv[5]);
	} else if (argc > 3 && atoi(Raw) == 5) {
		for (int i = 3; i < argc - 1; i++) {
			char* Dup = strdup(argv[i]);
			char* Eq = strstr(Dup, "=");

			if (Eq) {
				*Eq = '\0';

				m_ISupport->WriteString(Dup, ++Eq);
			} else
				m_ISupport->WriteString(Dup, "");

			free(Dup);
		}
	} else if (argc > 5 && atoi(Raw) == 324) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan)
			Chan->ParseModeChange(argv[4], argc - 5, &argv[5]);
	} else if (argc > 4 && strcmpi(Raw, "mode") == 0) {
		CChannel* Chan = GetChannel(argv[2]);

		if (Chan)
			Chan->ParseModeChange(argv[3], argc - 4, &argv[4]);

		UpdateHostHelper(Reply);
	} else if (argc > 4 && atoi(Raw) == 329) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan)
			Chan->SetCreationTime(atoi(argv[4]));
	} else if (argc > 4 && atoi(Raw) == 332) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan)
			Chan->SetTopic(argv[4]);
	} else if (argc > 5 && atoi(Raw) == 333) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan) {
			Chan->SetTopicNick(argv[4]);
			Chan->SetTopicStamp(atoi(argv[5]));
		}
	} else if (argc > 3 && atoi(Raw) == 331) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan) {
			Chan->SetNoTopic();
		}
	} else if (argc > 3 && strcmpi(Raw, "topic") == 0) {
		CChannel* Chan = GetChannel(argv[2]);

		if (Chan) {
			Chan->SetTopic(argv[3]);
			Chan->SetTopicStamp(time(NULL));
			Chan->SetTopicNick(argv[0]);
		}

		UpdateHostHelper(Reply);
	} else if (argc > 5 && atoi(Raw) == 353) {
		CChannel* Chan = GetChannel(argv[4]);

		const char* nicks = ArgTokenize(argv[5]);
		const char** nickv = ArgToArray(nicks);
		int nickc = ArgCount(nicks);

		if (Chan) {
			for (int i = 0; i < nickc; i++) {
				const char* n = nickv[i];

				while (IsNickPrefix(*n))
					++n;

				char* Modes = NULL;

				if (nickv[i] != n) {
					Modes = (char*)malloc(n - nickv[i] + 1);

					strncpy(Modes, nickv[i], n - nickv[i]);
					Modes[n - nickv[i]] = '\0';
				}

				Chan->AddUser(n, Modes);
			}
		}
	} else if (argc > 3 && atoi(Raw) == 366) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan)
			Chan->SetHasNames();
	} else if (argc > 7 && atoi(Raw) == 352) {
		const char* Ident = argv[4];
		const char* Host = argv[5];
		const char* Nick = argv[7];

		char* Mask = (char*)malloc(strlen(Nick) + 1 + strlen(Ident) + 1 + strlen(Host) + 1);

		snprintf(Mask, strlen(Nick) + 1 + strlen(Ident) + 1 + strlen(Host) + 1, "%s!%s@%s", Nick, Ident, Host);

		UpdateHostHelper(Mask);

		free(Mask);
	}

	CModule** Modules = g_Bouncer->GetModules();
	int Count = g_Bouncer->GetModuleCount();

	for (int i = 0; i < Count; i++) {
		if (Modules[i]) {
			if (!Modules[i]->InterceptIRCMessage(this, argc, argv))
				return false;
		}
	}

	return true;
}

void CIRCConnection::ParseLine(const char* Line) {
	const char* Args = ArgParseServerLine(Line);

	const char** argv = ArgToArray(Args);
	int argc = ArgCount(Args);

	if (ParseLineArgV(argc, argv)) {
		if (strcmpi(argv[0], "ping") == 0 && argc > 1) {
			WriteLine("PONG :%s", argv[1]);
		} else {
			CBouncerUser* User = GetOwningClient();

			if (User) {
				CClientConnection* Client = User->GetClientConnection();

				if (Client)
					Client->InternalWriteLine(Line);
			}
		}
	}

#ifdef _WIN32
	OutputDebugString(Line);
	OutputDebugString("\n");
#endif

	//puts(Line);

	ArgFreeArray(argv);
	ArgFree(Args);
}

CChannel** CIRCConnection::GetChannels(void) {
	return m_Channels;
}

int CIRCConnection::GetChannelCount(void) {
	return m_ChannelCount;
}

const char* CIRCConnection::GetCurrentNick(void) {
	return m_CurrentNick;
}

void CIRCConnection::AddChannel(const char* Channel) {
	for (int i = 0; i < m_ChannelCount; i++) {
		if (m_Channels[i] == NULL) {
			m_Channels[i] = new CChannel(Channel, this);

			UpdateChannelConfig();

			return;
		}
	}

	m_Channels = (CChannel**)realloc(m_Channels, ++m_ChannelCount * sizeof(CChannel*));
	m_Channels[m_ChannelCount - 1] = new CChannel(Channel, this);

	UpdateChannelConfig();
}

void CIRCConnection::RemoveChannel(const char* Channel) {
	for (int i = 0; i < m_ChannelCount; i++) {
		if (m_Channels[i] && strcmpi(m_Channels[i]->GetName(), Channel) == 0) {
			delete m_Channels[i];
			m_Channels[i] = NULL;
		}
	}

	UpdateChannelConfig();
}

const char* CIRCConnection::GetServer(void) {
	return m_Server;
}

void CIRCConnection::Destroy(void) {
	m_Owner->SetIRCConnection(NULL);

	delete this;
}

void CIRCConnection::UpdateChannelConfig(void) {
	char Out[1024] = "";

	for (int i = 0; i < m_ChannelCount; i++) {
		if (m_Channels[i]) {
			if (*Out)
				strcat(Out, ",");

			strcat(Out, m_Channels[i]->GetName());
		}
	}

	m_Owner->GetConfig()->WriteString("user.channels", Out);
}

bool CIRCConnection::IsOnChannel(const char* Channel) {
	for (int i = 0; i < m_ChannelCount; i++) {
		if (m_Channels[i] && strcmpi(m_Channels[i]->GetName(), Channel) == 0)
			return true;
	}

	return false;
}

char* CIRCConnection::NickFromHostmask(const char* Hostmask) {
	return ::NickFromHostmask(Hostmask);
}

void CIRCConnection::FreeNick(char* Nick) {
	free(Nick);
}

bool CIRCConnection::HasQueuedData(void) {
	bool queued = CConnection::HasQueuedData();

	if (m_Owner->GetClientConnection())
		return queued;

	if (queued && m_LastBurst < time(NULL) - 1) {
		m_LastBurst = time(NULL);
		return true;
	} else
		return false;
}

void CIRCConnection::Write(void) {
	char* p = NULL;

	if (m_Owner->GetClientConnection()) {
		CConnection::Write();
	}

	for (int i = 0; i < sendq_size; i++) {
		if (sendq[i] == '\n') {
			p = sendq + i + 1;
			break;
		}
	}

	if (p) {
		send(m_Socket, sendq, p - sendq, 0);

		char* copy = (char*)malloc(sendq_size - (p - sendq));
		memcpy(copy, p, sendq_size - (p - sendq));

		free(sendq);
		sendq_size -= (p - sendq);
		sendq = copy;
	} else
		CConnection::Write();
}

const char* CIRCConnection::GetISupport(const char* Feature) {
	const char* Val = m_ISupport->ReadString(Feature);

	return Val;
}

bool CIRCConnection::IsChanMode(char Mode) {
	const char* Modes = GetISupport("CHANMODES");

	return strchr(Modes, Mode) != NULL;
}

bool CIRCConnection::RequiresParameter(char	Mode) {
	const char* Modes = GetISupport("CHANMODES");
	char* p = strchr(Modes, Mode);

	if (p) {
		if (*(p+1) == ',')
			return true;
		else
			return false;
	} else
		return false;

	return false;
}

CChannel* CIRCConnection::GetChannel(const char* Name) {
	if (!Name)
		return NULL;

	for (int i = 0; i < m_ChannelCount; i++) {
		if (m_Channels[i] && strcmpi(m_Channels[i]->GetName(), Name) == 0)
			return m_Channels[i];
	}

	return NULL;
}

bool CIRCConnection::IsNickPrefix(char Char) {
	const char* Prefixes = GetISupport("PREFIX");
	bool flip = false;

	for (unsigned int i = 0; i < strlen(Prefixes); i++) {
		if (flip) {
			if (Prefixes[i] == Char)
				return true;
		} else if (Prefixes[i] == ')')
			flip = true;
	}

	return false;
}

bool CIRCConnection::IsNickMode(char Char) {
	const char* Prefixes = GetISupport("PREFIX");

	while (*Prefixes != '\0' && *Prefixes != ')')
		if (*Prefixes == Char && *Prefixes != '(')
			return true;
		else
			Prefixes++;

	return false;
}

char CIRCConnection::PrefixForChanMode(char Mode) {
	const char* Prefixes = GetISupport("PREFIX");
	char* pref = strstr(Prefixes, ")");

	Prefixes++;

	if (pref)
		pref++;
	else
		return '\0';

	while (*pref) {
		if (*Prefixes == Mode)
			return *pref;

		Prefixes++;
		pref++;
	}

	return '\0';
}

const char* CIRCConnection::GetServerVersion(void) {
	return m_ServerVersion;
}

const char* CIRCConnection::GetServerFeat(void) {
	return m_ServerFeat;
}

CBouncerConfig* CIRCConnection::GetISupportAll(void) {
	return m_ISupport;
}

void CIRCConnection::UpdateHostHelper(const char* Host) {
	char* Copy = strdup(Host);
	const char* Nick = Copy;
	char* Site = strstr(Copy, "!");

	if (Site) {
		*Site = '\0';
		Site++;
	} else {
		free(Copy);
		return;
	}

	for (int i = 0; i < m_ChannelCount; i++) {
		CChannel* Channel = m_Channels[i];

		if (Channel) {
			CNick* NickObj = Channel->GetNames()->Get(Nick);

			if (NickObj && NickObj->GetSite() == NULL)
				NickObj->SetSite(Site);
		}
	}

	free(Copy);
}
