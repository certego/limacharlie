/*
Copyright 2015 refractionPOINT

Licensed under the Apache License, Version 2.0 ( the "License" );
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http ://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "atoms.h"
#include <cryptoLib/cryptoLib.h>

#define _CLEANUP_EVERY  50
#define _ATOM_GRACE_MS  10000

static rBTree g_atoms = NULL;
static RU32 g_nextCleanup = _CLEANUP_EVERY;

static RS32
    _compareAtomKeys
    (
        Atom* atom1,
        Atom* atom2
    )
{
    RS32 ret = 0;

    if( NULL != atom1 && NULL != atom2 )
    {
        ret = rpal_memory_memcmp( &(atom1->key), &(atom2->key), sizeof( atom1->key ) );
    }

    return ret;
}

static RVOID
    _freeAtom
    (
        Atom* atom
    )
{
    rpal_memory_zero( atom, sizeof( *atom ) );
}

RBOOL
    atoms_init
    (

    )
{
    RBOOL isSuccess = FALSE;

    if( NULL != ( g_atoms = rpal_btree_create( sizeof( Atom ), _compareAtomKeys, _freeAtom ) ) )
    {
        isSuccess = TRUE;
    }

    return isSuccess;
}

RBOOL
    atoms_deinit
    (

    )
{
    RBOOL isSuccess = FALSE;

    if( NULL != g_atoms )
    {
        rpal_btree_destroy( g_atoms, FALSE );
        isSuccess = TRUE;
    }

    return isSuccess;
}

RBOOL
    atoms_register
    (
        Atom* pAtom
    )
{
    RBOOL isSuccess = FALSE;

    if( NULL != pAtom )
    {
        if( CryptoLib_genRandomBytes( pAtom->id, sizeof( pAtom->id ) ) )
        {
            isSuccess = rpal_btree_add( g_atoms, pAtom, FALSE );
            if( !isSuccess )
            {
                isSuccess = rpal_btree_update( g_atoms, pAtom, pAtom, FALSE );
            }
        }

        if( !isSuccess )
        {
            rpal_debug_error( "could not register atom" );
        }
    }

    return isSuccess;
}

RBOOL
    atoms_query
    (
        Atom* pAtom,
        RU64 atTime
    )
{
    RBOOL isSuccess = FALSE;

    if( NULL != pAtom )
    {
        isSuccess = rpal_btree_search( g_atoms, pAtom, pAtom, FALSE );
        if( !isSuccess )
        {
            rpal_debug_warning( "atom not found" );
        }
        else if( 0 != atTime &&
                 0 != pAtom->expiredOn &&
                 atTime > pAtom->expiredOn )
        {
            rpal_memory_zero( pAtom->id, sizeof( pAtom->id ) );
        }
    }

    return isSuccess;
}

RBOOL
    atoms_remove
    (
        Atom* pAtom,
        RU64 expiredOn
    )
{
    RBOOL isSuccess = FALSE;
    Atom tmpAtom = { 0 };
    Atom nextAtom = { 0 };
    RU64 curTime = 0;

    if( NULL != pAtom )
    {
        pAtom->expiredOn = expiredOn;
        isSuccess = rpal_btree_update( g_atoms, pAtom, pAtom, FALSE );
        if( !isSuccess )
        {
            rpal_debug_error( "atom not found" );
        }

        if( 0 == g_nextCleanup )
        {
            g_nextCleanup = _CLEANUP_EVERY;

            if( rpal_btree_minimum( g_atoms, &tmpAtom, FALSE ) )
            {
                curTime = rpal_time_getGlobalPreciseTime();

                while( rpal_btree_next( g_atoms, &tmpAtom, &nextAtom, FALSE ) )
                {
                    if( 0 != tmpAtom.expiredOn &&
                        curTime > tmpAtom.expiredOn + _ATOM_GRACE_MS )
                    {
                        rpal_btree_remove( g_atoms, &tmpAtom, NULL, FALSE );
                    }

                    tmpAtom = nextAtom;
                }

                if( 0 != tmpAtom.expiredOn &&
                    curTime > tmpAtom.expiredOn + _ATOM_GRACE_MS )
                {
                    rpal_btree_remove( g_atoms, &tmpAtom, NULL, FALSE );
                }
            }

            rpal_debug_info( "atom cleanup finished, %d left", rpal_btree_getSize( g_atoms, FALSE ) );
        }
        else
        {
            g_nextCleanup--;
        }
    }

    return isSuccess;
}

RBOOL
    atoms_getOneTime
    (
        Atom* pAtom
    )
{
    RBOOL isSuccess = FALSE;

    if( NULL != pAtom )
    {
        isSuccess = CryptoLib_genRandomBytes( pAtom->id, sizeof( pAtom->id ) );
    }

    return isSuccess;
}
