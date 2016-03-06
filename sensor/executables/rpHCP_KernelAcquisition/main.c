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
#include <rpHostCommonPlatformIFaceLib/rpHostCommonPlatformIFaceLib.h>

#include <obfuscationLib/obfuscationLib.h>
#include <rpHostCommonPlatformLib/rTags.h>


//=============================================================================
//  RP HCP Module Requirements
//=============================================================================
#define RPAL_FILE_ID 102
RpHcp_ModuleId g_current_Module_id = RP_HCP_MODULE_ID_KERNEL_ACQ;



//=============================================================================
//  Global Behavior Variables
//=============================================================================
#define HBS_DEFAULT_BEACON_TIMEOUT              (1*60)
#define HBS_DEFAULT_BEACON_TIMEOUT_FUZZ         (1*60)
#define HBS_EXFIL_QUEUE_MAX_NUM                 5000
#define HBS_EXFIL_QUEUE_MAX_SIZE                (1024*1024*10)

// Large blank buffer to be used to patch configurations post-build
#define _HCP_DEFAULT_STATIC_STORE_SIZE                          (1024 * 50)
#define _HCP_DEFAULT_STATIC_STORE_MAGIC                         { 0xFA, 0x57, 0xF0, 0x0D }
static RU8 g_patchedConfig[ _HCP_DEFAULT_STATIC_STORE_SIZE ] = _HCP_DEFAULT_STATIC_STORE_MAGIC;
#define _HCP_DEFAULT_STATIC_STORE_KEY                           { 0xFA, 0x75, 0x01 }

//=============================================================================
//  Utilities
//=============================================================================
static
rSequence
    getStaticConfig
    (

    )
{
    RU8 magic[] = _HCP_DEFAULT_STATIC_STORE_MAGIC;
    rSequence config = NULL;
    RU32 unused = 0;
    RU8 key[] = _HCP_DEFAULT_STATIC_STORE_KEY;

    if( 0 != rpal_memory_memcmp( g_patchedConfig, magic, sizeof( magic ) ) )
    {
        obfuscationLib_toggle( g_patchedConfig, sizeof( g_patchedConfig ), key, sizeof( key ) );

        if( rSequence_deserialise( &config, g_patchedConfig, sizeof( g_patchedConfig ), &unused ) )
        {
            rpal_debug_info( "static store patched, using it as config" );
        }

        obfuscationLib_toggle( g_patchedConfig, sizeof( g_patchedConfig ), key, sizeof( key ) );
    }
    else
    {
        rpal_debug_info( "static store not patched, using defaults" );
    }

    return config;
}


//=============================================================================
//  Entry Point
//=============================================================================
RU32
RPAL_THREAD_FUNC
    RpHcpI_mainThread
    (
        rEvent isTimeToStop
    )
{
    RU32 ret = 0;
    RU32 error = 0;
    rSequence config = NULL;
    RBOOL isLoaded = FALSE;

    FORCE_LINK_THAT(HCP_IFACE);

    if( !rEvent_wait( isTimeToStop, 0 ) )
    {
        if( NULL != ( config = getStaticConfig() ) )
        {
            error = 0;

            rpal_debug_info( "loading kernel acquisition" );

            #ifdef RPAL_PLATFORM_MACOSX
                do
                {
                    RCHAR tmpPath[] = "/tmp/tmp_hbs_acq.tar.gz";
                    RCHAR tmpUntar[] = "tar xzf /tmp/tmp_hbs_acq.tar.gz -C /tmp/; chown -R root:wheel /tmp/tmp_hbs_acq.kext; chmod 500 /tmp/tmp_hbs_acq.kext";
                    RCHAR tmpLoad[] = "kextload /tmp/tmp_hbs_acq.kext";
                    RPU8 package = NULL;
                    RU32 packageSize = 0;
                    RPVOID lastHandler = NULL;

                    rpal_debug_info( "getting kext from config" );
                    if( !rSequence_getBUFFER( config, RP_TAGS_BINARY, &package, &packageSize ) )
                    {
                        rpal_debug_error( "malformed config" );
                        break;
                    }

                    rpal_debug_info( "writing package to disk" );
                    if( !rpal_file_write( tmpPath, package, packageSize, TRUE ) )
                    {
                        rpal_debug_error( "could not write package to disk" );
                        break;
                    }

                    rpal_debug_info( "unpacking kernel extension" );
                    if( 0 != ( error = system( tmpUntar ) ) )
                    {
                        rpal_debug_error( "could not unpack kernel extension: %d", error );
                        break;
                    }

                    rpal_debug_info( "deleting package from disk" );
                    if( !rpal_file_delete( tmpPath, FALSE ) )
                    {
                        rpal_debug_warning( "error deleting package from disk" );
                        // This is not fatal
                    }

                    rpal_debug_info( "loading kernel extension" );
                    if( 0 != ( error = system( tmpLoad ) ) )
                    {
                        rpal_debug_error( "could not load kernel extension: %d", error );
                    }

                    isLoaded = TRUE;

                    rpal_debug_info( "deleting kernel extension from disk" );
                    if( !rpal_file_delete( tmpPath, FALSE ) )
                    {
                        rpal_debug_warning( "error deleting kernel extension from disk" );
                        // This is not fatal
                    }

                } while( FALSE );
            #else
                rpal_debug_warning( "no kernel acquisiton loading available for platform" );
            #endif

            rSequence_free( config );
        }
        else
        {
            rpal_debug_warning( "no kernel acquisition package found" );
        }
    }

    rpal_debug_info( "waiting for exit signal" );
    rEvent_wait( isTimeToStop, RINFINITE );

    rpal_debug_info( "unoading kernel acquisition" );
    #ifdef RPAL_PLATFORM_MACOSX
        do
        {
            RCHAR tmpUnload[] = "kextunload /tmp/tmp_hbs_acq.kext";

            if( isLoaded &&
                0 != ( error = system( tmpUnload ) ) )
            {
                rpal_debug_error( "could not unload kernel extension: %d", error );
                break;
            }

        } while( FALSE );
    #else
        rpal_debug_warning( "no kernel acquisiton unloading available for platform" );
    #endif

    return ret;
}

