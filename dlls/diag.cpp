// Diagnostic log of the game's physics: see diag.h.
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "usercmd.h"
#include "diag.h"

#include <stdio.h>

static cvar_t diag_log = { "diag_log", "" };

static FILE *s_log;
static int s_frame;           // the script's frame now running (1-based), 0 when none
static unsigned int s_serverFrame;
static int s_seq;              // the commands run since the script began

static const char *ModelName( edict_t *ent )
{
	if( !ent )
		return "-";
	if( ENTINDEX( ent ) == 0 )
		return "world";
	if( ent->v.model )
		return STRING( ent->v.model );
	return STRING( ent->v.classname );
}

static void Open( void )
{
	char path[256];
	char dir[128];
	GET_GAME_DIR( dir );
	snprintf( path, sizeof( path ), "%s/diag/%s.csv", dir, diag_log.string );
	s_log = fopen( path, "w" );
	if( !s_log )
	{
		g_engfuncs.pfnServerPrint( "DIAG: cannot write the log\n" );
		return;
	}
	fprintf( s_log, "#diag pc 1\n" );
	fprintf( s_log, "#map %s\n", STRING( gpGlobals->mapname ));
	// frame: the script's frame (the client's); seq: the command, from 1 (a
	// slow frame is run as more than one command); t: the server's time.
	fprintf( s_log, "#cols cmd frame seq t sframe msec fwd side up pitch yaw buttons impulse seed\n" );
	fprintf( s_log, "#cols pl frame seq t ox oy oz vx vy vz pitch yaw punchp punchy og duck ground water\n" );
	fprintf( s_log, "#cols phys frame seq t sframe ft\n" );
	fprintf( s_log, "#cols box frame seq t id cx cy cz vx vy vz og ground water\n" );
	fprintf( s_log, "#cols touch frame seq t id who push result tvx tvy bvx0 bvy0 bvx bvy\n" );
	s_seq = 0;
	g_engfuncs.pfnServerPrint( "DIAG: log open\n" );
}

static void Close( void )
{
	if( !s_log )
		return;
	fprintf( s_log, "END,%d,%d,%.4f\n", s_frame, s_seq, gpGlobals->time );
	fclose( s_log );
	s_log = NULL;
	g_engfuncs.pfnServerPrint( "DIAG: log closed\n" );
}

void Diag_Init( void )
{
	CVAR_REGISTER( &diag_log );
	g_engfuncs.pfnServerPrint( "DIAG: diagnostic server library\n" );
}

bool Diag_On( void )
{
	return s_log != NULL && s_frame > 0;
}

void Diag_CmdStart( CBasePlayer *player, const usercmd_t *cmd, unsigned int seed )
{
	int frame = (int)( cmd->impact_position[0] + 0.5f );
	if( frame > 0 && !s_log && diag_log.string[0] )
		Open();
	if( frame <= 0 )
	{
		// The script is over (or never ran): its last frame has been logged.
		if( s_log && s_frame > 0 )
			Close();
		s_frame = 0;
		return;
	}
	s_frame = frame;
	if( !s_log )
		return;
	s_seq++;
	fprintf( s_log, "cmd,%d,%d,%.4f,%u,%d,%.1f,%.1f,%.1f,%.4f,%.4f,%u,%d,%u\n",
		frame, s_seq, gpGlobals->time, s_serverFrame, cmd->msec,
		cmd->forwardmove, cmd->sidemove, cmd->upmove,
		cmd->viewangles[0], cmd->viewangles[1],
		cmd->buttons, cmd->impulse, seed );
}

void Diag_PlayerPostThink( CBasePlayer *player )
{
	if( !Diag_On())
		return;
	entvars_t *pev = player->pev;
	fprintf( s_log, "pl,%d,%d,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.4f,%d,%d,%s,%d\n",
		s_frame, s_seq, gpGlobals->time,
		pev->origin.x, pev->origin.y, pev->origin.z,
		pev->velocity.x, pev->velocity.y, pev->velocity.z,
		pev->v_angle.x, pev->v_angle.y, pev->punchangle.x, pev->punchangle.y,
		( pev->flags & FL_ONGROUND ) ? 1 : 0, ( pev->flags & FL_DUCKING ) ? 1 : 0,
		( pev->flags & FL_ONGROUND ) ? ModelName( pev->groundentity ) : "-",
		pev->waterlevel );
}

// Before the frame's physics: the boxes as the last frame's physics left them,
// with the velocity this frame's touches gave them.
void Diag_StartFrame( void )
{
	s_serverFrame++;
	if( !Diag_On())
		return;
	// The physics about to run, over ft seconds.
	fprintf( s_log, "phys,%d,%d,%.4f,%u,%.5f\n", s_frame, s_seq, gpGlobals->time, s_serverFrame, gpGlobals->frametime );
	CBaseEntity *ent = NULL;
	while(( ent = UTIL_FindEntityByClassname( ent, "func_pushable" )) != NULL )
	{
		entvars_t *pev = ent->pev;
		Vector c = pev->origin + ( pev->mins + pev->maxs ) * 0.5f;
		fprintf( s_log, "box,%d,%d,%.4f,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%s,%d\n",
			s_frame, s_seq, gpGlobals->time, STRING( pev->model ),
			c.x, c.y, c.z, pev->velocity.x, pev->velocity.y, pev->velocity.z,
			( pev->flags & FL_ONGROUND ) ? 1 : 0,
			( pev->flags & FL_ONGROUND ) ? ModelName( pev->groundentity ) : "-",
			pev->waterlevel );
	}
}

DiagPushScope::DiagPushScope( entvars_t *b, entvars_t *t, int p )
	: box( b ), toucher( t ), push( p ), before( b->velocity ), result( DIAG_PUSH_OK )
{
}

DiagPushScope::~DiagPushScope()
{
	if( !Diag_On())
		return;
	static const char *names[] = { "ok", "ride", "buttons", "air" };
	const char *who = ( toucher->flags & FL_CLIENT ) ? "player" : STRING( toucher->classname );
	fprintf( s_log, "touch,%d,%d,%.4f,%s,%s,%d,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
		s_frame, s_seq, gpGlobals->time, STRING( box->model ), who, push, names[result],
		toucher->velocity.x, toucher->velocity.y, before.x, before.y,
		box->velocity.x, box->velocity.y );
}
