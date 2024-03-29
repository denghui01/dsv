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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include "zmq.h"
#include "dsv.h"
#include "dsv_msg.h"
#include "dsv_log.h"

/* this name is a well-known dsv name */
#define DSV_DEVLIST         "[0]/SYS/DEV_LIST"

/*! devman module log level dsv */
#define DSV_DEV_LOG_LEVEL   "/DEV/CFG/LOG_LEVEL"

/*! system dsvs */
#define DSV_SYS_DSVS_FILE   "sys_dsvs.json"

/*! default system instant ID */
#define DSV_DEFAULT_INSTID  0

static struct state
{
    void *dsv_ctx;
    char *json_file;
    uint32_t instID;
    int log_level;
    void *hndl_log_level;
    void *hndl_devlist;
}g_state;

static void ProcessNotifications()
{
    char full_name[DSV_STRING_SIZE_MAX];
    char value[DSV_STRING_SIZE_MAX];
    void *hndl;
    int rc = 0;
    while( 1 )
    {
        rc = DSV_GetNotification( g_state.dsv_ctx,
                                  &hndl,
                                  full_name,
                                  DSV_STRING_SIZE_MAX,
                                  value,
                                  DSV_STRING_SIZE_MAX );
        if( rc == 0 )
        {
            if( hndl == g_state.hndl_log_level )
            {
                /* handle logmask updates */
                g_state.log_level = *(uint32_t *)value;
                DSV_SetLogmask( LOG_UPTO( g_state.log_level ) );
                printf( "%s=%d\n", full_name, g_state.log_level );

            }
            else if( hndl == g_state.hndl_devlist )
            {
                /* handle devlist changes, here we simply print it */
                char buffer[DSV_STRING_SIZE_MAX];
                DSV_PrintArray( value, buffer, DSV_STRING_SIZE_MAX );
                printf( "%s=%s\n", full_name, buffer );
            }
        }
    }
}

static void InitDevlist()
{
    int rc = EINVAL;
    g_state.hndl_devlist = DSV_Handle( g_state.dsv_ctx, DSV_DEVLIST );
    if( g_state.hndl_devlist == NULL )
    {
        /* create devlist dsv if it hasn't been created */
        dsv_info_t dsv;
        int array[] = { DSV_DEFAULT_INSTID };
        dsv.pName = strdup( DSV_DEVLIST );
        dsv.type = DSV_TYPE_INT_ARRAY;
        dsv.value.pArray = array;
        dsv.len = sizeof(int);
        dsv.instID = DSV_DEFAULT_INSTID;
        rc = DSV_Create( g_state.dsv_ctx, dsv.instID, &dsv );
        free( dsv.pName );
        if( rc != 0 )
        {
            dsvlog( LOG_ERR, "Failed to create dsv: %s", DSV_DEVLIST );
            return;
        }
        g_state.hndl_devlist = DSV_Handle( g_state.dsv_ctx, DSV_DEVLIST );

        /* create system DSVs */
        DSV_CreateWithJson( g_state.dsv_ctx,
                            dsv.instID,
                            DSV_SYS_DSVS_FILE );

    }

    if( g_state.hndl_devlist != NULL )
    {
        DSV_SubByName( g_state.dsv_ctx, DSV_DEVLIST );

        if( g_state.instID == 0 )
        {
            g_state.instID = DSV_GetInstID( NULL ) & 0x000FFFF;
        }
        DSV_AddItemToArray( g_state.dsv_ctx,
                            g_state.hndl_devlist,
                            g_state.instID );
    }

    if( g_state.json_file )
    {
        /* create device specific DSVs, passed by command line parameter */
        DSV_CreateWithJson( g_state.dsv_ctx,
                            g_state.instID,
                            g_state.json_file );
    }

}

static void InitLogmask()
{
    /* set log mask based on the log level */
    DSV_SetLogmask( LOG_UPTO( g_state.log_level ) );

    /* Create log level dsv with default value  and subscribe it */
    char name_log_level[DSV_STRING_SIZE_MAX];
    g_state.instID = DSV_GetInstID( NULL ) & 0x000FFFF;
    sprintf( name_log_level,
             "[%d]%s",
             g_state.instID,
             DSV_DEV_LOG_LEVEL );
    DSV_LogInit( g_state.dsv_ctx, name_log_level );

    /* set the log level dsv with current real value */
    g_state.hndl_log_level = DSV_Handle( g_state.dsv_ctx, name_log_level );
    if( g_state.hndl_log_level != NULL )
    {
        DSV_Set( g_state.dsv_ctx, g_state.hndl_log_level, g_state.log_level );
    }

}
/*!=============================================================================

    Entry point for the device manager

@param[in]
    argc
        number of arguments passed to the process

@param[in]
    argv
        array of null terminated argument strings passed to the process
        argv[0] is the name of the process

@return
    0 on sucess, others on failure
==============================================================================*/
int main( int argc, char **argv )
{
    int c;
    int rc = 0;

//  if( argc < 2 )
//  {
//      //usage();
//      exit( EXIT_FAILURE );
//  }

    /* the default log level is WARNING, any level below it will be ignored */
    g_state.log_level = LOG_WARNING;

    /* parse the command line options */
    while( (c = getopt( argc, argv, "vf:" )) != -1 )
    {
        switch( c )
        {
        case 'v':
            ++g_state.log_level;
            break;

        case 'f':
            g_state.json_file = optarg;
            break;

        default:
            break;
        }
    }

    g_state.dsv_ctx = DSV_Open();
    if( g_state.dsv_ctx == NULL )
    {
        exit( EXIT_FAILURE );
    }

    InitLogmask();
    InitDevlist();
    ProcessNotifications();

    exit( 1 );
}
