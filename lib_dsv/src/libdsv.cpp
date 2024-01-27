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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <assert.h>
#include <inttypes.h>
#include "zmq.h"
#include "czmq.h"
#include "dsv.h"
#include "dsv_msg.h"
#include "cjson/cJSON.h"


/*==============================================================================
                               Macros
==============================================================================*/
/*! zmq endpoint for tcp to cross node */
/*! TODO: the ip should come from auto detection */
#define DSV_FRONTEND            ( "tcp://localhost:56789" )
#define DSV_BACKEND             ( "tcp://localhost:56788" )
#define DSV_REPLY               ( "tcp://localhost:56787" )

/*==============================================================================
                                Enums
==============================================================================*/

/*=============================================================================
                              Structures
==============================================================================*/

/*! internal structure maintained by the library to manage connections with
 * the dsv server */
typedef struct dsv_context
{
    void *zmq_ctx;
    void *sock_request;
    void *sock_publish;
    void *sock_subscribe;

} dsv_context_t;

/*==============================================================================
                        Local/Private Function Protoypes
==============================================================================*/
/*!=============================================================================

    Convert a string into the value based on the dsv type

@param[in]
    str
        C string

@param[in]
    pDsv
        pointer to the dsv information

@return
    0 - success
    any other value specifies an error code (see errno.h)

==============================================================================*/
int DSV_Str2Value( const char *str, dsv_info_t *pDsv )
{
    int rc = 0;
    switch( pDsv->type )
    {
    case DSV_TYPE_STR:
        pDsv->value.pData = strdup(str);
        break;
    case DSV_TYPE_UINT32:
        pDsv->value.u32 = strtoul( str, NULL, 0 );
        break;
    case DSV_TYPE_UINT16:
        pDsv->value.u16 = strtoul( str, NULL, 0 );
        break;
    case DSV_TYPE_UINT8:
        pDsv->value.u8 = strtoul( str, NULL, 0 );
        break;
    case DSV_TYPE_SINT32:
        pDsv->value.s32 = strtol( str, NULL, 0 );
        break;
    case DSV_TYPE_SINT16:
        pDsv->value.s16 = strtol( str, NULL, 0 );
        break;
    case DSV_TYPE_SINT8:
        pDsv->value.s8 = strtol( str, NULL, 0 );
        break;
    case DSV_TYPE_UINT64:
        pDsv->value.u64 = strtoull( str, NULL, 0 );
        break;
    case DSV_TYPE_SINT64:
        pDsv->value.s64 = strtoll( str, NULL, 0 );
        break;
    case DSV_TYPE_FLOAT:
        pDsv->value.f32 = strtof( str, NULL );
        break;
    case DSV_TYPE_DOUBLE:
        pDsv->value.f64 = strtold( str, NULL );
        break;
    default:
        syslog( LOG_ERR, "Invalid dsv type to convert!" );
        rc = EINVAL;
        break;
    }
    return rc;
}

/*!=============================================================================

    Convert a value to string based on the dsv type

@param[in]
    str
        C string

@param[in]
    pDsv
        pointer to the dsv information

@param[out]
    buf
        pointer to the strin buffer

@param[in]
    len
        size of buffer
@return
    0 - success
    any other value specifies an error code (see errno.h)

==============================================================================*/
int DSV_Value2Str( char *buf, size_t len, const dsv_info_t *pDsv )
{
    assert(pDsv);
    assert(buf);

    int rc = 0;
    switch( pDsv->type )
    {
    case DSV_TYPE_STR:
        snprintf( buf, len, "%s", pDsv->value.pData );
        break;
    case DSV_TYPE_UINT16:
        snprintf( buf, len, "%" PRIu16, pDsv->value.u16 );
        break;
    case DSV_TYPE_UINT32:
        snprintf( buf, len, "%" PRIu32, pDsv->value.u32 );
        break;
    case DSV_TYPE_UINT64:
        snprintf( buf, len, "%" PRIu64, pDsv->value.u64 );
        break;
    case DSV_TYPE_UINT8:
        snprintf( buf, len, "%" PRIu8, pDsv->value.u8 );
        break;
    case DSV_TYPE_SINT16:
        snprintf( buf, len, "%" PRId16, pDsv->value.s16 );
        break;
    case DSV_TYPE_SINT32:
        snprintf( buf, len, "%" PRId32, pDsv->value.s32 );
        break;
    case DSV_TYPE_SINT64:
        snprintf( buf, len, "%" PRId64, pDsv->value.s64 );
        break;
    case DSV_TYPE_SINT8:
        snprintf( buf, len, "%" PRId8, pDsv->value.s8 );
        break;
    case DSV_TYPE_FLOAT:
        snprintf( buf, len, "%f", pDsv->value.f32 );
        break;
    case DSV_TYPE_DOUBLE:
        snprintf( buf, len, "%f", pDsv->value.f64 );
        break;
    default:
        printf("Unsupported type of dsv!\n");
        rc = EINVAL;
        break;
    }

    return rc;
}

void DSV_Print( const dsv_info_t *pDsv )
{
    assert(pDsv);
    printf( "name:  \t%s\n", pDsv->pName );
    printf( "desc:  \t%s\n", pDsv->pDesc );
    printf( "tags:  \t%s\n", pDsv->pTags );
    printf( "instID:\t%d\n", pDsv->instID );
    printf( "type:  \t%d\n", pDsv->type );
    printf( "ts:    \t%ld\n", pDsv->timestamp.tv_sec );
    char vstr[DSV_STRING_SIZE_MAX];
    DSV_Value2Str( vstr, DSV_STRING_SIZE_MAX, pDsv );
    printf( "value: \t%s\n", vstr );
    printf("================\n");
}

/*!=============================================================================

    Convert a double value into the proper value based on the dsv type,
    since cJson store numeric value as double

@param[in]
    df
        double float value

@param[in]
    pDsv
        pointer to the dsv information

@return
    0 - success
    any other value specifies an error code (see errno.h)

==============================================================================*/
int dsv_Double2Value( double df, dsv_info_t *pDsv )
{
    assert(pDsv);
    int rc = 0;
    switch( pDsv->type )
    {
    case DSV_TYPE_UINT32:
        pDsv->value.u32 = (uint32_t)df;
        break;
    case DSV_TYPE_UINT16:
        pDsv->value.u16 = (uint16_t)df;
        break;
    case DSV_TYPE_UINT8:
        pDsv->value.u8 = (uint8_t)df;
        break;
    case DSV_TYPE_SINT32:
        pDsv->value.s32 = (int32_t)df;
        break;
    case DSV_TYPE_SINT16:
        pDsv->value.s16 = (int16_t)df;
        break;
    case DSV_TYPE_SINT8:
        pDsv->value.s8 = (int8_t)df;
        break;
    case DSV_TYPE_UINT64:
        pDsv->value.u64 = (uint64_t)df;
        break;
    case DSV_TYPE_SINT64:
        pDsv->value.s64 = (int64_t)df;
        break;
    case DSV_TYPE_FLOAT:
        pDsv->value.f32 = (float)df;
        break;
    case DSV_TYPE_DOUBLE:
        pDsv->value.f64 = df;
        break;
    default:
        syslog( LOG_ERR, "Invalid dsv type to convert!" );
        rc = EINVAL;
        break;
    }

    return rc;
}
/*!=============================================================================

    Send a message to the server, and get the feedback from it on demand

@param[in]
    ctx
        dsv ctx ( returned by DSV_Open() )

@param[in]
    mbuf
        message buffer

@param[in]
    mlen
        message buffer length

@param[in]
    vbuf
        value buffer if it is not NULL

@param[in]
    vlen
        length of the value buffer

@return
    0 - success
    any other value specifies an error code (see errno.h)

==============================================================================*/
static int dsv_SendMsg(void *ctx,
                       void *req_buf,
                       size_t req_len,
                       void *rep_buf,
                       size_t rep_len)
{
    assert( ctx );
    assert( req_buf );

    int rc;
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    dsv_context_t *dsv_ctx = (dsv_context_t *)ctx;

    if( req->type == DSV_MSG_CREATE || req->type == DSV_MSG_SET )
    {
        /* create and set only use pub/sub pattern */
        rc = zmq_send( dsv_ctx->sock_publish, req_buf, req_len, 0 );
        if( rc == -1 )
        {
            syslog( LOG_ERR, "zmq_send failed: %s", strerror( errno ) );
            return EFAULT;
        }
        else
        {
            return 0;
        }
    }
    else if( req->type == DSV_MSG_GET_HANDLE ||
             req->type == DSV_MSG_GET_TYPE   ||
             req->type == DSV_MSG_GET )
    {
        if( req_buf != NULL )
        {
            rc = zmq_send( dsv_ctx->sock_request, req_buf, req_len, 0 );
            if( rc == -1 )
            {
                syslog( LOG_ERR, "zmq_send failed: %s", strerror( errno ) );
                return EFAULT;
            }
            else
            {
                rc = zmq_recv( dsv_ctx->sock_request, rep_buf, rep_len, 0 );
                if( rc == -1 )
                {
                    syslog( LOG_ERR, "zmq_recv failed: %s", strerror( errno ) );
                    return EFAULT;
                }
                else
                {
                    return rep->result;
                }
            }
        }
    }

    return 0;
}

static int dsv_RecvMsg(void *ctx,
                       void *sub_buf,
                       size_t size)
{
    assert( ctx );
    assert( sub_buf );

    int rc;
    dsv_context_t *dsv_ctx = (dsv_context_t *)ctx;
    rc = zmq_recv( dsv_ctx->sock_subscribe, sub_buf, size, 0 );
    if( rc == -1 )
    {
        syslog( LOG_ERR, "zmq_recv failed: %s", zmq_strerror(errno));
        return EFAULT;
    }
    return 0;
}
/*!=============================================================================

    Read the JSON file into the buffer

@param[in]
    file
        JSON file name

@param[in]
    buf
        buffer to hold the JSON file content

@param[in]
    size
        size of buffer

@return
    0 - success
    others - failed

==============================================================================*/
static int dsv_ReadJsonFile( const char *file, char *buf, size_t size )
{
    assert( file );
    assert( buf );

    int rc = 0;
    FILE *fp;
    int  file_size, read_size;

    /* Open the JSON file for reading */
    fp = fopen( file, "r" );
    if( fp != NULL )
    {
        /* Determine the size of the file */
        fseek( fp, 0L, SEEK_END );
        file_size = ftell( fp );
        rewind( fp );

        /* Allocate memory for the file contents */
        if( file_size < size )
        {
            /* Read the file contents into memory */
            read_size = fread( buf, 1, file_size, fp );
            if( read_size != file_size )
            {
                syslog( LOG_ERR, "Failed to read json file: %s", file );
                rc = EINVAL;
            }
        }
        else
        {
            syslog( LOG_ERR, "Json file exceeds maximum size: %s", file );
            rc = EINVAL;
        }
        fclose( fp );
    }
    else
    {
        syslog( LOG_ERR, "Failed to open json file: %s.", file );
        rc = EINVAL;
    }

    return rc;
}

/*!=============================================================================

    Parse the JSON buffer and call DSV_Create to create dsv

@param[in]
    ctx
        dsv ctx ( returned by DSV_Open() )

@param[in]
    instID
        dsv instance ID passed by the creator, normally device instance ID

@param[in]
    buf
        pointer to the JSON string

@return
    0 - success
    others - failed

==============================================================================*/
static int dsv_ParseJsonStr( void *ctx, uint32_t instID, char *buf )
{
    assert( ctx );
    assert( buf );

    dsv_info_t dsv = {0};
    int rc = EINVAL;
    int n;
    int i;
    cJSON *e;
    cJSON *m;
    cJSON *root = cJSON_Parse( buf );
    n = cJSON_GetArraySize( root );
    dsv.instID = instID;

    for( i = 0; i < n; i++ )
    {
        e = cJSON_GetArrayItem( root, i );

        /* handle dsv name */
        m = cJSON_GetObjectItem(e, "name");
        if( cJSON_IsString(m) && m->valuestring != NULL )
        {
            //printf("name:%s\n",m->valuestring);
            dsv.pName = strdup( m->valuestring );
        }

        /* handle dsv description */
        m = cJSON_GetObjectItem(e, "description");
        if( cJSON_IsString(m) && m->valuestring != NULL )
        {
            //printf("desc:%s\n",m->valuestring);
            dsv.pDesc = strdup( m->valuestring );
        }

        /* handle dsv tags */
        m = cJSON_GetObjectItem(e, "tags");
        if( cJSON_IsString(m) && m->valuestring != NULL )
        {
            //printf("tags:%s\n",m->valuestring);
            dsv.pTags = strdup( m->valuestring );
        }

        /* handle dsv type first */
        m = cJSON_GetObjectItem(e, "type");
        if( cJSON_IsString(m) && m->valuestring != NULL )
        {
            //printf("type:%s\n",m->valuestring);
            dsv.type = DSV_GetTypeFromStr( m->valuestring );
            dsv.len = DSV_GetSizeFromType( dsv.type );
        }

        /* handle dsv default value */
        m = cJSON_GetObjectItem(e, "value");
        if( cJSON_IsString(m) && m->valuestring != NULL )
        {
            //printf("value:%s\n",m->valuestring);
            if( dsv.type == DSV_TYPE_STR )
            {
                dsv.value.pData = strdup( m->valuestring );
                dsv.len = strlen( m->valuestring ) + 1;
            }
            else
            {
                /* When using string for numeric bvalue in Json file */
                DSV_Str2Value( m->valuestring, &dsv );
            }
        }
        else if( cJSON_IsNumber(m) || cJSON_IsBool(m) )
        {
            //printf("value:%f\n",m->valuedouble);
            if( dsv.type != DSV_TYPE_STR )
            {
                dsv_Double2Value( m->valuedouble, &dsv );
            }
            else
            {
                syslog( LOG_ERR,
                        "The dsv type doesn't match the value: %s",
                        dsv.pName  );
                continue;
            }
        }

        rc = DSV_Create( ctx, instID, &dsv );
        if( rc != 0 )
        {
            /* continue even fail one dsv */
            syslog( LOG_ERR, "Failed to create dsv: %s", dsv.pName );
        }

        if( dsv.pName )
        {
            free( dsv.pName );
        }
        if( dsv.pDesc )
        {
            free( dsv.pDesc );
        }
        if( dsv.pTags )
        {
            free( dsv.pTags );
        }
        if( dsv.type == DSV_TYPE_STR && dsv.value.pData != NULL )
        {
            free( dsv.value.pData );
        }
    }
    return rc;
}

/*!=============================================================================

    Get the type enum from a type string, eg, uint16, int16, float, ...

@param[in]
    type_str
        name string of the type

@return
    enum value of dsv_info_type_t - success
    -1 - failed

==============================================================================*/
dsv_type_t DSV_GetTypeFromStr( const char *type_str )
{
    assert( type_str );

    if( strcmp( type_str, "string") == 0 )
    {
        return DSV_TYPE_STR;
    }
    if( strcmp( type_str, "uint16") == 0 )
    {
        return DSV_TYPE_UINT16;
    }
    if( strcmp( type_str, "sint16") == 0 )
    {
        return DSV_TYPE_SINT16;
    }
    if( strcmp( type_str, "uint32") == 0 )
    {
        return DSV_TYPE_UINT32;
    }
    if( strcmp( type_str, "sint32") == 0 )
    {
        return DSV_TYPE_SINT32;
    }
    if( strcmp( type_str, "float") == 0 )
    {
        return DSV_TYPE_FLOAT;
    }
    if( strcmp( type_str, "uint64") == 0 )
    {
        return DSV_TYPE_UINT64;
    }
    if( strcmp( type_str, "sint64") == 0 )
    {
        return DSV_TYPE_SINT64;
    }
    if( strcmp( type_str, "double") == 0 )
    {
        return DSV_TYPE_DOUBLE;
    }
    if( strcmp( type_str, "uint8") == 0 )
    {
        return DSV_TYPE_UINT8;
    }
    if( strcmp( type_str, "sint8") == 0 )
    {
        return DSV_TYPE_SINT8;
    }
    syslog( LOG_ERR, "Unsupported type of dsv" );
    return DSV_TYPE_INVALID;
}

/*!=============================================================================

    Get the number of bytes from type enum. Currently we don't need this length.
    In the future we need this to support blob type

@param[in]
    type
        dsv type

@return
    number of bytes - success
    -1 - failed

==============================================================================*/
int DSV_GetSizeFromType( int type )
{
    assert( type < DSV_TYPE_MAX );
    int rc = -1;

    switch( type )
    {
    case DSV_TYPE_STR:
        /* For string type, update the length later */
        rc = 1;
        break;
    case DSV_TYPE_UINT16:
    case DSV_TYPE_SINT16:
        rc = sizeof(uint16_t);
        break;
    case DSV_TYPE_UINT32:
    case DSV_TYPE_SINT32:
        rc = sizeof(uint32_t);
        break;
    case DSV_TYPE_FLOAT:
        rc = sizeof(float);
        break;
    case DSV_TYPE_UINT64:
    case DSV_TYPE_SINT64:
        rc = sizeof(int64_t);
        break;
    case DSV_TYPE_DOUBLE:
        rc = sizeof(double);
        break;
    case DSV_TYPE_UINT8:
    case DSV_TYPE_SINT8:
        rc = sizeof(int8_t);
        break;
    default:
        syslog( LOG_ERR, "Unsupported type" );
        break;
    }

    return rc;
}

/*!=============================================================================

    Discover dsv server using zbeacon

@param[out]
    server_ip
        buffer to output server ip as string. NULL will ignore output.

@param[in]
    size
        size of buffer

@return
    0 - No dsv server found,
    1 - found a dsv server running
==============================================================================*/
int DSV_DiscoverServer( char *server_ip, size_t size )
{
    int rc = 0;
    zactor_t *listener = zactor_new (zbeacon, NULL);
    assert (listener);
//  zstr_sendx (listener, "VERBOSE", NULL);
    zsock_send (listener, "si", "CONFIGURE", 9999);
    char *hostname = zstr_recv (listener);
    assert (*hostname);
    freen (hostname);

    //  We will listen to anything (empty subscription)
    zsock_send (listener, "sb", "SUBSCRIBE", "", 0);

    //  Wait for at most 1/2 second if there's no broadcasting
    zsock_set_rcvtimeo (listener, 500);
    char *ipaddress = zstr_recv (listener);
    if (ipaddress) {
        zframe_t *content = zframe_recv (listener);
        if( zframe_size (content) == 2 &&
            zframe_data (content) [0] == 0xCA &&
            zframe_data (content) [1] == 0xFE )
        {
            printf( "Found a DSV server, ip=%s\n", ipaddress );
            if( server_ip != NULL )
            {
                strncpy( server_ip, ipaddress, size );
            }
            rc = 1;
        }

        zframe_destroy (&content);
        zstr_free (&ipaddress);
    }
    zactor_destroy (&listener);
    return rc;
}

/*!=============================================================================

    As a client, it creates zmq client and subscriber socket and connect them
    to the dsv server endpoint

@return
    dsv ctx pointer
    NULL for fail
==============================================================================*/
void * DSV_Open( void )
{
    dsv_context_t *ctx = NULL;
    int rc;
    char server_ip[64];
    char reply_url[DSV_STRING_SIZE_MAX];
    char frontend_url[DSV_STRING_SIZE_MAX];
    char backend_url[DSV_STRING_SIZE_MAX];

    if( !DSV_DiscoverServer( server_ip, 64 ) )
    {
        syslog( LOG_ERR, "Error: No DSV server found!" );
        return NULL;
    }

    snprintf( frontend_url, DSV_STRING_SIZE_MAX, "tcp://%s:56789", server_ip );
    snprintf( backend_url, DSV_STRING_SIZE_MAX, "tcp://%s:56788", server_ip );
    snprintf( reply_url, DSV_STRING_SIZE_MAX, "tcp://%s:56787", server_ip );

    /* allocate memory for the System Variable Resource Manager interface
       structure */
    ctx = (dsv_context_t *)calloc( sizeof(dsv_context_t), 1 );
    if( ctx == NULL )
    {
        syslog( LOG_ERR, "Unable to alloc memory for dsv_context_t" );
        goto error;
    }

    ctx->zmq_ctx = zmq_ctx_new();
    if( ctx == NULL )
    {
        syslog( LOG_ERR, "Failed to call zmq_ctx_new: %s", strerror(errno) );
        goto error;
    }

    /* Request socket */
    ctx->sock_request = zmq_socket( ctx->zmq_ctx, ZMQ_REQ );
    if( ctx->sock_request == NULL )
    {
        syslog( LOG_ERR, "Failed to call zmq_socket: %s", strerror(errno) );
        goto error;
    }
    rc = zmq_connect( ctx->sock_request, reply_url );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to call zmq_connect: %s", strerror(errno) );
        goto error;
    }

    /* publish socket */
    ctx->sock_publish = zmq_socket( ctx->zmq_ctx, ZMQ_PUB );
    if( ctx->sock_publish == NULL )
    {
        syslog( LOG_ERR, "Failed to call zmq_socket: %s", strerror(errno) );
        goto error;
    }
    rc = zmq_connect( ctx->sock_publish, frontend_url );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to call zmq_connect: %s", strerror(errno) );
        goto error;
    }
    /*now just wait for publish connected to endpoint */
    usleep(5000);

    /* subscribe socket */
    ctx->sock_subscribe = zmq_socket( ctx->zmq_ctx, ZMQ_SUB );
    if( ctx->sock_subscribe == NULL )
    {
        syslog( LOG_ERR, "Failed to call zmq_socket: %s", strerror(errno) );
        goto error;
    }
    rc = zmq_connect( ctx->sock_subscribe, backend_url );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to call zmq_connect: %s", strerror(errno) );
        goto error;
    }
    return (void *)ctx;

error:
    DSV_Close( ctx );

    return NULL;
}

/*!=============================================================================

    Release all the resource of the client

@param[in]
    ctx
        dsv ctx holding all the resource pointer

==============================================================================*/
void DSV_Close( void * ctx )
{
    assert( ctx );

    dsv_context_t *dsv_ctx = (dsv_context_t *)ctx;

    if( dsv_ctx->sock_subscribe != NULL )
    {
        zmq_close( dsv_ctx->sock_subscribe );
    }

    if( dsv_ctx->sock_publish != NULL )
    {
        zmq_close( dsv_ctx->sock_publish );
    }

    if( dsv_ctx->sock_request != NULL )
    {
        zmq_close( dsv_ctx->sock_request );
    }

    if( dsv_ctx->zmq_ctx != NULL )
    {
        zmq_ctx_destroy( dsv_ctx->zmq_ctx );
    }

    if( dsv_ctx != NULL )
    {
        free( dsv_ctx );
    }
}

/*!=============================================================================

    This function request dsv server to create one or multiple dsv with
    information provided.

@param[in]
    ctx
        dsv ctx ( returned by DSV_Open() )

@param[in]
    instID
        dsv instance ID passed by the creator, normally device instance ID

@param[in]
    pInfo
        pointer to the dsv information JSON string

@return
    0 - success
    any other value specifies an error code (see errno.h)

==============================================================================*/
int DSV_CreateWithJson( void *ctx, uint32_t instID, char *file )
{
    assert( ctx );
    assert( file );

    int rc;
    char *json_buf = (char *)malloc( DSV_JSON_FILE_SIZE_MAX );
    if( json_buf == NULL )
    {
        syslog( LOG_ERR, "Unable to allocate memory\n" );
        return EINVAL;
    }

    rc = dsv_ReadJsonFile( file, json_buf, DSV_JSON_FILE_SIZE_MAX );
    if( rc == 0 )
    {
        rc = dsv_ParseJsonStr( ctx, instID, json_buf );
    }

    free( json_buf );
    return rc;
}

/*!=============================================================================

    This function requests dsv server to create one dsv with dsv_info_t
    structure.

    The message buffer structure:
    +===============================+
    |        dsv_msg_create         |
    +===============================+
    | common(type, length, instID)  |
    +-------------------------------+
    | dsv_info_t                    |
    +-------------------------------+
    | data: name, desc, tags, value |
    +-------------------------------+

@param[in]
    ctx
        dsv ctx ( returned by DSV_Open() )

@param[in]
    instID
        dsv instance ID passed by the creator, normally device instance ID

@param[in]
    pDsv
        pointer to the dsv information of dsv_info_t structure

@return
    0 - success
    any other value specifies an error code (see errno.h)

==============================================================================*/
int DSV_Create( void *ctx, uint32_t instID, dsv_info_t *pDsv )
{
    assert( ctx );
    assert( pDsv );

    int rc = EINVAL;
    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;
    int len = 0;

    req->type = DSV_MSG_CREATE;
    req->instID = instID;
    req->length = sizeof(dsv_msg_request_t);

    memcpy( req_data, pDsv, sizeof(dsv_info_t));
    len = sizeof(dsv_info_t);
    req->length += len;
    req_data += len;

    strncpy( req_data, pDsv->pName ? pDsv->pName : "", DSV_STRING_SIZE_MAX );
    len = strlen(req_data) + 1;
    req->length += len;
    req_data += len;

    strncpy( req_data, pDsv->pDesc ? pDsv->pDesc : "", DSV_STRING_SIZE_MAX );
    len = strlen(req_data) + 1;
    req->length += len;
    req_data += len;

    strncpy( req_data, pDsv->pTags ? pDsv->pTags : "", DSV_STRING_SIZE_MAX );
    len = strlen(req_data) + 1;
    req->length += len;
    req_data += len;

    if( pDsv->type == DSV_TYPE_STR )
    {
        if( pDsv->value.pData == NULL || pDsv->len == 0 )
        {
            syslog( LOG_ERR, "Wrong dsv value!" );
        }
        else
        {
            strcpy( req_data, pDsv->value.pData );
            req->length += strlen(req_data) + 1;
        }
    }

    rc = dsv_SendMsg( ctx, req_buf, req->length, NULL, 0 );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server: %s", pDsv->pName );
        return EFAULT;
    }

    return rc;
}

/*!=============================================================================

    Query the dsv handle from dsv server database by dsv name and instID.
    The message buffer structure:
    +==============================+
    |    dsv_msg_get_handle        |
    +==============================+
    | common(type, length, instID) |
    +------------------------------+
    | data: name                   |
    +------------------------------+

@param[in]
    ctx
        dsv ctx ( returned by DSV_Open() )

@param[in]
    instID
        dsv instance ID passed by the creator, normally device instance ID

@param[in]
    name
        pointer to a NULL terminated dsv namne string

@return
    a handle to the dsv in the dsv server,
    NULL if not found.

==============================================================================*/
void *DSV_Handle( void *ctx, uint32_t instID, const char *name )
{
    assert( ctx );
    assert( name );

    int rc = EINVAL;
    void *handle = NULL;

    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    char rep_buf[BUFSIZE];
    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    char *rep_data = rep->data;

    req->type = DSV_MSG_GET_HANDLE;
    req->instID = instID;
    req->length = sizeof(dsv_msg_request_t);

    strncpy( req_data, name, DSV_STRING_SIZE_MAX );
    req->length += strlen(name) + 1;

    rc = dsv_SendMsg( ctx, req_buf, req->length, rep_buf, sizeof(rep_buf) );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server" );
        return NULL;
    }
    handle = *(void **)rep_data;
    return handle;
}

/*!=============================================================================

    Query the dsv type from dsv server by handle.
    The message buffer structure:
    +==============================+
    |         dsv_msg_type         |
    +==============================+
    | common(type, length, instID) |
    +------------------------------+
    | data: handle                 |
    +------------------------------+

@param[in]
    ctx
        dsv ctx ( returned by DSV_Open() )

@param[in]
    hndl
        dsv handle in server process
@return
    0 or positive number: type
    -1 if not found.

==============================================================================*/
int DSV_Type( void *ctx, void *hndl )
{
    assert( ctx );
    assert( hndl );

    int rc = EINVAL;
    int type = 0;

    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    char rep_buf[BUFSIZE];
    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    char *rep_data = rep->data;

    req->type = DSV_MSG_GET_TYPE;
    req->instID = -1; // no use
    req->length = sizeof(dsv_msg_request_t);

    *(void **)req_data = hndl;
    req->length += sizeof(hndl);

    rc = dsv_SendMsg( ctx, req_buf, req->length, rep_buf, sizeof(rep_buf) );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server" );
        return EFAULT;
    }
    type = *(int *)rep_data;
    return type;
}
/*!=============================================================================

    This function request dsv server to update dsv with value.
    This is a helper function that should not be used in time critical case.

    This function first query the handle from the server then call
    DSV_SetThruStr function to set the value in string form

@param[in]
    ctx
        dsv ctx ( returned by DSV_Open() )

@param[in]
    instID
        dsv instance ID passed by the creator, normally device instance ID

@param[in]
    name
        dsv name string terminated by NULL byte

@param[in]
    value
        dsv value as string

@return
    0 - success
    any other value specifies an error code (see errno.h)

==============================================================================*/
int DSV_SetByName( void *ctx, uint32_t instID, const char *name, char *value )
{
    assert( ctx );
    assert( name );
    assert( value );

    int rc = EINVAL;
    void *hndl = DSV_Handle( ctx, instID, name );
    if( hndl == NULL )
    {
        syslog( LOG_ERR, "Unable to find dsv: %s", name );
        return rc;
    }

    rc = DSV_SetThruStr( ctx, hndl, value );
    return rc;
}

/*!=============================================================================

    This function requests dsv server to update dsv with value in string form.

    This function first query the type from the server then call the
    corresponsing DSV_Set function to set the value

@param[in]
    ctx
        dsv ctx ( returned by DSV_Open() )

@param[in]
    hndl
        dsv handle in server process

@param[in]
    value
        dsv value in string form

@return
    0 - success
    any other value specifies an error code (see errno.h)

==============================================================================*/
int DSV_SetThruStr( void *ctx, void *hndl, char *value )
{
    assert(ctx);
    assert(hndl);
    assert(value);

    int rc = EINVAL;
    int type = DSV_Type( ctx, hndl );
    if( type < 0 )
    {
        syslog( LOG_ERR, "Unable to get the dsv type" );
        return rc;
    }

    switch( type )
    {
    case DSV_TYPE_STR:
        rc = DSV_Set( ctx, hndl, value );
        break;

    case DSV_TYPE_UINT16:
        rc = DSV_Set( ctx, hndl, (uint16_t)strtoul(value, NULL, 10) );
        break;

    case DSV_TYPE_UINT32:
        rc = DSV_Set( ctx, hndl, (uint32_t)strtoul(value, NULL, 10) );
        break;

    case DSV_TYPE_UINT64:
        rc = DSV_Set( ctx, hndl, (uint64_t)strtoull(value, NULL, 10) );
        break;

    case DSV_TYPE_UINT8:
        rc = DSV_Set( ctx, hndl, (uint8_t)strtoul(value, NULL, 10) );
        break;

    case DSV_TYPE_SINT16:
        rc = DSV_Set( ctx, hndl, (int16_t)strtol(value, NULL, 10) );
        break;

    case DSV_TYPE_SINT32:
        rc = DSV_Set( ctx, hndl, (int32_t)strtol(value, NULL, 10) );
        break;

    case DSV_TYPE_SINT64:
        rc = DSV_Set( ctx, hndl, (int64_t)strtoll(value, NULL, 10) );
        break;

    case DSV_TYPE_SINT8:
        rc = DSV_Set( ctx, hndl, (int8_t)strtol(value, NULL, 10) );
        break;

    case DSV_TYPE_FLOAT:
        rc = DSV_Set( ctx, hndl, strtof(value, NULL) );
        break;

    case DSV_TYPE_DOUBLE:
        rc = DSV_Set( ctx, hndl, strtod(value, NULL) );
        break;

    default:
        syslog( LOG_ERR, "Unsupported type for dsv" );
        break;
    }

    return rc;
}

/**
* Set value for string type of dsv
    message buffer data structure:
    +==============================+
    |         dsv_msg_set          |
    +==============================+
    | common(type, length, instID) |
    +------------------------------+
    | data:handle, value, ...      |
    +------------------------------+
*/
int DSV_Set( void *ctx, void *hndl, char *value )
{
    assert(ctx);
    assert(hndl);
    assert(value);

    int rc = EINVAL;
    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    req->type = DSV_MSG_SET;
    req->instID = -1; // no use
    req->length = sizeof(dsv_msg_request_t);

    memcpy( req_data, &hndl, sizeof(hndl) );
    req->length += sizeof(hndl);
    req_data += sizeof(hndl);

    strncpy( req_data, value, DSV_STRING_SIZE_MAX );
    req->length += strlen(req_data) + 1;

    rc = dsv_SendMsg( ctx, req_buf, req->length, NULL, 0 );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server" );
        return EFAULT;
    }

    return rc;
}

/**
* Set value for string type of dsv
    message buffer data structure:
    +==============================+
    |         dsv_msg_set          |
    +==============================+
    | common(type, length, instID) |
    +------------------------------+
    | data:handle, value, ...      |
    +------------------------------+
*/
template <typename T>
int DSV_Set( void *ctx, void *hndl, T value )
{
    assert(ctx);
    assert(hndl);

    int rc = EINVAL;
    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    req->type = DSV_MSG_SET;
    req->instID = -1; // no use
    req->length = sizeof(dsv_msg_request_t);

    memcpy( req_data, &hndl, sizeof(hndl) );
    req->length += sizeof(hndl);
    req_data += sizeof(hndl);

    memcpy( req_data, &value, sizeof(value) );
    req->length += sizeof(value);

    rc = dsv_SendMsg( ctx, req_buf, req->length, NULL, 0 );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server" );
        return EFAULT;
    }

    return rc;
}


/*!=============================================================================

    This function request dsv server to query dsv with value.
    This is a helper function that should not be used in time critical case.

    This function first query the handle from the server then call
    DSV_GetThruStr function to set the value in string form

@param[in]
    ctx
        dsv ctx ( returned by DSV_Open() )

@param[in]
    instID
        dsv instance ID passed by the creator, normally device instance ID

@param[in]
    name
        dsv name string terminated by NULL byte

@param[in]
    value
        dsv value as string

@param[in]
    size
        size of value buffer

@return
    0 - success
    any other value specifies an error code (see errno.h)

==============================================================================*/
int DSV_GetByName( void *ctx,
                   uint32_t instID,
                   const char *name,
                   char *value,
                   size_t size )
{

    assert( ctx );
    assert( name );
    assert( value );

    int rc = EINVAL;
    void *hndl = DSV_Handle( ctx, instID, name );
    if( hndl == NULL )
    {
        syslog( LOG_ERR, "Unable to find dsv: %s", name );
        return rc;
    }

    rc = DSV_GetThruStr( ctx, hndl, value, size );
    return rc;
}

/*!=============================================================================

    This function requests dsv server to query dsv with value in string form.

    This function first query the type from the server then call the
    corresponsing DSV_Get function to set the value

@param[in]
    ctx
        dsv ctx ( returned by DSV_Open() )

@param[in]
    hndl
        dsv handle in server process

@param[in]
    value
        dsv value buffer

 @param[in]
    size
        size of buffer

@return
    0 - success
    any other value specifies an error code (see errno.h)

==============================================================================*/
int DSV_GetThruStr( void *ctx, void *hndl, char *value, size_t size )
{
    assert(ctx);
    assert(hndl);
    assert(value);

    int rc = EINVAL;
    int type = DSV_Type( ctx, hndl );
    if( type < 0 )
    {
        syslog( LOG_ERR, "Unable to get the dsv type" );
        return rc;
    }

    switch( type )
    {
    case DSV_TYPE_STR:
        rc = DSV_Get( ctx, hndl, value, size );
        break;

    case DSV_TYPE_UINT16:
        uint16_t u16;
        rc = DSV_Get( ctx, hndl, &u16 );
        snprintf(value, size, "%" PRIu16, u16);
        break;
    case DSV_TYPE_UINT32:
        uint32_t u32;
        rc = DSV_Get( ctx, hndl, &u32 );
        snprintf(value, size, "%" PRIu32, u32);
        break;
    case DSV_TYPE_UINT64:
        uint64_t u64;
        rc = DSV_Get( ctx, hndl, &u64 );
        snprintf(value, size, "%" PRIu64, u64);
        break;
    case DSV_TYPE_UINT8:
        uint8_t u8;
        rc = DSV_Get( ctx, hndl, &u8 );
        snprintf(value, size, "%" PRIu8, u8);
        break;

    case DSV_TYPE_SINT16:
        int16_t s16;
        rc = DSV_Get( ctx, hndl, &s16 );
        snprintf(value, size, "%" PRId16, s16);
        break;
    case DSV_TYPE_SINT32:
        int32_t s32;
        rc = DSV_Get( ctx, hndl, &s32 );
        snprintf(value, size, "%" PRId32, s32);
        break;
    case DSV_TYPE_SINT64:
        int64_t s64;
        rc = DSV_Get( ctx, hndl, &s64 );
        snprintf(value, size, "%" PRId64, s64);
        break;
    case DSV_TYPE_SINT8:
        int8_t s8;
        rc = DSV_Get( ctx, hndl, &s8 );
        snprintf(value, size, "%" PRId8, s8);
        break;
    case DSV_TYPE_FLOAT:
        float f32;
        rc = DSV_Get( ctx, hndl, &f32 );
        snprintf(value, size, "%f", f32);
        break;
    case DSV_TYPE_DOUBLE:
        double f64;
        rc = DSV_Get( ctx, hndl, &f64 );
        snprintf(value, size, "%f", f64);
        break;

    default:
        syslog( LOG_ERR, "Unsupported type for dsv" );
        break;
    }

    return rc;
}

/**
* Get value for string type of dsv
    message buffer data structure:
    +==============================+
    |         dsv_msg_get          |
    +==============================+
    | common(type, length, instID) |
    +------------------------------+
    | data:handle, ...      |
    +------------------------------+
*/
int DSV_Get( void *ctx, void *hndl, char *value, size_t size )
{
    assert(ctx);
    assert(hndl);
    assert(value);

    int rc = EINVAL;
    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    char rep_buf[BUFSIZE];
    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    char *rep_data = rep->data;

    req->type = DSV_MSG_GET;
    req->instID = -1; // no use
    req->length = sizeof(dsv_msg_request_t);

    memcpy( req_data, &hndl, sizeof(hndl) );
    req->length += sizeof(hndl);

    rc = dsv_SendMsg( ctx, req_buf, req->length, rep_buf, sizeof(rep_buf) );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server" );
        return EFAULT;
    }
    strncpy( value, rep_data, size );
    return rc;
}

/**
* Get value for numeric type of dsv
    message buffer data structure:
    +==============================+
    |         dsv_msg_get          |
    +==============================+
    | common(type, length, instID) |
    +------------------------------+
    | data:handle, ...      |
    +------------------------------+
*/
template <typename T>
int DSV_Get( void *ctx, void *hndl, T *value )
{
    assert(ctx);
    assert(hndl);
    assert(value);

    int rc = EINVAL;
    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    char rep_buf[BUFSIZE];
    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    char *rep_data = rep->data;

    req->type = DSV_MSG_GET;
    req->instID = -1; // no use
    req->length = sizeof(dsv_msg_request_t);

    memcpy( req_data, &hndl, sizeof(hndl) );
    req->length += sizeof(hndl);

    rc = dsv_SendMsg( ctx, req_buf, req->length, rep_buf, sizeof(rep_buf) );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server" );
        return EFAULT;
    }
    memcpy(value, rep_data, sizeof(dsv_value_t));
    return rc;
}


/**
* return zero if successful. Otherwise it shall return -1
*/
int DSV_SubByName( void *ctx, uint32_t instID, const char *name )
{
    assert(ctx);
    assert(name);

    int rc;
    dsv_context_t *dsv_ctx = (dsv_context_t *)ctx;
    char full_name[DSV_STRING_SIZE_MAX];
    snprintf(full_name, DSV_STRING_SIZE_MAX, "[%d]%s", instID, name);

    rc = zmq_setsockopt( dsv_ctx->sock_subscribe,
                         ZMQ_SUBSCRIBE,
                         full_name,
                         strlen(full_name) + 1 );
    return rc;
}

/**
*
*/
int DSV_GetNotification( void *ctx,
                         char *name,
                         size_t nlen,
                         char *value,
                         size_t vlen )
{
    assert(ctx);
    assert(name);
    assert(value);

    char sub_buf[BUFSIZE];
    int rc = dsv_RecvMsg(ctx, sub_buf, sizeof(sub_buf));
    if( rc == 0 )
    {
        strncpy( name, sub_buf, nlen );
        memcpy( value, sub_buf + strlen(sub_buf) + 1, vlen);
    }

    return rc;
}

/* end of libsysvars group */

