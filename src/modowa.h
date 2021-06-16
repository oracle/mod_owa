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
** File:         modowa.h
** Description:  Interfaces between modowa files.
** Authors:      Doug McMahon      Doug.McMahon@oracle.com
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
**
** History
**
** 08/17/2000   D. McMahon      Created
** 05/31/2001   D. McMahon      Began revisioning with 1.3.9/2.3.9
** 06/16/2001   D. McMahon      Added support for RAW input/output bindings
** 09/01/2001   D. McMahon      Added array and scalar rounding
** 09/07/2001   D. McMahon      Added TIMING diagnostic
** 02/11/2002   D. McMahon      Add OwaDocFile
** 03/27/2002   D. McMahon      Add morq_stream and rework morq_read
** 04/12/2002   D. McMahon      Fix definition of TIMING diagnostic
** 06/12/2002   D. McMahon      Add nls_check_utf8 and nls_conv_utf8
** 08/08/2002   D. McMahon      Add owa_dav_request and related functions
** 10/10/2002   D. McMahon      Add ERROR diagnostic flag
** 10/10/2002   D. McMahon      Add prototype for socket_test
** 11/14/2002   D. McMahon      Add sql_bind_vcs and SQLT_VCS/SQLT_VBI
** 03/17/2003   D. McMahon      Add NOMERGE to OwaAlternate
** 05/26/2003   D. McMahon      Add random number generator prototypes
** 06/06/2003   D. McMahon      Support special error page for SQL errors
** 01/19/2004   D. McMahon      Add sessioning support
** 09/14/2004   D. McMahon      Add HTBUF_PLSQL_MAX
** 01/16/2006   D. McMahon      Made nls_length a public API
** 02/17/2006   D. McMahon      Add hook for describe caching
** 06/14/2006   D. McMahon      Make mem_find() public
** 09/30/2006   D. McMahon      Change to os_thrhand for threads
** 02/02/2007   D. McMahon      Add OwaLDAP directive
** 04/12/2007   D. McMahon      Add OwaDocTable directive
** 04/15/2007   D. McMahon      Add owa_getheader() and owa_lobtable()
** 04/27/2007   D. McMahon      Add file_lock()
** 05/07/2007   D. McMahon      Add mem_realloc() and mem_compare()
** 05/10/2007   D. McMahon      Add str_btox(), str_xtob()
** 09/18/2007   D. McMahon      Remove file_lock()
** 08/20/2008   C. Fischer      Added OwaWait and OwaOptmizer
** 08/31/2008   C. Fischer      Added owa_log_send()
** 07/04/2009   D. McMahon      Added owa_wlobtable()
** 03/01/2011   D. McMahon      Added InvalidThread()
** 04/18/2011   D. McMahon      util_scramble
** 11/12/2011   D. McMahon      Add OwaCharsize
** 03/28/2012   D. McMahon      Add OwaContentType support
** 05/21/2012   D. McMahon      Change signature of owa_wlobtable
** 05/21/2012   D. McMahon      Add socket_flush
** 06/01/2012   D. McMahon      Change owa_wlobtable again
** 11/27/2012   D. McMahon      GCC fixes, Win-64 porting
** 12/17/2012   D. McMahon      Add util_iso_time
** 04/12/2013   D. McMahon      Add support for REST operations
** 06/06/2013   D. McMahon      Add sql_bind_cursor for REF cursor operations
** 06/21/2013   D. McMahon      Add util_csv_escape
** 07/26/2013   D. McMahon      Add cdisp to owa_getheader
** 09/19/2013   D. McMahon      Changes needed for 64-bit content
** 09/20/2013   D. McMahon      Expose util_component_to_stamp
** 09/24/2013   D. McMahon      Add errinfo argument to sql_connect
** 09/26/2013   D. McMahon      Get client IP address after socket_accept
** 09/26/2013   D. McMahon      Add ALT_MODE_IPADDR
** 10/21/2013   D. McMahon      Changed interface to morq_set_status()
** 12/23/2014   D. McMahon      Add marker argument to owa_wlobtable()
** 04/24/2015   D. McMahon      Add os_get_user()
** 05/07/2015   D. McMahon      Make morq_get_range use 64-bit ints
** 05/11/2015   D. McMahon      Allow file_arg to be streaming
** 09/11/2015   D. McMahon      Add nls_sanitize_header and nls_copy_identifier
** 10/13/2015   D. McMahon      Add sql_bind_chr()
** 10/26/2015   J. Chung        OSX port: malloc inclusion
** 12/07/2015   D. McMahon      Add HTBUF_ARRAY_MAX_WIDTH
** 02/25/2017   D. McMahon      Add os_env_dump()
** 05/29/2018   D. McMahon      Add slotnum to connection structure
** 08/14/2018   D. McMahon      Limit HTBUF_ARRAY_MAX_WIDTH to 32511
** 08/20/2018   D. McMahon      Allow PATCH when OwaHttp == "REST"
** 10/18/2018   D. McMahon      Add dad_name.
*/

#ifndef MODOWA_H
#define MODOWA_H

#ifndef MODOWA_WINDOWS
# ifdef WIN32
#  define MODOWA_WINDOWS
# else
#  ifdef WIN64
#   define MODOWA_WINDOWS
#  endif
# endif
#endif

#define MODOWA_VERSION_STRING "mod_owa 2.11.12";

#ifdef MODOWA_WINDOWS

#ifdef WITH_APACHE
# define WITHOUT_WINDOWS /* ### windows.h INCOMPATIBLE WITH APACHE HEADERS */
#endif

#ifndef WITHOUT_WINDOWS
/*
** NT version should rely exclusively on Win32 APIs, no calls to the
** C run-time library so that modowa has no dependency on installing it.
*/
# ifndef MODOWA_WINDOWS_H
#  include <windows.h>
#  define MODOWA_WINDOWS_H
# endif
#endif /* !WITHOUT_WINDOWS */

#else /* !MODOWA_WINDOWS == UNIX */

/*
** Standard Unix headers
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#ifndef OSX /* OSX defines malloc in stdlib.h */
#include <malloc.h>
#endif /* !OSX */
#include <ctype.h>
#include <alloca.h>

#endif /* UNIX */

#ifndef OLD_RESET
#define RESET_AFTER_EXEC
#endif

#ifndef WITH_APACHE
#ifdef APACHE_LINKAGE

#define HTTP_CONTINUE                      100
#define HTTP_OK                            200
#define HTTP_CREATED                       201
#define HTTP_ACCEPTED                      202
#define HTTP_NO_CONTENT                    204
#define HTTP_MULTI_STATUS                  207
#define HTTP_MOVED_PERMANENTLY             301
#define HTTP_MOVED_TEMPORARILY             302
#define HTTP_NOT_MODIFIED                  304
#define HTTP_TEMPORARY_REDIRECT            307
#define HTTP_BAD_REQUEST                   400
#define HTTP_UNAUTHORIZED                  401
#define HTTP_FORBIDDEN                     403
#define HTTP_NOT_FOUND                     404
#define HTTP_REQUEST_TIME_OUT              408
#define HTTP_INTERNAL_SERVER_ERROR         500
#define HTTP_NOT_IMPLEMENTED               501
#define HTTP_SERVICE_UNAVAILABLE           503
#define HTTP_GATEWAY_TIME_OUT              504

#define OK  0  /* Apache signal that module has handled request stage */

typedef struct request_rec request_rec;

#endif /* APACHE_LINKAGE */
#else
# ifndef APACHE_LINKAGE
#  define APACHE_LINKAGE
# endif
#endif /* !WITH_APACHE */

#ifdef WITH_OCI
/*
** Oracle headers
*/
#include <oratypes.h>
#include <oci.h>

#endif /* WITH_OCI */

/*
** Mappings to match Apache method codes
*/
#define DAV_METHOD_GET                   0
#define DAV_METHOD_PUT                   1
#define DAV_METHOD_POST                  2
#define DAV_METHOD_DELETE                3
#define DAV_METHOD_CONNECT               4
#define DAV_METHOD_OPTIONS               5
#define DAV_METHOD_TRACE                 6
#define DAV_METHOD_PATCH                 7
#define DAV_METHOD_PROPFIND              8
#define DAV_METHOD_PROPPATCH             9
#define DAV_METHOD_MKCOL                10
#define DAV_METHOD_COPY                 11
#define DAV_METHOD_MOVE                 12
#define DAV_METHOD_LOCK                 13
#define DAV_METHOD_UNLOCK               14
#define DAV_METHOD_VERSION_CONTROL      15
#define DAV_METHOD_CHECKOUT             16
#define DAV_METHOD_UNCHECKOUT           17
#define DAV_METHOD_CHECKIN              18
#define DAV_METHOD_UPDATE               19
#define DAV_METHOD_LABEL                20
#define DAV_METHOD_REPORT               21
#define DAV_METHOD_MKWORKSPACE          22
#define DAV_METHOD_MKACTIVITY           23
#define DAV_METHOD_BASELINE_CONTROL     24
#define DAV_METHOD_MERGE                25
#define DAV_METHOD_INVALID              26



/*
** Diagnostic flags
*/
#define DIAG_COMMAND    0x0001
#define DIAG_ARGS       0x0002
#define DIAG_CGIENV     0x0004
#define DIAG_POOL       0x0008
#define DIAG_HEADER     0x0010
#define DIAG_COOKIES    0x0020
#define DIAG_CONTENT    0x0040
#define DIAG_RESPONSE   0x0080
#define DIAG_TIMING     0x0100
/*
** Mode flags
*/
#define DIAG_SQL        0x0200
#define DIAG_MEMORY     0x0400
#define DIAG_THREADS    0x0800
#define DIAG_ERROR      0x1000

/*
** Keepalive states
*/
#define OWA_KEEPALIVE_OFF   0
#define OWA_KEEPALIVE_ON    1
#define OWA_KEEPALIVE_OK    2

/*
** LOB mode flags
*/
#define LOB_MODE_BIN    1           /* Binary LOBs only                  */
#define LOB_MODE_CHAR   2           /* Binary and Character LOBs         */
#define LOB_MODE_NCHAR  3           /* Binary, Character, and NCHAR LOBs */
#define LOB_MODE_BFILE  4           /* All LOB types including BFILE     */
/*
** Long mode flags
*/
#define LONG_MODE_RETURN_LEN  1     /* document_proc returns content len */
#define LONG_MODE_FETCH_LEN   2     /* Statement returns content length  */

/*
** Alternate OWA mode flags
*/
#define ALT_MODE_KEEP     0x0001    /* Keep package state          */
#define ALT_MODE_RAW      0x0002    /* Add RAW buffer for GET_PAGE */
#define ALT_MODE_LOBS     0x0004    /* Use LOBs for GET_PAGE       */
#define ALT_MODE_CACHE    0x0008    /* Enable GET_PAGE caching     */
#define ALT_MODE_SETSEC   0x0010    /* Enable OWA_SEC workaround   */
#define ALT_MODE_GETRAW   0x0020    /* Enable GET_PAGE_RAW         */
#define ALT_MODE_NOMERGE  0x0040    /* Disable URL merge for POSTs */
#define ALT_MODE_CGITIME  0x0080    /* Pass timestamp to CGI       */
#define ALT_MODE_CGIPOST  0x0100    /* Pass POST args to CGI       */
#define ALT_MODE_LOGGING  0x0200    /* Enable logging callback     */
#define ALT_MODE_IPADDR   0x0400    /* Pass client IP address      */

/*
** Describe mode flags
*/
#define DESC_MODE_STRICT        0   /* Do not do describe           */
#define DESC_MODE_NORMAL        1   /* Do the collection describe   */
#define DESC_MODE_RELAXED       2   /* Do the advanced describe     */
#define DESC_MODE_POSITIONAL    3   /* Used for positional bindings */

/*
** Unicode mode flags
*/
#define UNI_MODE_NONE           0   /* Always use VARCHAR                  */
#define UNI_MODE_USER           1   /* Use NCHARs for user procecures only */
#define UNI_MODE_FULL           2   /* Use NCHARs for all OWA operations   */
#define UNI_MODE_RAW            4   /* Use RAWs for user procedures only   */

/*
** Reset mode flags
*/
#define RSET_MODE_NORMAL        0   /* Use RESET_PACKAGE                   */
#define RSET_MODE_FULL          1   /* Use FREE_ALL_RESOURCES              */
#define RSET_MODE_LAZY          2   /* Use REINITIALIZE                    */
#define RSET_MODE_INIT          3   /* Call HTP.INIT                       */

/*
** Authorization flags
*/
#define OWA_AUTH_NONE        0
#define OWA_AUTH_INIT        1
#define OWA_AUTH_CUSTOM      2
#define OWA_AUTH_PACKAGE  0x10

/*
** Apache request table types
*/
#define OWA_TABLE_SUBPROC    1  /* Table of subprocess environment vars    */
#define OWA_TABLE_HEADIN     2  /* Table of request header elements        */
#define OWA_TABLE_HEADOUT    3  /* Table of response header elements       */
#define OWA_TABLE_HEADERR    4  /* Table of response error header elements */

/*
** Cache performance flags
*/
#define CACHE_DEF_LIFE      0x7FFFFFFF  /* Infinite life                 */
#define CACHE_MAX_SIZE      0x1000000   /* Largest file size 16M         */
#define CACHE_MAX_ALIASES   8           /* Reallocation unit for aliases */

/*
** Buffer sizes
*/
#define HTBUF_LINE_CHARS     256    /* Size of GET_PAGE line value    */
#define HTBUF_LINE_LENGTH    768    /* Size of GET_PAGE line value X3 */
#define HTBUF_ARRAY_SIZE     256    /* Maximum array size             */
#define HTBUF_HEADER_MAX    4000    /* Maximum size of header element */
#define HTBUF_PLSQL_MAX    32512    /* Maximum size of PL/SQL string  */
#define HTBUF_BLOCK_SIZE   32768    /* Size of a file upload block    */
#define HTBUF_BLOCK_READ   65536    /* Size of a file download block  */
#define HTBUF_PARAM_CHUNK     64    /* Increment for parameter allocs */
#define HTBUF_ENV_MAX   HTBUF_HEADER_MAX
#define HTBUF_ENV_NAM   80          /* HTTP header element name size  */
#define HTBUF_ENV_ARR   40          /* Number of HTTP elements max    */

/*
** This is the maximum width of a string passed to a PL/SQL index-by table.
** The limit is imposed by OCI and in the past was as low as 2000 bytes,
** which was then increased to 4000 bytes. This was then increased to the
** OCI limit of 32512 (but not quite all the way to the PL/SQL limit of
** 32767 until very recently). For now, 32512 is deemed portable back to
** the oldest OCI supported.
*/
#ifdef OLD_MAX_STRING
# define HTBUF_ARRAY_MAX_WIDTH  (HTBUF_HEADER_MAX-1)
#else
# define HTBUF_ARRAY_MAX_WIDTH  (HTBUF_PLSQL_MAX-1)
#endif

/*
** PL/SQL-related constants
*/
#define PLSQL_TAB     251                 /* Data type for PL/SQL table      */
#define PLSQL_ERR     6550                /* Oracle PL/SQL error code        */
#define PLSQL_ARGERR  "PLS-00306"         /* Wrong number or types of args   */
#define LOGON_ERR     1017                /* Oracle logon failure            */
#define SQL_NAME_MAX  128                 /* Max size of a column/param name */
#define SQL_MAX_COLS  1000                /* Max number of SQL columns       */

/*
** Connection pool locking states
*/
#define C_LOCK_UNUSED           0
#define C_LOCK_NEW              1
#define C_LOCK_AVAILABLE        2
#define C_LOCK_INUSE            3
#define C_LOCK_OFFLINE          4
#define C_LOCK_UNKNOWN          7
#define C_LOCK_MAXIMUM          8

/*
** Shared memory constants
*/
#define SHMEM_WAIT_MAX       100     /* milliseconds */
#ifdef MODOWA_WINDOWS
#define SHMEM_WAIT_INFINITE  INFINITE
#else
#define SHMEM_WAIT_INFINITE  0xFFFF  /* 1 minute */
#endif

/*
** Global constants
*/
#define LONG_MAXSZ          0x7FFFFFFF  /* Maximum value for a signed long */
#define LONG_MAXSTRLEN      48          /* Maximum string width of oranum  */
#define ORA_MAXCOLNAME      32          /* Maximum length of column name   */
#define DESCRIBE_MAX        256         /* Maximum describe values         */

/*
** Max size of a content upload is either (2^31 - 1)
** or (if size_t is also 64 bits) then (2^63 - 1)
*/
#define DEFAULT_MAX(x)                    \
  if ((x) < 0)                            \
  {                                       \
    (x) = LONG_MAXSZ;                     \
    if (sizeof(size_t) == sizeof((x)))    \
      (x) = ((x) << 32) | ((x) << 1) | 1; \
  }

#ifdef MODOWA_WINDOWS

#define mem_zero(ptr, sz)    ZeroMemory((void *)(ptr),(DWORD)(sz))
#define mem_fill(ptr, sz, v) FillMemory((void *)(ptr),(DWORD)(sz),(BYTE)(v))
#define mem_copy(dt, sc, sz) CopyMemory((void *)(dt),(void *)(sc),(SIZE_T)(sz))
#define mem_move(dt, sc, sz) MoveMemory((void *)(dt),(void *)(sc),(SIZE_T)(sz))
#define os_str_length        lstrlen
#define os_str_copy          lstrcpy
#define os_str_print         wsprintf
#define os_alloca            _alloca
#define os_objhand           HANDLE
#define os_thrhand           HANDLE
#define os_objptr            HANDLE
#define os_socket            SOCKET
#define os_dir_separator     '\\'
#define os_nullthrhand       INVALID_HANDLE_VALUE
#define os_nullfilehand      INVALID_HANDLE_VALUE
#define os_nulldirhand       INVALID_HANDLE_VALUE
#define os_badsocket         INVALID_SOCKET
#define os_nullmutex         INVALID_HANDLE_VALUE
#define InvalidThread(th)    ((th) == os_nullthrhand)
#define InvalidFile(fp)      ((fp) == os_nullfilehand)
#define InvalidMutex(mx)     ((mx) == os_nullfilehand)

#else

#define mem_zero(ptr, sz)    memset((void *)(ptr),0,(sz))
#define mem_fill(ptr, sz, v) memset((void *)(ptr),(v),(sz))
#define mem_copy(dt, sc, sz) memcpy((void *)(dt),(void *)(sc),(sz))
#define mem_move(dt, sc, sz) memmove((void *)(dt),(void *)(sc),(sz))
#define os_str_length        strlen
#define os_str_copy          strcpy
#define os_str_print         sprintf
#define os_alloca            alloca
#define os_objhand           int
#define os_thrhand           unsigned long
#define os_objptr            void *
#define os_socket            int
#define os_dir_separator     '/'
#define os_nullthrhand       (~0)
#define os_nullfilehand      -1
#define os_nulldirhand       (void *)0
#define os_badsocket         -1
#define os_nullmutex         (void *)0
#define InvalidThread(th)    ((!th) || ((th) == os_nullthrhand))
#define InvalidFile(fp)      ((fp) < 0)
#define InvalidMutex(mx)     (!(mx))

#endif

#define un_long unsigned long

#ifdef MODOWA_WINDOWS
#define long_64 _int64
#else
#define long_64 long long
#endif

#ifdef WITH_OCI
/*
** Type for passing SQL argument vectors
*/
typedef struct arg_record
{
    int    nargs;
    char **names;
    char **values;
    sb4   *widths;
    ub4   *counts;
    int    arrsz;
} arg_record;
#endif

/*
** Type for passing lists of name/value pairs
*/
typedef struct env_record
{
    int      count;
    char    *names;
    char    *values;
    int      nwidth;
    int      vwidth;
    char    *authuser;
    char    *authpass;
    char    *session;
    un_long  ipaddr;
} env_record;

/*
** Timestamp structure
*/
typedef struct tstamp
{
    un_long secs;
    un_long musecs;
} tstamp;

/*
** Sub-context for logging sockets (visible only in modowa.c)
*/
typedef struct owa_log_socks owa_log_socks;

#define OWA_LOG_ADDR_WIDTH 80
#define OWA_LOG_ADDR_COUNT 20

#ifdef WITH_APACHE
/*
** Structure used by the logging service
** ### This work isn't done yet.  It's not clear I can get this to
** ### work because the apr_sock stuff doesn't exist in Apache 1.3.
** ### Also some of the stuff here (e.g. the log_addrs[]) may belong
** ### outside this code.
*/
struct owa_log_socks
{
    long_64        log_connect_ts;
    long_64        log_lookup_ts;
    char           log_addrs[OWA_LOG_ADDR_WIDTH * OWA_LOG_ADDR_COUNT];
    int            log_addr_i;
    int            log_addr_n;
    apr_pool_t    *mem_pool;
    apr_socket_t  *log_socket;
};
#endif

/*
** Handle to hold OCI connection information
*/
typedef struct connection connection;
#ifdef WITH_OCI
/* ### EVENTUALLY, MOVE ENTIRELY INTO owasql.c ### */
struct connection
{
    OCIEnv        *envhp;
    OCIError      *errhp;
    OCIServer     *srvhp;
    OCISvcCtx     *svchp;
    OCISession    *seshp;
    OCIStmt       *stmhp1;
    OCIStmt       *stmhp2;
    OCIStmt       *stmhp3;
    OCIStmt       *stmhp4;
    OCIStmt       *stmhp5;
    OCILobLocator *pblob;
    OCILobLocator *pclob;
    OCILobLocator *pnlob;
    OCILobLocator *pbfile;
    OCIStmt       *rset;
    char          *session;
    char          *lastsql;
    char          *errbuf;
    int            mem_err;
    int            c_lock;
    long_64        timestamp;
    int            ncflag;
    ub2            csid;
    ub2            blob_ind;
    ub2            clob_ind;
    ub2            nlob_ind;
    ub2            bfile_ind;
    ub2            out_ind;
    ub2            rcode;
    owa_log_socks *sockctx;        /* Back-pointer to logging sockets */
    int            slotnum;
};

#ifndef OCI_UCS2ID
# define OCI_UCS2ID 1000
#endif

#define OraCursor OCIStmt *

#endif

#define NEED_READ_DATA  OCI_NEED_DATA
#define NEED_WRITE_DATA OCI_NEED_DATA

/*
** Sub-context for multi-threading support (visible only in modowa.c)
*/
typedef struct owa_mt_context owa_mt_context;

/*
** Structure to hold a cache alias
*/
typedef struct alias
{
    char    *logname;
    char    *physname;
    un_long  lifespan;
} alias;

/*
** Structure to hold an environment name/value pair
*/
typedef struct envstruct
{
    char    *name;
    char    *value;
} envstruct;

/*
** Structure to hold a describe result
*/
typedef struct descstruct descstruct;
struct descstruct
{
    char       *pname;  /* Procedure or package name */
    char       *pargs;  /* Collection bind argument names */
    int         flex2;  /* Flags 2-argument flex mode */
    descstruct *next;   /* Next describe result */
};

/*
** Structure to hold LDAP result
*/
typedef struct ldapstruct ldapstruct;
struct ldapstruct
{
    char       *luser;  /* LDAP username */
    char       *lpass;  /* LDAP password */
    char       *sess;   /* Session string */
    char       *dbconn; /* Database connect information */
    ldapstruct *next;   /* Next result */
};

/*
** Shared memory context
*/
typedef struct shm_context
{
    size_t       mapsize;
    size_t       memthresh;
    size_t       filthresh;
    int          pagesize;
    os_objhand   map_fp;
    void        *map_ptr;
    os_objhand   f_mutex; /* HANDLE on Windows, int on Unix */
} shm_context;

/*
** Structure used to hold information associated with request
** so that it can be easily accessed in all mod_owa functions that
** are called to perform work on the request.
*/
typedef struct owa_request
{
    un_long  wait_time;
    un_long  lock_time;
    un_long  connect_time;
    un_long  current_time;
    char    *req_args;
    char    *post_args;
    char    *logbuffer;
    int      loglength;
} owa_request;

/*
** Main context area for an OWA Location
*/
typedef struct owa_context owa_context;
struct owa_context
{
    owa_mt_context *mtctx;          /* Back-pointer to module configuration */
    char           *location;       /* Location name                        */
    char           *oracle_userid;  /* Oracle username/password@dbname      */
    char           *dad_name;       /* DAD name string                      */
    char           *doc_path;       /* Document path prefix                 */
    char           *doc_long;       /* Document LONG/LONG RAW path prefix   */
    char           *doc_file;       /* Document LONG/LONG RAW path prefix   */
    char           *doc_gen;        /* Generated document path prefix       */
    char           *doc_proc;       /* Document read access procedure       */
    char           *doc_start;      /* First-page procedure                 */
    char           *doc_table;
    char           *doc_column;
    char           *ora_before;
    char           *ora_after;
    char           *ora_proc;
    char           *ora_ldap;
    char           *diagfile;
    int             diagflag;
    int             descmode;
    char           *desc_schema;
    char           *alternate;
    int             altflags;
    int             rsetflag;
    int             ncflag;
    char           *nls_cs;
    char           *nls_lang;
    char           *nls_terr;
    char           *nls_datfmt;
    char           *nls_tz;
    char           *verstring;
    char           *froot;
    char           *authrealm;
    char           *session;
    char           *defaultcs;
    int             poolsize;
    long_64         upmax;
    int             version;
    int             lobtypes;
    int             lontypes;
    int             nls_init;
    int             ora_csid;
    int             dad_csid;
    int             db_csid;
    int             db_charsize;
    int             init_complete;
    char           *dav_handler;
    int             dav_mode;
    int             keepalive_flag; /* ### Written at run-time on Unix */
    int             shm_offset;     /* ### Written at run-time on Unix */
    int             multithread;
    un_long         crtl_subnet;
    un_long         crtl_mask;
    int             realpid;
    int             pool_wait_ms;
    int             pool_wait_abort;
    char           *optimizer_mode;
    connection     *c_pool;
    int             poolstats[C_LOCK_MAXIMUM];
    char           *reset_stmt;
    char            cgi_stmt[512];
    char            sec_stmt[512];
    char            get_stmt[512];
    char           *lob_stmt;
    char           *cpmv_stmt;
    char           *res_stmt;
    char           *prop_stmt;
    char           *sqlerr_uri;
    int             nlifes;
    alias          *lifes;
    int             nflex;
    char          **flexprocs;
    int             nreject;
    char          **rejectprocs;
    int             nenvs;
    envstruct      *envvars;
    un_long         scale_round;
    un_long         arr_round;
    char           *ref_root_tag;
    char           *ref_row_tag;
    char           *ref_prefix;
    char           *ref_nsuri;
    char           *logbuffer;
    char           *default_ctype;
    char           *error_ctype;
    descstruct     *desc_cache;
    ldapstruct     *ldap_cache;
    shm_context    *mapmem;
    owa_context    *next;
};

/*
** Context used for directory reading
*/
typedef struct dir_record
{
    os_objptr dh;
    un_long   fsize;
    un_long   ftime;
    int       dir_flag;
    int       attrs;
    char      fname[HTBUF_HEADER_MAX];
} dir_record;


/*
** Structure used for random number generator states
*/
#define RAND_MAX_STATES  64
typedef struct randstate
{
    int     length;                             /* Length of the cycle */
    int     tap;                                /* Feedback tap */
    int     ind;                                /* Index to next value */
    int     carry;                              /* Carry flag */
    un_long state[RAND_MAX_STATES];             /* Previous states */
} randstate;

typedef struct file_arg file_arg;
struct file_arg
{
    file_arg *next;
    char     *filepath;
    char     *content_type;
    void     *data;
    size_t    len;
    size_t    total;
    int       param_num;
};

/*
** Argument vector
*/
typedef struct http_args
{
    int        arrsz;    /* Current allocated size of the arrays */
    int        nargs;    /* Current number of arguments in arrays */
    char     **names;    /* Array of names */
    char     **values;   /* Array of values */
    file_arg  *filelist; /* Linked list of file upload arguments */
} http_args;

/*
** String Functions
*/
#define str_length      os_str_length
#define str_copy        os_str_copy

int   str_concat(char *dest, int dstart, const char *src, int maxlen);

int   str_compare(const char *s1, const char *s2, int maxlen, int caseflag);

char *str_char(const char *sptr, int ch, int reverse_flag);

char *str_substr(const char *sptr, const char *ssub, int caseflag);

char *str_dup(const char *sptr);

int   str_chrcmp(char *a, char *b, int caseflag);

void  str_prepend(char *base_str, char *prefix_str);

int   str_ltoa(long_64 i, char *s);

int   str_itoa(int i, char *s);

int   str_itox(un_long i, char *s);

int   str_atoi(char *s);

un_long str_atox(char *s);

char *str_btox(void *p, char *s, int len);

int   str_xtob(char *s, void *p);

char *mem_find(char *mptr, long_64 mlen, const char *mpat, int plen);

int   mem_compare(char *ptr1, int len1, char *ptr2, int len2);

/*
** OS Functions
*/

void    os_exit(int status);

int     os_get_pid(void);

un_long os_get_tid(void);

un_long os_get_time(un_long *musec);

long_64 os_get_component_time(int gmt_flag);

void    os_milli_sleep(int ms);

un_long os_page_round(un_long sz);

int     os_get_errno(void);

void    os_set_errno(int e);

void    os_fd_close_exec(int fd);

int     os_get_user(char *username, int bufsize);

char   *os_env_get(char *name);

char   *os_env_set(char *name, char *value);

char   *os_env_dump(char *prior, un_long position);

/*
** Memory functions
*/

void *mem_alloc(size_t sz);

void *mem_zalloc(size_t sz);

void *mem_realloc(void *ptr, size_t sz);

void  mem_free(void *ptr);

/*
** IPC Functions
*/

os_objhand  os_sem_create(char *name, int sz, int secure_flag);

int         os_sem_acquire(os_objhand mh, int timeout);

int         os_sem_release(os_objhand mh);

int         os_sem_destroy(os_objhand mh);

os_objptr   os_mutex_create(char *name, int secure_flag);

int         os_mutex_acquire(os_objptr mh, int timeout);

int         os_mutex_release(os_objptr mh);

int         os_mutex_destroy(os_objptr mh);

os_objptr   os_cond_init(char *name, int sz, int secure_flag);

int         os_cond_wait(os_objptr mh, int timeout);

int         os_cond_signal(os_objptr mh);

int         os_cond_destroy(os_objptr mh);

/*
** File I/O Functions
*/

os_objhand  file_open_temp(char *fpath, char *tempname, int tmax);

os_objhand  file_open_write(char *fpath, int append_flag, int share_flag);

void        file_seek(os_objhand fp, long offset);

int         file_write_data(os_objhand fp, char *buffer, int buflen);

int         file_write_text(os_objhand fp, char *str, int slen);

int         file_read_data(os_objhand fp, char *buffer, int buflen);

void        file_close(os_objhand fp);

#ifndef NO_FILE_CACHE

void       *os_virt_alloc(size_t sz);

void        os_virt_free(void *ptr, size_t sz);

os_objhand  file_open_read(char *fpath, un_long *fsz, un_long *fage);

os_objhand  file_map(os_objhand fp, un_long fsz,
                     char *mapname, int write_flag);

void       *file_view(os_objhand hnd, un_long fsz, int write_flag);

void        file_unmap(os_objhand hnd, void *ptr, un_long fsz);

void        file_delete(char *fname);

void        file_move(char *src, char *dest);

int         file_readdir(char *dirname, dir_record *dr);

int         file_mkdir(char *name, int mode);

int         file_rmdir(char *name);

os_objhand  os_shm_get(char *mapname, un_long fsz);

void       *os_shm_attach(os_objhand hnd, un_long fsz);

void        os_shm_detach(void *ptr);

void        os_shm_destroy(os_objhand hnd);

#endif /* FILE_CACHE */

/*
** Thread Functions
*/

os_thrhand thread_spawn(void (*thread_start)(void *),
                        void *thread_context,
                        un_long *out_tid);

void       thread_exit(void);

void       thread_shutdown(os_thrhand th);

void       thread_join(os_thrhand th);

void       thread_detach(os_thrhand th);

void       thread_suspend(os_thrhand th);

void       thread_resume(os_thrhand th);

void       thread_cancel(os_thrhand th);

void       thread_check(void);

void       thread_block_signals(void);

/*
** Socket Functions
*/
#ifndef NO_SOCKETS

void      socket_init(void);

void      socket_close(os_socket sock);

os_socket socket_listen(int port, char *ipaddr, int backlog);

os_socket socket_accept(os_socket sock, char *buf, int bufsz);

os_socket socket_connect(int port, char *ipaddr);

int       socket_write(os_socket sock, char *buffer, int buflen);

int       socket_read(os_socket sock, char *buffer, int buflen);

int       socket_flush(os_socket sock);

int       socket_test(os_socket *sock, int nsock, int ms);

#endif

/*
** Debug log function
*/

void debug_out(char *diagfile, char *fmt,
               char *s1, char *s2, int i1, int i2);

/*
** Oracle NLS-related functions
*/

int   nls_cstype(int cs_id);

int   nls_csx_from_oracle(char *csname);

int   nls_csx_from_iana(char *csname);

int   nls_csid(int idx);

char *nls_iana(int idx);

int   nls_check_utf8(char *buf, int nbytes);

int   nls_conv_utf8(char *inbuf, char *outbuf);

int   nls_utf8_char(char *buf_in, int *nbytes);

int   nls_count_chars(int cs_id, char *outbuf, un_long *nbytes);

char *nls_find_delimiter(char *filepath, int cs_id);

int   nls_length(int cs_id, char *s, int maxbytes);

void  nls_validate_path(char *spath, int cs_id);

void  nls_sanitize_header(char *sptr);

int   nls_copy_identifier(char *dest, char *src, int cs_id);

/*
** Utilities
*/
char    *util_get_method(int m);

void     util_print_time(un_long t, char *outbuf, un_long *ms);

void     util_print_component_time(long_64 tval, char *outbuf);

void     util_iso_time(long_64 tval, char *outbuf);

void     util_header_time(long_64 tval, char *outbuf, char *tz);

void     util_set_mime(char *fpath, char *pmimetype, int bin_flag);

long_64  util_component_to_stamp(long_64 tval);

long_64  util_delta_time(long_64 stime, long_64 etime);

un_long  util_checksum(char *buffer, int buflen);

un_long  util_ipaddr(char *ipstr);

un_long  util_round(un_long n, un_long r);

int      util_decode64(char *inbuf, char *outbuf);

void     util_make_rand(randstate *prand, int length, int tap);

un_long  util_gen_rand(randstate *prand);

void     util_seed_rand(randstate *prand, randstate *srand);

un_long  util_get_rand(randstate *arand, randstate *brand);

void     util_init_rand(randstate *arand, randstate *brand,
                        char *key, int keylen);

int      util_json_escape(char *json_out, char *buf_in,
                          int quote_flag, int uni_flag);

int      util_csv_escape(char *csv_out, char *buf_in, int delim);

int      util_ncname_convert(char *json_out, char *buf_in);

int      util_scramble(char *scramble, int klen,
                       char *instr, int ilen, char *outstr,
                       int direction);

/*
** SQL functions
*/

int   sql_conn_size(int arrsz);

int   sql_init(void);

#ifdef WITH_OCI

sword sql_parse(connection *c, OraCursor stmhp, char *stmt, int slen);

sword sql_fetch(connection *c, OraCursor stmhp, ub4 numrows);

sword sql_exec(connection *c, OraCursor stmhp, ub4 niters, int exact);

sword sql_close_rset(connection *c);

sword sql_transact(connection *c, int rollback_flag);

void  sql_get_nls(connection *c, owa_context *octx);

sb4   sql_get_error(connection *c);

int   sql_get_errpos(connection *c, OraCursor stmhp);

sword sql_connect(connection *c, owa_context *octx,
                  char *authuser, char *authpass, sb4 *errinfo);

sword sql_disconnect(connection *c);

sword sql_define(connection *c, OraCursor stmhp, ub4 pos,
                 dvoid *buf, sb4 buflen, ub2 dtype, dvoid *inds);

sword sql_bind_int(connection *c, OraCursor stmhp, ub4 pos, sb4 *val);

sword sql_bind_long(connection *c, OraCursor stmhp, ub4 pos, long_64 *val);

sword sql_bind_str(connection *c, OraCursor stmhp, ub4 pos,
                   char *str, sb4 slen);

sword sql_bind_chr(connection *c, OraCursor stmhp, ub4 pos,
                   char *str, sb4 slen);

sword sql_bind_raw(connection *c, OraCursor stmhp, ub4 pos,
                   char *rawbuf, ub2 *rlen, sb4 buflen);

sword sql_bind_arr(connection *c, OraCursor stmhp, ub4 pos,
                   char *aptr, ub2 *lens, sb4 awidth, sb2 *inds,
                   ub4 *asize, ub4 amax);

sword sql_bind_rarr(connection *c, OraCursor stmhp, ub4 pos,
                    char *aptr, ub2 *lens, sb4 awidth, sb2 *inds,
                    ub4 *asize, ub4 amax);

sword sql_bind_iarr(connection *c, OraCursor stmhp, ub4 pos,
                    sb4 *ints, ub4 *asize, ub4 amax);

sword sql_bind_vcs(connection *c, OraCursor stmhp, ub4 pos,
                   dvoid *buf, ub2 *lens, sb4 awidth, sb2 *inds,
                   ub4 *asize, ub4 amax, ub4 swidth);

sword sql_bind_stream(connection *c, OraCursor stmhp, ub4 pos, ub2 sql_type);

sword sql_bind_ptrs(connection *c, OraCursor stmhp, ub4 pos,
                    char **aptr, sb4 awidth, ub4 *asize, ub4 amax);

sword sql_bind_lob(connection *c, OraCursor stmhp, ub4 pos, ub2 flag);

sword sql_define_lob(connection *c, OraCursor stmhp, ub4 pos, ub2 flag);

sword sql_get_rowcount(connection *c, OraCursor stmhp, ub4 *rowcount);

sword sql_get_stmt_state(connection *c, OraCursor stmhp, int *fstatus);

sword sql_bind_cursor(connection *c, OraCursor stmhp, ub4 pos);

sword sql_describe_col(connection *c, OraCursor stmhp, ub4 pos,
                       int *width, char *colname, int cs_expand);

sword sql_write_piece(connection *c, OraCursor stmhp,
                      dvoid *piecebuf, ub4 *nbytes, int pieceflag);

sword sql_read_piece(connection *c, OraCursor stmhp,
                     dvoid *piecebuf, ub4 *nbytes);

int   sql_describe(connection *c, char *pname, int descmode, char *schema,
                   int nargs, char *names[], ub4 counts[]);

#endif

/*
** Apache-based functions from module linkage code
*/

int   mowa_check_keepalive(int context_flag);

void  mowa_acquire_mutex(owa_context *octx);

void  mowa_release_mutex(owa_context *octx);

void  mowa_semaphore_create(owa_context *octx);

int   mowa_semaphore_get(owa_context *octx);

void  mowa_semaphore_put(owa_context *octx);

/*
** Apache-based request-related functions
*/
#ifdef APACHE_LINKAGE

void  morq_create_mutex(request_rec *request, owa_context *octx);

char *morq_get_buffer(request_rec *request, int sz);

void *morq_alloc(request_rec *request, size_t sz, int zero_flag);

int   morq_regex(request_rec *request,
                 char *pattern, char **str, int caseflag);

void  morq_send_header(request_rec *request);

void  morq_set_status(request_rec *request, int status, char *status_line);

void  morq_set_mimetype(request_rec *request, char *mtype);

void  morq_set_length(request_rec *request, size_t len, int range_flag);

int   morq_check_range(request_rec *request);

int   morq_get_range(request_rec *request, long_64 *off, long_64 *len);

char *morq_getword(request_rec *request,
                   const char **args, char ch, int un_escape);

int   morq_stream(request_rec *request, int flush_flag);

long  morq_read(request_rec *request, char *buffer, long buflen);

long  morq_write(request_rec *request, char *buffer, long buflen);

void  morq_print_int(request_rec *request, char *fmt, long ival);

void  morq_print_str(request_rec *request, char *fmt, char *sptr);

int   morq_table_get(request_rec *request,
                     int tableid, int index, char **name, char **value);

void  morq_table_put(request_rec *request, int tableid, int set_flag,
                     char *name, char *value);

char *morq_get_header(request_rec *request, char *name);

char *morq_get_env(request_rec *request, char *name);

char *morq_parse_auth(request_rec *request, char *buffer);

void  morq_add_context(request_rec *request, owa_context *octx);

void  morq_close_exec(request_rec *request, owa_context *octx, int first_flag);

void  morq_sql_error(request_rec *request,
                     int code, char *errbuf, char *lastsql);

#endif /* APACHE_LINKAGE */

/*
** Core gateway functions
*/
#ifdef APACHE_LINKAGE

int   owa_handle_request(owa_context *octx, request_rec *r,
                         char *req_args, int req_method, owa_request *owa_req);

int   owa_dav_request(owa_context *octx, request_rec *r,
                      char *req_args, char *req_uri, int req_method);

int   owa_writedata(connection *c, owa_context *octx, request_rec *r,
                    char *boundary, long_64 clen, int long_flag,
                    char *outbuf, char *stmt, char *pmimetype, int nargs,
                    int nuargs, char **pnames, char **pvalues);

int   owa_readlob(connection *c, owa_context *octx, request_rec *r,
                  char *fpath, char *pmimetype, char *physical,
                  char *outbuf);

int   owa_readlong(connection *c, owa_context *octx, request_rec *r,
                   char *fpath, char *pmimetype, char *physical,
                   char *outbuf, int bin_flag);

int   owa_rlobtable(connection *c, owa_context *octx, request_rec *r,
                    char *name, char *pmimetype, char *pcdisp, char *outbuf);

int   owa_wlobtable(connection *c, owa_context *octx, request_rec *r,
                    char *outbuf, file_arg *filelist,
                    char **vals, char **valptrs, char *marker);

int   owa_getpage(connection *c, owa_context *octx, request_rec *r,
                  char *physical, char *outbuf, int *rstatus,
                  owa_request *owa_req, int rset_flag);

int   owa_getheader(connection *c, owa_context *octx, request_rec *r,
                    char *ctype, char *cdisp, char *outbuf, int *rstatus);

void  owa_file_show(owa_context *octx, request_rec *r);

char *owa_ldap_lookup(owa_context *octx, request_rec *r,
                      char *authuser, char *authpass, char *session);

void  owa_log_send(owa_context *octx, request_rec *r,
                   connection *c, owa_request *owa_req);

owa_log_socks *owa_log_alloc(owa_context *octx);

owa_log_socks *owa_log_free(owa_log_socks *s);

#ifdef WITH_OCI

int   owa_runplsql(connection *c, char *stmt, char *outbuf, int xargs,
                   int nargs, char *values[], ub4 counts[], sb4 widths[],
                   char **pointers, ub2 *plens, un_long arr_round, int csid,
                   int zero_flag);

int   owa_showerror(connection *c, char *stmt, int errcode);

#ifndef NO_FILE_CACHE

char *owa_map_cache(owa_context *octx, request_rec *r,
                    char *fpath, ub4 *life);

int   owa_download_file(owa_context *octx, request_rec *r,
                        char *fpath, char *pmimetype, ub4 life, char *outbuf);

int   owa_create_path(char *fpath, char *tempbuf);

#endif

#endif /* WITH_OCI */

#endif /* APACHE_LINKAGE */

void  owa_find_session(owa_context *octx, char *cookieval, char *session);

int   owa_reset(connection *c, owa_context *octx);

int   owa_passenv(connection *c, owa_context *octx,
                  env_record *penv, owa_request *owa_req);

void  owa_set_statements(owa_context *octx);

void  owa_out_args(char *argarr[], int mode_flag);

void  owa_cleanup(owa_context *octx);

void  owa_pool_purge(owa_context *octx, int interval);

#ifndef NO_FILE_CACHE

void  owa_file_purge(owa_context *octx, int interval);

#endif

/*
** Shared-memory interfaces
*/

un_long owa_shmem_init(shm_context *map);

void    owa_shmem_update(shm_context *map, int *mapoff, int realpid,
                         char *location, int *poolstats);

int     owa_shmem_stats(shm_context *map, char *location, int *poolstats);

#endif /* MODOWA_H */
