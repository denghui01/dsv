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
std::unordered_map<std::string, void *> g_map;

/*==============================================================================
                              Defines
==============================================================================*/
void fill_fwd_buf( const char *full_name, dsv_info_t *dsv, char *fwd_buf )
{
    assert(dsv);
    assert(fwd_buf);
    assert(full_name);

    dsv_msg_forward_t *fwd = (dsv_msg_forward_t *)fwd_buf;
    char *fwd_data = fwd->data;

    fwd->length = 0;
    strcpy( fwd_data, full_name );
    fwd->length += strlen(full_name) + 1;
    if( dsv->type == DSV_TYPE_STR )
    {
        strcpy( fwd_data + fwd->length, dsv->value.pData );
        fwd->length += strlen( dsv->value.pData ) + 1;
    }
    else
    {
        memcpy( fwd_data + fwd->length, &dsv->value, sizeof(dsv_value_t) );
        fwd->length += sizeof(dsv_value_t);
    }
}

/**
* hash map has full dsv name as key, and dsv_info_t as value
* the memory should never be released as the dsv server never terminates
* message buffer data structure:
+===============================+
|      dsv_create_buffer        |
+===============================+
| request(type, length, instID) |
+-------------------------------+
| dsv_info_t                    |
+-------------------------------+
| data: name, desc, tags, value |
+-------------------------------+
*/
int var_create( const char *req_buf, char *fwd_buf )
{
    assert( req_buf );
    assert( fwd_buf );
    printf( "Enter %s\n", __func__ );

    int rc = 0;
    struct timespec now = {0};
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    /* full_name and dsv will be put into hash table */
    dsv_info_t *dsv = (dsv_info_t *)malloc( sizeof(dsv_info_t) );
    if( dsv != NULL )
    {
        /* fill dsv */
        memcpy(dsv, req_data, sizeof(dsv_info_t));
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
            dsv->value.pData = strdup( req_data );
        }

        std::stringstream ss;
        ss << "[" << req->instID << "]" << dsv->pName;
        std::string full_name = ss.str();
        auto e = g_map.find(full_name);
        if ( e == g_map.end() )
        {
            g_map.insert(std::make_pair(full_name, (void *)dsv));
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
        free( dsv->pName);
        free( dsv->pDesc);
        free( dsv->pTags);
        if( dsv->type == DSV_TYPE_STR )
        {
            free( dsv->value.pData );
        }
        free( dsv );
    }
    return rc;
}

/**
* message buffer data structure:
+==============================+
|         dsv_msg_set          |
+==============================+
| common(type, length, instID) |
+------------------------------+
| data:hndl, value, client     |
+------------------------------+
*/
int var_set( const char *req_buf, char *fwd_buf )
{
    assert( req_buf );
    assert( fwd_buf );
    printf( "Enter %s\n", __func__ );

    int rc = 0;
    struct timespec now = {0};
    char full_name[DSV_STRING_SIZE_MAX];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    dsv_info_t *pDsv = *(dsv_info_t **)req_data;
    req_data += sizeof(dsv_info_t *);
    if( pDsv != NULL )
    {
        snprintf( full_name, DSV_STRING_SIZE_MAX, "[%d]%s", pDsv->instID, pDsv->pName);
        clock_gettime( CLOCK_REALTIME, &now );
        pDsv->timestamp = now;
        if( pDsv->type == DSV_TYPE_STR )
        {
            free( pDsv->value.pData );
            pDsv->value.pData = strdup( req_data );
        }
        else
        {
            memcpy(&pDsv->value, req_data, sizeof(dsv_value_t));
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
    +==============================+
    |    dsv_msg_get_handle        |
    +==============================+
    | common(type, length, instID) |
    +------------------------------+
    | data: name                   |
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
    auto e = g_map.find(full_name);
    if ( e == g_map.end() )
    {
        printf("Unable to find the sysvar %s\n", full_name.c_str());
        rc = EINVAL;
        rep->result = rc;
    }
    else
    {
        *(void **)rep_data = e->second;
        rc = 0;
        rep->result = rc;
        rep->length += sizeof(void *);
    }
    return rc;
}

/**
*
    +==============================+
    |    dsv_msg_get_handle        |
    +==============================+
    | common(type, length, instID) |
    +------------------------------+
    | data: name                   |
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
        rep->result = rc;
        rep->length += sizeof(int);
    }
    else
    {
        rc = EINVAL;
        rep->result = rc;
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
        if( pDsv->type == DSV_TYPE_STR )
        {
            strcpy( rep_data, pDsv->value.pData );
            rep->length += strlen(rep_data) + 1;
        }
        else
        {
            memcpy( rep_data, &pDsv->value, sizeof(dsv_value_t) );
            rep->length += sizeof(dsv_value_t);
        }
        rc = 0;
        rep->result = rc;
    }
    else
    {
        rc = EINVAL;
        rep->result = rc;
    }
    return rc;
}

int var_notify( char *sub_buf, char *fwd_buf )
{
    assert( sub_buf );
    assert( fwd_buf );
    printf( "Enter %s\n", __func__ );

    /* byte 0 is the subscription flag */
    char sub_flag = sub_buf[0];

    dsv_msg_forward_t *fwd = (dsv_msg_forward_t *)fwd_buf;
    char *fwd_data = fwd->data;
    fwd->length = 0;
    int rc = EINVAL;

    std::string full_name( &sub_buf[1] );
    auto e = g_map.find(full_name);
    if ( e != g_map.end() )
    {
        dsv_info_t *pDsv = (dsv_info_t *)e->second;
        printf( "Subscribe %s\n", full_name.c_str() );
        strcpy( fwd_data, full_name.c_str() );
        fwd->length += full_name.length() + 1;
        if( pDsv->type == DSV_TYPE_STR )
        {
            strcpy( fwd_data + fwd->length, pDsv->value.pData );
            fwd->length += strlen(pDsv->value.pData) + 1;
        }
        else
        {
            memcpy( fwd_data + fwd->length, &pDsv->value, sizeof(dsv_value_t) );
            fwd->length += sizeof(dsv_value_t);
        }
        rc = 0;
    }

    return rc;
}

