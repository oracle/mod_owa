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
** File: modtest.c
**
** Configuration (httpd.conf):
**
**  LoadModule tst_module modules/mod_test.so
**  AddModule modtest.c
**  <Location /modtest>
**     AllowOverride  None
**     Options        None
**     SetHandler     tst_handler
**     test_val       Foobar
**     order          deny,allow
**     allow          from all
**  </Location>
**
** Compilation:
**   #define WITH_OCI  causes inclusion of a reference to Oracle libclntsh.so
*/

/*
** Standard headers
*/
#ifdef WIN32
#ifdef NEVER
/* ### FORCED TO REMOVE BECAUSE OF INCOMPATIBILITY WITH APACHE HEADERS ### */
#include <windows.h>
#endif
#define str_length    lstrlen
#define str_copy      lstrcpy
#define str_compare   lstrcmp
#define get_pid       GetCurrentProcessId
#else
/*
** Standard Unix headers
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <malloc.h>
#include <ctype.h>
#include <unistd.h>
#define str_length    strlen
#define str_copy      strcpy
#define str_compare   strcmp
#define get_pid       getpid
#endif

#ifdef APACHE24
# ifndef APACHE22
#  define APACHE22
# endif
#endif
#ifdef APACHE22
# ifndef APACHE20
#  define APACHE20
# endif
#endif

/*
** Apache headers
*/
#include <httpd.h>
#include <http_config.h>
#include <http_core.h>
#include <http_log.h>
#include <http_main.h>
#include <http_protocol.h>
#include <util_script.h>
#include <http_request.h>
#ifdef APACHE20
#include <apr.h>
#include <apr_strings.h>
#ifdef APACHE22
#include <ap_regex.h>
#endif
#define LOGSTATUS(s)        (apr_status_t)0,(s)
#else
#include <multithread.h>
#define apr_pool_t         pool
#define apr_table_t        table
#define apr_size_t         int
#define apr_array_header_t array_header
#define apr_table_entry_t  table_entry
#define apr_pcalloc        ap_pcalloc
#define apr_palloc         ap_palloc
#define apr_pstrdup        ap_pstrdup
#define apr_table_unset    ap_table_unset
#define apr_table_set      ap_table_set
#define apr_table_add      ap_table_add
#define apr_table_get      ap_table_get
#define apr_table_elts     ap_table_elts
#define apr_table_clear    ap_clear_table
#define LOGSTATUS(s)        (s)
#endif

#ifdef WITH_OCI
#include <oratypes.h>
#include <oci.h>
#endif

typedef struct test_config
{
    char  *test_val;
    char  *location;
} test_config;

/*****************************************************************************
 * Basic linkage to Apache                                                   *
 *****************************************************************************/

#ifdef APACHE20
AP_MODULE_DECLARE_DATA module tst_module;
#else
MODULE_VAR_EXPORT module tst_module;
#endif

static void *test_create_dir_config(apr_pool_t *p, char *dirspec)
{
    size_t       asize;
    test_config *ctx;

    /* Allocate space for structure, array of connections, and location */
    asize = (dirspec) ? str_length(dirspec) : 0;
    ctx = (test_config *)apr_pcalloc(p, sizeof(*ctx) + asize + 1);

    if (ctx)
    {
        ctx->location = (char *)(void *)(ctx + 1);
        if (dirspec) str_copy(ctx->location, dirspec);
    }

    return((void *)ctx);
}

static void tst_read_post(request_rec *r, char *buf)
{
    const apr_array_header_t *arr;
    apr_table_entry_t  *elements;
    int                 n;
    int                 i;
    int                 clen;
    int                 total;
    int                 count;
    int                 partial;
    char                buffer[4096];
    char               *name;
    char               *value;
    char               *sptr;

    arr = apr_table_elts(r->subprocess_env);
    elements = (apr_table_entry_t *)(arr->elts);
    n = arr->nelts;

    clen = 0;
    for (i = 0; i < n; ++i)
    {
        name  = elements[i].key;
        value = elements[i].val;
        if (!name) continue;
        if (!strcmp(name, "CONTENT_LENGTH"))
        {
            if (value)
              for (sptr = value; *sptr != '\0'; ++sptr)
                clen = (clen * 10) + (int)(*sptr - '0');
            break;
        }
    }

    total = 0;
    count = 0;
    partial = 0;
    while (clen > 0)
    {
        i = sizeof(buffer);
        if (i > clen) i = clen;
        n = (long)ap_get_client_block(r, buffer, (apr_size_t)i);
#ifdef NEVER
        ap_log_error(APLOG_MARK, APLOG_DEBUG, LOGSTATUS(r->server),
                     "TEST Read %d of %d", n, clen);
#endif
        if (n < 0) break;
        if (n > 0) clen -= n;
        if (n < i) ++partial;

        total += n;
        ++count;
    }

    if ((total > 0) || (count > 0))
        sprintf(buf + strlen(buf),
                "<p>Uploaded %d bytes in %d reads, %d partial fills</p>\n",
                total, count, partial);
}

static int tst_handler(request_rec *r)
{
    int          result;
    int          pid;
    char         buf[2000];
    test_config *ctx;

#ifdef APACHE20
    if (str_compare(r->handler, "tst_handler")) return(DECLINED);
#endif

    result = OK;

    ctx = (test_config *)ap_get_module_config(r->per_dir_config,
                                              &tst_module);

    ap_add_common_vars(r);
    ap_add_cgi_vars(r);

#ifndef APACHE20
    ap_soft_timeout("send a2o call trace", r);
#endif

    if (r->header_only)
    {
        /* Nothing to do, so just send header */
        r->content_type = "text/plain";
#ifndef APACHE20 /* ### NO-OP in 2.0? ### */
        ap_send_http_header(r);
#endif
    }
    else
    {
        pid = get_pid();
        ap_log_error(APLOG_MARK, APLOG_INFO, LOGSTATUS(r->server),
                     "TEST Request [%s]", r->uri);
        ap_log_error(APLOG_MARK, APLOG_DEBUG, LOGSTATUS(r->server),
                     "TEST PID is %d", pid);

        sprintf(buf, "<html><body bgcolor=\"#ffffff\" text=\"#cc0000\">\n");
        sprintf(buf + strlen(buf), "<h4>Test module</h4>\n");
        sprintf(buf + strlen(buf), "<p>Request for [%s] ", r->uri);
        sprintf(buf + strlen(buf), "handled by PID %d</p>\n", pid);
        sprintf(buf + strlen(buf), "<p>At location [%s] ", ctx->location);
        sprintf(buf + strlen(buf), " directive is %s</p>\n",
                ctx->test_val ? ctx->test_val : "&lt;null&gt;");

        if (r->method_number == M_POST) tst_read_post(r, buf);

        sprintf(buf + strlen(buf), "</body></html>\n");

        r->content_type = "text/html";
        r->status = HTTP_OK;
        ap_set_content_length(r, strlen(buf));
#ifndef APACHE20 /* ### NO-OP in 2.0? ### */
        ap_send_http_header(r);
#endif
        ap_rwrite(buf, strlen(buf), r);

        ap_log_error(APLOG_MARK, APLOG_INFO, LOGSTATUS(r->server),
                     "TEST Completed with status %d", result);
    }

#ifndef APACHE20
    ap_kill_timeout(r);
#endif
    return(result);
}

#ifdef APACHE20
static int test_module(apr_pool_t *p, apr_pool_t *ptemp, 
                        apr_pool_t *plog, server_rec *s)
#else
static void test_module(server_rec *s, apr_pool_t *p)
#endif
{
    /*
    ** Initialize module (DLL load-time) - nothing to do
    */
#ifdef APACHE20
    return(OK);
#endif
}

#ifdef APACHE20
static void *test_server_config(apr_pool_t *p, server_rec *s)
#else
static void *test_server_config(pool *p, server_rec *s)
#endif
{
    return((void *)apr_pcalloc(p, 32)); /* ### BLANK FOR TESTING */
}

#ifdef APACHE20
static apr_status_t test_exit(void *data)
#else
static void test_exit(server_rec *s, apr_pool_t *p)
#endif
{
#ifdef APACHE20
#ifdef NEVER
    server_rec *s = (server_rec *)data;
#endif
    return((apr_status_t)0);
#endif
}

#ifdef APACHE20
/*
** No-op function to satisfy requirement for child cleanup routine
*/
static apr_status_t test_nop(void *data)
{
    return((apr_status_t)0);
}
#endif

#ifdef APACHE20
static void test_init(apr_pool_t *p, server_rec *s)
#else
static int test_init(server_rec *s, apr_pool_t *p)
#endif
{
#ifdef WITH_OCI
    OCIInitialize((ub4)OCI_THREADED, (dvoid *)0, (dvoid * (*)())0,
                  (dvoid * (*)())0, (dvoid (*)())0);
#endif
#ifdef APACHE20
    apr_pool_cleanup_register(p, (void *)s, test_exit, test_nop);
#else
    return(OK);
#endif
}

#ifndef XtOffSetOf
#define XtOffsetOf(typ, fld) (((char *)(&(((typ *)0)->fld))) - ((char *)0))
#endif

#ifdef APACHE22
#define ARG_DFN       {(const char *(*)())ap_set_string_slot}
#define ARG_SET(loc)  ARG_DFN, (void *)XtOffsetOf(test_config, loc)
#define ARG_FN(f)     {(const char *(*)())f}, (void *)0
#else
#define ARG_DFN       (const char *(*)())ap_set_string_slot
#define ARG_SET(loc)  ARG_DFN, (void *)XtOffsetOf(test_config, loc)
#define ARG_FN(f)     (const char *(*)())f, (void *)0
#endif

#define ARG_PATTERN(nm, st, cf, tk, hlp) {nm, st, cf, tk, hlp}
static const command_rec tst_commands[] =
{
  ARG_PATTERN("test_val", ARG_SET(test_val), ACCESS_CONF,  TAKE1,
              "test_val [value]"),
 {0}
};

#ifndef APACHE20

static const handler_rec tst_handlers[] =
{
    {"tst_handler", tst_handler},
    {NULL}
};

MODULE_VAR_EXPORT module tst_module =
{
    MODULE_MAGIC_NUMBER_MAJOR,
    MODULE_MAGIC_NUMBER_MINOR,
    -1,
    "modtest.c",
    NULL,
    NULL,
#ifdef EAPI
    MODULE_MAGIC_COOKIE_AP13,
#else
    MODULE_MAGIC_COOKIE,
#endif
    test_module,                    /* module initializer */
    test_create_dir_config,         /* dir config creator */
    NULL,                           /* dir merger --- default is to override */
    test_server_config,             /* server config */
    NULL,                           /* merge server config */
    tst_commands,                   /* command table */
    tst_handlers,                   /* handlers */
    NULL,                           /* filename translation */
    NULL,                           /* check_user_id */
    NULL,                           /* check auth */
    NULL,                           /* check access */
    NULL,                           /* type_checker */
    NULL,                           /* fixups */
    NULL,                           /* logger */
    NULL,                           /* header parser */
    test_init,                      /* child initializer */
    test_exit,                      /* child cleanup */
    NULL                            /* post read_request handling */
#ifdef EAPI
    ,NULL                           /* add module */
    ,NULL                           /* remove module */
    ,NULL                           /* rewrite command */
    ,NULL                           /* new connection */
    ,NULL                           /* close connection */
#endif
};

#else

static void tst_hooks(apr_pool_t *p)
{
    ap_hook_handler(tst_handler, NULL, NULL, APR_HOOK_MIDDLE);

    ap_hook_post_config(test_module,
                        NULL, /* Predecessors */
                        NULL, /* Successors */
                        APR_HOOK_MIDDLE); /* FIRST/MIDDLE/LAST */
    ap_hook_child_init(test_init,
                       NULL, /* Predecessors */
                       NULL, /* Successors */
                       APR_HOOK_MIDDLE);
};

AP_MODULE_DECLARE_DATA module tst_module =
{
    STANDARD20_MODULE_STUFF,
    test_create_dir_config,         /* dir config creator */
    NULL,                           /* dir merger --- default is to override */
    test_server_config,             /* server config */
    NULL,                           /* merge server config */
    tst_commands,                   /* command table */
    tst_hooks
};

#endif /* APACHE20 */
