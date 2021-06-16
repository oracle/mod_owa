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
** File:         owahand.c
** Description:  Main request handling function and supporting interfaces.
** Author:       Doug McMahon      Doug.McMahon@oracle.com
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
**
** ### SHOULD REALLY BREAK UP owa_handle_request SO THAT SOME OF
** ### THE ARGUMENT PROCESSING AND SPECIAL-MODE PROCESSING IS
** ### IN SUBROUTINES.
**
** History
**
** 08/17/2000   D. McMahon      Created
** 05/31/2001   D. McMahon      Began revisioning with 1.3.9
** 06/16/2001   D. McMahon      Added support for RAW input/output bindings
** 09/01/2001   D. McMahon      Added array and scalar rounding
** 09/07/2001   D. McMahon      Added TIMING diagnostic
** 11/07/2001   D. McMahon      Fix mod_rewrite-related CGI problems
** 12/06/2001   D. McMahon      Add regex processing for LocationMatch
** 12/12/2001   D. McMahon      Add the code for OwaReject
** 02/11/2002   D. McMahon      Add OwaDocFile
** 02/15/2002   D. McMahon      Suppress SQL error page for close/reset errors
** 03/20/2002   D. McMahon      Unescape argument names
** 03/27/2002   D. McMahon      Rework request read handling
** 07/22/2002   D. McMahon      Flush response before reset/disconnect
** 10/10/2002   D. McMahon      Add ERROR diagnostic flag
** 11/07/2002   D. McMahon      Add support for raw POST transfer mode
** 03/17/2003   D. McMahon      Merge URL arguments for POSTs
** 06/06/2003   D. McMahon      Support special error page for SQL errors
** 12/19/2003   D. McMahon      Protect argument names from SQL injection
** 01/19/2004   D. McMahon      Add sessioning via session cookies
** 09/14/2004   D. McMahon      Use HTBUF_PLSQL_MAX for string buffers
** 01/16/2006   D. McMahon      Moved nls_length to owanls.c
** 02/12/2006   D. McMahon      Support for 2-argument flex-arg calls
** 09/30/2006   D. McMahon      Protect owa_cleanup from exit processing
** 10/28/2006   D. McMahon      Add errmsg to OwaSqlError URI
** 11/16/2006   D. McMahon      Fix OwaAdmin database password
** 02/02/2007   D. McMahon      Added owa_ldap_lookup() handling.
** 03/23/2007   D. McMahon      Allow OwaEnv to override ORACLE_HOME
** 04/12/2007   D. McMahon      Merge results of multiple describes
** 04/14/2007   D. McMahon      Add limited support for WPG_DOCLOAD
** 08/20/2008   C. Fischer      Added logging support
** 02/23/2009   D. McMahon      Tweaked to be more compatible with Apex
** 06/26/2009   D. McMahon      Added redirection for OwaStart page
** 07/04/2009   D. McMahon      Add support for Apex file uploads
** 03/11/2010   D. McMahon      Redo parse_args() to buffer request in memory
** 04/30/2010   D. McMahon      (long) casts for morq_read and morq_write
** 08/25/2010   D. McMahon      Allow * for OwaLDAP directive (NT auth)
** 03/01/2011   D. McMahon      Allow usr/pwd for OwaLDAP directive
** 04/14/2011   D. McMahon      Check mutex outside of sql_get_nls()
** 05/15/2011   D. McMahon      Check non-null connection before socket alloc
** 11/08/2011   D. McMahon      Allow for quoted identifiers (disabled)
** 11/12/2011   D. McMahon      Add OwaCharsize
** 03/15/2012   D. McMahon      Split PATH_INFO for /procedure/path
** 05/21/2012   D. McMahon      Support flex-args mode for Apex docload
** 06/01/2012   D. McMahon      Fix multi-file Apex docload
** 11/27/2012   D. McMahon      GCC fixes, Win-64 porting
** 12/17/2012   D. McMahon      Check for WDB_GATEWAY_LOGOUT
** 04/12/2013   D. McMahon      Add support for REST operations
** 06/06/2013   D. McMahon      Add REF cursor return bindings
** 07/20/2013   D. McMahon      Add DOCUMENT_TABLE hack for WPG_DOCLOAD
** 07/25/2013   D. McMahon      Parse packed doc info from WPG_DOCLOAD
** 09/19/2013   D. McMahon      64-bit content lengths
** 09/20/2013   D. McMahon      Use tick-count time stamps
** 09/24/2013   D. McMahon      Get errinfo from sql_connect (password expiry)
** 09/26/2013   D. McMahon      Pass client IP address to transfer_env
** 10/21/2013   D. McMahon      Changed interface to morq_set_status()
** 04/27/2014   D. McMahon      Zero init temporary connection cdefault
** 12/23/2014   D. McMahon      Add marker argument to owa_wlobtable()
** 04/27/2015   D. McMahon      Allow oversized file uploads in parse_args()
** 05/11/2015   D. McMahon      Allow read of large REST body to be deferred
** 05/26/2015   D. McMahon      Synthesize PATH_ALIAS for Apex
** 09/11/2015   D. McMahon      Disallow control characters in headers
** 12/07/2015   D. McMahon      Widen array bind limit to 32512
** 03/07/2016   D. McMahon      parse_args strips trailing dots
** 05/04/2016   D. McMahon      OwaReject exact match for strings with suffix
** 01/02/2018   D. McMahon      Widen ERRBUF_SIZE for OS errors
** 05/24/2018   D. McMahon      Fix volatile for connection lock/unlock code
** 06/01/2018   D. McMahon      Add volatile to the describe cache
** 08/14/2018   D. McMahon      (HTBUF_ARRAY_MAX_WIDTH + 1) for '\0'
** 08/20/2018   D. McMahon      Reclassify PATCH as a REST method
** 10/18/2018   D. McMahon      Add DAD_NAME to the CGI environment
** 10/22/2018   D. McMahon      Set PATH_INFO for OwaStart
** 05/29/2020   D. McMahon      Fix a serious bug in array resizing
*/

#define WITH_OCI
#define APACHE_LINKAGE
#include <modowa.h>

/*
** Define this to use alloca() for OWA buffer allocations.
** Prevents heap fragmentation on multi-threaded implementations.
** Unfortunately creates a dependency on the C run-time library
** on Windows, so don't use it on that platform.  Use with
** discretion elsewhere.
*/
#ifndef MODOWA_WINDOWS
# define OWA_USE_ALLOCA
#endif

#define ERRBUF_SIZE  OCI_ERROR_MAXMSG_SIZE+HTBUF_HEADER_MAX+SQL_NAME_MAX+50

static char empty_string[] = "\0";

static char wdb_trailer[] = "{TIME}";

/*
** Get milliseconds elapsed since request start
*/
static un_long get_elapsed_time(long_64 stime)
{
    long_64 etime = util_component_to_stamp(os_get_component_time(0));
    return((un_long)((stime - etime)/(long_64)1000));
}

/*
** Get unique identifier string for OS process+thread
*/
static void get_pid_tid_str(owa_context *octx, char *pid)
{
    int slen;
    un_long tid;

    slen = str_itoa(octx->realpid, pid);
    tid = os_get_tid();
    if (tid > 0)
    {
        pid[slen++] = '_';
        str_itox(tid, pid + slen);
    }
}

/*
** Reallocate array
*/
static void *resize_arr(request_rec *r, void *ptr, int elsize, int currsz)
{
    void *mptr;
    mptr = morq_alloc(r, (currsz + HTBUF_PARAM_CHUNK) * elsize, 1);
    if (mptr) mem_copy(mptr, ptr, currsz * elsize);
    return(mptr);
}

/*
** Find an available connection or slot in the pool and return it
*/
static connection *lock_connection(owa_context *octx, char *session)
{
    int                   i;
    volatile connection *cptr;

    if (octx->poolsize == 0) return((connection *)0);

    cptr = octx->c_pool;

    if (!mowa_semaphore_get(octx)) return((connection *)0);
    mowa_acquire_mutex(octx);

    if (!session) /* First-available search */
    {
        /* Search for any reusable or available connection handle/slot */
        for (i = 0; i < octx->poolsize; ++i)
        {
            if ((cptr->c_lock == C_LOCK_UNUSED) ||
                (cptr->c_lock == C_LOCK_AVAILABLE))
                break;
            ++cptr;
        }
    }
    else /* Session-matching search */
    {
        /* First see if there is a session match */
        for (i = 0; i < octx->poolsize; ++i)
        {
            if (cptr->session)
              if (!str_compare(cptr->session, session, -1, 0))
                if ((cptr->c_lock == C_LOCK_UNUSED) ||
                    (cptr->c_lock == C_LOCK_AVAILABLE))
                  break;
            ++cptr;
        }

        /* Then look for a connection with no session */
        if (i == octx->poolsize)
        {
            cptr = octx->c_pool;
            for (i = 0; i < octx->poolsize; ++i)
            {
                if (cptr->session == (char *)0)
                  if ((cptr->c_lock == C_LOCK_UNUSED) ||
                      (cptr->c_lock == C_LOCK_AVAILABLE))
                    break;
                ++cptr;
            }
        }

        /*
        ** Finally, look for any connection
        **
        ** ### AT SOME POINT WE MIGHT WANT TO SUPPORT "LOCKED" SESSIONING,
        ** ### WHERE A PERSISTENT SESSION IS MAINTAINED.  IN THAT SITUATION,
        ** ### THE CODE NEEDS TO SKIP THIS STEP AND PRODUCE SOME SORT OF
        ** ### ERROR RETURN
        */
        if (i == octx->poolsize)
        {
            cptr = octx->c_pool;
            for (i = 0; i < octx->poolsize; ++i)
            {
                if ((cptr->c_lock == C_LOCK_UNUSED) ||
                    (cptr->c_lock == C_LOCK_AVAILABLE))
                    break;
                ++cptr;
            }
        }
    }

    if (i < octx->poolsize)
    {
        --(octx->poolstats[cptr->c_lock]);
        ++(octx->poolstats[C_LOCK_INUSE]);
        ++(cptr->c_lock);
        cptr->slotnum = (int)(cptr - octx->c_pool);
    }
    else
        cptr = (connection *)0;

    if (!cptr)
        mowa_semaphore_put(octx);
    else
        owa_shmem_update(octx->mapmem, &(octx->shm_offset),
                         octx->realpid, octx->location, octx->poolstats);

    mowa_release_mutex(octx);

    return((connection *)cptr);
}

/*
** Release a connection back to the pool
*/
static void unlock_connection(owa_context *octx, connection *c)
{
    volatile connection *cptr = c;
    int                   lock_state;
    long_64               t;

    t = os_get_component_time(0);
    if (cptr->c_lock == C_LOCK_INUSE)        lock_state = C_LOCK_AVAILABLE;
    else if (cptr->c_lock != C_LOCK_OFFLINE) lock_state = C_LOCK_UNUSED;
    else                                     lock_state = C_LOCK_OFFLINE;
    --(octx->poolstats[C_LOCK_INUSE]);
    ++(octx->poolstats[lock_state]);
    mowa_acquire_mutex(octx);
    cptr->mem_err = 0;
    cptr->timestamp = util_component_to_stamp(t);
    cptr->c_lock = lock_state;
    owa_shmem_update(octx->mapmem, &(octx->shm_offset),
                     octx->realpid, octx->location, octx->poolstats);
    mowa_release_mutex(octx);
    mowa_semaphore_put(octx);
}

/*
** First-time setup for context area
*/
static void owa_setup(owa_context *octx, request_rec *r)
{
    int   i;
    char *ohome = (char *)0;

    /* Check to see if an ORACLE_HOME has been forced for this context */
    for (i = 0; i < octx->nenvs; ++i)
        if (!str_compare(octx->envvars[i].name, "ORACLE_HOME", -1, 0))
        {
            /* Compare it to any existing value of the environment variable */
            ohome = os_env_get("ORACLE_HOME");
            if (str_compare(ohome, octx->envvars[i].value, -1, 0))
                ohome = octx->envvars[i].value;
            else
                ohome = (char *)0;
            break;
        }

    /*
    ** If connection pool not assigned, this is the first call.
    ** Perform parse operations on parameters and set up structure
    */
    if (octx->init_complete) return;

    morq_create_mutex(r, octx);
    mowa_acquire_mutex(octx);
    if (octx->init_complete)
    {
        mowa_release_mutex(octx);
        return;
    }

    /* Change ORACLE_HOME if necessary to match the one forced */
    if (ohome) os_env_set("ORACLE_HOME", ohome);

    if (octx->diagfile)
        if (octx->diagfile[0] == '\0')
            octx->diagfile = (char *)0;

    if (octx->alternate)
        if (octx->alternate[0] == '\0')
            octx->alternate = (char *)0;

    /* Add this context to linked list of contexts */
    morq_add_context(r, octx);

    mowa_semaphore_create(octx);

    owa_set_statements(octx);

    if (!(octx->oracle_userid)) octx->oracle_userid = (char *)"";

    if (octx->lobtypes < 0) octx->lobtypes = LOB_MODE_NCHAR;

    octx->poolstats[C_LOCK_UNUSED] = octx->poolsize;

    /* Logging is incompatible with some get-page return modes */
    if (octx->altflags & (ALT_MODE_LOBS | ALT_MODE_RAW | ALT_MODE_GETRAW))
        octx->altflags &= (~ALT_MODE_LOGGING);

    octx->init_complete = 1;
    mowa_release_mutex(octx);
}

/*
** Append the describe schema if it appears to be necessary
*/
static char *append_schema(owa_context *octx, request_rec *r, char *spath)
{
    char *sptr;
    char *newpath;
    char *schema;
    int   i;

    schema = octx->desc_schema;
    if (!schema) return(spath);
    if ((*schema == '\0') || (*schema == '*')) return(spath);

    /* If of the form <schema>.<package>.<procedure>, return */
    sptr = str_char(spath, '.', 0);
    if (sptr)
    {
        sptr = str_char(++sptr, '.', 0);
        if (sptr) return(spath);
    }

    newpath = (char *)morq_alloc(r, str_length(schema) +
                                    str_length(spath) + 2, 0);
    if (!newpath) return(spath);

    i = str_concat(newpath, 0, schema, -1);
    newpath[i++] = '.';
    str_copy(newpath + i, spath);

    if (octx->diagflag & DIAG_POOL)
        debug_out(octx->diagfile, "Procedure to describe: [%s]\n",
                  newpath, (char *)0, 0, 0);

    return(newpath);
}

/*
** Remove unused bindings from statement (after a describe)
*/
static void revise_statement(char *stmt, int nargs, ub4 *counts)
{
    int   i;
    char *sptr;
    char *aptr;
    char  ch1, ch2;

    sptr = str_substr(stmt, "=>", 0);
    if (!sptr) return;

    ch1 = '(';
    ch2 = ',';
    for (i = 0; i < nargs; ++i)
    {
        if (counts[i] == LONG_MAXSZ)
        {
            if ((i + 1) == nargs) ch2 = ')';
            aptr = sptr;
            while (*aptr != ch1) *(aptr--) = ' ';
            while (*sptr != ch2) *(sptr++) = ' ';
            if (*sptr != ')') *(sptr++) = ' ';
        }
        else
            sptr += 2;

        sptr = str_substr(sptr, "=>", 0);
        if (!sptr) break;
        ch1 = ',';
    }
}

/*
** Inject a clause to force the HTBUF_LEN to a smaller limit
** working around a multibyte bug in the PL/SQL gateway.
*/
static char *set_db_bufsize(char *sptr, int dbchsz)
{
  switch (dbchsz)
  {
  case 2:
    sptr += str_concat(sptr, 0, "  HTP.HTBUF_LEN := 127;\n", -1);
    break;

  case 3:
    sptr += str_concat(sptr, 0, "  HTP.HTBUF_LEN := 85;\n", -1);
    break;

  case 4:
    sptr += str_concat(sptr, 0, "  HTP.HTBUF_LEN := 63;\n", -1);
    break;

  default:
    break;
  }
  return(sptr);
}

/*
** Save result of describe for later use (mutexed)
** Two forms are recorded:
**  1. Flexarg mode discovered to be 2-argument not 4-argument
**  2. Collection-bound argument names (promotions of singletons)
** ### Currently any describe result is saved, even if it later fails
*/
static void cache_describe(owa_context *octx, char *pname, int nargs,
                           char *names[], ub4 counts[])
{
    descstruct   *dptr;
    descstruct   *optr;
    int           pnamelen;
    int           i;
    int           slen;
    int           newflag;
    char         *sptr;
    char         *dname;
    char         *schema;

    /* Caching only enabled if the DESCRIBE schema is set to "*" */
    schema = octx->desc_schema;
    if (!schema) return;
    if (*schema != '*') return;

    /* See if procedure name is already in the cache */
    for (optr = octx->desc_cache; optr != (descstruct *)0; optr = optr->next)
        if (str_compare(optr->pname, pname, -1, 1) == 0)
            break;

    /* If already in cache skip any further processing of flexargs */
    if ((optr) && (nargs == 0)) return;

    pnamelen = str_length(pname) + 2;

    newflag = (optr == (descstruct *)0);

    for (i = 0; i < nargs; ++i)
    {
        if (counts[i] == LONG_MAXSZ)
            return; /* Don't cache relaxed describes */
        if ((counts[i] > 0) && (names[i]))
        {
            pnamelen += (str_length(names[i]) + 1);

            /* See if name was in prior cached version */
            if (!newflag)
            {
                dname = optr->pargs;
                while (*dname)
                {
                    if (str_compare(dname, names[i], -1, 1) == 0) break;
                    dname += (str_length(dname) + 1);
                }
                if (*dname == '\0') newflag = 1;
            }
        }
    }

    /* If cache already up-to-date, done */
    if (!newflag) return;

    /* Merge arguments from previous calls */
    if (optr)
    {
      sptr = optr->pargs;
      while (*sptr)
      {
        /* Search for previous argument usage on new call */
        for (i = 0; i < nargs; ++i)
          if ((counts[i] == 0) && (names[i]))
            if (str_compare(sptr, names[i], -1, 1) == 0)
              break;

        slen = str_length(sptr) + 1;

        /* If not found as a collection bind, include it */
        if (i == nargs) pnamelen += slen;

        sptr += slen;
      }
    }

    dptr = mem_alloc(sizeof(*dptr) + pnamelen);
    if (dptr)
    {
        dname = ((char *)(void *)dptr) + sizeof(*dptr);
        dptr->pname = dname;
        str_copy(dname, pname);
        if (nargs == 0)
        {
            /* Record 2-argument flexargs call mode */
            dptr->flex2 = 1;
            dptr->pargs = (char *)0;
        }
        else
        {
            /* Record collection-bound arguments */
            dptr->flex2 = 0;
            dname += (str_length(dname) + 1);
            dptr->pargs = dname;
            for (i = 0; i < nargs; ++i)
                if ((counts[i] > 0) && (names[i]))
                {
                    slen = str_length(names[i]) + 1;
                    mem_copy(dname, names[i], slen);
                    dname += slen;
                }

            if (optr)
            {
                sptr = optr->pargs;
                while (*sptr)
                {
                    /* Search for previous argument usage on new call */
                    for (i = 0; i < nargs; ++i)
                      if ((counts[i] == 0) && (names[i]))
                        if (str_compare(sptr, names[i], -1, 1) == 0)
                          break;

                    slen = str_length(sptr) + 1;

                    /* If not found as a collection bind, include it */
                    if (i == nargs)
                    {
                        mem_copy(dname, sptr, slen);
                        dname += slen;
                    }

                    sptr += slen;
                }
            }

            *dname = '\0'; /* Terminator */
        }

        /* Add to describe cache */
        mowa_acquire_mutex(octx);
        {
          volatile owa_context *optr = octx;
          /* ### This should use a compare-and-swap ### */
          dptr->next = optr->desc_cache;
          optr->desc_cache = dptr;
        }
        mowa_release_mutex(octx);
    }
}

/*
** Produce header and trailers for HTML error displays
*/
static void htp_error(request_rec *r, char *head)
{
    if (head)
    {
        morq_set_mimetype(r, "text/html");
        morq_send_header(r);
        morq_print_str(r, "<html>\n<title>%s</title>\n", head);
        morq_write(r, "<body bgcolor=\"#ffffff\" text=\"#cc0000\">\n", -1);
        morq_print_str(r, "<h4>%s</h4>\n", head);
    }
    else
        morq_write(r, "</body>\n</html>\n", -1);
}

/*
** Produce an error display for a memory error and return error code
*/
static int mem_error(request_rec *r, int asize, int diagflag)
{
    if (!(diagflag & DIAG_MEMORY)) return(HTTP_INTERNAL_SERVER_ERROR);

    morq_set_status(r, HTTP_INTERNAL_SERVER_ERROR, (char *)0);
    htp_error(r, "MEMORY ERROR");
    morq_print_int(r, "<p>Out of memory - could not allocate %d bytes</p>\n",
                   asize);
    htp_error(r, (char *)0);

    return(OK);
}

/*
** Produce an HTML error display for a SQL error
*/
static int sql_error(request_rec *r, connection *c,
                     int code, int opn, int diagflag, char *diagfile)
{
    if (diagflag & DIAG_ERROR)
    {
        debug_out(diagfile, "SQL error %d\n%s",
                  c->errbuf, (char *)0, code, 0);
        debug_out(diagfile, "The last SQL statement executed was:\n%s",
                  c->lastsql, (char *)0, 0, 0);
    }

    if (!(diagflag & DIAG_SQL))
    {
        morq_sql_error(r, code, c->errbuf, c->lastsql);
        return(HTTP_BAD_REQUEST);
    }

    morq_set_status(r, HTTP_BAD_REQUEST, (char *)0);
    htp_error(r, "SQL ERROR");

    switch (opn)
    {
    case 0:
        morq_print_int(r, "<p>Error %d during OCI operation:</p>\n", code);
        break;
    case 1:
        morq_print_int(r, "<p>Error %d during connect:</p>\n", code);
        break;
    case 2:
        morq_print_int(r, "<p>Error %d passing environment:</p>\n", code);
        break;
    case 3:
        morq_print_int(r, "<p>Error %d calling procedure:</p>\n", code);
        break;
    case 4:
        morq_print_int(r, "<p>Error %d fetching results:</p>\n", code);
        break;
    case 5:
        morq_print_int(r, "<p>Error %d during disconnect:</p>\n", code);
        break;
    default:
        morq_write(r, "<p>Informational message:</p>\n", -1);
        break;
    }
    if (c->errbuf[0] != '\0')
    {
        morq_print_str(r, "<dir><b><pre>\n%s</pre></b></dir>\n", c->errbuf);
    }
    if (c->lastsql)
    {
        morq_write(r, "<p>The last SQL statement executed was:</p>\n", -1);
        morq_print_str(r, "<dir><b><pre>\n%s\n</pre></b></dir>\n", c->lastsql);
    }

    htp_error(r, (char *)0);

    return(OK);
}

/*
** Issue Basic authentication challenge
*/
static void issue_challenge(request_rec *r, char *prealm, int append_timestamp)
{
    char  buf[HTBUF_LINE_LENGTH + LONG_MAXSTRLEN + 1];
    int   slen;

    slen = str_concat(buf, 0, (char *)"Basic realm=\"", HTBUF_LINE_LENGTH);
    slen = str_concat(buf, slen, prealm, HTBUF_LINE_LENGTH);
    if (append_timestamp)
    {
        /* If this is a mod_owa-generated challenge, append a timestamp */
        long_64 tstamp = os_get_component_time(0);
        slen -= (sizeof(wdb_trailer) - 1);
        util_iso_time(tstamp, buf + slen);
        slen += str_length(buf + slen);
    }
    str_copy(buf + slen, "\"");
    nls_sanitize_header(buf);
    morq_table_put(r, OWA_TABLE_HEADERR, 0, "WWW-Authenticate", buf);
    morq_set_status(r, HTTP_UNAUTHORIZED, (char *)0);
    htp_error(r, "Please log in first");
    htp_error(r, (char *)0);
}

/*
** Issue redirection with arguments
*/
static int issue_redirect(request_rec *r, owa_context *octx,
                          int pstatus, char *spath, int *rstatus, char *errbuf)
{
    char  buf[HTBUF_HEADER_MAX];
    char  hexbuf[32];
    int   slen;
    char *sname;
    char *qptr;

    if (!(octx->sqlerr_uri)) return(0);
    if (octx->sqlerr_uri[0] == '\0') return(0);

    slen = str_concat(buf, 0, octx->sqlerr_uri, sizeof(buf)-1);

    /* Try to prevent infinite redirection */
    sname = str_substr(octx->sqlerr_uri, spath, 1);
    if (sname)
    {
        qptr = sname + str_length(spath);
        if ((*qptr == '\0') || (*qptr == '?'))
            return(0);
    }

    /* Append first (or next) argument marker as needed */
    qptr = str_char(buf, '?', 0);
    if (!qptr)
        slen = str_concat(buf, slen, "?", sizeof(buf)-1);
    else if ((qptr[1] != '\0') && (buf[slen-1] != '&'))
        slen = str_concat(buf, slen, "&", sizeof(buf)-1);

    slen = str_concat(buf, slen, "proc=", sizeof(buf)-1);
    slen = str_concat(buf, slen, spath, sizeof(buf)-1);

    slen = str_concat(buf, slen, "&errcode=", sizeof(buf)-1);
    if ((sizeof(buf) - LONG_MAXSTRLEN) > slen) str_itoa(pstatus, buf + slen);

    if (*errbuf)
    {
        /* Tack on the error message */
        slen = str_length(buf);
        slen = str_concat(buf, slen, "&errmsg=", sizeof(buf)-1);
        while (*errbuf)
        {
            /* Escape spaces as + signs */
            if (*errbuf == ' ')
            {
                if ((sizeof(buf)-1) < slen) break;
                buf[slen++] = '+';
            }
            /* If a regular ASCII character not requiring URI escaping */
            else if ((*errbuf > ' ') && (*errbuf <= '~') &&
                     !str_char(" %&=?+\"#<>", *errbuf, 0))
            {
                if ((sizeof(buf)-1) < slen) break;
                buf[slen++] = *errbuf;
            }
            /* Otherwise escape it for the URI */
            else
            {
                if ((sizeof(buf)-3) < slen) break;
                str_itox((int)(*errbuf & 0xFF), hexbuf);
                buf[slen++] = '%';
                mem_copy(buf + slen, hexbuf + 6, 2);
                slen += 2;
          }
          ++errbuf;
        }
        buf[slen] = '\0';
    }

    nls_sanitize_header(buf);
    morq_table_put(r, OWA_TABLE_HEADERR, 0, "Location", buf);

    *rstatus = HTTP_MOVED_TEMPORARILY;
    return(1);
}

/*
** Log timestamp to OWA diagnostics
*/
static void debug_time(owa_context *octx)
{
    char    buf[30];
    long_64 tstamp;

    if (!(octx->diagflag & DIAG_TIMING)) return;
    tstamp = os_get_component_time(0);
    util_print_component_time(tstamp, buf);
    debug_out(octx->diagfile, "%s ", buf, (char *)0, 0, 0);
}

/*
** Log SQL status to OWA diagnostics
*/
static void debug_sql(owa_context *octx, char *sptr, char *pidstr,
                      int status, char *errbuf)
{
    if (!(octx->diagflag & DIAG_POOL)) return;
    debug_time(octx);
    debug_out(octx->diagfile, "SQL> %s %s status = %d\n",
              pidstr, (char *)sptr, status, 0);
    if ((status != OCI_SUCCESS) && (errbuf))
      debug_out(octx->diagfile, "SQL> %s\n", errbuf, (char *)0, 0, 0);
}

/*
** Write termination messages to OWA log file
*/
static void debug_status(owa_context *octx, char *pidstr,
                         int rstatus, int cstatus)
{
    debug_time(octx);
    debug_out(octx->diagfile, "Returning status %d to Apache\n",
              (char *)0, (char *)0, rstatus, 0);
    if (cstatus == OCI_SUCCESS)
        debug_out(octx->diagfile,
                  "Request thread %s completed successfully\n",
                  pidstr, (char *)0, 0, 0);
    else
        debug_out(octx->diagfile,
                  "Request thread %s completed unsuccessfully (%d)\n",
                  pidstr, (char *)0, cstatus, 0);
}

/*
** Lock a connection from the pool and return it
** If a session is specified, preferentially look for a matching connection
** Print diagnostic information
*/
static connection *get_connection(owa_context *octx, char *session)
{
    connection *c;
    char       *sptr;
    int         i;

    c = lock_connection(octx, session);
    if (octx->diagflag & DIAG_POOL)
    {
        if (!c)
        {
            sptr = (char *)"Pool: no connection\n";
            i = 0;
        }
        else
        {
            if (c->c_lock == C_LOCK_INUSE)
                sptr = (char *)"Pool: reuse connection %d\n";
            else
                sptr = (char *)"Pool: new connection %d\n";
            i = (int)((c - octx->c_pool)/sizeof(*c));
        }
        debug_time(octx);
        debug_out(octx->diagfile, sptr, (char *)0, (char *)0, i, 0);
        if (octx->session)
        {
            debug_time(octx);

            if (!session)
            {
                session = c->session;
                if (c->session)
                    sptr = "Pool: replacing session [%s] with new session\n";
                else
                    sptr = "Pool: no session for this request\n";
            }
            else if (!(c->session))
                sptr = "Pool: creating fresh session for [%s]\n";
            else if (!str_compare(c->session, session, -1, 0))
                sptr = "Pool: reusing session for [%s]\n";
            else
                sptr = "Pool: putting session [%s] in place of [%s]\n";

            debug_out(octx->diagfile, sptr, session, c->session, 0, 0);
        }
    }

    /* Allocate permanent buffer for socket maintenance */
    if (c)
      if ((octx->altflags & ALT_MODE_LOGGING) && (!(c->sockctx)))
        c->sockctx = owa_log_alloc(octx);

    return(c);
}

/*
** Disconnect/unlock a connection
*/
static int put_connection(owa_context *octx, int status, char *pidstr,
                          connection *c, connection *db)
{
    int  cstatus;
    int  tmp;
    char *errbuf;

    errbuf = c->errbuf;

    if (c == db)
    {
        tmp = c->mem_err;
        if (status != OCI_SUCCESS) c->errbuf = (char *)0;
        cstatus = sql_disconnect(c);
        c->mem_err = tmp;
    }
    else
    {
        db->lastsql = c->lastsql;
        db->mem_err = c->mem_err;
        cstatus = OCI_SUCCESS;
        if (status == OCI_SUCCESS)
        {
            c->c_lock = C_LOCK_INUSE;
        }
        else if (c->c_lock != C_LOCK_INUSE)
        {
            c->errbuf = (char *)0;
            cstatus = sql_disconnect(c);
        }
#ifdef RESET_AFTER_EXEC
        if ((c->c_lock == C_LOCK_INUSE) && (pidstr))
        {
            if (status != OCI_SUCCESS) c->errbuf = (char *)0;
            cstatus = owa_reset(c, octx);
            debug_sql(octx, "reset", pidstr, cstatus, (char *)0);
        }
#endif
        c->mem_err = 0;
        unlock_connection(octx, c);
    }

    db->errbuf = errbuf;

    return(cstatus);
}

/*
** Generate a response page for a control request
**   CLOSEPOOL!    - Close all OCI connections, take pool off line
**   OPENPOOL!     - Bring OCI connection pool on-line
**   CLEARPOOL!    - Clear old OCI connections by closing them
**   SHOWPOOL!     - Show status of OCI connection pool
**   CLEARCACHE!   - Clear file system cache
**   SHOWCACHE!    - Show status of file system cache
**   AUTHENTICATE! - Force authorization check
*/
static int handle_control(owa_context *octx, request_rec *r,
                          char *spath, char *args, char *pid,
                          un_long remote_addr, char *errbuf)
{
    char       *sptr;
    char       *aptr;
    int         i, n;
    int         poolstats[C_LOCK_MAXIMUM];
    sword       status;
    connection *c;
    connection  cdefault;

    /*
    ** Check that the remote address is valid for control functions
    */
    if ((remote_addr & octx->crtl_mask) != octx->crtl_subnet)
    {
        htp_error(r, "INVALID CONTROL CLIENT");
        htp_error(r, (char *)0);
        return(OK);
    }

    /*
    ** Check the URL for the control password, which must be present
    */
    if (!args)
    {
        htp_error(r, "MISSING CONTROL PASSWORD");
        htp_error(r, (char *)0);
        return(OK);
    }
    sptr = str_char(octx->oracle_userid, '/', 0);
    if (sptr)  ++sptr;
    else         sptr = octx->oracle_userid;
    aptr = str_char(sptr, '@', 0);
    n = (aptr) ? ((int)(aptr - sptr)) : str_length(sptr);
    for (aptr = args; *aptr != '\0'; ++aptr)
        if (*aptr == '=') break;
    if (*aptr == '=')  ++aptr;
    else                 aptr = args;
    i = str_length(aptr);
    if (i > n) n = i;
    if (str_compare(aptr, sptr, n, 1))
    {
        htp_error(r, "INVALID CONTROL PASSWORD");
        htp_error(r, (char *)0);
        return(OK);
    }

    if (!str_compare(spath, "AUTHENTICATE!", -1, 1))
    {
#ifdef NEVER
        /* ### THIS SHOULD WORK, BUT DOESN'T ### */
        sptr = (!ap_get_basic_auth_pw(r, &aptr)) ? r->connection->user :
                                                   (char *)"";
#endif
        sptr = morq_get_header(r, "Authorization");
        if (!sptr) sptr = (char *)"";

        if (*sptr == '\0')
        {
            sptr = octx->authrealm;
            if (!sptr) sptr = (char *)"localhost";
            issue_challenge(r, sptr, 0);
        }
        else
        {
            aptr = morq_parse_auth(r, sptr);
            htp_error(r, "Information:");
            morq_print_str(r, "<p>Logged in as [%s]</p>\n", aptr);
            htp_error(r, (char *)0);
        }
        return(OK);
    }
    else if (!str_compare(spath, "CLOSEPOOL!", -1, 1))
    {
        do
        {
            c = lock_connection(octx, (char *)0);
            if (c)
            {
                status = OCI_SUCCESS;
                if (c->c_lock == C_LOCK_INUSE)
                {
                    *errbuf = '\0';
                    c->errbuf = errbuf;
                    status = sql_disconnect(c);
                }
                c->c_lock = C_LOCK_OFFLINE;
                unlock_connection(octx, c);
                if (status != OCI_SUCCESS)
                {
                    c = &cdefault;
                    c->lastsql = (char *)"CLOSE CURSOR";
                    c->mem_err = 0;
                    c->errbuf = errbuf;
                    return(sql_error(r, c, status, 0,
                                     octx->diagflag, octx->diagfile));
                }
            }
        } while (c);
        /* Fall through to showpool */
    }
    else if (!str_compare(spath, "OPENPOOL!", -1, 1))
    {
        mowa_acquire_mutex(octx);
        n = 0;
        for (i = 0; i < octx->poolsize; ++i)
            if (octx->c_pool[i].c_lock == C_LOCK_OFFLINE) ++n;
        octx->poolstats[C_LOCK_OFFLINE] -= n;
        octx->poolstats[C_LOCK_UNUSED] += n;
        owa_shmem_update(octx->mapmem, &(octx->shm_offset),
                         octx->realpid, octx->location, octx->poolstats);
        for (i = 0; i < octx->poolsize; ++i)
        {
            c = octx->c_pool + i;
            if (c->c_lock == C_LOCK_OFFLINE) c->c_lock = C_LOCK_UNUSED;
        }
        mowa_release_mutex(octx);
        /* Fall through to showpool */
    }
    else if (!str_compare(spath, "CLEARPOOL!", -1, 1))
    {
        owa_pool_purge(octx, 0);
        /* Fall through to showpool */
    }
    else if ((!str_compare(spath, "CLEARCACHE!", -1, 1)) ||
             (!str_compare(spath, "SHOWCACHE!", -1, 1)))
    {
#ifdef NO_FILE_CACHE
        htp_error(r, "File system cache not supported");
        htp_error(r, (char *)0);
#else
        if (!str_compare(spath, "CLEARCACHE!", -1, 1))
            owa_file_purge(octx, 0);
        htp_error(r, "CACHE STATUS");
        owa_file_show(octx, r);
        htp_error(r, (char *)0);
#endif
        return(OK);
    }
    else if (str_compare(spath, "SHOWPOOL!", -1, 1))
    {
        /* Generate page showing available commands */
        htp_error(r, "COMMANDS");
        morq_write(r, "<table cellspacing=\"2\" cellpadding=\"2\""
                      " border=\"0\">\n", -1);
        sptr = (char *)"<tr><td align=\"right\">%s</td>";
        aptr = (char *)"<td>- %s</td></tr>\n";
        morq_print_str(r, sptr, "CLOSEPOOL!");
        morq_print_str(r, aptr,
                       "Close all OCI connections, take pool off line");
        morq_print_str(r, sptr, "OPENPOOL!");
        morq_print_str(r, aptr, "Bring OCI connection pool on-line");
        morq_print_str(r, sptr, "CLEARPOOL!");
        morq_print_str(r, aptr, "Remove old connections from pool");
        morq_print_str(r, sptr, "SHOWPOOL!");
        morq_print_str(r, aptr, "Show status of OCI connection pool");
        morq_print_str(r, sptr, "CLEARCACHE!");
        morq_print_str(r, aptr, "Remove old files from file system cache");
        morq_print_str(r, sptr, "SHOWCACHE!");
        morq_print_str(r, aptr, "Show files in file system cache");
        morq_print_str(r, sptr, "AUTHENTICATE!");
        morq_print_str(r, aptr, "Force authorization check");
        morq_write(r, "</table>\n", -1);
        morq_print_str(r, "<p>%s</p>\n", octx->verstring);
        htp_error(r, (char *)0);
        return(OK);
    }

    htp_error(r, "POOL STATUS");

    morq_print_str(r, "<p>PID = %s</p>\n", pid);

    if (owa_shmem_stats(octx->mapmem, octx->location, poolstats) > 0)
    {
        n = 0;
        for (i = 0; i < C_LOCK_MAXIMUM; ++i) n += poolstats[i];
    }
    else
    {
        for (i = 0; i < C_LOCK_MAXIMUM; ++i) poolstats[i] = 0;
        for (i = 0; i < octx->poolsize; ++i)
        {
            c = octx->c_pool + i;
            n = c->c_lock;
            if (n > C_LOCK_UNKNOWN) n = C_LOCK_UNKNOWN;
            ++poolstats[n];
        }
        n = octx->poolsize;
    }

    morq_print_str(r, "<p>Pool stats for location %s</p>\n", octx->location);

    morq_write(r, "<table cellspacing=\"2\" cellpadding=\"2\""
                  " border=\"0\">\n", -1);

    sptr = (char *)"<tr><td align=\"right\">%s</td>";
    aptr = (char *)"<td>%d</td></tr>\n";

    morq_print_str(r, sptr, "Unused:");
    morq_print_int(r, aptr, poolstats[C_LOCK_UNUSED]);
    morq_print_str(r, sptr, "Under Creation:");
    morq_print_int(r, aptr, poolstats[C_LOCK_NEW]);
    morq_print_str(r, sptr, "Available:");
    morq_print_int(r, aptr, poolstats[C_LOCK_AVAILABLE]);
    morq_print_str(r, sptr, "In use:");
    morq_print_int(r, aptr, poolstats[C_LOCK_INUSE]);
    morq_print_str(r, sptr, "Offline:");
    morq_print_int(r, aptr, poolstats[C_LOCK_OFFLINE]);
    morq_print_str(r, sptr, "Unknown:");
    morq_print_int(r, aptr, poolstats[C_LOCK_UNKNOWN]);

    morq_write(r, "<tr><td colspan=\"2\">&nbsp;</td></tr>\n", -1);

    morq_print_str(r, sptr, "Total:");
    morq_print_int(r, aptr, n);

    morq_write(r, "</table>\n", -1);

    htp_error(r, (char *)0);
    return(OK);
}

/*
** Scan a cookie string for WDB_GATEWAY_LOGOUT=YES.
*/
static int owa_find_logout(owa_context *octx, char *cookieval)
{
    char *sptr;
    char *cptr;
    char *nptr;
    char *gptr = "WDB_GATEWAY_LOGOUT";
    int   glen = str_length(gptr);

    if (!(octx->authrealm)) return(0);

    cptr = cookieval;

    while (*cptr == ' ') ++cptr;

    /* Get cookie name */
    sptr = cptr;
    while (*sptr != '=')
    {
      if (*sptr == '\0') return(0); /* ### Premature end of cookie string */
      ++sptr;
    }

    /* Get cookie value */
    nptr = sptr;
    while (*nptr != ';')
    {
      if (*nptr == '\0') break;
      ++nptr;
    }

    /* If this is the cookie we're looking for */
    if ((sptr - cptr) == glen)
      if (!str_compare(gptr, cptr, glen, 1))
      {
        ++sptr;
        /* If the value is YES */
        if (!str_compare(sptr, "YES", 3, 1))
          return(1);
      }

    return(0);
}

static const char env_iana_name[]    = "REQUEST_IANA_CHARSET";
static const char env_script_name[]  = "SCRIPT_NAME";
static const char env_path_name[]    = "PATH_INFO";
static const char env_remote_ip[]    = "REMOTE_ADDR";
static const char env_authorize[]    = "HTTP_AUTHORIZATION";
static const char env_remote_usr[]   = "REMOTE_USER";
static const char env_content_type[] = "CONTENT_TYPE";

/*
** Scan subprocess environment for path, boundary, content length, etc.
** Returns 0 on success, a non-zero value for the size of a memory failure.
*/
static int search_env(owa_context *octx, request_rec *r,
                      char **spath, long_64 *arglen, char **ctype,
                      char **boundary, un_long *remote_addr,
                      int *nam_max, int *val_max, int *nenv,
                      char **authuser, char **authpass, char *session,
                      int *wdb_realm_logout)
{
    int   i;
    int   nwidth = 30;
    int   vwidth = 30;
    long_64 clen = 0;
    int   n;
    char *sptr;
    char *nptr;
    char *vptr;
    char *bptr;
    char *virt_uri = (char *)0;
    char *path_info = (char *)0;
    char *script_name = (char *)0;
    char *authorize = (char *)0;
    char *remote_user = (char *)0;
    char *requri = (char *)0;
    char *usr = (char *)0;
    char *pwd = (char *)0;
    char *ldapauth = (char *)0;
    int   dad_found;
    int   path_found;
    int   script_found;
    int   loc_found;
    int   remote_user_override = 1;

    *arglen = -1;
    *spath = (char *)0;
    *boundary = (char *)0;
    *remote_addr = 0;

    dad_found = 0;

    if (octx->diagflag & DIAG_COOKIES)
    {
        debug_out(octx->diagfile, "Cookies\n", (char *)0, (char *)0, 0, 0);
        for (i = 0;
             morq_table_get(r, OWA_TABLE_HEADIN, i, &nptr, &vptr);
             ++i)
            if (!str_compare(nptr, "COOKIE", 6, 1))
                debug_out(octx->diagfile, "  %s\n", vptr, (char *)0, 0, 0);
    }

    /*
    ** Measure lengths of any additional env variables
    */
    for (i = 0; i < octx->nenvs; ++i)
    {
        n = str_length(octx->envvars[i].name);
        if (i > nwidth) nwidth = i;
        n = str_length(octx->envvars[i].value);
        if (i > vwidth) vwidth = i;
    }

    if (octx->diagflag & DIAG_CGIENV)
        debug_out(octx->diagfile,
                  "CGI Environment\n", (char *)0, (char *)0, 0, 0);

    /*
    ** Search for CONTENT_LENGTH, CONTENT_TYPE, PATH_INFO and values,
    ** measure maximum column widths for name and value arrays.
    */
    for (i = 0;
         morq_table_get(r, OWA_TABLE_SUBPROC, i, &nptr, &vptr);
         ++i)
    {
        if (!nptr) continue;
        if (!vptr) vptr = (char *)"";

        if (octx->diagflag & DIAG_CGIENV)
            debug_out(octx->diagfile, "  %s = %s\n", nptr, vptr, 0, 0);

        if (!str_compare(nptr, "HTTP_COOKIE", -1, 1))
        {
            if (owa_find_logout(octx, vptr))
              *wdb_realm_logout = 1;
            else
              owa_find_session(octx, vptr, session);
        }
        else if (!str_compare(nptr, "CONTENT_LENGTH", -1, 1))
        {
            for (sptr = vptr; *sptr != '\0'; ++sptr)
                clen = (clen * 10) + (long_64)(*sptr - '0');
            *arglen = clen;
        }
        else if (!str_compare(nptr, "CONTENT_TYPE", -1, 1))
        {
            if (!str_compare(vptr, "multipart/form-data;", 20, 1))
            {
                sptr = str_substr(vptr, "boundary=", 1);
                if (!sptr)
                    *boundary = (char *)"";
                else
                {
                    sptr += 9;
                    n = str_length(sptr) + 5;
                    bptr = (char *)morq_alloc(r, n, 0);
                    if (!bptr) return(-n); /* Trivial allocation failed! */
                    mem_copy(bptr, "\r\n--", 4);
                    mem_copy(bptr + 4, sptr, n - 4);
                    sptr = str_char(bptr, ' ', 0);
                    if (sptr) *sptr = '\0';
                    *boundary = bptr;
                }
            }

            /* Extract and return the media type of the request */
            sptr = str_char(vptr, ';', 0);
            if (!sptr)
              bptr = vptr;
            else
            {
              n = (int)(sptr - vptr) + 1;
              bptr = (char *)morq_alloc(r, n, 0);
              if (!bptr) return(-n); /* Trivial allocation failed! */
              --n;
              mem_copy(bptr, vptr, n);
              bptr[n] = '\0';
            }
            *ctype = bptr;
        }
        else if (!str_compare(nptr, "REQUEST_URI", -1, 1))
            requri = vptr;
        else if (!str_compare(nptr, env_path_name, -1, 1))
            path_info = vptr;
        else if (!str_compare(nptr, env_script_name, -1, 1))
            script_name = vptr;
        else if (!str_compare(nptr, env_iana_name, -1, 1))
            dad_found = 1;
        else if (!str_compare(nptr, env_remote_ip, -1, 1))
            *remote_addr = util_ipaddr(vptr);
        else if (!str_compare(nptr, env_authorize, -1, 1))
            authorize = vptr;
        else if (!str_compare(nptr, env_remote_usr, -1, 1))
            remote_user = vptr;
        n = str_length(nptr);
        if (n > nwidth) nwidth = n;
        n = str_length(vptr);
        if (n > vwidth) vwidth = n;
    }

    /*
    ** ### SHOULD THIS BE BASE-64 DECODED FIRST?  IT WOULD BE MORE
    ** ### CONVENIENT, BUT THE MAIN GOAL IS TO MATCH WHAT OAS DOES.
    ** ### WHAT DOES APACHE DO?  WE MIGHT NEED TO BASE-64 DECODE
    ** ### THAT AS WELL, OR OVERRIDE IT BACK TO BASE-64 FORMAT.
    */
    if (!authorize)
    {
        /*
        ** Transfer authorization string to CGI
        ** ### NOT NEEDED BECAUSE httpd.conf HAS A SETTING TO SUPPORT THIS?
        */
        vptr = morq_get_header(r, "Authorization");
        if (vptr)
        {
            authorize = vptr;
            morq_table_put(r,OWA_TABLE_SUBPROC,0,(char *)env_authorize,vptr);
            n = str_length(vptr);
            if (n > vwidth) vwidth = n;
            ++i;
        }
    }

    /* Decode Basic authentication string (if present) */
    if (authorize)
    {
        sptr = morq_parse_auth(r, authorize);
        usr = str_char(sptr, ' ', 0);
        if (!usr) usr = sptr;
        else      ++usr;
        pwd = str_char(usr, ':', 0);
        if (pwd) *(pwd++) = '\0';
        else     pwd = (char *)"";

        /* ### sptr will be truncated before the password ### */
        if (octx->diagflag & DIAG_CGIENV)
            debug_out(octx->diagfile,
                      "  Authentication: Raw = [%s] Parsed = [%s]\n",
                      authorize, sptr, 0, 0);
    }

    /* If configured, call LDAP to convert basic auth to DB user/pass */
    if (octx->ora_ldap)
    {
      char *slash = str_char(octx->ora_ldap, '/', 0);
      /* If a / is present, extract user/pass from HTTP headers */
      if (slash)
      {
        int   hdrlen = str_length(octx->ora_ldap) + 1;
        char *pwdhdr;
        char *usrhdr = (char *)morq_alloc(r, hdrlen, 0);
        if (!usrhdr) return(-hdrlen); /* Trivial allocation failed! */
        pwdhdr = usrhdr + (slash - octx->ora_ldap);
        mem_copy(usrhdr, octx->ora_ldap, hdrlen);
        *(pwdhdr++) = '\0';

        /* Look up the user header */
        usr = morq_get_env(r, usrhdr);
        if (!usr)
          usr = usrhdr; /* If not found, use the string as-is */
        else
        {
          /* Duplicate the header string to request memory */
          hdrlen = str_length(usr) + 1;
          usrhdr = (char *)morq_alloc(r, hdrlen, 0);
          if (!usrhdr) return(-hdrlen); /* Trivial allocation failed! */
          mem_copy(usrhdr, usr, hdrlen);
          usr = usrhdr;
        }
        
        /* Look up the password header */
        pwd = morq_get_env(r, pwdhdr);
        if (!pwd)
          pwd = pwdhdr; /* If not found, use the string as-is */
        else
        {
          /* Duplicate the header string to request memory */
          hdrlen = str_length(pwd) + 1;
          pwdhdr = (char *)morq_alloc(r, hdrlen, 0);
          if (!pwdhdr) return(-hdrlen); /* Trivial allocation failed! */
          mem_copy(pwdhdr, pwd, hdrlen);
          pwd = pwdhdr;
        }
      }
      /* Otherwise call a PL/SQL procedure if configured */
      else if ((authorize) || (*session))
        ldapauth = owa_ldap_lookup(octx, r, usr, pwd, session);
    }

    if (ldapauth)
    {
        if (octx->diagflag & DIAG_CGIENV)
            debug_out(octx->diagfile,
                      "  Using LDAP: DB user is [%s]\n",
                      ldapauth, (char *)0, 0, 0);
        usr = ldapauth;
        pwd = ldapauth + str_length(ldapauth) + 1;
    }

    /* Save the results of Basic authentication processing */
    if (usr)
    {
        vptr = usr;
        if (!remote_user)
        {
            morq_table_put(r,OWA_TABLE_SUBPROC,0,(char *)env_remote_usr,vptr);
            ++i;
        }
        else
        {
          if (octx->ora_ldap)
            if (*(octx->ora_ldap) == '*')
              remote_user_override = 0;
          if (remote_user_override)
            morq_table_put(r,OWA_TABLE_SUBPROC,1,(char *)env_remote_usr,vptr);
        }
        n = str_length(vptr);
        if (n > vwidth) vwidth = n;
        *authuser = usr;
        *authpass = pwd;
    }

    if (octx->dad_csid)
    {
        vptr = (char *)nls_iana(octx->dad_csid);
        if (dad_found)
            morq_table_put(r,OWA_TABLE_SUBPROC,1,(char *)env_iana_name,vptr);
        else
        {
            morq_table_put(r,OWA_TABLE_SUBPROC,0,(char *)env_iana_name,vptr);
            ++i;
        }
        n = str_length(vptr);
        if (n > vwidth) vwidth = n;
    }

    /*
    ** This forces a value for GATEWAY_IVERSION if you've got
    ** the WebDB table/column document directive set.  Otherwise
    ** WPG_DOCLOAD doesn't work properly.
    ** ### This is a pretty lame hack.
    */
    if ((octx->doc_table) || (octx->doc_column))
    {
        nptr = (char *)"GATEWAY_IVERSION";
        vptr = (char *)"mod_owa";
        morq_table_put(r, OWA_TABLE_SUBPROC, 0, nptr, vptr);
        n = str_length(nptr);
        if (n > nwidth) nwidth = n;
        n = str_length(vptr);
        if (n > vwidth) vwidth = n;
        ++i;
    }

    /*
    ** Forces a value for DOCUMENT_TABLE, also for WPG_DOCLOAD.
    ** ### Another lame hack.
    */
    if (octx->doc_table)
    {
        nptr = (char *)"DOCUMENT_TABLE";
        vptr = octx->doc_table;
        morq_table_put(r, OWA_TABLE_SUBPROC, 0, nptr, vptr);
        n = str_length(nptr);
        if (n > nwidth) nwidth = n;
        n = str_length(vptr);
        if (n > vwidth) vwidth = n;
        ++i;
    }

    /*
    ** Forces a value for PATH_ALIAS, to satisfy wwv_flow.resolve_friendly_url.
    ** ### Yet another lame hack.
    */
    if ((octx->doc_table) && ((octx->doc_gen) || (octx->doc_path)))
    {
        vptr = octx->doc_gen;
        if (!vptr) vptr = octx->doc_path;
        nptr = (char *)"PATH_ALIAS";
        morq_table_put(r, OWA_TABLE_SUBPROC, 0, nptr, vptr);
        n = str_length(nptr);
        if (n > nwidth) nwidth = n;
        n = str_length(vptr);
        if (n > vwidth) vwidth = n;
        ++i;
    }

#ifdef NEVER
    /* Force a value for DOC_ACCESS_PATH as well? */
    if ((octx->doc_table) && (octx->doc_path))
    {
        vptr = octx->doc_path;
        nptr = (char *)"DOC_ACCESS_PATH";
        morq_table_put(r, OWA_TABLE_SUBPROC, 0, nptr, vptr);
        n = str_length(nptr);
        if (n > nwidth) nwidth = n;
        n = str_length(vptr);
        if (n > vwidth) vwidth = n;
        ++i;
    }
#endif

    if (octx->altflags & ALT_MODE_CACHE)
    {
        nptr = (char *)"MODOWA_PAGE_CACHE";
        vptr = (char *)"enabled";
        morq_table_put(r, OWA_TABLE_SUBPROC, 0, nptr, vptr);
        n = str_length(nptr);
        if (n > nwidth) nwidth = n;
        n = str_length(vptr);
        if (n > vwidth) vwidth = n;
        ++i;
    }

    /*
    ** Set a value for DAD_NAME if configured.
    */
    if (octx->dad_name)
    {
        nptr = (char *)"DAD_NAME";
        vptr = octx->dad_name;
        morq_table_put(r, OWA_TABLE_SUBPROC, 0, nptr, vptr);
        n = str_length(nptr);
        if (n > nwidth) nwidth = n;
        n = str_length(vptr);
        if (n > vwidth) vwidth = n;
        ++i;
    }

    /*
    ** Construct a virtual Request URI from SCRIPT_NAME|PATH_INFO
    */
    if (!script_name)
        virt_uri = path_info;
    else if (!path_info)
        virt_uri = script_name;
    else
    {
        n = str_length(script_name) + str_length(path_info) + 1;
        if (n > 1)
        {
            virt_uri = (char *)morq_alloc(r, n, 0);
            if (!virt_uri) return(-n); /* Trivial allocation failed! */
            sptr = virt_uri;
            sptr += str_concat(sptr, 0, script_name, n - 1);
            str_copy(sptr, path_info);
        }
    }

    if (virt_uri)
    {
        nptr = (char *)"MODOWA_REQUEST_URI";
        vptr = virt_uri;
        morq_table_put(r, OWA_TABLE_SUBPROC, 0, nptr, vptr);
        n = str_length(nptr);
        if (n > nwidth) nwidth = n;
        n = str_length(vptr);
        if (n > vwidth) vwidth = n;
        ++i;
    }
    else if (requri) /* Desperation */
    {
        sptr = str_char(requri, '?', 0);
        if (sptr)
        {
            n = (int)(sptr - requri);
            virt_uri = (char *)morq_alloc(r, n, 0);
            if (!virt_uri) return(-n); /* Trivial allocation failed! */
            str_concat(virt_uri, 0, requri, n - 1);
        }
        else
            virt_uri = requri;
    }

    /*
    ** ### Currently the virt_uri will not have the QUERY_STRING
    ** ### attached.  If this should become necessary, it should
    ** ### be added above.  Later code assumes that it's not
    ** ### present and therefore doesn't search for the '?' when
    ** ### breaking off PATH_INFO, so that code must also be updated.
    */

    loc_found = 1;

    path_found = (path_info) ? 1 : 0;
    script_found = (script_name) ? 1 : 0;

    if (virt_uri)
    {
      /*
      ** In cases where the Location is simply a "/", Apache puts
      ** the whole request string into SCRIPT_NAME instead of PATH_INFO
      ** (where the OWA expects it).  So, set PATH_INFO == SCRIPT_NAME
      ** and null out SCRIPT_NAME (if SCRIPT_NAME is also null, use
      ** the REQUEST_URI instead).
      */
      if (!str_compare(octx->location, "/", -1, 0))
      {
        if (!path_found) ++i;
        path_info = virt_uri; /* ### Parse off "?" if necessary ### */
        morq_table_put(r, OWA_TABLE_SUBPROC, path_found,
                       (char *)env_path_name, path_info);
        path_found = 1;
        if (script_found)
            morq_table_put(r, OWA_TABLE_SUBPROC, 1,
                           (char *)env_script_name, (char *)"");
        script_name = (char *)0;
      }
      /*
      ** In other cases where there is no PATH_INFO, assume that the
      ** actual path is found in SCRIPT_NAME.  Scan it for the Location
      ** boundary and split off the PATH_INFO portion.  Again, use
      ** REQUEST_URI if SCRIPT_NAME is null.
      **
      ** ### I HAVE NO IDEA WHY APACHE SOMETIMES FAILS TO PASS A PATH_INFO.
      ** ### IT IS SUSPECTED THAT THIS HAS SOMETHING TO DO WITH NOT SETTING
      ** ### DocumentRoot IN httpd.conf.
      */
      else if (!path_info)
      {
        n = str_length(octx->location);
        if (str_compare(virt_uri, octx->location, n, 1)) n = 0;
        sptr = virt_uri + n;
        path_info = sptr; /* ### Parse off "?" if necessary ### */
        morq_table_put(r, OWA_TABLE_SUBPROC, 0,
                       (char *)env_path_name, path_info);
        ++i;
        if (n > 0)
        {
            morq_table_put(r, OWA_TABLE_SUBPROC, script_found,
                           (char *)env_script_name, octx->location);
            if (!script_found) ++i;
            script_found = 1;
            script_name = octx->location;
            n = str_length(script_name);
            if (n > vwidth) vwidth = n;
        }
        else
        {
            if (script_found)
                morq_table_put(r, OWA_TABLE_SUBPROC, 1,
                               (char *)env_script_name, (char *)"");
            script_name = (char *)0;
            loc_found = 0;
        }
      }
      n = str_length(path_info);
      if (n > vwidth) vwidth = n;
    }

    /*
    ** Apache parses the path differently than the way OWA is expecting.
    ** For example, if you have a Location "/owa/subdir", and you
    ** input "/localhost/owa/subdir/procedure?args", you get:
    **
    **   SCRIPT_NAME  /owa
    **   PATH_INFO        /subdir/procedure
    **
    ** whereas the OWA is expecting:
    **
    **   SCRIPT_NAME  /owa/subdir
    **   PATH_INFO               /procedure
    **
    ** If necessary, attempt to rewrite these values here.
    */
    if ((path_info) && (script_name))
    {
        loc_found = 0;
        sptr = str_substr(octx->location, script_name, 1);
        if (sptr)
        {
            /* SCRIPT_NAME found within Location */
            sptr += str_length(script_name);
            n = str_length(sptr);
            if (n == 0)
                loc_found = 1;
            else
            {
                if (!str_compare(sptr, path_info, n, 1))
                {
                    path_info += n;
                    script_name = octx->location;
                    morq_table_put(r, OWA_TABLE_SUBPROC, 1,
                                   (char *)env_path_name, path_info);
                    morq_table_put(r, OWA_TABLE_SUBPROC, 1,
                                   (char *)env_script_name, script_name);
                    n = str_length(script_name);
                    if (n > vwidth) vwidth = n;
                    if (octx->diagflag & DIAG_CGIENV)
                        debug_out(octx->diagfile, "  Rewrite [%s][%s]\n",
                                  script_name, path_info, 0, 0);
                    loc_found = 1;
                }
                /* Else trailing fragment of Location doesn't lead PATH_INFO */
            }
        }
    }

    if ((!loc_found) && (virt_uri))
    {
        /*
        ** LocationMatch processing
        **   Do a shortest-length regular-expression match against
        **   the full URI, using the Location as the pattern.  If
        **   a match is found, then the endpoint of the matching
        **   substring is the breakpoint between SCRIPT_NAME and
        **   PATH_INFO.
        */
        bptr = octx->location;
        n = str_length(bptr);
        if (n > 0)
          if (bptr[n - 1] != '/')
          {
              n += 2;
              bptr = (char *)morq_alloc(r, n, 0);
              if (!bptr) return(-n); /* Trivial allocation failed! */
              str_copy(bptr, octx->location);
              bptr[--n] = '\0';
              bptr[--n] = '/';
          }
        sptr = virt_uri;
        n = morq_regex(r, bptr, &sptr, 1);
        if (n > 0)
        {
            sptr += (n - 1);

            /* SCRIPT_NAME is matching fraction up to break point */
            n = (int)(sptr - virt_uri) + 1;
            script_name = (char *)morq_alloc(r, n, 0);
            if (!script_name) return(-n); /* Trivial allocation failed! */
            str_concat(script_name, 0, virt_uri, n - 1);

            /* PATH_INFO is remaining fraction up to any question mark */
            path_info = sptr; /* ### Parse off "?" if necessary ### */

            if (!path_found)
            {
                n = str_length(path_info);
                if (n > vwidth) vwidth = n;
                ++i;
            }
            if (!script_found)
            {
                n = str_length(script_name);
                if (n > vwidth) vwidth = n;
                ++i;
            }

            morq_table_put(r, OWA_TABLE_SUBPROC, path_found,
                           (char *)env_path_name, path_info);
            morq_table_put(r, OWA_TABLE_SUBPROC, script_found,
                           (char *)env_script_name, script_name);
            path_found = 1;
            script_found = 1;
            if (octx->diagflag & DIAG_CGIENV)
                debug_out(octx->diagfile, "  Rewrite [%s][%s]\n",
                          script_name, path_info, 0, 0);
        }
    }

    /*
    ** Break up paths that obviously can't be procedure names
    ** Look for the following patterns:
    **   /!proc/path...
    **   /^proc/path...
    **   /~proc/path...
    */
    if (path_info)
      if (path_info[0] == '/')
      {
        char *splitpath = str_char(path_info + 1, '/', 0);
        if (splitpath)
        {
          /*
          ** Only perform this special logic if there's no document procedure
          ** or if the document procedure is flagged with a '*'
          */
          char *docproc = octx->doc_proc;
          if (docproc)
            if ((*docproc == '\0') || (*docproc == '*'))
              docproc = (char *)0;
          /* Also omit this logic if there's a REST/DAV handler */
          if (!docproc && !(octx->dav_handler))
          {
            int ch = (int)path_info[1];
            /*
            ** Procedure must lead with a special character,
            ** unless no document procedure is configured.
            */
            if ((ch == '!') || (ch == '^') || (ch == '~') ||
                (!(octx->doc_proc)))
            {
              /* Copy the path portion off */
              n = str_length(splitpath) + 1;
              sptr = (char *)morq_alloc(r, n, 0);
              if (!sptr) return(-n); /* Trivial allocation failed! */
              mem_copy(sptr, splitpath, n);

              n = (int)(splitpath - path_info) + 1;

              splitpath = sptr;

              /* Re-do the procedure name */
              sptr = (char *)morq_alloc(r, n, 0);
              if (!sptr) return(-n); /* Trivial allocation failed! */
              --n;
              mem_copy(sptr, path_info, n);
              sptr[n] = '\0';
              path_info = sptr;

#ifdef NEVER
              /* Overwrite the CGI path info separate from the procedure */
              morq_table_put(r, OWA_TABLE_SUBPROC, path_found,
                             (char *)env_path_name, splitpath);
#endif
            }
          }
        }
      }

    /*
    ** Append additional environment variables
    */
    for (n = 0; n < octx->nenvs; ++n, ++i)
        morq_table_put(r, OWA_TABLE_SUBPROC, 0,
                       octx->envvars[n].name, octx->envvars[n].value);

    *nenv = i;

    *spath = (path_info) ? path_info : (char *)"\0\0";

    *nam_max = nwidth;
    *val_max = vwidth;

    return(0);
}

/*
** Transfer environment variables to OCI-compatible buffers
*/
static void transfer_env(request_rec *r, env_record *senv, un_long ipaddr,
                         int nwidth, int vwidth, int nenv, char *outbuf)
{
    char *names, *values;
    char *nptr,  *vptr;
    int   i;

    names = outbuf;
    values = outbuf + (nenv * nwidth);
    senv->count = nenv;
    senv->names = names;
    senv->values = values;
    senv->nwidth = nwidth;
    senv->vwidth = vwidth;
    for (i = 0; i < nenv; ++i)
    {
        morq_table_get(r, OWA_TABLE_SUBPROC, i, &nptr, &vptr);
        str_concat(names, 0, nptr, nwidth - 1);
        str_concat(values, 0, vptr, vwidth - 1);
        names += nwidth;
        values += vwidth;
    }
    senv->ipaddr = ipaddr;
}

/*
** Get arguments from stream
** ### SHOULD HANDLE "chunked" REQUESTS WITHOUT CONTENT LENGTHS ###
*/
static char *get_arguments(owa_context *octx, request_rec *r, long_64 *clen)
{
    char    *args;
    char    *sptr;
    long_64  bsize;
    long_64  i;
    int      n;

    /* ### Possibly use r->remaining for clen ### */
    if (*clen == 0) return("");

    if (*clen > 0)
    {
        if ((octx->upmax > 0) && (*clen > octx->upmax)) *clen = octx->upmax;
        bsize = *clen;
    }
    else /* Content length is unknown */
    {
        bsize = HTBUF_BLOCK_READ - 1;
        DEFAULT_MAX(*clen);
    }
    args = (char *)morq_alloc(r, (size_t)(bsize + 1), 0);
    if (!args) goto geterr;
    for (i = 0; i < *clen; i += n)
    {
        if (i == bsize)
        {
            sptr = args;
            bsize += bsize;
            args = (char *)morq_alloc(r, (size_t)(bsize + 1), 0);
            if (!args) goto geterr;
            mem_copy(args, sptr, i);
        }
        n = morq_read(r, args + i, (long)(bsize - i));
        if (n < 0) break;
    }
    args[i] = '\0';
    *clen = i;
geterr:
    if (!args) *clen = -(bsize + 1);
    return(args);
}

/*
** This section of code saved in case needed later;
** it contains a simplified/unified argument processor that treats
** POST and URI arguments in a symmetric fashion, even if the POST
** is multipart/form-data.
*/
#ifdef NOTUSED
/*
** Reallocate argument name/value pair arrays
*/
static void resize_arrs(request_rec *r, http_args *argrec)
{
    void *mptr;
    int   elsize = sizeof(char *);
    int   currsz = argrec->arrsz;
    int   newsz  = currsz + HTBUF_PARAM_CHUNK;

    mptr = morq_alloc(r, newsz * elsize * 2, 0);
    if (mptr)
    {
        if (currsz > 0)
            mem_copy(mptr, (void *)(argrec->names), currsz * elsize);
        argrec->names = (char **)mptr;
        mptr = (void *)(argrec->names + newsz);
        if (currsz > 0)
            mem_copy(mptr, (void *)(argrec->values), currsz * elsize);
        argrec->values = (char **)mptr;
        argrec->arrsz = newsz;
    }
}

/*
** Parse content data stream into argument vectors (name/value pairs)
*/
static int parse_stream(request_rec *r, owa_context *octx, char *outbuf,
                        char *boundary, long_64 clen, http_args *argrec)
{
    long_64 n;
    int   blen;
    char *aptr;
    char *sptr;
    char *optr;
    char *filpath = (char *)0;
    char *fldname = (char *)0;
    char *ctype = (char *)0;
    file_arg *newfile;
    int   nparams = argrec->nargs;

    blen = str_length(boundary);

    optr = outbuf;
    n = clen + 2;

    while (n > 0)
    {
        sptr = mem_find(optr, n, boundary, blen);
        if (sptr != optr)
          break; /* ### Error - boundary not at beginning */
        if (n < (long_64)blen)
          break; /* ### Error - boundary fragment remaining */

        /* Boundary has been found aligned at beginning of buffer */
        *sptr = '\0';
        sptr += blen;
        if (!str_compare(sptr, "--", 2, 0))
            /* End of content stream, exit loop */
            break;

        if (str_compare(sptr,
                        "\r\nContent-Disposition: form-data;", 33, 1))
            break; /* ### Error - unexpected header element */

        /* Look for trailing double-newline */
        sptr += 33;
        aptr = str_substr(sptr, "\r\n", 0);
        if (!aptr) break; /* ### Error - some sort of format problem */
        *(aptr++) = ';';
        *(aptr++) = '\0';
        while (*sptr == ' ') ++sptr;

        fldname = (char *)0;
        filpath = (char *)0;
        ctype = (char *)0;

        /*
        ** sptr now points to one of the following two forms:
        ** 1.  name="ordinary_field_name";
        ** 2.  name="filefield_name"; filename="filename.ext";
        */
        if (str_compare(sptr, "name=\"", 6, 1))
            break; /* ### Error - format parsing problem */
        sptr += 6;
        fldname = sptr;
        while (*sptr != '\"') ++sptr;
        *sptr = '\0';
        sptr += 2;
        while (*sptr == ' ') ++sptr;
        if (!str_compare(sptr, "filename=\"", 10, 1))
        {
            sptr += 10;
            filpath = sptr;
            while (*sptr != '\"') ++sptr;
            *sptr = '\0';
        }

        sptr = aptr;

        /* Now look for optional Content-type */
        if (!str_compare(sptr, "Content-Type: ", 14, 1))
        {
            aptr = str_substr(sptr, "\r\n", 0);
            if (!aptr) break; /* ### Error - parsing problem */
            *aptr = '\0';
            sptr += 14;
            if (*sptr) ctype = sptr;
            sptr = aptr + 2;
        }

        /* Skip any other header fields until blank line is found */
        while (1)
        {
            aptr = str_substr(sptr, "\r\n", 0);
            if (!aptr) break;
            if (aptr == sptr)
            {
                sptr += 2;
                break;
            }
            sptr = aptr + 2;
        }

        /* Remaining content until next boundary is parameter value */
        n -= (long_64)(sptr - optr);
        optr = sptr;

        sptr = mem_find(optr, n, boundary, blen);
        if (!sptr)
            sptr = optr + n; /* ### boundary not found */
        else if ((sptr - optr) + blen > n)
            sptr = optr + n; /* ### complete boundary not found */

        /* Add to parameter array */
        if (nparams >= argrec->arrsz)
        {
            resize_arrs(r, argrec);
            if (nparams >= argrec->arrsz) return(-1);
        }

        argrec->names[nparams] = fldname;
        argrec->values[nparams] = optr;

        if (filpath)
        {
            /* Save filpath, ctype, and templen */
            newfile = (file_arg *)morq_alloc(r, sizeof(*newfile), 0);
            if (!newfile) return(-((int)sizeof(*newfile)));
            newfile->next = argrec->filelist;
            newfile->filepath = filpath;
            newfile->content_type = ctype;
            newfile->param_num = nparams;
            newfile->len = newfile->total = (size_t)(sptr - optr);
            argrec->filelist = newfile;
        }

        ++nparams;

        /* Skip value and repoint at next boundary */
        n -= (long_64)(sptr - optr);
        optr = sptr;
    }

    argrec->nargs = nparams;
    return(0);
}

/*
** Parse query string or POST content string into name/value pairs
*/
static int parse_string(request_rec *r, char *args, http_args *argrec)
{
    int n;

    /*
    ** Parse argument names and values from the argument string
    */
    while ((args != (char *)0) && (*args != '\0'))
    {
        n = argrec->nargs;
        if (n == argrec->arrsz)
        {
            /* Reallocate arrays */
            resize_arrs(r, argrec);
            if (n >= argrec->arrsz) return(-1); /* Memory allocation error */
        }

        argrec->names[n]  = morq_getword(r, (const char **)&args, '=', 1);
        argrec->values[n] = morq_getword(r, (const char **)&args, '&', 1);

        argrec->nargs = ++n;
    }

    return(0);
}
#endif

/*
** Convert multi-valued arguments to collection form
*/
static int collect_args(request_rec *r, int *nargs, file_arg *filelist,
                        char **param_name, char **param_value,
                        char ***param_array, ub4 *param_count)
{
    int i, j, n, m;
    file_arg *afile;

    /* If there's 0 or 1 argument, nothing to do */
    n = *nargs;
    if (n <= 1) return(0);

    /* Go through all arguments starting with the second argument */
    for (i = 1; i < n; ++i)
    {
      /* Compare its name to all previous arguments */
      for (j = 0; j < i; ++j)
        if (param_name[i])
          if (!str_compare(param_name[i], param_name[j], -1, 1))
            break;

      /* If it matched, then it should be a collection binding */
      if (j < i)
      {
        /* If this is the first duplicate, convert the original to collection */
        if (param_value[j] != (char *)0)
        {
          m = sizeof(char *) * HTBUF_PARAM_CHUNK;
          param_array[j] = (char **)morq_alloc(r, m, 0);
          if (!param_array[j]) return(-m);
          param_array[j][0] = param_value[j];
          param_value[j] = (char *)0;
        }
        /* Otherwise, resize the collection if necessary */
        if ((param_count[j] & (HTBUF_PARAM_CHUNK-1)) == 0)
        {
            int nz = param_count[j];
            param_array[j] = (char **)resize_arr(r, (void *)param_array[j],
                                                 sizeof(char *), nz);
            nz += HTBUF_PARAM_CHUNK;
            if (!param_array[j]) return(-((int)(nz * sizeof(char *))));
        }
        /* Move the duplicate value into the collection */
        param_array[j][param_count[j]++] = param_value[i];
        /* Mark the duplicate as unused */
        param_name[i] = (char *)0;
        /*
        ** ### If it's a file argument, blindly fix up the location.
        ** ### This assumes all duplicate file arguments are sequential.
        */
        if (filelist)
        {
          int basepos = filelist->param_num;
          for (afile = filelist; afile; afile = afile->next)
          {
            if (afile->param_num == i)
            {
              afile->param_num = basepos;
              break;
            }
            basepos = afile->param_num;
          }
        }
      }
    }

    /* Shift values to remove the "holes" */
    for (i = j = 0; i < n; ++i)
    {
        if (param_name[i])
        {
          if (j < i)
          {
            param_name[j] = param_name[i];
            param_value[j] = param_value[i];
            param_array[j] = param_array[i];
            param_count[j] = param_count[i];
            for (afile = filelist; afile; afile = afile->next)
              if (afile->param_num == i) afile->param_num = j;
          }
          ++j;
        }
    }
    *nargs = j;

    return(0);
}

/*
** Append a body as a file-like parameter
*/
static int append_body(request_rec *r, int dad_csid,
                       char *post_body, int deferred_read,
                       char *content_type, long_64 clen, char *rest_path,
                       int *arrsz, int *nargs,
                       char ***pnames, char ***pvalues, file_arg **file_list)
{
    file_arg *newfile;
    file_arg *prevfile;
    int       nparams = *nargs;

    /* Add a string argument to convey the REST/DAV path (if any) */
    if (rest_path)
    {
      int i = str_length(rest_path);

      if (nparams >= *arrsz)
      {
        *pnames = (char **)resize_arr(r, (void *)(*pnames),
                                      sizeof(char *), nparams);
        *pvalues = (char **)resize_arr(r, (void *)(*pvalues),
                                       sizeof(char *), nparams);
        if (!(*pnames) || !(*pvalues)) return(-1);
        *arrsz += HTBUF_PARAM_CHUNK;
      }

      (*pnames)[nparams] = "MODOWA$REST_PATH";

      (*pvalues)[nparams] = (char *)morq_alloc(r, i + 1, 0);
      if (!(*pvalues)[nparams]) return(-(i + 1));
      mem_copy((*pvalues)[nparams], rest_path, i);
      (*pvalues)[nparams][i] = '\0';

      ++nparams;
    }

    /* Add a file argument for any content body */
    if (post_body)
    {
      int k;

      k = (content_type) ? str_length(content_type) : 0;
      k += (sizeof(*newfile) + 1);
      newfile = (file_arg *)morq_alloc(r, k, 0);
      if (!newfile) return(-k);

      prevfile = *file_list;

      if (prevfile)  prevfile->next = newfile;
      else          *file_list = newfile;

      newfile->next = (file_arg *)0;
      newfile->filepath = "rest_body";
      newfile->content_type = (char *)(void *)(newfile + 1);
      if (content_type)
        str_copy(newfile->content_type, content_type);
      else
        newfile->content_type = "";

      newfile->param_num = nparams;
      newfile->total = (size_t)clen;
      newfile->len = (deferred_read) ? 0 : newfile->total;
      newfile->data = (void *)post_body;

      if (nparams >= *arrsz)
      {
        *pnames = (char **)resize_arr(r, (void *)(*pnames),
                                      sizeof(char *), nparams);
        *pvalues = (char **)resize_arr(r, (void *)(*pvalues),
                                       sizeof(char *), nparams);
        if (!(*pnames) || !(*pvalues)) return(-1);
        *arrsz += HTBUF_PARAM_CHUNK;
      }

      /* Fake field name (must be PL/SQL compatible) */
      (*pnames)[nparams] = "MODOWA$CONTENT_BODY";

      /* Mark files with empty_string as a flag */
      (*pvalues)[nparams] = empty_string;

      ++nparams;
    }

    *nargs = nparams;

    return(0);
}

/*
** Parse content data stream into argument vectors (name/value pairs)
*/
static int parse_args(request_rec *r, owa_context *octx, int call_mode,
                      char *boundary, long_64 clen, int *arrsz, int *nargs,
                      char ***pnames, char ***pvalues, file_arg **file_list)
{
    int            dad_csid = octx->dad_csid;
    int            strip_mode = ((octx->doc_table) || (octx->doc_column));
    int            j, n;
    int            blen;
    long_64        bufsz;
    long_64        offset;
    char          *aptr;
    char          *sptr;
    char          *outbuf;
    char          *ctype;
    char          *filpath;
    char          *fldname;
    int            nparams = *nargs;
    file_arg      *newfile = (file_arg *)0;
    file_arg      *prevfile = (file_arg *)0;

    blen = str_length(boundary);

    bufsz = (clen > 0) ? clen : (long_64)HTBUF_BLOCK_SIZE;
    bufsz += 2;

    /* Force streaming read of chunked data */
    DEFAULT_MAX(clen);

    morq_stream(r, 0);

    /*
    ** This memory must remain stable for the request;
    ** internally the parser will retain pointers to sub-strings within it,
    ** after null-terminating them.
    */
    outbuf = morq_alloc(r, (size_t)(bufsz + 1), 0);

    str_copy(outbuf, "\r\n");
    offset = (long_64)2;

    /* Force all data to be read into memory */
    while (1)
    {
      long_64 blksz = (long_64)HTBUF_BLOCK_SIZE;

      if (blksz > clen) blksz = clen;

      if (blksz == 0) break;

      /* Grow the buffer if necessary */
      if ((offset + blksz) > bufsz)
      {
        char *temp = outbuf;

        while (bufsz < (offset + blksz)) bufsz *= 2; /* Resize buffer */
        outbuf = morq_alloc(r, (size_t)(bufsz + 1), 0);
        if (offset > 0) mem_copy(outbuf, temp, offset);

        /* Old memory block is lost until request terminated */
      }

      n = morq_read(r, outbuf + offset, (long)blksz);
      if (n < 0) break;

      clen   -= (long_64)n;
      offset += (long_64)n;
    }
    outbuf[offset] = '\0'; /* Null terminate so string operations work */
    bufsz = offset;        /* Last offset == total buffer size         */
    offset = 2;            /* Start right after \r\n pair              */

    while (bufsz > 0)
    {
        sptr = mem_find(outbuf, bufsz, boundary, blen);
        if (sptr != outbuf)
          break; /* ### Error - boundary not at beginning */
        if (bufsz < (long_64)blen)
          break; /* ### Error - boundary fragment remaining */

        sptr += blen;
        if (!str_compare(sptr, "--", 2, 0))
          /* End of content stream, exit loop */
          break;

        if (str_compare(sptr, "\r\nContent-Disposition: form-data;", 33, 1))
          break; /* Some sort of streaming error ### */

        /* Look for trailing double-newline */
        sptr += 33;
        aptr = str_substr(sptr, "\r\n", 0);
        if (!aptr) break; /* Another class of streaming error ### */
        *(aptr++) = ';';
        *(aptr++) = '\0';
        while (*sptr == ' ') ++sptr;
        j = (int)(aptr - sptr);
        fldname = (char *)morq_alloc(r, j, 0);
        if (!fldname) return(-j);
        filpath = (char *)0;
        ctype = (char *)0;

        j = (int)(aptr - outbuf);

        /*
        ** sptr now points to one of the following two forms:
        ** 1.  name="ordinary_field_name";
        ** 2.  name="filefield_name"; filename="filename.ext";
        */
        if (str_compare(sptr, "name=\"", 6, 1))
          break; /* Stream parsing error ### */
        sptr += 6;
        aptr = fldname;
        while (*sptr != '\"') *(aptr++) = *(sptr++);
        *(aptr++) = '\0'; /* Null terminates string <fldname> */
        sptr += 2;
        /* In Apex mode, truncate argument names that contain a "." */
        if (strip_mode)
        {
            char *dotptr = str_char(fldname, '.', 0);
            if (dotptr) *dotptr = '\0';
        }
        while (*sptr == ' ') ++sptr;
        if (!str_compare(sptr, "filename=\"", 10, 1))
        {
          sptr += 10;
          filpath = aptr;
          while (*sptr != '\"') *(aptr++) = *(sptr++);
          *aptr = '\0'; /* Null terminates string <filpath> */
        }
        /* Now look for optional Content-type */
        sptr = outbuf + j;
        if (!str_compare(sptr, "Content-Type: ", 14, 1))
        {
          aptr = str_substr(sptr, "\r\n", 0);
          if (!aptr) break; /* Another streaming error ### */
          *aptr = '\0'; /* Null terminates string <ctype> */
          j += (int)(aptr - sptr);
          sptr += 14;
          if (*sptr) ctype = sptr;
          sptr = aptr + 2;
          j += 2;
        }

        /* Skip any other header fields until blank line is found */
        while (1)
        {
          aptr = str_substr(sptr, "\r\n", 0);
          if (!aptr) break;
          j += 2;
          if (aptr == sptr) break;
          j += (int)(aptr - sptr);
          sptr = aptr + 2;
        }

        sptr = outbuf + j;

        /*
        ** Data until next boundary is content for this field
        */
        aptr = mem_find(sptr, bufsz - (long_64)j, boundary, blen);
        if (!aptr) break; /* Error - field item too large */

        /* Temporarily offset is the distance between pointers */
        offset = (long_64)(aptr - sptr);

        if (filpath)
        {
          /*
          ** ### Save filpath, ctype, and templen
          */
          n = (ctype) ? str_length(ctype) : 0;
          n += (int)(sizeof(*newfile) + 1);
          newfile = (file_arg *)morq_alloc(r, n, 0);
          if (!newfile) return(-n);
          if (prevfile)  prevfile->next = newfile;
          else          *file_list = newfile;
          prevfile = newfile;
          newfile->next = (file_arg *)0;
          newfile->filepath = filpath;
          newfile->content_type = (char *)(void *)(newfile + 1);
          if (ctype) str_copy(newfile->content_type, ctype);
          else newfile->content_type = "";
          newfile->param_num = nparams;
          newfile->len = newfile->total = (size_t)offset;
          newfile->data = (void *)0;
        }

        /* If not a file, and field width > limit, chunk the data */
        if ((!filpath) &&
            (offset >= ((call_mode) ? (long_64)HTBUF_ENV_MAX :
                                      (long_64)HTBUF_PLSQL_MAX)))
        {
          long_64  ncbytes = 0;
          int      nchunks = 0;
          int      chunklen;
          char    *pname, *pchunk;
          int      fnamlen = str_length(fldname);

          while (ncbytes < offset)
          {
            /*
            ** Allocate and construct parameter name
            ** of the form "ARG.1", "ARG.2", etc.
            */
            pname = (char *)morq_alloc(r, fnamlen + 10, 0);
            if (!pname) return(-(fnamlen + 10));
            mem_copy(pname, fldname, fnamlen);
            ++nchunks;
            if (call_mode == 0)
              pname[fnamlen] = '\0';
            else
            {
              pname[fnamlen] = '.';
              str_itoa(nchunks, pname + fnamlen + 1);
            }

            /*
            ** Compute size of next chunk, allocate,
            ** and copy data into null-terminated string.
            */
            if ((offset - ncbytes) < (long_64)HTBUF_ENV_MAX)
              chunklen = (int)(offset - ncbytes);
            else
              chunklen = nls_length(dad_csid, sptr + ncbytes,
                                    HTBUF_ENV_MAX - 1);

            pchunk = (char *)morq_alloc(r, chunklen + 1, 0);
            if (!pchunk) return(-(chunklen + 1));
            mem_copy(pchunk, sptr + ncbytes, chunklen);
            pchunk[chunklen] = '\0';
            ncbytes += (long_64)chunklen;

            if (nparams >= *arrsz)
            {
              *pnames = (char **)resize_arr(r, (void *)(*pnames),
                                            sizeof(char *), nparams);
              *pvalues = (char **)resize_arr(r, (void *)(*pvalues),
                                             sizeof(char *), nparams);
              if (!(*pnames) || !(*pvalues)) return(-1);
              *arrsz += HTBUF_PARAM_CHUNK;
            }

            (*pnames)[nparams] = pname;
            (*pvalues)[nparams] = pchunk;
            ++nparams;
          }
        }
        /* Else add simple scalar parameter */
        else
        {
          if (nparams >= *arrsz)
          {
            *pnames = (char **)resize_arr(r, (void *)(*pnames),
                                          sizeof(char *), nparams);
            *pvalues = (char **)resize_arr(r, (void *)(*pvalues),
                                           sizeof(char *), nparams);
            if (!(*pnames) || !(*pvalues)) return(-1);
            *arrsz += HTBUF_PARAM_CHUNK;
          }

          (*pnames)[nparams] = fldname;
          if (filpath)
          {
            /* Mark files with empty_string as a flag */
            (*pvalues)[nparams] = empty_string;
            /* Transfer the file content */
            if (newfile)
            {
              newfile->data = morq_alloc(r, (size_t)offset, 0);
              if (!(newfile->data)) return(-((int)offset));
              mem_copy(newfile->data, sptr, (size_t)offset);
            }
          }
          else
          {
            (*pvalues)[nparams] = (char *)morq_alloc(r, (size_t)(offset + 1), 0);
            if (!(*pvalues)[nparams]) return(-((int)(offset + 1)));
            mem_copy((*pvalues)[nparams], sptr, (size_t)offset);
            (*pvalues)[nparams][offset] = '\0';
          }
          ++nparams;
        }
        sptr = aptr;

        /* Shift buffer and continue */
        offset = (long_64)(sptr - outbuf);
        bufsz -= offset;
        outbuf = sptr;
    }

    *nargs = nparams;
    return(0);
}

static void set_wdb_cookie(request_rec *r, int is_logout)
{
  morq_table_put(r, OWA_TABLE_HEADOUT, 1, "Set-Cookie",
                 (is_logout) ? "WDB_GATEWAY_LOGOUT=YES; PATH=/;" :
                               "WDB_GATEWAY_LOGOUT=NO; PATH=/;");
}

static char *get_wpg_field(char **pbuf)
{
  char *pstr = *pbuf;
  char *fstart, *fend;
  int   flen = -1;

  *pbuf = (char *)0;

  /* Find the marker at the end of the length portion */
  fstart = str_char(pstr, 'X', 0);
  if (!fstart) return(pstr);

  /* Extract the length */
  *(fstart++) = '\0';
  if (*pstr)
  {
    /* If a length is available */
    flen = str_atoi(pstr);
    if (flen >= 0)
    {
      fend = fstart + flen;
      if (*fend == 'X')
      {
        *fend = '\0';
        *pbuf = ++fend;
      }
      return(fstart);
    }
  }
  
  fend = str_char(fstart, 'X', 0);
  if (fend)
  {
    *fend = '\0';
    *pbuf = ++fend;
  }
  return(fstart);
}

/*
** Handle user request
*/
int owa_handle_request(owa_context *octx, request_rec *r,
                       char *req_args, int req_method, owa_request *owa_req)
{
    connection    cdefault;
    connection   *c;
    int           diagflag;
    int           control_flag = 0;
    int           cache_flag = 0;
    int           call_mode;
    int           i, j, k, m, n;
    int           sphase;
    int           retrycount;
    long_64       clen;
    int           stmtlen;
    int           status, cstatus;
    int           pstatus = OCI_SUCCESS;
    int           rstatus = OK;
    int           nwidth = 0;
    int           vwidth = 0;
    int           nenv = 0;
    int           nargs;
    int           xargs;
    int           fargs;
    int           zero_args = 0;
    int           osize;
    int           esize;
    int           file_flag;
    int           long_flag;
    int           wpg_flag = 0;
    int           rset_flag = 0;
    char         *raw_post = (char *)0;
    int           gen_flag;
    int           bound_flag = 0;
    int           realm_flag = 0;
    int           nopool_flag = 0;
    int           desc_mode;
#ifndef NO_FILE_CACHE
    ub4           life;
    ub4           csum, clife;
#endif
    un_long       remote_addr;
    char          pidstr[LONG_MAXSTRLEN];
    char          bname[LONG_MAXSTRLEN];
    char          pcount[LONG_MAXSTRLEN];
    char         *outbuf;
    char         *stmt = (char *)0;
    char         *spath;
    char         *fpath;
    char         *args;
    char         *sptr = "";
    char         *aptr;
    char         *session = (char *)0;
    char         *authuser = (char *)0;
    char         *authpass = (char *)0;
    char         *boundary = (char *)0;
    char         *physical = (char *)0;
    int           spflag = 2;
    env_record    senv;
    char          stmtbuf[HTBUF_HEADER_MAX];
    char         *ptrbuf[HTBUF_PARAM_CHUNK*3];
    ub4           uintbuf[HTBUF_PARAM_CHUNK];
    sb4           sintbuf[HTBUF_PARAM_CHUNK];
    int           arrsz = HTBUF_PARAM_CHUNK;
    char       ***param_array = (char ***)(void *)ptrbuf;
    char        **param_name  = ptrbuf + HTBUF_PARAM_CHUNK;
    char        **param_value = ptrbuf + (HTBUF_PARAM_CHUNK * 2);
    ub4          *param_count = uintbuf;
    sb4          *param_width = sintbuf;
    char         *param_temp; /* For flexible argument mode */
    char        **param_ptrs = (char **)0;
    ub2          *param_lens = (ub2 *)0;
    char          pmimetype[HTBUF_HEADER_MAX];
    char          prawchar[LONG_MAXSTRLEN];
    char          pdocload[HTBUF_HEADER_MAX];
    char          pcdisp[HTBUF_HEADER_MAX];
    char          prealm[HTBUF_LINE_LENGTH];
    char          errbuf[ERRBUF_SIZE];
    char          emptystr[2];
    char         *arg_strs[2];
    int           adx;
    int           nadx = 0;
    char        **pnames = (char **)0;
    char        **pvalues = (char **)0;
    int           cs_id;
    int           makeutf = 0;
    int           wdb_realm_logout = 0;
    int           append_timestamp = 0;
    long_64       stime;
    descstruct   *dptr;
    char         *ctype = (char *)0;
    char         *post_body = (char *)0;
    char         *rest_path = (char *)0;
    file_arg     *filelist = (file_arg *)0;
    int           media_post = 0;
    int           deferred_read = 0;
    sb4           errinfo = 0;

#ifdef NEVER /* ### THIS IS THE DEFAULT, RIGHT? ### */
    morq_set_status(r, HTTP_OK, (char *)0);
#endif

    stime = util_component_to_stamp(os_get_component_time(0));

    owa_setup(octx, r);

    *pmimetype = '\0';
    *prawchar = '\0';

    diagflag = octx->diagflag;

    get_pid_tid_str(octx, pidstr);
    if (diagflag & ~(DIAG_SQL | DIAG_MEMORY | DIAG_THREADS))
    {
        debug_time(octx);
        debug_out(octx->diagfile,
                  "OWA request, PID is %s\n", pidstr, (char *)0, 0, 0);
    }

    /* If logging is enabled, set up the logging buffer */
    if (octx->altflags & ALT_MODE_LOGGING)
    {
        m = HTBUF_LINE_LENGTH * HTBUF_ARRAY_SIZE;
        owa_req->logbuffer = (char *)morq_alloc(r, m, 0);
        if (!owa_req->logbuffer) return(mem_error(r, m, diagflag));
    }

    /*
    ** Scan subprocess environment to get specific values needed
    ** by this routine.  Measure the maximum name and value widths
    ** so that later we can allocate the bind arrays.  Set the
    ** REQUEST_IANA_CHARSET, too.
    */
    m = search_env(octx, r, &spath, &clen, &ctype, &boundary, &remote_addr,
                   &nwidth, &vwidth, &nenv, &authuser, &authpass, pmimetype,
                   &wdb_realm_logout);
    if (m < 0) return(mem_error(r, -m, diagflag));
    file_flag = (boundary != (char *)0);

    if (*pmimetype)
    {
        session = morq_alloc(r, str_length(pmimetype) + 1, 0);
        if (session) str_copy(session, pmimetype);
        *pmimetype = '\0';
    }

    if (diagflag & ~(DIAG_SQL | DIAG_MEMORY | DIAG_THREADS))
        debug_out(octx->diagfile, "OWA request [%s]\n",
                  spath, (char *)0, 0, 0);

    long_flag = 0;
    gen_flag = 0;
    fpath = (char *)0;

    /*
    ** Check for document download case
    */
    if (!file_flag)
    {
      /* If the path looks like a REST or file-system path */
      if (*spath == '/')
      {
        /* If there's a REST/DAV handler, treat the request as such */
        if (octx->dav_handler)
        {
            /* ### Note: this mode only works with OwaDocTable set! ### */
            sptr = octx->dav_handler;
            if (*sptr == '@')
            {
                /* Optionally, call it in the REF cursor mode */
                rset_flag = 1;
                ++sptr;
            }
            rest_path = spath;
            call_mode = 1; /* Call mode is flexible arguments */
        }
        /* Else if there's a document procedure, run the file path logic */
        else if (octx->doc_proc)
        {
            sptr = spath + 1;
            aptr = octx->doc_path;
            if (!aptr) aptr = empty_string;
            else if (*aptr == '/') ++aptr;
            n = str_length(aptr);
            if (n > 0)
            {
                if (!str_compare(sptr, aptr, n, 0))
                    if ((sptr[n] == '/') || (sptr[n] == '\0'))
                        /* Set fpath to document path name */
                        fpath = spath;
            }
            else if (aptr != empty_string)
            {
                /* In cases where document_path == "/", consume all requests */
                fpath = spath;
            }

            aptr = octx->doc_long;
            if (!aptr) aptr = empty_string;
            else if (*aptr == '/') ++aptr;
            n = str_length(aptr);
            if (n > 0)
            {
                if (!str_compare(sptr, aptr, n, 0))
                    if ((sptr[n] == '/') || (sptr[n] == '\0'))
                    {
                        /* Set fpath to document path name */
                        fpath = spath;
                        long_flag = 1;
                    }
            }
            else if (aptr != empty_string)
            {
                /* In cases where document_path == "/", consume all requests */
                fpath = spath;
                long_flag = 1;
            }

#ifndef NO_FILE_CACHE
            if ((long_flag) && (octx->doc_file)) ++long_flag;
#endif

            aptr = octx->doc_gen;
            if (!aptr) aptr = (char *)"";
            else if (*aptr == '/') ++aptr;
            n = str_length(aptr);
            if (n > 0)
                if (!str_compare(sptr, aptr, n, 0))
                    if ((sptr[n] == '/') || (sptr[n] == '\0'))
                    {
                        /* Set fpath to document path name */
                        fpath = spath;
                        gen_flag = 1;
                        long_flag = 0;
                    }
        }
      }
    }

    if (fpath)
    {
        sptr = octx->doc_proc;
        /* Skip leading '*' if present */
        if (sptr)
          if (*sptr == '*')
            ++sptr;
    }
    else if (octx->ora_proc)
    {
        sptr = octx->ora_proc;
    }
    else if (!rest_path)
    {
        /*
        ** Interpret the path as a function name
        */
        sptr = str_char(spath, '/', 1);
        sptr = (sptr) ? sptr + 1 : spath;
    }

    /*
    ** No path specified, so call the start page or document root
    */
    if ((!fpath) && (*sptr == '\0'))
    {
        if (octx->doc_start)
        {
            sptr = octx->doc_start;
        }
        else if (octx->doc_proc)
        {
            sptr = octx->doc_proc;
            /* Skip leading '*' if present */
            if (sptr)
              if (*sptr == '*')
                ++sptr;
            fpath = spath;
        }
    }
    spath = sptr;

    if (*spath == '\0') return(HTTP_BAD_REQUEST);

    /* If this is a blind request */
    if (sptr == octx->doc_start)
    {
      /* Issue a redirect to the start page */
      if (*sptr == '!')
      {
        str_copy(pmimetype, octx->location);
        i = str_length(pmimetype);
        if (i > 0)
          if (pmimetype[i - 1] == '/')
            --i;
        pmimetype[i++] = '/';
        str_copy(pmimetype + i, sptr + 1);
        nls_sanitize_header(pmimetype);
        morq_table_put(r, OWA_TABLE_HEADERR, 0, "Location", pmimetype);
        morq_set_status(r, HTTP_MOVED_TEMPORARILY, (char *)0);
        return(rstatus);
      }
      /* Otherwise force PATH_INFO to match OwaStart */
      else
      {
        char  doc_start[HTBUF_HEADER_MAX];
        int   reset = morq_get_env(r, (char *)env_path_name) ? 1 : 0;
        int   wd;

        if (*sptr != '/')
        {
          wd = str_length(sptr);
          if (wd > (HTBUF_HEADER_MAX - 2)) wd = HTBUF_HEADER_MAX - 2;
          *doc_start = '/';
          mem_copy(doc_start + 1, sptr, (size_t)wd);
          doc_start[wd + 1] = '\0';
          sptr = doc_start;
        }

        morq_table_put(r, OWA_TABLE_SUBPROC, reset, (char *)env_path_name, sptr);
        wd = str_length((char *)env_path_name);
        if (wd > nwidth) nwidth = wd;
        wd = str_length(sptr);
        if (wd > vwidth) vwidth = wd;
        if (!reset) ++nenv;
      }
    }

    /* Check for disallowed patterns */
    for (i = 0; i < octx->nreject; ++i)
    {
      char *dot = str_char(octx->rejectprocs[i], '.', 0);

      /* If the rejection string contains a suffix, use an exact match */
      if ((dot) && (dot[1] != '\0'))
      {
        if (str_compare(spath, octx->rejectprocs[i], -1, 1) == 0)
          return(HTTP_BAD_REQUEST);
      }
      /* Else treat the rejection string as a prefix using starts-with match */
      else
      {
        if (str_substr(spath, octx->rejectprocs[i], 1) == spath)
          return(HTTP_BAD_REQUEST);
      }
    }

    if ((fpath) && (diagflag & ~(DIAG_SQL | DIAG_MEMORY | DIAG_THREADS)))
        debug_out(octx->diagfile, "File URI = [%s]\n", fpath, (char *)0, 0, 0);

    /*
    ** ! as first function name character means argc/argv calling
    ** ^ as first function name character means positional calling
    **
    ** ~ as first function name character means long-mode for file upload
    */
    call_mode = 0; /* Default: named arguments */
    if (spath == octx->ora_proc)
        call_mode = 1;
    else if (*spath == '!')
    {
        /*
        ** Call as:
        ** <function>(:num_entries, :name_array, :value_array, :reserved)
        */
        call_mode = 1;
        ++spath;
        goto checkflex; /* Check for 2-argument mode */
    }
    else if (*spath == '^')
    {
        /* Call with positional arguments */
        call_mode = 2;
        ++spath;
    }
    else if (*spath == '~')
    {
        /* Named-argument mode, with long_flag */
#ifndef NO_FILE_CACHE
        if (file_flag) long_flag = (octx->doc_file) ? 2 : 1;
#else
        if (file_flag) long_flag = 1;
#endif
        else           raw_post = (char *)"";
        ++spath;
    }
    else
    {
checkflex:
        /* Normal call, unless flexarg check matches */
        for (i = 0; i < octx->nflex; ++i)
        {
            aptr = octx->flexprocs[i];
            if (*aptr == '~')
              ++aptr; /* Flags 2-argument flexarg mode */
            else if (*aptr == '@')
              ++aptr; /* Flags ref-cursor return mode */
            if (str_compare(aptr, spath, -1, 1) == 0)
            {
                if (aptr == octx->flexprocs[i])
                    call_mode = 1;
                else if (*(octx->flexprocs[i]) == '@')
                {
                    call_mode = 1;
                    rset_flag = 1;
                }
                else
                    call_mode = 3;
                break;
            }
        }
        /* ### Check the REF cursor list here? ### */
        /* Also, check the describe cache */
        if (call_mode != 3)
        {
            for (dptr = octx->desc_cache;
                 dptr != (descstruct *)0;
                 dptr = dptr->next)
            {
                if (str_compare(dptr->pname, spath, -1, 1) == 0)
                {
                    if (dptr->flex2)
                    {
                        call_mode = 3;
                        break;
                    }
                }
            }
        }
        /* Convert multipart/form-data calls if necessary */
        if (file_flag)
        {
          if ((octx->doc_table) || (octx->doc_column) ||
              (call_mode == 1) || (call_mode == 3))
          {
            /*
            ** This is a flex-args call using multipart/form-data
            ** as the content type, or we are trying to support Apex
            ** with named-argument mode.  This can't use the file-upload
            ** code path.  Instead, the arguments have to be
            ** extracted using the boundary-scanner and used normally.
            */
            bound_flag = 1;
            file_flag = 0;
          }
        }
    }

    /*
    ** If this is not a normal argument-bearing body, it's flagged as "media"
    */
    if (ctype != NULL)
      if (str_compare(ctype, "application/x-www-form-urlencoded", -1, 0))
      {
        media_post = 1;

        /*
        ** If this is a REST request with a media type that doesn't
        ** carry arguments destined for PL/SQL, and the content length
        ** is large, set a flag deferring the the content read.
        */
        if ((rest_path) && (!raw_post) && (!bound_flag))
          if (clen > CACHE_MAX_SIZE)
            deferred_read = 1;
      }

    /*
    ** Find the argument vector based on the method type
    */
    args = (char *)0;
    if (file_flag)
    {
        /*
        ** For file uploads, always use the flexible argument mode
        ** and argument arrays will be built by the stream scanner
        */
        call_mode = 1;
        if (octx->altflags & ALT_MODE_NOMERGE) req_args = (char *)0;
    }
    else if (bound_flag)
    {
        if (octx->altflags & ALT_MODE_NOMERGE) req_args = (char *)0;
    }
    else if (req_method == DAV_METHOD_POST)
    {
        /* Retrieve the body */
        morq_stream(r, 0);
        if (deferred_read) args = "";
        else               args = get_arguments(octx, r, &clen);
        if (!args) return(mem_error(r, (int)(-clen), diagflag));
        if (octx->altflags & ALT_MODE_NOMERGE) req_args = (char *)0;
    }
    else if (req_method == DAV_METHOD_GET)
    {
        /* Discard any body */
        morq_stream(r, 1);
    }
    else if ((req_method == DAV_METHOD_PUT)    ||
             (req_method == DAV_METHOD_DELETE) ||
             (req_method == DAV_METHOD_PATCH))
    {
        /* PUT/DELETE/PATCH allowed only if configured for REST operations */
        if (octx->dav_mode < 1) return(HTTP_NOT_IMPLEMENTED);

        /* Treat the body in the same manner as a POST */
        morq_stream(r, 0);

        if (deferred_read) args = "";
        else               args = get_arguments(octx, r, &clen);

        if (!args) return(mem_error(r, (int)(-clen), diagflag));
    }
    else if (octx->dav_mode < 2) /* Unsupported request method */
    {
        return(HTTP_NOT_IMPLEMENTED);
    }
    else /* DAV request */
    {
        /* Treat the body in the same manner as a POST */
        morq_stream(r, 0);
        args = get_arguments(octx, r, &clen);
        if (!args) return(mem_error(r, (int)(-clen), diagflag));
    }
    if (spath == octx->doc_start)
        args = (char *)""; /* No arguments to start page */

    stmtlen = str_length(spath);
    if (stmtlen > 0) control_flag = (spath[stmtlen - 1] == '!');

    /*
    ** ### NOTE:
    ** ### There's a problem here because the first time a Location is
    ** ### used, we need to validate the path at a point in time prior
    ** ### to any database connect.  This means that the NLS_LANG
    ** ### character set won't be known, unless the administrator
    ** ### supplied a value in the oracle_nls directive.  Check for
    ** ### this condition, and attempt to get a character set ID based
    ** ### on the oracle_nls if necessary and possible.
    */
    if ((octx->ora_csid > 0) || (octx->nls_cs[0] == '\0'))
        cs_id = nls_csid(octx->ora_csid);
    else
    {
        i = nls_csx_from_oracle(octx->nls_cs);
        cs_id = nls_csid((i < 0) ? octx->ora_csid : i);
    }

    if (!control_flag) nls_validate_path(spath, cs_id);

    if (diagflag & DIAG_COMMAND)
        debug_out(octx->diagfile, "Command: %s\n", spath, (char *)0, 0, 0);
    if ((args) && (diagflag & DIAG_ARGS))
        debug_out(octx->diagfile, "Arguments\n", (char *)0, (char *)0, 0, 0);

    /*
    ** Count space needed for call to PL/SQL function
    **
    **   begin
    **     <set htp.htbuf_len>
    **     if <authfunction> then
    **       <before_procedure>
    **       <path>(<param_name>=<bindvar>[,...]);
    **       <after_procedure>
    **       commit;
    **     else
    **       :realm := OWA.PROTECTION_REALM;
    **     end if;
    **   end;
    */
    stmtlen *= 2;    /* Count path_length X 2 to allow for PACKAGE auth */
    stmtlen += 250;  /* Space for "begin" etc.                          */

    if ((octx->doc_table) || (octx->doc_column))
    {
        /*
        ** Add this after the procedure:
        **
        ** if (WPG_DOCLOAD.IS_FILE_DOWNLOAD) then
        **   WPG_DOCLOAD.GET_DOWNLOAD_FILE(:p_doc_info);
        **   WPG_DOCLOAD.GET_DOWNLOAD_BLOB(:p_blob);
        ** end if;
        */
        stmtlen += 200;
    }

    /* Control flag set, interpret command and execute */
    if (control_flag)
      return(handle_control(octx,r,spath,req_args,pidstr,remote_addr,errbuf));

    xargs = 0;
    nargs = 0;
    if (fpath)
    {
        if ((octx->doc_table) && (octx->doc_column))
        {
            /*
            ** Document download via the old WebDB table/column interface.
            ** In this case, build a no-arguments call to the document
            ** procedure and rely on the WPG_DOCLOAD interface.
            */
            call_mode = 0;
            wpg_flag = 1;
        }
        else
        {
            /*
            ** Read LOB always uses the flexible argument mode
            ** First argument is the document name passed to this routine
            */
            call_mode = 1;
            param_value[nargs] = fpath;
            param_name[nargs] = (char *)"document_name";
            param_count[nargs] = 1;
            stmtlen += 50;
            ++nargs;
        }
    }

    /*
    ** If a raw transfer of the POST data is requested,
    ** bypass argument processing
    */
    if (raw_post)
    {
        raw_post = args;
        args = (char *)0;
    }

    /*
    ** Merge request args (if any) with POST args (if any)
    **
    ** ### Note that request arguments in a URL may be encoded with
    ** ### another character set.  The standard calls for iso-8859-1,
    ** ### though the browsers typically use utf-8.  Under some
    ** ### circumstances, though, the browsers encode URLs with the
    ** ### page character set, especially if the GET method is used
    ** ### for a form submission.  And under some conditions, they use
    ** ### the local character set you selected in the browser
    ** ### configuration panels.  Finally, Microsoft IE encodes some
    ** ### values as UCS2 with %uXXXX, which it's not clear Apache
    ** ### will properly unescape.  It's a mess!  Anyway, this code
    ** ### just assumes that the escaping is in the page character
    ** ### set, and that Apache's native unescape functions will
    ** ### work properly.  This should be true of most URLs generated
    ** ### by form requests; for other links on a page, applications
    ** ### should probably escape them on the server when generating
    ** ### them so that they consistently use a valid character set.
    */
    arg_strs[0] = req_args;
    arg_strs[1] = args;

    /*
    ** Note that if the request body is not the URL-encoded content type
    ** it can't be parsed in the manner of the code below. Instead, the
    ** body needs to be treated as a file-like argument.
    */
    if ((args) && (!bound_flag) && (!raw_post))
      /* ### Possibly should check ctype even if (dav_mode == 0)? ### */
      if ((octx->dav_mode > 0) && (media_post))
      {
        arg_strs[1] = (char *)0;
        if (clen > 0)
          post_body = args;
        /* ### For now, only the OwaDocTable mode can handle the payload ### */
      }

    /* Save these argument vectors for later */
    owa_req->req_args = req_args;
    owa_req->post_args = args;

    /*
    ** Parse argument names and values from the argument string
    */
    for (adx = 0; adx < 2; ++adx)
    {
      args = arg_strs[adx];
      while ((args != (char *)0) && (*args != '\0'))
      {
        aptr = morq_getword(r, (const char **)&args, '=', 1);

        if ((call_mode == 1) || (call_mode == 3))
        {
            /* Flexible argument passing doesn't match argument names */
            n = nargs;
            /* ### A DIFFERENCE VERSUS WEBDB: LEAVES THE .x OR .y ### */
        }
        else
        {
            /* Truncate argument names that contain a "." */
            sptr = str_char(aptr, '.', 0);
            if (sptr) *sptr = '\0';

/*
** ### In theory we could allow these because the parameter name will be
** ### sanitized by nls_copy_identifier, which would surround it with
** ### double-quotes if it contains any questionable characters.
*/
#ifndef ALLOW_EXTENDED_PARAMETER_NAMES
            /*
            ** Prevent parameter name from being a code injection vector;
            ** force name to conform to simple set of allowed characters.
            */
            if (call_mode == 0) nls_validate_path(aptr, cs_id);
#endif

            /* Look for a parameter name to match this one */
            for (n = 0; n < nargs; ++n)
                if (!str_compare(param_name[n], aptr, -1, 1))
                    break;
        }
        if (n == nargs)
        {
            if (nargs == arrsz)
            {
                /* Reallocate arrays */
                arrsz += HTBUF_PARAM_CHUNK;

                param_name = (char **)resize_arr(r, (void *)param_name,
                                                 sizeof(char *), nargs);
                param_value = (char **)resize_arr(r, (void *)param_value,
                                                  sizeof(char *), nargs);
                if ((!param_name) || (!param_value))
                  return(mem_error(r, arrsz * sizeof(char *), diagflag));

                if ((call_mode != 1) && (call_mode != 3))
                {
                    param_array = (char ***)resize_arr(r, (void *)param_array,
                                                       sizeof(char **), nargs);
                    if (!param_array)
                      return(mem_error(r, arrsz * sizeof(char **), diagflag));
                    param_count = (ub4 *)resize_arr(r, (void *)param_count,
                                                    sizeof(ub4), nargs);
                    if (!param_count)
                      return(mem_error(r, arrsz * sizeof(ub4), diagflag));
                }
            }
            param_name[n] = aptr;
        }

        aptr = morq_getword(r, (const char **)&args, '&', 1);

        if ((call_mode == 1) || (call_mode == 3))
        {
            m = str_length(aptr);
            if (m > HTBUF_ARRAY_MAX_WIDTH)
            {
                /*
                ** In flexible argument mode, split large blocks into
                ** 4K chunks to get around a limitation of OCI.  The
                ** parameter names get suffixes of the form ".<chunknum>",
                ** e.g. parameter "data" => "data.1", "data.2", etc.
                */
                stmtlen += 10; /* Enough for bind variable name */
                param_temp = param_name[nargs];
                i = str_length(param_temp);

                /*
                ** ### NOTE:
                ** ### There's a problem here because if this request
                ** ### happens to be the first time this Location has
                ** ### been accessed, we need to parse the string at a
                ** ### point in time prior to any database connect.
                ** ### This may mean that the correct character set is
                ** ### not known.  Do the best possible; use the dad
                ** ### character set if it's specified, otherwise use
                ** ### the character set computed earlier for the
                ** ### path validation.
                */
                if (octx->dad_csid > 0) cs_id = nls_csid(octx->dad_csid);

                j = 0;
                while (j < m)
                {
                    sptr = (char *)morq_alloc(r, i + 10, 0);
                    if (!sptr) return(mem_error(r, i + 10, diagflag));
                    mem_copy(sptr, param_temp, i);
                    sptr[i] = '.';
                    str_itoa((nargs - n) + 1, sptr + i + 1);
                    param_name[nargs] = sptr;
                    k = (m - j);
                    if (k <= HTBUF_ARRAY_MAX_WIDTH)
                    {
                        /* Final piece is null-terminated */
                        sptr = aptr + j;
                    }
                    else
                    {
                        /* Copy a fragment and null-terminate it */
                        k = nls_length(cs_id, aptr + j,
                                       HTBUF_ARRAY_MAX_WIDTH);
                        sptr = (char *)morq_alloc(r, k + 1, 0);
                        if (!sptr) return(mem_error(r, k + 1, diagflag));
                        mem_copy(sptr, aptr + j, k);
                        sptr[k] = '\0';
                    }
                    param_value[nargs] = sptr;
                    j += k;
                    if (++nargs == arrsz)
                    {
                      /* Reallocate name/value arrays */
                      arrsz += HTBUF_PARAM_CHUNK;

                      param_name = (char **)resize_arr(r, (void *)param_name,
                                                       sizeof(char *), nargs);
                      param_value = (char **)resize_arr(r, (void *)param_value,
                                                        sizeof(char *), nargs);
                      if ((!param_name) || (!param_value))
                        return(mem_error(r, arrsz * sizeof(void *), diagflag));
                    }
                }
                continue;
            }
        }

        /* If a new name, add it to the argument list */
        if (n == nargs)
        {
            param_value[nargs] = aptr;
            stmtlen += 10;                  /* Enough for bind variable name */
            if (call_mode == 0)
                stmtlen += str_length(param_name[nargs]); /* Argument name */
            if ((call_mode != 1) && (call_mode != 3)) param_count[nargs] = 1;
            ++nargs;
        }
        /* Otherwise, it's an array argument */
        else
        {
            if (param_value[n] != (char *)0)
            {
                m = sizeof(char *) * HTBUF_PARAM_CHUNK;
                param_array[n] = (char **)morq_alloc(r, m, 0);
                if (!param_array[n]) return(mem_error(r, m, diagflag));
                param_array[n][0] = param_value[n];
                param_value[n] = (char *)0;
                param_count[n] = 1;
            }
            if ((param_count[n] & (HTBUF_PARAM_CHUNK-1)) == 0)
            {
                int nz = param_count[n];
                param_array[n] = (char **)resize_arr(r, (void *)param_array[n],
                                                     sizeof(char *), nz);
                nz += HTBUF_PARAM_CHUNK;
                if (!param_array[n])
                  return(mem_error(r, nz * sizeof(char *), diagflag));
            }
            param_array[n][param_count[n]++] = aptr;
        }
      }
    }
    fargs = nargs;

    /*
    ** Get arguments from content stream by boundary scanning
    ** Or inject the post body as a file-like argument
    */
    if ((bound_flag) || (post_body) || (rest_path))
    {
        if (bound_flag)
        {
          m = parse_args(r, octx, call_mode, boundary, clen,
                         &arrsz, &nargs, &param_name, &param_value, &filelist);
        }
        else /* Treat body as a file-like argument */
        {
          /*
          ** ### Note that this won't work unless we're running in Apex mode.
          */
          m = append_body(r, octx->dad_csid,
                          post_body, deferred_read,
                          ctype, clen, rest_path,
                          &arrsz, &nargs, &param_name, &param_value, &filelist);
        }
        if (m < 0) return(mem_error(r, m, diagflag));

        /* Add space for bind variable names and quoted identifiers */
        stmtlen += (12 * nargs);

        /* If necessary, resize the collection arrays */
        for (i = HTBUF_PARAM_CHUNK; i < arrsz; i += HTBUF_PARAM_CHUNK)
        {
            param_array = (char ***)resize_arr(r, (void *)param_array,
                                               sizeof(char **), i);
            if (!param_array)
              return(mem_error(r, i * sizeof(char **), diagflag));
            param_count = (ub4 *)resize_arr(r, (void *)param_count,
                                            sizeof(ub4), i);
            if (!param_count)
              return(mem_error(r, i * sizeof(ub4), diagflag));
        }

        for (j = 0; j < nargs; ++j) param_count[j] = 1;

        /* If not a flex-args call, convert collection arguments */
        if (call_mode == 0)
        {
          /* Convert repeated arguments to collection bindings */
          m = collect_args(r, &nargs, filelist,
                           param_name, param_value, param_array, param_count);
        }
    }

    if (arrsz > HTBUF_PARAM_CHUNK)
    {
        m = arrsz * sizeof(sb4);
        param_width = (sb4 *)morq_alloc(r, m, 0);
        if (!param_width) return(mem_error(r, m, diagflag));
    }

    emptystr[0] = '\0';
    emptystr[1] = '\0';

    if (octx->defaultcs)
    {
        /* Scan arguments for invalid UTF8 byte sequences */
        for (i = 0; i < nargs; ++i)
        {
            if (param_value[i] == (char *)0)
            {
                for (j = 0; j < (int)param_count[i]; ++j)
                   if (!nls_check_utf8(param_array[i][j], -1))
                       break;
                if (j < (int)param_count[i]) break;
            }
            else if (!nls_check_utf8(param_value[i], -1))
                break;
        }
        if (i < nargs)
        {
            makeutf = 1;
            if (diagflag & DIAG_ARGS)
                debug_out(octx->diagfile,
                          "UTF8> Invalid byte sequences detected in request\n",
                          (char *)0, (char *)0, 0, 0);
        }
    }

    /* Search for array arguments and allocate binding arrays */
    for (i = 0; i < nargs; ++i)
    {
        if (param_value[i] == (char *)0)
        {
            n = 2;
            for (j = 0; j < (int)param_count[i]; ++j)
            {
                /* If a file, reserve space for the file name */
                if (param_array[i][j] == empty_string)
                    m = HTBUF_LINE_CHARS;
                /* Otherwise, find the length */
                else
                    m = str_length(param_array[i][j]) + 1;
                if (m > n) n = m;
            }
            n = (int)util_round((un_long)n, octx->scale_round);
            if (n > HTBUF_ARRAY_MAX_WIDTH) n = HTBUF_ARRAY_MAX_WIDTH+1;
            m = n * j;

            /* Reuse array space for values, or reallocate if necessary */
            sptr = (char *)morq_alloc(r, m, 1);
            if (!sptr) return(mem_error(r, m, diagflag));

            param_value[i] = sptr;

            for (j = 0; j < (int)param_count[i]; ++j)
            {
                str_concat(sptr, 0, param_array[i][j], n - 1);
                sptr += n;
            }
        }
        else
        {
            /* If a file, reserve space for the file name */
            if (param_value[i] == empty_string)
                n = HTBUF_LINE_CHARS;
            /* Otherwise, find the length */
            else
                n = str_length(param_value[i]) + 1;
            if (n == 1)
            {
                /* Avoid OCI 0-length collection-binding bug */
                param_value[i] = emptystr;
                n = 2;
            }
            if ((call_mode != 1) && (call_mode != 3))
            {
                /*
                ** Set scalar parameter count and round column width,
                ** but only if not flexible argument mode.
                */
                param_count[i] = 0;
                if (n < HTBUF_ARRAY_MAX_WIDTH)
                {
                    n = (int)util_round((un_long)n, octx->scale_round);
                    if (n > HTBUF_ARRAY_MAX_WIDTH) n = HTBUF_ARRAY_MAX_WIDTH+1;
                }
            }
        }
        param_width[i] = (sb4)n;
    }

    if (file_flag)
    {
        /*
        ** For file uploads, build flexible argument passing structures,
        ** plus one extra varchar argument and two extra LOB output arguments
        */
        stmtlen += 200;

        /*
        ** Allocate storage for the name/value pairs of any parameters
        ** from the request URL and save them for later.
        */
        nadx = nargs;
        m = 2 * (nadx + 1) * sizeof(char *);
        pnames = (char **)morq_alloc(r, m, 0);
        if (!pnames) return(mem_error(r, m, diagflag));
        pvalues = pnames + (nadx + 1);
        for (i = 0; i < nadx; ++i)
        {
            pnames[i] = param_name[i];
            pvalues[i] = param_value[i];
        }

        for (i = 0; i < 4; ++i)
        {
            param_value[i] = (char *)"";
            param_width[i] = 1;
            param_count[i] = 1;
        }
        param_name[0] = (char *)"num_entries";
        param_name[1] = (char *)"name_array";
        param_name[2] = (char *)"value_array";
        param_name[3] = (char *)"reserved";

        for (i = 4; i < 9; ++i)
        {
            param_name[i] = (char *)"";
            param_width[i] = 0;
            param_count[i] = 0;
        }
        param_value[4] = pmimetype;
        param_width[4] = sizeof(pmimetype);

        param_count[0] = 0;

        if (long_flag == 2)
        {
            /*
            ** Add arguments for:
            **   File name return value
            */
            owa_out_args(param_value + 5, 3);
            param_width[5] = HTBUF_HEADER_MAX;

            nargs = 6;
            xargs = 2;
        }
        else if (long_flag)
        {
            /*
            ** Add arguments for:
            **   SQL stmt return value
            **   SQL bind  return value
            **   CHAR/RAW flag value
            */
            owa_out_args(param_value + 5, 0);
            param_width[5] = HTBUF_PLSQL_MAX;
            param_width[6] = HTBUF_HEADER_MAX;
            param_value[7] = prawchar;
            param_width[7] = sizeof(prawchar);

            nargs = 8;
            xargs = 4;
        }
        else
        {
            /*
            ** Add arguments for:
            **   BLOB return value
            **   CLOB return value
            **   NCLOB return value
            **   BFILE return value
            */
            owa_out_args(param_value + 5, 1);

            nargs = 5 + octx->lobtypes;
#ifndef BFILE_WRITE_SUPPORT
            /*
            ** ### Oracle doesn't yet support BFILE writes
            */
            if (nargs > 8) nargs = 8;
#endif
            xargs = nargs - 4;
        }
    }
    else if ((call_mode == 1) || (call_mode == 3))
    {
        stmtlen += 200;  /* Add space for flexible argument passing */

        /*
        ** Flip arguments into arrays for flexible passing mode
        ** :b1 string containing number of name/value pairs
        ** :b2 array of name strings
        ** :b3 array of value strings
        ** :b4 array of zero-length strings (reserved)
        */
        /*
        ** Bind values as array of pointers with callback mode to
        ** eliminate the need to construct an additional buffer.
        ** First scan all the values to make sure none of them were
        ** "chunked" (if they were, make sure there's a null terminator
        ** at the end).
        */
        param_ptrs = param_value;
        param_value = ptrbuf + (HTBUF_PARAM_CHUNK * 2);
        if (param_value == param_ptrs)
        {
            m = (nargs + 1) * sizeof(char *);
            param_ptrs = (char **)morq_alloc(r, m, 0);
            if (!param_ptrs) return(mem_error(r, m, diagflag));
            for (i = 0; i < nargs; ++i) param_ptrs[i] = param_value[i];
        }

        n = 2;
        for (i = 0; i < nargs; ++i)
        {
            j = param_width[i];
            if (j > HTBUF_ARRAY_MAX_WIDTH) j = HTBUF_ARRAY_MAX_WIDTH+1;
            if (j > n) n = j;
            if (param_ptrs[i][j - 1] != '\0') param_ptrs[i][j - 1] = '\0';
        }
        n = (int)util_round((un_long)n, octx->scale_round);
        if (n > HTBUF_ARRAY_MAX_WIDTH) n = HTBUF_ARRAY_MAX_WIDTH+1;
        param_width[2] = (sb4)n;
        param_value[2] = (char *)0;
        param_temp = (char *)"\0\0";
        if (nargs == 0) param_ptrs[0] = param_temp;

        n = 2;
        for (i = 0; i < nargs; ++i)
        {
            j = str_length(param_name[i]) + 1;
            if (j > n) n = j;
        }
        /* Round column widths for flexible argument mode */
        n = (int)util_round((un_long)n, octx->scale_round);
        if (n > HTBUF_HEADER_MAX) n = HTBUF_HEADER_MAX;
        sptr = (nargs == 0) ? param_temp : (char *)morq_alloc(r, n * nargs, 0);
        if (!sptr) return(mem_error(r, n * nargs, diagflag));
        param_temp = sptr;
        for (i = 0; i < nargs; ++i)
        {
            str_concat(sptr, 0, param_name[i], n - 1);
            sptr += n;
        }
        param_width[1] = (sb4)n;
        param_value[1] = param_temp;

        if (call_mode == 3)
        {
            /* 2-argument calling mode omits num_entries and reserved */

            param_width[0] = param_width[1];
            param_width[1] = param_width[2];
            param_value[0] = param_value[1];
            param_value[1] = param_value[2];

            param_name[0] = (char *)"name_array";
            param_name[1] = (char *)"value_array";

            /* Arguments forced up from count=0 */
            if (nargs == 0)
            {
                nargs = 1;
                zero_args = 0x0003; /* Bit max for uplifted arguments */
            }

            param_count[0] = nargs;
            param_count[1] = nargs;

            nargs = 2;
        }
        else
        {
            n = 2;
            sptr = (nargs == 0) ? param_temp : (char *)morq_alloc(r,n*nargs,0);
            if (!sptr) return(mem_error(r, n * nargs, diagflag));
            param_temp = sptr;
            for (i = 0; i < nargs; ++i)
            {
                *sptr = '\0';
                sptr += n;
            }
            param_width[3] = (sb4)n;
            param_value[3] = param_temp;

            param_width[0] = str_itoa(nargs, pcount) + 1;
            param_name[0] = (char *)"num_entries";
            param_value[0] = pcount;
            param_count[0] = 0;

            param_name[1] = (char *)"name_array";
            param_name[2] = (char *)"value_array";
            param_name[3] = (char *)"reserved";

            /* Arguments forced up from count=0 */
            if (nargs == 0)
            {
                nargs = 1;
                zero_args = 0x000E; /* Bit max for uplifted arguments */
            }
            param_count[1] = nargs;
            param_count[2] = nargs;
            param_count[3] = nargs;

            nargs = 4;
        }
    }

    /*
    ** Check describe cache to see if any scalar arguments
    ** should instead be bound as singleton collections
    */
    if ((call_mode == 0) && (nargs > 0))
    {
        for (dptr = octx->desc_cache;
             dptr != (descstruct *)0;
             dptr = dptr->next)
        {
            if (str_compare(dptr->pname, spath, -1, 1) == 0)
            {
                sptr = dptr->pargs;
                if (sptr)
                {
                    while (*sptr)
                    {
                        for (i = 0; i < nargs; ++i)
                        {
                            if (str_compare(sptr, param_name[i], -1, 1))
                            {
                                if (param_count[i] == 0) param_count[i] = 1;
                                break;
                            }
                        }
                        sptr += (str_length(sptr) + 1);
                    }
                }
            }
        }
    }

    if (wpg_flag)
    {
        /* No arguments to document procedure */
        xargs = nargs = 0;
    }
    else if (rset_flag)
    {
        /*
        ** Add argument for REF cursor return
        */
        stmtlen += 100; /* Enough for bind variable names */

        /* If necessary expand the parameter arrays */
        if ((nargs + 2) >= arrsz)
        {
          m = nargs + HTBUF_PARAM_CHUNK;
          param_width = (sb4 *)resize_arr(r, (void *)param_width,
                                          sizeof(sb4), nargs);
          param_count = (ub4 *)resize_arr(r, (void *)param_count,
                                          sizeof(ub4), nargs);
          if ((!param_width) || (!param_count))
            return(mem_error(r, m * sizeof(b4), diagflag));
          param_name = (char **)resize_arr(r, (void *)param_name,
                                           sizeof(char *), nargs);
          param_value = (char **)resize_arr(r, (void *)param_value,
                                            sizeof(char *), nargs);
          if ((!param_name) || (!param_value))
            return(mem_error(r, m * sizeof(void *), diagflag));
        }

        /* Set the special OUT arguments for the REF cursor mode */
        owa_out_args(param_value + nargs, 5);
        param_name[nargs] = (char *)"RESULT_CURSOR$";
        param_width[nargs] = 0;
        param_count[nargs] = 0;
        ++nargs;
        param_name[nargs] = (char *)"XML_TAGS$";
        param_width[nargs] = HTBUF_HEADER_MAX + 1;
        param_count[nargs] = 0;
        *pdocload = '\0';
        ++nargs;
        xargs = 2; /* Two OUT arguments appended */
    }
    else if (fpath)
    {
        if (long_flag == 2)
        {
            /*
            ** Add arguments for:
            **   mimetype return value
            **   File name return value
            */
            stmtlen += 50; /* Enough for bind variable names */
            for (i = 0; i < 2; ++i)
            {
                param_name[i + nargs] = (char *)"";  /* No argument name */
                param_count[i + nargs] = 0;  /* Scalar */
            }
            param_width[nargs] = sizeof(pmimetype);
            param_value[nargs++] = pmimetype;
            owa_out_args(param_value + nargs, 3);
            param_width[nargs++] = HTBUF_HEADER_MAX;
        }
        else if (long_flag)
        {
            /*
            ** Add arguments for:
            **   mimetype return value
            **   SQL stmt return value
            **   SQL bind  return value
            **   Content length return value (optional)
            **   CHAR/RAW flag value
            */
            stmtlen += 50; /* Enough for bind variable names */
            for (i = 0; i < 5; ++i)
            {
                param_name[i + nargs] = (char *)"";  /* No argument name */
                param_count[i + nargs] = 0;  /* Scalar */
            }
            param_width[nargs] = sizeof(pmimetype);
            param_value[nargs++] = pmimetype;
            owa_out_args(param_value + nargs, 0);
            param_width[nargs++] = HTBUF_PLSQL_MAX;
            param_width[nargs++] = HTBUF_HEADER_MAX;
            if (octx->lontypes == LONG_MODE_RETURN_LEN)
                param_width[nargs++] = LONG_MAXSTRLEN;
            param_width[nargs] = sizeof(prawchar);
            param_value[nargs++] = prawchar;
        }
        else if (!gen_flag)
        {
            /*
            ** Add arguments for:
            **   mimetype return value
            **   BLOB return value
            **   CLOB return value
            **   NCLOB return value
            **   BFILE return value
            */
            stmtlen += 60; /* Enough for bind variable names */
            for (i = 0; i < 5; ++i)
            {
                param_name[i + nargs] = (char *)"";  /* No argument name */
                param_count[i + nargs] = 0;  /* Scalar */
                param_width[i + nargs] = sizeof(pmimetype);
            }
            param_value[nargs] = pmimetype;
            owa_out_args(param_value + nargs + 1, 1);

            nargs += (octx->lobtypes + 1);
        }
        /* Else generated document mode: doesn't use any return arguments */

        xargs = nargs - 4;
    }

    if (raw_post)
    {
        nargs = 3;
        param_width[1] = sizeof(pmimetype);
        param_value[1] = pmimetype;
        owa_out_args(param_value + nargs - 1, 1);
        xargs = 2;

        for (i = 0; i < nargs; ++i)
        {
            param_name[i] = (char *)"";  /* No argument name */
            param_count[i] = 0;  /* Scalar */
        }
        param_width[0] = (sb4)clen;
        param_value[0] = raw_post;
    }

#ifndef NO_FILE_CACHE
    if ((octx->altflags & ALT_MODE_CACHE) && (!fpath) && (!file_flag))
    {
        cache_flag = 1;
        stmtlen += 100;
    }
#endif

    if ((call_mode == 0) && (nargs > 0))
        stmtlen += (2 * nargs); /* Allow for quoted identifiers */

    if (octx->ora_before) stmtlen += (str_length(octx->ora_before) + 10);
    if (octx->ora_after)  stmtlen += (str_length(octx->ora_after)  + 10);

    /* Attempt to use a static buffer for the statement */
    if (stmtlen <= (int)sizeof(stmtbuf))
        stmt = stmtbuf;
    else
        stmt = (char *)morq_alloc(r, stmtlen, 0);
    if (!stmt) return(mem_error(r, stmtlen, diagflag));

    /*
    ** Now build the dynamic SQL statement
    **  1. Build the preamble and authorization check
    **  2. Build the call with arguments
    **  3. Build the commit and end of statement
    */

    sptr = stmt;

    aptr = (char *)0;
    if ((fpath) || (file_flag))
    {
        /* File upload/download assumes authorization is in doc_proc */
        sptr += str_concat(sptr, 0, "begin\n", -1);
        sptr = set_db_bufsize(sptr, octx->db_charsize);
        sptr += str_concat(sptr, 0, "  ", -1);
        aptr = (char *)"";
    }
    else if (octx->version & OWA_AUTH_PACKAGE)
    {
        /* Reverse-search in case of schema prefix. */
        aptr = str_char(spath, '.', 1);
        if (aptr)
        {
            sptr += str_concat(sptr, 0, "begin\n", -1);
            sptr = set_db_bufsize(sptr, octx->db_charsize);
            sptr += str_concat(sptr, 0, "  if ", -1);
            sptr += str_concat(sptr, 0, spath, (int)(aptr - spath));
            sptr += str_concat(sptr, 0, ".AUTHORIZE then\n    ", -1);
            spflag = 4;
            realm_flag = 1;
        }
        /* If there's no prefix and OWA_INIT/OWA_CUSTOM not specified */
        else if ((octx->version & ~OWA_AUTH_PACKAGE) == 0)
        {
            aptr = ""; /* Use a function instead of a package procedure */
            sptr += str_concat(sptr, 0, "begin\n", -1);
            sptr = set_db_bufsize(sptr, octx->db_charsize);
            sptr += str_concat(sptr, 0, "  if AUTHORIZE then\n    ", -1);
            spflag = 4;
            realm_flag = 1;
        }
    }
    if (!aptr)
    {
        switch (octx->version & ~OWA_AUTH_PACKAGE)
        {
        case OWA_AUTH_INIT:
            aptr = (octx->alternate) ? octx->alternate : (char *)"OWA_INIT";
            break;
        case OWA_AUTH_CUSTOM:
            aptr = (octx->alternate) ? octx->alternate : (char *)"OWA_CUSTOM";
            break;
        default:
            break;
        }
        if (!aptr)
        {
            sptr += str_concat(sptr, 0, "begin\n", -1);
            sptr = set_db_bufsize(sptr, octx->db_charsize);
            sptr += str_concat(sptr, 0, "  ", -1);
        }
        else
        {
            sptr += str_concat(sptr, 0, "begin\n", -1);
            sptr = set_db_bufsize(sptr, octx->db_charsize);
            sptr += str_concat(sptr, 0, "  if ", -1);
            sptr += str_concat(sptr, 0, aptr, 32);
            sptr += str_concat(sptr, 0, ".AUTHORIZE then\n    ", -1);
            spflag = 4;
            realm_flag = 1;
        }
    }

    if (octx->ora_before)
    {
        sptr += str_concat(sptr, 0, octx->ora_before, -1);
        sptr += str_concat(sptr, 0,
                           (char *)((spflag == 4) ? ";\n    " : ";\n  "), -1);
    }

    sptr += str_concat(sptr, 0, spath, -1);
    if ((nargs > 0) || (call_mode == 1) || (call_mode == 3)) *(sptr++) = '(';

    for (i = 0; i < nargs; ++i)
    {
        bname[0] = ':';
        bname[1] = 'B';
        str_itoa(i + 1, bname + 2);

        if (i > 0) sptr += str_concat(sptr, 0, ",", -1);
        if ((call_mode == 0) && (param_name[i][0] != '\0'))
        {
            sptr += nls_copy_identifier(sptr, param_name[i], cs_id);
            sptr += str_concat(sptr, 0, "=>", -1);
        }
        sptr += str_concat(sptr, 0, bname, -1);

        if (diagflag & DIAG_ARGS)
        {
            debug_out(octx->diagfile, "  %s => %s",
                      param_name[i], bname, 0, 0);
            aptr = param_value[i];
            n = param_count[i];
            if (n == 0) n = 1;
            if ((raw_post) && (i == 0))
                debug_out(octx->diagfile, " [IN: <%d binary bytes>]",
                          (char *)0, (char *)0, (int)clen, 0);
            else if (!aptr)
              for (j = 0; j < n; ++j)
              {
                debug_out(octx->diagfile, " [%s]", param_ptrs[j],
                          (char *)0, 0, 0);
              }
            else
              while (n > 0)
              {
                --n;
                debug_out(octx->diagfile, " [%s]", aptr, (char *)0, 0, 0);
                aptr += param_width[i];
              }
            debug_out(octx->diagfile, "\n", (char *)0, (char *)0, 0, 0);
        }
    }
    if ((nargs > 0) || (call_mode == 1) || (call_mode == 3)) *(sptr++) = ')';

#ifndef NO_FILE_CACHE
    if (cache_flag)
    {
        aptr = str_char(spath, '.', 1);
        if (!aptr)
            cache_flag = 0; /* No package name, so no caching possible */
        else
        {
            if (octx->version == 0)
                sptr += str_concat(sptr, 0, ";\n  ", -1);
            else
                sptr += str_concat(sptr, 0, ";\n    ", -1);
            sptr += str_concat(sptr, 0, spath, (int)(aptr - spath));
            sptr += str_concat(sptr, 0, ".CHECK_CACHE(:O1,:O2,:O3,:O4)", -1);
        }
    }
#endif

    if ((!file_flag) && ((octx->doc_table) || (octx->doc_column)))
      wpg_flag = 1;

    /*
    ** Allow extra space in argument arrays for
    **  1) Cache output bindings
    **  2) Realm binding
    **  3) WPG_DOCLOAD bindings
    */
    i = nargs;
    if (cache_flag)     i += 4;
    if (realm_flag)        ++i;
    if (wpg_flag)       i += 2;
    if (i > arrsz)
    {
        m = nargs + HTBUF_PARAM_CHUNK;
        param_width = (sb4 *)resize_arr(r, (void *)param_width,
                                        sizeof(sb4), nargs);
        param_count = (ub4 *)resize_arr(r, (void *)param_count,
                                        sizeof(ub4), nargs);
        if ((!param_width) || (!param_count))
            return(mem_error(r, m * sizeof(b4), diagflag));
        param_name = (char **)resize_arr(r, (void *)param_name,
                                         sizeof(char *), nargs);
        param_value = (char **)resize_arr(r, (void *)param_value,
                                          sizeof(char *), nargs);
        if ((!param_name) || (!param_value))
            return(mem_error(r, m * sizeof(void *), diagflag));
    }

    /* Add on extra bindings to get results of WPG_DOCLOAD call, if any */
    if (wpg_flag)
    {
        owa_out_args(param_value + nargs, 4);
        param_name[nargs] = (char *)"O5";
        param_width[nargs] = sizeof(pdocload);
        param_value[nargs] = pdocload;
        param_count[nargs] = 0;
        *pdocload = '\0';
        ++nargs;
        ++xargs;
        param_name[nargs] = (char *)"O6";
        param_width[nargs] = 0;
        param_count[nargs] = 0;
        ++nargs;
        ++xargs;
    }

#ifndef NO_FILE_CACHE
    /*
    ** If page-caching mode is enabled, add bindings for:
    **
    **   <package_name>.CHECK_CACHE(:O1,:O2,:O3,:O4);
    **
    **   :O1 => URI return (4K)
    **   :O2 => Checksum return (32K)
    **   :O3 => Content-Type return
    **   :O4 => Lifespan return
    */
    if (cache_flag)
    {
        for (i = 0; i < 4; ++i)
        {
            param_name[i + nargs] = (char *)"";  /* No argument name */
            param_count[i + nargs] = 0;  /* Scalar */
        }
        param_width[nargs]     = HTBUF_HEADER_MAX;  /* URI return */
        param_width[nargs + 1] = HTBUF_PLSQL_MAX;   /* Checksum return */
        param_width[nargs + 2] = sizeof(pmimetype); /* Mime type return */
        param_width[nargs + 3] = sizeof(prawchar);  /* Lifespan return */
        param_value[nargs + 2] = pmimetype;
        param_value[nargs + 3] = prawchar;
        owa_out_args(param_value + nargs, 2);
        nargs += 4; /* So that the execution will find :O1,:O2,:O3,:O4 */
        xargs += 4;
    }
#endif

    *prealm = '\0';
    if (realm_flag)
    {
        param_name[nargs] = (char *)"realm";
        param_count[nargs] = 0;
        param_width[nargs] = sizeof(prealm);
        param_value[nargs] = prealm;
        ++nargs;
        ++xargs;
    }

    if (octx->ora_after)
    {
        sptr += str_concat(sptr, 0,
                           (char *)((spflag == 4) ? ";\n    " : ";\n  "), -1);
        sptr += str_concat(sptr, 0, octx->ora_after, -1);
    }

    /* Add on extra clause to get value of WPG_DOCLOAD call, if any */
    if (wpg_flag)
    {
      if (octx->version)
        sptr += str_concat(sptr, 0,
                           ";\n    if (WPG_DOCLOAD.IS_FILE_DOWNLOAD) then\n"
                           "      WPG_DOCLOAD.GET_DOWNLOAD_FILE(:O5);\n"
                           "      WPG_DOCLOAD.GET_DOWNLOAD_BLOB(:O6);\n"
                           "    end if", -1);
      else
        sptr += str_concat(sptr, 0,
                           ";\n  if (WPG_DOCLOAD.IS_FILE_DOWNLOAD) then\n"
                           "    WPG_DOCLOAD.GET_DOWNLOAD_FILE(:O5);\n"
                           "    WPG_DOCLOAD.GET_DOWNLOAD_BLOB(:O6);\n"
                           "  end if", -1);
    }

    if (file_flag)
        sptr += str_concat(sptr, 0, (char *)";\nend;", -1);
    else if (!realm_flag)
        sptr += str_concat(sptr, 0, (char *)";\n  commit;\nend;", -1);
    else
    {
        sptr += str_concat(sptr, 0,
                           (char *)";\n    commit;\n  else\n    :realm := ",
                           -1);
        if (octx->alternate)
            sptr += str_concat(sptr, 0, octx->alternate, -1);
        else
            sptr += str_concat(sptr, 0, (char *)"OWA", -1);
        sptr += str_concat(sptr, 0, (char *)".PROTECTION_REALM;", -1);
        sptr += str_concat(sptr, 0, (char *)"\n  end if;\nend;", -1);
    }

    /*
    ** Allocate array buffer space (max of input and output arrays):
    */
    if (++nwidth > HTBUF_ENV_MAX) nwidth = HTBUF_ENV_MAX;
    if (++vwidth > HTBUF_ENV_MAX) vwidth = HTBUF_ENV_MAX;
    if (nwidth < HTBUF_ENV_NAM) nwidth = HTBUF_ENV_NAM;

    /*
    ** Dynamically computing HTBUF_LINE_LENGTH based on
    ** the character set could save memory; enough space
    ** needs to be reserved for file upload/download, though.
    ** For now, the code always allocates for the UTF8 case
    ** (size X 3), and if it's single-byte uses the "extra"
    ** buffer space to fetch longer arrays.
    **
    ** ### THIS BUFFER IS VERY OVERWORKED NOW.  IT'S TIME TO
    ** ### CREATE A union/struct THAT CAN MORE CLEARLY SHOW
    ** ### THE VARIOUS USAGE SCENARIOS FOR IT.
    */
    esize = (nwidth + vwidth) * nenv;
    osize = HTBUF_ARRAY_SIZE * HTBUF_LINE_LENGTH;
    if (esize > osize)
        osize = esize;
    else if ((nenv * (nwidth + HTBUF_ENV_MAX)) < osize)
        vwidth = HTBUF_ENV_MAX;
    outbuf = morq_get_buffer(r, osize); /* Use static buffer if possible */
    if (!outbuf)
    {
#ifdef OWA_USE_ALLOCA
        if (osize <= HTBUF_ARRAY_SIZE * HTBUF_LINE_LENGTH)
            outbuf = (char *)os_alloca(osize);
        if (!outbuf)
#endif
            outbuf = (char *)morq_alloc(r, osize, 0);
    }
    if (!outbuf) return(mem_error(r, osize, diagflag));

#ifndef NO_FILE_CACHE
    /*
    ** Before running the SQL portion of the handler, perform
    ** a cache check.  Pages are eligible for caching if:
    **   - their origin is based on document download (e.g. "static")
    **   - there are no request arguments
    ** The check is done by comparing the URL prefix in fpath
    ** against the list of cached locations, and by looking for the
    ** file in the file system in the designated physical directory.
    **
    ** ### I should add a mode to defer this until after a read request
    ** ### done using the "document_gen" style (no output args).  In
    ** ### this case, the read procedure could do something like an
    ** ### authorization check and/or hit tracking, then mod_owa would
    ** ### proceed to read from the cache.  This really only makes sense
    ** ### for static caches, and then only if we're certain the content
    ** ### is actually present in the file system (otherwise you'll get
    ** ### an error).
    */
    if ((fpath) && (fargs <= 1))
    {
        life = (ub4)0;
        physical = owa_map_cache(octx, r, fpath, &life);
        if (physical)
          if (owa_download_file(octx, r, physical, pmimetype, life, outbuf))
            return(rstatus);
        if (life == (ub4)0) physical = (char *)0;
    }
#endif

    if (octx->authrealm)
    {
      /* If realm ends in wildcard, timestamp should be appended */
      char *wildcard = str_substr(octx->authrealm, wdb_trailer, 0);
      if (wildcard)
        if (!str_compare(wildcard, wdb_trailer, -1, 1))
          append_timestamp = 1;
    }

    /*
    ** ### PROBLEM: IF AN AUTH IS SPECIFIED, IT WON'T BE
    ** ### VERIFIED AGAINST THE DATABASE UNTIL AFTER
    ** ### THE CACHE WAS HIT IN THE CODE ABOVE.
    */
    if ((octx->authrealm) && ((!authuser) || (!authpass) || wdb_realm_logout))
    {
        if ((wdb_realm_logout) && (append_timestamp))
        {
          /*
          ** If the WDB_GATEWAY_LOGOUT cookie was YES, reset the cookie here
          ** ### Won't this be incorrect if the user response fails to log in?
          */
          set_wdb_cookie(r, 0);
        }

        issue_challenge(r, octx->authrealm, append_timestamp);
        return(rstatus);
    }

    /* Transfer the environment variables to a buffer array */
    transfer_env(r, &senv, remote_addr, nwidth, vwidth, nenv, outbuf);
    senv.authuser = authuser;
    senv.authpass = authpass;

    if (diagflag & DIAG_THREADS) nopool_flag = 1;

    if ((raw_post) || (octx->ncflag & UNI_MODE_RAW))
    {
        /*
        ** For raw bindings, we need bindable length arrays;
        ** allocate a single large array here and set up the
        ** lengths appropriately.
        */
        j = 0;
        for (i = 0; i < nargs - xargs; ++i)
        {
            if (param_count[i] == 0)  ++j;
            else                      j += param_count[i];
        }
        if (j > 0)
        {
          j *= sizeof(ub2);
          param_lens = (ub2 *)morq_alloc(r, j, 0);
          if (!param_lens) return(mem_error(r, j, diagflag));
          j = 0;
          for (i = 0; i < nargs - xargs; ++i)
          {
            if (param_count[i] == 0)
                param_lens[j++] = param_width[i];
            else
                for (k = 0; k < (int)param_count[i]; ++k)
                {
                    param_lens[j] = 0;
                    if (param_value)
                      if (param_value[k])
                        param_lens[j] = str_length(param_value[k]);
                    ++j;
                }
          }
        }
    }

    /*
    ** 1. Connect to Oracle using V8 OCI
    ** 2. Pass environment vector to OWA
    ** 3. Run PL/SQL stored procedure
    ** 4. Get data and transfer to output
    ** 5. Close OCI connection
    */
    owa_req->lock_time = get_elapsed_time(stime);
    c = (nopool_flag) ? (connection *)0 : get_connection(octx, session);
    if (!c)
    {
        /*
        ** If we can't get a connection, see if the request should simply
        ** be aborted
        */
        if (octx->pool_wait_abort)
        {
          if (diagflag & DIAG_ERROR)
            debug_out(octx->diagfile,
                      "Could not get a connection for request [%s], PID: %s\n",
                      spath, pidstr, 0, 0);
          return(HTTP_INTERNAL_SERVER_ERROR);
        }
        mem_zero(&cdefault, sizeof(cdefault));
        c = &cdefault;
        c->csid = 0;
        c->c_lock = C_LOCK_NEW;
        c->session = (char *)0;
        c->slotnum = -1; /* Mark it as a temp connection */
        c->sockctx = (owa_log_socks *)0; /* No logging on temp connection */
    }
    retrycount = (c->c_lock == C_LOCK_NEW) ? 1 : 0;
retry:
    *errbuf = '\0';
    sphase = 1;
    c->errbuf = errbuf;
    /* Make sure socket isn't inherited by Oracle shadow process */
    morq_close_exec(r, octx, (c->c_lock != C_LOCK_INUSE));
    owa_req->connect_time = get_elapsed_time(stime);
    status = sql_connect(c, octx, authuser, authpass, &errinfo);
    if (errinfo)
    {
        /* Log a warning if the connect was SUCCESS_WITH_INFO */
        debug_out(octx->diagfile, "Warning code %d during logon\n",
                  (char *)0, (char *)0, (int)errinfo, 0);
    }
    debug_sql(octx, "connect", pidstr, status, (char *)0);
    if (status == OCI_SUCCESS)
    {
        /* If the connect succeeds, and sessioning is used, set the session */
        if (session)
        {
            if (c->session)
            {
                if (str_compare(session, c->session, -1, 0))
                {
                    mem_free((void *)(c->session));
                    c->session = (char *)0;
                }
            }
            if (!(c->session)) c->session = str_dup(session);
        }

        if (!(octx->nls_init))
        {
          mowa_acquire_mutex(octx);
          if (!(octx->nls_init)) sql_get_nls(c, octx);
          mowa_release_mutex(octx);
        }

        ++sphase;
        owa_req->wait_time = get_elapsed_time(stime);
        status = owa_passenv(c, octx, &senv, owa_req);
        debug_sql(octx, "passenv", pidstr, status, (char *)0);
        c->c_lock = C_LOCK_INUSE;

        if (status == OCI_SUCCESS)
        {
            ++retrycount;
            ++sphase;

            if (file_flag)
            {
                /* Loop reading content stream and calling writer */
                c->ncflag |= (octx->ncflag & (UNI_MODE_USER | UNI_MODE_RAW));
                status = owa_writedata(c, octx, r, boundary, clen,
                                       long_flag, outbuf, stmt,
                                       pmimetype, nargs, nadx,
                                       pnames, pvalues);
                c->ncflag &= ~(UNI_MODE_USER | UNI_MODE_RAW);
                debug_sql(octx, "writefile", pidstr, status, (char *)0);
            }
            else
            {
                cs_id = 0;
                if (makeutf)
                {
                    /*
                    ** Data wasn't utf-8;
                    ** use the default character set temporarily
                    */
                    i = nls_csx_from_oracle(octx->defaultcs);
                    if (i >= 0) cs_id = nls_csid(i);
                    if (diagflag & DIAG_ARGS)
                      debug_out(octx->diagfile,
                                "UTF8> Replacing CS %d with %d\n",
                                (char *)0, (char *)0, c->csid, cs_id);
                }

                /* If uploading files via the WebDB interface */
                if (filelist)
                    status = owa_wlobtable(c, octx, r, outbuf, filelist,
                                           param_value, param_ptrs,
                                           empty_string);

                if (status == OCI_SUCCESS)
                {
                  /* Call single handling procedure */
                  c->ncflag |= (octx->ncflag & (UNI_MODE_USER | UNI_MODE_RAW));
                  if (raw_post) c->ncflag |= UNI_MODE_RAW;
                  status = owa_runplsql(c, stmt, outbuf, xargs, nargs,
                                        param_value, param_count, param_width,
                                        param_ptrs, param_lens, octx->arr_round,
                                        cs_id, zero_args);
                  c->ncflag &= ~(UNI_MODE_USER | UNI_MODE_RAW);
                  debug_sql(octx, "runplsql", pidstr, status, (char *)0);
                }

                if ((call_mode != 3) && (status == PLSQL_ERR) &&
                    (octx->descmode != DESC_MODE_STRICT) &&
                    (str_substr(errbuf, PLSQL_ARGERR, 0)))
                {
                    if (call_mode == 1)
                    {
                        /* ### This is a complete hack ### */
                        sptr = str_substr(stmt, "(:B1,:B2,:B3,:B4)", 0);
                        if (sptr)
                        {
                            param_count[0] = LONG_MAXSZ;
                            param_count[3] = LONG_MAXSZ;
                            mem_copy(sptr, "(:B2,:B3        )", 17);
                            call_mode = 3;
                            cache_describe(octx,spath,0,(char **)0,(ub4 *)0);
                            goto rerun_statement;
                        }
                        desc_mode = DESC_MODE_RELAXED;
                    }
                    else if (call_mode == 2) desc_mode = DESC_MODE_POSITIONAL;
                    else                     desc_mode = octx->descmode;
                    /*
                    ** Procedure call may have failed because of a
                    ** mismatch between an array-bound argument type
                    ** and a scalar.  Attempt to "promote" one or more
                    ** of the scalars to array type and retry (once)
                    ** before failing.
                    */
                    spath = append_schema(octx, r, spath);
                    if (sql_describe(c, spath, desc_mode, octx->desc_schema,
                                     nargs - xargs, param_name, param_count))
                    {
                        cache_describe(octx, spath, nargs - xargs,
                                       param_name, param_count);
                        revise_statement(stmt, nargs-xargs, param_count);
rerun_statement:
                        c->ncflag |= (octx->ncflag &
                                      (UNI_MODE_USER | UNI_MODE_RAW));
                        if (raw_post) c->ncflag |= UNI_MODE_RAW;
                        status = owa_runplsql(c, stmt, outbuf, xargs,
                                              nargs, param_value, param_count,
                                              param_width, param_ptrs,
                                              param_lens, octx->arr_round,
                                              cs_id, zero_args);
                        c->ncflag &= ~(UNI_MODE_USER | UNI_MODE_RAW);
                        debug_sql(octx, "redescribe",
                                  pidstr, status, (char *)0);
                    }
                }

                if ((status != OCI_SUCCESS) && (octx->sqlerr_uri))
                    pstatus = status;

#ifndef NO_FILE_CACHE
                if ((cache_flag) && (*outbuf))
                {
                    /*
                    ** If the call has resulted in a request to read
                    ** a cached page, try to find it and use it.
                    */

                    sptr = outbuf + HTBUF_HEADER_MAX;
                    if (*sptr)
                    {
                        /*
                        ** If a checksum string is available, compute
                        ** the sum and then insert the result into the
                        ** URI before the file extension.
                        */
                        csum = (ub4)util_checksum(sptr, str_length(sptr));
                        aptr = str_char(outbuf, '.', 1);
                        if (!aptr) aptr = outbuf + str_length(outbuf);
                        str_copy(sptr, aptr);
                        aptr += str_itox((un_long)csum, aptr);
                        str_copy(aptr, sptr);
                    }

                    /* Override lifespan with return result */
                    clife  = 0;
                    for (sptr = prawchar;
                         ((*sptr >= '0') && (*sptr <= '9'));
                         ++sptr)
                        clife = clife * 10 + (*sptr - '0');

                    physical = owa_map_cache(octx, r, outbuf, &life);
                    if (life == 0)      physical = (char *)0;
                    else if (clife > 0) life = clife;
                    if (physical)
                      if (owa_download_file(octx, r, physical,
                                            pmimetype, life, outbuf))
                      {
                          /* Success, unlock the connection and return */
                          if (c->slotnum < 0)
                              sql_disconnect(c);
                          else
                          {
#ifdef RESET_AFTER_EXEC
                              morq_write(r, (char *)0, 0);
                              owa_reset(c, octx);
#endif
                              unlock_connection(octx, c);
                          }
                          return(rstatus);
                      }
                    /*
                    ** If the download failed, then re-run the procedure
                    ** to force it to generate content; keep the physical
                    ** path computed by this process for the GET_PAGE
                    ** phase, when we will write the contents to the file.
                    */
                    c->ncflag |= (octx->ncflag & (UNI_MODE_USER|UNI_MODE_RAW));
                    if (raw_post) c->ncflag |= UNI_MODE_RAW;
                    status = owa_runplsql(c, stmt, outbuf, xargs, nargs,
                                          param_value, param_count,
                                          param_width, param_ptrs,
                                          param_lens, octx->arr_round,
                                          cs_id, zero_args);
                    c->ncflag &= ~(UNI_MODE_USER | UNI_MODE_RAW);
                    debug_sql(octx, "rerun", pidstr, status, (char *)0);
                }
#endif
                if (raw_post)
                {
                    if (c->blob_ind != (ub2)-1) fpath = "";
                }
                else
                if (long_flag)
                {
                  if (*outbuf == '\0')
                    fpath = (char *)0; /* No LONG returned, get error page */
                }
                else if (gen_flag)
                    fpath = (char *)0; /* Use GET_PAGE for content return */
                else if (wpg_flag)
                    fpath = (char *)0; /* ### Use WebDB content return */
                else if (fpath)
                {
                  if ((c->blob_ind == (ub2)-1) && (c->clob_ind == (ub2)-1) &&
                      (c->nlob_ind == (ub2)-1) && (c->bfile_ind == (ub2)-1))
                    fpath = (char *)0; /* No LOB returned, get error page */
                }
            }

            if ((status == OCI_SUCCESS) && (c->mem_err == 0))
            {
                ++sphase;
                if ((realm_flag) && (*prealm))
                {
                    /* Authorization failed, requested Basic authentication */
                    issue_challenge(r, prealm, 0);
                }
                else if ((wpg_flag) && (*pdocload))
                {
                  *pcdisp = '\0';
                  status = owa_getheader(c, octx, r, pmimetype, pcdisp,
                                         outbuf, &rstatus);
                  debug_sql(octx, "getheader", pidstr, status, (char *)0);
                  if ((*pdocload != 'B') && (status == OCI_SUCCESS))
                  {
                      /*
                      ** Unpack pdocload encoded string containing:
                      **
                      ** filename,timestamp,mimetype,lobtype,charset,size
                      **
                      ** These are encoded as blenXparamX (repeating).
                      */
                      char *pbuf = pdocload;
                      char *fctype = (char *)0;
                      char *fname;

                      /* Get the file name */
                      fname = get_wpg_field(&pbuf);
                      /* Skip the timestamp */
                      if (pbuf) get_wpg_field(&pbuf);
                      /* Get the mime type */
                      if (pbuf) fctype = get_wpg_field(&pbuf);
                      if (fctype)
                        if ((*fctype) && (*pmimetype == '\0'))
                          str_copy(pmimetype, fctype);

                      if (rstatus == HTTP_NOT_FOUND)
                      {
                        /* ### For now assume no explicit header is OK ### */
                        rstatus = OK;
                      }

                      /*
                      ** Now fetch from the document table.
                      **
                      ** ### Safe to assume only BLOB is used.
                      ** ### Possibly just the filename is enough, the other
                      ** ### information is available from the table?
                      ** ### For now this is not supported because I don't
                      ** ### know if Apex actually uses that mode or not.
                      */
                      status = owa_rlobtable(c, octx, r, fname,
                                             pmimetype, pcdisp, outbuf);
                      debug_sql(octx, "rlobtable", pidstr, status, (char *)0);
                  }

                  if (c->blob_ind != (ub2)-1)
                  {
                      /* Use the returned mime type and BLOB */
                      status = owa_readlob(c, octx, r, pmimetype, pmimetype,
                                           (char *)0, outbuf);

                      debug_sql(octx, "docload", pidstr, status, (char *)0);
                  }
                  else
                  {
                      morq_set_status(r, HTTP_INTERNAL_SERVER_ERROR, (char *)0);
                      htp_error(r, "FILE ERROR");
                      morq_write(r,"<p>File download not supported</p>\n", -1);
                      htp_error(r, (char *)0);
                  }
                }
                else if (!fpath) /* Get page from OWA normally */
                {
                    status = owa_getpage(c, octx, r, physical, outbuf,
                                         &rstatus, owa_req, rset_flag);
                    debug_sql(octx, "getpage", pidstr, status, (char *)0);
                }
                else /* File download, return mime-typed content */
                {
                  c->ncflag |= (octx->ncflag & (UNI_MODE_USER | UNI_MODE_RAW));
#ifndef NO_FILE_CACHE
                  if (long_flag == 2)
                  {
                      str_prepend(outbuf, octx->doc_file);
                      status = OCI_SUCCESS;
                      if (owa_download_file(octx, r, outbuf, pmimetype, (ub4)0,
                                            outbuf + str_length(outbuf) + 1))
                      {
                          morq_set_status(r, HTTP_INTERNAL_SERVER_ERROR,
                                          (char *)0);
                          htp_error(r, "FILE ERROR");
                          morq_print_str(r,
                                         "<p>File %s could not be read</p>\n",
                                         outbuf);
                          htp_error(r, (char *)0);
                      }
                  }
                  else
#endif
                  if (long_flag)
                    status = owa_readlong(c, octx, r, fpath, pmimetype,
                                          physical, outbuf, *prawchar & 0xFF);
                  else
                    status = owa_readlob(c, octx, r, fpath, pmimetype,
                                         physical, outbuf);

                  c->ncflag &= ~(UNI_MODE_USER | UNI_MODE_RAW);
                  debug_sql(octx, "readfile", pidstr, status, (char *)0);
                }
            }
        }
    }
    else if ((status == LOGON_ERR) || (!(octx->authrealm)))
    {
        ++retrycount;
    }
    if ((status != OCI_SUCCESS) && (retrycount == 0))
    {
        ++retrycount;
        cstatus = sql_disconnect(c);
        debug_sql(octx, "retry", pidstr, cstatus, (char *)0);
        goto retry;
    }

    debug_sql(octx, "cleanup", pidstr, status, errbuf);

    /*
    ** Flush the socket to the client but not if we are returning an error
    ** status to Apache. The flush call will force out a response header
    ** if this is the very first write to the client, and the subsequent
    ** Apache error page won't render properly.
    */
    if (status == OCI_SUCCESS)
      if (rstatus == 0)
        morq_write(r, (char *)0, 0);

    if (octx->altflags & ALT_MODE_LOGGING)
        owa_log_send(octx, r, c, owa_req);

    cstatus = put_connection(octx, status, pidstr, c, &cdefault);

    if (status == OCI_SUCCESS)
        ++sphase;
    else
        cstatus = status;

    if (diagflag & ~(DIAG_SQL | DIAG_MEMORY | DIAG_THREADS))
        debug_status(octx, pidstr, rstatus, cstatus);

    if ((status == LOGON_ERR) && (octx->authrealm) && (outbuf))
    {
        /*
        ** When logon failure occurs due to invalid username/password,
        ** re-challenge user if in authrealm mode
        */
        issue_challenge(r, octx->authrealm, append_timestamp);
        return(rstatus);
    }

    if (status != OCI_SUCCESS)
    {
        if (diagflag & DIAG_ERROR)
            debug_out(octx->diagfile, "Error on request [%s], PID: %s\n",
                      spath, pidstr, 0, 0);

        if (octx->sqlerr_uri)
          if (issue_redirect(r, octx, pstatus, spath, &rstatus, errbuf))
            return(rstatus);

        return(sql_error(r, &cdefault, status, sphase, diagflag, octx->diagfile));
    }
    if (cdefault.mem_err > 0)
        return(mem_error(r, cdefault.mem_err, diagflag));
    return(rstatus);
}

/*
** Handle DAV-like requests
*/
int owa_dav_request(owa_context *octx, request_rec *r,
                    char *req_args, char *req_uri, int req_method)
{
    /* ### STUB: NOT YET SUPPORTED ### */
    return(HTTP_NOT_IMPLEMENTED);
}

/*
** Close connections in the connection pool and leave it offline.
** Designed to be called on process exit to clean up.
*/
void owa_cleanup(owa_context *octx)
{
    connection *c;
    sword       status;
    char        errbuf[ERRBUF_SIZE];

    if (!(octx->init_complete)) return;

    /* Clean up connection pool, close OCI connections */
    do
    {
        c = lock_connection(octx, (char *)0);
        if (c)
        {
            if (c->c_lock == C_LOCK_INUSE)
            {
                *errbuf = '\0';
                c->errbuf = errbuf;
                c->mem_err = 0;
                status = sql_disconnect(c);
                if (status)
                {
                    /* ### NOTHING TO DO ### */
                }
            }
            c->c_lock = C_LOCK_OFFLINE;
            if (c->sockctx) c->sockctx = owa_log_free(c->sockctx);
            unlock_connection(octx, c);
        }
    } while (c);
}
