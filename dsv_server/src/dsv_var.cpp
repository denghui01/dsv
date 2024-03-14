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
#include <assert.h>
#include <errno.h>
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include "zmq.h"
#include "dsv.h"
#include "dsv_msg.h"
#include "dsv_log.h"

/*! hasp table to hold the dsv name and dsv info data */
std::unordered_map<std::string, void *> g_map;
using dsv_array_t = std::vector<int>;

#define DSV_SAVE_FILE       ("/var/run/dsv.save")
/*==============================================================================
                              Defines
==============================================================================*/

/*!=============================================================================

    This function fills the forward buffer with the information in
    dsv_info_t structure.

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

    *(dsv_info_t **)fwd_data = dsv;
    fwd_data += sizeof(dsv);
    fwd->length += sizeof(dsv);

    fwd->length += DSV_Memcpy( fwd_data, dsv );
}

/**
 * hash map has full dsv name as key, and dsv_info_t as value
 * the memory should never be released as the dsv server never terminates
 */
int var_create( const char *req_buf, char *fwd_buf )
{
    assert( req_buf );
    assert( fwd_buf );
    printf( "Enter %s\n", __func__ );

    int rc = EINVAL;
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
            dsv->value.pArray = static_cast<void *>(
                new dsv_array_t( (int *)req_data,
                                 (int *)(req_data + dsv->len) ) );
        }

        std::string full_name(dsv->pName);
        auto e = g_map.find( full_name );
        if( e == g_map.end() )
        {
            g_map.insert( std::make_pair( full_name, (void *)dsv ) );
            fill_fwd_buf( full_name.c_str(), dsv, fwd_buf );
            rc = 0;
        }
        else
        {
            dsvlog( LOG_ERR, "dsv existed: %s", full_name.c_str() );
            rc = EEXIST;
        }
    }

    if( rc == EEXIST )
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
 */
int var_set( const char *req_buf, char *fwd_buf )
{
    assert( req_buf );
    assert( fwd_buf );
    printf( "Enter %s\n", __func__ );

    int rc = EINVAL;
    struct timespec now = { 0 };
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    dsv_info_t *dsv = *(dsv_info_t **)req_data;
    req_data += sizeof(dsv);
    if( dsv != NULL )
    {
        clock_gettime( CLOCK_REALTIME, &now );
        dsv->timestamp = now;
        dsv->dirty = 1;
        if( dsv->type == DSV_TYPE_STR )
        {
            free( dsv->value.pStr );
            dsv->value.pStr = strdup( req_data );
        }
        else if( dsv->type == DSV_TYPE_INT_ARRAY )
        {
            delete (dsv_array_t *)dsv->value.pArray;
            dsv->value.pArray = static_cast<void *>
                (new dsv_array_t( (int *)req_data,
                                  (int *)(req_data + dsv->len) ) );
        }
        else
        {
            memcpy( &dsv->value, req_data, sizeof(dsv_value_t) );
        }

        fill_fwd_buf( dsv->pName, dsv, fwd_buf );
        rc = 0;
    }
    return rc;
}


int var_add_item( const char *req_buf, char *fwd_buf )
{
    assert( req_buf );
    assert( fwd_buf );
    printf( "Enter %s\n", __func__ );

    int rc = EINVAL;
    struct timespec now = { 0 };
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    dsv_info_t *dsv = *(dsv_info_t **)req_data;
    req_data += sizeof(dsv);
    int value = *(int *)req_data;
    if( dsv != NULL && dsv->type == DSV_TYPE_INT_ARRAY )
    {
        clock_gettime( CLOCK_REALTIME, &now );
        dsv->timestamp = now;
        dsv->dirty = 1;

        dsv_array_t *ai =  (dsv_array_t *)dsv->value.pArray;
        ai->push_back(value);
        dsv->len = ai->size() * sizeof(int);
        fill_fwd_buf( dsv->pName, dsv, fwd_buf );
        rc = 0;
    }
    return rc;
}

int var_set_item( const char *req_buf, char *fwd_buf )
{
    assert( req_buf );
    assert( fwd_buf );
    printf( "Enter %s\n", __func__ );

    int rc = EINVAL;
    struct timespec now = { 0 };
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    dsv_info_t *dsv = *(dsv_info_t **)req_data;
    req_data += sizeof(dsv);
    int index = *(int *)req_data;
    req_data += sizeof(index);
    int value = *(int *)req_data;
    if( dsv != NULL && dsv->type == DSV_TYPE_INT_ARRAY )
    {
        clock_gettime( CLOCK_REALTIME, &now );
        dsv->timestamp = now;
        dsv->dirty = 1;

        dsv_array_t *ai =  (dsv_array_t *)dsv->value.pArray;
        (*ai)[index] = value;

        fill_fwd_buf( dsv->pName, dsv, fwd_buf );
        rc = 0;
    }
    return rc;
}

int var_ins_item( const char *req_buf, char *fwd_buf )
{
    assert( req_buf );
    assert( fwd_buf );
    printf( "Enter %s\n", __func__ );

    int rc = EINVAL;
    struct timespec now = { 0 };
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    dsv_info_t *dsv = *(dsv_info_t **)req_data;
    req_data += sizeof(dsv);
    int index = *(int *)req_data;
    req_data += sizeof(index);
    int value = *(int *)req_data;
    if( dsv != NULL && dsv->type == DSV_TYPE_INT_ARRAY )
    {
        clock_gettime( CLOCK_REALTIME, &now );
        dsv->timestamp = now;
        dsv->dirty = 1;

        dsv_array_t *ai =  (dsv_array_t *)dsv->value.pArray;
        auto it = ai->begin();
        ai->insert(std::next(it, index), value);
        dsv->len = ai->size() * sizeof(int);

        fill_fwd_buf( dsv->pName, dsv, fwd_buf );
        rc = 0;
    }
    return rc;
}

int var_del_item( const char *req_buf, char *fwd_buf )
{
    assert( req_buf );
    assert( fwd_buf );
    printf( "Enter %s\n", __func__ );

    int rc = EINVAL;
    struct timespec now = { 0 };
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    dsv_info_t *dsv = *(dsv_info_t **)req_data;
    req_data += sizeof(dsv);
    int index = *(int *)req_data;
    if( dsv != NULL && dsv->type == DSV_TYPE_INT_ARRAY )
    {
        clock_gettime( CLOCK_REALTIME, &now );
        dsv->timestamp = now;
        dsv->dirty = 1;

        dsv_array_t *ai = (dsv_array_t *)dsv->value.pArray;
        auto it = ai->begin();
        ai->erase(std::next(it, index));
        dsv->len = ai->size() * sizeof(int);

        fill_fwd_buf( dsv->pName, dsv, fwd_buf );
        rc = 0;
    }
    return rc;
}

int var_get_item( const char *req_buf, char *rep_buf )
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

    dsv_info_t *dsv = *(dsv_info_t **)req_data;
    int index = *(int *)(req_data + sizeof(dsv_info_t *));
    if( dsv != NULL && dsv->type == DSV_TYPE_INT_ARRAY )
    {
        dsv_array_t *ai = (dsv_array_t *)dsv->value.pArray;
        if( index < ai->size() )
        {
            *(int *)rep_data = (*ai)[index];
            rep->length += sizeof(int);
            rc = 0;
        }
    }
    return rc;
}

/**
 *
 */
int var_get_handle( const char *req_buf, char *rep_buf )
{
    assert( req_buf );
    assert( rep_buf );
    printf( "Enter %s\n", __func__ );

    int rc = ENOENT;
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    char *rep_data = rep->data;
    rep->length = sizeof(dsv_msg_reply_t);

    std::string full_name(req_data);
    auto e = g_map.find( full_name );
    if( e != g_map.end() )
    {
        *(void **)rep_data = e->second;
        rc = 0;
        rep->length += sizeof(void *);
    }
    return rc;
}

/**
 *
 */
int var_get_type( const char *req_buf, char *rep_buf )
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

    dsv_info_t *pDsv = *(dsv_info_t **)req_data;
    if( pDsv != NULL )
    {
        *(int *)rep_data = pDsv->type;
        rc = 0;
        rep->length += sizeof(int);
    }
    return rc;
}

/**
 *
 */
int var_get_len( const char *req_buf, char *rep_buf )
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

    dsv_info_t *pDsv = *(dsv_info_t **)req_data;
    if( pDsv != NULL )
    {
        *(size_t *)rep_data = pDsv->len;
        rc = 0;
        rep->length += sizeof(size_t);
    }
    return rc;
}

int var_get( const char *req_buf, char *rep_buf )
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

    dsv_info_t *pDsv = *(dsv_info_t **)req_data;
    if( pDsv != NULL )
    {
        rep->length += DSV_Memcpy( rep_data, pDsv );
        rc = 0;
    }
    return rc;
}

int var_get_next( const char *req_buf, char *rep_buf )
{
    assert( req_buf );
    assert( rep_buf );
    printf( "Enter %s\n", __func__ );

    int rc = ENOENT;
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    char *rep_data = rep->data;
    rep->length = sizeof(dsv_msg_reply_t);

    int last_index = *(int *)req_data;
    int index = -1;
    std::string search_name( req_data + sizeof( int ) );
    for( auto [dsv_name, dsv_info] : g_map )
    {
        if( dsv_name.find( search_name ) != -1 )
        {
            if( ++index > last_index )
            {
                /* fill index */
                *(int *)rep_data = index;
                rep->length += sizeof(int);

                /* fill name */
                rep_data += sizeof(int);
                strcpy( rep_data, dsv_name.c_str() );
                rep->length += strlen( rep_data ) + 1;

                /* fill value */
                rep_data += strlen( rep_data ) + 1;
                dsv_info_t *pDsv = (dsv_info_t *)dsv_info;
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
        char *full_name( &sub_buf[1] );
        auto e = g_map.find( full_name );
        if( e != g_map.end() )
        {
            dsv_info_t *pDsv = (dsv_info_t *)e->second;
            printf( "Subscribe %s\n", full_name );
            fill_fwd_buf( full_name, pDsv, fwd_buf );
            rc = 0;
        }
    }
    return rc;
}

/**
*@return 
* -1: fail 
*/
int var_save()
{
    int rc = -1;
    printf( "Enter %s\n", __func__ );

    std::string filename{ DSV_SAVE_FILE };
    std::fstream s{ filename, s.binary | s.in | s.out | s.app };
    char value_buf[DSV_STRING_SIZE_MAX];

    if (!s.is_open())
    {
        printf( "Failed to open %s\n", filename.c_str() );
        return rc;
    }

    dsv_info_t *pDsv;
    for( auto [dsv_name, dsv_info] : g_map )
    {
        dsv_info_t *pDsv = (dsv_info_t *)dsv_info;
        if( pDsv->dirty && ( pDsv->flags & DSV_FLAG_SAVE ) )
        {
            rc = DSV_Value2Str( value_buf,
                                DSV_STRING_SIZE_MAX,
                                pDsv );
            if( rc != -1 )
            {
                s << dsv_name << "=" << value_buf << ";";
                rc = 0;
            }
            pDsv->dirty = 0;
        }
    }
    s.flush();
    return rc;
}

/** 
* This function should be called after all dsvs are created
* the dsv.save file should be like this
* [123]/SYS/TEST/U16=16;[123]/SYS/TEST/U32=32;
*@return 
* -1: fail 
*/
int var_restore()
{
    int rc = -1;
    printf( "Enter %s\n", __func__ );

    std::string filename{ DSV_SAVE_FILE };

    /* read entire file into string */
    if (std::ifstream ifs{ filename, ifs.binary | ifs.ate })
    {
        auto size = ifs.tellg();
        std::string save_str( size, '\0' );
        ifs.seekg(0);
        ifs.read( &save_str[0], size );

        int out_pos = 0; // position of after ; delimiter
        int in_pos = 0;  // position of after = delimiter
        while( out_pos < save_str.size() )
        {
            in_pos = save_str.find( "=", out_pos );
            std::string sv_name = save_str.substr( out_pos, in_pos - out_pos );
            ++in_pos; // skip '='

            out_pos = save_str.find( ";", out_pos );
            std::string sv_value = save_str.substr( in_pos, out_pos - in_pos );
            ++out_pos; // skip ';'

            auto e = g_map.find( sv_name );
            if( e != g_map.end() )
            {
                dsv_info_t *pDsv = (dsv_info_t *)e->second;
                DSV_Str2Value( sv_value.c_str(), pDsv );
            }
        }
        rc = 0;
    }
    else
    {
        std::cout << "Failed to open " << filename << std::endl;
    }
    return rc;
}

