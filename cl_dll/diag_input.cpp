// Diagnostic script driver: plays a scenario's inputs frame by frame, in place
// of the keyboard, so that the real game and the Half-Life NDS port are given
// exactly the same inputs (its tools/ref/diag.sh runs both).
//
//   diag_script <file>   the scenario to play
//   diag_go              play it from the next frame
//
// A scenario is text, one line per span of frames:
//   <frames> <forward> <side> <yaw> <pitch> [buttons]
// forward and side are move speeds (400 is a key held), yaw and pitch the
// view in degrees (pitch down is positive, as the game's), buttons a set of
// letters: a attack, b attack2, u use, j jump, d duck, r reload. A line
//   cmd <console command>
// runs that command when it is reached. '#' starts a comment.
//
// Each command the script makes carries its frame number (from 1) in
// impact_position[0], which the server's diagnostic log (dlls/diag.cpp)
// files its rows under; the command after the last carries 0, which ends the log.
#include "hud.h"
#include "cl_util.h"
#include "cvardef.h"
#include "usercmd.h"
#include "in_defs.h"
#include "const.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void Diag_ClientInit( void );
void Diag_ClientMove( struct usercmd_s *cmd, int active );

namespace
{
struct Span
{
	int frames;     // 0 for a command line
	float forward, side, yaw, pitch;
	int buttons;
	char command[96];
};

const int kMaxSpans = 256;
Span spans[kMaxSpans];
int spanCount;
int frame;          // the frame being made, 1-based; 0 when not playing
int ending;         // one more command, carrying 0, after the last frame
cvar_t *scriptPath;

int Buttons( const char *s )
{
	int b = 0;
	for( ; *s; s++ )
	{
		switch( *s )
		{
		case 'a': b |= IN_ATTACK; break;
		case 'b': b |= IN_ATTACK2; break;
		case 'u': b |= IN_USE; break;
		case 'j': b |= IN_JUMP; break;
		case 'd': b |= IN_DUCK; break;
		case 'r': b |= IN_RELOAD; break;
		}
	}
	return b;
}

bool Load( const char *path )
{
	FILE *f = fopen( path, "r" );
	if( !f )
	{
		gEngfuncs.Con_Printf( "DIAG: cannot read %s\n", path );
		return false;
	}
	spanCount = 0;
	char line[160];
	while( fgets( line, sizeof( line ), f ) && spanCount < kMaxSpans )
	{
		char *hash = strchr( line, '#' );
		if( hash )
			*hash = 0;
		char *nl = strpbrk( line, "\r\n" );
		if( nl )
			*nl = 0;
		Span s = {};
		if( !strncmp( line, "cmd ", 4 ))
		{
			strncpy( s.command, line + 4, sizeof( s.command ) - 1 );
			spans[spanCount++] = s;
			continue;
		}
		char buttons[16] = "";
		if( sscanf( line, "%d %f %f %f %f %15s", &s.frames, &s.forward, &s.side, &s.yaw, &s.pitch, buttons ) >= 5 && s.frames > 0 )
		{
			s.buttons = Buttons( buttons );
			spans[spanCount++] = s;
		}
	}
	fclose( f );
	return spanCount > 0;
}

void Go( void )
{
	if( !Load( scriptPath->string ))
		return;
	frame = 1;
	ending = 0;
	gEngfuncs.Con_Printf( "DIAG: script %s, %d lines\n", scriptPath->string, spanCount );
}
}

void Diag_ClientInit( void )
{
	scriptPath = gEngfuncs.pfnRegisterVariable( "diag_script", "", 0 );
	gEngfuncs.pfnAddCommand( "diag_go", Go );
}

void Diag_ClientMove( struct usercmd_s *cmd, int active )
{
	if( ending )
	{
		ending = 0;
		cmd->impact_position[0] = 0;
		gEngfuncs.Con_Printf( "DIAG: script done\n" );
		return;
	}
	if( !frame || !active )
		return;
	// The span holding this frame; the commands met on the way are run.
	int at = 0, i = 0;
	const Span *span = NULL;
	for( ; i < spanCount; i++ )
	{
		if( !spans[i].frames )
		{
			if( at == frame - 1 )
				gEngfuncs.pfnClientCmd( spans[i].command );
			continue;
		}
		if( frame <= at + spans[i].frames )
		{
			span = &spans[i];
			break;
		}
		at += spans[i].frames;
	}
	if( !span )
	{
		frame = 0;
		ending = 1;
		Diag_ClientMove( cmd, active );
		return;
	}
	cmd->forwardmove = span->forward;
	cmd->sidemove = span->side;
	cmd->upmove = 0;
	cmd->buttons = span->buttons;
	if( span->forward > 0 ) cmd->buttons |= IN_FORWARD;
	if( span->forward < 0 ) cmd->buttons |= IN_BACK;
	if( span->side > 0 ) cmd->buttons |= IN_MOVERIGHT;
	if( span->side < 0 ) cmd->buttons |= IN_MOVELEFT;
	vec3_t angles( span->pitch, span->yaw, 0.0f );
	gEngfuncs.SetViewAngles( angles );
	VectorCopy( angles, cmd->viewangles );
	cmd->impact_position[0] = (float)frame;
	frame++;
}
