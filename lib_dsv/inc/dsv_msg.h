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

#ifndef DSV_MSG_H
#define DSV_MSG_H

/*==============================================================================
 Includes
 =============================================================================*/
#include <stdbool.h>

/*==============================================================================
                              Defines
==============================================================================*/
typedef enum DSV_MSG_TYPE
{
    DSV_MSG_START,
    DSV_MSG_CREATE,
    DSV_MSG_GET_HANDLE,
    DSV_MSG_GET_TYPE,
    DSV_MSG_SET,
    DSV_MSG_GET,
    DSV_MSG_GET_FIRST,
    DSV_MSG_GET_NEXT,
    DSV_MSG_SUBSCRIBE,
    
    DSV_MSG_APPLY_ID,
    DSV_MSG_MAX
}dsv_msg_type_t;

/*=============================================================================
                              Structures
==============================================================================*/
/*! The dsv_reply_msg_t message is used to reply message from server. */
typedef struct dsv_msg_request
{
    int         type;
    size_t      length;
    uint32_t    instID;
    char        data[0];
}dsv_msg_request_t;

typedef struct dsv_msg_reply
{
    size_t      length;
    int         result;
    char        data[0];
}dsv_msg_reply_t;

typedef struct dsv_msg_forward
{
    size_t      length;
    char        data[0];
}dsv_msg_forward_t;

#if 0
#define dsv_comm_msg_def struct {\
    size_t length; \
    int type; \
    uint32_t instID; }

typedef dsv_comm_msg_def dsv_comm_msg_t;

/*! The dsv_msg_create_t message is passed to the server to request the 
 * creation of a new system variable. 
 */
typedef struct
{
    /*! Common party of the message */
    dsv_comm_msg_def;

} dsv_msg_create_t;

/*! The dsv_msg_set_t message is used to set the value of a dsv using its 
 * name and a string representation of its value  
 */
typedef struct
{
    /*! Common party of the message */
    dsv_comm_msg_def;

    /*! The pid of the process requesting to set the dsv */
    pid_t pid;

    /*! dsv name */
    char name[DSV_STRING_SIZE_MAX];

    /*! dsv value union */
    dsv_value_t value;

} dsv_msg_set_t;

/*! The dsv_msg_get_t message is used to get the value of a dsv by name. */
typedef struct
{
    /*! Common party of the message */
    dsv_comm_msg_def;

    /*! The length of the buffer available to store the retrieved value */
    char name[DSV_STRING_SIZE_MAX];

} dsv_msg_get_t;

typedef union
{
    dsv_comm_msg_def;
    dsv_msg_create_t create;
    dsv_msg_set_t set;
    dsv_msg_get_t get;
}dsv_msg_t;

/*! The dsv_reply_msg_t message is used to reply message from server. */
#define dsv_comm_reply_def struct {\
    ssize_t length; \
    int type; \
    int result; }

typedef dsv_comm_reply_def dsv_comm_reply_t;

/*! The dsv_msg_reply_t message is use to reply the request from the client */
typedef struct
{
    /*! Common party of the message */
    dsv_comm_reply_def;

    /*! dsv value data */
    char vstr[DSV_STRING_SIZE_MAX];

} dsv_reply_get_t;

typedef union
{
    dsv_comm_reply_def;
    dsv_reply_get_t get;
}dsv_reply_t;

#endif
#endif // DSV_MSG_H
