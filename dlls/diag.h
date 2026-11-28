// Diagnostic log of the game's physics, frame by frame, to compare the real
// game with the Half-Life NDS port (its tools/ref/diag.sh).
//
// Nothing is written unless the cvar diag_log names a log, and then only while
// the client's script driver (cl_dll/diag_input.cpp) is playing a scenario:
// each of its commands carries its frame number in impact_position[0].
//
// The log is <gamedir>/diag/<diag_log>.csv. Every row is
//   kind,frame,time,...
// with the columns of each kind given once at the top by a "#cols" row.
#pragma once
#ifndef DIAG_H
#define DIAG_H

struct usercmd_s;
class CBasePlayer;
class CBaseMonster;

void Diag_Init( void );
bool Diag_On( void );
void Diag_CmdStart( CBasePlayer *player, const struct usercmd_s *cmd, unsigned int seed );
void Diag_PlayerPostThink( CBasePlayer *player );
void Diag_StartFrame( void );
void Diag_ClientCommand( const char *cmd, const char *args );
// One bullet of FireBulletsPlayer: where from, the way asked for (before the
// spread), the spread drawn (x, y: the sum of the two shared draws each) and
// the weapon's (its cone), the seed, and what its trace found.
void Diag_Shot( const Vector &src, const Vector &aim, float x, float y, const Vector &spread, int seed, int n,
	const Vector &end, float frac, edict_t *hit );
// A RadiusDamage: where (as given, before its own z + 1), how much, how far.
void Diag_RadiusDamage( const Vector &src, entvars_t *inflictor, float damage, float radius, int type );
// A mark sent to the clients (UTIL_DecalTrace, UTIL_GunshotDecalTrace): where
// its trace ended, the wall's normal there, which decal (gDecals) and on what.
void Diag_Decal( const TraceResult *trace, int decalNumber );

// One CBasePlayer::TakeDamage: what was asked, and the player before and after
// (the push is CBaseMonster::TakeDamage's).
struct DiagHurtScope
{
	entvars_t *player;
	entvars_t *inflictor;
	float amount;
	int type;
	Vector velocity;
	float health, armor;
	DiagHurtScope( entvars_t *p, entvars_t *i, float a, int t );
	~DiagHurtScope();
};

// One CPushable::Move: what it did with the toucher (see the result names in
// diag.cpp), the box's velocity before and after.
enum DiagPushResult
{
	DIAG_PUSH_OK,
	DIAG_PUSH_RIDE,     // the toucher stands on the box
	DIAG_PUSH_BUTTONS,  // the player's buttons say no (USE held, ...)
	DIAG_PUSH_AIR,      // the player is off the ground, the box is dry
};

struct DiagPushScope
{
	entvars_t *box;
	entvars_t *toucher;
	int push;
	Vector before;
	DiagPushResult result;
	DiagPushScope( entvars_t *b, entvars_t *t, int p );
	~DiagPushScope();
};

// The monsters (the AI's comparison with the port's).
// Their start (StartMonster): where, and when their first think is, kept from
// the map's load on and written when the log opens ("mstart").
void Diag_MonsterStart( CBaseMonster *monster );
// Each think's end (MonsterThink): where it is and what it is doing ("mon").
void Diag_MonsterThink( CBaseMonster *monster );
// Something its AI did ("aiev"): what (sched, task, fail, anim), a name
// (the schedule's) and two numbers (a task's id, a condition mask, an event).
void Diag_AiEvent( CBaseMonster *monster, const char *what, const char *name, int a, int b );
// A spoken line ("!NAME"), as EMIT_SOUND_DYN is asked for it: who says it (a
// monster, or nobody's: a button's, a speaker's, the suit) and which.
void Diag_Say( edict_t *entity, const char *sample );
// Move's look ahead refused (CheckLocalMove): where it looked to, the height
// its walk got to there, the waypoint, the result (LOCALMOVE_*) and how far it
// got.
void Diag_LookAhead( CBaseMonster *monster, const Vector &end, float reachedZ, const Vector &waypoint, int result,
	float dist );
// While a monster thinks or starts, the engine's random draws are its own
// ("rnd"): which monster, low, high and what came.
struct DiagMonsterScope
{
	CBaseMonster *previous;
	DiagMonsterScope( CBaseMonster *monster );
	~DiagMonsterScope();
};
// A way worked out (BuildRoute, FindCover, BuildNearestRoute): what the
// route's first step is once it is done ("aiev" route/cover/near).
struct DiagRouteScope
{
	CBaseMonster *monster;
	const char *what;
	DiagRouteScope( CBaseMonster *m, const char *w ) : monster( m ), what( w ) {}
	~DiagRouteScope();
};

#endif
