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
#include <cstdint>
#include <assert.h>
#include <syslog.h>
#include <stdarg.h>
#include <cstring>
#include <zmq.h>
#include "dsv.h"

/*! console log level, just like syslog mask */
static uint32_t g_console_logmask = ~0;
static bool g_run_in_foreground = true;
/* TODO: use hndl to check the notification */
void *g_hndl_logmask;

/*!=============================================================================

    output log message based on process running on foreground or background as
    well as the logmask

    running on foreground: output through console
    running on background: output through syslog service

@param[in]
    priority
        the log priority, same definition as syslog

    This determines the importance of the message. The levels are,
    in order of decreasing importance:

    LOG_EMERG   system is unusable

    LOG_ALERT   action must be taken immediately

    LOG_CRIT    critical conditions

    LOG_ERR     error conditions

    LOG_WARNING warning conditions

    LOG_NOTICE  normal, but significant, condition

    LOG_INFO    informational message

    LOG_DEBUG   debug-level message
==============================================================================*/
void dsvlog( int priority, const char* format, ... )
{
    va_list args;
    va_start( args, format );

    if( g_run_in_foreground )
    {
        /* use console print when process is runnig in foreground */
        if( priority & g_console_logmask )
        {
            vprintf( format, args );
            printf("\n");
        }
    }
    else
    {
        /* use syslog when program is running in background */
        vsyslog( priority, format, args );
    }
    va_end( args );
}

/*!=============================================================================

    Initialize dsv log service with the module name

@param[in]
    log_level
        the name of log_level dsv , like "[0]/SYS/DEV/LOG_LEVEL".
==============================================================================*/
void DSV_LogInit(void *ctx, const char *log_level)
{
    int rc = 0;
    /* determine the process is running background or not */
    g_run_in_foreground = (getpgrp() == tcgetpgrp(STDOUT_FILENO));

    if( ctx != NULL && log_level != NULL)
    {
        /* create console log mask dsv */
        dsv_info_t dsv;
        char name[DSV_STRING_SIZE_MAX];
        sscanf(log_level, "[%d]", &dsv.instID);
        dsv.pName = strdup(log_level);
        dsv.type = DSV_TYPE_UINT32;
        dsv.len = DSV_GetSizeFromType( dsv.type );
        dsv.value.u32 = LOG_WARNING;
        rc = DSV_Create( ctx, dsv.instID, &dsv );
        free( dsv.pName );
        if( rc != 0 )
        {
            /* continue even fail one dsv */
            dsvlog( LOG_ERR, "Failed to create dsv: %s", log_level );
        }

        /* subscribe the dsv */
        rc = DSV_SubByName( ctx, log_level );
        if( rc != 0 )
        {
            /* continue even fail one dsv */
            dsvlog( LOG_ERR, "Failed to subscribe dsv: %s", log_level );
        }
    }
}

void DSV_SetLogmask( uint32_t mask )
{
    g_console_logmask = mask;
}

uint32_t DSV_GetLogmask()
{
    return g_console_logmask;
}

