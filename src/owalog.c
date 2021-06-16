/*
** mod_owa
**
** Copyright (c) 1999-2021 Oracle Corporation, All rights reserved.
**
** The Universal Permissive License (UPL), Version 1.0
**
** Subject to the condition set forth below, permission is hereby granted
** to any person obtaining a copy of this software, associated documentation
** and/or data (collectively the "Software"), free of charge and under any
** and all copyright rights in the Software, and any and all patent rights
** owned or freely licensable by each licensor hereunder covering either
** (i) the unmodified Software as contributed to or provided by such licensor,
** or (ii) the Larger Works (as defined below), to deal in both
** 
** (a) the Software, and
** (b) any piece of software and/or hardware listed in the lrgrwrks.txt file
** if one is included with the Software (each a "Larger Work" to which the
** Software is contributed by such licensors),
** 
** without restriction, including without limitation the rights to copy, create
** derivative works of, display, perform, and distribute the Software and make,
** use, sell, offer for sale, import, export, have made, and have sold the
** Software and the Larger Work(s), and to sublicense the foregoing rights on
** either these or other terms.
** 
** This license is subject to the following condition:
** The above copyright notice and either this complete permission notice or at
** a minimum a reference to the UPL must be included in all copies or
** substantial portions of the Software.
** 
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
** IN THE SOFTWARE.
*/
/*
** File:         owalog.c
** Description:  Logging via Apache socket connections
** Author:       Doug McMahon (from work by Chris Fischer)
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
**
** This file contains a specialized routine for logging diagnostic
** information to logging servers via socket connections.  It uses
** the OCI to call a special PL/SQL procedure that returns lists
** of IP addresses, and uses Apache internal socket interfaces to
** connect to these addresses and send logging information.  The
** Apache socket interfaces are available only on 2.0 or higher, so
** no implementation is provided for Apache 1.3.  The logging data
** structure is opaque outside this routine because of the direct
** references to Apache internal data types within the structure.
**
** History
**
** 08/31/2008   C. Fischer      Created
** 11/27/2012   D. McMahon      GCC fixes, Win-64 porting
** 09/19/2013   D. McMahon      Switch to component time facility
*/

#define WITH_OCI
#define WITH_APACHE

#ifdef WIN64
# ifndef WIN32
# define WIN32 /* ### This works around define issues in Apache headers ### */
# endif
#endif
#include <http_request.h>
#include <http_log.h>
#include <apr.h>
#include <apr_strings.h>

#include <modowa.h>

#ifndef PROTOCOL_ARG
# ifdef APACHE22
#  define PROTOCOL_ARG
# endif
#endif
#ifndef PROTOCOL_ARG
# ifdef APACHE24
#  define PROTOCOL_ARG
# endif
#endif

#define MIN_LOOKUP_PERIOD  60000
#define MIN_CONNECT_PERIOD 60000

#define GET_LOG_ADDRS  "begin MOD_OWA.GET_LOG_ADDRESSES(:addresses); end;"

/*
** Allocate a socket-management sub-structure and return it,
** or null on failure.
*/
owa_log_socks *owa_log_alloc(owa_context *octx)
{
    owa_log_socks *s = (owa_log_socks *)0;
    s = (owa_log_socks *)mem_zalloc(sizeof(owa_log_socks));
    apr_pool_create(&(s->mem_pool), 0);
    return(s);
}

/*
** Free a socket-management sub-structure.
** Returns null if freed successfully
*/
owa_log_socks *owa_log_free(owa_log_socks *s)
{
    if (s)
    {
        if (s->log_socket) apr_socket_close(s->log_socket);
        if (s->mem_pool) apr_pool_destroy(s->mem_pool);
        mem_free(s);
        s = (owa_log_socks *)0;
    }
    return(s);
}

static un_long log_delta_time(long_64 stime, long_64 etime)
{
  return((un_long)(util_delta_time(stime, etime)/(long_64)1000));
}

/*
** Often a PL/SQL HTTP generating program will produce logging information that
** is used for debugging and diagnostic procedures.  This procedure processes
** this logging information after it has been retrieved from the OWA toolkit.
*/
void owa_log_send(owa_context *octx, request_rec *r,
                  connection *c, owa_request *owa_req)
{
    sword           status;
    ub4             asize;
    sb4             oerrno;
    sb2             inds[OWA_LOG_ADDR_COUNT];
    ub2             lens[OWA_LOG_ADDR_COUNT];
    char           *optr;
    ub4             i;
    char           *listenHost;
    char           *scopeID; /* unused */
    apr_port_t      listenPort;
    apr_status_t    apr_sts;
    apr_sockaddr_t *addr;
    apr_size_t      send_len;
    int             retry = 1;
    apr_pool_t     *local_pool = 0;
    char           *op = "";
    long_64         etime;
    owa_log_socks  *s = c->sockctx;

    if (!s) return;
    if (!(owa_req->logbuffer) || !(owa_req->loglength)) return;

    /* Wrap around back to first address in list */
    if (s->log_addr_i >= s->log_addr_n)
    {
        s->log_addr_i = 0;
    }

    etime = os_get_component_time(0);

nextlookup:
    /*
    ** If we do not know the address(es) of the logging service then we make
    ** a call to the database to fetch them.
    */
    if ((s->log_addr_n <= 0) &&
        (log_delta_time(s->log_lookup_ts, etime) > MIN_LOOKUP_PERIOD))
    {
        s->log_lookup_ts = etime;

        retry = 0;
        asize = OWA_LOG_ADDR_COUNT;

        optr = s->log_addrs;
        for (i=0; (i < asize); ++i)
        {
            optr[0] = '\0';
            inds[i] = -1;
            lens[i] = 0;
            optr += OWA_LOG_ADDR_WIDTH;
        }

        c->lastsql = GET_LOG_ADDRS;
        status = sql_parse(c, c->stmhp3, c->lastsql, -1);
        if (status != OCI_SUCCESS) goto logerr;

        status = sql_bind_arr(c, c->stmhp3, (ub4)1, s->log_addrs, lens,
                              (sb4)OWA_LOG_ADDR_WIDTH, inds, &asize,
                              OWA_LOG_ADDR_COUNT);
        if (status != OCI_SUCCESS) goto logerr;

        asize = 0;
        status = sql_exec(c, c->stmhp3, (ub4)1, 0);
        if (status != OCI_SUCCESS) goto logerr;

        s->log_addr_n = asize;
        s->log_addr_i = 0;

logerr:
        if (status != OCI_SUCCESS)
        {
            oerrno = sql_get_error(c); 
            if (oerrno) status = oerrno;
        }
    }

nextaddr:
    /*
    ** If we do not have an open socket connection but we do know the address
    ** of the next available server then we go ahead and open a loggin socket
    ** that will be preserved with the Oracle connection object.
    */
    if (!(s->log_socket) && (s->log_addr_i < s->log_addr_n) &&
        (log_delta_time(s->log_connect_ts, etime) > MIN_CONNECT_PERIOD))
    {
        s->log_connect_ts = etime;
        optr = s->log_addrs + (s->log_addr_i * OWA_LOG_ADDR_WIDTH);

        op = "apr_pool_create";
        apr_sts = apr_pool_create(&local_pool, r->pool);
        if (apr_sts != 0) goto sockerr;

        op = "apr_parse_addr_port";
        apr_sts = apr_parse_addr_port(&listenHost, &scopeID, &listenPort,
                                      optr, local_pool);
        if (apr_sts != 0) goto sockerr;

        op = "apr_sockaddr_info_get";
        apr_sts = apr_sockaddr_info_get(&addr, listenHost, APR_INET,
                                        listenPort, 0, local_pool);
        if (apr_sts != 0) goto sockerr;

        op = "apr_socket_create";
        apr_sts = apr_socket_create(&(s->log_socket), APR_INET, SOCK_STREAM,
#ifdef PROTOCOL_ARG
                                    APR_PROTO_TCP,
#endif
                                    s->mem_pool);
        if (apr_sts != 0) goto sockerr;

        op = "apr_socket_connect";
        apr_sts = apr_socket_connect(s->log_socket, addr);

sockerr:
        if (apr_sts != 0)
        {
            ap_log_error(APLOG_MARK, APLOG_ERR, apr_sts, r->server,
                         "owa_log_send: %s (%d)", op, (int)apr_sts);

            if (s->log_socket)
            {
                apr_socket_close(s->log_socket);
                s->log_socket = 0;
            }
        }

        if (local_pool)
        {
            apr_pool_destroy(local_pool);
        }
    }

    /*
    ** If we have an open socket connection for this database connection
    ** then we use it to send the logging data to the appropriate server.
    */
    if (s->log_socket)
    {
        send_len = owa_req->loglength;
        apr_sts = apr_socket_send(s->log_socket,
                                  owa_req->logbuffer, &send_len);

        if ((apr_sts == 0) && (send_len == (apr_size_t)(owa_req->loglength)))
            return; /* Success */

        ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, r->server,
                     "owa_log_send: log send failure %d <=> %d (%d)",
                     (int)send_len, (int)(owa_req->loglength), (int)apr_sts);

        apr_socket_close(s->log_socket);
        s->log_socket = NULL;
    }

    /*
    ** If we get here then we were not able to send the logging data to
    ** the server.
    */
    ++(s->log_addr_i);

    /*
    ** Try the next server unless we have reached the end of our addres
    ** list (from the last call to the database).
    */
    if (s->log_addr_i < s->log_addr_n)
    {
        ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, 0, r->server,
                     "owa_log_send: retry next address (%d of %d)",
                     (int)(s->log_addr_i), (int)(s->log_addr_n));

        goto nextaddr;
    }

    /*
    ** If we have run out of servers then look for new ones.  We only do
    ** this once to avoid spinning while no servers are available.
    */
    if (retry &&
        (log_delta_time(s->log_lookup_ts, etime) > MIN_LOOKUP_PERIOD))
    {
        ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, 0, r->server,
                     "owa_log_send: retry address lookup (%d)",
                     (int)(s->log_addr_n));

        s->log_addr_n = 0;
        retry = 0;
        goto nextlookup;
    }

    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, r->server,
                 "owa_log_send: discarding %d bytes of logging data",
                 (int)(owa_req->loglength));
}
