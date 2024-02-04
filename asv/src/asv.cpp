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
#include <syslog.h>
#include <assert.h>
#include "zmq.h"
#include "dsv.h"
#include "dsv_msg.h"

typedef enum asv_operation
{
    ASV_OPINVAL,
    ASV_ADD,
    ASV_SET,
    ASV_GET,
    ASV_DEL,
    ASV_INS
}asv_op_t;

struct state
{
    void *dsv_ctx;
    int operation;
    int index;
    dsv_info_t dsv;
    int dsv_val;
}g_state;
/*==============================================================================
                           Function Definitions
==============================================================================*/

/*!=============================================================================
    Display the usage information for the command.
==============================================================================*/
static void usage( void )
{
    fprintf( stderr,
             "asv command is dedicated for array type of dsv"
             "usage: asv [set][get][add][-i][-v] <variable-name> [<value>]\n"
             "    set - set an item value of an array dsv\n"
             "    get - get an item value of an array dsv\n"
             "    add - add an item to an array dsv\n"
             "    del - delete an item from an array dsv\n"
             "    ins - insert an item to an array dsv\n"
             "    -i <index> - index of item, used in set/get/del/ins command\n"
             "    -v <value> - value of item, used in set/add/ins\n"
             "example:\n"
             "   asv set -i 3 -v 9900 [123]/SYS/TEST/INT_ARRAY\n"
             "   asv get -i 3 [123]/SYS/TEST/INT_ARRAY\n"
             "   asv add -v 9809 [123]/SYS/TEST/INT_ARRAY\n"
             "   asv del -i 3 [123]/SYS/TEST/INT_ARRAY\n"
             "   asv ins -i 3 -v 9900 [123]/SYS/TEST/INT_ARRAY\n"
           );
}

/*!=============================================================================

    Process asv add command, eg,
    asv add -v 9908 [123]/SYS/TEST/INT_ARRAY

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
int ProcessAddItem( int argc, char **argv )
{
    int rc = EINVAL;
    char dsv_name[DSV_STRING_SIZE_MAX];
    if( optind == argc - 1 )
    {
        /* the last parameters should be dsv name */
        strncpy( dsv_name, argv[optind], DSV_STRING_SIZE_MAX );
        void *hndl = DSV_Handle( g_state.dsv_ctx, dsv_name );
        if( hndl == NULL )
        {
            syslog( LOG_ERR, "Unable to find dsv: %s", dsv_name );
            return rc;
        }

        rc = DSV_AddItemToArray( g_state.dsv_ctx, hndl, g_state.dsv_val );
    }
    else
    {
        fprintf( stderr, "Wrong parameters\n" );
    }

    return rc;
}

/*!=============================================================================

    Process asv ins command, eg,
    asv ins -i 3 -v 9907 [123]/SYS/TEST/INT_ARRAY

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
static int ProcessInsItem( int argc, char **argv )
{
    int rc = EINVAL;
    char dsv_name[DSV_STRING_SIZE_MAX];
    if( optind == argc - 1 )
    {
        /* the last parameters should be dsv name */
        strncpy( dsv_name, argv[optind], DSV_STRING_SIZE_MAX );
        void *hndl = DSV_Handle( g_state.dsv_ctx, dsv_name );
        if( hndl == NULL )
        {
            syslog( LOG_ERR, "Unable to find dsv: %s", dsv_name );
            return rc;
        }

        rc = DSV_InsItemToArray( g_state.dsv_ctx,
                                 hndl,
                                 g_state.index,
                                 g_state.dsv_val );
    }
    else
    {
        fprintf( stderr, "Wrong parameters\n" );
    }

    return rc;
}

/*!=============================================================================

    Process asv del command, eg,
    asv del -i 3 [123]/SYS/TEST/INT_ARRAY

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
int ProcessDelItem( int argc, char **argv )
{
    int rc = EINVAL;
    char dsv_name[DSV_STRING_SIZE_MAX];
    if( optind == argc - 1 )
    {
        /* the last parameters should be dsv name */
        strncpy( dsv_name, argv[optind], DSV_STRING_SIZE_MAX );
        void *hndl = DSV_Handle( g_state.dsv_ctx, dsv_name );
        if( hndl == NULL )
        {
            syslog( LOG_ERR, "Unable to find dsv: %s", dsv_name );
            return rc;
        }

        rc = DSV_DelItemFromArray( g_state.dsv_ctx, hndl, g_state.index );
    }
    else
    {
        fprintf( stderr, "Wrong parameters\n" );
    }

    return rc;
}

/*!=============================================================================

    Process asv set command, eg,
    asv set -i 3 -v 9908 [123]/SYS/TEST/INT_ARRAY


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
int ProcessSetItem( int argc, char **argv )
{
    int rc = EINVAL;
    char dsv_name[DSV_STRING_SIZE_MAX];
    if( optind == argc - 1 )
    {
        /* the last parameters should be dsv name */
        strncpy( dsv_name, argv[optind], DSV_STRING_SIZE_MAX );
        void *hndl = DSV_Handle( g_state.dsv_ctx, dsv_name );

        if( hndl == NULL )
        {
            syslog( LOG_ERR, "Unable to find dsv: %s", dsv_name );
            return rc;
        }

        rc = DSV_SetItemInArray( g_state.dsv_ctx,
                                 hndl,
                                 g_state.index,
                                 g_state.dsv_val );
    }
    else
    {
        fprintf( stderr, "Wrong parameters\n" );
    }

    return rc;
}

/*!=============================================================================

    Process asv get command, eg,
    sav get -i 3 [123]/SYS/TEST/INT_ARRAY

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
int ProcessGetItem( int argc, char **argv )
{
    int rc = EINVAL;
    int value;
    char dsv_name[DSV_STRING_SIZE_MAX];
    if( optind == argc - 1 )
    {
        /* the last parameters should be dsv name */
        strncpy( dsv_name, argv[optind], DSV_STRING_SIZE_MAX );
        void *hndl = DSV_Handle( g_state.dsv_ctx, dsv_name );
        if( hndl == NULL )
        {
            syslog( LOG_ERR, "Unable to find dsv: %s", dsv_name );
            return rc;
        }

        rc = DSV_GetItemFromArray( g_state.dsv_ctx, hndl, g_state.index, &value );
        if( rc == 0 )
        {
            printf( "%s[%d]=%d\n", dsv_name, g_state.index, value );
        }
        else
        {
            printf( "%s[%d]: wrong result\n", dsv_name, g_state.index );
        }
    }
    else
    {
        fprintf( stderr, "Wrong parameters\n" );
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
int ProcessOptions( int argc, char **argv )
{
    int rc = 0;
    int opt;
    char name[DSV_STRING_SIZE_MAX];

    /* process all the command line options */
    while( (opt = getopt( argc, argv, "i:v:" )) != -1 )
    {
        switch( opt )
        {
        case 'i':
            g_state.index = atoi( optarg );
            break;

        case 'v':
            g_state.dsv_val = atoi( optarg );
            break;

        default:
            rc = EINVAL;
            break;
        }
    }

    if( g_state.operation == ASV_OPINVAL && rc == 0 )
    {
        /* No short option indicate operation, at lease one long argv*/
        if( optind < argc )
        {
            if( strcmp( argv[optind], "set" ) == 0 )
            {
                g_state.operation = ASV_SET;
                optind++;
            }
            else if( strcmp( argv[optind], "get" ) == 0 )
            {
                g_state.operation = ASV_GET;
                optind++;
            }
            else if( strcmp( argv[optind], "add" ) == 0 )
            {
                g_state.operation = ASV_ADD;
                optind++;
            }
            else if( strcmp( argv[optind], "del" ) == 0 )
            {
                g_state.operation = ASV_DEL;
                optind++;
            }
            else if( strcmp( argv[optind], "ins" ) == 0 )
            {
                g_state.operation = ASV_INS;
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

    switch( g_state.operation )
    {
    case ASV_ADD:
        rc = ProcessAddItem( argc, argv );
        break;
    case ASV_SET:
        rc = ProcessSetItem( argc, argv );
        break;
    case ASV_GET:
        rc = ProcessGetItem( argc, argv );
        break;
    case ASV_DEL:
        rc = ProcessDelItem( argc, argv );
        break;
    case ASV_INS:
        rc = ProcessInsItem( argc, argv );
        break;
    default:
        break;
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
