/**
 * Copyright (C) 2014 Justin James.
 *
 * This license must be preserved.
 * Any applications, libraries, or code which make any use of any
 * component of this program must not be commercial, unless explicit
 * permission is granted from the original author. The use of this
 * program for non-profit purposes is permitted.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In the event that this license restricts you from making desired use of this program, contact the original author.
 * Written by Justin James <justin.aj@hotmail.com>
 */

#if !defined _RENX_MEDALS_H_HEADER
#define _RENX_MEDALS_H_HEADER

#include "Jupiter/Plugin.h"
#include "Jupiter/INIFile.h"
#include "Jupiter/String.h"
#include "RenX_Plugin.h"
#include "RenX_GameCommand.h"

/** Adds a recommendation to the player's medal data */
void addRec(RenX::PlayerInfo *player, int amount = 1);

/** Adds a noob to the player's medal data */
void addNoob(RenX::PlayerInfo *player, int amount = 1);

/** Fetches a player's recommendation count */
unsigned long getRecs(const RenX::PlayerInfo *player);

/** Fetches a player's noob count */
unsigned long getNoobs(const RenX::PlayerInfo *player);

/** Calculates a player's worth (recs - noobs) */
int getWorth(const RenX::PlayerInfo *player);

class RenX_MedalsPlugin : public RenX::Plugin
{
public: // RenX::Plugin
	void RenX_OnJoin(RenX::Server *server, const RenX::PlayerInfo *player) override;
	void RenX_OnPart(RenX::Server *server, const RenX::PlayerInfo *player) override;
	void RenX_OnGameOver(RenX::Server *server, RenX::WinType winType, RenX::TeamType team, int gScore, int nScore) override;
	void RenX_OnDestroy(RenX::Server *server, const RenX::PlayerInfo *player, const Jupiter::ReadableString &objectName, const Jupiter::ReadableString &damageType, RenX::ObjectType type) override;
	RenX_MedalsPlugin();
	~RenX_MedalsPlugin();

public: // Jupiter::Plugin
	int OnRehash() override;
	const Jupiter::ReadableString &getName() override { return name; }

public:
	bool firstGame = true;
	time_t killCongratDelay;
	time_t vehicleKillCongratDelay;
	time_t kdrCongratDelay;
	Jupiter::StringS nameTag;
	Jupiter::StringS recsTag;
	Jupiter::StringS noobTag;
	Jupiter::StringS worthTag;
	Jupiter::StringS firstSection;
	Jupiter::StringS medalsFileName;
	Jupiter::StringS joinMessageFileName;
	Jupiter::INIFile medalsFile;
	Jupiter::INIFile joinMessageFile;

private:
	STRING_LITERAL_AS_NAMED_REFERENCE(name, "RenX.Medals");
	void init();
};

GENERIC_GAME_COMMAND(RecsGameCommand)
GENERIC_GAME_COMMAND(RecGameCommand)
GENERIC_GAME_COMMAND(NoobGameCommand)

#endif // _RENX_MEDALS_H_HEADER