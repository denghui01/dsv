/*==============================================================================
MIT License

Copyright (c) 2023 David Deng

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
==============================================================================*/

/*==============================================================================
                              Includes
==============================================================================*/
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>
#include "zmq.h"
#include "dsv.h"
#include "dsv_msg.h"
#include "dsv_log.h"
#include "cjson/cJSON.h"

typedef enum dsv_operation
{
    DSV_OPINVAL,
    DSV_CREATE,
    DSV_SET,
    DSV_GET,
    DSV_SUB,
    DSV_SAVE,
    DSV_RESTORE,
    DSV_TRACK
}dsv_op_t;

using dsv_array_t = std::vector< int >;

static struct state
{
    void *dsv_ctx;
    int operation;
    char *json_file;
    uint32_t instID;
    dsv_info_t dsv;
    char dsv_val[DSV_STRING_SIZE_MAX];
    char dsv_name[DSV_STRING_SIZE_MAX];
}g_state;
/*==============================================================================
                           Function Definitions
==============================================================================*/
/*==============================================================================
    Termination handlings
==============================================================================*/
#define S_NOTIFY_MSG " "
#define S_ERROR_MSG "Error while writing to self-pipe.\n"
static int s_fd;
static int pipefds[2];
static void s_signal_handler( int signal_value )
{
    signal_value = signal_value;
    int rc = write( s_fd, S_NOTIFY_MSG, sizeof(S_NOTIFY_MSG) );
    if( rc != sizeof(S_NOTIFY_MSG) )
    {
        rc = write( STDOUT_FILENO, S_ERROR_MSG, sizeof(S_ERROR_MSG) - 1 );
        exit( 1 );
    }
}

static void s_catch_signals( int fd )
{
    s_fd = fd;

    struct sigaction action;
    action.sa_handler = s_signal_handler;
    //  Doesn't matter if SA_RESTART set because self-pipe will wake up zmq_poll
    //  But setting to 0 will allow zmq_read to be interrupted.
    action.sa_flags = 0;
    sigemptyset( &action.sa_mask );
    sigaction( SIGINT, &action, NULL );
    sigaction( SIGTERM, &action, NULL );
}

static void s_create_pipe()
{
    int rc;
    rc = pipe( pipefds );
    if( rc != 0 )
    {
        perror( "Creating self-pipe" );
        exit( 1 );
    }
    for( int i = 0; i < 2; i++ )
    {
        int flags = fcntl( pipefds[i], F_GETFL, 0 );
        if( flags < 0 )
        {
            perror( "fcntl(F_GETFL)" );
            exit( 1 );
        }
        rc = fcntl( pipefds[i], F_SETFL, flags | O_NONBLOCK );
        if( rc != 0 )
        {
            perror( "fcntl(F_SETFL)" );
            exit( 1 );
        }
    }

    s_catch_signals( pipefds[1] );
}

/*!=============================================================================
    Display the usage information for the command.
==============================================================================*/
static void usage( void )
{
    fprintf( stderr,
             "usage: sv [-c][-s][-g][-i][-f][-y][-d][t][-v] <variable-name> [<value>]\n"
             "    -c/create - create a new dsv\n"
             "    -s/set/write - set a dsv value\n"
             "    -g/get/read - get a dsv value\n"
             "    -u/sub/subscribe - subscribe a dsv\n"
             "    save - persist all sysvars that need to save\n"
             "    restore - restore all sysvars from non-volatile memory\n"
             "    track - track the change of particular dsvs\n"
             "    -f <file-name> - create a batch of DSVs from a JSON file\n"
             "    -i <instance ID> - create a DSV with instance ID\n"
             "    -y <type> - create a DSV with type\n"
             "    -d <description> - create a DSV with description\n"
             "    -t <tag1,tag2> - create a DSV with tags, delimiter with ','\n"
             "    -v <default value> - create a DSV with default value\n"
             "example:\n"
             "   sv -c -i 1235 -f dsvs.json\n"
             "   sv -c /SYS/STS/DEVICE_NAME -i 1235 -v \"wifi router\" -y string -d \"device name\" -t \"sys.sts\"\n"
             "   sv set [0]/SYS/STS/DATE 2023-12-25\n"
             "   sv set [0]/SYS/STS/NAME \"wifi router\"\n"
             "   sv get [123]/SYS/STS/DEVICE_NAME\n"
             "   sv sub [123]/SYS/STS/DEVICE_NAME\n"
             "   sv save\n"
             "   sv restore\n"
             "   sv track enable /SYS/STS/DEVICE_NAME\n"
           );
}

static int HandleNotification( void )
{
    char full_name[DSV_STRING_SIZE_MAX];
    char value[DSV_STRING_SIZE_MAX];
    void *hndl;
    int rc = 0;
    rc = DSV_GetNotification( g_state.dsv_ctx,
                              &hndl,
                              full_name,
                              DSV_STRING_SIZE_MAX,
                              value,
                              DSV_STRING_SIZE_MAX );
    if( rc == 0 )
    {
        dsv_info_t dsv;
        dsv.type = DSV_Type( g_state.dsv_ctx, hndl );
        if( dsv.type == DSV_TYPE_STR )
        {
            printf( "%s=%s\n", full_name, value );
        }
        else if( dsv.type == DSV_TYPE_INT_ARRAY )
        {
            char buffer[DSV_STRING_SIZE_MAX];
            DSV_PrintArray( value, buffer, DSV_STRING_SIZE_MAX );
            printf( "%s=%s\n", full_name, buffer );
        }
        else
        {
            dsv.value = *(dsv_value_t *)value;
            DSV_Value2Str( value, DSV_STRING_SIZE_MAX, &dsv );
            printf( "%s=%s\n", full_name, value );
        }
    }

    return rc;
}

/*!=============================================================================

    Process dsv sub/subscribe command

@param[in]
    argc
        number of arguments passed to the process

@param[in]
    argv
        array of null terminated argument strings passed to the process
        The arguments are processed using getopt()

@retval
    0 - success
    others - failed
/*============================================================================*/
static int ProcessSub( int argc, char **argv )
{
    int rc = EINVAL;
    for(; optind < argc; ++optind )
    {
        /* the rest of parameters should be multiple names */
        strncpy( g_state.dsv_name, argv[optind], DSV_STRING_SIZE_MAX );
        strtoupper( g_state.dsv_name );
        rc = DSV_SubByName( g_state.dsv_ctx, g_state.dsv_name );
    }

    if( rc == 0 )
    {
        s_create_pipe();

        dsv_context_t *dsv_ctx = (dsv_context_t *)g_state.dsv_ctx;

        zmq_pollitem_t items[] = {
            { 0, pipefds[0], ZMQ_POLLIN, 0 },
            { dsv_ctx->sock_subscribe, 0, ZMQ_POLLIN, 0 }
        };
        while( 1 )
        {
            /* zmq_poll provides level-triggered fashion */
            if( zmq_poll( items, 2, -1 ) == -1 )
            {
                dsvlog( LOG_ERR, "zmq_poll failed: %s", strerror( errno ) );
                break;
            }

            /* Signal pipe FD */
            if( items[0].revents & ZMQ_POLLIN )
            {
                char buffer[1];
                rc = read( pipefds[0], buffer, 1 );  // clear notifying byte
                dsvlog( LOG_WARNING, "interrupt received, killing server...\n" );
                break;
            }

            /*  new publish from frontend, update cache and then forward */
            if( items[1].revents & ZMQ_POLLIN )
            {
                rc = HandleNotification();
            }
        }
    }
    else
    {
        dsvlog( LOG_ERR, "Failed to subscribe DSVs\n" );
    }

    return rc;
}

static int ProcessSave( int argc, char **argv )
{
    return DSV_Save( g_state.dsv_ctx );
}

static int ProcessRestore( int argc, char **argv )
{
    return DSV_Restore( g_state.dsv_ctx );
}

static int ProcessTrack( int argc, char **argv )
{
    char enable_str[DSV_STRING_SIZE_MAX];
    int enable_track;
    int rc = EINVAL;

    strncpy( enable_str, argv[optind++], DSV_STRING_SIZE_MAX );
    if( strstr( enable_str, "enable" ) != NULL )
    {   
        enable_track = 1;
    }
    else if( strstr( enable_str, "disable" ) != NULL )
    {   
        enable_track = 0;
    }
    else
    {
        fprintf( stderr, "Wrong parameters\n" );
        return rc;
    }

    for(; optind < argc; ++optind )
    {
        /* the rest of parameters should be multiple names */
        strncpy( g_state.dsv_name, argv[optind], DSV_STRING_SIZE_MAX );
        strtoupper( g_state.dsv_name );
        int index = -1;
        do
        {
            index = DSV_TrackByNameFuzzy( g_state.dsv_ctx,
                                          g_state.dsv_name,
                                          index,
                                          enable_track );
        } while( index != -1 );
    }

    return 0;

}
/*!=============================================================================

    Process dsv get/read command

@param[in]
    argc
        number of arguments passed to the process

@param[in]
    argv
        array of null terminated argument strings passed to the process
        The arguments are processed using getopt()

@retval
    0 - success
    others - failed
/*============================================================================*/
static int ProcessGet( int argc, char **argv )
{
    int rc = 0;
    char value[DSV_STRING_SIZE_MAX];
    char name[DSV_STRING_SIZE_MAX];

    for(; optind < argc; ++optind )
    {
        /* the rest of parameters should be multiple names */
        strncpy( g_state.dsv_name, argv[optind], DSV_STRING_SIZE_MAX );
        strtoupper( g_state.dsv_name );
        int index = -1;
        do
        {
            index = DSV_GetByNameFuzzy( g_state.dsv_ctx,
                                        g_state.dsv_name,
                                        index,
                                        name,
                                        DSV_STRING_SIZE_MAX,
                                        value,
                                        DSV_STRING_SIZE_MAX );
            if( index != -1 )
            {
                printf( "%s=%s\n", name, value );
            }
        } while( index != -1 );
    }

    return rc;
}

/*!=============================================================================

    Process dsv set/write command

@param[in]
    argc
        number of arguments passed to the process

@param[in]
    argv
        array of null terminated argument strings passed to the process
        The arguments are processed using getopt()

@retval
    0 - success
    others - failed
/*============================================================================*/
static int ProcessSet( int argc, char **argv )
{
    int rc = EINVAL;
    if( optind == argc - 2 )
    {
        /* the last two parameters should be dsv name and new value */
        strncpy( g_state.dsv_name, argv[optind++], DSV_STRING_SIZE_MAX );
        strtoupper( g_state.dsv_name );
        strncpy( g_state.dsv_val, argv[optind], DSV_STRING_SIZE_MAX );
        rc = DSV_SetByName( g_state.dsv_ctx, g_state.dsv_name, g_state.dsv_val );
    }
    else
    {
        fprintf( stderr, "Wrong parameters\n" );
    }

    return rc;
}

/*!=============================================================================

    Process dsv add/create command

@param[in]
    argc
        number of arguments passed to the process

@param[in]
    argv
        array of null terminated argument strings passed to the process
        The arguments are processed using getopt()

@retval
    0 - success
    others - failed
/*============================================================================*/
static int ProcessCreate( int argc, char **argv )
{
    int rc = EINVAL;
    if( g_state.json_file == NULL )
    {
        if( optind == argc - 1 )
        {
            /* the last parameter should be dsv name */
            g_state.dsv.pName = (char *)malloc( DSV_STRING_SIZE_MAX );
            snprintf( g_state.dsv.pName,
                      DSV_STRING_SIZE_MAX,
                      "[%d]%s",
                      g_state.instID,
                      argv[optind] );
            strtoupper( g_state.dsv.pName );
            rc = DSV_Create( g_state.dsv_ctx,
                             g_state.instID,
                             &g_state.dsv );
        }
        else
        {
            fprintf( stderr, "Wrong parameters\n" );
            rc = EINVAL;
        }
    }
    else
    {
        rc = DSV_CreateWithJson( g_state.dsv_ctx,
                                 g_state.instID,
                                 g_state.json_file );
    }

    return rc;
}

/*!=============================================================================

    Parse the command line options and update g_state information

@param[in]
    argc
        number of arguments passed to the process

@param[in]
    argv
        array of null terminated argument strings passed to the process
        The arguments are processed using getopt()

@retval
    0 - success
    others - failed
/*============================================================================*/
static int ProcessOptions( int argc, char **argv )
{
    int rc = 0;
    int opt;
    char name[DSV_STRING_SIZE_MAX];

    /* process all the command line options */
    while( (opt = getopt( argc, argv, "csguf:i:y:d:t:v:" )) != -1 )
    {
        switch( opt )
        {
        case 'c':
            g_state.operation = DSV_CREATE;
            break;

        case 's':
            g_state.operation = DSV_SET;
            break;

        case 'g':
            g_state.operation = DSV_GET;
            break;

        case 'u':
            g_state.operation = DSV_SUB;
            break;

        case 'f':
            g_state.json_file = optarg;
            break;

        case 'i':
            g_state.instID = strtoul( optarg, NULL, 0 );
            break;

        case 'y':
            g_state.dsv.type = DSV_GetTypeFromStr( optarg );
            break;

        case 'd':
            g_state.dsv.pDesc = strdup( optarg );
            break;

        case 't':
            g_state.dsv.pTags = strdup( optarg );
            break;

        case 'v':
            DSV_Str2Value( optarg, &g_state.dsv );
            break;

        default:
            rc = EINVAL;
            break;
        }
    }

    if( g_state.operation == DSV_OPINVAL && rc == 0 )
    {
        /* No short option indicate operation, at lease one long argv*/
        if( optind < argc )
        {
            if( strcmp( argv[optind], "create" ) == 0 )
            {
                g_state.operation = DSV_CREATE;
                optind++;
            }
            else if( (strcmp( argv[optind], "set" ) == 0) ||
                     (strcmp( argv[optind], "write" ) == 0) )
            {
                g_state.operation = DSV_SET;
                optind++;
            }
            else if( (strcmp( argv[optind], "get" ) == 0) ||
                     (strcmp( argv[optind], "read" ) == 0) )
            {
                g_state.operation = DSV_GET;
                optind++;
            }
            else if( (strcmp( argv[optind], "sub" ) == 0) ||
                     (strcmp( argv[optind], "subscribe" ) == 0) )
            {
                g_state.operation = DSV_SUB;
                optind++;
            }
            else if( (strcmp( argv[optind], "save" ) == 0) )
            {
                g_state.operation = DSV_SAVE;
                optind++;
            }
            else if( (strcmp( argv[optind], "restore" ) == 0) )
            {
                g_state.operation = DSV_RESTORE;
                optind++;
            }
            else if( (strcmp( argv[optind], "track" ) == 0) )
            {
                g_state.operation = DSV_TRACK;
                optind++;
            }
            else
            {
                fprintf( stderr, "Missing/Unspported operation type\n" );
                rc = EINVAL;
            }
        }
        else
        {
            fprintf( stderr, "Missing operation type\n" );
            rc = EINVAL;
        }
    }

    return rc;
}
/*!=============================================================================

    Entry point for the dsv command

@param[in]
    argc
        number of arguments passed to the process

@param[in]
    argv
        array of null terminated argument strings passed to the process
        The arguments are processed using getopt()

@retval
    EXIT_SUCCESS - success
    EXIT_FAILURE - failed
/*============================================================================*/
int main( int argc, char *argv[] )
{
    int rc = 0;

    if( argc < 2 )
    {
        usage();
        exit( EXIT_FAILURE );
    }

    rc = ProcessOptions( argc, argv );
    if( rc != 0 )
    {
        usage();
        exit( EXIT_FAILURE );
    }

    g_state.dsv_ctx = DSV_Open();
    if( g_state.dsv_ctx == NULL )
    {
        exit( EXIT_FAILURE );
    }

    DSV_LogInit( NULL, NULL );

//  uint32_t instID = DSV_GetInstID("ens34") & 0x000FFFF;
//  dsvlog(LOG_DEBUG, "instID=%u", instID);

    switch( g_state.operation )
    {
    case DSV_CREATE:
        rc = ProcessCreate( argc, argv );
        break;
    case DSV_SET:
        rc = ProcessSet( argc, argv );
        break;
    case DSV_GET:
        rc = ProcessGet( argc, argv );
        break;
    case DSV_SUB:
        rc = ProcessSub( argc, argv );
        break;
    case DSV_SAVE:
        rc = ProcessSave( argc, argv );
        break;
    case DSV_RESTORE:
        rc = ProcessRestore( argc, argv );
        break;
    case DSV_TRACK:
        rc = ProcessTrack( argc, argv );
        break;
    default:
        break;
    }

    if( rc != 0 )
    {
        usage();
    }

    if( g_state.dsv.pName )
    {
        free( g_state.dsv.pName );
    }
    if( g_state.dsv.pDesc )
    {
        free( g_state.dsv.pDesc );
    }
    if( g_state.dsv.pTags )
    {
        free( g_state.dsv.pTags );
    }
    if( g_state.dsv.type == DSV_TYPE_STR )
    {
        if( g_state.dsv.value.pStr )
        {
            free( g_state.dsv.value.pStr );
        }
    }
    if( g_state.dsv.type == DSV_TYPE_INT_ARRAY )
    {
        if( g_state.dsv.value.pArray )
        {
            free( g_state.dsv.value.pArray );
        }
    }

    /* close the system variable manager */
    DSV_Close( g_state.dsv_ctx );
    return rc;

}
