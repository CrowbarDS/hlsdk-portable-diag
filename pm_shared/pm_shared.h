/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#pragma once
#if !defined(PM_SHARED_H)
#define PM_SHARED_H

char *PM_memfgets( byte *pMemFile, int fileSize, int *pFilePos, char *pBuffer, int bufferSize );
void PM_Init( struct playermove_s *ppmove );
void PM_Move( struct playermove_s *ppmove, int server );
char PM_FindTextureType( char *name );

// DIAG: what the server's last player move made of a ladder (dlls/diag.cpp
// logs it; the client's prediction does not fill it). on: a ladder was
// touched this move; hit: its trace from the origin to its centre found it
// (the move went by it), with the normal and fraction found; onfloor: solid
// under the feet; fwd/right: the climb speeds the buttons asked for.
typedef struct pm_diag_ladder_s
{
	int on, ent, hit, onfloor;
	float frac, normal[3], fwd, right;
} pm_diag_ladder_t;
#ifdef __cplusplus
extern "C" pm_diag_ladder_t pm_diag_ladder;
#else
extern pm_diag_ladder_t pm_diag_ladder;
#endif

// Spectator Movement modes (stored in pev->iuser1, so the physics code can get at them)
#define OBS_NONE			0
#define OBS_CHASE_LOCKED		1
#define OBS_CHASE_FREE			2
#define OBS_ROAMING			3		
#define OBS_IN_EYE			4
#define OBS_MAP_FREE			5
#define OBS_MAP_CHASE			6
#endif//PM_SHARED_H
