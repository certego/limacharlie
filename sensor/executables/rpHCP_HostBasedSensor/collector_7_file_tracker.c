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
#include <rpHostCommonPlatformLib/rTags.h>
#include <kernelAcquisitionLib/kernelAcquisitionLib.h>
#include <kernelAcquisitionLib/common.h>

#define RPAL_FILE_ID          67

static RBOOL g_is_kernel_failure = FALSE;  // Kernel acquisition failed for this method

static
RBOOL
    _assemble_full_name
    (
        RPWCHAR out,
        RU32 outSize,
        RPWCHAR root,
        RPWCHAR file
    )
{
    RBOOL isSuccess = FALSE;

    if( NULL != out &&
        0 != outSize &&
        NULL != root &&
        NULL != file )
    {
        rpal_memory_zero( out, outSize );
        rpal_string_strcatw( out, root );

        if( outSize > ( rpal_string_strlenw( out ) + rpal_string_strlenw( file ) ) * sizeof( RWCHAR ) )
        {
            rpal_string_strcatw( out, file );
            isSuccess = TRUE;
        }
    }

    return isSuccess;
}

static
RPVOID
    processUmFileChanges
    (
        rEvent isTimeToStop
    )
{    
#ifdef RPAL_PLATFORM_WINDOWS
    RWCHAR rootEnv[] = _WCH( "%SYSTEMDRIVE%\\" );
#else
    RWCHAR rootEnv[] = _WCH( "/" );
#endif
    RWCHAR fullName[ 1024 ] = { 0 };
    rDirWatch watch = NULL;
    RPWCHAR root = NULL;
    RPWCHAR fileName = NULL;
    RU32 apiAction = 0;
    rpcm_tag event = RP_TAGS_INVALID;
    rSequence notif = 0;
    RU64 curTime = 0;

    if( rpal_string_expandw( (RPWCHAR)&rootEnv, &root ) &&
        NULL != ( watch = rDirWatch_new( root, 
                                         RPAL_DIR_WATCH_CHANGE_CREATION |
                                         RPAL_DIR_WATCH_CHANGE_FILE_NAME | 
                                         RPAL_DIR_WATCH_CHANGE_LAST_ACCESS, 
                                         TRUE ) ) )
    {
        while( rpal_memory_isValid( isTimeToStop ) &&
               !rEvent_wait( isTimeToStop, 0 ) &&
               ( !kAcq_isAvailable() ||
                 g_is_kernel_failure ) )
        {
            event = RP_TAGS_INVALID;

            if( rDirWatch_next( watch, 100, &fileName, &apiAction ) &&
                ( RPAL_DIR_WATCH_ACTION_ADDED  == apiAction ||
                  RPAL_DIR_WATCH_ACTION_REMOVED == apiAction ||
                  RPAL_DIR_WATCH_ACTION_MODIFIED  == apiAction ) )
            {
                curTime = rpal_time_getGlobal();

                if( _assemble_full_name( fullName, sizeof( fullName ), root, fileName ) )
                {
                    if( NULL != ( notif = rSequence_new() ) )
                    {
                        if( RPAL_DIR_WATCH_ACTION_ADDED == apiAction )
                        {
                            event = RP_TAGS_NOTIFICATION_FILE_CREATE;
                        }
                        else if( RPAL_DIR_WATCH_ACTION_REMOVED == apiAction )
                        {
                            event = RP_TAGS_NOTIFICATION_FILE_DELETE;
                        }
                        else if( RPAL_DIR_WATCH_ACTION_MODIFIED == apiAction )
                        {
                            event = RP_TAGS_NOTIFICATION_FILE_MODIFIED;
                        }

                        if( rSequence_addSTRINGW( notif, RP_TAGS_FILE_PATH, (RPWCHAR)&fullName ) &&
                            rSequence_addTIMESTAMP( notif, RP_TAGS_TIMESTAMP, curTime ) )
                        {
                            notifications_publish( event, notif );
                        }
                        
                        rSequence_free( notif );
                    }
                }
            }
        }

        rDirWatch_free( watch );
    }

    rpal_memory_free( root );

    return NULL;
}


static
RPVOID
    processKmFileChanges
    (
        rEvent isTimeToStop
    )
{
    rpcm_tag event = RP_TAGS_INVALID;
    rSequence notif = 0;
    RU32 nScratch = 0;
    KernelAcqFileIo new_from_kernel[ 200 ] = { 0 };
    RU32 i = 0;

    while( rpal_memory_isValid( isTimeToStop ) &&
           !rEvent_wait( isTimeToStop, 1000 ) )
    {
        nScratch = ARRAY_N_ELEM( new_from_kernel );
        rpal_memory_zero( new_from_kernel, sizeof( new_from_kernel ) );
        if( !kAcq_getNewFileIo( new_from_kernel, &nScratch ) )
        {
            rpal_debug_warning( "kernel acquisition for new file io failed" );
            g_is_kernel_failure = TRUE;
            break;
        }

        for( i = 0; i < nScratch; i++ )
        {
            if( KERNEL_ACQ_FILE_ACTION_ADDED == new_from_kernel[ i ].action )
            {
                event = RP_TAGS_NOTIFICATION_FILE_CREATE;
            }
            else if( KERNEL_ACQ_FILE_ACTION_REMOVED == new_from_kernel[ i ].action )
            {
                event = RP_TAGS_NOTIFICATION_FILE_DELETE;
            }
            else if( KERNEL_ACQ_FILE_ACTION_MODIFIED == new_from_kernel[ i ].action )
            {
                event = RP_TAGS_NOTIFICATION_FILE_MODIFIED;
            }
            else
            {
                continue;
            }

            if( NULL != ( notif = rSequence_new() ) )
            {
                if( rSequence_addSTRINGN( notif, RP_TAGS_FILE_PATH, new_from_kernel[ i ].path ) &&
                    rSequence_addTIMESTAMP( notif,
                                            RP_TAGS_TIMESTAMP,
                                            rpal_time_getGlobalFromLocal( new_from_kernel[ i ].ts ) ) )
                {
                    notifications_publish( event, notif );
                }

                rSequence_free( notif );
            }
        }
    }

    return NULL;
}

static
RPVOID
    processFileChanges
    (
        rEvent isTimeToStop,
        RPVOID ctx
    )
{
    UNREFERENCED_PARAMETER( ctx );

    while( !rEvent_wait( isTimeToStop, 0 ) )
    {
        if( kAcq_isAvailable() &&
            !g_is_kernel_failure )
        {
            // We first attempt to get new fileio through
            // the kernel mode acquisition driver
            rpal_debug_info( "running kernel acquisition fileio notification" );
            processKmFileChanges( isTimeToStop );
        }
        // If the kernel mode fails, or is not available, try
        // to revert to user mode
        else if( !rEvent_wait( isTimeToStop, 0 ) )
        {
            rpal_debug_info( "running usermode acquisition fileio notification" );
            processUmFileChanges( isTimeToStop );
        }
    }

    return NULL;
}

//=============================================================================
// COLLECTOR INTERFACE
//=============================================================================

rpcm_tag collector_7_events[] = { RP_TAGS_NOTIFICATION_FILE_CREATE,
                                  RP_TAGS_NOTIFICATION_FILE_DELETE,
                                  RP_TAGS_NOTIFICATION_FILE_MODIFIED,
                                  0};

RBOOL
    collector_7_init
    (
        HbsState* hbsState,
        rSequence config
    )
{
    RBOOL isSuccess = FALSE;

    UNREFERENCED_PARAMETER( config );

    if( NULL != hbsState )
    {
        g_is_kernel_failure = FALSE;

        if( rThreadPool_task( hbsState->hThreadPool, processFileChanges, NULL ) )
        {
            isSuccess = TRUE;
        }
    }

    return isSuccess;
}

RBOOL
    collector_7_cleanup
    (
        HbsState* hbsState,
        rSequence config
    )
{
    RBOOL isSuccess = FALSE;

    UNREFERENCED_PARAMETER( config );

    if( NULL != hbsState )
    {
        isSuccess = TRUE;
    }

    return isSuccess;
}
