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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <assert.h>
#include <errno.h>
#include <unordered_map>
#include <string>
#include <sstream>
#include "zmq.h"
#include "dsv.h"
#include "dsv_msg.h"

/*! hasp table to hold the dsv name and dsv info data */
std::unordered_map< std::string, void * > g_map;

/*==============================================================================
                              Defines
==============================================================================*/

/*!=============================================================================

    This function fills the forward buffer with the information in
    dsv_info_t structure.
    The forward buffer is a data structure:
    +---------------------------+
    | length                    |
    +---------------------------+
    | full_name                 |
    +---------------------------+
    | value (string or numeric) |
    +---------------------------+

@param[in]
    dest
        destination buffer

@param[in]
    dsv
        pointer of dsv information structure, holding type, len and value

@return
    positive number - number of bytes copied
    -1 - failed

==============================================================================*/
void fill_fwd_buf( const char *full_name, dsv_info_t *dsv, char *fwd_buf )
{
    assert( dsv );
    assert( fwd_buf );
    assert( full_name );

    dsv_msg_forward_t *fwd = (dsv_msg_forward_t *)fwd_buf;
    char *fwd_data = fwd->data;

    fwd->length = 0;
    strcpy( fwd_data, full_name );
    fwd->length += strlen( full_name ) + 1;
    fwd_data += strlen( full_name ) + 1;

    fwd->length += DSV_Memcpy( fwd_data, dsv );
}

/**
 * hash map has full dsv name as key, and dsv_info_t as value
 * the memory should never be released as the dsv server never terminates
 * message buffer data structure:
 * +===============================+
 * |      dsv_create_buffer        |
 * +===============================+
 * | request(type, length, instID) |
 * +-------------------------------+
 * | dsv_info_t                    |
 * +-------------------------------+
 * | data: name, desc, tags, value |
 * +-------------------------------+
 */
int var_create( const char *req_buf, char *fwd_buf )
{
    assert( req_buf );
    assert( fwd_buf );
    printf( "Enter %s\n", __func__ );

    int rc = 0;
    struct timespec now = { 0 };
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    /* full_name and dsv will be put into hash table */
    dsv_info_t *dsv = (dsv_info_t *)malloc( sizeof(dsv_info_t) );
    if( dsv != NULL )
    {
        /* fill dsv */
        memcpy( dsv, req_data, sizeof(dsv_info_t) );
        clock_gettime( CLOCK_REALTIME, &now );
        dsv->timestamp = now;

        req_data += sizeof(dsv_info_t);
        dsv->pName = strdup( req_data );

        req_data += strlen( req_data ) + 1;
        dsv->pDesc = strdup( req_data );

        req_data += strlen( req_data ) + 1;
        dsv->pTags = strdup( req_data );

        req_data += strlen( req_data ) + 1;
        if( dsv->type == DSV_TYPE_STR )
        {
            dsv->value.pStr = strdup( req_data );
        }
        else if( dsv->type == DSV_TYPE_INT_ARRAY )
        {
            dsv->value.pArray = memdup( req_data, dsv->len );
        }

        std::stringstream ss;
        ss << "[" << req->instID << "]" << dsv->pName;
        std::string full_name = ss.str();
        auto e = g_map.find( full_name );
        if( e == g_map.end() )
        {
            g_map.insert( std::make_pair( full_name, (void *)dsv ) );
            fill_fwd_buf( full_name.c_str(), dsv, fwd_buf );
        }
        else
        {
            syslog( LOG_ERR, "dsv existed: %s", full_name.c_str() );
            rc = EEXIST;
        }
    }

    if( rc != 0 )
    {
        free( dsv->pName );
        free( dsv->pDesc );
        free( dsv->pTags );
        if( dsv->type == DSV_TYPE_STR )
        {
            free( dsv->value.pStr );
        }
        if( dsv->type == DSV_TYPE_INT_ARRAY )
        {
            free( dsv->value.pArray );
        }
        free( dsv );
    }
    return rc;
}

/**
 * message buffer data structure:
 * +==============================+
 * |         dsv_msg_set          |
 * +==============================+
 * | common(type, length, instID) |
 * +------------------------------+
 * | data:hndl, value, client     |
 * +------------------------------+
 */
int var_set( const char *req_buf, char *fwd_buf )
{
    assert( req_buf );
    assert( fwd_buf );
    printf( "Enter %s\n", __func__ );

    int rc = 0;
    struct timespec now = { 0 };
    char full_name[DSV_STRING_SIZE_MAX];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    dsv_info_t *pDsv = *(dsv_info_t **)req_data;
    req_data += sizeof(dsv_info_t *);
    if( pDsv != NULL )
    {
        snprintf( full_name, DSV_STRING_SIZE_MAX, "[%d]%s", pDsv->instID, pDsv->pName );
        clock_gettime( CLOCK_REALTIME, &now );
        pDsv->timestamp = now;
        if( pDsv->type == DSV_TYPE_STR )
        {
            free( pDsv->value.pStr );
            pDsv->value.pStr = strdup( req_data );
        }
        else if( pDsv->type == DSV_TYPE_INT_ARRAY )
        {
            free( pDsv->value.pArray );
            pDsv->value.pArray = memdup( req_data, pDsv->len );
        }
        else
        {
            memcpy( &pDsv->value, req_data, sizeof(dsv_value_t) );
        }

        fill_fwd_buf( full_name, pDsv, fwd_buf );
        rc = 0;
    }
    else
    {
        rc = EINVAL;
    }
    return rc;
}

/**
 *
 * +==============================+
 * |    dsv_msg_get_handle        |
 * +==============================+
 * | common(type, length, instID) |
 * +------------------------------+
 * | data: name                   |
 *   +------------------------------+
 */
int var_get_handle( const char *req_buf, char *rep_buf )
{
    assert( req_buf );
    assert( rep_buf );
    printf( "Enter %s\n", __func__ );

    int rc = 0;
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    char *rep_data = rep->data;
    rep->length = sizeof(dsv_msg_reply_t);

    std::stringstream ss;
    ss << "[" << req->instID << "]" << req_data;
    std::string full_name = ss.str();
    auto e = g_map.find( full_name );
    if( e == g_map.end() )
    {
        printf( "Unable to find the sysvar %s\n", full_name.c_str() );
        rc = EINVAL;
    }
    else
    {
        *(void **)rep_data = e->second;
        rc = 0;
        rep->length += sizeof(void *);
    }
    return rc;
}

/**
 *
 * +==============================+
 * |    dsv_msg_get_handle        |
 * +==============================+
 * | common(type, length, instID) |
 * +------------------------------+
 * | data: name                   |
 *   +------------------------------+
 */
int var_get_type( const char *req_buf, char *rep_buf )
{
    assert( req_buf );
    assert( rep_buf );
    printf( "Enter %s\n", __func__ );

    int rc = 0;
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    char *rep_data = rep->data;
    rep->length = sizeof(dsv_msg_reply_t);

    dsv_info_t *pDsv = *(dsv_info_t **)req_data;
    if( pDsv != NULL )
    {
        *(int *)rep_data = pDsv->type;
        rc = 0;
        rep->length += sizeof(int);
    }
    else
    {
        rc = EINVAL;
    }
    return rc;
}


int var_get( const char *req_buf, char *rep_buf )
{
    assert( req_buf );
    assert( rep_buf );
    printf( "Enter %s\n", __func__ );

    int rc = 0;
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    char *rep_data = rep->data;
    rep->length = sizeof(dsv_msg_reply_t);

    dsv_info_t *pDsv = *(dsv_info_t **)req_data;
    if( pDsv != NULL )
    {
        rep->length += DSV_Memcpy( rep_data, pDsv );
        rc = 0;
    }
    else
    {
        rc = EINVAL;
    }
    return rc;
}


int var_get_next( const char *req_buf, char *rep_buf )
{
    assert( req_buf );
    assert( rep_buf );
    printf( "Enter %s\n", __func__ );

    int rc = EINVAL;
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    char *rep_data = rep->data;
    rep->length = sizeof(dsv_msg_reply_t);

    int last_index = *(int *)req_data;
    int index = -1;
    std::string search_name( req_data + sizeof( int ) );
    for( auto e : g_map )
    {
        if( e.first.find( search_name ) != -1 )
        {
            if( ++index > last_index )
            {
                /* fill index */
                *(int *)rep_data = index;
                rep->length += sizeof(int);

                /* fill name */
                rep_data += sizeof(int);
                strcpy( rep_data, e.first.c_str() );
                rep->length += strlen( rep_data ) + 1;

                /* fill value */
                rep_data += strlen( rep_data ) + 1;
                dsv_info_t *pDsv = (dsv_info_t *)e.second;
                rep->length += DSV_Value2Str( rep_data,
                                              DSV_STRING_SIZE_MAX,
                                              pDsv );
                rc = 0;
                break;
            }
        }
    }
    return rc;
}


int var_notify( char *sub_buf, char *fwd_buf )
{
    assert( sub_buf );
    assert( fwd_buf );
    printf( "Enter %s\n", __func__ );

    int rc = EINVAL;
    /* byte 0 is the subscription flag */
    char sub_flag = sub_buf[0];
    if( sub_flag == 1 )
    {
        /* fill the forward buffer */
        std::string full_name( &sub_buf[1] );
        auto e = g_map.find( full_name );
        if( e != g_map.end() )
        {
            dsv_info_t *pDsv = (dsv_info_t *)e->second;
            printf( "Subscribe %s\n", full_name.c_str() );
            fill_fwd_buf( full_name.c_str(), pDsv, fwd_buf );
            rc = 0;
        }
    }
    return rc;
}

