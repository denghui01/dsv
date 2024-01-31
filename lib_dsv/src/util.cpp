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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <assert.h>
#include <inttypes.h>
#include <vector>
#include "dsv.h"
#include "dsv_msg.h"

/*!=============================================================================

    This function allocates the memory and copy the content from source buffer
    Just like strdup, the caller will have to manage the memory

@param[in]
    buf
        source buffer

@param[in]
    count
        number of bytes to be copied

@return
    pointer - allocated memory pointer
    NULL - failed

==============================================================================*/
void* memdup( const void *buf, size_t count )
{
    assert( buf );
    void *dest = malloc( count );
    if( dest != NULL )
    {
        memcpy( dest, buf, count );
    }
    return dest;
}

/*!=============================================================================

    This function copied dsv value to the destination buffer based on the type
    of dsv value, string, array, numeric, ...

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
int DSV_Memcpy( void *dest, dsv_info_t *dsv )
{
    assert( dest );
    assert( dsv );

    int rc = 0;
    void *pData = dest;
    if( dsv->type == DSV_TYPE_STR )
    {
        strcpy( (char *)dest, dsv->value.pStr );
        rc += strlen( dsv->value.pStr ) + 1;
    }
    else if( dsv->type == DSV_TYPE_INT_ARRAY )
    {
        memcpy( pData, dsv->value.pArray, dsv->len );
        rc += dsv->len;
    }
    else
    {
        memcpy( dest, &dsv->value, sizeof(dsv_value_t) );
        rc += sizeof(dsv_value_t);
    }
    return rc;
}
/*!=============================================================================

    Convert a string into the value based on the dsv type

@param[in]
    input
        C string. If the type is array should be "a,b,c"

@param[in]
    pDsv
        pointer to the dsv information

@return
    0 - success
    any other value specifies an error code (see errno.h)

==============================================================================*/
int DSV_Str2Array( const char *input, dsv_info_t *pDsv )
{
    int rc = EINVAL;
    std::vector< int > vi;
    char str[DSV_STRING_SIZE_MAX];
    strncpy( str, input, DSV_STRING_SIZE_MAX );
    pDsv->len = 0;
    if( pDsv->type == DSV_TYPE_INT_ARRAY )
    {
        /* parse and allocate memory for int array */
        char *token = strtok( str, "," );
        while( token )
        {
            vi.push_back( atoi( token ) );
            token = strtok( NULL, "," );
        }
        pDsv->len = vi.size() * sizeof(int);
        pDsv->value.pArray = memdup( vi.data(), pDsv->len );
        if( pDsv->value.pArray )
        {
            rc = 0;
        }
    }
    return rc;
}

/*!=============================================================================

    Convert a string into the value based on the dsv type

@param[in]
    str
        C string. If the type is array should be "a,b,c"

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
        pDsv->value.pStr = strdup( str );
        pDsv->len = strlen( str ) + 1;
        break;
    case DSV_TYPE_INT_ARRAY:
        DSV_Str2Array( str, pDsv );
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

    Convert an array in dsv into string in output buffer

@param[out]
    buf
        output buffer holding the converted string, null termninated

@param[in]
    len
        the size of buffer

@param[in]
    pDsv
        pointer to the dsv information

@return
    positive number - number of bytes copied
    -1 - error happened

==============================================================================*/
int DSV_Array2Str( char *buf, size_t len, const dsv_info_t *pDsv )
{
    assert( pDsv );
    assert( buf );

    int rc = -1;
    int i;
    int *ai;

    if( pDsv->type == DSV_TYPE_INT_ARRAY )
    {
        ai = (int *)pDsv->value.pArray;
        rc = 0;
        for( i = 0; i < pDsv->len / sizeof(int); ++i )
        {
            rc += snprintf( buf + rc, len - rc, "%d,", ai[i] );
        }
        buf[rc - 1] = '\0';
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
        pointer to the string buffer. For array, the output will be "a,b,c"

@param[in]
    len
        size of buffer
@return
    positive number - number of bytes copied
    -1 - error happened

==============================================================================*/
int DSV_Value2Str( char *buf, size_t len, const dsv_info_t *pDsv )
{
    assert( pDsv );
    assert( buf );

    int rc = -1;
    int i;
    int *ai;
    switch( pDsv->type )
    {
    case DSV_TYPE_STR:
        rc = snprintf( buf, len, "%s", pDsv->value.pStr );
        rc += 1; /* include terminated null byte */
        break;
    case DSV_TYPE_INT_ARRAY:
        rc = DSV_Array2Str( buf, len, pDsv );
        break;
    case DSV_TYPE_UINT16:
        rc = snprintf( buf, len, "%" PRIu16, pDsv->value.u16 );
        break;
    case DSV_TYPE_UINT32:
        rc = snprintf( buf, len, "%" PRIu32, pDsv->value.u32 );
        break;
    case DSV_TYPE_UINT64:
        rc = snprintf( buf, len, "%" PRIu64, pDsv->value.u64 );
        break;
    case DSV_TYPE_UINT8:
        rc = snprintf( buf, len, "%" PRIu8, pDsv->value.u8 );
        break;
    case DSV_TYPE_SINT16:
        rc = snprintf( buf, len, "%" PRId16, pDsv->value.s16 );
        break;
    case DSV_TYPE_SINT32:
        rc = snprintf( buf, len, "%" PRId32, pDsv->value.s32 );
        break;
    case DSV_TYPE_SINT64:
        rc = snprintf( buf, len, "%" PRId64, pDsv->value.s64 );
        break;
    case DSV_TYPE_SINT8:
        rc = snprintf( buf, len, "%" PRId8, pDsv->value.s8 );
        break;
    case DSV_TYPE_FLOAT:
        rc = snprintf( buf, len, "%f", pDsv->value.f32 );
        break;
    case DSV_TYPE_DOUBLE:
        rc = snprintf( buf, len, "%f", pDsv->value.f64 );
        break;
    default:
        printf( "Unsupported type of dsv!\n" );
        rc = EINVAL;
        break;
    }

    return rc;
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
int DSV_Double2Value( double df, dsv_info_t *pDsv )
{
    assert( pDsv );
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

    if( strcmp( type_str, "string" ) == 0 )
    {
        return DSV_TYPE_STR;
    }
    if( strcmp( type_str, "int_array" ) == 0 )
    {
        return DSV_TYPE_INT_ARRAY;
    }
    if( strcmp( type_str, "uint16" ) == 0 )
    {
        return DSV_TYPE_UINT16;
    }
    if( strcmp( type_str, "sint16" ) == 0 )
    {
        return DSV_TYPE_SINT16;
    }
    if( strcmp( type_str, "uint32" ) == 0 )
    {
        return DSV_TYPE_UINT32;
    }
    if( strcmp( type_str, "sint32" ) == 0 )
    {
        return DSV_TYPE_SINT32;
    }
    if( strcmp( type_str, "float" ) == 0 )
    {
        return DSV_TYPE_FLOAT;
    }
    if( strcmp( type_str, "uint64" ) == 0 )
    {
        return DSV_TYPE_UINT64;
    }
    if( strcmp( type_str, "sint64" ) == 0 )
    {
        return DSV_TYPE_SINT64;
    }
    if( strcmp( type_str, "double" ) == 0 )
    {
        return DSV_TYPE_DOUBLE;
    }
    if( strcmp( type_str, "uint8" ) == 0 )
    {
        return DSV_TYPE_UINT8;
    }
    if( strcmp( type_str, "sint8" ) == 0 )
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
    case DSV_TYPE_INT_ARRAY:
        /* For array type, update the length later */
        rc = 0;
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

void DSV_Print( const dsv_info_t *pDsv )
{
    assert( pDsv );
    printf( "name:  \t%s\n", pDsv->pName );
    printf( "desc:  \t%s\n", pDsv->pDesc );
    printf( "tags:  \t%s\n", pDsv->pTags );
    printf( "instID:\t%d\n", pDsv->instID );
    printf( "type:  \t%d\n", pDsv->type );
    printf( "ts:    \t%ld\n", pDsv->timestamp.tv_sec );
    char vstr[DSV_STRING_SIZE_MAX];
    DSV_Value2Str( vstr, DSV_STRING_SIZE_MAX, pDsv );
    printf( "value: \t%s\n", vstr );
    printf( "================\n" );
}
