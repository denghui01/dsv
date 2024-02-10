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
#define DSV_DEVLIST     "[0]/SYS/DEV/LIST"

static struct state
{
    void *dsv_ctx;
    char *json_file;
    uint32_t instID;
    dsv_info_t dsv;
    int log_level;
    void *hndl_log_level;
    void *hndl_devlist;
}g_state;

static void handle_notifications()
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
                g_state.log_level = *(uint32_t*)value;
                DSV_SetLogmask(LOG_UPTO( g_state.log_level));
                printf( "%s=%d\n", full_name, g_state.log_level );

            }
            else if( hndl == g_state.hndl_devlist )
            {
                /* handle devlist changes, here we simply print it */
                int size = DSV_Len(g_state.dsv_ctx, hndl) / sizeof(int);
                int *ai = (int *)value;
                printf( "%s=", full_name );
                for(int i = 0; i < size; i++)
                {
                    if( i !=  size - 1 )
                    {
                        printf( "%d,", ai[i] );
                    }
                    else
                    {
                        printf( "%d\n", ai[i] );
                    }
                }
            }
        }
    }
}

static void update_devlist()
{
    int rc = EINVAL;
    g_state.hndl_devlist = DSV_Handle( g_state.dsv_ctx, DSV_DEVLIST );
    if( g_state.hndl_devlist == NULL )
    {
        /* create devlist dsv if it hasn't been created */
        dsv_info_t dsv;
        int array[] = {0};
        dsv.pName = strdup(DSV_DEVLIST);
        dsv.type = DSV_TYPE_INT_ARRAY;
        dsv.value.pArray = array;
        dsv.len = sizeof(int);
        dsv.instID = 0;
        rc = DSV_Create( g_state.dsv_ctx, dsv.instID, &dsv );
        free( dsv.pName );
        if( rc != 0 )
        {
            dsvlog( LOG_ERR, "Failed to create dsv: %s", DSV_DEVLIST );
            return;
        }
        g_state.hndl_devlist = DSV_Handle( g_state.dsv_ctx, DSV_DEVLIST );
    }

    if( g_state.hndl_devlist != NULL )
    {
        DSV_SubByName( g_state.dsv_ctx, DSV_DEVLIST );
        DSV_AddItemToArray( g_state.dsv_ctx,
                            g_state.hndl_devlist,
                            g_state.instID );
    }
}

static void update_logmask()
{
    /* set log mask based on the log level */
    DSV_SetLogmask(LOG_UPTO( g_state.log_level));

    /* Create log level dsv with default value  and subscribe it */
    char name_log_level[DSV_STRING_SIZE_MAX];
    g_state.instID = DSV_GetInstID( NULL ) & 0x000FFFF;
    sprintf( name_log_level, "[%d]/SYS/DEV/LOG_LEVEL", g_state.instID );
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
    while( (c = getopt( argc, argv, "v" )) != -1 )
    {
        switch( c )
        {
        case 'v':
            ++g_state.log_level;
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

    update_logmask();

    update_devlist();

    handle_notifications();

    exit( 1 );
}
