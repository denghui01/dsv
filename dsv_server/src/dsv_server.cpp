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
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include "zmq.h"
#include "czmq.h"
#include "dsv.h"
#include "dsv_msg.h"
#include "dsv_var.h"
#include "dsv_log.h"

/*==============================================================================
                              Defines
==============================================================================*/

/*! Server name used in ZMQ endpoint */
#define DSV_SERVER_NAME         "DSV"

/*! zmq endpoint for tcp to cross node */
#define DSV_FRONTEND            ( "tcp://*:56789" )
#define DSV_BACKEND             ( "tcp://*:56788" )
#define DSV_REPLY               ( "tcp://*:56787" )

struct dsv_state
{
    /*! zmq context */
    void *zmq_ctx;

    /*! zmq frontend socket */
    void *sock_frontend;

    /*! zmq backend socket */
    void *sock_backend;

    /*! zmq reply socket */
    void *sock_reply;

    /*! zactor for speaker */
    void *speaker;

}g_state;

/*!=============================================================================
    Termination handlings
==============================================================================*/
#define S_NOTIFY_MSG " "
#define S_ERROR_MSG "Error while writing to self-pipe.\n"
static int s_fd;
static int pipefds[2];
static void s_signal_handler( int signal_value )
{
    signal_value = signal_value;
    int rc = write( s_fd, S_NOTIFY_MSG, sizeof(S_NOTIFY_MSG) );
    if( rc != sizeof(S_NOTIFY_MSG) )
    {
        rc = write( STDOUT_FILENO, S_ERROR_MSG, sizeof(S_ERROR_MSG) - 1 );
        exit( 1 );
    }
}

static void s_catch_signals( int fd )
{
    s_fd = fd;

    struct sigaction action;
    action.sa_handler = s_signal_handler;
    //  Doesn't matter if SA_RESTART set because self-pipe will wake up zmq_poll
    //  But setting to 0 will allow zmq_read to be interrupted.
    action.sa_flags = 0;
    sigemptyset( &action.sa_mask );
    sigaction( SIGINT, &action, NULL );
    sigaction( SIGTERM, &action, NULL );
}

static void s_create_pipe()
{
    int rc;
    rc = pipe( pipefds );
    if( rc != 0 )
    {
        perror( "Creating self-pipe" );
        exit( 1 );
    }
    for( int i = 0; i < 2; i++ )
    {
        int flags = fcntl( pipefds[i], F_GETFL, 0 );
        if( flags < 0 )
        {
            perror( "fcntl(F_GETFL)" );
            exit( 1 );
        }
        rc = fcntl( pipefds[i], F_SETFL, flags | O_NONBLOCK );
        if( rc != 0 )
        {
            perror( "fcntl(F_SETFL)" );
            exit( 1 );
        }
    }

    s_catch_signals( pipefds[1] );
}

/*!=============================================================================

    Handle request messages from client, including dsv get_xxx APIs that need
    feedback

@return
    0 for success, non-zero for failure
==============================================================================*/
static int dsv_handle_reply()
{
    void *rep_sock = g_state.sock_reply;
    assert( rep_sock );

    int rc = 0;
    char req_buf[BUFSIZE];
    char rep_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    dsv_msg_reply_t *rep = (dsv_msg_reply_t *)rep_buf;

    rc = zmq_recv( rep_sock, req_buf, BUFSIZE, 0 );
    if( rc == -1 )
    {
        dsvlog( LOG_ERR, "zmq_recv failed: %s", strerror( errno ) );
        return rc;
    }

    switch( req->type )
    {
    case DSV_MSG_GET_HANDLE:
        rc = var_get_handle( req_buf, rep_buf );
        rep->result = rc;
        break;

    case DSV_MSG_GET_TYPE:
        rc = var_get_type( req_buf, rep_buf );
        rep->result = rc;
        break;

    case DSV_MSG_GET_LEN:
        rc = var_get_len( req_buf, rep_buf );
        rep->result = rc;
        break;

    case DSV_MSG_GET:
        rc = var_get( req_buf, rep_buf );
        rep->result = rc;
        break;

    case DSV_MSG_GET_NEXT:
        rc = var_get_next( req_buf, rep_buf );
        rep->result = rc;
        break;

    case DSV_MSG_GET_ITEM:
        rc = var_get_item( req_buf, rep_buf );
        rep->result = rc;
        break;

    default:
        dsvlog( LOG_ERR, "Unsupported request type!" );
        break;
    }

    rc = zmq_send( rep_sock, rep_buf, rep->length, 0 );
    if( rc == -1 )
    {
        dsvlog( LOG_ERR, "zmq_send failed: %s", strerror( errno ) );
        return rc;
    }

    return 0;
}

/*!=============================================================================

    Handle messages from client, including dsv create and set APIs that don't
    need any feedback. Forward the dsv value to backend.


@return
    0 for success, non-zero for failure
==============================================================================*/
static int dsv_handle_frontend()
{
    void *frontend = g_state.sock_frontend;
    void *backend = g_state.sock_backend;
    assert( frontend );
    assert( backend );

    int rc = 0;
    char req_buf[BUFSIZE];
    char fwd_buf[BUFSIZE];
    dsv_msg_request_t *req = (dsv_msg_request_t *)req_buf;
    dsv_msg_forward_t *fwd = (dsv_msg_forward_t *)fwd_buf;
    rc = zmq_recv( frontend, req_buf, BUFSIZE, 0 );
    if( rc == -1 )
    {
        dsvlog( LOG_ERR, "zmq_recv failed: %s", strerror( errno ) );
        return rc;
    }

    switch( req->type )
    {
    case DSV_MSG_CREATE:
        rc = var_create( req_buf, fwd_buf );
        break;

    case DSV_MSG_SET:
        rc = var_set( req_buf, fwd_buf );
        break;

    case DSV_MSG_ADD_ITEM:
        rc = var_add_item( req_buf, fwd_buf );
        break;

    case DSV_MSG_INS_ITEM:
        rc = var_ins_item( req_buf, fwd_buf );
        break;

    case DSV_MSG_DEL_ITEM:
        rc = var_del_item( req_buf, fwd_buf );
        break;

    case DSV_MSG_SET_ITEM:
        rc = var_set_item( req_buf, fwd_buf );
        break;

    case DSV_MSG_SAVE:
        rc = var_save();
        break;
    default:
        dsvlog( LOG_ERR, "Unsupported request type!" );
        break;
    }

    /* success operation needs forward the value to downstream */
    if( rc == 0 )
    {
        rc = zmq_send( backend, fwd->data, fwd->length, 0 );
        if( rc == -1 )
        {
            dsvlog( LOG_ERR, "zmq_send failed: %s", strerror( errno ) );
            return rc;
        }
    }

    return 0;
}

/*!=============================================================================

    Handle backend message, send the last value cached to the subscriber

@return
    0 for success, non-zero for failure
==============================================================================*/
static int dsv_handle_backend()
{
    void *backend = g_state.sock_backend;
    assert( backend );

    int rc = 0;
    char sub_buf[BUFSIZE];
    char fwd_buf[BUFSIZE];
    dsv_msg_forward_t *fwd = (dsv_msg_forward_t *)fwd_buf;
    char *fwd_data = fwd->data;

    rc = zmq_recv( backend, sub_buf, BUFSIZE, 0 );
    if( rc == -1 )
    {
        dsvlog( LOG_ERR, "zmq_recv failed: %s", strerror( errno ) );
        return rc;
    }

    rc = var_notify( sub_buf, fwd_buf );
    if( rc == 0 )
    {
        rc = zmq_send( backend, fwd_data, fwd->length, 0 );
        if( rc == -1 )
        {
            dsvlog( LOG_ERR, "zmq_send failed: %s", strerror( errno ) );
            return rc;
        }
    }
    return rc;
}

/*!=============================================================================

    listen to the client request and subscrription

@return
    0 for success, non-zero for failure
==============================================================================*/
static int mainloop()
{
    void *frontend = g_state.sock_frontend;
    void *backend = g_state.sock_backend;
    void *reply = g_state.sock_reply;

    assert( frontend );
    assert( backend );
    assert( reply );

    zmq_pollitem_t items[] = {
        { 0, pipefds[0], ZMQ_POLLIN, 0 },
        { frontend, 0, ZMQ_POLLIN, 0 },
        { backend, 0, ZMQ_POLLIN, 0 },
        { reply, 0, ZMQ_POLLIN, 0 }
    };

    while( 1 )
    {
        /* zmq_poll provides level-triggered fashion */
        if( zmq_poll( items, 4, -1 ) == -1 )
        {
            dsvlog( LOG_ERR, "zmq_poll failed: %s", strerror( errno ) );
            break;
        }

        /* Signal pipe FD */
        if( items[0].revents & ZMQ_POLLIN )
        {
            char buffer[1];
            int rc = read( pipefds[0], buffer, 1 );  // clear notifying byte
            dsvlog( LOG_WARNING, "interrupt received, killing server...\n" );
            break;
        }

        /*  new publish from frontend, update cache and then forward */
        if( items[1].revents & ZMQ_POLLIN )
        {
            /* Failure could be caused by client, don't exit proxy */
            dsv_handle_frontend();
        }

        /* new subscription from backend, send lvc */
        if( items[2].revents & ZMQ_POLLIN )
        {
            /* Failure could be caused by client, don't exit proxy */
            dsv_handle_backend();
        }

        /* new request from any client */
        if( items[3].revents & ZMQ_POLLIN )
        {
            /* Failure could be caused by client, don't exit proxy */
            dsv_handle_reply();
        }
    }

    var_save();
    return -1;
}

/*!=============================================================================
    Destroy the resource of dsv server
==============================================================================*/
static void dsv_server_destroy()
{
    dsvlog( LOG_ERR, "Exit and clean up proxy!" );

    /* clean up discover server */
    if( g_state.speaker != NULL )
    {
        zstr_sendx( (zactor_t *)g_state.speaker, "SILENCE", NULL );
        zactor_destroy( (zactor_t **)&g_state.speaker );
    }

    /* cleanup and terminate the process */
    if( g_state.sock_backend != NULL )
    {
        zmq_close( g_state.sock_backend );
    }
    if( g_state.sock_frontend != NULL )
    {
        zmq_close( g_state.sock_frontend );
    }
    if( g_state.sock_reply != NULL )
    {
        zmq_close( g_state.sock_reply );
    }
    if( g_state.zmq_ctx != NULL )
    {
        zmq_ctx_destroy( g_state.zmq_ctx );
    }
}

/*!=============================================================================

    running mainloop

@return
    0 for success, non-zero for failure
==============================================================================*/
static int dsv_server_run()
{
    return mainloop();
}

/*!=============================================================================

    Bind the socket to server endpoint, used for dsv read and write

@return
    0 for success, non-zero for failure
==============================================================================*/
static int dsv_setup_frontend()
{
    assert( g_state.zmq_ctx );

    int rc = 0;
    void *sock = zmq_socket( g_state.zmq_ctx, ZMQ_SUB );
    assert( sock );

    rc = zmq_bind( sock, DSV_FRONTEND );
    if( rc == 0 )
    {
        g_state.sock_frontend = sock;
        rc = zmq_setsockopt( sock, ZMQ_SUBSCRIBE, "", 0 );
        assert( rc == 0 );
    }
    else
    {
        dsvlog( LOG_ERR, "Error calling zmq_bind: %s", strerror( errno ) );
        zmq_close( sock );
        g_state.sock_frontend = NULL;
    }

    return rc;
}

/*!=============================================================================

    Bind the socket to an backend publisher endpoint, used for dsv subscription

@return
    0 for success, non-zero for failure
==============================================================================*/
static int dsv_setup_backend()
{
    assert( g_state.zmq_ctx );

    int rc = 0;
    void *sock = zmq_socket( g_state.zmq_ctx, ZMQ_XPUB );
    assert( sock );

    /* Bind PUB that subscribers need to connect to */
    rc = zmq_bind( sock, DSV_BACKEND );
    if( rc  == 0 )
    {
        g_state.sock_backend = sock;
    }
    else
    {
        dsvlog( LOG_ERR, "Error calling zmq_bind: %s", strerror( errno ) );
        zmq_close( sock );
        g_state.sock_backend = NULL;
    }

    return rc;
}

/*!=============================================================================

    Bind the socket to an req/rep endpoint, used for client request

@return
    0 for success, non-zero for failure
==============================================================================*/
static int dsv_setup_reply()
{
    assert( g_state.zmq_ctx );

    int rc = 0;
    void *sock = zmq_socket( g_state.zmq_ctx, ZMQ_REP );
    assert( sock );

    /* Bind PUB that subscribers need to connect to */
    rc = zmq_bind( sock, DSV_REPLY );
    if( rc  == 0 )
    {
        g_state.sock_reply = sock;
    }
    else
    {
        dsvlog( LOG_ERR, "Error calling zmq_bind: %s", strerror( errno ) );
        zmq_close( sock );
        g_state.sock_reply = NULL;
    }

    return rc;
}

/*!=============================================================================

    Initialize zmq for dsv server

@return
    0 for success, non-zero for failure
==============================================================================*/
static int dsv_server_init_zmq()
{
    int rc = 0;

    memset( &g_state, 0, sizeof(g_state) );

    /* create a self-pipe to get the exit signal */
    s_create_pipe();

    /**
     *  Try to discover the existing dsv server on the network.
     *  If there is one, don't run the server.
     */
    if( DSV_DiscoverServer( NULL, 0 ) )
    {
        return -1;
    }

    /*! run discoery server if there is not */
    g_state.speaker = DSV_RunServer();

    /* Create zmq context */
    g_state.zmq_ctx = zmq_ctx_new();
    if( g_state.zmq_ctx != NULL )
    {
        rc = dsv_setup_frontend();
        if( rc == 0 )
        {
            rc = dsv_setup_backend();
            if( rc == 0 )
            {
                rc = dsv_setup_reply();
                if( rc != 0 )
                {
                    dsvlog( LOG_ERR, "Failed to setup dsv reply" );
                    rc = -1;
                }
            }
            else
            {
                dsvlog( LOG_ERR, "Failed to setup dsv backend" );
                rc = -1;
            }
        }
        else
        {
            dsvlog( LOG_ERR, "Failed to setup dsv frontend" );
            rc = -1;
        }
    }
    else
    {
        dsvlog( LOG_ERR, "Error calling zmq_ctx_new, exiting" );
        rc = -1;
    }

    if( rc == -1 )
    {
        if( g_state.zmq_ctx != NULL )
        {
            zmq_ctx_destroy( g_state.zmq_ctx );
        }
    }

    return rc;
}

/*!=============================================================================
    Initialize dsv server

@return
    0 for success, non-zero for failure
==============================================================================*/
static int dsv_server_init()
{
    return dsv_server_init_zmq();
}

/*!=============================================================================

    Entry point for the proxy

@param[in]
    argc
        number of arguments passed to the process

@param[in]
    argv
        array of null terminated argument strings passed to the process
        argv[0] is the name of the process

@return
    0 on sucess, others on failure
==============================================================================*/
int main( int argc, char **argv )
{
    int rc;
    int c;

    /* parse the command line options */
    while( (c = getopt( argc, argv, "v" )) != -1 )
    {
        switch( c )
        {
        case 'v':
            break;

        default:
            break;
        }
    }

    /* initialize dsv module */
    DSV_LogInit( NULL, NULL );
    rc = dsv_server_init();
    if( rc == 0 )
    {
        /* Run dsv server mainloop */
        dsv_server_run();
    }

    /* should never reach here */
    dsv_server_destroy();
    exit( 1 );
}

