/*
** mod_owa
**
** Copyright (c) 1999-2019 Oracle Corporation, All rights reserved.
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
** File:         owad.c
** Description:  OWA server daemon.
** Author:       Doug McMahon      Doug.McMahon@oracle.com
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
**
** Demonstration of multithreaded daemon program with socket communications.
** The main program creates a latch and a socket that are shared by
** all worker threads.  With the latch held, it spawns the workers,
** which all block waiting for the latch.  The main program then
** releases the latch, allowing the first worker to claim the socket.
** Thereafter, workers, gated by the latch, take turns listening for
** requests on the socket, processing them as needed.  A pool of database
** connections could be created here and reused for subsequent requests.
** Sessioning would also be possible, by returning the host IP address
** and socket number in a cookie, and using this for subsequent routing.
** Connections could be held open if desired, though this consumes child
** threads in the server, which must block waiting for more requests
** from the same client.
**
** History
**
** 09/15/2000   D. McMahon      Created
** 05/31/2001   D. McMahon      Began revisioning with 2.3.9
** 06/16/2001   D. McMahon      Added support for RAW input/output bindings
** 09/01/2001   D. McMahon      Added OwaRound
** 09/07/2001   D. McMahon      Added TIMING diagnostic
** 09/13/2001   D. McMahon      Fixed parse problems with OwaReset
** 12/12/2001   D. McMahon      Add the code for OwaReject
** 02/11/2002   D. McMahon      Add OwaDocFile
** 05/17/2002   D. McMahon      Added OwaEnv
** 06/11/2002   D. McMahon      Add THREADS mode to oracle_pool
** 06/19/2002   D. McMahon      Add backlog argument to socket_listen
** 07/22/2002   D. McMahon      Support flush for morq_write
** 06/06/2003   D. McMahon      Support special error page for SQL errors
** 01/19/2004   D. McMahon      Add sessioning support
** 09/02/2008   D. McMahon      Add OwaOptimizer and OwaWait
** 04/30/2010   D. McMahon      (long) casts for morq_read and morq_write
** 04/19/2011   D. McMahon      Unscramble connect string
** 03/28/2012   D. McMahon      Add OwaContentType support
** 05/14/2012   D. McMahon      Fix handling of pipelined requests on socket
** 02/25/2013   D. McMahon      Bump version
** 04/12/2013   D. McMahon      Add support for REST operations
** 06/09/2013   D. McMahon      Add OwaRefXml
** 09/19/2013   D. McMahon      Handle 64-bit content lengths
** 09/26/2013   D. McMahon      Pass REMOTE_ADDR and OWA.IP_ADDRESS if possible
** 10/21/2013   D. McMahon      Allow status line text in morq_set_status()
** 04/27/2015   D. McMahon      Bump version
** 05/07/2015   D. McMahon      Make morq_get_range use 64-bit ints
*/

#define APACHE_LINKAGE
#include <modowa.h>
#ifndef MODOWA_WINDOWS
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

#define REGEX_SUPPORTED /* Linux, Solaris */

#ifdef REGEX_SUPPORTED
# include <regex.h>
#endif

#endif

#define HTTP_LINE_MAX   1024
#define MAX_THREADS     255
#define MAX_WAIT        100
#define MAX_BLOCK       20
#define MAX_HEADERS     256
#define LISTEN_BACKLOG  500
#define WAIT_SOCKET     500 /* 1/2 second */

static char modowa_version[] = MODOWA_VERSION_STRING;

/*
** Sub-context for multi-threading support
*/
struct owa_mt_context
{
    os_objptr c_mutex;     /* Per-process connection pool latch */
    os_objptr c_semaphore; /* Per-process connection pool queue */
};

typedef struct daemon_context
{
    un_long      maintid;
    os_objptr    socklatch;
    os_socket    sock;
    os_thrhand   tids[MAX_THREADS];
    long         tinterval;
    owa_context *loc_list;
    char        *froot;
    char        *ipaddr;
    shm_context  mapmem;    
} daemon_context;

/*
** Structure to track request memory blocks
*/
typedef struct mem_block mem_block;
struct mem_block
{
    mem_block *next;
    void      *data;
};

/*
** Structure to hold a name/value pair
*/
typedef struct header_rec
{
    char *name;
    char *value;
} header_rec;

/*
** Request record
*/
struct request_rec
{
    char         buffer[4096];
    int          buflen;
    int          bufptr;
    long         contlen;
    os_socket    sock;
    int          sock_end;
    int          rtype;
    char        *uri;
    char        *script;
    char        *args;
    owa_context *octx;
    header_rec   headin[MAX_HEADERS];
    header_rec   headout[MAX_HEADERS];
    int          nheadin;
    int          nheadout;
    long_64      clength;
    char        *content_type;
    int          status;
    char        *status_line;
    mem_block   *mlist;
};

/*
** Global owad context; must be global to be visible to signal handers
*/
static daemon_context owad_context = {0};

void raise_file_limit()
{
#ifndef MODOWA_WINDOWS
    struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0)
      if (rlim.rlim_cur < rlim.rlim_max)
      {
          rlim.rlim_cur = rlim.rlim_max;
          setrlimit(RLIMIT_NOFILE, &rlim);
      }
#endif
}

/*
** Print a line to standard out, followed by a line break
** ### SHOULD PUT SOMETHING LIKE THIS IN owafile.c
*/
static void std_print(char *str)
{
#ifdef MODOWA_WINDOWS
    HANDLE  fp;
    char   *send;
    char   *sptr;
    char   *aptr;
    DWORD   nbytes;
    int     slen;

    if (!str) return;

    fp = GetStdHandle(STD_OUTPUT_HANDLE);

    sptr = str;
    slen = str_length(str);
    send = sptr + slen;
    for (aptr = sptr; aptr < send; ++aptr)
    {
        if (*aptr == '\n')
        {
            nbytes = (DWORD)(aptr - sptr);
            if (nbytes > 0)
                if (!WriteFile(fp, sptr, nbytes, &nbytes, NULL)) break;
            sptr += nbytes;
            if (*sptr == '\n')
            {
                if (!WriteFile(fp, "\r\n", 2, &nbytes, NULL)) break;
                ++sptr;
            }
            ++slen;
        }
    }
    nbytes = (DWORD)(aptr - sptr);
    if (nbytes > 0) WriteFile(fp, sptr, nbytes, &nbytes, NULL);
#else
    if (str) printf("%s", str);
#endif
}

static un_long get_thread_id()
{
#ifdef MODOWA_WINDOWS
    un_long tid = (un_long)GetCurrentThreadId();
#else
    un_long tid = pthread_self();
#endif
    return(tid);
}

static void terminate_program(daemon_context *pdctx)
{
    int          i;
    owa_context *octx;

    if (pdctx->maintid != get_thread_id()) thread_exit();

    std_print("owad terminating\n");

    /*
    ** Termination processing
    */

    for (i = 0; i < MAX_THREADS; ++i)
        if (pdctx->tids[i] != os_nullfilehand)
        {
#ifdef MODOWA_WINDOWS
            thread_suspend(pdctx->tids[i]);
#else
            thread_cancel(pdctx->tids[i]);
#endif
        }

    for (octx = pdctx->loc_list; octx; octx = octx->next)
    {
        os_mutex_destroy(octx->mtctx->c_mutex);
        /* No need to destroy unused octx->mtctx->c_semaphore */
    }

    /* Detach shared memory and remove semaphore */
    if (pdctx->mapmem.map_ptr)
        os_shm_detach(pdctx->mapmem.map_ptr);
    if (!InvalidFile(pdctx->mapmem.f_mutex))
        os_sem_destroy(pdctx->mapmem.f_mutex);
    if (!InvalidFile(pdctx->mapmem.map_fp))
        os_shm_destroy(pdctx->mapmem.map_fp);

    os_mutex_destroy(pdctx->socklatch);

    os_exit(0);
}

#ifdef MODOWA_WINDOWS

BOOL WINAPI control_handler(DWORD ctrl_type)
{
    if (ctrl_type == CTRL_C_EVENT)
        terminate_program(&owad_context);
    return(0);
}

#else

/* ### ON SOME OSES, MIGHT NEED TO RETURN 0 ### */
static void control_handler(int signum)
{
    terminate_program(&owad_context);
}

#endif

/*
** Make a copy of a string value and return a pointer to it
*/
static char *dup_string(request_rec *r, char *s)
{
    char   *dup;
    size_t  slen;
    slen = str_length(s) + 1;
    dup = (char *)morq_alloc(r, slen, 0);
    if (dup) mem_copy(dup, s, slen);
    return(dup);
}

/*
** Read stream to next line break, return pointer to line.
** Use the buffer if possible (up to 1K), otherwise allocate
** memory as necessary to hold the complete line.
*/
static char *read_next_line(request_rec *r, char *buffer)
{
    int   i;
    char *tbuf;
    char *outbuf;
    char *nbuf;
    int   osize;
    int   olen;

    outbuf = buffer;
    osize = HTTP_LINE_MAX;
    olen = 0;

    tbuf = r->buffer;
    while (1)
    {
      i = r->bufptr;
      while (i < r->buflen)
      {
        outbuf[olen++] = tbuf[i++];
        if (olen >= 2)
        {
          if ((outbuf[olen-1] == '\n') && (outbuf[olen-2] == '\r'))
          {
              r->bufptr = i;
              outbuf[olen-2] = '\0';
              return(outbuf);
          }
        }
        if (olen == osize)
        {
            osize += HTTP_LINE_MAX;
            nbuf = outbuf;
            outbuf = (char *)morq_alloc(r, (size_t)osize, 0);
            if (!outbuf) return((char *)0);
            mem_copy(outbuf, nbuf, olen);
        }
      }
      r->bufptr = i;

      if (r->sock_end) break;
      i = socket_read(r->sock, r->buffer, sizeof(r->buffer));
      if (i <= 0)
      {
          r->sock_end = -1;
          break;
      }
      r->buflen = i;
      r->bufptr = 0;
    }
    outbuf[olen] = '\0';
    return(outbuf);
}

/*
** Read an HTTP request from the specified socket.
** First, read the header up to the first double-linebreak.
** Parse it, then, if a content-length is specified, use it
** to read in the rest of the data.
**
**   <GET or POST><space><URI><space><HTTP/1.x>\r\n
**   <name>:<space><value>\r\n
**   ...
**   \r\n
**   <data if any>
*/
static int read_http_request(request_rec *r, daemon_context *pdctx)
{
    char         buffer[HTTP_LINE_MAX];
    char        *aptr;
    char        *sptr;
    char        *mptr;
    int          i;
    long         clen;
    owa_context *octx;

    r->contlen = clen = (long)0;

    sptr = read_next_line(r, buffer);
    if (!sptr) return(-1);
    if (*sptr == '\0') return(-1);

    while (*sptr == ' ') ++sptr;
    aptr = sptr;
    while (*sptr != ' ') ++sptr;
    *(sptr++) = '\0';

    r->rtype = -1; /* ### UNSUPPORTED TYPE ### */
    for (i = 0; r->rtype < 0; ++i)
    {
        mptr = util_get_method(i);
        if (*mptr == '\0') break;
        if (!str_compare(aptr, mptr, -1, 1)) r->rtype = i;
    }

    while (*sptr == ' ') ++sptr;
    aptr = sptr;
    while (*sptr != ' ') ++sptr;
    *(sptr++) = '\0';

    r->uri = dup_string(r, aptr);
    if (!r->uri) return(-1);

    while (*sptr == ' ') ++sptr;
    aptr = sptr;
    while (*sptr != '\0') ++sptr;
    sptr = dup_string(r, aptr);
    if (!sptr) return(-1);
    morq_table_put(r, OWA_TABLE_SUBPROC, 0, "SERVER_PROTOCOL", sptr);

    morq_table_put(r, OWA_TABLE_SUBPROC, 0, "SERVER_ADDR", pdctx->ipaddr);

    aptr = str_char(r->uri, '?', 0);
    if (aptr)
    {
        *(aptr++) = '\0';
        r->args = aptr;
        morq_table_put(r, OWA_TABLE_SUBPROC, 0, "QUERY_STRING", r->args);
    }
    morq_table_put(r, OWA_TABLE_SUBPROC, 0, "REQUEST_URI", r->uri);

    for (octx = pdctx->loc_list; octx; octx = octx->next)
    {
        i = str_length(octx->location);
        if ((!str_compare(r->uri, octx->location, i, 1)) &&
            ((r->uri[i] == '/') || (r->uri[i] == '\0')))
        {
            r->octx = octx;
            r->script = octx->location;
            morq_table_put(r, OWA_TABLE_SUBPROC, 0, "SCRIPT_NAME", r->script);
            morq_table_put(r, OWA_TABLE_SUBPROC, 0, "PATH_INFO", r->uri + i);
            break;
        }
    }

    if (!(r->octx))
    {
        aptr = str_char(r->uri, '/', 1);
        if (!aptr) aptr = r->uri;
        morq_table_put(r, OWA_TABLE_SUBPROC, 0, "PATH_INFO", aptr);
        if (aptr > r->uri)
        {
            sptr = dup_string(r, r->uri);
            sptr[aptr - r->uri] = '\0';
            morq_table_put(r, OWA_TABLE_SUBPROC, 0, "SCRIPT_NAME", sptr);
            r->script = sptr;
        }
        else
            r->script = "";
    }

    if (r->rtype < 0)
        std_print("Invalid HTTP request\n");
    else
    {
        std_print("HTTP Request [");
        std_print(util_get_method(r->rtype));
        std_print("] for URI [");
        std_print(r->uri);
        std_print("]\n");
    }

    while (1)
    {
        sptr = read_next_line(r, buffer);
        if (!sptr) return(-1);
        if (*sptr == '\0') break;
        aptr = sptr;
        while ((*sptr != ':') && (*sptr != '\0')) ++sptr;
        if (*sptr == '\0') return(-1);
        *(sptr++) = '\0';
        while (*sptr == ' ') ++sptr;
        sptr = dup_string(r, sptr);
        if (!sptr) return(-1);
        if (!str_compare(aptr, "Content-Length", -1, 1))
        {
            morq_table_put(r, OWA_TABLE_SUBPROC, 0, "CONTENT_LENGTH", sptr);
            r->contlen = clen = (long)str_atoi(sptr);
        }
        else if (!str_compare(aptr, "Content-Type", -1, 1))
            morq_table_put(r, OWA_TABLE_SUBPROC, 0,
                           "CONTENT_TYPE", sptr);
        else if (!str_compare(aptr, "User-Agent", -1, 1))
            morq_table_put(r, OWA_TABLE_SUBPROC, 0,
                           "HTTP_USER_AGENT", sptr);
        else if (!str_compare(aptr, "Accept", -1, 1))
            morq_table_put(r, OWA_TABLE_SUBPROC, 0,
                           "HTTP_ACCEPT", sptr);
        else if (!str_compare(aptr, "Accept-Charset", -1, 1))
            morq_table_put(r, OWA_TABLE_SUBPROC, 0,
                           "HTTP_ACCEPT_CHARSET", sptr);
        else if (!str_compare(aptr, "Accept-Encoding", -1, 1))
            morq_table_put(r, OWA_TABLE_SUBPROC, 0,
                           "HTTP_ACCEPT_ENCODING", sptr);
        else if (!str_compare(aptr, "Accept-Language", -1, 1))
            morq_table_put(r, OWA_TABLE_SUBPROC, 0,
                           "HTTP_ACCEPT_LANGUAGE", sptr);
        else if (!str_compare(aptr, "Connection", -1, 1))
            morq_table_put(r, OWA_TABLE_SUBPROC, 0,
                           "HTTP_CONNECTION", sptr);
        else if (!str_compare(aptr, "Authorization", -1, 1))
            morq_table_put(r, OWA_TABLE_SUBPROC, 0,
                           "HTTP_AUTHORIZATION", sptr);
        else if (!str_compare(aptr, "Referer", -1, 1))
            morq_table_put(r, OWA_TABLE_SUBPROC, 0,
                           "HTTP_REFERER", sptr);
        else if (!str_compare(aptr, "Cookie", -1, 1))
            morq_table_put(r, OWA_TABLE_SUBPROC, 0,
                           "HTTP_COOKIE", sptr);
        /*
        ** ### Hacks for DAV headers
        */
        else if (!str_compare(aptr, "Depth", -1, 1))
            morq_table_put(r, OWA_TABLE_SUBPROC, 0,
                           "HTTP_DAV_DEPTH", sptr);
        else if (!str_compare(aptr, "Overwrite", -1, 1))
            morq_table_put(r, OWA_TABLE_SUBPROC, 0,
                           "HTTP_DAV_DEPTH", sptr);
        else if (!str_compare(aptr, "Lock-Token", -1, 1))
            morq_table_put(r, OWA_TABLE_SUBPROC, 0,
                           "HTTP_DAV_LOCKTOKEN", sptr);
        else if (!str_compare(aptr, "Host", -1, 1))
        {
            aptr = str_char(sptr, ':', 0);
            if (aptr)
            {
                *(aptr++) = '\0';
                morq_table_put(r, OWA_TABLE_SUBPROC, 0, "SERVER_PORT", aptr);
            }
            morq_table_put(r, OWA_TABLE_SUBPROC, 0, "SERVER_NAME", sptr);
        }
        else
        {
            std_print(aptr);
            std_print(": ");
            std_print(sptr);
            std_print("\n");
        }
    }

    return(0);
}

static int handle_file(daemon_context *pdctx, request_rec *r, char *uri)
{
    os_objhand  fp;
    char        pmimetype[256];
    char        fpath[1024];
    char        outbuf[HTBUF_BLOCK_SIZE];
    un_long     fage, clen, ctot;
    un_long     nbytes;
    int         i;

    i = str_concat(fpath, 0, pdctx->froot, sizeof(fpath)-1);
    str_concat(fpath, i, uri, sizeof(fpath)-1);

    fp = file_open_read(fpath, &clen, &fage);
    if (InvalidFile(fp)) goto ff_err;

    /*
    ** Assume the mime-type can be determined from the file extension
    */
    *pmimetype = '\0';
    util_set_mime(fpath, pmimetype, 1);

    /*
    ** Set header Status and Content-Type
    */
    morq_set_mimetype(r, pmimetype);

    morq_set_length(r, (size_t)clen, 0);

    /*
    ** Send the header
    */
    morq_send_header(r);

    for (ctot = clen; ctot > 0; ctot -= nbytes)
    {
        nbytes = sizeof(outbuf);
        if (nbytes > ctot) nbytes = ctot;
        i = file_read_data(fp, outbuf, (int)nbytes);
        if (i != (int)nbytes) goto ff_err;
        morq_write(r, outbuf, (int)nbytes);
    }

ff_err:
    file_close(fp);
    return(OK);
}

#ifdef NEVER
static void read_special_request(os_socket sock)
{
    int   opcode;
    int   numvals;
    char  buffer[65536];
    int   n, i, j, k;
    char *sptr;

    n = socket_read(sock, buffer, 1);
    opcode = (buffer[0] & 0xFF);
    opcode &= 0x7F; /* ### IGNORE KEEPALIVE REQUEST FLAG FOR NOW ### */
    if (opcode == 0)
    {
        /* Request to close the socket */
        buffer[0] = 0;
        n = socket_write(sock, buffer, 1);
    }

    n = socket_read(sock, buffer, 2);
    numvals = ((buffer[0] & 0xFF) << 8) | (buffer[1] & 0xFF);

    for (i = 0; i < numvals; ++i)
    {
        n = socket_read(sock, buffer, 2);
        j = ((buffer[0] & 0xFF) << 8) | (buffer[1] & 0xFF);
        for (k = 0; k < j; k += n)
        {
            n = socket_read(sock, buffer + k, j - k);
            if (n <= 0) break;
        }
        buffer[k] = '\0';
        std_print(buffer);
        std_print(" = ");
        n = socket_read(sock, buffer, 2);
        j = ((buffer[0] & 0xFF) << 8) | (buffer[1] & 0xFF);
        for (k = 0; k < j; k += n)
        {
            n = socket_read(sock, buffer + k, j - k);
            if (n <= 0) break;
        }
        buffer[k] = '\0';
        std_print(buffer);
        std_print("\n");
    }
    n = socket_read(sock, buffer, 4);
    j = 0;
    for (i = 0; i < 4; ++i)
    {
        j <<= 8;
        j |= (buffer[i] & 0xFF);
    }
    while (j > 0)
    {
        k = sizeof(buffer) - 1;
        if (k > j) k = j;
        n = socket_read(sock, buffer, k);
        if (n <= 0) break;
        j -= n;
        buffer[n] = '\0';
        std_print(buffer);
    }

    /* ### SEND A RESPONSE ### */
    buffer[0] = 0; /* No keepalive */
    buffer[1] = 0;
    buffer[2] = 1; /* 1 header element */
    n = socket_write(sock, buffer, 3);

    sptr = "Content-Type";
    buffer[1] = str_length(sptr);
    for (i = 2; *sptr; ++sptr) buffer[i++] = *sptr;
    n = socket_write(sock, buffer, i);
    sptr = "text/html";
    buffer[1] = str_length(sptr);
    for (i = 2; *sptr; ++sptr) buffer[i++] = *sptr;
    n = socket_write(sock, buffer, i);

    sptr = "<html><body bgcolor=\"#ffffff\">\n"
           "<p>Got HTTP request</p>\n</body></html>\n";
    i = str_length(sptr);
    buffer[0] = 0; buffer[1] = 0; buffer[2] = 0; buffer[3] = i;
    socket_write(sock, buffer, 4);
    socket_write(sock, sptr, i);
}
#endif

/*****************************************************************************
 * MODOWA LINKAGE                                                            *
 *****************************************************************************/

int mowa_check_keepalive(int context_flag)
{
    return(1); /* OWAD never uses keepalive */
}

void mowa_acquire_mutex(owa_context *octx)
{
    if (!InvalidMutex(octx->mtctx->c_mutex))
        os_mutex_acquire(octx->mtctx->c_mutex, SHMEM_WAIT_INFINITE);
}

void mowa_release_mutex(owa_context *octx)
{
    if (!InvalidMutex(octx->mtctx->c_mutex))
        os_mutex_release(octx->mtctx->c_mutex);
}

void mowa_semaphore_create(owa_context *octx)
{
    /* Not needed because pool size == nthreads */
}

int mowa_semaphore_get(owa_context *octx)
{
    return(1); /* pool size == nthreads */
}

void mowa_semaphore_put(owa_context *octx)
{
    /* Not needed because pool size == nthreads */
}

void morq_create_mutex(request_rec *request, owa_context *octx)
{
    /* Nothing to do - mutex created during startup */
}

char *morq_get_buffer(request_rec *request, int sz)
{
    return((char *)0);
}

void *morq_alloc(request_rec *request, size_t sz, int zero_flag)
{
    mem_block *mptr;
    mptr = (zero_flag) ? mem_zalloc(sizeof(*mptr) + sz) :
                         mem_alloc(sizeof(*mptr) + sz);
    if (mptr)
    {
        mptr->next = request->mlist;
        request->mlist = mptr;
        mptr->data = (void *)(mptr + 1);
        return(mptr->data);
    }
    return((void *)0);
}

int morq_regex(request_rec *request, char *pattern, char **str, int caseflag)
{
#ifdef REGEX_SUPPORTED
    regex_t    rules;
    regmatch_t match;
    int        result;
    int        flags;

    flags = REG_EXTENDED;
    if (caseflag) flags |= REG_ICASE;

    if (regcomp(&rules, pattern, flags)) return(0);
    result = regexec(&rules, *str, 1, &match, 0);
    regfree(&rules);
    if (result) return(0);
    *str += match.rm_so;
    return(match.rm_eo - match.rm_so);
#else
    return(0);
#endif
}

void morq_send_header(request_rec *request)
{
    char  headline[HTTP_LINE_MAX];
    char *sptr;
    int   i, n;

    os_str_print(headline, "Response status %d\n", request->status);
    std_print(headline);
    switch (request->status)
    {
    case HTTP_OK:                    sptr = "OK";                    break;
    case HTTP_CONTINUE:              sptr = "Continue";              break;
    case HTTP_NO_CONTENT:            sptr = "No Content";            break;
    case HTTP_MOVED_TEMPORARILY:     sptr = "Moved Temporarily";     break;
    case HTTP_MOVED_PERMANENTLY:     sptr = "Moved Permanently";     break;
    case HTTP_TEMPORARY_REDIRECT:    sptr = "Temporary Redirect";    break;
    case HTTP_NOT_MODIFIED:          sptr = "Not Modified";          break;
    case HTTP_BAD_REQUEST:           sptr = "Bad Request";           break;
    case HTTP_UNAUTHORIZED:          sptr = "Unauthorized";          break;
    case HTTP_FORBIDDEN:             sptr = "Forbidden";             break;
    case HTTP_NOT_FOUND:             sptr = "Not Found";             break;
    case HTTP_REQUEST_TIME_OUT:      sptr = "Request Timeout";       break;
    case HTTP_INTERNAL_SERVER_ERROR: sptr = "Internal Server Error"; break;
    case HTTP_NOT_IMPLEMENTED:       sptr = "Not Implemented";       break;
    case HTTP_SERVICE_UNAVAILABLE:   sptr = "Service Unavailable";   break;
    case HTTP_GATEWAY_TIME_OUT:      sptr = "Gateway Timeout";       break;
    default:
      /* ### Instead of "OK", should use HTTP code; list is incomplete ### */
      sptr = "OK";
      break;
    }
    if (request->status_line) sptr = request->status_line;
    os_str_print(headline, "HTTP/1.0 %d %s\r\n", request->status, sptr);
    n = socket_write(request->sock, headline, str_length(headline));
    str_copy(headline, "Server: owad\r\n");
    n = socket_write(request->sock, headline, str_length(headline));
#ifdef NEVER
    {
      /* ### Should write "Date: Mon, 09 Oct 2000 16:20:00 GMT" ### */
      char timebuf[HTTP_LINE_MAX];
      util_header_time(os_get_component_time(1), timebuf, "GMT");
      os_str_print(headline, "Date: %s\r\n", timebuf);
    }
#endif
    if (request->content_type)
    {
        os_str_print(headline, "Content-Type: %s\r\n", request->content_type);
        n = socket_write(request->sock, headline, str_length(headline));
    }
    if (request->clength >= 0)
    {
        os_str_print(headline, "Content-Length: %d\r\n", (int)request->clength);
        n = socket_write(request->sock, headline, str_length(headline));
    }
    for (i = 0; i < request->nheadout; ++i)
    {
        sptr = request->headout[i].name;
        n = socket_write(request->sock, sptr, str_length(sptr));
        n = socket_write(request->sock, ": ", 2);
        sptr = request->headout[i].value;
        n = socket_write(request->sock, sptr, str_length(sptr));
        n = socket_write(request->sock, "\r\n", 2);
    }

    n = socket_write(request->sock, "\r\n", 2);
}

void morq_set_status(request_rec *request, int status, char *status_line)
{
    request->status = status;
    request->status_line = status_line;
}

void morq_set_mimetype(request_rec *request, char *mtype)
{
    int len;
    int has_controls = 0;

    for (len = 0; mtype[len]; ++len)
        if ((mtype[len] & 0xFF) < ' ')
            has_controls = 1;

    if (has_controls)
    {
        char *sptr = morq_alloc(request, (size_t)(len + 1), 1);
        for (len = 0; mtype[len]; ++len)
            sptr[len] = ((mtype[len] & 0xFF) < ' ') ? ' ' : mtype[len];
        mtype = sptr;
    }

    request->content_type = mtype;
}

void morq_set_length(request_rec *request, size_t len, int range_flag)
{
  request->clength = (long_64)len;
}

int morq_check_range(request_rec *request)
{
    return(0); /* ### NOT IMPLEMENTED ### */
}

int morq_get_range(request_rec *request, long_64 *off, long_64 *len)
{
    return(0); /* ### RANGE TRANSFERS NOT SUPPORTED ### */
}

char *morq_getword(request_rec *request,
                   const char **args, char ch, int un_escape)
{
    char *aptr;
    char *sptr;
    char  hex[3];
    int   i, j;
    int   slen;

    aptr = (char *)(*args);
    sptr = str_char(aptr, ch, 0);
    if (sptr)
    {
        slen = (int)(sptr - aptr);
        *args = sptr + 1;
    }
    else
    {
        slen = str_length(aptr);
        *args = aptr + slen;
    }
    sptr = morq_alloc(request, slen + 1, 0);
    if (!sptr) return("");
    if (un_escape)
    {
        hex[2] = '\0';
        i = j = 0;
        while (i < slen)
        {
            if (aptr[i] == '+')
                sptr[j++] = ' ';
            else if (aptr[i] == '%')
            {
                hex[0] = aptr[++i];
                hex[1] = aptr[++i];
                sptr[j++] = (char)(str_atox(hex) & 0xFF);
            }
            else
                sptr[j++] = aptr[i];
            ++i;
        }
        sptr[j] = '\0';
    }
    else
    {
        str_concat(sptr, 0, aptr, slen);
        return(sptr);
    }
    return(sptr);
}

int morq_stream(request_rec *request, int flush_flag)
{
    char buffer[4096];
    if (flush_flag) /* ### NEEDS TO FLUSH THE INPUT - WILL THIS WORK? ### */
      while (!(request->sock_end))
        morq_read(request, buffer, (long)sizeof(buffer));
    return(0);
}

long morq_read(request_rec *request, char *buffer, long buflen)
{
    long j;
    int  i;

    if ((buflen == 0) || (!buffer)) return((long)0);
    j = 0;
    i = request->bufptr;
    while (i < request->buflen)
    {
        buffer[j++] = request->buffer[i++];
        if (j == buflen)
        {
            request->bufptr = i;
            request->contlen -= j;
            return(j);
        }
    }
    request->bufptr = i;
    request->contlen -= j;
    if (j > 0) return(j);
    if (request->contlen == 0) request->sock_end = 1;
    if (request->sock_end) return(0);
    if (socket_test(&(request->sock), 1, MAX_BLOCK))
    {
        j = (buflen > request->contlen) ? request->contlen : buflen;
        j = (long)socket_read(request->sock, buffer, (int)j);
        if (j > 0) request->contlen -= j;
    }
    if (j <= 0) request->sock_end = -1;
    return(j);
}

long morq_write(request_rec *request, char *buffer, long buflen)
{
    if (!buffer) return(0); /* ### SHOULD FLUSH SOCKET SOMEHOW ### */
    if (buflen < 0) buflen = str_length(buffer);
    return((long)socket_write(request->sock, buffer, buflen));
}

void morq_print_int(request_rec *request, char *fmt, long ival)
{
    int   slen;
    char  tbuf[HTTP_LINE_MAX];
    char *tstr;
    slen = str_length(fmt) + LONG_MAXSTRLEN;
    if (slen <= sizeof(tbuf))
        tstr = tbuf;
    else
        tstr = (char *)morq_alloc(request, slen, 0);
    if (tstr)
        os_str_print(tstr, fmt, ival);
    morq_write(request, tstr, -1);
}

void morq_print_str(request_rec *request, char *fmt, char *sptr)
{
    int   slen;
    char  tbuf[HTTP_LINE_MAX];
    char *tstr;
    slen = str_length(fmt) + str_length(sptr);
    if (slen <= sizeof(tbuf))
        tstr = tbuf;
    else
        tstr = (char *)morq_alloc(request, slen, 0);
    if (tstr)
        os_str_print(tstr, fmt, sptr);
    morq_write(request, tstr, -1);
}

int  morq_table_get(request_rec *request, int tableid, int index,
                    char **name, char **value)
{
    if (request->nheadin > index)
    {
        *name = request->headin[index].name;
        *value = request->headin[index].value;
        if (tableid == OWA_TABLE_HEADIN)
        {
            /* ### HACK -- THE ONLY VALUE REQUIRED IS "COOKIE" ### */
            if (!str_compare(*name, "HTTP_COOKIE", -1, 1)) *name = "Cookie";
        }
        return(1);
    }
    return(0);
}

void morq_table_put(request_rec *request, int tableid, int set_flag,
                    char *name, char *value)
{
    int         i;
    int         nhead;
    header_rec *htab;

    if (tableid == OWA_TABLE_SUBPROC)
    {
        nhead = request->nheadin;
        htab = request->headin;
    }
    else
    {
        nhead = request->nheadout;
        htab = request->headout;
    }

    if (set_flag)
    {
        for (i = 0; i < nhead; ++i)
          if (!str_compare(htab[i].name, name, -1, 1))
          {
              htab[i].name = name;
              htab[i].value = value;
              return;
          }
    }

    i = nhead;
    if (i < MAX_HEADERS)
    {
        htab[i].name = name;
        htab[i].value = value;
        ++i;
    }

    if (tableid == OWA_TABLE_SUBPROC)
        request->nheadin = i;
    else
        request->nheadout = i;
}

char *morq_get_env(request_rec *request, char *name)
{
    int i;
    for (i = 0; i < request->nheadin; ++i)
        if (!str_compare(request->headin[i].name, name, -1, 1))
            return(request->headin[i].value);
    return((char *)0);
}

char *morq_get_header(request_rec *request, char *name)
{
    char *nptr;
    /*
    ** ### This implementation is a hack.  It converts the header
    ** ### requests into environment requests, then gets the results
    ** ### from the environment table.  This avoids the need to maintain
    ** ### the header elements in a separate table.  Some header elements
    ** ### had to be forced into the environment (e.g. the DAV ones)
    ** ### to support this.  Only a limited list of header elements
    ** ### is hard-coded in here.
    */
    if (!str_compare(name, "Authorization", -1, 1))
        nptr = "HTTP_AUTHORIZATION";
    else if (!str_compare(name, "Depth", -1, 1))
        nptr = "HTTP_DAV_DEPTH";
    else if (!str_compare(name, "Overwrite", -1, 1))
        nptr = "HTTP_DAV_DEPTH";
    else if (!str_compare(name, "Lock-Token", -1, 1))
        nptr = "HTTP_DAV_LOCKTOKEN";
    else
        return((char *)0);

    return(morq_get_env(request, nptr));
}

char *morq_parse_auth(request_rec *request, char *buffer)
{
    char *sptr;
    char *aptr;
    int   len;

    len = str_length(buffer);
    sptr = (char *)morq_alloc(request, len + 4, 0);
    if (!sptr) return("");
    mem_copy(sptr, buffer, len + 1);
    aptr = str_char(buffer, ' ', 0);
    if (aptr)
    {
        ++aptr;
        util_decode64(aptr, sptr + (aptr - buffer));
    }
    return(sptr);
}

void morq_add_context(request_rec *request, owa_context *octx)
{
    /* ### NOTHING TO DO -- DONE AT SERVER START-UP ### */
}

void morq_close_exec(request_rec *request, owa_context *octx, int first_flag)
{
    /* ### NOTHING TO DO - DONE IMMEDIATELY AFTER accept() ### */
}

void morq_sql_error(request_rec *request,
                    int code, char *errbuf, char *lastsql)
{
    /* ### NOTHING TO DO -- NO LOG FILE ### */
}


/*****************************************************************************
 * OWAD                                                                      *
 *****************************************************************************/

/*
** Main for worker thread
*/
static void main_server(void *ctx)
{
    daemon_context *pdctx;
    mem_block      *mptr;
    os_socket       asock;
    char            tid[32];
    request_rec     rr;
    int             result;
    char           *sptr;
    owa_request     owa_req;
    int             hard_close;
    char            client_ip[64];

    str_itox(get_thread_id(), tid);

    pdctx = (daemon_context *)ctx;
    while (1)
    {
        *client_ip = '\0';
        os_mutex_acquire(pdctx->socklatch, -1);
        asock = socket_accept(pdctx->sock, client_ip, sizeof(client_ip));
        os_mutex_release(pdctx->socklatch);
        thread_check();
        if (asock == os_badsocket)
            hard_close = 1;
        else
        {
#ifndef MODOWA_WINDOWS
            os_fd_close_exec(asock);
#endif
            hard_close = 0;

            /* ### DEBUG PRINT ### */
            std_print("Accepted socket");
            if (*client_ip)
            {
              std_print(" from client ");
              std_print(client_ip);
            }
            std_print(" in thread ");
            std_print(tid);
            std_print("\n");
        }

        while (!hard_close)
        {
            mem_zero(&rr, sizeof(rr));
            rr.sock = asock;
            rr.status = HTTP_OK;
            rr.clength = -1; /* Content length unknown */
            if (*client_ip)
                morq_table_put(&rr, OWA_TABLE_SUBPROC, 0,
                               "REMOTE_ADDR", client_ip);
            if (read_http_request(&rr, pdctx) >= 0)
            {
                mem_zero(&owa_req, sizeof(owa_req));

                /*
                ** ### SHOULD REALLY SEARCH FOR MATCHING LOCATION ON LINKED
                ** ### LIST.  ALSO, SHOULD DIRECT NON-MATCHING REQUESTS TO
                ** ### SPECIAL FILE-SYSTEM HANDLER.
                */
                if (rr.octx)
                {
                    result = owa_handle_request(rr.octx, &rr, rr.args,
                                                rr.rtype, &owa_req);
                }
                else
                    result = handle_file(pdctx, &rr, rr.uri);

                if (result != OK)
                {
                    /* ### NEED TO SEND AUTO-RESPONSE ### */
                    rr.status = result;
                    rr.content_type = "text/html";
                    rr.clength = -1;
                    morq_send_header(&rr);
                    sptr = "<html><body bgcolor=\"#ffffff\" text=\"cc0000\">\n"
                           "<p>owad error page</p>\n</body></html>\n";
                    morq_write(&rr, sptr, -1);
                }

                /* Flush socket writes to client */
                socket_flush(asock);
            }
            if (rr.sock_end < 0) hard_close = 1;

            /* Free request memory */
            while (rr.mlist)
            {
                mptr = rr.mlist;
                rr.mlist = mptr->next;
                mem_free((void *)mptr);
            }

            /* See if another request is available on the socket */
            if (!hard_close)
              if (!socket_test(&asock, 1, WAIT_SOCKET))
                hard_close = 1;

            /* Otherwise close the socket */
            if (hard_close) socket_close(asock);
        }
        thread_check();
    }
    thread_exit();
}

#ifdef NEVER
/*
** ### Test program: send and receive back simple message
*/
static int main_client(int port, char *ipaddr)
{
    int        n;
    char       buffer[4096];
    char      *sptr;
    os_socket  asock;

    asock = socket_connect(port, ipaddr);
    if (asock != os_badsocket)
    {
        sptr = "GET /index.html HTTP/1.0\r\n\r\n";
        n = socket_write(asock, sptr, str_length(sptr));
        n = socket_read(asock, buffer, sizeof(buffer)-1);
        buffer[n] = 0;
        std_print(buffer);
        socket_close(asock);
    }
    return(0);
}
#endif

static long_64 str_to_mem(char *sptr)
{
    long_64 sz = 0;
    while ((*sptr >= '0') && (*sptr <= '9'))
        sz = sz * 10 + (long_64)(*(sptr++) - '0');
    if ((*sptr == 'k') || (*sptr == 'K')) sz *= 1024;
    if ((*sptr == 'm') || (*sptr == 'M')) sz *= 1024*1024;
    if ((*sptr == 'g') || (*sptr == 'G')) sz *= 1024*1024*1024;
    return(sz);
}

static int str_to_tim(char *sptr)
{
    int tm = 0;
    while ((*sptr >= '0') && (*sptr <= '9'))
        tm = tm * 10 + (*(sptr++) - '0');
    while (*sptr == ' ') ++sptr;
    if ((*sptr == 'd') || (*sptr == 'D')) tm *= (60*60*24);
    if ((*sptr == 'h') || (*sptr == 'H')) tm *= (60*60);
    if ((*sptr == 'm') || (*sptr == 'M')) tm *= 60;
    return(tm);
}

static void mowa_diag(owa_context *octx, char *dstr)
{
    int diagflag;

    /*
    ** Parse and set the diagnostic flags
    */
    if (dstr)
    {
        diagflag = 0;
        if (str_substr(dstr, "COMMAND",  1)) diagflag |= DIAG_COMMAND;
        if (str_substr(dstr, "ARGS",     1)) diagflag |= DIAG_ARGS;
        if (str_substr(dstr, "CGIENV",   1)) diagflag |= DIAG_CGIENV;
        if (str_substr(dstr, "POOL",     1)) diagflag |= DIAG_POOL;
        if (str_substr(dstr, "HEADER",   1)) diagflag |= DIAG_HEADER;
        if (str_substr(dstr, "COOKIES",  1)) diagflag |= DIAG_COOKIES;
        if (str_substr(dstr, "CONTENT",  1)) diagflag |= DIAG_CONTENT;
        if (str_substr(dstr, "RESPONSE", 1)) diagflag |= DIAG_RESPONSE;
        if (str_substr(dstr, "TIMING",   1)) diagflag |= DIAG_TIMING;
        if (str_substr(dstr, "SQL",      1)) diagflag |= DIAG_SQL;
        if (str_substr(dstr, "MEMORY",   1)) diagflag |= DIAG_MEMORY;
        if (str_substr(dstr, "THREADS",  1)) diagflag |= DIAG_THREADS;
        octx->diagflag |= diagflag;
    }
}

static void mowa_desc(owa_context *octx, char *dstr, char *schema)
{
    /*
    ** Parse and set the describe mode and schema
    */
    if (dstr)
    {
        if (!str_compare(dstr, "STRICT", -1, 1))
            octx->descmode = DESC_MODE_STRICT;
        else if (!str_compare(dstr, "NORMAL", -1, 1))
            octx->descmode = DESC_MODE_NORMAL;
        else if (!str_compare(dstr, "RELAXED", -1, 1))
            octx->descmode = DESC_MODE_RELAXED;
    }
    if (schema) octx->desc_schema = schema;
}

static void mowa_alt(owa_context *octx, char *astr)
{
    /*
    ** Set up alternate OWA package name and optional flags
    */
    if (astr)
    {
        if (octx->alternate == (char *)0)
            octx->alternate = astr;
        else if (str_substr(astr, "KEEPSTATE",  1))
            octx->altflags |= ALT_MODE_KEEP;
        else if (str_substr(astr, "WITHRAW",  1))
            octx->altflags |= ALT_MODE_RAW;
        else if (str_substr(astr, "USELOBS",  1))
            octx->altflags |= ALT_MODE_LOBS;
        else if (str_substr(astr, "CACHE", 1))
            octx->altflags |= ALT_MODE_CACHE;
        else if (str_substr(astr, "SETSEC", 1))
            octx->altflags |= ALT_MODE_SETSEC;
        else if (str_substr(astr, "GETRAW",  1))
            octx->altflags |= ALT_MODE_GETRAW;
        else if (str_substr(astr, "NOMERGE",  1))
            octx->altflags |= ALT_MODE_NOMERGE;
        else if (str_substr(astr, "LOGGING",  1))
            octx->altflags |= ALT_MODE_LOGGING;
        else if (str_substr(astr, "CGITIME",  1))
            octx->altflags |= ALT_MODE_CGITIME;
        else if (str_substr(astr, "CGIPOST",  1))
            octx->altflags |= ALT_MODE_CGIPOST;
        else if (str_substr(astr, "IPADDR",  1))
            octx->altflags |= ALT_MODE_IPADDR;
    }
}

static void mowa_uni(owa_context *octx, char *astr)
{
    /*
    ** Set up NCHAR binding support
    */
    if (astr)
    {
        if (str_substr(astr, "USER",  1))
            octx->ncflag = UNI_MODE_USER;
        else if (str_substr(astr, "FULL",  1))
            octx->ncflag = UNI_MODE_FULL;
        else if (str_substr(astr, "RAW",  1))
            octx->ncflag = UNI_MODE_RAW;
    }
}

static void mowa_rset(owa_context *octx, char *astr)
{
    if (astr)
    {
        if (!str_compare(astr, "FULL",  -1, 1))
            octx->rsetflag = RSET_MODE_FULL;
        else if (!str_compare(astr, "LAZY",  -1, 1))
            octx->rsetflag = RSET_MODE_LAZY;
        else if (!str_compare(astr, "INIT",  -1, 1))
            octx->rsetflag = RSET_MODE_INIT;
    }
}

static void mowa_ver(owa_context *octx, char *vauth, char *pkg)
{
    /*
    ** Default is no authorization
    ** Older versions of OWA use OWA_INIT.AUTHORIZE
    ** Newer versions of OWA require OWA_CUSTOM.AUTHORIZE
    ** PACKAGE indicates a per-package authorization is required
    */

    if (vauth)
    {
        if (str_substr(vauth, "OWA_INIT", 1))
        {
            octx->version &= ~OWA_AUTH_CUSTOM;
            octx->version |= OWA_AUTH_INIT;
        }
        else if (str_substr(vauth, "OWA_CUSTOM", 1))
        {
            octx->version &= ~OWA_AUTH_INIT;
            octx->version |= OWA_AUTH_CUSTOM;
        }
        if (str_substr(vauth, "PACKAGE", 1))
            octx->version |= OWA_AUTH_PACKAGE;
    }

    if (pkg)
    {
        if (!str_compare(pkg, "PACKAGE", -1, 1))
            octx->version |= OWA_AUTH_PACKAGE;
    }
}

static const char *mowa_ctype(owa_context *octx,
                              char *ctype, char *ectype)
{
    if (ctype)
      octx->default_ctype = ctype;
    if (ectype)
      octx->error_ctype = ectype;

    return((char *)0);
}

static void mowa_dad(owa_context *octx, char *odad)
{
    int i;

    if (odad)
    {
        i = nls_csx_from_iana(odad);
        if (i > 0) octx->dad_csid = i;
    }
}

static void mowa_nls(owa_context *octx, char *onls)
{
    int   i;
    char *aptr;
    char *bptr;

    if (onls)
    {
        /*
        ** Check NLS parameter for
        ** "." separating character set and "_" separating territory
        */
        aptr = str_char(onls, '.', 0);
        bptr = str_char(onls, '_', 0);

        if (aptr)
        {
            /* "." found, so character set points into original string */
            if (bptr)
                if (aptr < bptr)
                    bptr = (char *)0;
            i = (int)(aptr - onls);
            octx->nls_cs = ++aptr;
        }
        else if (bptr)
        {
            /* "_" found, so need to parse the string */
            i = str_length(onls);
        }
        else
        {
            /* string is just the character set, no parse needed */
            octx->nls_cs = onls;
            i = 0;
        }

        if (i > 0)
        {
            ++i;
            aptr = (char *)mem_alloc(i);
            if (!aptr) return;
            mem_copy(aptr, onls, i);
            aptr[i - 1] = '\0';

            octx->nls_lang = aptr;
            if (bptr)
            {
                bptr = aptr + (bptr - onls);
                *(bptr++) = '\0';
                octx->nls_terr = bptr;
            }
        }
    }
}

static void mowa_upmx(owa_context *octx, char *umaxstr)
{
    /*
    ** Set the content upload maximum
    */
    octx->upmax = (umaxstr) ? str_to_mem(umaxstr) : 0;
}

static void mowa_pool(owa_context *octx, char *poolstr, int nthreads)
{
    /* Do nothing - poolsize is always == nthreads */
}

static const char *mowa_wait(owa_context *octx,
                             char *waitstr, char *abortstr)
{
    int wait_ms;
    char *sptr;

    if (waitstr)
    {
        wait_ms = 0;

        for (sptr = waitstr; ((*sptr >= '0') && (*sptr <= '9')); ++sptr)
            wait_ms = wait_ms * 10 + (*sptr - '0');

        octx->pool_wait_ms = wait_ms;
    }

    if (abortstr) octx->pool_wait_abort = 1;

    return((char *)0);
}

static void mowa_uid(owa_context *octx, char *uid)
{
    char *sptr;

    if (uid)
    {
        char *scramble = os_env_get("MODOWA_SCRAMBLE");
        if ((scramble) && (!str_char(uid, '/', 0) && !str_char(uid, '@', 0)))
        {
          int   klen = str_length(scramble);
          int   slen = str_length(uid);
          char *binstr = (char *)os_alloca(slen + 1);
          char *tempstr = (char *)mem_alloc(slen + 1);
          slen = str_xtob(uid, (void *)binstr);
          if (util_scramble(scramble, klen, binstr, slen, tempstr, 1))
            uid = tempstr + 4; /* Skip the salt */
        }
        octx->oracle_userid = uid;
        for (sptr = uid; *sptr != '\0'; ++sptr)
            if (*sptr == '@')
            {
                /* OK to send content-lengths */
                octx->keepalive_flag = OWA_KEEPALIVE_OK;
                break;
            }
    }
}

static void mowa_lobs(owa_context *octx, char *ltypes)
{
    int lobtypes = 0;

    if (ltypes)
    {
        if (str_substr(ltypes,"BIN",  1)) lobtypes = LOB_MODE_BIN;
        if (str_substr(ltypes,"CHAR", 1)) lobtypes = LOB_MODE_CHAR;
        if (str_substr(ltypes,"NCHAR",1)) lobtypes = LOB_MODE_NCHAR;
        if (str_substr(ltypes,"FILE", 1)) lobtypes = LOB_MODE_BFILE;
        if (lobtypes > octx->lobtypes) octx->lobtypes = lobtypes;

        if (str_substr(ltypes, "LONG_RETURN_LENGTH", 1))
            octx->lontypes = LONG_MODE_RETURN_LEN;
        else if (str_substr(ltypes, "LONG_FETCH_LENGTH", 1))
            octx->lontypes = LONG_MODE_FETCH_LEN;
    }
}

static void mowa_cache(owa_context *octx,
                       char *logname, char *physname, char *life)
{
    un_long  life_seconds;
    alias   *new_lifes;
    alias   *old_lifes;
    int      i, j, n;

    /*
    ** Table of aliases that direct modowa to read physical files
    ** by translating logical path names; the idea is to allow modowa
    ** to track accesses to physical files, and also to allow modowa
    ** to project files from the database out to a file-system cache.
    ** The table is of the form:
    **   logname  = <Logical (URL) name>
    **   physname = <physical (file system) name>
    **   lifespan = <number of seconds>
    ** The table is reverse-alphabetically ordered so that longer
    ** paths will appear first (thus ensuring a match against the
    ** deepest applicable mapping later).
    */

    old_lifes = octx->lifes;
    n = octx->nlifes;
    if ((n % CACHE_MAX_ALIASES) == 0)
    {
        /* Extend the table by reallocation */
        n += CACHE_MAX_ALIASES;
        new_lifes = (alias *)mem_alloc(n * sizeof(alias));
        if (!new_lifes) return;
        octx->lifes = new_lifes;
    }
    else
        new_lifes = old_lifes;

    life_seconds = 0;
    if (life)
      if (*life)
      {
          life_seconds = str_to_tim(life);
          if (life_seconds == 0) life_seconds = CACHE_DEF_LIFE;
      }

    n = (octx->nlifes++);

    for (i = 0; i < n; ++i)
    {
        /* if (s1 < s2), returns -1 */
        if (str_compare(old_lifes[i].logname, logname, -1, 1) < 0)
        {
            for (j = n; j > i; --j)
            {
                new_lifes[j].logname  = old_lifes[j - 1].logname;
                new_lifes[j].physname = old_lifes[j - 1].physname;
                new_lifes[j].lifespan = old_lifes[j - 1].lifespan;
            }
            break;
        }
        else if (new_lifes != old_lifes)
        {
            new_lifes[i].logname  = old_lifes[i].logname;
            new_lifes[i].physname = old_lifes[i].physname;
            new_lifes[i].lifespan = old_lifes[i].lifespan;
        }
    }
    new_lifes[i].logname  = logname;
    new_lifes[i].physname = physname;
    new_lifes[i].lifespan = life_seconds;
}

static void mowa_http(owa_context *octx, char *mode)
{
    if (mode)
    {
        if      (str_compare(mode, "NORMAL", -1, 1) == 0) octx->dav_mode = 0;
        else if (str_compare(mode, "REST", -1, 1) == 0)   octx->dav_mode = 1;
        else if (str_compare(mode, "DAV", -1, 1) == 0)    octx->dav_mode = 2;
    }
}

static void mowa_flex(owa_context *octx, char *flexname)
{
    char **new_flexes;
    char **old_flexes;
    int    i, n;

    if (flexname)
    {
        old_flexes = octx->flexprocs;
        n = octx->nflex;

        if ((n % CACHE_MAX_ALIASES) == 0)
        {
            i = (n + CACHE_MAX_ALIASES) * sizeof(*new_flexes);
            new_flexes = mem_alloc(i);
            if (!new_flexes) return;

            octx->flexprocs = new_flexes;
            for (i = 0; i < n; ++i) new_flexes[i] = old_flexes[i];
            old_flexes = new_flexes;
        }

        old_flexes[n++] = flexname;
        octx->nflex = n;
    }
}

static void mowa_reject(owa_context *octx, char *rname)
{
    char **new_rejs;
    char **old_rejs;
    int    i, n;

    if (rname)
    {
        old_rejs = octx->rejectprocs;
        n = octx->nreject;

        if ((n % CACHE_MAX_ALIASES) == 0)
        {
            i = (n + CACHE_MAX_ALIASES) * sizeof(*new_rejs);
            new_rejs = mem_alloc(i);
            if (!new_rejs) return;

            octx->rejectprocs = new_rejs;
            for (i = 0; i < n; ++i) new_rejs[i] = old_rejs[i];
            old_rejs = new_rejs;
        }

        old_rejs[n++] = rname;
        octx->nreject = n;
    }
}

static void mowa_env(owa_context *octx, char *ename, char *evalue)
{
    envstruct *new_envs;
    envstruct *old_envs;
    int      i, n;

    if ((ename) && (evalue))
    {
        old_envs = octx->envvars;
        n = octx->nenvs;
        if ((n % CACHE_MAX_ALIASES) == 0)
        {
            /* Extend the table by reallocation */
            i = (n + CACHE_MAX_ALIASES) * sizeof(*new_envs);
            new_envs = (envstruct *)mem_alloc(i);
            if (!new_envs) return;
            octx->envvars = new_envs;
            for (i = 0; i < n; ++i) new_envs[i] = old_envs[i];
            old_envs = new_envs;
        }

        old_envs[n].name = ename;
        old_envs[n].value = evalue;
        ++(octx->nenvs);
    }
}

static void mowa_crtl(owa_context *octx, char *ipsubnet, char *ipmask)
{
    un_long ip;

    if (ipsubnet)
    {
        ip = util_ipaddr(ipsubnet);
        octx->crtl_subnet = ip;
        octx->crtl_mask = ~((un_long)0);
    }
    if (ipmask)
    {
        ip = util_ipaddr(ipmask);
        octx->crtl_mask = ip;
    }
}

static const char *mowa_dbchsz(owa_context *octx, char *charsize)
{
    int mxchsz;
    if (charsize)
    {
        mxchsz = str_atoi(charsize);
        if (mxchsz > 1)
        {
            if (mxchsz > 4) mxchsz = 4;
            octx->db_charsize = mxchsz;
        }
    }
    return((char *)0);
}

static void mowa_round(owa_context *octx, char *scale_round, char *arr_round)
{
    if (scale_round)
      if (*scale_round)
        octx->scale_round = (un_long)str_atoi(scale_round);

    if (arr_round)
      if (*arr_round)
        octx->arr_round = (un_long)str_atoi(arr_round);
}

static const char *mowa_table(owa_context *octx,
                              char *table_name, char *content_column)
{
    octx->doc_table = table_name;
    octx->doc_column = content_column;

    return((char *)0);
}

static const char *mowa_refcur(owa_context *octx, char *aptr)
{
    if (aptr)
      if (*aptr == '\0')
        aptr = (char *)0;

    if (octx->ref_root_tag == (char *)0)
    {
        if (!aptr) aptr = ""; /* "collection" */
        octx->ref_root_tag = aptr;
    }
    else if (octx->ref_row_tag == (char *)0)
    {
        if (!aptr) aptr = "row";
        octx->ref_row_tag = aptr;
    }
    else if (octx->ref_prefix == (char *)0)
    {
        if (!aptr) aptr = "";
        octx->ref_prefix = aptr;
    }
    else if (octx->ref_nsuri == (char *)0)
    {
        octx->ref_nsuri = aptr;
    }

    return((char *)0);
}

static void mowa_mem(daemon_context *pdctx, char *astr)
{
    size_t sz;

    /*
    ** The context passed in is for the "default" ("null") location.
    ** We don't really want to put this information there, because
    ** it won't be easily accessible to all locations.  Instead,
    ** put this on the module configuration.
    */
    if ((pdctx) && (astr))
    {
        sz = (size_t)str_to_mem(astr);
        if (sz > 0)
        {
            pdctx->mapmem.mapsize = sz;
            pdctx->mapmem.memthresh = sz >> 4;
        }
    }
}

/*
** Create a location structure
*/
owa_context *create_location(char *dirspec, int nthreads)
{
    owa_context *octx;
    size_t       psize;
    size_t       asize;

    /*
    ** Create the location structure
    */
    psize = sql_conn_size(nthreads);
    asize = sizeof(owa_context) + sizeof(owa_mt_context) +
            psize + str_length(dirspec) + 1;
    octx = (owa_context *)mem_zalloc((int)asize);
    if (!octx) return(octx);

    octx->mtctx = (owa_mt_context *)(void *)(octx + 1);
    octx->c_pool = (connection *)(void *)(octx->mtctx + 1);
    octx->pool_wait_ms = MAX_WAIT;
    octx->poolsize = nthreads;
    octx->location = ((char *)(void *)octx->c_pool) + psize;

    octx->version = OWA_AUTH_NONE;
    octx->nls_cs = "";
    octx->nls_lang = "";
    octx->nls_terr = "";
    octx->nls_tz = "";
    octx->nls_datfmt = "";
    octx->lobtypes = -1;
    octx->shm_offset = -1;
    octx->verstring = modowa_version;
    octx->multithread = 1;
    octx->descmode = DESC_MODE_NORMAL;
    octx->crtl_mask = (un_long)0xFFFFFFFF;

    if (dirspec) str_copy(octx->location, dirspec);

    return(octx);
}

/*
** Look for next argument, parse it off and return it
*/
char *find_arg(char **pptr)
{
    char *aptr;
    char *sptr;
    char *qptr;
    int   alen;

    sptr = *pptr;
    while ((*sptr == ' ') || (*sptr == '\t')) ++sptr;
    if (*sptr == '"')
    {
        qptr = str_char(sptr + 1, '"', 0);
        if (!qptr)
        {
            alen = str_length(sptr);
            qptr = sptr + alen;
        }
        else
        {
            *(qptr++) = '\0';
            ++sptr;
        }
    }
    else
    {
        qptr = sptr;
        while (*qptr > ' ') ++qptr;
        if (*qptr != '\0') *(qptr++) = '\0';
    }

    *pptr = qptr;
    alen = str_length(sptr) + 1;
    aptr = (char *)mem_alloc(alen);
    if (aptr) mem_copy(aptr, sptr, alen);
    return(aptr);    
}

/*
** Read and parse file containing location directives for OWA
*/
void parse_locations(daemon_context *pdctx, char *fname, int nthreads)
{
    os_objhand   fp;
    un_long      fage, clen;
    owa_context *octx;
    char        *lptr;
    char        *eptr;
    char        *sptr;
    char        *arg1, *arg2, *arg3;
    char        *buffer;

    fp = file_open_read(fname, &clen, &fage);
    if (InvalidFile(fp)) return;

    buffer = (char *)mem_alloc(clen + 1);
    if (buffer)
    {
        file_read_data(fp, buffer, (int)clen);
        buffer[clen] = '\0';

        octx = (owa_context *)0;

        for (lptr = buffer; *lptr != '\0'; lptr = eptr)
        {
            eptr = str_char(lptr, '\n', 0);
            if (eptr)
            {
                *eptr = '\0';
                if (eptr > buffer)
                    if (eptr[-1] == '\r')
                        eptr[-1] = '\0';
                ++eptr;
            }
            else
                eptr = lptr + str_length(lptr);

            while ((*lptr == ' ') || (*lptr == '\t')) ++lptr;
            if (*lptr == '#') continue; /* Skip comments */

            if (*lptr == '<')
            {
                if ((octx) && (lptr[1] == '/'))
                    octx = (owa_context *)0;
                else if (!str_compare(lptr + 1, "Location", 8, 1))
                {
                    lptr += 10;
                    while ((*lptr == ' ') || (*lptr == '\t')) ++lptr;
                    for (sptr = lptr; *sptr != '>'; ++sptr)
                        if ((*sptr == ' ') || (*sptr == '\t')) break;
                    *sptr = '\0';

                    octx = create_location(lptr, nthreads);
                    if (!octx)
                    {
                        std_print("Unable to allocate memory for Location\n");
                        break;
                    }
                    octx->mapmem = &(pdctx->mapmem);
                }
            }
            else if ((octx) && (!str_compare(lptr, "SetHandler", 10, 1)))
            {
                lptr += 10;
                while ((*lptr == ' ') || (*lptr == '\t')) ++lptr;
                for (sptr = lptr; *sptr > ' '; ++sptr);
                *sptr = '\0';
                if (!str_compare(lptr, "owa_handler", 11, 1))
                {
                    octx->next = pdctx->loc_list;
                    pdctx->loc_list = octx;
                    octx->mtctx->c_mutex =
                        os_mutex_create((char *)0, 1);
                    octx->mtctx->c_semaphore = os_nullmutex;
                }
            }
            else if ((octx) && (!str_compare(lptr, "Owa", 3, 1)))
            {
                /*
                ** Parse the line and process selected mod_owa directives;
                ** Look for only the Apache 2.0 versions, and ignore
                ** directives that are globally set (e.g. pool size).
                ** Also, ignore the global directives for shared memory
                ** and the cleanup thread, since they don't apply.
                */
                lptr += 3;
                sptr = lptr;
                while (*sptr > ' ') ++sptr;
                *(sptr++) = '\0';

                /*
                ** ### ADD CODE TO FIND THE ARGUMENT BOUNDARIES STARTING
                ** ### FROM sptr, BEING AWARE OF SPACES/TABS AND QUOTES.
                */
                if (!str_compare(lptr, "Log", -1, 1))
                    octx->diagfile = find_arg(&sptr);
                else if (!str_compare(lptr, "Start", -1, 1))
                    octx->doc_start = find_arg(&sptr);
                else if (!str_compare(lptr, "Before", -1, 1))
                    octx->ora_before = find_arg(&sptr);
                else if (!str_compare(lptr, "After", -1, 1))
                    octx->ora_after = find_arg(&sptr);
                else if (!str_compare(lptr, "Proc", -1, 1))
                    octx->ora_proc = find_arg(&sptr);
                else if (!str_compare(lptr, "Realm", -1, 1))
                    octx->authrealm = find_arg(&sptr);
                else if (!str_compare(lptr, "Optimizer", -1, 1))
                    octx->optimizer_mode = find_arg(&sptr);
                else if (!str_compare(lptr, "DocPath", -1, 1))
                    octx->doc_path = find_arg(&sptr);
                else if (!str_compare(lptr, "DocProc", -1, 1))
                    octx->doc_proc = find_arg(&sptr);
                else if (!str_compare(lptr, "DocLong", -1, 1))
                    octx->doc_long = find_arg(&sptr);
                else if (!str_compare(lptr, "DocFile", -1, 1))
                    octx->doc_file = find_arg(&sptr);
                else if (!str_compare(lptr, "DocGen", -1, 1))
                    octx->doc_gen = find_arg(&sptr);
                else if (!str_compare(lptr, "TZ", -1, 1))
                    octx->nls_tz = find_arg(&sptr);
                else if (!str_compare(lptr, "DateFmt", -1, 1))
                    octx->nls_datfmt = find_arg(&sptr);
                else if (!str_compare(lptr, "Bindset", -1, 1))
                    octx->defaultcs = find_arg(&sptr);
                else if (!str_compare(lptr, "Dav", -1, 1))
                    octx->dav_handler = find_arg(&sptr);
                else if (!str_compare(lptr, "SqlError", -1, 1))
                    octx->sqlerr_uri = find_arg(&sptr);
                else if (!str_compare(lptr, "Session", -1, 1))
                    octx->session = find_arg(&sptr);
                else if (!str_compare(lptr, "LDAP", -1, 1))
                    octx->ora_ldap = find_arg(&sptr);
                else if (!str_compare(lptr, "Http", -1, 1))
                    mowa_http(octx, find_arg(&sptr));
                else if (!str_compare(lptr, "Charset", -1, 1))
                    mowa_dad(octx, find_arg(&sptr));
                else if (!str_compare(lptr, "Userid", -1, 1))
                    mowa_uid(octx, find_arg(&sptr));
                else if (!str_compare(lptr, "NLS", -1, 1))
                    mowa_nls(octx, find_arg(&sptr));
                else if (!str_compare(lptr, "UploadMax", -1, 1))
                    mowa_upmx(octx, find_arg(&sptr));
                else if (!str_compare(lptr, "Pool", -1, 1))
                    mowa_pool(octx, find_arg(&sptr), nthreads);
                else if (!str_compare(lptr, "Unicode", -1, 1))
                    mowa_uni(octx, find_arg(&sptr));
                else if (!str_compare(lptr, "Reset", -1, 1))
                    mowa_rset(octx, find_arg(&sptr));
                else if (!str_compare(lptr, "Diag", -1, 1))
                    while (*sptr) mowa_diag(octx, find_arg(&sptr));
                else if (!str_compare(lptr, "Alternate", -1, 1))
                    while (*sptr) mowa_alt(octx, find_arg(&sptr));
                else if (!str_compare(lptr, "DocLobs", -1, 1))
                    while (*sptr) mowa_lobs(octx, find_arg(&sptr));
                else if (!str_compare(lptr, "RefXml", -1, 1))
                    while (*sptr) mowa_refcur(octx, find_arg(&sptr));
                else if (!str_compare(lptr, "Flex", -1, 1))
                    mowa_flex(octx, find_arg(&sptr));
                else if (!str_compare(lptr, "Reject", -1, 1))
                    mowa_reject(octx, find_arg(&sptr));
                else if (!str_compare(lptr, "Charsize", -1, 1))
                    mowa_dbchsz(octx, find_arg(&sptr));
                else if (!str_compare(lptr, "Describe", -1, 1))
                {
                    arg1 = find_arg(&sptr);
                    arg2 = find_arg(&sptr);
                    mowa_desc(octx, arg1, arg2);
                }
                else if (!str_compare(lptr, "Admin", -1, 1))
                {
                    arg1 = find_arg(&sptr);
                    arg2 = find_arg(&sptr);
                    mowa_crtl(octx, arg1, arg2);
                }
                else if (!str_compare(lptr, "Round", -1, 1))
                {
                    arg1 = find_arg(&sptr);
                    arg2 = find_arg(&sptr);
                    mowa_round(octx, arg1, arg2);
                }
                else if (!str_compare(lptr, "Table", -1, 1))
                {
                    arg1 = find_arg(&sptr);
                    arg2 = find_arg(&sptr);
                    mowa_table(octx, arg1, arg2);
                }
                else if (!str_compare(lptr, "Auth", -1, 1))
                {
                    arg1 = find_arg(&sptr);
                    arg2 = (*sptr) ? find_arg(&sptr) : (char *)0;
                    mowa_ver(octx, arg1, arg2);
                }
                else if (!str_compare(lptr, "ContentType", -1, 1))
                {
                    arg1 = find_arg(&sptr);
                    arg2 = (*sptr) ? find_arg(&sptr) : (char *)0;
                    mowa_ctype(octx, arg1, arg2);
                }
                else if (!str_compare(lptr, "Wait", -1, 1))
                {
                    arg1 = find_arg(&sptr);
                    arg2 = (*sptr) ? find_arg(&sptr) : (char *)0;
                    mowa_wait(octx, arg1, arg2);
                }
                else if (!str_compare(lptr, "Env", -1, 1))
                {
                    arg1 = find_arg(&sptr);
                    arg2 = find_arg(&sptr);
                    mowa_env(octx, arg1, arg2);
                }
                else if (!str_compare(lptr, "Cache", -1, 1))
                {
                    arg1 = find_arg(&sptr);
                    arg2 = find_arg(&sptr);
                    arg3 = (*sptr) ? find_arg(&sptr) : (char *)0;
                    mowa_cache(octx, arg1, arg2, arg3);
                }
            }
            else if ((!octx) && (!str_compare(lptr, "Owa", 3, 1)))
            {
                lptr += 3;
                sptr = lptr;
                while (*sptr > ' ') ++sptr;
                *(sptr++) = '\0';
                if (!str_compare(lptr, "FileRoot", -1, 1))
                    pdctx->froot = find_arg(&sptr);
                else if (!str_compare(lptr, "SharedMemory", -1, 1))
                    mowa_mem(pdctx, find_arg(&sptr));
                else if (!str_compare(lptr, "SharedThread", -1, 1))
                    pdctx->tinterval = str_to_tim(find_arg(&sptr));
            }
        }
        mem_free((void *)buffer);
    }

    file_close(fp);
}

/*
** Main loop for the cleanup thread
*/
static void cleanup_thread(daemon_context *pdctx)
{
    owa_context   *octx;
    int            t;

    thread_block_signals();

    while (pdctx->tinterval > 0)
    {
        octx = pdctx->loc_list;

        t = pdctx->tinterval;
 
        if (t > 0)
            while (octx)
            {
                thread_check();
                owa_pool_purge(octx, t);
                owa_file_purge(octx, t);
                octx = octx->next;
            }

        t = pdctx->tinterval;
        if (t < 1000000) t *= 1000;
        os_milli_sleep(t);
    }
    thread_exit();
}

/*
** Stubs for logging APIs
*/

owa_log_socks *owa_log_alloc(owa_context *octx)
{
    return((owa_log_socks *)0);
}

owa_log_socks *owa_log_free(owa_log_socks *s)
{
    return((owa_log_socks *)0);
}

void owa_log_send(owa_context *octx, request_rec *r,
                  connection *c, owa_request *owa_req)
{
}

/*
** Arguments: <IP address> <port number> <nthreads> <conf file>
*/
int main(argc, argv)
int   argc;
char *argv[];
{
    daemon_context *pdctx;
    char           *ipaddr;
    char           *config;
    int             port;
    int             nthreads;
    un_long         tid;
    int             i;

    if (argc < 5)
    {
        std_print("usage:   owad "
                  "<IP address> <port number> <nthreads> <conf file>\n");
        std_print("example: "
                  "owad 127.0.0.1    1234          20         owad.conf\n");
        return(0);
    }

    /*
    ** Decode the arguments
    */
    ipaddr = argv[1];
    port = str_atoi(argv[2]);
    nthreads = str_atoi(argv[3]);
    config = argv[4];
    if (nthreads > MAX_THREADS) nthreads = MAX_THREADS;

    raise_file_limit();

    socket_init();

    sql_init();

    pdctx = &owad_context;
    pdctx->maintid = get_thread_id();

#ifdef MODOWA_WINDOWS
    SetConsoleCtrlHandler(control_handler, (BOOL)1);
#else
    signal(SIGHUP, control_handler);
    signal(SIGINT, control_handler);
    signal(SIGQUIT, control_handler);
#endif

    /*
    ** Load location directives file
    */
    pdctx->loc_list = (owa_context *)0;
    pdctx->ipaddr = ipaddr;
    pdctx->froot = "";
    pdctx->tinterval = 60000; /* Default: 1 minute */
    pdctx->mapmem.filthresh = CACHE_MAX_SIZE;
    pdctx->mapmem.f_mutex = os_nullfilehand;
    pdctx->mapmem.map_ptr = (void *)0;
    pdctx->mapmem.map_fp = os_nullfilehand;
    parse_locations(pdctx, config, nthreads);

    owa_shmem_init(&(pdctx->mapmem));

    pdctx->socklatch = os_mutex_create((char *)0, 1);
    if (InvalidMutex(pdctx->socklatch))
    {
        std_print("Unable to create socket latch\n");
        return(1);
    }

    if (!os_mutex_acquire(pdctx->socklatch, MAX_WAIT))
    {
        std_print("Unable to acquire socket latch\n");
        return(1);
    }

    /*
    ** Create the socket
    */
    pdctx->sock = socket_listen(port, ipaddr, LISTEN_BACKLOG);
    if (pdctx->sock == os_badsocket)
    {
        std_print("Unable to create socket\n");
        return(1);
    }

    for (i = 0; i < nthreads; ++i)
        pdctx->tids[i] = thread_spawn(main_server, (void *)pdctx, &tid);

    while (i < MAX_THREADS) pdctx->tids[i++] = os_nullfilehand;

    /*
    ** Release mutex and enter monitoring loop
    */
    os_mutex_release(pdctx->socklatch);
    cleanup_thread(pdctx);

    return(0);
}
