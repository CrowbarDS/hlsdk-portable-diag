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

#endif
