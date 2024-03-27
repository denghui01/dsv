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
    DSV_MSG_GET_LEN,
    DSV_MSG_SET,
    DSV_MSG_GET,
    DSV_MSG_GET_NEXT,
    DSV_MSG_ADD_ITEM,
    DSV_MSG_DEL_ITEM,
    DSV_MSG_INS_ITEM,
    DSV_MSG_SET_ITEM,
    DSV_MSG_GET_ITEM,
    DSV_MSG_APPLY_ID,
    DSV_MSG_SAVE,
    DSV_MSG_RESTORE,
    DSV_MSG_TRACK,
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

#endif // DSV_MSG_H
