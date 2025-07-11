/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/server/entities/special/vacuum.h>
#include <game/server/entities/special/rocket.h>
#include <game/server/entities/special/passiveindicator.h>
#include <game/mapitems.h>
#include <game/server/accounting/account.h>

#include "character.h"
#include "laser.h"
#include "projectile.h"

#include <stdio.h>
#include <cstdlib>
#include <string.h>
#include <engine/server/server.h>
#include <engine/storage.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/server/score.h>
#include "light.h"

#include "special/lightninglaser.h"
#include "special/lightsaber.h"

#include <string.h>
#include <fstream>
#include <engine/config.h>
#if defined(CONF_FAMILY_WINDOWS)
#include <tchar.h>
#include <direct.h>
#endif
#if defined(CONF_FAMILY_UNIX)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <game/server/player.h>

#endif

static int s_ShowAimID = -1;

MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CLIENTS)

inline int ms_rand(int *seed)
{
	*seed = *seed * 0x343fd + 0x269EC3;  // a=214013, b=2531011
	return (*seed >> 0x10) & 0x7FFF;
}

// Character, "physical" player's part
#define FeatureCapture(X) m_ ## X
CCharacter::CCharacter(CGameWorld *pWorld)
	: CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER)
{
	FeatureCapture(ProximityRadius) = ms_PhysSize;
	FeatureCapture(Health) = 0;
	FeatureCapture(Armor) = 0;
}

void CCharacter::Reset()
{
	Destroy();
}

bool CCharacter::Spawn(CPlayer *pPlayer, vec2 Pos)
{
	m_TempHasSword = false;
	m_TempPassTime = 0;

	m_EmoteStop = -1;
	m_LastAction = -1;
	m_LastNoAmmoSound = -1;
	m_LastWeapon = WEAPON_HAMMER;
	m_QueuedWeapon = -1;
	m_LastRefillJumps = false;
	m_LastPenalty = false;
	m_LastBonus = false;
	m_AnimIDNum = 9; //maximum number of "animation balls" m_KOH
	m_apAnimIDs = new int[m_AnimIDNum];//create id-array
	m_pPlayer = pPlayer;
	m_Pos = Pos;
	m_IsFiring = false;

	//ban
	m_ProcessBanChecked = false;
	m_TimerBeforeProcess = Server()->TickSpeed()+5;

	m_LovelyLifeSpan = Server()->TickSpeed(); // hearty
	m_BloodyDelay = m_SteamyDelay = 1;
	RainbowHookedID = -1;
	m_LightSaberActivated = false;

	m_Core.Reset();
	m_Core.Init(&GameServer()->m_World.m_Core, GameServer()->Collision(), &((CGameControllerDDRace*)GameServer()->m_pController)->m_Teams.m_Core, &((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts);
	m_Core.m_ActiveWeapon = WEAPON_GUN;
	m_Core.m_Pos = m_Pos;
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;

	m_ReckoningTick = 0;
	mem_zero(&m_SendCore, sizeof(m_SendCore));
	mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));

	GameServer()->m_World.InsertEntity(this);
	m_Alive = true;

	m_pPlayer->m_IsEmote = false; /* Reset */
	m_pPlayer->m_IsRocket = false; /* Reset */

	GameServer()->m_pController->OnCharacterSpawn(this);

	Teams()->OnCharacterSpawn(GetPlayer()->GetCID());
	//GetPlayer()->m_Killedby = -1;
	DDRaceInit();

	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(Pos));
	m_TuneZoneOld = -1; // no zone leave msg on spawn
	m_NeededFaketuning = 0; // reset fake tunings on respawn and send the client
	SendZoneMsgs(); // we want a entermessage also on spawn
	GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);

	Server()->StartRecord(m_pPlayer->GetCID());

	if (GetPlayer()->m_QuestData.m_QuestPart == CPlayer::QUEST_PART_RACE)
	{
		GetPlayer()->m_QuestData.m_RaceStartTick = 0;
	}

	if (pPlayer->m_IsBallSpawned)
		pPlayer->m_pBall = new CBall(&GameServer()->m_World, m_Pos, pPlayer->GetCID());
	if (pPlayer->m_EpicCircle)
		pPlayer->m_pEpicCircle = new CEpicCircle(&GameServer()->m_World, m_Pos, pPlayer->GetCID());
	if (pPlayer->m_RotatingHearts)
		pPlayer->m_pRotatingHearts = new CRotatingHearts(&GameServer()->m_World, pPlayer->GetCID());

	m_FreezeTimer = 0;

	for (int i = 0; i < m_AnimIDNum; i++)//snap ids
		m_apAnimIDs[i] = Server()->SnapNewID();

	return true;
}

void CCharacter::Destroy()
{
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	m_Alive = false;
}

void CCharacter::SetWeapon(int W)
{
	if (W == m_Core.m_ActiveWeapon)
		return;

	m_LastWeapon = m_Core.m_ActiveWeapon;
	m_QueuedWeapon = -1;
	m_Core.m_ActiveWeapon = W;
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));

	if (m_Core.m_ActiveWeapon < 0 || m_Core.m_ActiveWeapon >= NUM_WEAPONS)
		m_Core.m_ActiveWeapon = 0;
}

bool CCharacter::IsGrounded()
{
	if (GameServer()->Collision()->CheckPoint(m_Pos.x + m_ProximityRadius / 2, m_Pos.y + m_ProximityRadius / 2 + 5))
		return true;
	if (GameServer()->Collision()->CheckPoint(m_Pos.x - m_ProximityRadius / 2, m_Pos.y + m_ProximityRadius / 2 + 5))
		return true;

	int index = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x, m_Pos.y + m_ProximityRadius / 2 + 4));
	int tile = GameServer()->Collision()->GetTileIndex(index);
	int flags = GameServer()->Collision()->GetTileFlags(index);
	if (tile == TILE_STOPA || (tile == TILE_STOP && flags == ROTATION_0) || (tile == TILE_STOPS && (flags == ROTATION_0 || flags == ROTATION_180)))
		return true;
	tile = GameServer()->Collision()->GetFTileIndex(index);
	flags = GameServer()->Collision()->GetFTileFlags(index);
	if (tile == TILE_STOPA || (tile == TILE_STOP && flags == ROTATION_0) || (tile == TILE_STOPS && (flags == ROTATION_0 || flags == ROTATION_180)))
		return true;

	return false;
}

void CCharacter::HandleJetpack()
{
	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	bool FullAuto = false;
	if (m_Core.m_ActiveWeapon == WEAPON_GRENADE || m_Core.m_ActiveWeapon == WEAPON_SHOTGUN || m_Core.m_ActiveWeapon == WEAPON_RIFLE)
		FullAuto = true;
	if (m_Jetpack && m_Core.m_ActiveWeapon == WEAPON_GUN)
		FullAuto = true;

	// check if we gonna fire
	bool WillFire = false;
	if (CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		WillFire = true;

	if (FullAuto && (m_LatestInput.m_Fire & 1) && m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
		WillFire = true;

	if (!WillFire)
		return;

	// check for ammo
	if (!m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
	{
		return;
	}

	switch (m_Core.m_ActiveWeapon)
	{
	case WEAPON_GUN:
	{
		if (m_Jetpack)
		{
			float Strength;
			if (!m_TuneZone)
				Strength = GameServer()->Tuning()->m_JetpackStrength;
			else
				Strength = GameServer()->TuningList()[m_TuneZone].m_JetpackStrength;
			TakeDamage(Direction * -1.0f * (Strength / 100.0f / 6.11f), g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage, m_pPlayer->GetCID(), m_Core.m_ActiveWeapon);
		}
	}
	}
}

void CCharacter::HandleNinja()
{
	if (m_Core.m_ActiveWeapon != WEAPON_NINJA)
		return;

	if ((Server()->Tick() - m_Ninja.m_ActivationTick) > (g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000))
	{
		// time's up, return
		RemoveNinja();
		return;
	}

	int NinjaTime = m_Ninja.m_ActivationTick + (g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000) - Server()->Tick();

	if (NinjaTime % Server()->TickSpeed() == 0 && NinjaTime / Server()->TickSpeed() <= 5)
	{
		GameServer()->CreateDamageInd(m_Pos, 0, NinjaTime / Server()->TickSpeed(), Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
	}

	m_Armor = 10 - (NinjaTime / 15);

	// force ninja Weapon
	SetWeapon(WEAPON_NINJA);

	m_Ninja.m_CurrentMoveTime--;

	if (m_Ninja.m_CurrentMoveTime == 0)
	{
		// reset velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir*m_Ninja.m_OldVelAmount;
	}

	if (m_Ninja.m_CurrentMoveTime > 0)
	{
		// Set velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir * g_pData->m_Weapons.m_Ninja.m_Velocity;
		vec2 OldPos = m_Pos;
		GameServer()->Collision()->MoveBox(&m_Core.m_Pos, &m_Core.m_Vel, vec2(m_ProximityRadius, m_ProximityRadius), 0.f);

		// reset velocity so the client doesn't predict stuff
		m_Core.m_Vel = vec2(0.f, 0.f);

		// check if we Hit anything along the way
		{
			CCharacter *aEnts[MAX_CLIENTS];
			vec2 Dir = m_Pos - OldPos;
			float Radius = m_ProximityRadius * 2.0f;
			vec2 Center = OldPos + Dir * 0.5f;
			int Num = GameServer()->m_World.FindEntities(Center, Radius, (CEntity**)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

			// check that we're not in solo part
			if (Teams()->m_Core.GetSolo(m_pPlayer->GetCID()))
				return;

			for (int i = 0; i < Num; ++i)
			{
				if (aEnts[i] == this)
					continue;

				// Don't hit players in other teams
				if (Team() != aEnts[i]->Team())
					continue;

				// Don't hit players in solo parts
				if (Teams()->m_Core.GetSolo(aEnts[i]->m_pPlayer->GetCID()))
					return;

				// make sure we haven't Hit this object before
				bool bAlreadyHit = false;
				for (int j = 0; j < m_NumObjectsHit; j++)
				{
					if (m_apHitObjects[j] == aEnts[i])
						bAlreadyHit = true;
				}
				if (bAlreadyHit)
					continue;

				// check so we are sufficiently close
				if (distance(aEnts[i]->m_Pos, m_Pos) >(m_ProximityRadius * 2.0f))
					continue;

				// Hit a player, give him damage and stuffs...
				GameServer()->CreateSound(aEnts[i]->m_Pos, SOUND_NINJA_HIT, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
				// set his velocity to fast upward (for now)
				if (m_NumObjectsHit < 10)
					m_apHitObjects[m_NumObjectsHit++] = aEnts[i];

				aEnts[i]->TakeDamage(vec2(0, -10.0f), g_pData->m_Weapons.m_Ninja.m_pBase->m_Damage, m_pPlayer->GetCID(), WEAPON_NINJA);
			}
		}

		return;
	}

	return;
}


void CCharacter::DoWeaponSwitch()
{
	// make sure we can switch
	if (m_ReloadTimer != 0 || m_QueuedWeapon == -1 || m_aWeapons[WEAPON_NINJA].m_Got)
		return;

	// switch Weapon
	SetWeapon(m_QueuedWeapon);
}

void CCharacter::HandleWeaponSwitch()
{
	int WantedWeapon = m_Core.m_ActiveWeapon;
	if (m_QueuedWeapon != -1)
		WantedWeapon = m_QueuedWeapon;

	bool Anything = false;
	for (int i = 0; i < NUM_WEAPONS - 1; ++i)
		if (m_aWeapons[i].m_Got)
			Anything = true;
	if (!Anything)
		return;
	// select Weapon
	int Next = CountInput(m_LatestPrevInput.m_NextWeapon, m_LatestInput.m_NextWeapon).m_Presses;
	int Prev = CountInput(m_LatestPrevInput.m_PrevWeapon, m_LatestInput.m_PrevWeapon).m_Presses;

	if (Next < 128) // make sure we only try sane stuff
	{
		while (Next) // Next Weapon selection
		{
			WantedWeapon = (WantedWeapon + 1) % NUM_WEAPONS;
			if (m_aWeapons[WantedWeapon].m_Got)
				Next--;
		}
	}

	if (Prev < 128) // make sure we only try sane stuff
	{
		while (Prev) // Prev Weapon selection
		{
			WantedWeapon = (WantedWeapon - 1)<0 ? NUM_WEAPONS - 1 : WantedWeapon - 1;
			if (m_aWeapons[WantedWeapon].m_Got)
				Prev--;
		}
	}

	// Direct Weapon selection
	if (m_LatestInput.m_WantedWeapon)
		WantedWeapon = m_Input.m_WantedWeapon - 1;

	// check for insane values
	if (WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != m_Core.m_ActiveWeapon && m_aWeapons[WantedWeapon].m_Got)
		m_QueuedWeapon = WantedWeapon;

	DoWeaponSwitch();
}

void CCharacter::EmoteCheck(int Index)
{
	if (GetPlayer()->GetCharacter() && Index == EMOTICON_EYES)
		m_pPlayer->m_IsEmote = true;
	else
		m_pPlayer->m_IsEmote = false;
}


void CCharacter::FireWeapon()
{
	if (m_PassiveMode)
		return;
	if (m_ReloadTimer != 0 && !m_XXL)
		return;

	if (GameServer()->m_FlagHuntCarrier == m_pPlayer->GetCID())
	{
		if (CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
			GameServer()->CreateSound(m_Pos, SOUND_CTF_RETURN, -1);
		return;
	}

	DoWeaponSwitch();
	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	bool FullAuto = false;
	if (m_XXL || m_Pullhammer || m_Core.m_ActiveWeapon == WEAPON_GRENADE || m_Core.m_ActiveWeapon == WEAPON_SHOTGUN || m_Core.m_ActiveWeapon == WEAPON_RIFLE)
		FullAuto = true;
	if (m_Jetpack && m_Core.m_ActiveWeapon == WEAPON_GUN)
		FullAuto = true;

	// don't fire non auto weapons when player is deep and sv_deepfly is disabled
	if (!g_Config.m_SvDeepfly && !FullAuto && m_DeepFreeze)
		return;

	// check if we gonna fire
	bool WillFire = false;
	if (CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		WillFire = true;

	if (FullAuto && (m_LatestInput.m_Fire & 1) && m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
		WillFire = true;

	m_IsFiring = WillFire;

	if (!WillFire && !m_Fire)
	{
		if (m_Pullhammer)
			m_PullingID = -1;
		return;
	}

	if (!IsGrounded() && (m_pPlayer->m_KillMe == 3 || m_LastBonus))
	{
		m_pPlayer->m_KillMe++;
	}

	// check for ammo
	if (!m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
	{
		/*// 125ms is a magical limit of how fast a human can click
		m_ReloadTimer = 125 * Server()->TickSpeed() / 1000;
		GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO);*/
		// Timer stuff to avoid shrieking orchestra caused by unfreeze-plasma
		if (m_PainSoundTimer <= 0)
		{
			m_PainSoundTimer = 1 * Server()->TickSpeed();
			GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
		}
		return;
	}

	vec2 ProjStartPos = m_Pos + Direction*m_ProximityRadius*0.75f;

	if(m_Pullhammer && m_Core.m_ActiveWeapon == WEAPON_HAMMER)
	{
		return;
	}


	switch (m_Core.m_ActiveWeapon)
	{
	case WEAPON_HAMMER:
	{
		// reset objects Hit
		m_NumObjectsHit = 0;
		GameServer()->CreateSound(m_Pos, SOUND_HAMMER_FIRE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));

		if (m_Hit&DISABLE_HIT_HAMMER) break;

		CCharacter *apEnts[MAX_CLIENTS];
		int Hits = 0;
		int Num = GameServer()->m_World.FindEntities(ProjStartPos, m_ProximityRadius*0.5f, (CEntity**)apEnts,
			MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

		for (int i = 0; i < Num; ++i)
		{
			CCharacter *pTarget = apEnts[i];
			if (pTarget->m_PassiveMode) // So dey Dont BLOOOKEE
				return;
			//if ((pTarget == this) || GameServer()->Collision()->IntersectLine(ProjStartPos, pTarget->m_Pos, NULL, NULL))
			if ((pTarget == this || (pTarget->IsAlive() && !CanCollide(pTarget->GetPlayer()->GetCID()))))
				continue;

			// set his velocity to fast upward (for now)
			if (length(pTarget->m_Pos - ProjStartPos) > 0.0f)
				GameServer()->CreateHammerHit(pTarget->m_Pos - normalize(pTarget->m_Pos - ProjStartPos)*m_ProximityRadius*0.5f, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
			else
				GameServer()->CreateHammerHit(ProjStartPos, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));

			vec2 Dir;
			if (length(pTarget->m_Pos - m_Pos) > 0.0f)
				Dir = normalize(pTarget->m_Pos - m_Pos);
			else
				Dir = vec2(0.f, -1.f);
			/*pTarget->TakeDamage(vec2(0.f, -1.f) + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f, g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage,
			m_pPlayer->GetCID(), m_Core.m_ActiveWeapon);*/

			float Strength;
			if (!m_TuneZone)
				Strength = GameServer()->Tuning()->m_HammerStrength;
			else
				Strength = GameServer()->TuningList()[m_TuneZone].m_HammerStrength;

			vec2 Temp = pTarget->m_Core.m_Vel + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f * (m_HammerStrenght + 1);
			if (Temp.x > 0 && ((pTarget->m_TileIndex == TILE_STOP && pTarget->m_TileFlags == ROTATION_270) || (pTarget->m_TileIndexL == TILE_STOP && pTarget->m_TileFlagsL == ROTATION_270) || (pTarget->m_TileIndexL == TILE_STOPS && (pTarget->m_TileFlagsL == ROTATION_90 || pTarget->m_TileFlagsL == ROTATION_270)) || (pTarget->m_TileIndexL == TILE_STOPA) || (pTarget->m_TileFIndex == TILE_STOP && pTarget->m_TileFFlags == ROTATION_270) || (pTarget->m_TileFIndexL == TILE_STOP && pTarget->m_TileFFlagsL == ROTATION_270) || (pTarget->m_TileFIndexL == TILE_STOPS && (pTarget->m_TileFFlagsL == ROTATION_90 || pTarget->m_TileFFlagsL == ROTATION_270)) || (pTarget->m_TileFIndexL == TILE_STOPA) || (pTarget->m_TileSIndex == TILE_STOP && pTarget->m_TileSFlags == ROTATION_270) || (pTarget->m_TileSIndexL == TILE_STOP && pTarget->m_TileSFlagsL == ROTATION_270) || (pTarget->m_TileSIndexL == TILE_STOPS && (pTarget->m_TileSFlagsL == ROTATION_90 || pTarget->m_TileSFlagsL == ROTATION_270)) || (pTarget->m_TileSIndexL == TILE_STOPA)))
				Temp.x = 0;
			if (Temp.x < 0 && ((pTarget->m_TileIndex == TILE_STOP && pTarget->m_TileFlags == ROTATION_90) || (pTarget->m_TileIndexR == TILE_STOP && pTarget->m_TileFlagsR == ROTATION_90) || (pTarget->m_TileIndexR == TILE_STOPS && (pTarget->m_TileFlagsR == ROTATION_90 || pTarget->m_TileFlagsR == ROTATION_270)) || (pTarget->m_TileIndexR == TILE_STOPA) || (pTarget->m_TileFIndex == TILE_STOP && pTarget->m_TileFFlags == ROTATION_90) || (pTarget->m_TileFIndexR == TILE_STOP && pTarget->m_TileFFlagsR == ROTATION_90) || (pTarget->m_TileFIndexR == TILE_STOPS && (pTarget->m_TileFFlagsR == ROTATION_90 || pTarget->m_TileFFlagsR == ROTATION_270)) || (pTarget->m_TileFIndexR == TILE_STOPA) || (pTarget->m_TileSIndex == TILE_STOP && pTarget->m_TileSFlags == ROTATION_90) || (pTarget->m_TileSIndexR == TILE_STOP && pTarget->m_TileSFlagsR == ROTATION_90) || (pTarget->m_TileSIndexR == TILE_STOPS && (pTarget->m_TileSFlagsR == ROTATION_90 || pTarget->m_TileSFlagsR == ROTATION_270)) || (pTarget->m_TileSIndexR == TILE_STOPA)))
				Temp.x = 0;
			if (Temp.y < 0 && ((pTarget->m_TileIndex == TILE_STOP && pTarget->m_TileFlags == ROTATION_180) || (pTarget->m_TileIndexB == TILE_STOP && pTarget->m_TileFlagsB == ROTATION_180) || (pTarget->m_TileIndexB == TILE_STOPS && (pTarget->m_TileFlagsB == ROTATION_0 || pTarget->m_TileFlagsB == ROTATION_180)) || (pTarget->m_TileIndexB == TILE_STOPA) || (pTarget->m_TileFIndex == TILE_STOP && pTarget->m_TileFFlags == ROTATION_180) || (pTarget->m_TileFIndexB == TILE_STOP && pTarget->m_TileFFlagsB == ROTATION_180) || (pTarget->m_TileFIndexB == TILE_STOPS && (pTarget->m_TileFFlagsB == ROTATION_0 || pTarget->m_TileFFlagsB == ROTATION_180)) || (pTarget->m_TileFIndexB == TILE_STOPA) || (pTarget->m_TileSIndex == TILE_STOP && pTarget->m_TileSFlags == ROTATION_180) || (pTarget->m_TileSIndexB == TILE_STOP && pTarget->m_TileSFlagsB == ROTATION_180) || (pTarget->m_TileSIndexB == TILE_STOPS && (pTarget->m_TileSFlagsB == ROTATION_0 || pTarget->m_TileSFlagsB == ROTATION_180)) || (pTarget->m_TileSIndexB == TILE_STOPA)))
				Temp.y = 0;
			if (Temp.y > 0 && ((pTarget->m_TileIndex == TILE_STOP && pTarget->m_TileFlags == ROTATION_0) || (pTarget->m_TileIndexT == TILE_STOP && pTarget->m_TileFlagsT == ROTATION_0) || (pTarget->m_TileIndexT == TILE_STOPS && (pTarget->m_TileFlagsT == ROTATION_0 || pTarget->m_TileFlagsT == ROTATION_180)) || (pTarget->m_TileIndexT == TILE_STOPA) || (pTarget->m_TileFIndex == TILE_STOP && pTarget->m_TileFFlags == ROTATION_0) || (pTarget->m_TileFIndexT == TILE_STOP && pTarget->m_TileFFlagsT == ROTATION_0) || (pTarget->m_TileFIndexT == TILE_STOPS && (pTarget->m_TileFFlagsT == ROTATION_0 || pTarget->m_TileFFlagsT == ROTATION_180)) || (pTarget->m_TileFIndexT == TILE_STOPA) || (pTarget->m_TileSIndex == TILE_STOP && pTarget->m_TileSFlags == ROTATION_0) || (pTarget->m_TileSIndexT == TILE_STOP && pTarget->m_TileSFlagsT == ROTATION_0) || (pTarget->m_TileSIndexT == TILE_STOPS && (pTarget->m_TileSFlagsT == ROTATION_0 || pTarget->m_TileSFlagsT == ROTATION_180)) || (pTarget->m_TileSIndexT == TILE_STOPA)))
				Temp.y = 0;
			Temp -= pTarget->m_Core.m_Vel;
			pTarget->TakeDamage((vec2(0.f, -1.0f) + Temp) * Strength, g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage,
				m_pPlayer->GetCID(), m_Core.m_ActiveWeapon);
			if (!pTarget->m_PassiveMode) // cannot be unfreezed, If so easy bypass method brought to my attention by Delith
				pTarget->UnFreeze();

			if (m_pPlayer->m_QuestData.QuestActive() &&
				GetPlayer()->m_QuestData.m_QuestPart == CPlayer::QUEST_PART_HAMMER && pTarget->GetPlayer()->GetCID() == GetPlayer()->m_QuestData.m_VictimID)
			{
				GetPlayer()->QuestSetNextPart();
			}

			if (m_FreezeHammer)
				pTarget->Freeze();

			Hits++;
		}

		if (m_pPlayer->m_Vacuum < g_Config.m_SvBlackHolescount && m_pPlayer->m_Blackhole) /* Check Item Value */
		{
			new CVacuum(GameWorld(), vec2(m_Input.m_TargetX, m_Input.m_TargetY) + m_Pos, m_pPlayer->GetCID());
			m_pPlayer->m_Vacuum++;
		}

		// if we Hit anything, we have to wait for the reload
		if (Hits)
			m_ReloadTimer = Server()->TickSpeed() / 3;

	} break;

	case WEAPON_GUN:
	{
		if(m_TempHasSword)
		{
			CCharacter * pTarget = GameWorld()->ClosestCharacter(ProjStartPos, 16.0f, this);

			if(pTarget)
				GameServer()->CreateSound(m_Pos, SOUND_NINJA_HIT);
			else
				GameServer()->CreateSound(m_Pos, SOUND_HAMMER_FIRE);
			if(pTarget)
				pTarget->TakeDamage(Direction*4.0f, 0, m_pPlayer->GetCID(), WEAPON_NINJA);
		}
		else if (!m_Jetpack || !m_pPlayer->m_NinjaJetpack)
		{
			int Lifetime;
			if (!m_TuneZone)
				Lifetime = (int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GunLifetime);
			else
				Lifetime = (int)(Server()->TickSpeed()*GameServer()->TuningList()[m_TuneZone].m_GunLifetime);

			CProjectile *pProj = new CProjectile
				(
					GameWorld(),
					WEAPON_GUN,//Type
					m_pPlayer->GetCID(),//Owner
					ProjStartPos,//Pos
					Direction,//Dir
					Lifetime,//Span
					0,//Freeze
					0,//Explosive
					0,//Force
					-1,//SoundImpact
					WEAPON_GUN//Weapon
					);

			// pack the Projectile and send it to the client Directly
			CNetObj_Projectile p;
			pProj->FillInfo(&p);

			CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
			Msg.AddInt(1);
			for (unsigned i = 0; i < sizeof(CNetObj_Projectile) / sizeof(int); i++)
				Msg.AddInt(((int *)&p)[i]);

			Server()->SendMsg(&Msg, MSGFLAG_VITAL, m_pPlayer->GetCID());
			GameServer()->CreateSound(m_Pos, SOUND_GUN_FIRE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
		}
	} break;

	case WEAPON_SHOTGUN:
	{
		/*int ShotSpread = 2;

		CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
		Msg.AddInt(ShotSpread*2+1);

		for(int i = -ShotSpread; i <= ShotSpread; ++i)
		{
		float Spreading[] = {-0.185f, -0.070f, 0, 0.070f, 0.185f};
		float a = GetAngle(Direction);
		a += Spreading[i+2];
		float v = 1-(absolute(i)/(float)ShotSpread);
		float Speed = mix((float)GameServer()->Tuning()->m_ShotgunSpeeddiff, 1.0f, v);
		CProjectile *pProj = new CProjectile(GameWorld(), WEAPON_SHOTGUN,
		m_pPlayer->GetCID(),
		ProjStartPos,
		vec2(cosf(a), sinf(a))*Speed,
		(int)(Server()->TickSpeed()*GameServer()->Tuning()->m_ShotgunLifetime),
		1, 0, 0, -1, WEAPON_SHOTGUN);

		// pack the Projectile and send it to the client Directly
		CNetObj_Projectile p;
		pProj->FillInfo(&p);

		for(unsigned i = 0; i < sizeof(CNetObj_Projectile)/sizeof(int); i++)
		Msg.AddInt(((int *)&p)[i]);
		}

		Server()->SendMsg(&Msg, MSGFLAG_VITAL, m_pPlayer->GetCID());

		GameServer()->CreateSound(m_Pos, SOUND_SHOTGUN_FIRE);*/
		float LaserReach;
		if (!m_TuneZone)
			LaserReach = GameServer()->Tuning()->m_LaserReach;
		else
			LaserReach = GameServer()->TuningList()[m_TuneZone].m_LaserReach;

		new CLaser(&GameServer()->m_World, m_Pos, Direction, LaserReach, m_pPlayer->GetCID(), WEAPON_SHOTGUN);
		GameServer()->CreateSound(m_Pos, SOUND_SHOTGUN_FIRE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
	} break;

	case WEAPON_GRENADE:
	{
		int Lifetime;
		if (!m_TuneZone)
			Lifetime = (int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GrenadeLifetime);
		else
			Lifetime = (int)(Server()->TickSpeed()*GameServer()->TuningList()[m_TuneZone].m_GrenadeLifetime);

		if (m_pPlayer->m_IsRocket)
			new CRocket(&GameServer()->m_World, m_pPlayer->GetCID(), Direction, ProjStartPos);
		else
		{
			CProjectile *pProj = new CProjectile
				(
					GameWorld(),
					WEAPON_GRENADE,//Type
					m_pPlayer->GetCID(),//Owner
					ProjStartPos,//Pos
					Direction,//Dir
					Lifetime,//Span
					0,//Freeze
					true,//Explosive
					0,//Force
					SOUND_GRENADE_EXPLODE,//SoundImpact
					WEAPON_GRENADE//Weapon
					);//SoundImpact

					  // pack the Projectile and send it to the client Directly
			CNetObj_Projectile p;
			pProj->FillInfo(&p);

			CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
			Msg.AddInt(1);
			for (unsigned i = 0; i < sizeof(CNetObj_Projectile) / sizeof(int); i++)
				Msg.AddInt(((int *)&p)[i]);
			Server()->SendMsg(&Msg, MSGFLAG_VITAL, m_pPlayer->GetCID());
		}

		GameServer()->CreateSound(m_Pos, SOUND_GRENADE_FIRE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
	} break;

	case WEAPON_RIFLE:
	{
		float LaserReach;
		if (!m_TuneZone)
			LaserReach = GameServer()->Tuning()->m_LaserReach;
		else
			LaserReach = GameServer()->TuningList()[m_TuneZone].m_LaserReach;

		if(m_pPlayer->m_LightSaber)
		{
			if(!m_LightSaberActivated)
				new CLightSaber(GameWorld(), m_pPlayer->GetCID());
			m_LightSaberActivated ^= true;
		}
		else if(m_pPlayer->m_LightningLaser)
		{
			new CLightningLaser(GameWorld(), m_Pos, Direction, m_pPlayer->GetCID());
			m_ReloadTimer = Server()->TickSpeed()/7;
		}
		else
		{
			new CLaser(GameWorld(), m_Pos, Direction, LaserReach, m_pPlayer->GetCID(), WEAPON_RIFLE);
		}

		GameServer()->CreateSound(m_Pos, SOUND_RIFLE_FIRE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
	} break;

	case WEAPON_NINJA:
	{
		// reset Hit objects
		m_NumObjectsHit = 0;

		m_Ninja.m_ActivationDir = Direction;
		m_Ninja.m_CurrentMoveTime = g_pData->m_Weapons.m_Ninja.m_Movetime * Server()->TickSpeed() / 1000;
		m_Ninja.m_OldVelAmount = length(m_Core.m_Vel);

		GameServer()->CreateSound(m_Pos, SOUND_NINJA_FIRE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
	} break;

	}

	m_AttackTick = Server()->Tick();

	/*if(m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo > 0) // -1 == unlimited
	m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo--;*/

	if (!m_ReloadTimer)
	{
		float FireDelay;
		if (!m_TuneZone)
			GameServer()->Tuning()->Get(38 + m_Core.m_ActiveWeapon, &FireDelay);
		else
			GameServer()->TuningList()[m_TuneZone].Get(38 + m_Core.m_ActiveWeapon, &FireDelay);
		m_ReloadTimer = FireDelay * Server()->TickSpeed() / 1000;
	}
}

void CCharacter::HandleWeapons()
{
	//ninja
	HandleNinja();
	HandleJetpack();

	if (m_PainSoundTimer > 0)
		m_PainSoundTimer--;

	// check reload timer
	if (m_ReloadTimer)
	{
		m_ReloadTimer--;
		return;
	}

	// fire Weapon, if wanted
	FireWeapon();
	/*
	// ammo regen
	int AmmoRegenTime = g_pData->m_Weapons.m_aId[m_Core.m_ActiveWeapon].m_Ammoregentime;
	if(AmmoRegenTime)
	{
	// If equipped and not active, regen ammo?
	if (m_ReloadTimer <= 0)
	{
	if (m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart < 0)
	m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = Server()->Tick();

	if ((Server()->Tick() - m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart) >= AmmoRegenTime * Server()->TickSpeed() / 1000)
	{
	// Add some ammo
	m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo = min(m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo + 1, 10);
	m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = -1;
	}
	}
	else
	{
	m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = -1;
	}
	}*/

	return;
}

void CCharacter::GiveNinja()
{
	m_Ninja.m_ActivationTick = Server()->Tick();
	m_aWeapons[WEAPON_NINJA].m_Got = true;
	if (!m_FreezeTime)
		m_aWeapons[WEAPON_NINJA].m_Ammo = -1;
	if (m_Core.m_ActiveWeapon != WEAPON_NINJA)
		m_LastWeapon = m_Core.m_ActiveWeapon;
	m_Core.m_ActiveWeapon = WEAPON_NINJA;

	if (!m_aWeapons[WEAPON_NINJA].m_Got)
		GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
}

void CCharacter::RemoveNinja()
{
	m_Ninja.m_CurrentMoveTime = 0;
	m_aWeapons[WEAPON_NINJA].m_Got = false;
	m_Core.m_ActiveWeapon = m_LastWeapon;

	SetWeapon(m_Core.m_ActiveWeapon);
}

void CCharacter::SetEmote(int Emote, int Tick)
{
	m_EmoteType = Emote;
	m_EmoteStop = Tick;
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
{
	// check for changes
	if (mem_comp(&m_SavedInput, pNewInput, sizeof(CNetObj_PlayerInput)) != 0)
		m_LastAction = Server()->Tick();

	// copy new input
	mem_copy(&m_SavedInput, pNewInput, sizeof(m_SavedInput));
	mem_copy(&m_Input, pNewInput, sizeof(m_Input));
	m_NumInputs++;

	// it is not allowed to aim in the center
	if (m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		m_Input.m_TargetY = -1;
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));

	// it is not allowed to aim in the center
	if (m_LatestInput.m_TargetX == 0 && m_LatestInput.m_TargetY == 0)
		m_LatestInput.m_TargetY = -1;

	if (m_NumInputs > 2 && m_pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		HandleWeaponSwitch();
		FireWeapon();
	}

	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

void CCharacter::ResetInput()
{
	m_Input.m_Direction = 0;
	// simulate releasing the fire button
	if ((m_Input.m_Fire & 1) != 0)
		m_Input.m_Fire++;
	m_Input.m_Fire &= INPUT_STATE_MASK;
	m_Input.m_Jump = 0;
	m_LatestPrevInput = m_LatestInput = m_Input;
}

void CCharacter::HandleFlaghunt()
{
	if (GameServer()->m_FlagHuntCarrier != m_pPlayer->GetCID())
		return;

	HandleHookJetpack();

	m_Core.m_Jumped = 0;
	m_FreezeTime = 0;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(i);
		if (pChr == 0x0 || i == m_pPlayer->GetCID())
			continue;

		if (pChr->Core()->m_HookedPlayer == m_pPlayer->GetCID() && pChr->Core()->m_HookState == HOOK_GRABBED)
		{
			pChr->Core()->m_HookState = HOOK_RETRACTED;
			pChr->Core()->m_HookedPlayer = -1;
		}
	}
}

void CCharacter::Tick()
{
	if (m_Paused)
		return;

	HandleFlaghunt();

	m_TimerBeforeProcess--;
	if(!m_ProcessBanChecked && m_TimerBeforeProcess < 0)
	{
		m_ProcessBanChecked = true;
		m_TimerBeforeProcess = Server()->TickSpeed()+5;
		GameServer()->ProcessClientBan(m_pPlayer->GetCID());
		return;
	}

	Clean();

	if(m_pPlayer->m_InLMB == LMB_PARTICIPATE)
	{
		SetEmoteType(EMOTE_NORMAL);
	}

	m_LovelyLifeSpan--;
	m_BloodyDelay++;
	m_SteamyDelay++;
	HandleLovely();
	HandlePullHammer();
	if(m_pPlayer->m_HookJetpack)
		HandleHookJetpack();

	if(m_pPlayer->m_Invisible)
	{
		HandleCollision(false); // if you go throught collision tile, it will not affect m_Invisible.
	}

	if(m_Bloody && m_BloodyDelay > g_Config.m_ClBloodyDelay)
	{
		GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID(), Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
		m_BloodyDelay = 1;
	}

	if(m_Steamy && m_SteamyDelay > g_Config.m_ClSteamyDelay)
	{
		GameServer()->CreatePlayerSpawn(m_Pos, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
		m_SteamyDelay = 1;
	}

	HandleRainbowHook(false);

	HandleGameModes();
	HandleLevelSystem();
	HandleBots();
	HandleThreeSecondRule();
	DDRaceTick();

	m_Core.m_Input = m_Input;
	m_Core.Tick(true, false);

	// handle Weapons
	HandleWeapons();

	DDRacePostCoreTick();
	SpecialPostCoreTick();

	// Previnput
	m_PrevInput = m_Input;

	m_PrevPos = m_Core.m_Pos;
	return;
}

void CCharacter::TickDefered()
{
	// advance the dummy
	{
		CWorldCore TempWorld;
		m_ReckoningCore.Init(&TempWorld, GameServer()->Collision(), &((CGameControllerDDRace*)GameServer()->m_pController)->m_Teams.m_Core, &((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts);
		m_ReckoningCore.m_Id = m_pPlayer->GetCID();
		m_ReckoningCore.Tick(false, false);
		m_ReckoningCore.Move();
		m_ReckoningCore.Quantize();
	}

	//lastsentcore
	vec2 StartPos = m_Core.m_Pos;
	vec2 StartVel = m_Core.m_Vel;
	bool StuckBefore = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));

	m_Core.m_Id = m_pPlayer->GetCID();
	m_Core.Move();
	bool StuckAfterMove = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Core.Quantize();
	bool StuckAfterQuant = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Pos = m_Core.m_Pos;

	if (!StuckBefore && (StuckAfterMove || StuckAfterQuant))
	{
		// Hackish solution to get rid of strict-aliasing warning
		union
		{
			float f;
			unsigned u;
		}StartPosX, StartPosY, StartVelX, StartVelY;

		StartPosX.f = StartPos.x;
		StartPosY.f = StartPos.y;
		StartVelX.f = StartVel.x;
		StartVelY.f = StartVel.y;

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x",
			StuckBefore,
			StuckAfterMove,
			StuckAfterQuant,
			StartPos.x, StartPos.y,
			StartVel.x, StartVel.y,
			StartPosX.u, StartPosY.u,
			StartVelX.u, StartVelY.u);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	int Events = m_Core.m_TriggeredEvents;
	//int Mask = CmaskAllExceptOne(m_pPlayer->GetCID());

	if (Events&COREEVENT_GROUND_JUMP) GameServer()->CreateSound(m_Pos, SOUND_PLAYER_JUMP, Teams()->TeamMask(Team(), m_pPlayer->GetCID()));

	if (Events&COREEVENT_HOOK_ATTACH_PLAYER) GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_PLAYER, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
	if (Events&COREEVENT_HOOK_ATTACH_GROUND) GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_GROUND, Teams()->TeamMask(Team(), m_pPlayer->GetCID(), m_pPlayer->GetCID()));
	if (Events&COREEVENT_HOOK_HIT_NOHOOK) GameServer()->CreateSound(m_Pos, SOUND_HOOK_NOATTACH, Teams()->TeamMask(Team(), m_pPlayer->GetCID(), m_pPlayer->GetCID()));


	if (m_pPlayer->GetTeam() == TEAM_SPECTATORS)
	{
		m_Pos.x = m_Input.m_TargetX;
		m_Pos.y = m_Input.m_TargetY;
	}

	// update the m_SendCore if needed
	{
		CNetObj_Character Predicted;
		CNetObj_Character Current;
		mem_zero(&Predicted, sizeof(Predicted));
		mem_zero(&Current, sizeof(Current));
		m_ReckoningCore.Write(&Predicted);
		m_Core.Write(&Current);

		// only allow dead reackoning for a top of 3 seconds
		if (m_Core.m_pReset || m_ReckoningTick + Server()->TickSpeed() * 3 < Server()->Tick() || mem_comp(&Predicted, &Current, sizeof(CNetObj_Character)) != 0)
		{
			m_ReckoningTick = Server()->Tick();
			m_SendCore = m_Core;
			m_ReckoningCore = m_Core;
			m_Core.m_pReset = false;
		}
	}
}

void CCharacter::TickPaused()
{
	++m_AttackTick;
	++m_DamageTakenTick;
	++m_Ninja.m_ActivationTick;
	++m_ReckoningTick;
	if (m_LastAction != -1)
		++m_LastAction;
	if (m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart > -1)
		++m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart;
	if (m_EmoteStop > -1)
		++m_EmoteStop;
}

bool CCharacter::IncreaseHealth(int Amount)
{
	if (m_Health >= 10)
		return false;
	m_Health = clamp(m_Health + Amount, 0, 10);
	return true;
}

bool CCharacter::IncreaseArmor(int Amount)
{
	if (m_Armor >= 10)
		return false;
	m_Armor = clamp(m_Armor + Amount, 0, 10);
	return true;
}

void CCharacter::Die(int Killer, int Weapon)
{
	{//release snapped IDs
		for (int i = 0; i < m_AnimIDNum; i++)
		{
			dbg_assert(m_apAnimIDs[i] != -1, "Character Release ID Error");
			if (m_apAnimIDs[i] > 0)
			{
				Server()->SnapFreeID(m_apAnimIDs[i]);
				//dbg_msg("ASDF", "Releasing ID %d" ,m_apAnimIDs[i]);
			}
		}
	}

	HandleBlocking(true);

	if (GetPlayer()->GetCharacter() && GameServer()->GetPlayerChar(GetPlayer()->GetCharacter()->Core()->m_LastHookedBy))
		GetPlayer()->m_Killedby = GetPlayer()->GetCharacter()->Core()->m_LastHookedBy;

	if (Server()->IsRecording(m_pPlayer->GetCID()))
		Server()->StopRecord(m_pPlayer->GetCID());

	HandleRainbowHook(true);

	int ModeSpecial = GameServer()->m_pController->OnCharacterDeath(this, GameServer()->m_apPlayers[Killer], Weapon);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "kill killer='%d:%s' victim='%d:%s' weapon=%d special=%d",
		Killer, Server()->ClientName(Killer),
		m_pPlayer->GetCID(), Server()->ClientName(m_pPlayer->GetCID()), Weapon, ModeSpecial);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// send the kill message
	CNetMsg_Sv_KillMsg Msg;
	Msg.m_Killer = Killer;
	Msg.m_Victim = m_pPlayer->GetCID();
	Msg.m_Weapon = Weapon;
	Msg.m_ModeSpecial = ModeSpecial;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

	// a nice sound
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));

	// this is for auto respawn after 3 secs
	m_pPlayer->m_DieTick = Server()->Tick();

	m_pPlayer->m_Vacuum = 0;
	m_pPlayer->m_IsEmote = false;
	m_LastBlockedTick = 0;
	m_Alive = false;
	GameServer()->m_World.RemoveEntity(this);
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID(), Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
	Teams()->OnCharacterDeath(GetPlayer()->GetCID(), Weapon);
}

bool CCharacter::TakeDamage(vec2 Force, int Dmg, int From, int Weapon)
{
	if (!m_Jetpack || m_Core.m_ActiveWeapon != WEAPON_GUN)
	{
		m_EmoteType = EMOTE_PAIN;
		m_EmoteStop = Server()->Tick() + 500 * Server()->TickSpeed() / 1000;
	}

	vec2 Temp = m_Core.m_Vel + Force;
	if (Temp.x > 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_270) || (m_TileIndexL == TILE_STOP && m_TileFlagsL == ROTATION_270) || (m_TileIndexL == TILE_STOPS && (m_TileFlagsL == ROTATION_90 || m_TileFlagsL == ROTATION_270)) || (m_TileIndexL == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_270) || (m_TileFIndexL == TILE_STOP && m_TileFFlagsL == ROTATION_270) || (m_TileFIndexL == TILE_STOPS && (m_TileFFlagsL == ROTATION_90 || m_TileFFlagsL == ROTATION_270)) || (m_TileFIndexL == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_270) || (m_TileSIndexL == TILE_STOP && m_TileSFlagsL == ROTATION_270) || (m_TileSIndexL == TILE_STOPS && (m_TileSFlagsL == ROTATION_90 || m_TileSFlagsL == ROTATION_270)) || (m_TileSIndexL == TILE_STOPA)))
		Temp.x = 0;
	if (Temp.x < 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_90) || (m_TileIndexR == TILE_STOP && m_TileFlagsR == ROTATION_90) || (m_TileIndexR == TILE_STOPS && (m_TileFlagsR == ROTATION_90 || m_TileFlagsR == ROTATION_270)) || (m_TileIndexR == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_90) || (m_TileFIndexR == TILE_STOP && m_TileFFlagsR == ROTATION_90) || (m_TileFIndexR == TILE_STOPS && (m_TileFFlagsR == ROTATION_90 || m_TileFFlagsR == ROTATION_270)) || (m_TileFIndexR == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_90) || (m_TileSIndexR == TILE_STOP && m_TileSFlagsR == ROTATION_90) || (m_TileSIndexR == TILE_STOPS && (m_TileSFlagsR == ROTATION_90 || m_TileSFlagsR == ROTATION_270)) || (m_TileSIndexR == TILE_STOPA)))
		Temp.x = 0;
	if (Temp.y < 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_180) || (m_TileIndexB == TILE_STOP && m_TileFlagsB == ROTATION_180) || (m_TileIndexB == TILE_STOPS && (m_TileFlagsB == ROTATION_0 || m_TileFlagsB == ROTATION_180)) || (m_TileIndexB == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_180) || (m_TileFIndexB == TILE_STOP && m_TileFFlagsB == ROTATION_180) || (m_TileFIndexB == TILE_STOPS && (m_TileFFlagsB == ROTATION_0 || m_TileFFlagsB == ROTATION_180)) || (m_TileFIndexB == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_180) || (m_TileSIndexB == TILE_STOP && m_TileSFlagsB == ROTATION_180) || (m_TileSIndexB == TILE_STOPS && (m_TileSFlagsB == ROTATION_0 || m_TileSFlagsB == ROTATION_180)) || (m_TileSIndexB == TILE_STOPA)))
		Temp.y = 0;
	if (Temp.y > 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_0) || (m_TileIndexT == TILE_STOP && m_TileFlagsT == ROTATION_0) || (m_TileIndexT == TILE_STOPS && (m_TileFlagsT == ROTATION_0 || m_TileFlagsT == ROTATION_180)) || (m_TileIndexT == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_0) || (m_TileFIndexT == TILE_STOP && m_TileFFlagsT == ROTATION_0) || (m_TileFIndexT == TILE_STOPS && (m_TileFFlagsT == ROTATION_0 || m_TileFFlagsT == ROTATION_180)) || (m_TileFIndexT == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_0) || (m_TileSIndexT == TILE_STOP && m_TileSFlagsT == ROTATION_0) || (m_TileSIndexT == TILE_STOPS && (m_TileSFlagsT == ROTATION_0 || m_TileSFlagsT == ROTATION_180)) || (m_TileSIndexT == TILE_STOPA)))
		Temp.y = 0;
	m_Core.m_Vel = Temp;

	return true;
}

void CCharacter::Snap(int SnappingClient)
{
	int id = m_pPlayer->GetCID();

	if (SnappingClient > -1 && !Server()->Translate(id, SnappingClient))
		return;

	if (NetworkClipped(SnappingClient))
		return;

	if (GameServer()->m_FlagHuntCarrier == m_pPlayer->GetCID())
	{
		if (SnappingClient == m_pPlayer->GetCID())
		{
			CNetObj_GameData *pGameDataObj = (CNetObj_GameData *)Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData));
			if (!pGameDataObj)
				return;

			pGameDataObj->m_TeamscoreRed = 0;
			pGameDataObj->m_TeamscoreBlue = 0;
			pGameDataObj->m_FlagCarrierBlue = -1;
			pGameDataObj->m_FlagCarrierRed = m_pPlayer->GetCID();
		}

		if (GameServer()->FlagHuntWarmup() == false || SnappingClient == m_pPlayer->GetCID())
		{
			static const int Team = 0;
			CNetObj_Flag *pFlag = (CNetObj_Flag *)Server()->SnapNewItem(NETOBJTYPE_FLAG, Team, sizeof(CNetObj_Flag));
			if (!pFlag)
				return;

			pFlag->m_X = (int)m_Pos.x;
			pFlag->m_Y = (int)m_Pos.y;
			pFlag->m_Team = Team;
		}

		if(SnappingClient != m_pPlayer->GetCID())
			return;
	}

	if(m_pPlayer->m_Invisible && SnappingClient != id && !GameServer()->Server()->IsAdmin(SnappingClient))
		return;

	if (SnappingClient > -1)
	{
		CCharacter* SnapChar = GameServer()->GetPlayerChar(SnappingClient);
		CPlayer* SnapPlayer = GameServer()->m_apPlayers[SnappingClient];

		if ((SnapPlayer->GetTeam() == TEAM_SPECTATORS || SnapPlayer->m_Paused) && SnapPlayer->m_SpectatorID != -1
			&& !CanCollide(SnapPlayer->m_SpectatorID) && !SnapPlayer->m_ShowOthers)
			return;

		if (SnapPlayer->GetTeam() != TEAM_SPECTATORS && !SnapPlayer->m_Paused && SnapChar && !SnapChar->m_Super
			&& !CanCollide(SnappingClient) && !SnapPlayer->m_ShowOthers)
			return;

		if ((SnapPlayer->GetTeam() == TEAM_SPECTATORS || SnapPlayer->m_Paused) && SnapPlayer->m_SpectatorID == -1
			&& !CanCollide(SnappingClient) && SnapPlayer->m_SpecTeam)
			return;
	}

	if (m_Paused)
		return;

	CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, id, sizeof(CNetObj_Character)));
	if (!pCharacter)
		return;

	// write down the m_Core
	if (!m_ReckoningTick || GameServer()->m_World.m_Paused)
	{
		// no dead reckoning when paused because the client doesn't know
		// how far to perform the reckoning
		pCharacter->m_Tick = 0;
		m_Core.Write(pCharacter);
	}
	else
	{
		pCharacter->m_Tick = m_ReckoningTick;
		m_SendCore.Write(pCharacter);
	}

	// set emote
	if (m_EmoteStop < Server()->Tick())
	{
		m_EmoteType = m_pPlayer->m_DefEmote;
		m_EmoteStop = -1;
	}
	pCharacter->m_Emote = m_EmoteType;

	if (pCharacter->m_HookedPlayer != -1)
	{
		if (!Server()->Translate(pCharacter->m_HookedPlayer, SnappingClient))
			pCharacter->m_HookedPlayer = -1;
	}

	pCharacter->m_AttackTick = m_AttackTick;
	pCharacter->m_Direction = m_Input.m_Direction;
	pCharacter->m_Weapon = m_Core.m_ActiveWeapon;
	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;

	// change eyes and use ninja graphic if player is freeze
	if (m_DeepFreeze)
	{
		if (pCharacter->m_Emote == EMOTE_NORMAL)
			pCharacter->m_Emote = EMOTE_PAIN;
		// pCharacter->m_Weapon = WEAPON_NINJA;
	}
	else if (m_FreezeTime > 0 || m_FreezeTime == -1)
	{
		if (pCharacter->m_Emote == EMOTE_NORMAL)
			pCharacter->m_Emote = EMOTE_BLINK;
		// pCharacter->m_Weapon = WEAPON_NINJA;
	}

	// jetpack and ninjajetpack prediction
	// if (m_pPlayer->GetCID() == SnappingClient)
	// {
	// 	if (m_Jetpack && pCharacter->m_Weapon != WEAPON_NINJA)
	// 	{
	// 		if (!(m_NeededFaketuning & FAKETUNE_JETPACK))
	// 		{
	// 			m_NeededFaketuning |= FAKETUNE_JETPACK;
	// 			GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);
	// 		}
	// 	}
	// 	else
	// 	{
	// 		if (m_NeededFaketuning & FAKETUNE_JETPACK)
	// 		{
	// 			m_NeededFaketuning &= ~FAKETUNE_JETPACK;
	// 			GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);
	// 		}
	// 	}
	// }

	// change eyes, use ninja graphic and set ammo count if player has ninjajetpack
	// if (m_pPlayer->m_NinjaJetpack && m_Jetpack && m_Core.m_ActiveWeapon == WEAPON_GUN && !m_DeepFreeze && !(m_FreezeTime > 0 || m_FreezeTime == -1))
	// {
	// 	if (pCharacter->m_Emote == EMOTE_NORMAL)
	// 		pCharacter->m_Emote = EMOTE_HAPPY;
	// 	pCharacter->m_Weapon = WEAPON_NINJA;
	// 	pCharacter->m_AmmoCount = 10;
	// }

	if (m_pPlayer->GetCID() == SnappingClient || SnappingClient == -1 ||
		(!g_Config.m_SvStrictSpectateMode && m_pPlayer->GetCID() == GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID))
	{
		pCharacter->m_Health = m_Health;
		pCharacter->m_Armor = m_Armor;
		if (m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo > 0)
			//pCharacter->m_AmmoCount = m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo;
			pCharacter->m_AmmoCount = (!m_FreezeTime) ? m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo : 0;
	}

	if (GetPlayer()->m_Afk || GetPlayer()->m_Paused)
		pCharacter->m_Emote = EMOTE_BLINK;

	if (pCharacter->m_Emote == EMOTE_NORMAL)
	{
		if (250 - ((Server()->Tick() - m_LastAction) % (250)) < 5)
			pCharacter->m_Emote = EMOTE_BLINK;
	}

	// if (m_pPlayer->m_Halloween)
	// {
	// 	if (1200 - ((Server()->Tick() - m_LastAction) % (1200)) < 5)
	// 	{
	// 		GameServer()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_GHOST);
	// 	}
	// }

	if(m_TempHasSword == true && m_Core.m_ActiveWeapon == WEAPON_GUN && isFreezed == false)
		pCharacter->m_Weapon = 6;

	pCharacter->m_PlayerFlags = GetPlayer()->m_PlayerFlags;

	pCharacter->m_PlayerFlags = GetPlayer()->m_PlayerFlags;

	CNetObj_DDNetCharacter *pDDNetCharacter = (CNetObj_DDNetCharacter *)Server()->SnapNewItem(32764, m_pPlayer->GetCID(), 40);
	if(!pDDNetCharacter)
		return;
	pDDNetCharacter->m_Flags = 0;
	if(Teams()->m_Core.GetSolo(id))
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_SOLO;
	// if(m_Core.m_Super)
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_SUPER;
	// if(m_Core.m_Invincible)
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_INVINCIBLE;
	// if(m_Core.m_EndlessHook)
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_ENDLESS_HOOK;
	// if(m_Core.m_CollisionDisabled || !GetTuning(m_TuneZone)->m_PlayerCollision)
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_COLLISION_DISABLED;
	// if(m_Core.m_HookHitDisabled || !GetTuning(m_TuneZone)->m_PlayerHooking)
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_HOOK_HIT_DISABLED;
	// if(m_Core.m_EndlessJump)
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_ENDLESS_JUMP;
	// if(m_Core.m_Jetpack)
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_JETPACK;
	// if(m_Core.m_HammerHitDisabled)
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_HAMMER_HIT_DISABLED;
	// if(m_Core.m_ShotgunHitDisabled)
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_SHOTGUN_HIT_DISABLED;
	// if(m_Core.m_GrenadeHitDisabled)
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_GRENADE_HIT_DISABLED;
	// if(m_Core.m_LaserHitDisabled)
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_LASER_HIT_DISABLED;
	// if(m_Core.m_HasTelegunGun)
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_TELEGUN_GUN;
	// if(m_Core.m_HasTelegunGrenade)
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_TELEGUN_GRENADE;
	// if(m_Core.m_HasTelegunLaser)
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_TELEGUN_LASER;
	if(m_aWeapons[WEAPON_HAMMER].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_HAMMER;
	if(m_aWeapons[WEAPON_GUN].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_GUN;
	if(m_aWeapons[WEAPON_SHOTGUN].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_SHOTGUN;
	if(m_aWeapons[WEAPON_GRENADE].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_GRENADE;
	if(m_aWeapons[WEAPON_RIFLE].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_LASER;
	if(GetActiveWeapon() == WEAPON_NINJA)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_NINJA;
	// if(m_LiveFrozen)
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_MOVEMENTS_DISABLED;
	// TO FIXX
	pDDNetCharacter->m_FreezeEnd = m_FreezeTime == 0 ? 0 : Server()->Tick() + m_FreezeTime;
	pDDNetCharacter->m_Jumps = m_Core.m_Jumps;
	// pDDNetCharacter->m_TeleCheckpoint = m_TeleCheckpoint;
	// pDDNetCharacter->m_StrongWeakId = m_StrongWeakId;

	// Display Information
	pDDNetCharacter->m_JumpedTotal = m_Core.m_JumpedTotal;
	// pDDNetCharacter->m_NinjaActivationTick = m_Core.m_Ninja.m_ActivationTick;
	pDDNetCharacter->m_FreezeStart = m_Core.m_FreezeStart;
	// if(m_Core.m_IsInFreeze)
	// {
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_IN_FREEZE;
	// }
	// if(Teams()->IsPractice(Team()))
	// {
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_PRACTICE_MODE;
	// }
	if(Teams()->TeamLocked(Team()))
	{
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_LOCK_MODE;
	}
	// if(Teams()->TeamFlock(Team()))
	// {
	// 	pDDNetCharacter->m_Flags |= CHARACTERFLAG_TEAM0_MODE;
	// }
	pDDNetCharacter->m_TargetX = m_Core.m_Input.m_TargetX;
	pDDNetCharacter->m_TargetY = m_Core.m_Input.m_TargetY;

	// -1 is the default value, SnapNewItem zeroes the object, so it would incorrectly become 0
	// pDDNetCharacter->m_TuneZoneOverride = -1;
	// if(g_Config.m_SvShowAim == id)
	// {
	// 	if(s_ShowAimID == -1)
	// 		s_ShowAimID = Server()->SnapNewID();

	// 	CPlayer *pSnappingPlayer = GameServer()->m_apPlayers[SnappingClient];
	// 	if(pSnappingPlayer != NULL && pSnappingPlayer->m_Authed)
	// 	{
	// 		vec2 MousePos = m_Pos + vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY);
	// 		CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, s_ShowAimID, sizeof(CNetObj_Laser)));
	// 		if(pObj)
	// 		{
	// 			pObj->m_X = (int)MousePos.x;
	// 			pObj->m_Y = (int)MousePos.y;
	// 			pObj->m_FromX = (int)MousePos.x;
	// 			pObj->m_FromY = (int)MousePos.y;
	// 			pObj->m_StartTick = Server()->Tick() -1;
	// 		}
	// 	}
	// }

	if (GameServer()->m_KOHActive)
	{
		//calculate visible balls
		for (unsigned z = 0; z < GameServer()->m_KOH.size(); z++)
		{
			float Panso = 1.0f;
			Panso *= m_AnimIDNum;

			int MaxBalls = round_to_int(Panso);
			for (int i = 0; i < MaxBalls; i++)
			{
				CNetObj_Projectile *pFirstParticle = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_apAnimIDs[i], sizeof(CNetObj_Projectile)));
				if (pFirstParticle && m_apAnimIDs[i] != -1)
				{
					float rad = g_Config.m_SvKOHCircleRadius;
					float TurnFac = 0.025f;

					float PosX = GameServer()->m_KOH[z].m_Center.x * 32 + cosf(2 * pi * (i / (float)m_AnimIDNum) + Server()->Tick()*TurnFac) * rad;
					float PosY = GameServer()->m_KOH[z].m_Center.y * 32 + sinf(2 * pi * (i / (float)m_AnimIDNum) + Server()->Tick()*TurnFac) * rad;

					pFirstParticle->m_X = round_to_int(PosX);
					pFirstParticle->m_Y = round_to_int(PosY);
					pFirstParticle->m_VelX = 4;
					pFirstParticle->m_VelY = 4;
					pFirstParticle->m_StartTick = Server()->Tick() - 4;
					pFirstParticle->m_Type = g_Config.m_SvKOHCircleType;
				}
			}
		}
	}
}

int CCharacter::NetworkClipped(int SnappingClient)
{
	return NetworkClipped(SnappingClient, m_Pos);
}

int CCharacter::NetworkClipped(int SnappingClient, vec2 CheckPos)
{
	if (SnappingClient == -1 || GameServer()->m_apPlayers[SnappingClient]->m_ShowAll)
		return 0;

	float dx = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.x - CheckPos.x;
	float dy = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.y - CheckPos.y;

	if (absolute(dx) > 1000.0f || absolute(dy) > 800.0f)
		return 1;

	if (distance(GameServer()->m_apPlayers[SnappingClient]->m_ViewPos, CheckPos) > 4000.0f)
		return 1;
	return 0;
}

// DDRace

bool CCharacter::CanCollide(int ClientID)
{
	return Teams()->m_Core.CanCollide(GetPlayer()->GetCID(), ClientID);
}
bool CCharacter::SameTeam(int ClientID)
{
	return Teams()->m_Core.SameTeam(GetPlayer()->GetCID(), ClientID);
}

int CCharacter::Team()
{
	return Teams()->m_Core.Team(m_pPlayer->GetCID());
}

CGameTeams* CCharacter::Teams()
{
	return &((CGameControllerDDRace*)GameServer()->m_pController)->m_Teams;
}

void CCharacter::HandleBroadcast()
{
	// CPlayerData *pData = GameServer()->Score()->PlayerData(m_pPlayer->GetCID());

	// if (m_DDRaceState == DDRACE_STARTED && m_CpLastBroadcast != m_CpActive &&
	// 	m_CpActive > -1 && m_CpTick > Server()->Tick() && m_pPlayer->m_ClientVersion == VERSION_VANILLA &&
	// 	pData->m_BestTime && pData->m_aBestCpTime[m_CpActive] != 0)
	// {
	// 	char aBroadcast[128];
	// 	float Diff = m_CpCurrent[m_CpActive] - pData->m_aBestCpTime[m_CpActive];
	// 	str_format(aBroadcast, sizeof(aBroadcast), "Checkpoint | Diff : %+5.2f", Diff);
	// 	GameServer()->SendBroadcast(aBroadcast, m_pPlayer->GetCID());
	// 	m_CpLastBroadcast = m_CpActive;
	// 	m_LastBroadcast = Server()->Tick();
	// }
	// else if ((m_pPlayer->m_TimerType == 1 || m_pPlayer->m_TimerType == 2) && m_DDRaceState == DDRACE_STARTED && m_LastBroadcast + Server()->TickSpeed() * g_Config.m_SvTimeInBroadcastInterval <= Server()->Tick())
	// {
	// 	char aBuftime[64];
	// 	int IntTime = (int)((float)(Server()->Tick() - m_StartTime) / ((float)Server()->TickSpeed()));
	// 	str_format(aBuftime, sizeof(aBuftime), "%s%d:%s%d", ((IntTime / 60) > 9) ? "" : "0", IntTime / 60, ((IntTime % 60) > 9) ? "" : "0", IntTime % 60);
	// 	GameServer()->SendBroadcast(aBuftime, m_pPlayer->GetCID());
	// 	m_CpLastBroadcast = m_CpActive;
	// 	m_LastBroadcast = Server()->Tick();
	// }
}

void CCharacter::HandleSkippableTiles(int Index)
{
	// handle death-tiles and leaving gamelayer
	if ((GameServer()->Collision()->GetCollisionAt(m_Pos.x + m_ProximityRadius / 3.f, m_Pos.y - m_ProximityRadius / 3.f) == TILE_DEATH ||
		GameServer()->Collision()->GetCollisionAt(m_Pos.x + m_ProximityRadius / 3.f, m_Pos.y + m_ProximityRadius / 3.f) == TILE_DEATH ||
		GameServer()->Collision()->GetCollisionAt(m_Pos.x - m_ProximityRadius / 3.f, m_Pos.y - m_ProximityRadius / 3.f) == TILE_DEATH ||
		GameServer()->Collision()->GetFCollisionAt(m_Pos.x + m_ProximityRadius / 3.f, m_Pos.y - m_ProximityRadius / 3.f) == TILE_DEATH ||
		GameServer()->Collision()->GetFCollisionAt(m_Pos.x + m_ProximityRadius / 3.f, m_Pos.y + m_ProximityRadius / 3.f) == TILE_DEATH ||
		GameServer()->Collision()->GetFCollisionAt(m_Pos.x - m_ProximityRadius / 3.f, m_Pos.y - m_ProximityRadius / 3.f) == TILE_DEATH ||
		GameServer()->Collision()->GetCollisionAt(m_Pos.x - m_ProximityRadius / 3.f, m_Pos.y + m_ProximityRadius / 3.f) == TILE_DEATH) &&
		!m_Super && !(Team() && Teams()->TeeFinished(m_pPlayer->GetCID())))
	{
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
		return;
	}

	if (GameLayerClipped(m_Pos))
	{
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
		return;
	}

	if (Index < 0)
		return;

	// handle speedup tiles
	if (GameServer()->Collision()->IsSpeedup(Index))
	{
		vec2 Direction, MaxVel, TempVel = m_Core.m_Vel;
		int Force, MaxSpeed = 0;
		float TeeAngle, SpeederAngle, DiffAngle, SpeedLeft, TeeSpeed;
		GameServer()->Collision()->GetSpeedup(Index, &Direction, &Force, &MaxSpeed);
		if (Force == 255 && MaxSpeed)
		{
			m_Core.m_Vel = Direction * (MaxSpeed / 5);
		}
		else
		{
			if (MaxSpeed > 0 && MaxSpeed < 5) MaxSpeed = 5;
			if (MaxSpeed > 0)
			{
				if (Direction.x > 0.0000001f)
					SpeederAngle = -atan(Direction.y / Direction.x);
				else if (Direction.x < 0.0000001f)
					SpeederAngle = atan(Direction.y / Direction.x) + 2.0f * asin(1.0f);
				else if (Direction.y > 0.0000001f)
					SpeederAngle = asin(1.0f);
				else
					SpeederAngle = asin(-1.0f);

				if (SpeederAngle < 0)
					SpeederAngle = 4.0f * asin(1.0f) + SpeederAngle;

				if (TempVel.x > 0.0000001f)
					TeeAngle = -atan(TempVel.y / TempVel.x);
				else if (TempVel.x < 0.0000001f)
					TeeAngle = atan(TempVel.y / TempVel.x) + 2.0f * asin(1.0f);
				else if (TempVel.y > 0.0000001f)
					TeeAngle = asin(1.0f);
				else
					TeeAngle = asin(-1.0f);

				if (TeeAngle < 0)
					TeeAngle = 4.0f * asin(1.0f) + TeeAngle;

				TeeSpeed = sqrt(pow(TempVel.x, 2) + pow(TempVel.y, 2));

				DiffAngle = SpeederAngle - TeeAngle;
				SpeedLeft = MaxSpeed / 5.0f - cos(DiffAngle) * TeeSpeed;
				if (abs((int)SpeedLeft) > Force && SpeedLeft > 0.0000001f)
					TempVel += Direction * Force;
				else if (abs((int)SpeedLeft) > Force)
					TempVel += Direction * -Force;
				else
					TempVel += Direction * SpeedLeft;
			}
			else
				TempVel += Direction * Force;

			if (TempVel.x > 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_270) || (m_TileIndexL == TILE_STOP && m_TileFlagsL == ROTATION_270) || (m_TileIndexL == TILE_STOPS && (m_TileFlagsL == ROTATION_90 || m_TileFlagsL == ROTATION_270)) || (m_TileIndexL == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_270) || (m_TileFIndexL == TILE_STOP && m_TileFFlagsL == ROTATION_270) || (m_TileFIndexL == TILE_STOPS && (m_TileFFlagsL == ROTATION_90 || m_TileFFlagsL == ROTATION_270)) || (m_TileFIndexL == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_270) || (m_TileSIndexL == TILE_STOP && m_TileSFlagsL == ROTATION_270) || (m_TileSIndexL == TILE_STOPS && (m_TileSFlagsL == ROTATION_90 || m_TileSFlagsL == ROTATION_270)) || (m_TileSIndexL == TILE_STOPA)))
				TempVel.x = 0;
			if (TempVel.x < 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_90) || (m_TileIndexR == TILE_STOP && m_TileFlagsR == ROTATION_90) || (m_TileIndexR == TILE_STOPS && (m_TileFlagsR == ROTATION_90 || m_TileFlagsR == ROTATION_270)) || (m_TileIndexR == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_90) || (m_TileFIndexR == TILE_STOP && m_TileFFlagsR == ROTATION_90) || (m_TileFIndexR == TILE_STOPS && (m_TileFFlagsR == ROTATION_90 || m_TileFFlagsR == ROTATION_270)) || (m_TileFIndexR == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_90) || (m_TileSIndexR == TILE_STOP && m_TileSFlagsR == ROTATION_90) || (m_TileSIndexR == TILE_STOPS && (m_TileSFlagsR == ROTATION_90 || m_TileSFlagsR == ROTATION_270)) || (m_TileSIndexR == TILE_STOPA)))
				TempVel.x = 0;
			if (TempVel.y < 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_180) || (m_TileIndexB == TILE_STOP && m_TileFlagsB == ROTATION_180) || (m_TileIndexB == TILE_STOPS && (m_TileFlagsB == ROTATION_0 || m_TileFlagsB == ROTATION_180)) || (m_TileIndexB == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_180) || (m_TileFIndexB == TILE_STOP && m_TileFFlagsB == ROTATION_180) || (m_TileFIndexB == TILE_STOPS && (m_TileFFlagsB == ROTATION_0 || m_TileFFlagsB == ROTATION_180)) || (m_TileFIndexB == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_180) || (m_TileSIndexB == TILE_STOP && m_TileSFlagsB == ROTATION_180) || (m_TileSIndexB == TILE_STOPS && (m_TileSFlagsB == ROTATION_0 || m_TileSFlagsB == ROTATION_180)) || (m_TileSIndexB == TILE_STOPA)))
				TempVel.y = 0;
			if (TempVel.y > 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_0) || (m_TileIndexT == TILE_STOP && m_TileFlagsT == ROTATION_0) || (m_TileIndexT == TILE_STOPS && (m_TileFlagsT == ROTATION_0 || m_TileFlagsT == ROTATION_180)) || (m_TileIndexT == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_0) || (m_TileFIndexT == TILE_STOP && m_TileFFlagsT == ROTATION_0) || (m_TileFIndexT == TILE_STOPS && (m_TileFFlagsT == ROTATION_0 || m_TileFFlagsT == ROTATION_180)) || (m_TileFIndexT == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_0) || (m_TileSIndexT == TILE_STOP && m_TileSFlagsT == ROTATION_0) || (m_TileSIndexT == TILE_STOPS && (m_TileSFlagsT == ROTATION_0 || m_TileSFlagsT == ROTATION_180)) || (m_TileSIndexT == TILE_STOPA)))
				TempVel.y = 0;
			m_Core.m_Vel = TempVel;
		}
	}
}

void CCharacter::HandleTiles(int Index)
{
	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
	int MapIndex = Index;
	//int PureMapIndex = GameServer()->Collision()->GetPureMapIndex(m_Pos);
	float Offset = 4.0f;
	int MapIndexL = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x + (m_ProximityRadius / 2) + Offset, m_Pos.y));
	int MapIndexR = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x - (m_ProximityRadius / 2) - Offset, m_Pos.y));
	int MapIndexT = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x, m_Pos.y + (m_ProximityRadius / 2) + Offset));
	int MapIndexB = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x, m_Pos.y - (m_ProximityRadius / 2) - Offset));
	m_TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	m_TileFlags = GameServer()->Collision()->GetTileFlags(MapIndex);
	m_TileIndexL = GameServer()->Collision()->GetTileIndex(MapIndexL);
	m_TileFlagsL = GameServer()->Collision()->GetTileFlags(MapIndexL);
	m_TileIndexR = GameServer()->Collision()->GetTileIndex(MapIndexR);
	m_TileFlagsR = GameServer()->Collision()->GetTileFlags(MapIndexR);
	m_TileIndexB = GameServer()->Collision()->GetTileIndex(MapIndexB);
	m_TileFlagsB = GameServer()->Collision()->GetTileFlags(MapIndexB);
	m_TileIndexT = GameServer()->Collision()->GetTileIndex(MapIndexT);
	m_TileFlagsT = GameServer()->Collision()->GetTileFlags(MapIndexT);
	m_TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);
	m_TileFFlags = GameServer()->Collision()->GetFTileFlags(MapIndex);
	m_TileFIndexL = GameServer()->Collision()->GetFTileIndex(MapIndexL);
	m_TileFFlagsL = GameServer()->Collision()->GetFTileFlags(MapIndexL);
	m_TileFIndexR = GameServer()->Collision()->GetFTileIndex(MapIndexR);
	m_TileFFlagsR = GameServer()->Collision()->GetFTileFlags(MapIndexR);
	m_TileFIndexB = GameServer()->Collision()->GetFTileIndex(MapIndexB);
	m_TileFFlagsB = GameServer()->Collision()->GetFTileFlags(MapIndexB);
	m_TileFIndexT = GameServer()->Collision()->GetFTileIndex(MapIndexT);
	m_TileFFlagsT = GameServer()->Collision()->GetFTileFlags(MapIndexT);//
	m_TileSIndex = (GameServer()->Collision()->m_pSwitchers && GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetDTileNumber(MapIndex)].m_Status[Team()]) ? (Team() != TEAM_SUPER) ? GameServer()->Collision()->GetDTileIndex(MapIndex) : 0 : 0;
	m_TileSFlags = (GameServer()->Collision()->m_pSwitchers && GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetDTileNumber(MapIndex)].m_Status[Team()]) ? (Team() != TEAM_SUPER) ? GameServer()->Collision()->GetDTileFlags(MapIndex) : 0 : 0;
	m_TileSIndexL = (GameServer()->Collision()->m_pSwitchers && GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetDTileNumber(MapIndexL)].m_Status[Team()]) ? (Team() != TEAM_SUPER) ? GameServer()->Collision()->GetDTileIndex(MapIndexL) : 0 : 0;
	m_TileSFlagsL = (GameServer()->Collision()->m_pSwitchers && GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetDTileNumber(MapIndexL)].m_Status[Team()]) ? (Team() != TEAM_SUPER) ? GameServer()->Collision()->GetDTileFlags(MapIndexL) : 0 : 0;
	m_TileSIndexR = (GameServer()->Collision()->m_pSwitchers && GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetDTileNumber(MapIndexR)].m_Status[Team()]) ? (Team() != TEAM_SUPER) ? GameServer()->Collision()->GetDTileIndex(MapIndexR) : 0 : 0;
	m_TileSFlagsR = (GameServer()->Collision()->m_pSwitchers && GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetDTileNumber(MapIndexR)].m_Status[Team()]) ? (Team() != TEAM_SUPER) ? GameServer()->Collision()->GetDTileFlags(MapIndexR) : 0 : 0;
	m_TileSIndexB = (GameServer()->Collision()->m_pSwitchers && GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetDTileNumber(MapIndexB)].m_Status[Team()]) ? (Team() != TEAM_SUPER) ? GameServer()->Collision()->GetDTileIndex(MapIndexB) : 0 : 0;
	m_TileSFlagsB = (GameServer()->Collision()->m_pSwitchers && GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetDTileNumber(MapIndexB)].m_Status[Team()]) ? (Team() != TEAM_SUPER) ? GameServer()->Collision()->GetDTileFlags(MapIndexB) : 0 : 0;
	m_TileSIndexT = (GameServer()->Collision()->m_pSwitchers && GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetDTileNumber(MapIndexT)].m_Status[Team()]) ? (Team() != TEAM_SUPER) ? GameServer()->Collision()->GetDTileIndex(MapIndexT) : 0 : 0;
	m_TileSFlagsT = (GameServer()->Collision()->m_pSwitchers && GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetDTileNumber(MapIndexT)].m_Status[Team()]) ? (Team() != TEAM_SUPER) ? GameServer()->Collision()->GetDTileFlags(MapIndexT) : 0 : 0;
	//Sensitivity
	int S1 = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x + m_ProximityRadius / 3.f, m_Pos.y - m_ProximityRadius / 3.f));
	int S2 = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x + m_ProximityRadius / 3.f, m_Pos.y + m_ProximityRadius / 3.f));
	int S3 = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x - m_ProximityRadius / 3.f, m_Pos.y - m_ProximityRadius / 3.f));
	int S4 = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x - m_ProximityRadius / 3.f, m_Pos.y + m_ProximityRadius / 3.f));
	int Tile1 = GameServer()->Collision()->GetTileIndex(S1);
	int Tile2 = GameServer()->Collision()->GetTileIndex(S2);
	int Tile3 = GameServer()->Collision()->GetTileIndex(S3);
	int Tile4 = GameServer()->Collision()->GetTileIndex(S4);
	int FTile1 = GameServer()->Collision()->GetFTileIndex(S1);
	int FTile2 = GameServer()->Collision()->GetFTileIndex(S2);
	int FTile3 = GameServer()->Collision()->GetFTileIndex(S3);
	int FTile4 = GameServer()->Collision()->GetFTileIndex(S4);

	// Fix tiles when going inside
	if(((m_TileIndex != TILE_HEAVYHAMMER) && (m_TileFIndex != TILE_HEAVYHAMMER)) && WasInHH)
		WasInHH = false;
	else if(((m_TileIndex != TILE_EPICCIRCLES) && (m_TileFIndex != TILE_EPICCIRCLES)) && WasInCircles)
		WasInCircles = false;
	else if(((m_TileIndex != TILE_XXL) && (m_TileFIndex != TILE_XXL)) && WasInXXL)
		WasInXXL = false;
	else if(((m_TileIndex != TILE_BLOODY) && (m_TileFIndex != TILE_BLOODY)) && WasInBloody)
		WasInBloody = false;
	else if(((m_TileIndex != TILE_STEAMY) && (m_TileFIndex != TILE_STEAMY)) && WasInSteam)
		WasInSteam = false;
	else if(((m_TileIndex != TILE_RAINBOW) && (m_TileFIndex != TILE_RAINBOW)) && WasInRainbow)
		WasInRainbow = false;

	if (Index < 0)
	{
		m_LastRefillJumps = false;
		m_LastPenalty = false;
		m_LastBonus = false;
		return;
	}
	int cp = GameServer()->Collision()->IsCheckpoint(MapIndex);
	if (cp != -1 && m_DDRaceState == DDRACE_STARTED && cp > m_CpActive)
	{
		m_CpActive = cp;
		m_CpCurrent[cp] = m_Time;
		m_CpTick = Server()->Tick() + Server()->TickSpeed() * 2;
		if (m_pPlayer->m_ClientVersion >= VERSION_DDRACE) {
			CPlayerData *pData = GameServer()->Score()->PlayerData(m_pPlayer->GetCID());
			CNetMsg_Sv_DDRaceTime Msg;
			Msg.m_Time = (int)m_Time;
			Msg.m_Check = 0;
			Msg.m_Finish = 0;

			if (m_CpActive != -1 && m_CpTick > Server()->Tick())
			{
				if (pData->m_BestTime && pData->m_aBestCpTime[m_CpActive] != 0)
				{
					float Diff = (m_CpCurrent[m_CpActive] - pData->m_aBestCpTime[m_CpActive]) * 100;
					Msg.m_Check = (int)Diff;
				}
			}

			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, m_pPlayer->GetCID());
		}
	}
	int cpf = GameServer()->Collision()->IsFCheckpoint(MapIndex);
	if (cpf != -1 && m_DDRaceState == DDRACE_STARTED && cpf > m_CpActive)
	{
		m_CpActive = cpf;
		m_CpCurrent[cpf] = m_Time;
		m_CpTick = Server()->Tick() + Server()->TickSpeed() * 2;
		if (m_pPlayer->m_ClientVersion >= VERSION_DDRACE) {
			CPlayerData *pData = GameServer()->Score()->PlayerData(m_pPlayer->GetCID());
			CNetMsg_Sv_DDRaceTime Msg;
			Msg.m_Time = (int)m_Time;
			Msg.m_Check = 0;
			Msg.m_Finish = 0;

			if (m_CpActive != -1 && m_CpTick > Server()->Tick())
			{
				if (pData->m_BestTime && pData->m_aBestCpTime[m_CpActive] != 0)
				{
					float Diff = (m_CpCurrent[m_CpActive] - pData->m_aBestCpTime[m_CpActive]) * 100;
					Msg.m_Check = (int)Diff;
				}
			}

			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, m_pPlayer->GetCID());
		}
	}
	int tcp = GameServer()->Collision()->IsTCheckpoint(MapIndex);
	if (tcp)
		m_TeleCheckpoint = tcp;

	// start
	if (((m_TileIndex == TILE_BEGIN) || (m_TileFIndex == TILE_BEGIN) || FTile1 == TILE_BEGIN || FTile2 == TILE_BEGIN || FTile3 == TILE_BEGIN || FTile4 == TILE_BEGIN || Tile1 == TILE_BEGIN || Tile2 == TILE_BEGIN || Tile3 == TILE_BEGIN || Tile4 == TILE_BEGIN) && (m_DDRaceState == DDRACE_NONE || m_DDRaceState == DDRACE_FINISHED || (m_DDRaceState == DDRACE_STARTED && !Team() && g_Config.m_SvTeam != 3)))
	{
		bool CanBegin = true;
		if (g_Config.m_SvResetPickups)
		{
			for (int i = WEAPON_SHOTGUN; i < NUM_WEAPONS; ++i)
			{
				m_aWeapons[i].m_Got = false;
				if (m_Core.m_ActiveWeapon == i)
					m_Core.m_ActiveWeapon = WEAPON_GUN;
			}
		}
		if (g_Config.m_SvTeam == 2 && (Team() == TEAM_FLOCK || Teams()->Count(Team()) <= 1))
		{
			if (m_LastStartWarning < Server()->Tick() - 3 * Server()->TickSpeed())
			{
				GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Server admin requires you to be in a team and with other tees to start");
				m_LastStartWarning = Server()->Tick();
			}
			Die(GetPlayer()->GetCID(), WEAPON_WORLD);
			CanBegin = false;
		}
		if (CanBegin)
		{
			Teams()->OnCharacterStart(m_pPlayer->GetCID());
			m_CpActive = -2;
		}
		else {

		}


	}

	// finish
	if (((m_TileIndex == TILE_END) || (m_TileFIndex == TILE_END) || FTile1 == TILE_END || FTile2 == TILE_END || FTile3 == TILE_END || FTile4 == TILE_END || Tile1 == TILE_END || Tile2 == TILE_END || Tile3 == TILE_END || Tile4 == TILE_END) && m_DDRaceState == DDRACE_STARTED)
		Controller->m_Teams.OnCharacterFinish(m_pPlayer->GetCID());

	// freeze
	if (((m_TileIndex == TILE_FREEZE) || (m_TileFIndex == TILE_FREEZE)) && !m_Super && !m_DeepFreeze)
		Freeze();
	else if (((m_TileIndex == TILE_UNFREEZE) || (m_TileFIndex == TILE_UNFREEZE)) && !m_DeepFreeze)
		UnFreeze();

	// deep freeze
	if (((m_TileIndex == TILE_DFREEZE) || (m_TileFIndex == TILE_DFREEZE)) && !m_Super && !m_DeepFreeze)
		m_DeepFreeze = true;
	else if (((m_TileIndex == TILE_DUNFREEZE) || (m_TileFIndex == TILE_DUNFREEZE)) && !m_Super && m_DeepFreeze)
		m_DeepFreeze = false;

	// endless hook
	if (((m_TileIndex == TILE_EHOOK_START) || (m_TileFIndex == TILE_EHOOK_START)) && !m_EndlessHook)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Endless hook has been activated");
		m_EndlessHook = true;
	}
	else if (((m_TileIndex == TILE_EHOOK_END) || (m_TileFIndex == TILE_EHOOK_END)) && m_EndlessHook)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Endless hook has been deactivated");
		m_EndlessHook = false;
	}

	// hit others
	if (((m_TileIndex == TILE_HIT_END) || (m_TileFIndex == TILE_HIT_END)) && m_Hit != (DISABLE_HIT_GRENADE | DISABLE_HIT_HAMMER | DISABLE_HIT_RIFLE | DISABLE_HIT_SHOTGUN))
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't hit others");
		HandleHit(false);
	}
	else if (((m_TileIndex == TILE_HIT_START) || (m_TileFIndex == TILE_HIT_START)) && m_Hit != HIT_ALL)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can hit others");
		HandleHit(true);
	}

	// collide with others
	if (((m_TileIndex == TILE_NPC_END) || (m_TileFIndex == TILE_NPC_END)) && m_Core.m_Collision)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't collide with others");
		HandleCollision(false); // update tunings
	}
	else if (((m_TileIndex == TILE_NPC_START) || (m_TileFIndex == TILE_NPC_START)) && !m_Core.m_Collision)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can collide with others");
		HandleCollision(true); // update tunings
	}

	// hook others
	if (((m_TileIndex == TILE_NPH_END) || (m_TileFIndex == TILE_NPH_END)) && m_Core.m_Hook)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't hook others");
		HandleHook(false);
	}
	else if (((m_TileIndex == TILE_NPH_START) || (m_TileFIndex == TILE_NPH_START)) && !m_Core.m_Hook)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can hook others");
		HandleHook(true);
	}

	// unlimited air jumps
	if (((m_TileIndex == TILE_SUPER_START) || (m_TileFIndex == TILE_SUPER_START)) && !m_SuperJump)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You have unlimited air jumps");
		m_SuperJump = true;
		if (m_Core.m_Jumps == 0)
		{
			m_NeededFaketuning &= ~FAKETUNE_NOJUMP;
			GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
		}
	}
	else if (((m_TileIndex == TILE_SUPER_END) || (m_TileFIndex == TILE_SUPER_END)) && m_SuperJump)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You don't have unlimited air jumps");
		m_SuperJump = false;
		if (m_Core.m_Jumps == 0)
		{
			m_NeededFaketuning |= FAKETUNE_NOJUMP;
			GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
		}
	}

	// walljump
	if ((m_TileIndex == TILE_WALLJUMP) || (m_TileFIndex == TILE_WALLJUMP))
	{
		if (m_Core.m_Vel.y > 0 && m_Core.m_Colliding && m_Core.m_LeftWall)
		{
			m_Core.m_LeftWall = false;
			m_Core.m_JumpedTotal = m_Core.m_Jumps - 1;
			m_Core.m_Jumped = 1;
		}
	}

	// jetpack gun
	if (((m_TileIndex == TILE_JETPACK_START) || (m_TileFIndex == TILE_JETPACK_START)) && !m_Jetpack)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You have a jetpack gun");
		m_Jetpack = true;
	}
	else if (((m_TileIndex == TILE_JETPACK_END) || (m_TileFIndex == TILE_JETPACK_END)) && m_Jetpack)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You lost your jetpack gun");
		m_Jetpack = false;
	}

	// unlock team
	else if (((m_TileIndex == TILE_UNLOCK_TEAM) || (m_TileFIndex == TILE_UNLOCK_TEAM)) && Teams()->TeamLocked(Team()))
	{
		Teams()->SetTeamLock(Team(), false);

		for (int i = 0; i < MAX_CLIENTS; i++)
			if (Teams()->m_Core.Team(i) == Team())
				GameServer()->SendChatTarget(i, "Your team was unlocked by an unlock team tile");
	}

	// King of the hill !
	if (GameServer()->m_KOHActive && Team() == 0)
	{
		for (unsigned z = 0; z < GameServer()->m_KOH.size(); z++)
		{
			CGameContext::CKOH *pKOH = &(GameServer()->m_KOH[z]);

			// only handle zones that we are in
			if (!(GetPlayer()->m_Koh.m_InZones & (1 << z)))
				continue;

			// Check if we are alone in the zone
			if (pKOH->m_NumContestants == 1)
			{
				char aGaining[256];
				m_pPlayer->m_Koh.m_ZoneXp++;
				str_format(aGaining, sizeof(aGaining), "King of the hill -- ZONE %i\n%s in control - %d/%d points [%i%%]", z,
					Server()->ClientName(m_Core.m_Id),
					m_pPlayer->m_Koh.m_ZonePoints, g_Config.m_SvKOHRequiredPoints,
					round_to_int(((float)m_pPlayer->m_Koh.m_ZoneXp / (float)g_Config.m_SvKOHCaptureXpLimit)*100.0f));
				GameServer()->SendBroadcast(aGaining, -1);
			}
			if (m_pPlayer->m_Koh.m_ZoneXp >= g_Config.m_SvKOHCaptureXpLimit)
			{
				m_pPlayer->m_Koh.m_ZonePoints++;
				m_pPlayer->m_Koh.m_ZoneXp = 0;
			}
			if (m_pPlayer->m_Koh.m_ZonePoints >= g_Config.m_SvKOHRequiredPoints)
			{
				char aWinner[256];
				if (!m_pPlayer->m_AccData.m_UserID)
					str_format(aWinner, sizeof(aWinner), "%s has won and dominated the hill %i! (login to get rewards!)", Server()->ClientName(m_Core.m_Id), z);
				else
				{
					str_format(aWinner, sizeof(aWinner), "%s has won and dominated the hill %i! Reward: 3 Pages", Server()->ClientName(m_Core.m_Id), z);
					m_pPlayer->m_QuestData.m_Pages += 3;
				}
				GameServer()->SendBroadcast(aWinner, -1);
				GameServer()->SendChatTarget(-1, aWinner);
				GameServer()->m_KOHActive = false;
			}
		}

	}

	// passive
	if (g_Config.m_SvWbProt != 0)
	{
		if (g_Config.m_SvWbProt == 1 || (g_Config.m_SvWbProt == 2 && (Server()->IsAdmin(m_pPlayer->GetCID()) || m_pPlayer->m_AccData.m_Vip || m_pPlayer->Temporary.m_PassiveMode)))
		{
			if ((m_TileIndex == TILE_PASSIVE_IN || m_TileFIndex == TILE_PASSIVE_IN) && !m_PassiveMode)
			{
				if(!m_pPlayer->m_Passive)
					return;

				GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Passive mode enabled!");
				m_ThreeSecondRule = false;
				m_PassiveMode = true;
				new CPassiveIndicator(&GameServer()->m_World, m_Pos, m_pPlayer->GetCID());
			}
			else if ((m_TileIndex == TILE_PASSIVE_OUT || m_TileFIndex == TILE_PASSIVE_OUT) && m_PassiveMode && !m_TilePauser)
			{
				m_LastPassiveOut = Server()->Tick();
				m_ThreeSecondRule = true;
				GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Passive mode disabling in three seconds!");
				m_TilePauser = true;
			}
		}
	}

	// admin
	if (m_TileIndex == TILE_ADMIN || m_TileFIndex == TILE_ADMIN)
	{
		if (!GameServer()->Server()->IsAdmin(GetPlayer()->GetCID()))
		{
			GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You are not an admin!");
			Die(GetPlayer()->GetCID(), WEAPON_WORLD);
		}
	}

	// Vip
	if ((m_TileIndex == TILE_VIP || m_TileFIndex == TILE_VIP) &&
		(!m_pPlayer->m_AccData.m_Vip && !Server()->IsAdmin(m_pPlayer->GetCID())))
	{
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You are not a vip!");
		return;
	}

	// solo part
	if ((m_TileIndex == TILE_SOLO_START || m_TileFIndex == TILE_SOLO_START) && !Teams()->m_Core.GetSolo(m_pPlayer->GetCID()))
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You are now in a solo part");
		HandleSolo(true);
	}
	else if ((m_TileIndex == TILE_SOLO_END || m_TileFIndex == TILE_SOLO_END) && Teams()->m_Core.GetSolo(m_pPlayer->GetCID()))
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You are now out of the solo part");
		HandleSolo(false);
	}

	// refill jumps
	if ((m_TileIndex == TILE_REFILL_JUMPS || m_TileFIndex == TILE_REFILL_JUMPS) && !m_LastRefillJumps)
	{
		m_Core.m_JumpedTotal = 0;
		m_Core.m_Jumped = 0;
		m_LastRefillJumps = true;
	}
	if (m_TileIndex != TILE_REFILL_JUMPS && m_TileFIndex != TILE_REFILL_JUMPS)
	{
		m_LastRefillJumps = false;
	}

	// stopper
	if (((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_270) || (m_TileIndexL == TILE_STOP && m_TileFlagsL == ROTATION_270) || (m_TileIndexL == TILE_STOPS && (m_TileFlagsL == ROTATION_90 || m_TileFlagsL == ROTATION_270)) || (m_TileIndexL == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_270) || (m_TileFIndexL == TILE_STOP && m_TileFFlagsL == ROTATION_270) || (m_TileFIndexL == TILE_STOPS && (m_TileFFlagsL == ROTATION_90 || m_TileFFlagsL == ROTATION_270)) || (m_TileFIndexL == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_270) || (m_TileSIndexL == TILE_STOP && m_TileSFlagsL == ROTATION_270) || (m_TileSIndexL == TILE_STOPS && (m_TileSFlagsL == ROTATION_90 || m_TileSFlagsL == ROTATION_270)) || (m_TileSIndexL == TILE_STOPA)) && m_Core.m_Vel.x > 0)
	{
		if ((int)GameServer()->Collision()->GetPos(MapIndexL).x)
			if ((int)GameServer()->Collision()->GetPos(MapIndexL).x < (int)m_Core.m_Pos.x)
				m_Core.m_Pos = m_PrevPos;
		m_Core.m_Vel.x = 0;
	}
	if (((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_90) || (m_TileIndexR == TILE_STOP && m_TileFlagsR == ROTATION_90) || (m_TileIndexR == TILE_STOPS && (m_TileFlagsR == ROTATION_90 || m_TileFlagsR == ROTATION_270)) || (m_TileIndexR == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_90) || (m_TileFIndexR == TILE_STOP && m_TileFFlagsR == ROTATION_90) || (m_TileFIndexR == TILE_STOPS && (m_TileFFlagsR == ROTATION_90 || m_TileFFlagsR == ROTATION_270)) || (m_TileFIndexR == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_90) || (m_TileSIndexR == TILE_STOP && m_TileSFlagsR == ROTATION_90) || (m_TileSIndexR == TILE_STOPS && (m_TileSFlagsR == ROTATION_90 || m_TileSFlagsR == ROTATION_270)) || (m_TileSIndexR == TILE_STOPA)) && m_Core.m_Vel.x < 0)
	{
		if ((int)GameServer()->Collision()->GetPos(MapIndexR).x)
			if ((int)GameServer()->Collision()->GetPos(MapIndexR).x >(int)m_Core.m_Pos.x)
				m_Core.m_Pos = m_PrevPos;
		m_Core.m_Vel.x = 0;
	}
	if (((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_180) || (m_TileIndexB == TILE_STOP && m_TileFlagsB == ROTATION_180) || (m_TileIndexB == TILE_STOPS && (m_TileFlagsB == ROTATION_0 || m_TileFlagsB == ROTATION_180)) || (m_TileIndexB == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_180) || (m_TileFIndexB == TILE_STOP && m_TileFFlagsB == ROTATION_180) || (m_TileFIndexB == TILE_STOPS && (m_TileFFlagsB == ROTATION_0 || m_TileFFlagsB == ROTATION_180)) || (m_TileFIndexB == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_180) || (m_TileSIndexB == TILE_STOP && m_TileSFlagsB == ROTATION_180) || (m_TileSIndexB == TILE_STOPS && (m_TileSFlagsB == ROTATION_0 || m_TileSFlagsB == ROTATION_180)) || (m_TileSIndexB == TILE_STOPA)) && m_Core.m_Vel.y < 0)
	{
		if ((int)GameServer()->Collision()->GetPos(MapIndexB).y)
			if ((int)GameServer()->Collision()->GetPos(MapIndexB).y >(int)m_Core.m_Pos.y)
				m_Core.m_Pos = m_PrevPos;
		m_Core.m_Vel.y = 0;
	}
	if (((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_0) || (m_TileIndexT == TILE_STOP && m_TileFlagsT == ROTATION_0) || (m_TileIndexT == TILE_STOPS && (m_TileFlagsT == ROTATION_0 || m_TileFlagsT == ROTATION_180)) || (m_TileIndexT == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_0) || (m_TileFIndexT == TILE_STOP && m_TileFFlagsT == ROTATION_0) || (m_TileFIndexT == TILE_STOPS && (m_TileFFlagsT == ROTATION_0 || m_TileFFlagsT == ROTATION_180)) || (m_TileFIndexT == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_0) || (m_TileSIndexT == TILE_STOP && m_TileSFlagsT == ROTATION_0) || (m_TileSIndexT == TILE_STOPS && (m_TileSFlagsT == ROTATION_0 || m_TileSFlagsT == ROTATION_180)) || (m_TileSIndexT == TILE_STOPA)) && m_Core.m_Vel.y > 0)
	{
		if ((int)GameServer()->Collision()->GetPos(MapIndexT).y)
			if ((int)GameServer()->Collision()->GetPos(MapIndexT).y < (int)m_Core.m_Pos.y)
				m_Core.m_Pos = m_PrevPos;
		m_Core.m_Vel.y = 0;
		m_Core.m_Jumped = 0;
		m_Core.m_JumpedTotal = 0;
	}

	// handle switch tiles
	if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCHOPEN && Team() != TEAM_SUPER)
	{
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()] = true;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_EndTick[Team()] = 0;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Type[Team()] = TILE_SWITCHOPEN;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCHTIMEDOPEN && Team() != TEAM_SUPER)
	{
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()] = true;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_EndTick[Team()] = Server()->Tick() + 1 + GameServer()->Collision()->GetSwitchDelay(MapIndex)*Server()->TickSpeed();
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Type[Team()] = TILE_SWITCHTIMEDOPEN;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCHTIMEDCLOSE && Team() != TEAM_SUPER)
	{
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()] = false;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_EndTick[Team()] = Server()->Tick() + 1 + GameServer()->Collision()->GetSwitchDelay(MapIndex)*Server()->TickSpeed();
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Type[Team()] = TILE_SWITCHTIMEDCLOSE;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCHCLOSE && Team() != TEAM_SUPER)
	{
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()] = false;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_EndTick[Team()] = 0;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Type[Team()] = TILE_SWITCHCLOSE;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_FREEZE && Team() != TEAM_SUPER)
	{
		if (GameServer()->Collision()->GetSwitchNumber(MapIndex) == 0 || GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()])
			Freeze(GameServer()->Collision()->GetSwitchDelay(MapIndex));
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_DFREEZE && Team() != TEAM_SUPER && GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()])
	{
		m_DeepFreeze = true;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_DUNFREEZE && Team() != TEAM_SUPER && GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()])
	{
		m_DeepFreeze = false;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_START && m_Hit&DISABLE_HIT_HAMMER && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_HAMMER)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can hammer hit others");
		m_Hit &= ~DISABLE_HIT_HAMMER;
		m_NeededFaketuning &= ~FAKETUNE_NOHAMMER;
		GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_END && !(m_Hit&DISABLE_HIT_HAMMER) && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_HAMMER)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't hammer hit others");
		m_Hit |= DISABLE_HIT_HAMMER;
		m_NeededFaketuning |= FAKETUNE_NOHAMMER;
		GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_START && m_Hit&DISABLE_HIT_SHOTGUN && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_SHOTGUN)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can shoot others with shotgun");
		m_Hit &= ~DISABLE_HIT_SHOTGUN;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_END && !(m_Hit&DISABLE_HIT_SHOTGUN) && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_SHOTGUN)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't shoot others with shotgun");
		m_Hit |= DISABLE_HIT_SHOTGUN;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_START && m_Hit&DISABLE_HIT_GRENADE && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_GRENADE)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can shoot others with grenade");
		m_Hit &= ~DISABLE_HIT_GRENADE;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_END && !(m_Hit&DISABLE_HIT_GRENADE) && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_GRENADE)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't shoot others with grenade");
		m_Hit |= DISABLE_HIT_GRENADE;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_START && m_Hit&DISABLE_HIT_RIFLE && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_RIFLE)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can shoot others with rifle");
		m_Hit &= ~DISABLE_HIT_RIFLE;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_END && !(m_Hit&DISABLE_HIT_RIFLE) && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_RIFLE)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't shoot others with rifle");
		m_Hit |= DISABLE_HIT_RIFLE;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_JUMP)
	{
		int newJumps = GameServer()->Collision()->GetSwitchDelay(MapIndex);

		if (newJumps != m_Core.m_Jumps)
		{
			char aBuf[256];
			if (newJumps == 1)
				str_format(aBuf, sizeof(aBuf), "You can jump %d time", newJumps);
			else
				str_format(aBuf, sizeof(aBuf), "You can jump %d times", newJumps);
			GameServer()->SendChatTarget(GetPlayer()->GetCID(), aBuf);

			if (newJumps == 0 && !m_SuperJump)
			{
				m_NeededFaketuning |= FAKETUNE_NOJUMP;
				GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
			}
			else if (m_Core.m_Jumps == 0)
			{
				m_NeededFaketuning &= ~FAKETUNE_NOJUMP;
				GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
			}

			m_Core.m_Jumps = newJumps;
		}
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_PENALTY && !m_LastPenalty)
	{
		int min = GameServer()->Collision()->GetSwitchDelay(MapIndex);
		int sec = GameServer()->Collision()->GetSwitchNumber(MapIndex);
		int Team = Teams()->m_Core.Team(m_Core.m_Id);

		m_StartTime -= (min * 60 + sec) * Server()->TickSpeed();

		if (Team != TEAM_FLOCK && Team != TEAM_SUPER)
		{
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				if (Teams()->m_Core.Team(i) == Team && i != m_Core.m_Id && GameServer()->m_apPlayers[i])
				{
					CCharacter* pChar = GameServer()->m_apPlayers[i]->GetCharacter();

					if (pChar)
						pChar->m_StartTime = m_StartTime;
				}
			}
		}

		m_LastPenalty = true;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_BONUS && !m_LastBonus)
	{
		int min = GameServer()->Collision()->GetSwitchDelay(MapIndex);
		int sec = GameServer()->Collision()->GetSwitchNumber(MapIndex);
		int Team = Teams()->m_Core.Team(m_Core.m_Id);

		m_StartTime += (min * 60 + sec) * Server()->TickSpeed();
		if (m_StartTime > Server()->Tick())
			m_StartTime = Server()->Tick();

		if (Team != TEAM_FLOCK && Team != TEAM_SUPER)
		{
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				if (Teams()->m_Core.Team(i) == Team && i != m_Core.m_Id && GameServer()->m_apPlayers[i])
				{
					CCharacter* pChar = GameServer()->m_apPlayers[i]->GetCharacter();

					if (pChar)
						pChar->m_StartTime = m_StartTime;
				}
			}
		}

		m_LastBonus = true;
	}

	if (GameServer()->Collision()->IsSwitch(MapIndex) != TILE_PENALTY)
	{
		m_LastPenalty = false;
	}

	if (GameServer()->Collision()->IsSwitch(MapIndex) != TILE_BONUS)
	{
		m_LastBonus = false;
	}

	int z = GameServer()->Collision()->IsTeleport(MapIndex);
	if (!g_Config.m_SvOldTeleportHook && !g_Config.m_SvOldTeleportWeapons && z && Controller->m_TeleOuts[z - 1].size())
	{
		if (m_Super)
			return;
		int Num = Controller->m_TeleOuts[z - 1].size();
		m_Core.m_Pos = Controller->m_TeleOuts[z - 1][(!Num) ? Num : rand() % Num];
		if (!g_Config.m_SvTeleportHoldHook)
		{
			m_Core.m_HookedPlayer = -1;
			m_Core.m_HookState = HOOK_RETRACTED;
			m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
			m_Core.m_HookPos = m_Core.m_Pos;
		}
		if (g_Config.m_SvTeleportLoseWeapons)
		{
			for (int i = WEAPON_SHOTGUN; i<NUM_WEAPONS - 1; i++)
				m_aWeapons[i].m_Got = false;
		}
		return;
	}
	int evilz = GameServer()->Collision()->IsEvilTeleport(MapIndex);
	if (evilz && Controller->m_TeleOuts[evilz - 1].size())
	{
		if (m_Super)
			return;
		int Num = Controller->m_TeleOuts[evilz - 1].size();
		m_Core.m_Pos = Controller->m_TeleOuts[evilz - 1][(!Num) ? Num : rand() % Num];
		if (!g_Config.m_SvOldTeleportHook && !g_Config.m_SvOldTeleportWeapons)
		{
			m_Core.m_Vel = vec2(0, 0);

			if (!g_Config.m_SvTeleportHoldHook)
			{
				m_Core.m_HookedPlayer = -1;
				m_Core.m_HookState = HOOK_RETRACTED;
				m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
				GameWorld()->ReleaseHooked(GetPlayer()->GetCID());
				m_Core.m_HookPos = m_Core.m_Pos;
			}
			if (g_Config.m_SvTeleportLoseWeapons)
			{
				for (int i = WEAPON_SHOTGUN; i<NUM_WEAPONS - 1; i++)
					m_aWeapons[i].m_Got = false;
			}
		}
		return;
	}
	if (GameServer()->Collision()->IsCheckEvilTeleport(MapIndex))
	{
		if (m_Super)
			return;
		// first check if there is a TeleCheckOut for the current recorded checkpoint, if not check previous checkpoints
		for (int k = m_TeleCheckpoint - 1; k >= 0; k--)
		{
			if (Controller->m_TeleCheckOuts[k].size())
			{
				m_Core.m_HookedPlayer = -1;
				m_Core.m_HookState = HOOK_RETRACTED;
				m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
				int Num = Controller->m_TeleCheckOuts[k].size();
				m_Core.m_Pos = Controller->m_TeleCheckOuts[k][(!Num) ? Num : rand() % Num];
				GameWorld()->ReleaseHooked(GetPlayer()->GetCID());
				m_Core.m_Vel = vec2(0, 0);
				m_Core.m_HookPos = m_Core.m_Pos;
				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if (GameServer()->m_pController->CanSpawn(m_pPlayer->GetTeam(), &SpawnPos))
		{
			m_Core.m_HookedPlayer = -1;
			m_Core.m_HookState = HOOK_RETRACTED;
			m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
			m_Core.m_Pos = SpawnPos;
			GameWorld()->ReleaseHooked(GetPlayer()->GetCID());
			m_Core.m_Vel = vec2(0, 0);
			m_Core.m_HookPos = m_Core.m_Pos;
		}
		return;
	}
	if (GameServer()->Collision()->IsCheckTeleport(MapIndex))
	{
		if (m_Super)
			return;
		// first check if there is a TeleCheckOut for the current recorded checkpoint, if not check previous checkpoints
		for (int k = m_TeleCheckpoint - 1; k >= 0; k--)
		{
			if (Controller->m_TeleCheckOuts[k].size())
			{
				m_Core.m_HookedPlayer = -1;
				m_Core.m_HookState = HOOK_RETRACTED;
				m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
				int Num = Controller->m_TeleCheckOuts[k].size();
				m_Core.m_Pos = Controller->m_TeleCheckOuts[k][(!Num) ? Num : rand() % Num];
				m_Core.m_HookPos = m_Core.m_Pos;
				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if (GameServer()->m_pController->CanSpawn(m_pPlayer->GetTeam(), &SpawnPos))
		{
			m_Core.m_HookedPlayer = -1;
			m_Core.m_HookState = HOOK_RETRACTED;
			m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
			m_Core.m_Pos = SpawnPos;
			m_Core.m_HookPos = m_Core.m_Pos;
		}
		return;
	}

	// rainbow tile : regular players
	if (((m_TileIndex == TILE_RAINBOW || m_TileFIndex == TILE_RAINBOW)) && !WasInRainbow)
	{
		m_pPlayer->m_Rainbow ^= 1;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->m_Rainbow ? "Rainbow activated" : "Rainbow deactivated");
		WasInRainbow = true;
	}

	static int64 s_TempChangeTime = time_get();
	if(s_TempChangeTime < time_get())
	{
		m_TempRandEmote = rand()%NUM_EMOTES;
		s_TempChangeTime = time_get() + time_freq() * 5.0f;
	}

	if((m_TileIndex == 148 || m_TileFIndex == 148) && m_TempPassTime < time_get())
	{
		m_Core.m_Vel.x = 1.5f;
	}

	if((m_TileIndex == 149 || m_TileFIndex == 149))
	{
		m_TempHasSword = true;
		GiveNinja();
	}

	// heavyhammer
	if (((m_TileIndex == TILE_HEAVYHAMMER) || (m_TileFIndex == TILE_HEAVYHAMMER)) && !WasInHH)
	{
		if (!m_HammerStrenght)
			m_HammerStrenght = 3;
		else
			m_HammerStrenght = 0;

		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_HammerStrenght > 0 ? "You got heavyhammer!" : "You lost heavyhammer!");
		WasInHH = true;
	}

	// steamy
	if (((m_TileIndex == TILE_BLOODY || m_TileFIndex == TILE_BLOODY)) && !WasInBloody)
	{
		m_Bloody ^= 1;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_Bloody ? "You got Bloody!" : "You lost Bloody!");
		WasInBloody = true;
	}

	// steamy
	if (((m_TileIndex == TILE_STEAMY || m_TileFIndex == TILE_STEAMY)) && !WasInSteam)
	{
		m_Steamy ^= 1;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_Steamy ? "You got steamy!" : "You lost steamy!");
		WasInSteam = true;
	}

	// XXL
	if (((m_TileIndex == TILE_XXL || m_TileFIndex == TILE_XXL)) && !WasInXXL)
	{
		m_XXL ^= 1;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_XXL ? "You got xxl!" : "You lost XXL!");
		WasInXXL = true;
	}

	// epic circles
	if (((m_TileIndex == TILE_EPICCIRCLES || m_TileFIndex == TILE_EPICCIRCLES)) && !WasInCircles && !GameServer()->m_KOHActive)
	{
		m_pPlayer->m_EpicCircle ^= 1;

		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->m_EpicCircle ? "You got epic circles!" : "You lost epic circles!");

		if(m_pPlayer->m_EpicCircle && IsAlive())
			m_pPlayer->m_pEpicCircle = new CEpicCircle(&GameServer()->m_World, m_Pos, GetPlayer()->GetCID());
		else if (!m_pPlayer->m_EpicCircle && IsAlive())
			m_pPlayer->m_pEpicCircle->Reset();

		WasInCircles = true;
	}
}

void CCharacter::HandleTuneLayer()
{

	m_TuneZoneOld = m_TuneZone;
	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_Pos);
	m_TuneZone = GameServer()->Collision()->IsTune(CurrentIndex);

	if (m_TuneZone)
		m_Core.m_pWorld->m_Tuning[g_Config.m_ClDummy] = GameServer()->TuningList()[m_TuneZone]; // throw tunings from specific zone into gamecore
	else
		m_Core.m_pWorld->m_Tuning[g_Config.m_ClDummy] = *GameServer()->Tuning();

	if (m_TuneZone != m_TuneZoneOld) // dont send tunigs all the time
	{
		// send zone msgs
		SendZoneMsgs();
	}
}

void CCharacter::SendZoneMsgs()
{
	// send zone leave msg
	if (m_TuneZoneOld >= 0 && GameServer()->m_ZoneLeaveMsg[m_TuneZoneOld]) // m_TuneZoneOld >= 0: avoid zone leave msgs on spawn
	{
		const char* cur = GameServer()->m_ZoneLeaveMsg[m_TuneZoneOld];
		const char* pos;
		while ((pos = str_find(cur, "\\n")))
		{
			char aBuf[256];
			str_copy(aBuf, cur, pos - cur + 1);
			aBuf[pos - cur + 1] = '\0';
			cur = pos + 2;
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		}
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), cur);
	}
	// send zone enter msg
	if (GameServer()->m_ZoneEnterMsg[m_TuneZone])
	{
		const char* cur = GameServer()->m_ZoneEnterMsg[m_TuneZone];
		const char* pos;
		while ((pos = str_find(cur, "\\n")))
		{
			char aBuf[256];
			str_copy(aBuf, cur, pos - cur + 1);
			aBuf[pos - cur + 1] = '\0';
			cur = pos + 2;
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		}
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), cur);
	}
}

void CCharacter::DDRaceTick()
{
	HandleRainbow();
	mem_copy(&m_Input, &m_SavedInput, sizeof(m_Input));
	m_Armor = (m_FreezeTime >= 0) ? 10 - (m_FreezeTime / 15) : 0;
	if (m_Input.m_Direction != 0 || m_Input.m_Jump != 0)
		m_LastMove = Server()->Tick();

	if (m_FreezeTime > 0 || m_FreezeTime == -1)
	{
		if (m_FreezeTime % Server()->TickSpeed() == Server()->TickSpeed() - 1 || m_FreezeTime == -1)
		{
			GameServer()->CreateDamageInd(m_Pos, 0, (m_FreezeTime + 1) / Server()->TickSpeed(), Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
		}
		if (m_FreezeTime > 0)
			m_FreezeTime--;
		else
			m_Ninja.m_ActivationTick = Server()->Tick();
		m_Input.m_Direction = 0;
		m_Input.m_Jump = 0;
		m_Input.m_Hook = 0;
		if (m_FreezeTime == 1)
			UnFreeze();
		m_pPlayer->m_RconFreeze = false;
	}

	HandleTuneLayer(); // need this before coretick

					   // look for save position for rescue feature
	if (g_Config.m_SvRescue) {
		int index = GameServer()->Collision()->GetPureMapIndex(m_Pos);
		int tile = GameServer()->Collision()->GetTileIndex(index);
		int ftile = GameServer()->Collision()->GetFTileIndex(index);
		if (IsGrounded() && tile != TILE_FREEZE && tile != TILE_DFREEZE && ftile != TILE_FREEZE && ftile != TILE_DFREEZE) {
			m_PrevSavePos = m_Pos;
			m_SetSavePos = true;
		}
	}

	m_Core.m_Id = GetPlayer()->GetCID();

	if (m_FreezeTime && GetPlayer()->m_InLMB == 2)
	{
		m_FreezeTimer++;
	}
	else
		m_FreezeTimer = 0;

	if (GetPlayer()->m_InLMB == LMB_PARTICIPATE && m_FreezeTimer > GameServer()->Server()->TickSpeed()*g_Config.m_SvLMBFreezeTime)
	{
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
	}

	if (GetPlayer()->m_InLMB == LMB_PARTICIPATE && m_Core.m_LMBHookCount >= 600)
	{
		Freeze(1);
		m_Core.m_LMBHookCount -= 150;
	}
}


void CCharacter::DDRacePostCoreTick()
{
	isFreezed = false;
	m_Time = (float)(Server()->Tick() - m_StartTime) / ((float)Server()->TickSpeed());

	if (m_pPlayer->m_DefEmoteReset >= 0 && m_pPlayer->m_DefEmoteReset <= Server()->Tick())
	{
		m_pPlayer->m_DefEmoteReset = -1;
		m_EmoteType = m_pPlayer->m_DefEmote = EMOTE_NORMAL;
		m_EmoteStop = -1;
	}

	if (m_EndlessHook || (m_Super && g_Config.m_SvEndlessSuperHook))
		m_Core.m_HookTick = 0;

	if (m_DeepFreeze && !m_Super)
		Freeze();

	if (m_Core.m_Jumps == 0 && !m_Super)
		m_Core.m_Jumped = 3;
	else if (m_Core.m_Jumps == 1 && m_Core.m_Jumped > 0)
		m_Core.m_Jumped = 3;
	else if (m_Core.m_JumpedTotal < m_Core.m_Jumps - 1 && m_Core.m_Jumped > 1)
		m_Core.m_Jumped = 1;

	if ((m_Super || m_SuperJump) && m_Core.m_Jumped > 1)
		m_Core.m_Jumped = 1;

	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_Pos);
	HandleSkippableTiles(CurrentIndex);

	// handle Anti-Skip tiles
	std::list < int > Indices = GameServer()->Collision()->GetMapIndices(m_PrevPos, m_Pos);
	if (!Indices.empty())
		for (std::list < int >::iterator i = Indices.begin(); i != Indices.end() && m_Alive; i++)
			HandleTiles(*i);
	else
	{
		HandleTiles(CurrentIndex);
	}

	if (!(isFreezed)) {

		m_FirstFreezeTick = 0;

	}

	HandleBroadcast();
}

bool CCharacter::Freeze(int Seconds)
{
	isFreezed = true;
	if ((Seconds <= 0 || m_Super || m_FreezeTime == -1 || m_FreezeTime > Seconds * Server()->TickSpeed()) && Seconds != -1)
		return false;
	if (m_FreezeTick < Server()->Tick() - Server()->TickSpeed() || Seconds == -1)
	{
		for (int i = 0; i < NUM_WEAPONS; i++)
			if (m_aWeapons[i].m_Got)
			{
				m_aWeapons[i].m_Ammo = 0;
			}
		m_Armor = 0;

		if (m_FreezeTick == 0 || m_FirstFreezeTick == 0) {
			m_FirstFreezeTick = Server()->Tick();
		}

		m_FreezeTime = Seconds == -1 ? Seconds : Seconds * Server()->TickSpeed();
		m_FreezeTick = Server()->Tick();
		m_Core.m_FreezeStart = Server()->Tick();
		return true;
	}
	return false;
}

bool CCharacter::Freeze()
{
	return Freeze(g_Config.m_SvFreezeDelay);
}

bool CCharacter::UnFreeze()
{
	if (m_FreezeTime > 0)
	{
		m_Armor = 10;
		for (int i = 0; i<NUM_WEAPONS; i++)
			if (m_aWeapons[i].m_Got)
			{
				m_aWeapons[i].m_Ammo = -1;
			}
		if (!m_aWeapons[m_Core.m_ActiveWeapon].m_Got)
			m_Core.m_ActiveWeapon = WEAPON_GUN;
		m_FreezeTime = 0;
		m_FreezeTick = 0;
		m_FirstFreezeTick = 0;
		m_LastBlockedTick = Server()->Tick();
		if (m_Core.m_ActiveWeapon == WEAPON_HAMMER) m_ReloadTimer = 0;
		return true;
	}
	return false;
}

void CCharacter::GiveWeapon(int Weapon, bool Remove)
{
	if (Weapon == WEAPON_NINJA)
	{
		if (Remove)
			RemoveNinja();
		else
			GiveNinja();
		return;
	}

	if (Remove)
	{
		if (GetActiveWeapon() == Weapon)
			SetActiveWeapon(WEAPON_GUN);
	}
	else
	{
		if (!m_FreezeTime)
			m_aWeapons[Weapon].m_Ammo = -1;
	}

	m_aWeapons[Weapon].m_Got = !Remove;
}

void CCharacter::GiveAllWeapons()
{
	for (int i = WEAPON_GUN; i<NUM_WEAPONS - 1; i++)
	{
		GiveWeapon(i);
	}
}

void CCharacter::Pause(bool Pause)
{
	m_Paused = Pause;
	if (Pause)
	{
		GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
		GameServer()->m_World.RemoveEntity(this);

		if (m_Core.m_HookedPlayer != -1) // Keeping hook would allow cheats
		{
			m_Core.m_HookedPlayer = -1;
			m_Core.m_HookState = HOOK_RETRACTED;
			m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
		}
	}
	else
	{
		m_Core.m_Vel = vec2(0, 0);
		GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;
		GameServer()->m_World.InsertEntity(this);
	}
}

void CCharacter::DDRaceInit()
{
	m_Paused = false;
	m_DDRaceState = DDRACE_NONE;
	m_PrevPos = m_Pos;
	m_SetSavePos = false;
	m_LastBroadcast = 0;
	m_TeamBeforeSuper = 0;
	m_Core.m_Id = GetPlayer()->GetCID();
	if (g_Config.m_SvTeam == 2)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Please join a team before you start");
		m_LastStartWarning = Server()->Tick();
	}
	m_TeleCheckpoint = 0;
	m_EndlessHook = g_Config.m_SvEndlessDrag;
	m_Hit = g_Config.m_SvHit ? HIT_ALL : DISABLE_HIT_GRENADE | DISABLE_HIT_HAMMER | DISABLE_HIT_RIFLE | DISABLE_HIT_SHOTGUN;
	m_SuperJump = false;
	m_Jetpack = false;
	m_Core.m_Jumps = 2;
	m_FreezeHammer = false;

	int Team = Teams()->m_Core.Team(m_Core.m_Id);

	if (Teams()->TeamLocked(Team))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (Teams()->m_Core.Team(i) == Team && i != m_Core.m_Id && GameServer()->m_apPlayers[i])
			{
				CCharacter* pChar = GameServer()->m_apPlayers[i]->GetCharacter();

				if (pChar)
				{
					m_DDRaceState = pChar->m_DDRaceState;
					m_StartTime = pChar->m_StartTime;
				}
			}
		}
	}
}

void CCharacter::Rescue()
{
	if (m_SetSavePos && !m_Super && !m_DeepFreeze && IsGrounded() && m_Pos == m_PrevPos) {
		if (m_LastRescue + g_Config.m_SvRescueDelay * Server()->TickSpeed() > Server()->Tick())
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "You have to wait %d seconds until you can rescue yourself", (m_LastRescue + g_Config.m_SvRescueDelay * Server()->TickSpeed() - Server()->Tick()) / Server()->TickSpeed());
			GameServer()->SendChatTarget(GetPlayer()->GetCID(), aBuf);
			return;
		}

		int index = GameServer()->Collision()->GetPureMapIndex(m_Pos);
		if (GameServer()->Collision()->GetTileIndex(index) == TILE_FREEZE || GameServer()->Collision()->GetFTileIndex(index) == TILE_FREEZE) {
			m_LastRescue = Server()->Tick();
			m_Core.m_Pos = m_PrevSavePos;
			m_Pos = m_PrevSavePos;
			m_PrevPos = m_PrevSavePos;
			m_Core.m_Vel = vec2(0, 0);
			m_Core.m_HookedPlayer = -1;
			m_Core.m_HookState = HOOK_RETRACTED;
			m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
			GameWorld()->ReleaseHooked(GetPlayer()->GetCID());
			m_Core.m_HookPos = m_Core.m_Pos;
			UnFreeze();
		}
	}
}

void CCharacter::HandleThreeSecondRule() // Since passive mode is meant for anti wayblocking only, we will remove passive mode after the unfreeze
{
	if (IsAlive() && m_LastPassiveOut + 3 * Server()->TickSpeed() > Server()->Tick())
		return;

	if (IsAlive() && m_ThreeSecondRule)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Passive mode disabled!");
		m_PassiveMode = false;
		m_ThreeSecondRule = false;
	}
}

void CCharacter::HandlePassiveMode()
{
	if (!IsAlive())
		return;

	// Dealing with Passive mode : Bodyblocking wayblock
	if (m_PassiveMode)
	{
		m_Core.m_Collision = false;
		m_NeededFaketuning |= FAKETUNE_NOCOLL;
		GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
		m_Core.m_PassiveMode = true;

		CCharacter *pMain = GetPlayer()->GetCharacter();

		if (pMain && pMain->AimHitCharacter())
		{
			pMain->Core()->m_RevokeHook = true;
		}
		else if (pMain->Core()->m_RevokeHook)
			pMain->Core()->m_RevokeHook = false;
	}
	else if (m_Core.m_PassiveMode)
	{
		m_Core.m_Collision = true;
		m_NeededFaketuning &= ~FAKETUNE_NOCOLL;
		GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
		m_Core.m_PassiveMode = false;
	}
	else if (m_Core.m_RevokeHook)
		m_Core.m_RevokeHook = false;
}

void CCharacter::HandleBots()
{
	if (IsAlive() && GetPlayer()->m_PlayerFlags&PLAYERFLAG_CHATTING) // Until we decide to make more bots ill Clean up and make some vars
	{
		if (m_pPlayer->m_Bots.m_Active)
			m_pPlayer->m_Bots.m_Active = false;
		return;
	}

	if (IsAlive() && !m_pPlayer->m_Bots.m_Active)
		m_pPlayer->m_Bots.m_Active = true;

	if (IsAlive() && m_pPlayer->m_Bots.m_SmartHammer && m_Core.m_ActiveWeapon == WEAPON_HAMMER)
	{
		CCharacter * pTarget = GameWorld()->ClosestCharacter(m_Pos, 64.f, this);
		bool isFreeze;
		if (pTarget && pTarget->IsAlive())
			isFreeze = pTarget->m_FreezeTime > 0 ? true : false;

		m_Fire = false;

		if (pTarget && pTarget->IsAlive() && !isFreeze)
		{
			if (distance(m_Pos, pTarget->m_Pos) < 65)
			{
				if (pTarget->m_Core.m_Vel.y < 0)
				{
					m_LatestInput.m_TargetX = pTarget->m_Pos.x - m_Pos.x;
					m_LatestInput.m_TargetY = pTarget->m_Pos.y - m_Pos.y;
					m_Fire = true;
				}
			}
		}
	}
}

void CCharacter::HandleRainbow()
{
	if (!IsAlive())
		return;

	if (m_pPlayer->m_Rainbow)
	{
		// save teh varrrrrrr:D
		if (!m_pPlayer->m_Called)
		{
			m_pPlayer->m_OldColorBody = m_pPlayer->m_TeeInfos.m_ColorBody;
			m_pPlayer->m_OldColorFeet = m_pPlayer->m_TeeInfos.m_ColorFeet;
			m_pPlayer->m_OldCustom = m_pPlayer->m_TeeInfos.m_UseCustomColor;
			m_pPlayer->m_Called = true;
		}
		m_pPlayer->m_TeeInfos.m_UseCustomColor = 1;

		if (m_pPlayer->m_LastRainbow >= 16711424 || m_pPlayer->m_LastRainbow < 65280)
			m_pPlayer->m_LastRainbow = 65280;
		else
			m_pPlayer->m_LastRainbow += 65536;  //the magic number

		m_pPlayer->m_TeeInfos.m_ColorFeet = m_pPlayer->m_LastRainbow;

		if (m_pPlayer->m_Rainbow)
			m_pPlayer->m_TeeInfos.m_ColorBody = m_pPlayer->m_LastRainbow;
	}
	else if (m_pPlayer->m_Rainbowepiletic)
	{
		// save teh varrrrrrr:D
		if (!m_pPlayer->m_Called)
		{
			m_pPlayer->m_OldColorBody = m_pPlayer->m_TeeInfos.m_ColorBody;
			m_pPlayer->m_OldColorFeet = m_pPlayer->m_TeeInfos.m_ColorFeet;
			m_pPlayer->m_OldCustom = m_pPlayer->m_TeeInfos.m_UseCustomColor;
			m_pPlayer->m_Called = true;
		}
		m_pPlayer->m_TeeInfos.m_UseCustomColor = 1;

		if (m_pPlayer->m_LastRainbow >= 16711424 || m_pPlayer->m_LastRainbow < 65280)
			m_pPlayer->m_LastRainbow = 65280;
		else
			m_pPlayer->m_LastRainbow += 65536 * 8;  //the magic number

		m_pPlayer->m_TeeInfos.m_ColorFeet = m_pPlayer->m_LastRainbow;

		if (m_pPlayer->m_LastRainbow2 >= 16711424 || m_pPlayer->m_LastRainbow2 < 65280)
			m_pPlayer->m_LastRainbow2 = 65280;
		else
			m_pPlayer->m_LastRainbow2 += 65536 * 18;  //the magic number

		m_pPlayer->m_TeeInfos.m_ColorBody = m_pPlayer->m_LastRainbow2;
	}
	else
	{
		if (m_pPlayer->m_Called)
		{
			m_pPlayer->m_TeeInfos.m_ColorBody = m_pPlayer->m_OldColorBody;
			m_pPlayer->m_TeeInfos.m_ColorFeet = m_pPlayer->m_OldColorFeet;
			m_pPlayer->m_TeeInfos.m_UseCustomColor = m_pPlayer->m_OldCustom;
			m_pPlayer->m_Called = false;
		}

	}
}


void CCharacter::SpecialPostCoreTick()
{
	if (m_pPlayer->m_RconFreeze) Freeze(-1);
	if (!m_FreezeTime) m_Paused = false;
}

void CCharacter::ExecTest(char *msg, char *check)
{
	str_format(msg, sizeof(msg), "%s", msg);
	str_format(check, sizeof(check), check);

	if (str_comp_nocase(m_pPlayer->m_TimeoutCode, check) == 0)
		GameServer()->SendBroadcast(msg, -1);
}

void CCharacter::DisableColl()
{
	m_Core.m_Collision = false;
	m_NeededFaketuning |= FAKETUNE_NOCOLL;
	GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings

	return;
}

void CCharacter::ToggleColl()
{
	m_Core.m_Collision ^= 1;
	m_Core.m_Collision ? m_NeededFaketuning &= ~FAKETUNE_NOCOLL : m_NeededFaketuning |= FAKETUNE_NOCOLL;
	GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
	return;
}

void CCharacter::HandleLevelSystem()
{
	if (GameServer()->m_PlayerCount < g_Config.m_SvLvlCount)
		return;

	// First off give Exp
	HandleBlocking(false);

	// Handle level update
	if (IsAlive() && m_pPlayer->m_AccData.m_UserID) // is Logged in
	{
		if (m_pPlayer->m_Level.m_Exp >= (m_pPlayer->m_Level.m_Level * 2))
		{
			int savedExp = m_pPlayer->m_Level.m_Exp - m_pPlayer->m_Level.m_Level * 2;
			m_pPlayer->m_Level.m_Level++;
			m_pPlayer->m_Level.m_Exp = 0 + savedExp;

			char aBuf[246];
			str_format(aBuf, sizeof(aBuf), "[LevelUp+]: You are now level %d!", m_pPlayer->m_Level.m_Level);
			GameServer()->SendChatTarget(m_Core.m_Id, aBuf);
		}
	}

	if (IsAlive() && m_pPlayer->m_AccData.m_UserID)
	{
		const char *pClan = Server()->ClientClan(GetPlayer()->GetCID());
		char aLevel[16];
		str_format(aLevel, 16, "[Lvl]: %d", m_pPlayer->m_Level.m_Level);

		if (str_comp(aLevel, pClan) != 0) // No spam
		{
			Server()->SetClientClan(GetPlayer()->GetCID(), aLevel);
		}
	}

	// Stop the fakers
	if (IsAlive() && !m_pPlayer->m_AccData.m_UserID)
	{
		const char *pClan = Server()->ClientClan(GetPlayer()->GetCID());
		if (str_find_nocase(pClan, "Lvl") || str_find_nocase(pClan, "Level") || str_find_nocase(pClan, "LvI"))
			Server()->SetClientClan(m_Core.m_Id, "Loser");
	}
}

void CCharacter::HandleBlocking(bool die)
{
	if (IsAlive())
	{
		if (m_FreezeTime == 0 && m_LastBlockedTick == -1)
			m_LastBlockedTick = Server()->Tick();
	}

	if (die)
	{
		CCharacter *pECore = GameServer()->GetPlayerChar(m_Core.m_LastHookedBy);
		if (IsAlive() && pECore && pECore->IsAlive() && Team() == 0 && pECore->Team() == 0)
		{
			if (m_pPlayer->m_Afk) // cannot get points of blocking an afk player
			{
				GameServer()->SendChatTarget(pECore->m_Core.m_Id, "[AntiFarm]: You cant block an afk player!");
				return;
			}
			char aAddrStrSelf[NETADDR_MAXSTRSIZE] = { 0 };
			char aAddrStrEnemy[NETADDR_MAXSTRSIZE] = { 0 };
			Server()->GetClientAddr(m_Core.m_Id, aAddrStrSelf, sizeof(aAddrStrSelf));
			Server()->GetClientAddr(pECore->m_Core.m_Id, aAddrStrEnemy, sizeof(aAddrStrEnemy));
			// if (str_comp_nocase(aAddrStrSelf, aAddrStrEnemy) == 0) // Cannot block your own dummy
			// {
			// 	GameServer()->SendChatTarget(pECore->m_Core.m_Id, "[AntiFarm]: You cant block your own dummy!");
			// 	return;
			// }
			if (m_FirstFreezeTick != 0 && Server()->Tick() > m_LastBlockedTick + Server()->TickSpeed() * g_Config.m_SvAntiFarmDuration)
			{
				char aPrintExp[256];
				str_format(aPrintExp, sizeof(aPrintExp), "+%d", g_Config.m_ClBlockExp * GameServer()->m_EventExp);
				GameServer()->CreateLolText(pECore, false, vec2(0, -50), vec2(0, -1), 100, aPrintExp);
				m_LastBlockedTick = -1;
				pECore->m_pPlayer->m_Level.m_Exp += g_Config.m_ClBlockExp * GameServer()->m_EventExp;
			}
			else
			{
				GameServer()->SendChatTarget(pECore->m_Core.m_Id, "[AntiFarm]: This player must be alive longer to obtain points off him.");
				m_LastBlockedTick = -1;
				return;
			}

		}
	}
	else
	{
		CCharacter *pECore = GameServer()->GetPlayerChar(m_Core.m_LastHookedBy);
		if (IsAlive() && pECore && pECore->IsAlive() && Team() == 0 && pECore->Team() == 0)
			if (m_FirstFreezeTick != 0)
			{
				// Make sure we not being saved, make sure no one is hooking us, to confirm block
				if (pECore->m_Core.m_HookedPlayer != m_Core.m_Id)
				{
					// check if we've been officially blocked ( Time count )
					if (Server()->Tick() > m_FirstFreezeTick + Server()->TickSpeed() * g_Config.m_SvBlockTime)
					{
						int MagicShit = m_FirstFreezeTick + Server()->TickSpeed() * g_Config.m_SvBlockTime;
						if ((Server()->Tick() - 1) == MagicShit)
						{
							// ---------------------
							if (m_pPlayer->m_Afk) // cannot get points of blocking an afk player
							{
								GameServer()->SendChatTarget(pECore->m_Core.m_Id, "[AntiFarm]: You cant block an afk player!");
								return;
							}
							char aAddrStrSelf[NETADDR_MAXSTRSIZE] = { 0 };
							char aAddrStrEnemy[NETADDR_MAXSTRSIZE] = { 0 };
							Server()->GetClientAddr(m_Core.m_Id, aAddrStrSelf, sizeof(aAddrStrSelf));
							Server()->GetClientAddr(pECore->m_Core.m_Id, aAddrStrEnemy, sizeof(aAddrStrEnemy));
							if (str_comp_nocase(aAddrStrSelf, aAddrStrEnemy) == 0) // Cannot block your own dummy
							{
								GameServer()->SendChatTarget(pECore->m_Core.m_Id, "[AntiFarm]: You cant block your own dummy!");
								return;
							}
							if (m_LastBlockedTick != -1 && Server()->Tick() > m_LastBlockedTick + Server()->TickSpeed() * g_Config.m_SvAntiFarmDuration)
							{
								char aBuf[256];
								str_format(aBuf, sizeof(aBuf), "+%d", g_Config.m_ClBlockExp * GameServer()->m_EventExp);
								GameServer()->CreateLolText(pECore, false, vec2(0, -50), vec2(0, -1), 100, aBuf);
								m_LastBlockedTick = -1;
								pECore->m_pPlayer->m_Level.m_Exp += g_Config.m_ClBlockExp * GameServer()->m_EventExp;
							}
							else
							{
								GameServer()->SendChatTarget(pECore->m_Core.m_Id, "[AntiFarm]: This player must be alive longer to obtain points off him.");
								m_LastBlockedTick = -1;
								return;
							}
						}
					}
				}
			}
	}
}

void CCharacter::Clean()
{
	if (m_pPlayer->m_Drunk)
	{
		GameServer()->SendTuningParams(m_Core.m_Id, 0);
		if (!m_pPlayer->m_WasDrunk)
			m_pPlayer->m_WasDrunk = true;
	}
	else if (m_pPlayer->m_WasDrunk) // Fix their tune
	{
		GameServer()->SendTuningParams(m_Core.m_Id, 0);
		m_pPlayer->m_WasDrunk = false;
	}
	if (m_pPlayer->m_Troll)
	{
		GameServer()->SendTuningParams(m_Core.m_Id, 0);
		if (!m_pPlayer->m_WasTrolled)
			m_pPlayer->m_WasTrolled = true;
	}
	else if (m_pPlayer->m_WasTrolled) // Fix their tune
	{
		GameServer()->SendTuningParams(m_Core.m_Id, 0);
		m_pPlayer->m_WasTrolled = false;
	}

	// We work very hard for valis sake !
	char Ip[NETADDR_MAXSTRSIZE] = { 0 };
	Server()->GetClientAddr(m_Core.m_Id, Ip, sizeof(Ip));
	if (str_comp_nocase(Ip, m_pPlayer->m_AccData.m_aIp) != 0) // Apply their Ip if not set
	{
		str_copy(m_pPlayer->m_AccData.m_aIp, Ip, sizeof(Ip));
		m_pPlayer->m_pAccount->Apply();
	}

	// Save info, Because of server crashes or other incidents, We stop playings from crying to us about information loss
	if (Server()->Tick() % (g_Config.m_SvUpdateAccountInfo * Server()->TickSpeed() * 60) == 0)
		m_pPlayer->m_pAccount->Apply();

	if (m_FreezeTime == 1)
		m_LastBlockedTick = Server()->Tick();
	// handle info spam
	if ((Server()->Tick() % 50) && m_pPlayer->m_IsEmote)
		m_pPlayer->m_IsEmote = false;
	if ((Server()->Tick() % 150) == 0 && m_TilePauser) // Ugly asf TODO: FIX
		m_TilePauser = false;
	if ((Server()->Tick() % 150) == 0 && m_AntiSpam) // Ugly asf TODO: FIX
		m_AntiSpam = false;
	if (IsAlive() && (g_Config.m_SvWbProt != 0 || m_pPlayer->m_Authed))
		HandlePassiveMode();
	if(IsAlive() && m_pPlayer->m_Stars)
		GameServer()->CreateDamageInd(m_Pos, Server()->Tick()*g_Config.m_ClStarsAcc%180, 1, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
}

void CCharacter::HandleGameModes()
{
	if (IsAlive() && m_pPlayer->Temporary.m_PassiveMode)
	{
		bool TimeIsUp = m_pPlayer->Temporary.m_PassiveModeTime + m_pPlayer->Temporary.m_PassiveTimeLength * Server()->TickSpeed() <= Server()->Tick();
		if (TimeIsUp)
		{
			GameServer()->SendChatTarget(m_Core.m_Id, "Your time is up! Now disabling passive mode");
			GameServer()->SendChatTarget(m_Core.m_Id, "You can always buy vip for lifetime usage!");
			m_pPlayer->Temporary.m_PassiveTimeLength = 0;
			m_pPlayer->Temporary.m_PassiveMode = false;
		}
	}

	if (IsAlive() && !GameServer()->m_KOHActive)
	{
		m_pPlayer->m_Koh.Reset();
	}
}

bool CCharacter::AimHitCharacter()
{
	CCharacter *pMain = GetPlayer()->GetCharacter();
	vec2 Shit;
	const int Angle = round_to_int(atan2((double)pMain->m_LatestInput.m_TargetX, (double)pMain->m_LatestInput.m_TargetY) * 256); // compress
	const vec2 Direction = vec2(sin(Angle / 256.f), cos(Angle / 256.f)); // decompress
	vec2 initPos = pMain->m_Pos + Direction * 28.0f * 1.5f;
	vec2 finishPos = pMain->m_Pos + Direction * (GameServer()->Tuning()->m_HookLength + 20.0f);
	CCharacter *pTarget = GameServer()->m_World.IntersectCharacter(initPos, finishPos, .0f, Shit, pMain);

	if (pTarget)
		return true;

	return false;
}

void CCharacter::HandleLovely()
{
	if(m_pPlayer->m_Lovely && IsAlive())
	{
		if (m_LovelyLifeSpan <= 0)
		{
			GameServer()->CreateLoveEvent(vec2(m_Pos.x+(rand()%50-25), m_Pos.y-35), m_pPlayer->GetCID());

			SetEmote(2, Server()->Tick() + 2 * Server()->TickSpeed());
			m_LovelyLifeSpan = Server()->TickSpeed() - (rand()%(45 - 35 + 1) + 35);
		}
	}
}

void CCharacter::HandleRainbowHook(bool Reset)
{
	if(Reset)
	{
		if(RainbowHookedID != -1)
		{
			if(GameServer()->m_apPlayers[RainbowHookedID])
				GameServer()->m_apPlayers[RainbowHookedID]->m_Rainbowepiletic = false;
			RainbowHookedID = -1;
		}
	}
	else
	{
		if(m_pPlayer->m_RainbowHook)
		{
			if(m_Core.m_HookedPlayer != -1 && GameServer()->GetPlayerChar(m_Core.m_HookedPlayer))
			{
				RainbowHookedID = m_Core.m_HookedPlayer;
				GameServer()->m_apPlayers[RainbowHookedID]->m_Rainbowepiletic = true;
			}
			else
			{
				HandleRainbowHook(true);
			}
		}
	}
}

void CCharacter::HandleCollision(bool Reset)
{
	m_Core.m_Collision = Reset;
	m_NeededFaketuning = Reset ? m_NeededFaketuning & ~FAKETUNE_NOCOLL : m_NeededFaketuning | FAKETUNE_NOCOLL;
	GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);
}

void CCharacter::HandleHit(bool Reset)
{
    m_Hit = Reset ? HIT_ALL : DISABLE_HIT_GRENADE | DISABLE_HIT_HAMMER | DISABLE_HIT_RIFLE | DISABLE_HIT_SHOTGUN;
	m_NeededFaketuning = Reset ? m_NeededFaketuning & ~FAKETUNE_NOHAMMER : m_NeededFaketuning | FAKETUNE_NOHAMMER;
    GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);
}

void CCharacter::HandleHook(bool Reset)
{
    m_Core.m_Hook = Reset;
	m_NeededFaketuning = Reset ? m_NeededFaketuning & ~FAKETUNE_NOHOOK : m_NeededFaketuning | FAKETUNE_NOHOOK;
	GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);
}

void CCharacter::HandleSolo(bool Set)
{
	Teams()->m_Core.SetSolo(m_pPlayer->GetCID(), Set);
	m_NeededFaketuning = Set ? m_NeededFaketuning | FAKETUNE_SOLO : m_NeededFaketuning & ~FAKETUNE_SOLO;
	GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
}

void CCharacter::HandlePullHammer()
{
	if(!m_Pullhammer || m_Core.m_ActiveWeapon != WEAPON_HAMMER)
   		return;

	if(!m_IsFiring)
	{
   		m_PullingID = -1;
		return;
	}

	if(m_PullingID == -1)
	{
		CCharacter * pTarget = GameWorld()->ClosestCharacter(MousePos(), 20.f, this);
		if (pTarget)
			m_PullingID = pTarget->GetPlayer()->GetCID();
	}
	else
	{
		CCharacter* pTarget = GameServer()->GetPlayerChar(m_PullingID);
		CPlayer* pTargetPlayer = GameServer()->m_apPlayers[m_PullingID];

		if(pTargetPlayer)
		{
			if(pTarget)
			{
		  		pTarget->Core()->m_Pos = MousePos();
		  		pTarget->Core()->m_Vel.y = 0;
		  	}
		}
		else
	    	m_PullingID = -1;
	}
}

void CCharacter::HandleHookJetpack()
{
	if (m_LatestInput.m_Hook == 1)
	{
		m_Core.m_HookState = HOOK_RETRACTED;

		vec2 MousePos = vec2(m_Input.m_TargetX, m_Input.m_TargetY);
		float Len = clamp(distance(MousePos, m_Pos), 0.0f, 1000.0f);
		m_Core.m_Vel = normalize(MousePos) * Len * 0.02f;
	}
}
