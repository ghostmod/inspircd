/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_DEPRECATE

#include "inspircd.h"

typedef std::map<irc::string,irc::string> censor_t;

/* $ModDesc: Provides user and channel +G mode */

/** Handles usermode +G
 */
class CensorUser : public SimpleUserModeHandler
{
 public:
	CensorUser(InspIRCd* Instance) : SimpleUserModeHandler(Instance, 'G') { }
};

/** Handles channel mode +G
 */
class CensorChannel : public SimpleChannelModeHandler
{
 public:
	CensorChannel(InspIRCd* Instance) : SimpleChannelModeHandler(Instance, 'G') { }
};

class ModuleCensor : public Module
{
	censor_t censors;
	CensorUser *cu;
	CensorChannel *cc;

 public:
	ModuleCensor(InspIRCd* Me)
		: Module(Me)
	{
		/* Read the configuration file on startup.
		 */
		OnRehash(NULL,"");
		cu = new CensorUser(ServerInstance);
		cc = new CensorChannel(ServerInstance);
		if (!ServerInstance->Modes->AddMode(cu) || !ServerInstance->Modes->AddMode(cc))
		{
			delete cu;
			delete cc;
			throw ModuleException("Could not add new modes!");
		}
		Implementation eventlist[] = { I_OnRehash, I_OnUserPreMessage, I_OnUserPreNotice };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}


	virtual ~ModuleCensor()
	{
		ServerInstance->Modes->DelMode(cu);
		ServerInstance->Modes->DelMode(cc);
		delete cu;
		delete cc;
	}

	virtual void ReplaceLine(irc::string &text, irc::string pattern, irc::string replace)
	{
		if ((!pattern.empty()) && (!text.empty()))
		{
			std::string::size_type pos;
			while ((pos = text.find(pattern)) != irc::string::npos)
			{
				text.erase(pos,pattern.length());
				text.insert(pos,replace);
			}
		}
	}

	// format of a config entry is <badword text="shit" replace="poo">
	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return 0;

		bool active = false;

		if (target_type == TYPE_USER)
			active = ((User*)dest)->IsModeSet('G');
		else if (target_type == TYPE_CHANNEL)
		{
			active = ((Channel*)dest)->IsModeSet('G');
			Channel* c = (Channel*)dest;
			if (CHANOPS_EXEMPT(ServerInstance, 'G') && c->GetStatus(user) == STATUS_OP)
			{
				return 0;
			}
		}

		if (!active)
			return 0;

		irc::string text2 = text.c_str();
		for (censor_t::iterator index = censors.begin(); index != censors.end(); index++)
		{
			if (text2.find(index->first) != irc::string::npos)
			{
				if (index->second.empty())
				{
					user->WriteNumeric(ERR_WORDFILTERED, "%s %s %s :Your message contained a censored word, and was blocked", user->nick.c_str(), ((Channel*)dest)->name.c_str(), index->first.c_str());
					return 1;
				}

				this->ReplaceLine(text2,index->first,index->second);
			}
		}
		text = text2.c_str();
		return 0;
	}

	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	virtual void OnRehash(User* user, const std::string &parameter)
	{
		/*
		 * reload our config file on rehash - we must destroy and re-allocate the classes
		 * to call the constructor again and re-read our data.
		 */
		ConfigReader* MyConf = new ConfigReader(ServerInstance);
		censors.clear();

		for (int index = 0; index < MyConf->Enumerate("badword"); index++)
		{
			irc::string pattern = (MyConf->ReadValue("badword","text",index)).c_str();
			irc::string replace = (MyConf->ReadValue("badword","replace",index)).c_str();
			censors[pattern] = replace;
		}

		delete MyConf;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_COMMON|VF_VENDOR,API_VERSION);
	}

};

MODULE_INIT(ModuleCensor)
