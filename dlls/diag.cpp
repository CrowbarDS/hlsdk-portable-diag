// Diagnostic log of the game's physics: see diag.h.
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "usercmd.h"
#include "diag.h"
#include "pm_shared.h"
#include "weapons.h"
#include "hornet.h"

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
	// mt: the movetype (3 walk, 5 fly: on a ladder); lad: the ladder's model,
	// or "-".
	fprintf( s_log, "#cols pl frame seq t ox oy oz vx vy vz pitch yaw punchp punchy og duck ground water mt lad\n" );
	fprintf( s_log, "#cols phys frame seq t sframe ft\n" );
	fprintf( s_log, "#cols box frame seq t id cx cy cz vx vy vz og ground water\n" );
	fprintf( s_log, "#cols touch frame seq t id who push result tvx tvy bvx0 bvy0 bvx bvy\n" );
	// The ladder the move was on (pm_shared.c's pm_diag_ladder): hit, its
	// trace found it; frac, nx..nz, what that trace found; fwd/right, the
	// climb speeds asked for.
	fprintf( s_log, "#cols lad frame seq t id hit frac nx ny nz onfloor fwd right\n" );
	// A console command the server ran (a scenario's "cmd" line), as it ran.
	fprintf( s_log, "#cols ccmd frame seq t text\n" );
	// The weapon in hand after the player's think: its class, clip, the ammo
	// carried (its first and second kinds), the times until it may fire
	// again and idle (as the game keeps them: from 0 with its own client's
	// weapons), and a reload under way.
	fprintf( s_log, "#cols wpn frame seq t cls clip ammo1 ammo2 next1 next2 idle reload\n" );
	// What the player has thrown or fired, before the frame's physics moves
	// it (as the boxes): id its entity, cls its class, sub how it goes
	// (a grenade: timed, contact or slide; a hornet: track or dart), mt its
	// movetype, og on the ground.
	fprintf( s_log, "#cols proj frame seq t id cls sub ox oy oz vx vy vz mt og water\n" );
	// One bullet: n which of the shot's, src where from, ax..az the way aimed
	// (before the spread), x y the spread drawn (each the sum of two shared
	// draws, -1..1), cx cy the weapon's cone (the way goes off by x * cx of
	// the view's right and y * cy of its up), seed the command's, ex..ez
	// where its trace ended, frac how far, hit what it hit.
	fprintf( s_log, "#cols shot frame seq t n seed sx sy sz ax ay az x y cx cy ex ey ez frac hit\n" );
	// A blast (RadiusDamage), at x y z as given.
	fprintf( s_log, "#cols boom frame seq t x y z damage radius type by\n" );
	// The player hurt: amount and type asked, by what from where, the
	// velocity before and after (the push), health and armour before and after.
	fprintf( s_log, "#cols dmg frame seq t amount type by fx fy fz vx0 vy0 vz0 vx vy vz hp0 hp ap0 ap\n" );
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
	const char *ladder = "-";
	if( pm_diag_ladder.on )
		ladder = ModelName( INDEXENT( pm_diag_ladder.ent ));
	fprintf( s_log, "pl,%d,%d,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.4f,%d,%d,%s,%d,%d,%s\n",
		s_frame, s_seq, gpGlobals->time,
		pev->origin.x, pev->origin.y, pev->origin.z,
		pev->velocity.x, pev->velocity.y, pev->velocity.z,
		pev->v_angle.x, pev->v_angle.y, pev->punchangle.x, pev->punchangle.y,
		( pev->flags & FL_ONGROUND ) ? 1 : 0, ( pev->flags & FL_DUCKING ) ? 1 : 0,
		( pev->flags & FL_ONGROUND ) ? ModelName( pev->groundentity ) : "-",
		pev->waterlevel, pev->movetype, ladder );
	if( pm_diag_ladder.on )
		fprintf( s_log, "lad,%d,%d,%.4f,%s,%d,%.4f,%.4f,%.4f,%.4f,%d,%.1f,%.1f\n",
			s_frame, s_seq, gpGlobals->time, ladder, pm_diag_ladder.hit, pm_diag_ladder.frac,
			pm_diag_ladder.normal[0], pm_diag_ladder.normal[1], pm_diag_ladder.normal[2],
			pm_diag_ladder.onfloor, pm_diag_ladder.fwd, pm_diag_ladder.right );
	CBasePlayerWeapon *weapon = player->m_pActiveItem ? (CBasePlayerWeapon *)player->m_pActiveItem->GetWeaponPtr() : NULL;
	if( weapon )
	{
		int ammo1 = weapon->m_iPrimaryAmmoType >= 0 ? player->m_rgAmmo[weapon->m_iPrimaryAmmoType] : -1;
		int ammo2 = weapon->m_iSecondaryAmmoType >= 0 ? player->m_rgAmmo[weapon->m_iSecondaryAmmoType] : -1;
		fprintf( s_log, "wpn,%d,%d,%.4f,%s,%d,%d,%d,%.4f,%.4f,%.4f,%d\n",
			s_frame, s_seq, gpGlobals->time, STRING( weapon->pev->classname ), weapon->m_iClip, ammo1, ammo2,
			weapon->m_flNextPrimaryAttack, weapon->m_flNextSecondaryAttack, weapon->m_flTimeWeaponIdle,
			weapon->m_fInReload );
	}
}

void Diag_ClientCommand( const char *cmd, const char *args )
{
	if( !Diag_On())
		return;
	// (No commas in the row: they would be columns.)
	char text[160];
	snprintf( text, sizeof( text ), "%s %s", cmd, args ? args : "" );
	for( char *c = text; *c; ++c )
	{
		if( *c == ',' || *c == '\n' || *c == '\r' )
			*c = ' ';
	}
	fprintf( s_log, "ccmd,%d,%d,%.4f,%s\n", s_frame, s_seq, gpGlobals->time, text );
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
	static const char *thrown[] = { "grenade", "crossbow_bolt", "rpg_rocket", "hornet", "monster_satchel",
		"monster_snark", "monster_tripmine" };
	for( unsigned k = 0; k < sizeof( thrown ) / sizeof( thrown[0] ); k++ )
	{
		ent = NULL;
		while(( ent = UTIL_FindEntityByClassname( ent, thrown[k] )) != NULL )
		{
			entvars_t *pev = ent->pev;
			if( pev->flags & FL_KILLME )
				continue;
			const char *sub = "-";
			typedef void ( CBaseEntity::*Touch )( CBaseEntity * );
			typedef void ( CBaseEntity::*Think )( void );
			if( k == 0 )
			{
				if( ent->m_pfnTouch == static_cast<Touch>( &CGrenade::ExplodeTouch ))
					sub = "contact";
				else if( ent->m_pfnTouch == static_cast<Touch>( &CGrenade::BounceTouch ))
					sub = "timed";
				else if( ent->m_pfnTouch == static_cast<Touch>( &CGrenade::SlideTouch ))
					sub = "slide";
			}
			else if( k == 3 )
			{
				const bool dart = ent->m_pfnThink == static_cast<Think>( &CHornet::StartDart ) ||
					ent->m_pfnTouch == static_cast<Touch>( &CHornet::DartTouch );
				sub = dart ? "dart" : "track";
			}
			fprintf( s_log, "proj,%d,%d,%.4f,%d,%s,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%d\n",
				s_frame, s_seq, gpGlobals->time, ENTINDEX( ent->edict()), thrown[k], sub,
				pev->origin.x, pev->origin.y, pev->origin.z, pev->velocity.x, pev->velocity.y, pev->velocity.z,
				pev->movetype, ( pev->flags & FL_ONGROUND ) ? 1 : 0, pev->waterlevel );
		}
	}
}

void Diag_Shot( const Vector &src, const Vector &aim, float x, float y, const Vector &spread, int seed, int n,
	const Vector &end, float frac, edict_t *hit )
{
	if( !Diag_On())
		return;
	fprintf( s_log, "shot,%d,%d,%.4f,%d,%d,%.3f,%.3f,%.3f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.3f,%.3f,%.3f,%.5f,%s\n",
		s_frame, s_seq, gpGlobals->time, n, seed, src.x, src.y, src.z, aim.x, aim.y, aim.z, x, y, spread.x, spread.y,
		end.x, end.y, end.z, frac, frac < 1.0f ? ModelName( hit ) : "-" );
}

void Diag_RadiusDamage( const Vector &src, entvars_t *inflictor, float damage, float radius, int type )
{
	if( !Diag_On())
		return;
	fprintf( s_log, "boom,%d,%d,%.4f,%.3f,%.3f,%.3f,%.2f,%.2f,%d,%s\n",
		s_frame, s_seq, gpGlobals->time, src.x, src.y, src.z, damage, radius, type,
		inflictor ? STRING( inflictor->classname ) : "-" );
}

DiagHurtScope::DiagHurtScope( entvars_t *p, entvars_t *i, float a, int t )
	: player( p ), inflictor( i ), amount( a ), type( t ), velocity( p->velocity ), health( p->health ),
	  armor( p->armorvalue )
{
}

DiagHurtScope::~DiagHurtScope()
{
	if( !Diag_On())
		return;
	Vector from = inflictor ? inflictor->origin : Vector( 0, 0, 0 );
	fprintf( s_log, "dmg,%d,%d,%.4f,%.2f,%d,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%.2f\n",
		s_frame, s_seq, gpGlobals->time, amount, type, inflictor ? STRING( inflictor->classname ) : "-",
		from.x, from.y, from.z, velocity.x, velocity.y, velocity.z,
		player->velocity.x, player->velocity.y, player->velocity.z, health, player->health, armor,
		player->armorvalue );
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
