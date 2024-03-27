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

#ifndef DSV_VAR_H
#define DSV_VAR_H

int var_create( const char *req_buf, char *fwd_buf );
int var_set( const char *req_buf, char *fwd_buf );

int var_get( const char *req_buf, char *rep_buf );
int var_get_handle( const char *req_buf, char *rep_buf );
int var_get_type( const char *req_buf, char *rep_buf );
int var_get_len( const char *req_buf, char *rep_buf );
int var_get_next( const char *req_buf, char *rep_buf );
int var_notify( char *sub_buf, char *fwd_buf );
int var_save();
int var_restore();
int var_track( const char *req_buf, const char *rep_buf );

int var_get_item( const char *req_buf, char *rep_buf );
int var_set_item( const char *req_buf, char *fwd_buf );
int var_ins_item( const char *req_buf, char *fwd_buf );
int var_add_item( const char *req_buf, char *fwd_buf );
int var_del_item( const char *req_buf, char *fwd_buf );
#endif // DSV_VAR_H
