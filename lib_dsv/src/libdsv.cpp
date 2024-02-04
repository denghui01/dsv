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

    Send a message to the server, and get the feedback from it on demand

@param[in]
    ctx
        dsv ctx ( returned by DSV_Open() )
@param[in]
    req_buf
        request message buffer
@param[in]
    req_len
        request message buffer length
@param[in]
    rep_buf
        reply message buffer if it is not NULL
@param[in]
    rep_len
        length of the reply message buffer
@return
    0 - success
    any other value specifies an error code (see errno.h)

==============================================================================*/
static int dsv_SendMsg( void *ctx,
                        const void *req_buf,
                        size_t req_len,
                        void *rep_buf,
                        size_t rep_len )
{
    assert( ctx );
    assert( req_buf );

    int rc;
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    dsv_context_t *dsv_ctx = (dsv_context_t *)ctx;

    if( req->type == DSV_MSG_CREATE ||
        req->type == DSV_MSG_SET ||
        req->type == DSV_MSG_INS_ITEM ||
        req->type == DSV_MSG_DEL_ITEM ||
        req->type == DSV_MSG_ADD_ITEM ||
        req->type == DSV_MSG_SET_ITEM )
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
             req->type == DSV_MSG_GET_LEN ||
             req->type == DSV_MSG_GET ||
             req->type == DSV_MSG_GET_NEXT ||
             req->type == DSV_MSG_GET_ITEM)
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

/*!=============================================================================

    Recv a message from the server, used in querying the notifications

@param[in]
    ctx
        dsv ctx ( returned by DSV_Open() )
@param[in]
    sub_buf
        subscribe message buffer
@param[in]
    sub_len
        subscribe message buffer length
@return
    0 - success
    any other value specifies an error code (see errno.h)

==============================================================================*/
static int dsv_RecvMsg( void *ctx,
                        void *sub_buf,
                        size_t sub_len )
{
    assert( ctx );
    assert( sub_buf );

    int rc;
    dsv_context_t *dsv_ctx = (dsv_context_t *)ctx;
    rc = zmq_recv( dsv_ctx->sock_subscribe, sub_buf, sub_len, 0 );
    if( rc == -1 )
    {
        syslog( LOG_ERR, "zmq_recv failed: %s", zmq_strerror( errno ) );
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
        dsv instance ID passed by the creator, normally device GUID
@param[in]
    buf
        pointer to the JSON string
@return
    0 - success
    others - failed

==============================================================================*/
static int dsv_ParseJsonStr( void *ctx, uint32_t instID, const char *buf )
{
    assert( ctx );
    assert( buf );

    dsv_info_t dsv = { 0 };
    int rc = EINVAL;
    int array_size;
    int i;
    cJSON *e;
    cJSON *m;
    cJSON *root = cJSON_Parse( buf );
    array_size = cJSON_GetArraySize( root );
    dsv.instID = instID;

    for( i = 0; i < array_size; i++ )
    {
        e = cJSON_GetArrayItem( root, i );

        /* handle dsv name */
        m = cJSON_GetObjectItem( e, "name" );
        if( cJSON_IsString( m ) && m->valuestring != NULL )
        {
            //printf("name:%s\n",m->valuestring);
            dsv.pName = (char *)malloc( DSV_STRING_SIZE_MAX );
            snprintf( dsv.pName, DSV_STRING_SIZE_MAX,
                      "[%d]%s", instID, m->valuestring );
        }

        /* handle dsv description */
        m = cJSON_GetObjectItem( e, "description" );
        if( cJSON_IsString( m ) && m->valuestring != NULL )
        {
            //printf("desc:%s\n",m->valuestring);
            dsv.pDesc = strdup( m->valuestring );
        }

        /* handle dsv tags */
        m = cJSON_GetObjectItem( e, "tags" );
        if( cJSON_IsString( m ) && m->valuestring != NULL )
        {
            //printf("tags:%s\n",m->valuestring);
            dsv.pTags = strdup( m->valuestring );
        }

        /* handle dsv type first */
        m = cJSON_GetObjectItem( e, "type" );
        if( cJSON_IsString( m ) && m->valuestring != NULL )
        {
            //printf("type:%s\n",m->valuestring);
            dsv.type = DSV_GetTypeFromStr( m->valuestring );
            dsv.len = DSV_GetSizeFromType( dsv.type );
        }

        /* handle dsv default value */
        m = cJSON_GetObjectItem( e, "value" );
        if( cJSON_IsString( m ) && m->valuestring != NULL )
        {
            DSV_Str2Value( m->valuestring, &dsv );
        }
        else if( cJSON_IsNumber( m ) || cJSON_IsBool( m ) )
        {
            if( DSV_Double2Value( m->valuedouble, &dsv ) != 0 )
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
        if( dsv.type == DSV_TYPE_STR && dsv.value.pStr != NULL )
        {
            free( dsv.value.pStr );
        }
        if( dsv.type == DSV_TYPE_INT_ARRAY && dsv.value.pArray != NULL )
        {
            free( dsv.value.pArray );
        }

    }
    return rc;
}

/*!=============================================================================

    This function fills the request buffer with hndl
    The request buffer is a data structure:
    +-----------------------+
    | type: DSV_MSG_SET     |
    +-----------------------+
    | length                |
    +-----------------------+
    | handle (dsv_info_t *) |
    +-----------------------+

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
static int fill_req_buf(char *req_buf, int type, const void *hndl)
{
    assert(req_buf);
    assert(hndl);

    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    req->type = type;
    req->length = sizeof(dsv_msg_request_t);

    memcpy( req_data, &hndl, sizeof(hndl) );
    req->length += sizeof(hndl);

    return sizeof(hndl);
}
/*!=============================================================================

    As a client, it creates zmq client and subscriber socket and connect them
    to the dsv server endpoint

@return
    dsv ctx pointer
    NULL for fail
==============================================================================*/
void* DSV_Open( void )
{
    dsv_context_t *ctx = NULL;
    int rc;
    char server_ip[64] = {0};
    char reply_url[DSV_STRING_SIZE_MAX];
    char frontend_url[DSV_STRING_SIZE_MAX];
    char backend_url[DSV_STRING_SIZE_MAX];

    if( !DSV_DiscoverServer( server_ip, 64 ) )
    {
        syslog( LOG_ERR, "Error: No DSV server found!" );
        return NULL;
    }

    assert(server_ip[0]);
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
        syslog( LOG_ERR, "Failed to call zmq_ctx_new: %s", strerror( errno ) );
        goto error;
    }

    /* Request socket */
    ctx->sock_request = zmq_socket( ctx->zmq_ctx, ZMQ_REQ );
    if( ctx->sock_request == NULL )
    {
        syslog( LOG_ERR, "Failed to call zmq_socket: %s", strerror( errno ) );
        goto error;
    }
    rc = zmq_connect( ctx->sock_request, reply_url );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to call zmq_connect: %s", strerror( errno ) );
        goto error;
    }

    /* publish socket */
    ctx->sock_publish = zmq_socket( ctx->zmq_ctx, ZMQ_PUB );
    if( ctx->sock_publish == NULL )
    {
        syslog( LOG_ERR, "Failed to call zmq_socket: %s", strerror( errno ) );
        goto error;
    }
    rc = zmq_connect( ctx->sock_publish, frontend_url );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to call zmq_connect: %s", strerror( errno ) );
        goto error;
    }
    /*now just wait 100ms for publisher connected to endpoint */
    usleep( 100000 );

    /* subscribe socket */
    ctx->sock_subscribe = zmq_socket( ctx->zmq_ctx, ZMQ_SUB );
    if( ctx->sock_subscribe == NULL )
    {
        syslog( LOG_ERR, "Failed to call zmq_socket: %s", strerror( errno ) );
        goto error;
    }
    rc = zmq_connect( ctx->sock_subscribe, backend_url );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to call zmq_connect: %s", strerror( errno ) );
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
void DSV_Close( void *ctx )
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
        dsv instance ID passed by the creator, normally device short GUID
@param[in]
    file
        JSON file name
@return
    0 - success
    any other value specifies an error code (see errno.h)

==============================================================================*/
int DSV_CreateWithJson( void *ctx, uint32_t instID, const char *file )
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
    req->length = sizeof(dsv_msg_request_t);

    memcpy( req_data, pDsv, sizeof(dsv_info_t) );
    len = sizeof(dsv_info_t);
    req->length += len;
    req_data += len;

    strncpy( req_data, pDsv->pName ? pDsv->pName : "", DSV_STRING_SIZE_MAX );
    len = strlen( req_data ) + 1;
    req->length += len;
    req_data += len;

    strncpy( req_data, pDsv->pDesc ? pDsv->pDesc : "", DSV_STRING_SIZE_MAX );
    len = strlen( req_data ) + 1;
    req->length += len;
    req_data += len;

    strncpy( req_data, pDsv->pTags ? pDsv->pTags : "", DSV_STRING_SIZE_MAX );
    len = strlen( req_data ) + 1;
    req->length += len;
    req_data += len;

    if( pDsv->type == DSV_TYPE_STR || pDsv->type == DSV_TYPE_INT_ARRAY )
    {
        if( pDsv->value.pStr != NULL && pDsv->len != 0 )
        {
            memcpy( req_data, pDsv->value.pStr, pDsv->len );
            req->length += pDsv->len;
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

    Query the dsv handle from dsv server database by dsv name

@param[in]
    ctx
        dsv ctx ( returned by DSV_Open() )
@param[in]
    name
        pointer to a NULL terminated dsv namne string
@return
    a handle to the dsv in the dsv server,
    NULL if not found.

==============================================================================*/
void* DSV_Handle( void *ctx, const char *name )
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
    req->length = sizeof(dsv_msg_request_t);

    strncpy( req_data, name, DSV_STRING_SIZE_MAX );
    req->length += strlen( name ) + 1;

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

    fill_req_buf(req_buf, DSV_MSG_GET_TYPE, hndl);

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

    Query the dsv len from dsv server by handle. (Used for array type of dsv)

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
size_t DSV_Len( void *ctx, void *hndl )
{
    assert( ctx );
    assert( hndl );

    int rc = EINVAL;
    size_t len = 0;

    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    char rep_buf[BUFSIZE];
    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    char *rep_data = rep->data;

    fill_req_buf(req_buf, DSV_MSG_GET_LEN, hndl);

    rc = dsv_SendMsg( ctx, req_buf, req->length, rep_buf, sizeof(rep_buf) );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server" );
        return EFAULT;
    }
    len = *(size_t *)rep_data;
    return len;
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
    name
        dsv name string terminated by NULL byte
@param[in]
    value
        dsv value as string
@return
    0 - success
    any other value specifies an error code (see errno.h)

==============================================================================*/
int DSV_SetByName( void *ctx, const char *name, char *value )
{
    assert( ctx );
    assert( name );
    assert( value );

    int rc = EINVAL;
    void *hndl = DSV_Handle( ctx, name );
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
int DSV_SetThruStr( void *ctx, void *hndl, const char *value )
{
    assert( ctx );
    assert( hndl );
    assert( value );

    int rc = EINVAL;
    int type = DSV_Type( ctx, hndl );
    if( type < 0 )
    {
        syslog( LOG_ERR, "Unable to get the dsv type" );
        return rc;
    }

    dsv_info_t dsv;
    size_t size;
    void *data;
    switch( type )
    {
    case DSV_TYPE_STR:
        rc = DSV_Set( ctx, hndl, value );
        break;
    case DSV_TYPE_INT_ARRAY:
        rc = DSV_Str2Array(value, &data, &size);
        if( rc == 0 )
        {
            rc = DSV_Set( ctx, hndl, data, size );
            free( data );
        }
        break;
    case DSV_TYPE_UINT16:
        rc = DSV_Set( ctx, hndl, (uint16_t)strtoul( value, NULL, 10 ) );
        break;

    case DSV_TYPE_UINT32:
        rc = DSV_Set( ctx, hndl, (uint32_t)strtoul( value, NULL, 10 ) );
        break;

    case DSV_TYPE_UINT64:
        rc = DSV_Set( ctx, hndl, (uint64_t)strtoull( value, NULL, 10 ) );
        break;

    case DSV_TYPE_UINT8:
        rc = DSV_Set( ctx, hndl, (uint8_t)strtoul( value, NULL, 10 ) );
        break;

    case DSV_TYPE_SINT16:
        rc = DSV_Set( ctx, hndl, (int16_t)strtol( value, NULL, 10 ) );
        break;

    case DSV_TYPE_SINT32:
        rc = DSV_Set( ctx, hndl, (int32_t)strtol( value, NULL, 10 ) );
        break;

    case DSV_TYPE_SINT64:
        rc = DSV_Set( ctx, hndl, (int64_t)strtoll( value, NULL, 10 ) );
        break;

    case DSV_TYPE_SINT8:
        rc = DSV_Set( ctx, hndl, (int8_t)strtol( value, NULL, 10 ) );
        break;

    case DSV_TYPE_FLOAT:
        rc = DSV_Set( ctx, hndl, strtof( value, NULL ) );
        break;

    case DSV_TYPE_DOUBLE:
        rc = DSV_Set( ctx, hndl, strtod( value, NULL ) );
        break;

    default:
        syslog( LOG_ERR, "Unsupported type for dsv" );
        break;
    }

    return rc;
}

/**
 * Set value for string type of dsv
 */
int DSV_Set( void *ctx, void *hndl, char *value )
{
    assert( ctx );
    assert( hndl );
    assert( value );

    int rc = EINVAL;
    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    rc = fill_req_buf(req_buf, DSV_MSG_SET, hndl);
    req_data += rc;
    strncpy( req_data, value, DSV_STRING_SIZE_MAX );
    req->length += strlen( req_data ) + 1;

    rc = dsv_SendMsg( ctx, req_buf, req->length, NULL, 0 );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server" );
        return EFAULT;
    }

    return rc;
}

/**
 * Set value for int array type of dsv
 */
int DSV_Set( void *ctx, void *hndl, void *data, size_t size )
{
    assert( ctx );
    assert( hndl );
    assert( data );

    int rc = EINVAL;
    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    rc = fill_req_buf(req_buf, DSV_MSG_SET, hndl);
    req_data += rc;
    memcpy( req_data, data, size );
    req->length += size;

    rc = dsv_SendMsg( ctx, req_buf, req->length, NULL, 0 );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server" );
        return EFAULT;
    }

    return rc;
}

/**
 * Set value for numeric type of dsv
 */
template< typename T >
int DSV_Set( void *ctx, void *hndl, T value )
{
    assert( ctx );
    assert( hndl );

    int rc = EINVAL;
    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    rc = fill_req_buf(req_buf, DSV_MSG_SET, hndl);
    req_data += rc;
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
    The instID and name must be exactly matched

    This function first query the handle from the server then call
    DSV_GetThruStr function to set the value in string form

@param[in]
    ctx
        dsv ctx ( returned by DSV_Open() )
@param[in]
    name
        dsv name string terminated by NULL byte
@param[out]
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
                   const char *name,
                   char *value,
                   size_t size )
{

    assert( ctx );
    assert( name );
    assert( value );

    int rc = EINVAL;
    void *hndl = DSV_Handle( ctx, name );
    if( hndl == NULL )
    {
        syslog( LOG_ERR, "Unable to find dsv: %s", name );
        return rc;
    }

    rc = DSV_GetThruStr( ctx, hndl, value, size );
    return rc;
}

/*!=============================================================================

    This function request dsv server to query dsv with value.
    This is a helper function that should not be used in time critical case.
    Partially matched name will also return its value

    This function first query the handle from the server then call
    DSV_GetThruStr function to set the value in string form

@param[in]
    ctx
        dsv ctx ( returned by DSV_Open() )

@param[in]
    name
        dsv name string terminated by NULL byte

@param[in]
    index
        index of last match

@param[out]
    value
        dsv value as string

@param[in]
    size
        size of value buffer

@return
    index of dsv matched this time - success
    -1 - the end of match

==============================================================================*/
int DSV_GetByNameFuzzy( void *ctx,
                        const char *search_name,
                        int last_index,
                        char *name,
                        size_t namesz,
                        char *value,
                        size_t valuesz )
{
    assert( ctx );
    assert( name );
    assert( value );

    int rc = EINVAL;
    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    char rep_buf[BUFSIZE];
    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    char *rep_data = rep->data;

    req->type = DSV_MSG_GET_NEXT;
    req->length = sizeof(dsv_msg_request_t);

    *(int *)req_data = last_index;
    req->length += sizeof(int);

    req_data += sizeof(int);
    strcpy( req_data, search_name );
    req->length += strlen( search_name ) + 1;

    rc = dsv_SendMsg( ctx, req_buf, req->length, rep_buf, sizeof(rep_buf) );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server" );
        rc = -1;
    }
    else
    {
        rc = *(int *)rep_data;
        rep_data += sizeof(int);
        strncpy( name, rep_data, namesz );
        rep_data += strlen( rep_data ) + 1;
        strncpy( value, rep_data, valuesz );
    }
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
    assert( ctx );
    assert( hndl );
    assert( value );

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
    case DSV_TYPE_INT_ARRAY:
        rc = DSV_Get( ctx, hndl, value, size );
        break;
    case DSV_TYPE_UINT16:
        uint16_t u16;
        rc = DSV_Get( ctx, hndl, &u16 );
        snprintf( value, size, "%" PRIu16, u16 );
        break;
    case DSV_TYPE_UINT32:
        uint32_t u32;
        rc = DSV_Get( ctx, hndl, &u32 );
        snprintf( value, size, "%" PRIu32, u32 );
        break;
    case DSV_TYPE_UINT64:
        uint64_t u64;
        rc = DSV_Get( ctx, hndl, &u64 );
        snprintf( value, size, "%" PRIu64, u64 );
        break;
    case DSV_TYPE_UINT8:
        uint8_t u8;
        rc = DSV_Get( ctx, hndl, &u8 );
        snprintf( value, size, "%" PRIu8, u8 );
        break;

    case DSV_TYPE_SINT16:
        int16_t s16;
        rc = DSV_Get( ctx, hndl, &s16 );
        snprintf( value, size, "%" PRId16, s16 );
        break;
    case DSV_TYPE_SINT32:
        int32_t s32;
        rc = DSV_Get( ctx, hndl, &s32 );
        snprintf( value, size, "%" PRId32, s32 );
        break;
    case DSV_TYPE_SINT64:
        int64_t s64;
        rc = DSV_Get( ctx, hndl, &s64 );
        snprintf( value, size, "%" PRId64, s64 );
        break;
    case DSV_TYPE_SINT8:
        int8_t s8;
        rc = DSV_Get( ctx, hndl, &s8 );
        snprintf( value, size, "%" PRId8, s8 );
        break;
    case DSV_TYPE_FLOAT:
        float f32;
        rc = DSV_Get( ctx, hndl, &f32 );
        snprintf( value, size, "%f", f32 );
        break;
    case DSV_TYPE_DOUBLE:
        double f64;
        rc = DSV_Get( ctx, hndl, &f64 );
        snprintf( value, size, "%f", f64 );
        break;

    default:
        syslog( LOG_ERR, "Unsupported type for dsv" );
        break;
    }

    return rc;
}

/**
 * Get value for string type of dsv
 */
int DSV_Get( void *ctx, void *hndl, char *value, size_t size )
{
    assert( ctx );
    assert( hndl );
    assert( value );

    int rc = EINVAL;
    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    char rep_buf[BUFSIZE];
    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    char *rep_data = rep->data;

    fill_req_buf(req_buf, DSV_MSG_GET, hndl);

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
 */
template< typename T >
int DSV_Get( void *ctx, void *hndl, T *value )
{
    assert( ctx );
    assert( hndl );
    assert( value );

    int rc = EINVAL;
    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    char rep_buf[BUFSIZE];
    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    char *rep_data = rep->data;

    fill_req_buf(req_buf, DSV_MSG_GET, hndl);

    rc = dsv_SendMsg( ctx, req_buf, req->length, rep_buf, sizeof(rep_buf) );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server" );
        return EFAULT;
    }
    memcpy( value, rep_data, sizeof(dsv_value_t) );
    return rc;
}


/**
 * return zero if successful. Otherwise it shall return -1
 */
int DSV_SubByName( void *ctx, const char *name )
{
    assert( ctx );
    assert( name );

    int rc;
    dsv_context_t *dsv_ctx = (dsv_context_t *)ctx;
    rc = zmq_setsockopt( dsv_ctx->sock_subscribe,
                         ZMQ_SUBSCRIBE,
                         name,
                         strlen( name ) + 1 );
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
    assert( ctx );
    assert( name );
    assert( value );

    char sub_buf[BUFSIZE];
    int rc = dsv_RecvMsg( ctx, sub_buf, sizeof(sub_buf) );
    if( rc == 0 )
    {
        strncpy( name, sub_buf, nlen );
        memcpy( value, sub_buf + strlen( sub_buf ) + 1, vlen );
    }

    return rc;
}

int DSV_AddItemToArray( void *ctx, void *hndl, int value )
{
    assert(ctx);
    assert(hndl);

    int rc = EINVAL;
    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    rc = fill_req_buf(req_buf, DSV_MSG_ADD_ITEM, hndl);
    req_data += rc;

    *(int *)req_data = value;
    req->length += sizeof(value);

    rc = dsv_SendMsg( ctx, req_buf, req->length, NULL, 0 );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server" );
        return EFAULT;
    }

    return rc;
}

int DSV_InsItemToArray( void *ctx, void *hndl, int index, int value )
{
    assert(ctx);
    assert(hndl);

    int rc = EINVAL;
    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    rc = fill_req_buf(req_buf, DSV_MSG_INS_ITEM, hndl);
    req_data += rc;

    *(int *)req_data = index;
    req->length += sizeof(index);
    req_data += sizeof(index);

    *(int *)req_data = value;
    req->length += sizeof(value);

    rc = dsv_SendMsg( ctx, req_buf, req->length, NULL, 0 );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server" );
        return EFAULT;
    }

    return rc;
}

int DSV_DelItemFromArray( void *ctx, void *hndl, int index )
{
    assert(ctx);
    assert(hndl);

    int rc = EINVAL;
    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    rc = fill_req_buf(req_buf, DSV_MSG_DEL_ITEM, hndl);
    req_data += rc;

    *(int *)req_data = index;
    req->length += sizeof(index);

    rc = dsv_SendMsg( ctx, req_buf, req->length, NULL, 0 );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server" );
        return EFAULT;
    }

    return rc;
}

int DSV_SetItemInArray( void *ctx, void *hndl, int index, int value )
{
    assert(ctx);
    assert(hndl);

    int rc = EINVAL;
    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    rc = fill_req_buf(req_buf, DSV_MSG_SET_ITEM, hndl);
    req_data += rc;

    *(int *)req_data = index;
    req->length += sizeof(index);
    req_data += sizeof(index);

    *(int *)req_data = value;
    req->length += sizeof(value);

    rc = dsv_SendMsg( ctx, req_buf, req->length, NULL, 0 );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server" );
        return EFAULT;
    }

    return rc;
}

int DSV_GetItemFromArray( void *ctx, void *hndl, int index, int *value )
{
    assert(ctx);
    assert(hndl);

    int rc = EINVAL;
    char req_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    char *req_data = req->data;

    char rep_buf[BUFSIZE];
    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;
    char *rep_data = rep->data;

    rc = fill_req_buf(req_buf, DSV_MSG_GET_ITEM, hndl);
    req_data += rc;

    *(int *)req_data = index;
    req->length += sizeof(index);

    rc = dsv_SendMsg( ctx, req_buf, req->length, rep_buf, sizeof(rep_buf) );
    if( rc != 0 )
    {
        syslog( LOG_ERR, "Failed to send message to the server" );
        return EFAULT;
    }
    *value = *(int *)rep_data;
    return rc;
}
/* end of libsysvars group */

