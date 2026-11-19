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
