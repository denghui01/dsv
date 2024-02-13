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
#include <assert.h>
#include "zmq.h"
#include "czmq.h"

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
    zactor_t *listener = zactor_new( zbeacon, NULL );
    assert( listener );
//  zstr_sendx (listener, "VERBOSE", NULL);
    zsock_send( listener, "si", "CONFIGURE", 9999 );
    char *hostname = zstr_recv( listener );
    assert( *hostname );
    freen( hostname );

    //  We will listen to anything (empty subscription)
    zsock_send( listener, "sb", "SUBSCRIBE", "", 0 );

    //  Wait for at most 1/2 second if there's no broadcasting
    zsock_set_rcvtimeo( listener, 500 );
    char *ipaddress = zstr_recv( listener );
    if( ipaddress )
    {
        zframe_t *content = zframe_recv( listener );
        if( zframe_size( content ) == 2 &&
            zframe_data( content )[0] == 0xCA &&
            zframe_data( content )[1] == 0xFE )
        {
            printf( "Found a DSV server, ip=%s\n", ipaddress );
            if( server_ip != NULL )
            {
                strncpy( server_ip, ipaddress, size );
            }
            rc = 1;
        }

        zframe_destroy( &content );
        zstr_free( &ipaddress );
    }
    zactor_destroy( &listener );
    return rc;
}

/*!=============================================================================

    Broadcast message using zbeacon

@return
    0 for success, non-zero for failure
==============================================================================*/
void* DSV_RunServer()
{
    printf( "!!!Start to run server\n" );
    zactor_t *speaker = zactor_new( zbeacon, NULL );
    assert( speaker );
//  zstr_sendx (speaker, "VERBOSE", NULL);
    zsock_send( speaker, "si", "CONFIGURE", 9999 );
    char *hostname = zstr_recv( speaker );
    assert( *hostname );
    freen( hostname );

    //  We will broadcast the magic value 0xCAFE
    byte announcement[2] = { 0xCA, 0xFE };
    zsock_send( speaker, "sbi", "PUBLISH", announcement, 2, 100 );

    return speaker;
}

static uint32_t hash(unsigned char *str)
{
    assert(str);
    uint32_t hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

static int localIP(const char *if_name, char *ip, size_t size)
{
    assert(ip);
    struct ifaddrs* ifAddrStruct = NULL;
    struct ifaddrs* ifa = NULL;
    void* tmpAddrPtr = NULL;
    int rc = EINVAL;

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr->sa_family == AF_INET)
        {
            tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, tmpAddrPtr, ip, size);
            if( if_name != NULL )
            {
                if( strcmp(ifa->ifa_name, if_name) == 0 )
                {
                    rc = 0;
                    break;
                }
            }
            else
            {
                if( atoi(ip) != 127 )
                {
                    /* Don't use loopback interface */
                    rc = 0;
                    break;
                }
            }
        }
    }
    if (ifAddrStruct != NULL)
    {
        freeifaddrs(ifAddrStruct);//remember to free ifAddrStruct
    }

    return rc;
}

/**
* We here use local IP to generate instID from its hash value
*/
uint32_t DSV_GetInstID(const char *if_name)
{
    char ip[64];
    uint32_t instID = 0;

    int rc = localIP( if_name, ip, 64 );
    if( rc == 0 )
    {
        instID = hash( (unsigned char *)ip );
    }
    return instID;
}

