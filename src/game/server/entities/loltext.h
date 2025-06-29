#ifndef GAME_SERVER_ENTITIES_LOLTEXT_H
#define GAME_SERVER_ENTITIES_LOLTEXT_H

#include <game/server/entity.h>

#define MAX_LOLTEXTS 16
#define MAX_PLASMA_PER_LOLTEXT 128

//usage: GameServer()->CreateLoltext(...)
//it will dispose itself after lifespan ended

class CLolPlasma : public CEntity
{
public:
	//position relative to pParent->m_Pos. if pParent is NULL, Pos is absolute. lifespan in ticks
	CLolPlasma(CGameWorld *pGameWorld, CEntity *pParent, vec2 Pos, vec2 Vel, int Lifespan, int ltid, int plid);

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);


private:
	vec2 m_LocalPos; // local coordinate system is origin'd wherever we actually start (i.e. this is (0,0) after creation)
	vec2 m_Vel;
	int m_Life; // remaining ticks
	int m_StartTick; // tick created
	vec2 m_StartOff; // initial offset from parent, for proper following
	CEntity *m_pParent;
	int m_ltid;
	int m_plid;
};

class CLoltext
{
private:
	static bool s_aaaChars[256][5][3];
	static CLolPlasma *s_aapPlasma[MAX_LOLTEXTS][MAX_PLASMA_PER_LOLTEXT];
	static int s_aExpire[MAX_LOLTEXTS];
	static bool HasRepr(char c);
public:
	static vec2 TextSize(const char *pText, int size = 14);
	static int Create(CGameWorld *pGameWorld, CEntity *pParent, vec2 Pos, vec2 Vel, int Lifespan, const char *pText, bool Center, bool Follow, int size = 14);
	static void Destroy(CGameWorld *pGameWorld, int TextID);
	static void PlasmaGone(int TextID, int plid);
	static void Dump(); //debugging
};

#endif
