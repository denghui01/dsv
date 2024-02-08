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

#ifndef DSV_LOG_H
#define DSV_LOG_H

#define        LOG_EMERG        0        /* system is unusable */
#define        LOG_ALERT        1        /* action must be taken immediately */
#define        LOG_CRIT        2        /* critical conditions */
#define        LOG_ERR                3        /* error conditions */
#define        LOG_WARNING        4        /* warning conditions */
#define        LOG_NOTICE        5        /* normal but significant condition */
#define        LOG_INFO        6        /* informational */
#define        LOG_DEBUG        7        /* debug-level messages */

void DSV_LogInit(void *ctx = NULL, const char *logmask = NULL);
void DSV_SetLogmask( uint32_t mask );
void dsvlog( int priority, const char* format, ... );
#endif
