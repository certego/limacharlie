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

#include <rpal/rpal.h>
#include <librpcm/librpcm.h>
#include "collectors.h"
#include <notificationsLib/notificationsLib.h>
#include <libOs/libOs.h>
#include <processLib/processLib.h>
#include <rpHostCommonPlatformLib/rTags.h>

#define RPAL_FILE_ID        77

#define _FULL_SNAPSHOT_DEFAULT_DELTA        (60*60*24)

static
RVOID
    os_services
    (
        rpcm_tag eventType,
        rSequence event
    )
{
    rList svcList;

    UNREFERENCED_PARAMETER( eventType );

    if( rpal_memory_isValid( event ) )
    {
        if( NULL != ( svcList = libOs_getServices( TRUE ) ) )
        {
            if( !rSequence_addLIST( event, RP_TAGS_SVCS, svcList ) )
            {
                rList_free( svcList );
            }
        }
        else
        {
            rSequence_addRU32( event, RP_TAGS_ERROR, rpal_error_getLast() );
        }

        rSequence_addTIMESTAMP( event, RP_TAGS_TIMESTAMP, rpal_time_getGlobal() );
        notifications_publish( RP_TAGS_NOTIFICATION_OS_SERVICES_REP, event );
    }
}


static
RVOID
    os_drivers
    (
        rpcm_tag eventType,
        rSequence event
    )
{
    rList svcList;

    UNREFERENCED_PARAMETER( eventType );

    if( rpal_memory_isValid( event ) )
    {
        if( NULL != ( svcList = libOs_getDrivers( TRUE ) ) )
        {
            if( !rSequence_addLIST( event, RP_TAGS_SVCS, svcList ) )
            {
                rList_free( svcList );
            }
        }
        else
        {
            rSequence_addRU32( event, RP_TAGS_ERROR, rpal_error_getLast() );
        }

        rSequence_addTIMESTAMP( event, RP_TAGS_TIMESTAMP, rpal_time_getGlobal() );
        notifications_publish( RP_TAGS_NOTIFICATION_OS_DRIVERS_REP, event );
    }
}


static
RVOID
    os_processes
    (
        rpcm_tag eventType,
        rSequence event
    )
{
    rList procList = NULL;
    rSequence proc = NULL;
    rList mods = NULL;
    processLibProcEntry* entries = NULL;
    RU32 entryIndex = 0;

    UNREFERENCED_PARAMETER( eventType );

    if( rpal_memory_isValid( event ) &&
        rSequence_addTIMESTAMP( event, RP_TAGS_TIMESTAMP, rpal_time_getGlobal() ) &&
        NULL != ( procList = rList_new( RP_TAGS_PROCESS, RPCM_SEQUENCE ) ) &&
        rSequence_addLIST( event, RP_TAGS_PROCESSES, procList ) )
    {
        entries = processLib_getProcessEntries( TRUE );

        while( NULL != entries && 0 != entries[ entryIndex ].pid )
        {
            if( NULL != ( proc = processLib_getProcessInfo( entries[ entryIndex ].pid, NULL ) ) )
            {
                if( NULL != ( mods = processLib_getProcessModules( entries[ entryIndex ].pid ) ) )
                {
                    if( !rSequence_addLIST( proc, RP_TAGS_MODULES, mods ) )
                    {
                        rList_free( mods );
                        mods = NULL;
                    }
                }

                if( !rList_addSEQUENCE( procList, proc ) )
                {
                    rSequence_free( proc );
                    proc = NULL;
                }
            }

            entryIndex++;

            proc = NULL;
            mods = NULL;
        }

        rpal_memory_free( entries );

        notifications_publish( RP_TAGS_NOTIFICATION_OS_PROCESSES_REP, event );
    }
    else
    {
        rList_free( procList );
    }
}

static
RVOID
    os_autoruns
    (
        rpcm_tag eventType,
        rSequence event
    )
{
    rList autoruns = NULL;

    UNREFERENCED_PARAMETER( eventType );

    if( rpal_memory_isValid( event ) )
    {
        if( rpal_memory_isValid( event ) )
        {

            if( NULL != ( autoruns = libOs_getAutoruns( TRUE ) ) )
            {
                if( !rSequence_addLIST( event, RP_TAGS_AUTORUNS, autoruns ) )
                {
                    rList_free( autoruns );
                    autoruns = NULL;
                }

                rSequence_addTIMESTAMP( event, RP_TAGS_TIMESTAMP, rpal_time_getGlobal() );

                notifications_publish( RP_TAGS_NOTIFICATION_OS_AUTORUNS_REP, event );
            }
        }
    }
}


static
RPVOID
    allOsSnapshots
    (
        rEvent isTimeToStop,
        RPVOID ctx
    )
{
    rpcm_tag events[] = { RP_TAGS_NOTIFICATION_OS_AUTORUNS_REQ,
                          RP_TAGS_NOTIFICATION_OS_DRIVERS_REQ,
                          RP_TAGS_NOTIFICATION_OS_PROCESSES_REQ,
                          RP_TAGS_NOTIFICATION_OS_SERVICES_REQ };
    RU32 i = 0;
    rSequence dummy = NULL;

    UNREFERENCED_PARAMETER( ctx );

    if( NULL != ( dummy = rSequence_new() ) )
    {
        rpal_debug_info( "beginning full os snapshots run" );
        while( !rEvent_wait( isTimeToStop, MSEC_FROM_SEC( 10 ) ) &&
               rpal_memory_isValid( isTimeToStop ) &&
               i < ARRAY_N_ELEM( events ) )
        {
            notifications_publish( events[ i ], dummy );
            i++;
        }

        rSequence_free( dummy );
    }

    return NULL;
}

static
RVOID
    os_kill_process
    (
        rpcm_tag eventType,
        rSequence event
    )
{
    RU32 pid = 0;

    UNREFERENCED_PARAMETER( eventType );

    if( rpal_memory_isValid( event ) )
    {
        if( rSequence_getRU32( event, RP_TAGS_PROCESS_ID, &pid ) )
        {
            if( processLib_killProcess( pid ) )
            {
                rSequence_addRU32( event, RP_TAGS_ERROR, RPAL_ERROR_SUCCESS );
            }
            else
            {
                rSequence_addRU32( event, RP_TAGS_ERROR, rpal_error_getLast() );
            }

            rSequence_addTIMESTAMP( event, RP_TAGS_TIMESTAMP, rpal_time_getGlobal() );
            notifications_publish( RP_TAGS_NOTIFICATION_OS_KILL_PROCESS_REP, event );
        }
    }
}

//=============================================================================
// COLLECTOR INTERFACE
//=============================================================================

rpcm_tag collector_11_events[] = { RP_TAGS_NOTIFICATION_OS_SERVICES_REP,
                                   RP_TAGS_NOTIFICATION_OS_DRIVERS_REP,
                                   RP_TAGS_NOTIFICATION_OS_PROCESSES_REP,
                                   RP_TAGS_NOTIFICATION_OS_AUTORUNS_REP,
                                   RP_TAGS_NOTIFICATION_OS_KILL_PROCESS_REP,
                                   0 };

RBOOL
    collector_11_init
    (
        HbsState* hbsState,
        rSequence config
    )
{
    RBOOL isSuccess = FALSE;
    RU64 timeDelta = _FULL_SNAPSHOT_DEFAULT_DELTA;

    UNREFERENCED_PARAMETER( config );

    if( NULL != hbsState )
    {
        if( notifications_subscribe( RP_TAGS_NOTIFICATION_OS_SERVICES_REQ, NULL, 0, NULL, os_services ) &&
            notifications_subscribe( RP_TAGS_NOTIFICATION_OS_DRIVERS_REQ, NULL, 0, NULL, os_drivers ) &&
            notifications_subscribe( RP_TAGS_NOTIFICATION_OS_PROCESSES_REQ, NULL, 0, NULL, os_processes ) &&
            notifications_subscribe( RP_TAGS_NOTIFICATION_OS_AUTORUNS_REQ, NULL, 0, NULL, os_autoruns ) &&
            notifications_subscribe( RP_TAGS_NOTIFICATION_OS_KILL_PROCESS_REQ, NULL, 0, NULL, os_kill_process ) )
        {
            isSuccess = TRUE;

            if( rpal_memory_isValid( config ) )
            {
                if( !rSequence_getTIMEDELTA( config, RP_TAGS_TIMEDELTA, &timeDelta ) )
                {
                    timeDelta = _FULL_SNAPSHOT_DEFAULT_DELTA;
                }
            }

            if( !rThreadPool_scheduleRecurring( hbsState->hThreadPool, timeDelta, allOsSnapshots, NULL, TRUE ) )
            {
                isSuccess = FALSE;
            }
        }
        else
        {
            notifications_unsubscribe( RP_TAGS_NOTIFICATION_OS_SERVICES_REQ, NULL, os_services );
            notifications_unsubscribe( RP_TAGS_NOTIFICATION_OS_DRIVERS_REQ, NULL, os_drivers );
            notifications_unsubscribe( RP_TAGS_NOTIFICATION_OS_PROCESSES_REQ, NULL, os_processes );
            notifications_unsubscribe( RP_TAGS_NOTIFICATION_OS_AUTORUNS_REQ, NULL, os_autoruns );
        }
    }

    return isSuccess;
}

RBOOL
    collector_11_cleanup
    (
        HbsState* hbsState,
        rSequence config
    )
{
    RBOOL isSuccess = FALSE;

    UNREFERENCED_PARAMETER( config );

    if( NULL != hbsState )
    {
        notifications_unsubscribe( RP_TAGS_NOTIFICATION_OS_SERVICES_REQ, NULL, os_services );
        notifications_unsubscribe( RP_TAGS_NOTIFICATION_OS_DRIVERS_REQ, NULL, os_drivers );
        notifications_unsubscribe( RP_TAGS_NOTIFICATION_OS_PROCESSES_REQ, NULL, os_processes );
        notifications_unsubscribe( RP_TAGS_NOTIFICATION_OS_AUTORUNS_REQ, NULL, os_autoruns );
        notifications_unsubscribe( RP_TAGS_NOTIFICATION_OS_KILL_PROCESS_REQ, NULL, os_kill_process );

        isSuccess = TRUE;
    }

    return isSuccess;
}
