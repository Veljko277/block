/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "gamecontext.h"
#include <engine/shared/config.h>
#include <engine/server/server.h>
#include <game/server/teams.h>
#include <game/server/accounting/account.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/version.h>
#include <game/generated/nethash.cpp>
#include <game/client/components/console.h>

#if defined(CONF_SQL)
#include <game/server/score/sql_score.h>
#endif

bool CheckClientID(int ClientID);

void CGameContext::ConGoLeft(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, -1, 0);
}

void CGameContext::ConGoRight(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, 1, 0);
}

void CGameContext::ConGoDown(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, 0, 1);
}

void CGameContext::ConGoUp(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, 0, -1);
}

void CGameContext::ConMove(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, pResult->GetInteger(0),
		pResult->GetInteger(1));
}

void CGameContext::ConMoveRaw(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, pResult->GetInteger(0),
		pResult->GetInteger(1), true);
}

void CGameContext::MoveCharacter(int ClientID, int X, int Y, bool Raw)
{
	CCharacter* pChr = GetPlayerChar(ClientID);

	if (!pChr)
		return;

	pChr->Core()->m_Pos.x += ((Raw) ? 1 : 32) * X;
	pChr->Core()->m_Pos.y += ((Raw) ? 1 : 32) * Y;
	pChr->m_DDRaceState = DDRACE_CHEAT;
}

void CGameContext::ConKillPlayer(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (pSelf->m_apPlayers[Victim])
	{
		pSelf->m_apPlayers[Victim]->KillCharacter(WEAPON_GAME);
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "%s was killed by %s",
			pSelf->Server()->ClientName(Victim),
			pSelf->Server()->ClientName(pResult->m_ClientID));
		if (pSelf->m_apPlayers[pResult->m_ClientID]->m_AccData.m_Vip)
			pSelf->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}
}

void CGameContext::ConLevelReset(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (pSelf->m_apPlayers[Victim])
	{
		pSelf->m_apPlayers[Victim]->m_Level.m_Exp = 0;
		pSelf->m_apPlayers[Victim]->m_Level.m_Level = 0;
		//pSelf->m_apPlayers[Victim]->m_pAccount->SetStorage(pSelf->Storage());
		pSelf->m_apPlayers[Victim]->m_pAccount->Apply(); // save it
	}
}

void CGameContext::ConBlackhole(IConsole::IResult *pResult, void *pUserData) // give or remove blackhole
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (pSelf->m_apPlayers[Victim])
	{
		pSelf->m_apPlayers[Victim]->m_Blackhole ^= 1;
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), pSelf->m_apPlayers[Victim]->m_Blackhole ? "%s gave you blackhole gun!" : "%s removed your blackhole gun!", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(Victim, aBuf);
	}
}

void CGameContext::ConEndless(IConsole::IResult *pResult, void *pUserData) // give or remove endless
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (!pSelf->GetPlayerChar(Victim))
		return;
	
	pSelf->GetPlayerChar(Victim)->m_EndlessHook ^= 1;
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), pSelf->GetPlayerChar(Victim)->m_EndlessHook ? "%s gave you endless!" : "%s removed your endless!", pSelf->Server()->ClientName(pResult->m_ClientID));
	pSelf->SendChatTarget(Victim, aBuf);
}

void CGameContext::ConPullhammer(IConsole::IResult *pResult, void *pUserData) // give or remove pullhammer
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (pSelf->m_apPlayers[Victim])
	{
		pSelf->m_apPlayers[Victim]->GetCharacter()->m_Pullhammer ^= 1;
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), pSelf->m_apPlayers[Victim]->GetCharacter()->m_Pullhammer ? "%s gave you pullhammer!" : "%s removed your pullhammer!", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(Victim, aBuf);
	}
}

void CGameContext::ConSmarthammer(IConsole::IResult *pResult, void *pUserData) // give or remove smarthammer
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (pSelf->m_apPlayers[Victim])
	{
		pSelf->m_apPlayers[Victim]->m_Bots.m_SmartHammer ^= 1;
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), pSelf->m_apPlayers[Victim]->m_Bots.m_SmartHammer ? "%s gave you smarthammer!" : "%s removed your smarthammer!", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(Victim, aBuf);
	}
}

void CGameContext::ConEpicCircles(IConsole::IResult *pResult, void *pUserData) // give or remove epic circles
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	CPlayer* pPlayer = pSelf->m_apPlayers[Victim];

	if (pPlayer)
	{
		pPlayer->m_EpicCircle ^= 1;
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), pPlayer->m_EpicCircle ? "%s gave you epic circles!" : "%s removed your epic circles!", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(Victim, aBuf);

		if(pPlayer->m_EpicCircle && pSelf->GetPlayerChar(Victim))
			pPlayer->m_pEpicCircle = new CEpicCircle(&pSelf->m_World, pSelf->GetPlayerChar(Victim)->m_Pos, Victim);
		else if (!pPlayer->m_EpicCircle && pSelf->GetPlayerChar(Victim))
			pPlayer->m_pEpicCircle->Reset();
	}
}

void CGameContext::ConXXL(IConsole::IResult *pResult, void *pUserData) // give or remove xxl
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (!pSelf->GetPlayerChar(Victim))
		return;

	pSelf->GetPlayerChar(Victim)->m_XXL ^= 1;
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), pSelf->GetPlayerChar(Victim)->m_XXL ? "%s gave you xxl!" : "%s removed your xxl!", pSelf->Server()->ClientName(pResult->m_ClientID));
	pSelf->SendChatTarget(Victim, aBuf);
}

void CGameContext::ConBloody(IConsole::IResult *pResult, void *pUserData) // give or remove bloody
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (!pSelf->GetPlayerChar(Victim))
		return;

	pSelf->GetPlayerChar(Victim)->m_Bloody ^= 1;
	
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), pSelf->GetPlayerChar(Victim)->m_Bloody ? "%s gave you bloody!" : "%s removed your bloody!", pSelf->Server()->ClientName(pResult->m_ClientID));
	pSelf->SendChatTarget(Victim, aBuf);
}

void CGameContext::ConSteamy(IConsole::IResult *pResult, void *pUserData) // give or remove steamy
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (!pSelf->GetPlayerChar(Victim))
		return;

	pSelf->GetPlayerChar(Victim)->m_Steamy ^= 1;
	
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), pSelf->GetPlayerChar(Victim)->m_Steamy ? "%s gave you steamy!" : "%s removed your steamy!", pSelf->Server()->ClientName(pResult->m_ClientID));
	pSelf->SendChatTarget(Victim, aBuf);
}

void CGameContext::ConRainbow(IConsole::IResult *pResult, void *pUserData) // give or remove Rainbow
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (pSelf->m_apPlayers[Victim])
	{
		pSelf->m_apPlayers[Victim]->m_Rainbow ^= 1;
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), pSelf->m_apPlayers[Victim]->m_Rainbow ? "%s gave you rainbow!" : "%s removed your rainbow!", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(Victim, aBuf);
	}
}

void CGameContext::ConEpileticRainbow(IConsole::IResult *pResult, void *pUserData) // give or remove epiletic Rainbow
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (pSelf->m_apPlayers[Victim])
	{
		pSelf->m_apPlayers[Victim]->m_Rainbowepiletic ^= 1;
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), pSelf->m_apPlayers[Victim]->m_Rainbowepiletic ? "%s gave you epiletic rainbow!" : "%s removed your epiletic rainbow!", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(Victim, aBuf);
	}
}

void CGameContext::ConLovely(IConsole::IResult *pResult, void *pUserData) // give or remove Lovely
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (pSelf->m_apPlayers[Victim])
	{
		pSelf->m_apPlayers[Victim]->m_Lovely ^= 1;
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), pSelf->m_apPlayers[Victim]->m_Lovely ? "%s gave you lovely!" : "%s removed your lovely!", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(Victim, aBuf);
	}
}

void CGameContext::ConRotatingHearts(IConsole::IResult *pResult, void *pUserData) // give or remove rotating hearts
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	CPlayer* pPlayer = pSelf->m_apPlayers[Victim];

	if (pPlayer)
	{
		pPlayer->m_RotatingHearts ^= 1;
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), pPlayer->m_RotatingHearts ? "%s gave you rotating hearts!" : "%s removed your rotating hearts!", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(Victim, aBuf);

		if(pPlayer->m_RotatingHearts && pSelf->GetPlayerChar(Victim))
			pPlayer->m_pRotatingHearts = new CRotatingHearts(&pSelf->m_World, Victim);
		else if (!pPlayer->m_RotatingHearts && pSelf->GetPlayerChar(Victim))
			pPlayer->m_pRotatingHearts->Reset();
	}
}

void CGameContext::ConBall(IConsole::IResult *pResult, void *pUserData) // give or remove Ball
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	int Victim = pResult->GetVictim();
	CPlayer* pPlayer = pSelf->m_apPlayers[Victim];

	if (pPlayer)
	{
		pPlayer->m_IsBallSpawned ^= 1;
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), pPlayer->m_IsBallSpawned ? "%s gave you ball!" : "%s removed your ball!", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(Victim, aBuf);

		if (pPlayer->m_IsBallSpawned && pSelf->GetPlayerChar(Victim))
			pPlayer->m_pBall = new CBall(&pSelf->m_World, pSelf->GetPlayerChar(Victim)->m_Pos, Victim);
		else if (!pPlayer->m_IsBallSpawned && pSelf->GetPlayerChar(Victim))
			pPlayer->m_pBall->Reset();
	}
}

void CGameContext::ConHeartGuns(IConsole::IResult *pResult, void *pUserData) // give or remove heartguns
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (pSelf->m_apPlayers[Victim])
	{
		pSelf->m_apPlayers[Victim]->m_HeartGuns ^= 1;
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), pSelf->m_apPlayers[Victim]->m_HeartGuns ? "%s gave you heartguns!" : "%s removed your heartguns!", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(Victim, aBuf);
	}
}

void CGameContext::ConRainbowHook(IConsole::IResult *pResult, void *pUserData) // give or remove rainbow hook
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (pSelf->m_apPlayers[Victim])
	{
		pSelf->m_apPlayers[Victim]->m_RainbowHook ^= 1;
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), pSelf->m_apPlayers[Victim]->m_RainbowHook ? "%s gave you rainbow hook!" : "%s removed your rainbow hook!", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(Victim, aBuf);
	}
}

void CGameContext::ConStars(IConsole::IResult *pResult, void *pUserData) // give or remove stars
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (pSelf->m_apPlayers[Victim])
	{
		pSelf->m_apPlayers[Victim]->m_Stars ^= 1;
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), pSelf->m_apPlayers[Victim]->m_Stars ? "%s gave you stars!" : "%s removed your stars!", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(Victim, aBuf);
	}
}

void CGameContext::ConInvisible(IConsole::IResult *pResult, void *pUserData) // give or remove invisible
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	CPlayer* pPlayer = pSelf->m_apPlayers[Victim];

	if (pPlayer)
	{
		if(!pSelf->GetPlayerChar(Victim) || !pSelf->GetPlayerChar(Victim)->IsAlive())
			return;

		if(pPlayer->m_Invisible)
			pSelf->CreatePlayerSpawn(pPlayer->GetCharacter()->m_Pos, pPlayer->GetCharacter()->Teams()->TeamMask(pPlayer->GetCharacter()->Team(), -1, Victim));
		else
			pSelf->CreateDeath(pPlayer->GetCharacter()->m_Pos, Victim, pPlayer->GetCharacter()->Teams()->TeamMask(pPlayer->GetCharacter()->Team(), -1, Victim));

		pSelf->m_apPlayers[Victim]->m_Invisible ^= 1;
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), pSelf->m_apPlayers[Victim]->m_Invisible ? "%s gave you invisible!" : "%s removed your invisible!", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(Victim, aBuf);

		char FakeMsg[256];
		str_format(FakeMsg, sizeof(FakeMsg), pSelf->m_apPlayers[Victim]->m_Invisible ? "'%s' has left the game" : "'%s' entered and joined the game", pSelf->Server()->ClientName(Victim));
		pSelf->SendChat(-1, CGameContext::CHAT_ALL, FakeMsg);
	
		if(!pSelf->m_apPlayers[Victim]->m_Invisible)
		{
			pSelf->GetPlayerChar(Victim)->HandleCollision(true);
		}
	}
}

void CGameContext::ConVip(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int VipID = pResult->GetVictim();

	CPlayer *pPlayer = pSelf->m_apPlayers[VipID];

	char aBuf[64];
	if (pPlayer)
	{
		if (pPlayer->m_AccData.m_UserID)
		{
			pPlayer->m_AccData.m_Vip ^= 1;
			pPlayer->m_pAccount->Apply();

			if (pPlayer->m_AccData.m_Vip)
				str_format(aBuf, sizeof aBuf, "'%s' is Vip now.", pSelf->Server()->ClientName(VipID));
			else
				str_format(aBuf, sizeof aBuf, "'%s' is no longer Vip.", pSelf->Server()->ClientName(VipID));
			pSelf->SendChat(-1, CHAT_ALL, aBuf);
		}
		else
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "vip", "Player must be logged in to receive vip");
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "There is no player with ID %i", VipID);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "vip", aBuf);
	}
}

void CGameContext::ConCheckVip(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int VipID = pResult->GetVictim();
	char aBuf[200];

	CPlayer* pPlayer = pSelf->m_apPlayers[VipID];
	if (pPlayer)
	{
		if (!pPlayer->m_AccData.m_UserID)
			str_format(aBuf, sizeof aBuf, "'%s' is not even logged in.", pSelf->Server()->ClientName(VipID));
		else if (pPlayer->m_AccData.m_Vip)
			str_format(aBuf, sizeof aBuf, "'%s' has Vip.", pSelf->Server()->ClientName(VipID));
		else
			str_format(aBuf, sizeof aBuf, "'%s' does not have Vip.", pSelf->Server()->ClientName(VipID));
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "vip", aBuf);
	}
}

void CGameContext::ConNinja(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_NINJA, false);
}

void CGameContext::ConSuper(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	
	CCharacter* pChr = pSelf->GetPlayerChar(pResult->GetVictim());
	if (pChr && !pChr->m_Super)
	{
		pChr->m_Super = true;
		pChr->UnFreeze();
		pChr->m_TeamBeforeSuper = pChr->Team();
		pChr->Teams()->SetCharacterTeam(pResult->GetVictim(), TEAM_SUPER);
		pChr->m_DDRaceState = DDRACE_CHEAT;
	}
}

void CGameContext::ConUnSuper(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	
	CCharacter* pChr = pSelf->GetPlayerChar(pResult->GetVictim());
	if (pChr && pChr->m_Super)
	{
		pChr->m_Super = false;
		pChr->Teams()->SetForceCharacterTeam(pResult->GetVictim(),
			pChr->m_TeamBeforeSuper);
	}
}

void CGameContext::ConUnSolo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	CCharacter* pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if (pChr)
		pChr->HandleSolo(false);
}

void CGameContext::ConUnDeep(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	CCharacter* pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if (pChr)
		pChr->m_DeepFreeze = false;
}

void CGameContext::ConShotgun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_SHOTGUN, false);
}

void CGameContext::ConGrenade(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_GRENADE, false);
}

void CGameContext::ConRifle(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_RIFLE, false);
}

void CGameContext::ConJetpack(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter* pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if (pChr)
		pChr->m_Jetpack = true;
}

void CGameContext::ConWeapons(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, -1, false);
}

void CGameContext::ConUnShotgun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_SHOTGUN, true);
}

void CGameContext::ConUnGrenade(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_GRENADE, true);
}

void CGameContext::ConUnRifle(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_RIFLE, true);
}

void CGameContext::ConUnJetpack(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter* pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if (pChr)
		pChr->m_Jetpack = false;
}

void CGameContext::ConUnWeapons(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, -1, true);
}

void CGameContext::ConAddWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, pResult->GetInteger(0), false);
}

void CGameContext::ConRemoveWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, pResult->GetInteger(0), true);
}

void CGameContext::ModifyWeapons(IConsole::IResult *pResult, void *pUserData,
	int Weapon, bool Remove)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter* pChr = GetPlayerChar(pResult->GetVictim());
	if (!pChr)
		return;

	if (clamp(Weapon, -1, NUM_WEAPONS - 1) != Weapon)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info",
			"invalid weapon id");
		return;
	}

	if (Weapon == -1)
	{
		pChr->GiveWeapon(WEAPON_SHOTGUN);
		pChr->GiveWeapon(WEAPON_GRENADE);
		pChr->GiveWeapon(WEAPON_RIFLE);
	}
	else
	{
		pChr->GiveWeapon(Weapon, Remove);
	}

	pChr->m_DDRaceState = DDRACE_CHEAT;
}

void CGameContext::ConToTeleporter(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	unsigned int TeleTo = pResult->GetInteger(0);

	if (((CGameControllerDDRace*)pSelf->m_pController)->m_TeleOuts[TeleTo - 1].size())
	{
		int Num = ((CGameControllerDDRace*)pSelf->m_pController)->m_TeleOuts[TeleTo - 1].size();
		vec2 TelePos = ((CGameControllerDDRace*)pSelf->m_pController)->m_TeleOuts[TeleTo - 1][(!Num) ? Num : rand() % Num];
		CCharacter* pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
		if (pChr)
		{
			pChr->Core()->m_Pos = TelePos;
			pChr->m_Pos = TelePos;
			pChr->m_PrevPos = TelePos;
			pChr->m_DDRaceState = DDRACE_CHEAT;
		}
	}
}

void CGameContext::ConToCheckTeleporter(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	unsigned int TeleTo = pResult->GetInteger(0);

	if (((CGameControllerDDRace*)pSelf->m_pController)->m_TeleCheckOuts[TeleTo - 1].size())
	{
		int Num = ((CGameControllerDDRace*)pSelf->m_pController)->m_TeleCheckOuts[TeleTo - 1].size();
		vec2 TelePos = ((CGameControllerDDRace*)pSelf->m_pController)->m_TeleCheckOuts[TeleTo - 1][(!Num) ? Num : rand() % Num];
		CCharacter* pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
		if (pChr)
		{
			pChr->Core()->m_Pos = TelePos;
			pChr->m_Pos = TelePos;
			pChr->m_PrevPos = TelePos;
			pChr->m_DDRaceState = DDRACE_CHEAT;
			pChr->m_TeleCheckpoint = TeleTo;
		}
	}
}

void CGameContext::ConTeleport(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int TeleTo = pResult->GetInteger(0);
	int Tele = pResult->m_ClientID;
	if (pResult->NumArguments() > 0)
		Tele = pResult->GetVictim();

	if (pSelf->m_apPlayers[TeleTo])
	{
		CCharacter* pChr = pSelf->GetPlayerChar(Tele);
		if (pChr && pSelf->GetPlayerChar(TeleTo))
		{
			pChr->Core()->m_Pos = pSelf->m_apPlayers[TeleTo]->m_ViewPos;
			pChr->m_Pos = pSelf->m_apPlayers[TeleTo]->m_ViewPos;
			pChr->m_PrevPos = pSelf->m_apPlayers[TeleTo]->m_ViewPos;
			pChr->m_DDRaceState = DDRACE_CHEAT;
		}
	}
}

void CGameContext::ConRocket(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];
	if (!pPlayer)
		return;

	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_GRENADE, false);

	pPlayer->m_IsRocket ^= true;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), pPlayer->m_IsRocket ? "You got rocket by %s." : "%s removed your rocket.", pSelf->Server()->ClientName(pResult->m_ClientID));
	pSelf->SendChatTarget(Victim, aBuf);
}

void CGameContext::ConSkin(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	const char *Skin = pResult->GetString(0);
	int Victim = pResult->GetVictim();

	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];
	if (!pPlayer)
		return;

	//change skin
	str_copy(pPlayer->m_TeeInfos.m_SkinName, Skin, sizeof(pPlayer->m_TeeInfos.m_SkinName));
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s's skin changed to %s", pSelf->Server()->ClientName(Victim), Skin);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", aBuf);
}

void CGameContext::ConSendSound(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientID = clamp(pResult->GetInteger(0), 0, 64), Sound = clamp(pResult->GetInteger(1), 0, 40);

	if(pSelf->m_apPlayers[ClientID] && pSelf->GetPlayerChar(ClientID))
	{
		pSelf->CreateSound(pSelf->GetPlayerChar(ClientID)->Core()->m_Pos, Sound, -1LL);
	}
}

void CGameContext::ConKill(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];

	if (!pPlayer
		|| (pPlayer->m_LastKill
			&& pPlayer->m_LastKill
			+ pSelf->Server()->TickSpeed()
			* g_Config.m_SvKillDelay
					> pSelf->Server()->Tick()))
		return;

	pPlayer->m_LastKill = pSelf->Server()->Tick();
	pPlayer->KillCharacter(WEAPON_SELF);
	//pPlayer->m_RespawnTick = pSelf->Server()->Tick() + pSelf->Server()->TickSpeed() * g_Config.m_SvSuicidePenalty;
}

void CGameContext::ConForcePause(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CServer* pServ = (CServer*)pSelf->Server();
	int Victim = pResult->GetVictim();
	int Seconds = 0;
	if (pResult->NumArguments() > 0)
		Seconds = clamp(pResult->GetInteger(0), 0, 360);

	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];
	if (!pPlayer)
		return;

	pPlayer->m_ForcePauseTime = Seconds*pServ->TickSpeed();
	pPlayer->m_Paused = CPlayer::PAUSED_FORCE;
}

void CGameContext::Mute(IConsole::IResult *pResult, NETADDR *Addr, int Secs,
	const char *pDisplayName, bool silent)
{
	char aBuf[128];
	int Found = 0;
	// find a matching mute for this ip, update expiration time if found
	for (int i = 0; i < m_NumMutes; i++)
	{
		if (net_addr_comp(&m_aMutes[i].m_Addr, Addr) == 0)
		{
			m_aMutes[i].m_Expire = Server()->Tick()
				+ Secs * Server()->TickSpeed();
			Found = 1;
		}
	}

	if (!Found) // nothing found so far, find a free slot..
	{
		if (m_NumMutes < MAX_MUTES)
		{
			m_aMutes[m_NumMutes].m_Addr = *Addr;
			m_aMutes[m_NumMutes].m_Expire = Server()->Tick()
				+ Secs * Server()->TickSpeed();
			m_NumMutes++;
			Found = 1;
		}
	}
	if (Found)
	{
		if(!silent)
		{
			str_format(aBuf, sizeof aBuf, "'%s' has been muted for %d seconds.",
				pDisplayName, Secs);
			SendChat(-1, CHAT_ALL, aBuf);
		}
	}
	else // no free slot found
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", "mute array is full");
}

void CGameContext::ConMute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->Console()->Print(
		IConsole::OUTPUT_LEVEL_STANDARD,
		"mutes",
		"Use either 'muteid <client_id> <seconds>' or 'muteip <ip> <seconds>'");
}

// mute through client id
void CGameContext::ConMuteID(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->GetVictim();

	NETADDR Addr;
	pSelf->Server()->GetClientAddr(Victim, &Addr);

	pSelf->Mute(pResult, &Addr, clamp(pResult->GetInteger(0), 1, 86400),
		pSelf->Server()->ClientName(Victim), false);
}

// silent version of MuteID
void CGameContext::ConSilentMuteID(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->GetVictim();

	NETADDR Addr;
	pSelf->Server()->GetClientAddr(Victim, &Addr);

	pSelf->Mute(pResult, &Addr, clamp(pResult->GetInteger(0), 1, 86400),
		pSelf->Server()->ClientName(Victim), true);

	if(pSelf->m_apPlayers[Victim])
		pSelf->m_apPlayers[Victim]->m_SilentMuted = true;
}

// mute through ip, arguments reversed to workaround parsing
void CGameContext::ConMuteIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	NETADDR Addr;
	if (net_addr_from_str(&Addr, pResult->GetString(0)))
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes",
			"Invalid network address to mute");
	}
	pSelf->Mute(pResult, &Addr, clamp(pResult->GetInteger(1), 1, 86400),
		pResult->GetString(0), false);
}

// unmute by mute list index
void CGameContext::ConUnmute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aIpBuf[64];
	char aBuf[64];
	int Victim = pResult->GetVictim();

	if (Victim < 0 || Victim >= pSelf->m_NumMutes)
		return;

	pSelf->m_NumMutes--;
	pSelf->m_aMutes[Victim] = pSelf->m_aMutes[pSelf->m_NumMutes];

	net_addr_str(&pSelf->m_aMutes[Victim].m_Addr, aIpBuf, sizeof(aIpBuf), false);
	str_format(aBuf, sizeof(aBuf), "Unmuted %s", aIpBuf);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", aBuf);
}

// list mutes
void CGameContext::ConMutes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aIpBuf[64];
	char aBuf[128];
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes",
		"Active mutes:");
	for (int i = 0; i < pSelf->m_NumMutes; i++)
	{
		net_addr_str(&pSelf->m_aMutes[i].m_Addr, aIpBuf, sizeof(aIpBuf), false);
		str_format(
			aBuf,
			sizeof aBuf,
			"%d: \"%s\", %d seconds left",
			i,
			aIpBuf,
			(pSelf->m_aMutes[i].m_Expire - pSelf->Server()->Tick())
			/ pSelf->Server()->TickSpeed());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", aBuf);
	}
}

void CGameContext::ConList(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientID = pResult->m_ClientID;
	if (!CheckClientID(ClientID)) return;

	char zerochar = 0;
	if (pResult->NumArguments() > 0)
		pSelf->List(ClientID, pResult->GetString(0));
	else
		pSelf->List(ClientID, &zerochar);
}

void CGameContext::ConFreezeHammer(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->GetVictim();

	CCharacter* pChr = pSelf->GetPlayerChar(Victim);

	if (!pChr)
		return;

	char aBuf[128];
	str_format(aBuf, sizeof aBuf, "'%s' got freeze hammer!",
		pSelf->Server()->ClientName(Victim));
	pSelf->SendChat(-1, CHAT_ALL, aBuf);

	pChr->m_FreezeHammer = true;
}

void CGameContext::ConUnFreezeHammer(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->GetVictim();

	CCharacter* pChr = pSelf->GetPlayerChar(Victim);

	if (!pChr)
		return;

	char aBuf[128];
	str_format(aBuf, sizeof aBuf, "'%s' lost freeze hammer!",
		pSelf->Server()->ClientName(Victim));
	pSelf->SendChat(-1, CHAT_ALL, aBuf);

	pChr->m_FreezeHammer = false;
}

void CGameContext::ConRename(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	const char *newName = pResult->GetString(0);
	int Victim = pResult->GetVictim();

	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];
	if (!pPlayer)
		return;

	//change name
	char oldName[MAX_NAME_LENGTH];
	str_copy(oldName, pSelf->Server()->ClientName(Victim), MAX_NAME_LENGTH);

	pSelf->Server()->SetClientName(Victim, newName);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s has changed %s's name to '%s'", pSelf->Server()->ClientName(pResult->m_ClientID), oldName, pSelf->Server()->ClientName(Victim));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", aBuf);

	str_format(aBuf, sizeof(aBuf), "%s changed your name to %s.", pSelf->Server()->ClientName(pResult->m_ClientID), pSelf->Server()->ClientName(Victim));
	//pSelf->SendChatTarget(Victim, aBuf);
}

void CGameContext::ConHL(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	const char *HammerLevel = pResult->GetString(0);
	int Victim = pResult->GetVictim();

	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (!pChr)
		return;

	// Set hammer level
	pChr->m_HammerStrenght = str_toint(HammerLevel);
	char aBuf[246];
	str_format(aBuf, 246, "%s has set %s hammer level to %d", pSelf->Server()->ClientName(pResult->m_ClientID), pSelf->Server()->ClientName(Victim), pChr->m_HammerStrenght);
	pSelf->SendChatTarget(-1, aBuf);
}

void CGameContext::ConClan(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	const char *newClan = pResult->GetString(0);
	int Victim = pResult->GetVictim();

	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];
	if (!pPlayer)
		return;

	//change name
	char oldClan[MAX_CLAN_LENGTH];
	str_copy(oldClan, pSelf->Server()->ClientClan(Victim), MAX_CLAN_LENGTH);

	pSelf->Server()->SetClientClan(Victim, newClan);
	
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s has changed '%s' clan to '%s'", pSelf->Server()->ClientName(pResult->m_ClientID), oldClan, pSelf->Server()->ClientClan(Victim));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", aBuf);
}

void CGameContext::ConFreeze(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Seconds = -1;
	int Victim = pResult->GetVictim();

	char aBuf[128];

	if (pResult->NumArguments() == 1)
		Seconds = clamp(pResult->GetInteger(0), -2, 9999);

	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (!pChr)
		return;

	if (pSelf->m_apPlayers[Victim])
	{
		pChr->Freeze(Seconds);
		pChr->GetPlayer()->m_RconFreeze = Seconds != -2;
		CServer* pServ = (CServer*)pSelf->Server();
		if (Seconds >= 0)
			str_format(aBuf, sizeof(aBuf), "'%s' ClientID=%d has been Frozen for %d.", pServ->ClientName(Victim), Victim, Seconds);
		else if (Seconds == -2)
		{
			pChr->m_DeepFreeze = true;
			str_format(aBuf, sizeof(aBuf), "'%s' ClientID=%d has been Deep Frozen.", pServ->ClientName(Victim), Victim);
		}
		else
			str_format(aBuf, sizeof(aBuf), "'%s' ClientID=%d is Frozen until you unfreeze him.", pServ->ClientName(Victim), Victim);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", aBuf);
	}

}

void CGameContext::ConUnFreeze(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();
	static bool Warning = false;
	char aBuf[128];
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (!pChr)
		return;
	if (pChr->m_DeepFreeze && !Warning)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warning", "This client is deeply frozen, repeat the command to defrost him.");
		Warning = true;
		return;
	}
	if (pChr->m_DeepFreeze && Warning)
	{
		pChr->m_DeepFreeze = false;
		Warning = false;
	}
	pChr->m_FreezeTime = 2;
	pChr->GetPlayer()->m_RconFreeze = false;
	CServer* pServ = (CServer*)pSelf->Server();
	str_format(aBuf, sizeof(aBuf), "'%s' ClientID=%d has been defrosted.", pServ->ClientName(Victim), Victim);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", aBuf);
}

void CGameContext::ConFixAccounts(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->Server()->FixAccounts();

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Accounts fixed!");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", aBuf);
}

void CGameContext::ConPublicExecCommand(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->GetInteger(0);
	char aBuf[256];

	str_format(aBuf, sizeof(aBuf), pResult->GetString(1));
	if(!pSelf->m_apPlayers[Victim])
		return;

	pSelf->ChatCommands(aBuf, Victim);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
}

void CGameContext::ConHookJetpack(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];
	if (!pPlayer)
		return;

	pPlayer->m_HookJetpack ^= true;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), pPlayer->m_HookJetpack ? "You got hook jetpack by %s." : "%s removed your hook jetpack.", pSelf->Server()->ClientName(pResult->m_ClientID));
	pSelf->SendChatTarget(Victim, aBuf);
}

void CGameContext::ConLightSaber(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->GetVictim()];
	if (!pPlayer)
		return;

	pPlayer->m_LightSaber ^= true;
	pPlayer->m_LightningLaser = false;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), pPlayer->m_LightSaber ? "You got light saber by %s." : "%s removed your light saber.", pSelf->Server()->ClientName(pResult->m_ClientID));
	pSelf->SendChatTarget(pResult->GetVictim(), aBuf);
}

void CGameContext::ConLightningLaser(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->GetVictim()];
	if (!pPlayer)
		return;

	pPlayer->m_LightningLaser ^= true;
	pPlayer->m_LightSaber = false;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), pPlayer->m_LightningLaser ? "You got lightning laser by %s." : "%s removed your lightning laser.", pSelf->Server()->ClientName(pResult->m_ClientID));
	pSelf->SendChatTarget(pResult->GetVictim(), aBuf);
}

void CGameContext::ConEventExp(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	pSelf->m_EventExp       = pResult->GetInteger(0);
	pSelf->m_EventSecs      = pSelf->Server()->TickSpeed() * pResult->GetInteger(1);
	pSelf->m_Event          = true;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "An event x%d has started for %d seconds!", pResult->GetInteger(0), pResult->GetInteger(1));

	pSelf->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
}
