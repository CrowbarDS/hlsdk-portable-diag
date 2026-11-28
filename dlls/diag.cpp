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
#include "monsters.h"
#include "decals.h"

#include <stdio.h>

extern DLL_GLOBAL int g_iSkillLevel;

static cvar_t diag_log = { "diag_log", "" };

static FILE *s_log;
static int s_frame;           // the script's frame now running (1-based), 0 when none
static unsigned int s_serverFrame;
static int s_seq;              // the commands run since the script began

// The monsters' starts, from the map's load on (written when the log opens).
struct DiagStart
{
	int id;
	char cls[32];
	Vector at;
	float start, first; // when it started; its first think
};
static DiagStart s_starts[256];
static int s_startCount;
// The monster thinking now (its random draws are its own), or none.
static CBaseMonster *s_monster;
static int ( *s_randomLong )( int, int );
static float ( *s_randomFloat )( float, float );

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
	// How hard the game is (1 easy .. 3 hard): the replay plays at the same.
	fprintf( s_log, "#skill %d\n", g_iSkillLevel );
	// frame: the script's frame (the client's); seq: the command, from 1 (a
	// slow frame is run as more than one command); t: the server's time.
	fprintf( s_log, "#cols cmd frame seq t sframe msec fwd side up pitch yaw buttons impulse seed\n" );
	// mt: the movetype (3 walk, 5 fly: on a ladder); lad: the ladder's model,
	// or "-"; lamp: the flashlight on (EF_DIMLIGHT); bat: its battery.
	fprintf( s_log, "#cols pl frame seq t ox oy oz vx vy vz pitch yaw punchp punchy og duck ground water mt lad lamp bat\n" );
	// The flashlight, while it is on: where the client's light would be
	// (CL_UpdateFlashlight: a line of 2000 from the eye along the view; the
	// light at its end, or at the origin of a model it stops on). Traced
	// here on the server, so a monster is met as its box, not its hit
	// boxes. dist: how far the line went; hit: what stopped it; lx..lz:
	// where the light sits.
	fprintf( s_log, "#cols lamp frame seq t ex ey ez dist hit lx ly lz\n" );
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
	// A mark (a decal the server sends): where its trace ended, the wall's
	// normal, its family (the decal's name without its "{" and number:
	// blood, yblood, shot, bigshot, scorch, ...), its name, and what it is on.
	fprintf( s_log, "#cols decal frame seq t x y z nx ny nz family name on\n" );
	// A turning brush (func_rotating), before the frame's physics: its
	// angles and how fast each turns (degrees, degrees a second).
	fprintf( s_log, "#cols rot frame seq t id ax ay az wx wy wz\n" );
	// The player hurt: amount and type asked, by what from where, the
	// velocity before and after (the push), health and armour before and after.
	fprintf( s_log, "#cols dmg frame seq t amount type by fx fy fz vx0 vy0 vz0 vx vy vz hp0 hp ap0 ap\n" );
	// A monster's start (StartMonster): its entity, class, where it stood
	// then, when that was and when its first think was set for (the spread).
	fprintf( s_log, "#cols mstart frame seq t id cls sx sy sz start first\n" );
	// A monster at the end of its think: where, which way, its state and the
	// one it should be in, schedule (name), task (index, id), conditions
	// (before the schedule's own changes clear them: as RunAI left them),
	// enemy (entity, - none), activity, sequence, health, the next think.
	fprintf( s_log, "#cols mon frame seq t id ox oy oz yaw state ideal sched ti tid cond enemy act seq hp next\n" );
	// Something its AI did: sched (name; a the conditions that ended the
	// last), task (a its id), fail (a the task's id), anim (a the event),
	// route/cover/near (a the route's first step's type, b the goal; name
	// "x y z>x y z" that step, and the route's goal), look (Move's look ahead
	// refused: a LOCALMOVE_*, b how far it got; name "x y z>z>x y z" where it
	// looked to, the height its walk reached there, the waypoint).
	fprintf( s_log, "#cols aiev frame seq t id what name a b\n" );
	// A random draw of the engine's while a monster thought: L whole, F float.
	fprintf( s_log, "#cols rnd frame seq t id kind lo hi value\n" );
	for( int i = 0; i < s_startCount; i++ )
	{
		const DiagStart &st = s_starts[i];
		fprintf( s_log, "mstart,0,0,%.4f,%d,%s,%.3f,%.3f,%.3f,%.4f,%.4f\n", gpGlobals->time, st.id, st.cls,
			st.at.x, st.at.y, st.at.z, st.start, st.first );
	}
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

static int MonsterId( CBaseMonster *m )
{
	return m ? ENTINDEX( m->edict()) : -1;
}

static int DiagRandomLong( int low, int high )
{
	int value = s_randomLong( low, high );
	if( s_monster && Diag_On())
		fprintf( s_log, "rnd,%d,%d,%.4f,%d,L,%d,%d,%d\n", s_frame, s_seq, gpGlobals->time, MonsterId( s_monster ),
			low, high, value );
	return value;
}

static float DiagRandomFloat( float low, float high )
{
	float value = s_randomFloat( low, high );
	if( s_monster && Diag_On())
		fprintf( s_log, "rnd,%d,%d,%.4f,%d,F,%.6f,%.6f,%.6f\n", s_frame, s_seq, gpGlobals->time,
			MonsterId( s_monster ), low, high, value );
	return value;
}

void Diag_Init( void )
{
	CVAR_REGISTER( &diag_log );
	// (The game library's own copy of the engine's table: what RANDOM_LONG
	// and RANDOM_FLOAT call, enginecallback.h.)
	s_randomLong = g_engfuncs.pfnRandomLong;
	s_randomFloat = g_engfuncs.pfnRandomFloat;
	g_engfuncs.pfnRandomLong = DiagRandomLong;
	g_engfuncs.pfnRandomFloat = DiagRandomFloat;
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
	fprintf( s_log, "pl,%d,%d,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.4f,%d,%d,%s,%d,%d,%s,%d,%d\n",
		s_frame, s_seq, gpGlobals->time,
		pev->origin.x, pev->origin.y, pev->origin.z,
		pev->velocity.x, pev->velocity.y, pev->velocity.z,
		pev->v_angle.x, pev->v_angle.y, pev->punchangle.x, pev->punchangle.y,
		( pev->flags & FL_ONGROUND ) ? 1 : 0, ( pev->flags & FL_DUCKING ) ? 1 : 0,
		( pev->flags & FL_ONGROUND ) ? ModelName( pev->groundentity ) : "-",
		pev->waterlevel, pev->movetype, ladder, ( pev->effects & EF_DIMLIGHT ) ? 1 : 0, player->m_iFlashBattery );
	if( pev->effects & EF_DIMLIGHT )
	{
		UTIL_MakeVectors( pev->v_angle );
		const Vector eye = pev->origin + pev->view_ofs;
		TraceResult tr;
		UTIL_TraceLine( eye, eye + gpGlobals->v_forward * 2000.0f, dont_ignore_monsters, player->edict(), &tr );
		Vector light = tr.vecEndPos;
		CBaseEntity *hit = tr.flFraction < 1.0f && tr.pHit ? CBaseEntity::Instance( tr.pHit ) : NULL;
		if( hit && hit->pev->modelindex && hit->MyMonsterPointer())
			light = hit->pev->origin;
		fprintf( s_log, "lamp,%d,%d,%.4f,%.3f,%.3f,%.3f,%.3f,%s,%.3f,%.3f,%.3f\n", s_frame, s_seq, gpGlobals->time,
			tr.vecEndPos.x, tr.vecEndPos.y, tr.vecEndPos.z, tr.flFraction * 2000.0f,
			tr.flFraction < 1.0f ? ModelName( tr.pHit ) : "-", light.x, light.y, light.z );
	}
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
	ent = NULL;
	while(( ent = UTIL_FindEntityByClassname( ent, "func_rotating" )) != NULL )
	{
		entvars_t *pev = ent->pev;
		fprintf( s_log, "rot,%d,%d,%.4f,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
			s_frame, s_seq, gpGlobals->time, STRING( pev->model ),
			pev->angles.x, pev->angles.y, pev->angles.z, pev->avelocity.x, pev->avelocity.y, pev->avelocity.z );
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

void Diag_Decal( const TraceResult *trace, int decalNumber )
{
	if( !Diag_On() || decalNumber < 0 )
		return;
	const char *name = gDecals[decalNumber].name;
	char family[32];
	int n = 0;
	for( const char *c = name; *c && n < (int)sizeof( family ) - 1; ++c )
	{
		if( *c != '{' && ( *c < '0' || *c > '9' ))
			family[n++] = *c;
	}
	family[n] = 0;
	fprintf( s_log, "decal,%d,%d,%.4f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%s,%s,%s\n",
		s_frame, s_seq, gpGlobals->time, trace->vecEndPos.x, trace->vecEndPos.y, trace->vecEndPos.z,
		trace->vecPlaneNormal.x, trace->vecPlaneNormal.y, trace->vecPlaneNormal.z, family, name,
		ModelName( trace->pHit ));
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

void Diag_MonsterStart( CBaseMonster *monster )
{
	const int id = MonsterId( monster );
	int i = 0;
	while( i < s_startCount && s_starts[i].id != id )
		i++;
	if( i == s_startCount )
	{
		if( s_startCount == (int)( sizeof( s_starts ) / sizeof( s_starts[0] )))
			return;
		s_startCount++;
	}
	DiagStart &st = s_starts[i];
	st.id = id;
	snprintf( st.cls, sizeof( st.cls ), "%s", STRING( monster->pev->classname ));
	st.at = monster->pev->origin;
	st.start = gpGlobals->time;
	st.first = monster->pev->nextthink;
	if( Diag_On())
		fprintf( s_log, "mstart,%d,%d,%.4f,%d,%s,%.3f,%.3f,%.3f,%.4f,%.4f\n", s_frame, s_seq, gpGlobals->time, id,
			st.cls, st.at.x, st.at.y, st.at.z, st.start, st.first );
}

void Diag_MonsterThink( CBaseMonster *monster )
{
	if( !Diag_On())
		return;
	entvars_t *pev = monster->pev;
	Schedule_t *sched = monster->m_pSchedule;
	Task_t *task = sched && monster->m_iScheduleIndex < sched->cTasks ? &sched->pTasklist[monster->m_iScheduleIndex] : NULL;
	CBaseEntity *enemy = monster->m_hEnemy;
	fprintf( s_log, "mon,%d,%d,%.4f,%d,%.3f,%.3f,%.3f,%.3f,%d,%d,%s,%d,%d,%08x,%d,%d,%d,%.2f,%.4f\n",
		s_frame, s_seq, gpGlobals->time, MonsterId( monster ), pev->origin.x, pev->origin.y, pev->origin.z,
		pev->angles.y, monster->m_MonsterState, monster->m_IdealMonsterState, sched ? sched->pName : "-",
		monster->m_iScheduleIndex, task ? task->iTask : -1, monster->m_afConditions,
		enemy ? ENTINDEX( enemy->edict()) : -1, monster->m_Activity, pev->sequence, pev->health, pev->nextthink );
}

void Diag_LookAhead( CBaseMonster *monster, const Vector &end, float reachedZ, const Vector &waypoint, int result,
	float dist )
{
	if( !Diag_On())
		return;
	char at[160];
	snprintf( at, sizeof( at ), "%.1f %.1f %.1f>%.1f>%.1f %.1f %.1f", end.x, end.y, end.z, reachedZ, waypoint.x,
		waypoint.y, waypoint.z );
	Diag_AiEvent( monster, "look", at, result, (int)dist );
}

void Diag_AiEvent( CBaseMonster *monster, const char *what, const char *name, int a, int b )
{
	if( !Diag_On())
		return;
	fprintf( s_log, "aiev,%d,%d,%.4f,%d,%s,%s,%d,%d\n", s_frame, s_seq, gpGlobals->time, MonsterId( monster ),
		what, name && name[0] ? name : "-", a, b );
}

DiagMonsterScope::DiagMonsterScope( CBaseMonster *monster ) : previous( s_monster )
{
	s_monster = monster;
}

DiagMonsterScope::~DiagMonsterScope()
{
	s_monster = previous;
}

DiagRouteScope::~DiagRouteScope()
{
	if( !Diag_On())
		return;
	const WayPoint_t &step = monster->m_Route[monster->m_iRouteIndex];
	// (Its first step, and the one it is all for: the route's goal.)
	const WayPoint_t *goal = &step;
	for( int i = monster->m_iRouteIndex; i < ROUTE_SIZE; i++ )
	{
		goal = &monster->m_Route[i];
		if( goal->iType & bits_MF_IS_GOAL )
			break;
	}
	char at[128];
	snprintf( at, sizeof( at ), "%.1f %.1f %.1f>%.1f %.1f %.1f", step.vecLocation.x, step.vecLocation.y,
		step.vecLocation.z, goal->vecLocation.x, goal->vecLocation.y, goal->vecLocation.z );
	Diag_AiEvent( monster, what, at, step.iType, monster->m_movementGoal );
	// (... and every step of it, to the goal: "x y z/type" each, ';' between,
	// for the port's to be held to by place -- a route by the nodes has
	// bits_MF_TO_NODE in its steps' types.)
	if( !step.iType )
		return;
	char path[512];
	int used = 0, steps = 0;
	path[0] = 0;
	for( int i = monster->m_iRouteIndex; i < ROUTE_SIZE && used < (int)sizeof( path ) - 48; i++ )
	{
		const WayPoint_t &w = monster->m_Route[i];
		if( !w.iType )
			break;
		used += snprintf( path + used, sizeof( path ) - used, "%s%.1f %.1f %.1f/%d", steps ? ";" : "",
			w.vecLocation.x, w.vecLocation.y, w.vecLocation.z, w.iType );
		steps++;
		if( w.iType & bits_MF_IS_GOAL )
			break;
	}
	Diag_AiEvent( monster, "path", path, steps, monster->m_movementGoal );
}
