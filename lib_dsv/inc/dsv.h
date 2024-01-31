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

#ifndef DSV_H
#define DSV_H

/*==============================================================================
 Includes
 =============================================================================*/

#include <time.h>
#include <stdio.h>
#include <stdbool.h>

#ifndef BUFSIZE
    #define BUFSIZE                 (64 * 1024)
#endif

/*! the maximum length of any string in dsv, eg name, desc, tags, value, ... */
#define DSV_STRING_SIZE_MAX         (128)

/*! maximum number of dsv supported */
#define DSV_VRAS_NUM_MAX            (16 * 1024)

/*! maximum jason file size */
#define DSV_JSON_FILE_SIZE_MAX      (2 * 1024 * 1024)

typedef enum dsv_type
{
    DSV_TYPE_INVALID = 0,

    DSV_TYPE_STR = 1,

    DSV_TYPE_INT_ARRAY = 2,

    DSV_TYPE_UINT16 = 3,

    DSV_TYPE_SINT16 = 4,

    DSV_TYPE_UINT32 = 5,

    DSV_TYPE_SINT32 = 6,

    DSV_TYPE_FLOAT = 7,

    DSV_TYPE_UINT64 = 8,

    DSV_TYPE_SINT64 = 9,

    DSV_TYPE_DOUBLE = 10,

    DSV_TYPE_UINT8 = 11,

    DSV_TYPE_SINT8 = 12,

    DSV_TYPE_MAX = 13

} dsv_type_t;

typedef enum dsv_notification
{
    DSV_NOTI_INVALID = 0,

    DSV_NOTI_MODIFY = 1,

    DSV_NOTI_RENDER = 2,

    DSV_NOTI_CALC = 4,

    DSV_NOTI_VALIDATE = 8

} dsv_notification_t;


typedef union dsv_value
{
    /*! used for string */
    char *pStr;

    /*! used for array */
    void *pArray;

    uint16_t u16;

    int16_t s16;

    uint32_t u32;

    int32_t s32;

    float f32;

    double f64;

    uint8_t u8;

    int8_t s8;

    uint64_t u64;

    int64_t s64;

}dsv_value_t;

typedef struct dsv_info
{
    /*! pointer of the dsv name */
    char *pName;

    /*! pointer of dsv description */
    char *pDesc;

    /*! pointer of dsv tags, delimiter with , */
    char *pTags;

    /*! 32-bit instance identifier */
    uint32_t instID;

    /*! timestamp - time of last successful update/write */
    struct timespec timestamp;

    /*! dsv flags, like trackable, ... */
    uint32_t flags;

    /*! dsv type */
    int type;

    /*! specifies the length of the system variable data */
    size_t len;

    /*! buffer of dsv data */
    dsv_value_t value;

}dsv_info_t;

/*==============================================================================
                           Function Declarations
==============================================================================*/
void *DSV_Open( void );

void DSV_Close( void *ctx );

/* create a single dsv with dsv_info_t */
int DSV_Create( void *ctx, uint32_t instID, dsv_info_t *pDsv );

/* create a batch of dsv with JSON file */
int DSV_CreateWithJson( void *ctx, uint32_t instID, char *file );

/* query the handle of dsv */
void *DSV_Handle( void *ctx, uint32_t instID, const char *name );

/* query the type of dsv */
int DSV_Type( void *ctx, void *hndl );

/* the value set is in string form, no matter the real type of dsv */
int DSV_SetThruStr( void *ctx, void *hndl, char *value );

/* dsv is string type, set the value */
int DSV_Set( void *ctx, void *hndl, char *value );

/* dsv is int array type, set the value */
int DSV_Set( void *ctx, void *hndl, dsv_info_t *dsv );

/* dsv is numeric type, set the value */
template<typename T>
int DSV_Set( void *ctx, void *hndl, T value );

/* the value get is in string form, no matter the real type of dsv */
int DSV_GetThruStr( void *ctx, void *hndl, char *value, size_t size );

/* dsv is string type, get the value */
int DSV_Get( void *ctx, void *hndl, char *value, size_t size );

/* dsv is numeric type, set the value */
template<typename T>
int DSV_Get( void *ctx, void *hndl, T *value );

/* get notifications of subscribed dsvs*/
int DSV_GetNotification( void *ctx, char *name, size_t nlen, char *value, size_t vlen );

/* helper functions */
int DSV_SetByName( void *ctx, uint32_t instID, const char *name, char *value );
int DSV_GetByName( void *ctx, uint32_t instID, const char *name, char *value, size_t size );
int DSV_SubByName( void *ctx, uint32_t instID, const char *name );
int DSV_GetByNameFuzzy( void *ctx,
                        const char *search_name,
                        int last_index,
                        char *name,
                        size_t namesz,
                        char *value,
                        size_t valuesz );

/* dsv utilities */
void *memdup( const void *buf, size_t count );
int DSV_Memcpy( void *dest, dsv_info_t *dsv );
int DSV_Str2Value( const char *str, dsv_info_t *pDsv );
int DSV_Str2Array( const char *input, dsv_info_t *pDsv );
int DSV_Array2Str( char *buf, size_t len, const dsv_info_t *pDsv );
int DSV_Value2Str( char *buf, size_t len, const dsv_info_t *pDsv );
int DSV_Double2Value( double df, dsv_info_t *pDsv );
dsv_type_t DSV_GetTypeFromStr( const char *type_str );
int DSV_GetSizeFromType( int type );
void DSV_Print( const dsv_info_t *pDsv );

/* dsv discovery */
int DSV_DiscoverServer( char *server_ip, size_t size );
void *DSV_RunServer();


#if 0

int SYSVAR_fnCreateConstString( SVRM_HANDLE svrm_handle,
                                uint32_t instanceID,
                                const char *name,
                                const char *value,
                                const char *tags );


int SYSVAR_fnNotifyByHandle( SVRM_HANDLE svrm_handle,
                     int coid,
                     SYSVAR_HANDLE hSysVar,
                     teNotificationType notificationType );

int SYSVAR_fnNotifyByHandleExt( SVRM_HANDLE svrm_handle,
                                int coid,
                                SYSVAR_HANDLE hSysVar,
                                void *pData,
                                teNotificationType notificationType,
                                uint32_t options );

int SYSVAR_fnNotifyByName( SVRM_HANDLE svrm_handle,
                     int coid,
                     char *name,
                     teNotificationType notificationType );

/* 3rd party rendering functions */
FILE * SYSVAR_fnOpenFP( SVRM_HANDLE svrm_handle,
                        SYSVAR_PRINT_SESSION_HANDLE hSession,
                        SYSVAR_HANDLE *pHandle );

int SYSVAR_fnCloseFP( SVRM_HANDLE svrm_handle,
                      SYSVAR_PRINT_SESSION_HANDLE hSession,
                      FILE *fp );

int SYSVAR_fnOpenfd( SVRM_HANDLE svrm_handle,
                     SYSVAR_PRINT_SESSION_HANDLE hSession,
                     SYSVAR_HANDLE *pHandle );

int SYSVAR_fnClosefd( SVRM_HANDLE svrm_handle,
                      SYSVAR_PRINT_SESSION_HANDLE hSession,
                      int fd );

/* Shared memory producer/consumer APIs */

/* called by the consumer */
char *SYSVAR_fnCreateMem( SVRM_HANDLE svrm_handle,
                          size_t len,
                          int *fd );

void SYSVAR_fnDestroyMem( SVRM_HANDLE svrm_handle );

/* called by the producer */
char *SYSVAR_fnOpenMem( pid_t pid,
                        size_t len,
                         int fd );

void SYSVAR_fnCloseMem( int fd,
                        char *pBuf,
                        size_t len );

void SYSVAR_fnUnlinkMem( void );

void SYSVAR_fnSyncMem( char *pBuf,
                       size_t len );

void SYSVAR_fnRefreshMem( char *pBuf,
                          size_t len );

char *SYSVAR_fnGetSharedMem( SVRM_HANDLE svrm_handle );

/* system variable information functions */
uint32_t SYSVAR_fnGetID( SVRM_HANDLE svrm_handle,
                         SYSVAR_HANDLE hSysVar );

uint32_t SYSVAR_fnGetInstanceID( SVRM_HANDLE svrm_handle,
                                 SYSVAR_HANDLE hSysVar );

/* access system variables by name and instance ID */
SYSVAR_HANDLE SYSVAR_fnInstanceByName( SVRM_HANDLE svrm_handle,
                                       char *name,
                                       uint32_t instanceID );


/* tag functions */

int SYSVAR_fnHasTag( SVRM_HANDLE svrm_handle,
                     SYSVAR_HANDLE hSysVar,
                     char *tag,
                     uint32_t options );

int SYSVAR_fnSetTagsByName( SVRM_HANDLE svrm_handle,
                            char *name,
                            char *tag,
                            uint32_t options );

int SYSVAR_fnSetTagsByHandle( SVRM_HANDLE svrm_handle,
                              SYSVAR_HANDLE hSysVar,
                              char *tag,
                              uint32_t options );

int SYSVAR_fnGetTagName( SVRM_HANDLE svrm_handle,
                         uint8_t tagID,
                         char *buffer,
                         size_t len,
                         uint32_t options );

int SYSVAR_fnGetTagString( SVRM_HANDLE svrm_handle,
                           SYSVAR_HANDLE hSysvar,
                           char *buf,
                           size_t len );

int SYSVAR_fnTagsToString( SVRM_HANDLE svrm_handle,
                           uint8_t *pTags,
                           char *buf,
                           size_t len );

/* iterator functions */
SYSVAR_HANDLE SYSVAR_fnGetFirst( SVRM_HANDLE svrm_handle,
                                 uint32_t instanceID,
                                 char *pMatch,
                                 teMatchType matchType,
                                 char *pTagMatch,
                                 teTagMatchType tagMatchType,
                                 char *pValueMatch,
                                 teValMatchType valMatchType,
                                 uint16_t flags,
                                 uint16_t noFlags,
                                 uint32_t *contextIDName,
                                 uint32_t *contextIDTag,
                                 tzSysVarValueData *ptzSysVarValueData );

SYSVAR_HANDLE SYSVAR_fnGetNext( SVRM_HANDLE svrm_handle,
                                uint32_t instanceID,
                                SYSVAR_HANDLE hSysVar,
                                char *match,
                                teMatchType matchType,
                                teTagMatchType tagMatchType,
                                uint16_t flags,
                                uint16_t noFlags,
                                uint32_t contextIDName,
                                uint32_t contextIDTag,
                                tzSysVarValueData *ptzSysVarValueData );

/* expansion functions */
int SYSVAR_fnNeedsExpansion(char *input);

int SYSVAR_fnExpand(SVRM_HANDLE svrm_handle,
                    char *input,
                    FILE *fp,
                    char *tokenbuf,
                    size_t tokenbuflen,
                    uint32_t options);

/* TLV functions */
int SYSVAR_fnSetTLV( SVRM_HANDLE svrm_handle,
                     tzTLV *ptzTLV );

int SYSVAR_fnGetTLV( SVRM_HANDLE svrm_handle,
                     tzTLV *ptzTLV,
                     void *pBuf,
                     size_t buflen );

/* flag functions */
int SYSVAR_fnClearFlags( SVRM_HANDLE svrm_handle,
                         char *match,
                         uint16_t flags );

int SYSVAR_fnSetFlags( SVRM_HANDLE svrm_handle,
                       char *match,
                       uint16_t flags );

int SYSVAR_fnGetFlagsByHandle( SVRM_HANDLE svrm_handle,
                               SYSVAR_HANDLE hSysvar,
                               uint16_t *flags );

int SYSVAR_fnSetFlagsByHandle( SVRM_HANDLE svrm_handle,
                               SYSVAR_HANDLE hSysvar,
                               uint16_t flags );

int SYSVAR_fnClearFlagsByHandle( SVRM_HANDLE svrm_handle,
                                 SYSVAR_HANDLE hSysvar,
                                 uint16_t flags );

/* protection functions */
int SYSVAR_fnProtect( SVRM_HANDLE svrm_handle );

int SYSVAR_fnUnprotect( SVRM_HANDLE svrm_handle );

/* quality functions */
int SYSVAR_fnSetQualitiesStr( SVRM_HANDLE svrm_handle,
                              SYSVAR_tzCache **pSysVarCache,
                              const char *match,
                              const char *qstr,
                              uint32_t options );

int SYSVAR_fnSetQualities( SVRM_HANDLE svrm_handle,
                           SYSVAR_tzCache **pSysVarCache,
                           char *match,
                           uint32_t qualities,
                           uint32_t options );

int SYSVAR_fnSetQualitiesInCache( SVRM_HANDLE hSvrm,
                                  SYSVAR_tzCache *pSvCache,
                                  uint32_t quality,
                                  uint32_t options );

int SYSVAR_fnSetQualityByHandle( SVRM_HANDLE svrm_handle,
                                 SYSVAR_HANDLE hSysvar,
                                 uint32_t quality,
                                 uint32_t options);

int SYSVAR_fnSetQualityBitsByHandle( SVRM_HANDLE svrm_handle,
                                     SYSVAR_HANDLE hSysvar,
                                     uint32_t quality,
                                     uint32_t options );

int SYSVAR_fnClearQualityBitsByHandle( SVRM_HANDLE svrm_handle,
                                       SYSVAR_HANDLE hSysvar,
                                       uint32_t quality,
                                       uint32_t options );

int SYSVAR_fnGetQualityByHandle( SVRM_HANDLE svrm_handle,
                                 SYSVAR_HANDLE hSysvar,
                                 uint32_t *quality,
                                 uint32_t options);

int SYSVAR_fnSetOperatorBlocked( SVRM_HANDLE svrm_handle,
                                 SYSVAR_HANDLE hSysvar,
                                 bool blocked,
                                 uint32_t options );

int SYSVAR_fnQualityToStr( SYSVAR_tzQUALITY quality,
                           const char* delimiter,
                           char qstr[QUALITY_FLAGS_MAX_STRING_LEN] );

SYSVAR_tzQUALITY SYSVAR_fnStrToQuality( const char* pStr );
/* SYSVAR_HANDLE access functions */

int SYSVAR_fnGetByHandle( SVRM_HANDLE svrm_handle,
                          SYSVAR_HANDLE hSysvar,
                          teSysVarType type,
                          void *pBuf,
                          size_t len,
                          uint32_t options );

int SYSVAR_fnSetByHandle( SVRM_HANDLE svrm_handle,
                          SYSVAR_HANDLE hSysvar,
                          teSysVarType type,
                          void *pBuf,
                          size_t len,
                          uint32_t options );

int SYSVAR_fnIncrByHandle( SVRM_HANDLE svrm_handle,
                           SYSVAR_HANDLE hSysvar,
                           uint32_t options );

int SYSVAR_fnDecrByHandle( SVRM_HANDLE svrm_handle,
                           SYSVAR_HANDLE hSysvar,
                           uint32_t options );

int SYSVAR_fnWriteName( SVRM_HANDLE svrm_handle,
                        int fd,
                        SYSVAR_HANDLE handle,
                        uint32_t options );

int SYSVAR_fnPrintName( SVRM_HANDLE svrm_handle,
                        FILE *fp,
                        SYSVAR_HANDLE handle,
                        uint32_t options );

int SYSVAR_fnGetName( SVRM_HANDLE svrm_handle,
                      SYSVAR_HANDLE handle,
                      char *pBuf,
                      size_t buflen );

int SYSVAR_fnPrint(SVRM_HANDLE svrm_handle,
                   FILE *fp,
                   SYSVAR_HANDLE handle,
                   uint16_t arrayIdx,
                   uint32_t options);

int SYSVAR_fnWrite(SVRM_HANDLE svrm_handle,
                   int fd,
                   SYSVAR_HANDLE handle,
                   uint16_t arrayIdx,
                   uint32_t options);

int SYSVAR_fnSWrite(SVRM_HANDLE svrm_handle,
                   char* buffer,
                   size_t bufLen,
                   SYSVAR_HANDLE handle,
                   uint16_t arrayIdx,
                   uint32_t options);

int SYSVAR_fnGetExtData( SVRM_HANDLE svrm_handle,
                         SYSVAR_HANDLE hSysVar,
                         teExtDataType extDataType,
                         uint32_t extDataInstance,
                         tzSysvarExtData *ptzSysvarExtData,
                         size_t len,
                         void **context,
                         uint32_t options );

int SYSVAR_fnSetExtData( SVRM_HANDLE svrm_handle,
                         SYSVAR_HANDLE hSysVar,
                         tzSysvarExtData *ptzSysvarExtData,
                         size_t len,
                         uint32_t options );

int SYSVAR_fnQuery( SVRM_HANDLE svrm_handle,
					SYSVAR_HANDLE hSysvar,
					teQueryType type,
					SYSVAR_tzQUERY *ptzQuery,
					void *pData,
					size_t len );

int SYSVAR_fnGet16( SVRM_HANDLE svrm_handle,
                    SYSVAR_HANDLE hSysvar,
                    uint16_t *pVal,
                    uint32_t options );

int SYSVAR_fnGet16s( SVRM_HANDLE svrm_handle,
                    SYSVAR_HANDLE hSysvar,
                    int16_t *pVal,
                    uint32_t options );

int SYSVAR_fnGet32( SVRM_HANDLE svrm_handle,
                    SYSVAR_HANDLE hSysvar,
                    uint32_t *pVal,
                    uint32_t options );

int SYSVAR_fnGet32s( SVRM_HANDLE svrm_handle,
                    SYSVAR_HANDLE hSysvar,
                    int32_t *pVal,
                    uint32_t options );

int SYSVAR_fnGetFloat( SVRM_HANDLE svrm_handle,
                       SYSVAR_HANDLE hSysvar,
                       float *pVal,
                       uint32_t options );

int SYSVAR_fnGetStr( SVRM_HANDLE svrm_handle,
                     SYSVAR_HANDLE hSysvar,
                     char *pVal,
                     size_t len,
                     uint32_t options );

int SYSVAR_fnSet16( SVRM_HANDLE svrm_handle,
                    SYSVAR_HANDLE hSysvar,
                    uint16_t val,
                    uint32_t options );

int SYSVAR_fnSet16s( SVRM_HANDLE svrm_handle,
                    SYSVAR_HANDLE hSysvar,
                    int16_t val,
                    uint32_t options );

int SYSVAR_fnSet32( SVRM_HANDLE svrm_handle,
                    SYSVAR_HANDLE hSysvar,
                    uint32_t val,
                    uint32_t options );

int SYSVAR_fnSet32s( SVRM_HANDLE svrm_handle,
                     SYSVAR_HANDLE hSysvar,
                     int32_t val,
                     uint32_t options );

int SYSVAR_fnGet64( SVRM_HANDLE svrm_handle,
                    SYSVAR_HANDLE hSysvar,
                    uint64_t *pVal,
                    uint32_t options );

int SYSVAR_fnGet64s( SVRM_HANDLE svrm_handle,
                    SYSVAR_HANDLE hSysvar,
                    int64_t *pVal,
                    uint32_t options );

int SYSVAR_fnSet64( SVRM_HANDLE svrm_handle,
                    SYSVAR_HANDLE hSysvar,
                    uint64_t val,
                    uint32_t options );

int SYSVAR_fnSet64s( SVRM_HANDLE svrm_handle,
                     SYSVAR_HANDLE hSysvar,
                     int64_t val,
                     uint32_t options );

int SYSVAR_fnGet8( SVRM_HANDLE svrm_handle,
                   SYSVAR_HANDLE hSysvar,
                   uint8_t *pVal,
                   uint32_t options );

int SYSVAR_fnGet8s( SVRM_HANDLE svrm_handle,
                    SYSVAR_HANDLE hSysvar,
                    int8_t *pVal,
                    uint32_t options );

int SYSVAR_fnSet8( SVRM_HANDLE svrm_handle,
                   SYSVAR_HANDLE hSysvar,
                   uint8_t val,
                   uint32_t options );

int SYSVAR_fnSet8s( SVRM_HANDLE svrm_handle,
                    SYSVAR_HANDLE hSysvar,
                    int8_t val,
                    uint32_t options );

int SYSVAR_fnSetFloat( SVRM_HANDLE svrm_handle,
                       SYSVAR_HANDLE hSysvar,
                       float val,
                       uint32_t options );

int SYSVAR_fnSetStr( SVRM_HANDLE svrm_handle,
                     SYSVAR_HANDLE hSysvar,
                     char *pVal,
                     uint32_t options );

int SYSVAR_fnGetStats( SVRM_HANDLE svrm_handle, FILE *fp);

tzSysVarMetaData *SYSVAR_fnMetaDataInit( SVRM_HANDLE svrm_handle,
                                         uint32_t options );

tzSysVarMetaData *SYSVAR_fnMetaDataGetStruct( SVRM_HANDLE svrm_handle,
                                              SYSVAR_HANDLE hSysvar,
                                              uint32_t options );

int SYSVAR_fnMetaDataAdd( SVRM_HANDLE svrm_handle,
                          tzSysVarMetaData *ptzSysVarMetaData,
                          char *key,
                          char *value,
                          uint32_t options );

int SYSVAR_fnMetaDataAssign( SVRM_HANDLE svrm_handle,
                             SYSVAR_HANDLE hSysvar,
                             tzSysVarMetaData *ptzSysVarMetaData,
                             uint32_t options );

int SYSVAR_fnMetaDataGet( SVRM_HANDLE svrm_handle,
                          SYSVAR_HANDLE hSysvar,
                          char *key,
                          char *buffer,
                          size_t len,
                          uint32_t options );

int SYSVAR_fnAlias( SVRM_HANDLE svrm_handle,
                          SYSVAR_HANDLE hSysvar,
                          char *alias,
                          uint32_t options );

int SYSVAR_fnGetAlias( SVRM_HANDLE svrm_handle,
                       SYSVAR_HANDLE hSysvar,
                       char *buffer,
                       size_t len,
                       uint32_t *context,
                       uint32_t options );

int SYSVAR_fnGetNotificationInfo( SVRM_HANDLE svrm_handle,
                                  void *hNotificationInfo,
                                  SYSVAR_HANDLE *hSysvar,
                                  uint32_t *guid,
                                  uint32_t *instanceID,
                                  uint16_t *type,
                                  uint16_t *flags,
                                  uint32_t *length,
                                  SVData *data,
                                  void *pExtData,
                                  size_t extDataLen,
                                  void **pUserData,
                                  uint32_t options );

int SYSVAR_fnNotificationEnd( SVRM_HANDLE svrm_handle,
                              void *hNotificationInfo,
                              int result,
                              uint32_t options );

int SYSVAR_fnGetQualityString( uint32_t qualityValue,
                               char *pQualityString );

int SYSVAR_fnPrintQuality(SVRM_HANDLE svrm_handle,
                          SYSVAR_HANDLE hSysvar,
                          FILE *fp,
                          uint32_t options);

int SYSVAR_fnGetPermissions( SVRM_HANDLE svrm_handle,
                             SYSVAR_HANDLE hSysvar,
                             permission_t *pPermissions,
                             uint32_t options );

int SYSVAR_fnSetPermissions( SVRM_HANDLE svrm_handle,
                             SYSVAR_HANDLE hSysvar,
                             permission_t *pPermissions,
                             uint32_t options );

int SYSVAR_fnGetReference( SVRM_HANDLE svrm_handle,
                           SYSVAR_HANDLE hSysvar,
                           uint32_t *pReference,
                           uint32_t options );

int SYSVAR_fnCheckReference( SVRM_HANDLE svrm_handle,
                             SYSVAR_HANDLE hSysvar1,
                             SYSVAR_HANDLE hSysvar2,
                             uint32_t options );

/* cache functions */
int SYSVAR_fnSetAll( SVRM_HANDLE svrm_handle,
                     const char *match,
                     teSysVarType type,
                     void *pData,
                     size_t len,
                     SYSVAR_tzCache **pSysVarCache,
                     uint32_t options );

int SYSVAR_fnSetAllStr(  SVRM_HANDLE svrm_handle,
                         const char *match,
                         const char *value,
                         SYSVAR_tzCache **pSysVarCache,
                         uint32_t options );

int SYSVAR_fnConvertStringToNumber( teSysVarType type,
                                    const char *value,
                                    void *pNumber,
                                    size_t *pLen );

int SYSVAR_fnCreateCache( SVRM_HANDLE svrm_handle,
                          const char *match,
                          SYSVAR_tzCache **pSysVarCache,
                          uint32_t options );


int SYSVAR_fnFindAddToCache( SVRM_HANDLE svrm_handle,
                             const char *tags,
                             const char *match,
                             SYSVAR_tzCache **pSysVarCache,
                             uint32_t options );

int SYSVAR_fnFreeCache( SYSVAR_tzCache **pSysVarCache );

int SYSVAR_fnSetAllInCache( SVRM_HANDLE svrm_handle,
                            SYSVAR_tzCache *pSysVarCache,
                            teSysVarType type,
                            void *pData,
                            size_t len,
                            uint32_t options );

int SYSVAR_fnAddToCache( SYSVAR_tzCache **pSysVarCache,
                         SYSVAR_HANDLE hSysvar,
                         uint16_t flags,
                         uint32_t options );

int SYSVAR_fnNotifyAll( SVRM_HANDLE svrm_handle,
                        const char *match,
                        SYSVAR_tzCache **pSysVarCache,
                        int coid,
                        teNotificationType notificationType,
                        uint32_t options );

SYSVAR_HANDLE SYSVAR_fnIterateCache( SYSVAR_tzCache *pSysVarCache,
                                     void **saveptr,
                                     uint32_t options );

int SYSVAR_fnSetLanguage( SVRM_HANDLE svrm_handle,
                          const char *language,
                          uint32_t options );

int SYSVAR_fnGetPrintSessionLanguage( SVRM_HANDLE svrm_handle,
                                      SYSVAR_PRINT_SESSION_HANDLE hSession,
                                      char *langbuf,
                                      size_t langbuflen );

const char *SYSVAR_fnGetTypeName( teSysVarType type );
int SYSVAR_fnSetFromFloat( SVRM_HANDLE svrm_handle,
                           SYSVAR_HANDLE hSysvar,
                           float val,
                           teSysVarType *pType,
                           uint32_t options );

char *SYSVAR_fnCleanupCmd( char *pProcessName );

int SYSVAR_fnSetAllInCacheFromFloat( SVRM_HANDLE svrm_handle,
                                     SYSVAR_tzCache *pSysVarCache,
                                     float val,
                                     uint32_t options );
int SYSVAR_fnGetFloatAndQ( SVRM_HANDLE svrm_handle,
                           SYSVAR_HANDLE hSysvar,
                           float *f,
                           uint32_t *q );

int SYSVAR_fnCascade( SVRM_HANDLE hSVRM, char *name, char *value, uint32_t op );

teSysVarType SYSVAR_fnDecodeTypeName( char *typeName );

int SYSVAR_fnSVDataConst( char *pValue,
                          teSysVarType type,
                          SVData *pSVData );

int SYSVAR_fnSetSVData( SVRM_HANDLE svrm_handle,
                        SYSVAR_HANDLE hSysvar,
                        teSysVarType type,
                        SVData *pSVData,
                        uint32_t options );

int SYSVAR_fnGetSVData( SVRM_HANDLE svrm_handle,
                        SYSVAR_HANDLE hSysvar,
                        teSysVarType type,
                        SVData *pSVData,
                        size_t len,
                        uint32_t options );

int SYSVAR_fnCompareSVData( SVData data1, SVData data2, teSysVarType type );


/*! @} */
#endif

#endif

