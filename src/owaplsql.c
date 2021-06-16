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
** File:         owaplsql.c
** Description:  Basic gateway routines.
** Author:       Doug McMahon      Doug.McMahon@oracle.com
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
**
** History
**
** 08/17/2000   D. McMahon      Created
** 05/31/2001   D. McMahon      Began revisioning with 1.3.9
** 06/16/2001   D. McMahon      Added support for RAW input/output bindings
** 09/01/2001   D. McMahon      Added array and scalar rounding
** 11/25/2001   D. McMahon      Support returning Content-Length:0
** 02/11/2002   D. McMahon      Add OwaDocFile support (file name return)
** 06/06/2003   D. McMahon      Add owa_showerror
** 12/17/2003   D. McMahon      Fixed redirect handling (for Apache 2.0)
** 01/19/2004   D. McMahon      Added owa_find_session and related code
** 09/14/2004   D. McMahon      Use HTBUF_PLSQL_MAX for string buffers
** 02/02/2007   D. McMahon      Added owa_ldap_lookup()
** 04/14/2007   D. McMahon      Add limited support for WPG_DOCLOAD
** 04/15/2007   D. McMahon      Add owa_getheader()
** 04/30/2010   D. McMahon      (long) casts for morq_read and morq_write
** 08/25/2010   D. McMahon      Allow * for OwaLDAP directive (NT auth)
** 03/28/2012   D. McMahon      Add OwaContentType support
** 11/27/2012   D. McMahon      GCC fixes, Win-64 porting
** 06/06/2013   D. McMahon      Add support for REF cursor return
** 06/21/2013   D. McMahon      Fix CSV escaping mode
** 07/19/2013   D. McMahon      Allow Content-Disposition from owa_getheader
** 07/25/2013   D. McMahon      Add cdisp to owa_getheader
** 09/13/2013   D. McMahon      Fix 0-length array problem
** 09/17/2013   D. McMahon      Add Last-Modified, Expires, ETag to owa_getheader
** 09/18/2013   D. McMahon      Return OK from blank return in REST/DAV modes
** 09/19/2013   D. McMahon      64-bit content lengths
** 09/24/2013   D. McMahon      Add errinfo argument to sql_connect
** 09/26/2013   D. McMahon      Pass client IP address if available
** 10/21/2013   D. McMahon      Capture status line text
** 04/10/2014   D. McMahon      Allow PL/SQL to send permanent redirection
** 04/27/2014   D. McMahon      Zero init temporary connection hdb
** 09/11/2015   D. McMahon      Disallow control characters in headers
** 10/13/2015   D. McMahon      Limit CGIPOST buffer to 32k (per Fulvio Bille)
*/

#define WITH_OCI
#define APACHE_LINKAGE
#include <modowa.h>

/*
** Dummy names for output bind variables
**
** ### NEED TO FIND A WAY TO ALSO USE THIS MECHANISM FOR IN/OUT AND
** ### SOME MISC. OUT BINDS, SO THAT DIAGNOSTICS WILL BE EASIER TO READ.
*/
static char var_blob[]  = "OUT: <BLOB>";
static char var_clob[]  = "OUT: <CLOB>";
static char var_nlob[]  = "OUT: <NCLOB>";
static char var_bfile[] = "OUT: <BFILE>";
static char var_stmt[]  = "OUT: <SQL STMT>";
static char var_bind[]  = "OUT: <SQL BIND>";
static char var_clen[]  = "OUT: <CONTENT LENGTH>";
static char var_curi[]  = "OUT: <CACHE URI>";
static char var_csum[]  = "OUT: <CHECKSUM STRING>";
static char var_fname[] = "OUT: <FILE NAME>";
static char var_rset[]  = "OUT: <REF CURSOR>";
static char var_xtags[] = "OUT: <XML TAGS>";

/*
** SQL Statements
*/
static char sql_stmt1a[] = "begin "
                           "OWA.INIT_CGI_ENV(:ecount, :namarr, :valarr); "
                           "end;";
static char sql_stmt1b[] = "begin "
                           "OWA.INIT_CGI_ENV(:ecount, :namarr, :valarr); "
                           "OWA.INIT_CGI_EXTRA(:rtime, :postargs); "
                           "end;";
static char sql_stmt1c[] = "begin\n"
                           "  OWA.INIT_CGI_ENV(:ecount, :namarr, :valarr);\n"
                           "  if (:ipflag > 0) then\n"
                           "    OWA.IP_ADDRESS := :ipaddr;\n"
                           "  end if;\n"
                           "end;";
static char sql_stmt2a[] = "begin\n"
                           "  OWA.USER_ID := :usr;\n"
                           "  OWA.PASSWORD := :pwd;\n"
                           "end;";
static char sql_stmt2b[] = "begin\n"
                           "  OWA.SET_USER_ID(:usr);\n"
                           "  OWA.SET_PASSWORD(:pwd);\n"
                           "end;";
static char sql_stmt2c[] = "begin\n"
                           "  OWA.SET_USER_ID(:usr);\n"
                           "  OWA.SET_PASSWORD(:pwd);\n"
                           "  OWA.ENABLE_RAW_MODE;\n"
                           "end;";
static char sql_stmt4a[] = "begin OWA.GET_PAGE(:linearr, :nlines); end;";
static char sql_stmt4b[] = "begin OWA.GET_PAGE_RAW(:linearr, :nlines); end;";
static char sql_stmt5a[] = "begin DBMS_SESSION.RESET_PACKAGE; end;";
static char sql_stmt5b[] = "begin DBMS_SESSION.MODIFY_PACKAGE_STATE(1); end;";
static char sql_stmt5c[] = "begin DBMS_SESSION.MODIFY_PACKAGE_STATE(2); end;";
static char sql_stmt5d[] = "begin HTP.INIT; end;";

/*
** Content type serialization modes for owa_getrows:
*/
#define OWAROWS_MODE_CSV     1
#define OWAROWS_MODE_XML     2
#define OWAROWS_MODE_JSON    3
#define OWAROWS_MODE_PLAIN   4

/*
** Max buffer size for REF cursor fetching
** Maximum number of rows to array-fetch
*/
#define OWAROWS_BUFSIZE      1000000
#define OWAROWS_MAXFETCH     1000

/*
** Reset PL/SQL package state
*/
int owa_reset(connection *c, owa_context *octx)
{
    sword    status;
    sb4      oerrno;

    if (octx->altflags & ALT_MODE_KEEP) return(OCI_SUCCESS);
    if (octx->authrealm) return(OCI_SUCCESS);

    status = sql_exec(c, c->stmhp5, (ub4)1, 0);

    if (status != OCI_SUCCESS)
    {
        if (c->errbuf) c->lastsql = octx->reset_stmt;
        oerrno = sql_get_error(c); 
        if (oerrno) status = oerrno;
    }
    else if (c->errbuf)
    {
        c->errbuf[0] = '\0';
        c->lastsql = (char *)0;
    }

    return(status);
}

/*
** Pass environment information to OWA
*/
int owa_passenv(connection *c, owa_context *octx,
                env_record *penv, owa_request *owa_req)
{
    sword    status;
    sb4      oerrno;
    sb4      ecount;
    ub4      asize;
    ub4      amax;
    sb4      wait_time;
    sb4      pieces[4]; /* ### Can't handle IPV6 ### */
    ub4      npieces;
    sb4      ipflag;
    char    *post_args;
    char    *usr;
    char    *pwd;

    status = OCI_SUCCESS;
    c->errbuf[0] = '\0';

#ifndef RESET_AFTER_EXEC
    /*
    ** ### CONSIDER DOING THIS AFTER THE CURRENT OPERATION, IT MIGHT
    ** ### IMPROVE PERCEIVED USER RESPONSE TIME.
    */
    if (c->c_lock == C_LOCK_INUSE)
    {
        /*
        ** No need to execute this on the very first call for this connection
        ** Saves overhead in the case where the connection is not being reused
        */
        status = owa_reset(c, octx);
        if (status != OCI_SUCCESS) return(status);
    }
#endif

    ecount = (sb4)penv->count;
    asize = (ub4)penv->count;
    amax = (ub4)asize;
    if (amax < (ub4)HTBUF_ENV_ARR) amax = (ub4)HTBUF_ENV_ARR;

    c->lastsql = octx->cgi_stmt;

    status = sql_bind_int(c, c->stmhp1, (ub4)1, &ecount);
    if (status != OCI_SUCCESS) goto passerr;
    status = sql_bind_arr(c, c->stmhp1, (ub4)2, penv->names,
                          (ub2 *)0, (sb4)(penv->nwidth), (sb2 *)0,
                          &asize, amax);
    if (status != OCI_SUCCESS) goto passerr;
    status = sql_bind_arr(c, c->stmhp1, (ub4)3, penv->values,
                          (ub2 *)0, (sb4)(penv->vwidth), (sb2 *)0,
                          &asize, amax);
    if (status != OCI_SUCCESS) goto passerr;

    if (octx->altflags & (ALT_MODE_CGITIME | ALT_MODE_CGIPOST))
    {
        sb4 slen;

        wait_time = (sb4)(owa_req->connect_time - owa_req->lock_time);
        post_args = (char *)0;
        if (octx->altflags & ALT_MODE_CGIPOST) post_args = owa_req->post_args;
        if (!post_args) post_args = (char *)"";

        status = sql_bind_int(c, c->stmhp1, (ub4)4, &wait_time);
        if (status != OCI_SUCCESS) goto passerr;
        slen = str_length(post_args);
        if (slen > HTBUF_PLSQL_MAX) slen = HTBUF_PLSQL_MAX;
        status = sql_bind_chr(c, c->stmhp1, (ub4)5, post_args, slen);
        if (status != OCI_SUCCESS) goto passerr;
    }
    else if (octx->altflags & ALT_MODE_IPADDR)
    {
        pieces[0] = (sb4)((penv->ipaddr >> 24) & 0xFF);
        pieces[1] = (sb4)((penv->ipaddr >> 16) & 0xFF);
        pieces[2] = (sb4)((penv->ipaddr >>  8) & 0xFF);
        pieces[3] = (sb4)((penv->ipaddr      ) & 0xFF);

        npieces = (ub4)4;
        ipflag = (penv->ipaddr) ? 4 : 0;

        status = sql_bind_int(c, c->stmhp1, (ub4)4, &ipflag);
        status = sql_bind_iarr(c, c->stmhp1, (ub4)5, pieces, &npieces, npieces);
    }

    status = sql_exec(c, c->stmhp1, (ub4)1, 0);
    if (status != OCI_SUCCESS) goto passerr;

    /*
    ** If Basic authentication information is present,
    ** pass it to OWA so that OWA_SEC will work.
    ** ### NOTE THAT hostname AND ip_address ARE NOT SET.
    ** ### THE hostname SHOULD BE AVAILABLE IN A CGI VARIABLE.
    ** ### SO IS THE ip_address, THOUGH IN THAT CASE THE OWA
    ** ### REQUIRES THAT IT BE BOUND AS A TABLE OF INTEGER VALUES,
    ** ### NOT A STRING.
    */
    usr = penv->authuser;
    pwd = penv->authpass;
    if (!usr) usr = (char *)"";
    if (!pwd) pwd = (char *)"";

    if ((*usr) || (*pwd) ||
        (octx->altflags & ALT_MODE_GETRAW) || (octx->altflags & ALT_MODE_KEEP))
    {
        c->lastsql = octx->sec_stmt;

        status = sql_bind_str(c, c->stmhp2, (ub4)1, usr, (sb4)-1);
        if (status != OCI_SUCCESS) goto passerr;
        status = sql_bind_str(c, c->stmhp2, (ub4)2, pwd, (sb4)-1);
        if (status != OCI_SUCCESS) goto passerr;

        if (!(*usr) && !(*pwd)) c->out_ind = -1; /* Null bindings */

        status = sql_exec(c, c->stmhp2, (ub4)1, 0);
    }

passerr:
    if (status != OCI_SUCCESS)
    {
        oerrno = sql_get_error(c); 
        if (oerrno) status = oerrno;
    }
    else
        c->lastsql = (char *)0;
    return(status);
}

/*
** Run PL/SQL procedure with arguments
*/
int owa_runplsql(connection *c, char *stmt, char *outbuf, int xargs,
                 int nargs, char *values[], ub4 counts[], sb4 widths[],
                 char **pointers, ub2 *plens, un_long arr_round, int csid,
                 int zero_flag)
{
    sword  status;
    sb4    oerrno;
    int    i, j;
    char  *optr;
    ub4    amax;
    int    ocsid = c->csid;
    int    hacked_flag = 0;

    status = OCI_SUCCESS;

    c->lastsql = stmt;
    c->errbuf[0] = '\0';

    status = sql_parse(c, c->stmhp3, stmt, -1);
    if (status != OCI_SUCCESS) goto runerr;

    /* Set indicators for unbound arguments to null */
    c->bfile_ind = (ub2)-1;
    c->nlob_ind  = (ub2)-1;
    c->clob_ind  = (ub2)-1;
    c->blob_ind  = (ub2)-1;

    optr = outbuf;

    if (csid) c->csid = csid;

    j = 0;
    for (i = 0; i < nargs; ++i)
    {
        ub4 old_count = counts[i];

        if (i == (nargs - xargs))
            c->ncflag &= ~(UNI_MODE_USER | UNI_MODE_RAW);

        if (old_count == LONG_MAXSZ) continue; /* Skip binding */

        if ((zero_flag & (1 << i)) && (old_count == 1))
        {
            /*
            ** ### Logical problem here - in owahand.c, some effort is made
            ** ### to force the count to be 1 if there are really 0 arguments.
            ** ### This then leads to misleading counts at the PL/SQL level
            ** ### for the 0-argument case. Why was that done? Can we use
            ** ### another signalling mechanism, or alternatively set the
            ** ### count to 0 here if the array doesn't actually carry a real
            ** ### parameter? For now, use a bit flag to "fix up" the count.
            */
            counts[i] = 0;
            hacked_flag |= (1 << i); /* Remember that the count was altered */
        }

        if (values[i] == (char *)0)
        {
            amax = (ub4)util_round((un_long)old_count, arr_round);
            status = sql_bind_ptrs(c, c->stmhp3, (ub4)(j + 1), pointers,
                                   widths[i], counts + i, amax);
            if (c->ncflag & UNI_MODE_RAW) plens += old_count;
        }
        else if (old_count > 0)
        {
          amax = (ub4)util_round((un_long)old_count, arr_round);

          if (c->ncflag & UNI_MODE_RAW)
          {
            status = sql_bind_rarr(c, c->stmhp3, (ub4)(j + 1), values[i],
                                   plens, widths[i], (sb2 *)0, counts + i,
                                   amax);
            plens += old_count;
          }
          else
            status = sql_bind_arr(c, c->stmhp3, (ub4)(j + 1), values[i],
                                  (ub2 *)0, widths[i], (sb2 *)0, counts + i,
                                  amax);
        }
        else if (values[i] == var_blob)
        {
            status = sql_bind_lob(c, c->stmhp3, (ub4)(j + 1), SQLT_BLOB);
            if (c->ncflag & UNI_MODE_RAW) ++plens;
        }
        else if (values[i] == var_clob)
        {
            c->csid = ocsid;
            status = sql_bind_lob(c, c->stmhp3, (ub4)(j + 1), SQLT_CLOB);
            if (c->ncflag & UNI_MODE_RAW) ++plens;
        }
        else if (values[i] == var_bfile)
        {
            status = sql_bind_lob(c, c->stmhp3, (ub4)(j + 1), SQLT_BFILE);
            if (c->ncflag & UNI_MODE_RAW) ++plens;
        } 
        else if (values[i] == var_nlob)
        {
            status = sql_bind_lob(c, c->stmhp3, (ub4)(j + 1), 0);
            if (c->ncflag & UNI_MODE_RAW) ++plens;
        }
        else if (values[i] == var_rset)
        {
            status = sql_bind_cursor(c, c->stmhp3, (ub4)(j + 1));
        }
        else if ((values[i] == var_stmt) || (values[i] == var_bind) ||
                 (values[i] == var_clen) || (values[i] == var_fname) ||
                 (values[i] == var_curi) || (values[i] == var_csum) ||
                 (values[i] == var_xtags))
        {
            c->csid = ocsid;
            *optr = '\0';
            status = sql_bind_str(c, c->stmhp3, (ub4)(j + 1), optr, widths[i]);
            optr += widths[i];
            if (c->ncflag & UNI_MODE_RAW) ++plens;
        }
        else
        {
          if (c->ncflag & UNI_MODE_RAW)
          {
            status = sql_bind_raw(c, c->stmhp3, (ub4)(j + 1),
                                  values[i], plens, widths[i]);
            ++plens;
          }
          else
          {
            status = sql_bind_str(c, c->stmhp3, (ub4)(j + 1),
                                  values[i], widths[i]);
          }
        }
        if (status != OCI_SUCCESS) goto runerr;
        ++j;
    }

    status = sql_exec(c, c->stmhp3, (ub4)1, 0);

    /* If counts were altered due to the zero_flag, restore them */
    if (hacked_flag)
      for (i = 0; i < nargs; ++i)
        if (hacked_flag & (1 << i))
          counts[i] = 1;

runerr:
    if (status != OCI_SUCCESS)
    {
        oerrno = sql_get_error(c); 
        if (oerrno) status = oerrno;
    }
    else
        c->lastsql = (char *)0;
    c->csid = ocsid;
    return(status);
}

/*
** Show special error page for SQL errors
*/
int owa_showerror(connection *c, char *stmt, int errcode)
{
    sword status;
    sb4   oerrno;
    sb4   val = errcode;
    status = OCI_SUCCESS;

    c->lastsql = stmt;
    c->errbuf[0] = '\0';

    status = sql_parse(c, c->stmhp3, stmt, -1);
    if (status != OCI_SUCCESS) goto errerr;

    status = sql_bind_int(c, c->stmhp3, (ub4)1, &val);
    if (status != OCI_SUCCESS) goto errerr;
    status = sql_bind_str(c, c->stmhp3, (ub4)2, (char *)"", (sb4)0);
    if (status != OCI_SUCCESS) goto errerr;

    status = sql_exec(c, c->stmhp3, (ub4)1, 0);

errerr:
    if (status != OCI_SUCCESS)
    {
        oerrno = sql_get_error(c); 
        if (oerrno) status = oerrno;
    }
    else
        c->lastsql = (char *)0;
    return(status);
}

/*
** If necessary, convert an LDAP user/pass to DB user/pass
*/
char *owa_ldap_lookup(owa_context *octx, request_rec *r,
                      char *authuser, char *authpass, char *session)
{
    sword       status;
    char        usr[HTBUF_LINE_LENGTH];
    char        pwd[HTBUF_LINE_LENGTH];
    char        errbuf[OCI_ERROR_MAXMSG_SIZE + 1];
    char       *sptr;
    char       *stmt;
    int         llen;
    int         slen;
    int         ulen = 0;
    int         plen = 0;
    int         xlen = 0;
    ldapstruct *lptr;
    connection  hdb;
    connection *c = &hdb;

    mem_zero(&hdb, sizeof(hdb));
    c->errbuf = errbuf;
    c->csid = 0;
    c->c_lock = C_LOCK_NEW;
    c->session = (char *)0;

    if (!octx->ora_ldap) return((char *)0);
    if (*(octx->ora_ldap) == '*') return((char *)0);

    /* See if the user or session is already in the LDAP cache */
    for (lptr = octx->ldap_cache; lptr; lptr = lptr->next)
    {
        /* Prefer a session match */
        if ((session) && (lptr->sess))
          if (!str_compare(session, lptr->sess, -1, 0))
            return(lptr->dbconn);
        /* Otherwise try a username match */
        if ((authuser) && (authpass) && (lptr->luser) && (lptr->lpass))
          if (!str_compare(authuser, lptr->luser, -1, 0) &&
              !str_compare(authpass, lptr->lpass, -1, 0))
            return(lptr->dbconn);
    }

    /* Build the PL/SQL call */
    slen = str_length(octx->ora_ldap);
    stmt = (char *)morq_alloc(r, (size_t)(slen + 100), 0);
    if (!stmt) return((char *)0);
    mem_copy(stmt, "begin ", 6);
    mem_copy(stmt + 6, octx->ora_ldap, slen);
    str_copy(stmt + slen + 6, "(:username, :password, :session); end;");

    /* Set up the arguments */
    *usr = *pwd = '\0';
    if (authuser) str_concat(usr, 0, authuser, sizeof(usr)-1);
    if (authpass) str_concat(pwd, 0, authpass, sizeof(pwd)-1);
    if (!session) session = (char *)"";

    sptr = (char *)0;

    /* Connect, set up, and run the statement */
    status = sql_connect(c, octx, (char *)0, (char *)0, (sb4 *)0);
    if (status != OCI_SUCCESS) goto ldaperr;
    status = sql_parse(c, c->stmhp3, stmt, -1);
    if (status != OCI_SUCCESS) goto ldaperr;
    status = sql_bind_str(c, c->stmhp3, (ub4)1, usr, (sb4)sizeof(usr));
    if (status != OCI_SUCCESS) goto ldaperr;
    status = sql_bind_str(c, c->stmhp3, (ub4)2, pwd, (sb4)sizeof(pwd));
    if (status != OCI_SUCCESS) goto ldaperr;
    status = sql_bind_str(c, c->stmhp3, (ub4)3, session, (sb4)-1);
    if (status != OCI_SUCCESS) goto ldaperr;
    status = sql_exec(c, c->stmhp3, (ub4)1, 0);
    if (status != OCI_SUCCESS) goto ldaperr;

    /* Process the results */
    if ((*usr) && (*pwd))
    {
        ulen = str_length(usr) + 1;
        plen = str_length(pwd) + 1;
        slen = ulen + plen;
        sptr = (char *)morq_alloc(r, (size_t)slen, 0);
        if (sptr)
        {
            mem_copy(sptr, usr, ulen);
            mem_copy(sptr + ulen, pwd, plen);

            /* Build cache entry */
            llen = slen;
            if (*session)
            {
              xlen = str_length(session) + 1;
              llen += xlen;
            }
            if ((authuser) && (authpass))
            {
                ulen = str_length(authuser) + 1;
                plen = str_length(authpass) + 1;
                llen += (ulen + plen);
            }
            lptr = (ldapstruct *)mem_zalloc(sizeof(*lptr) + llen);
            if (lptr)
            {
                lptr->dbconn = (char *)(void *)(lptr + 1);
                mem_copy(lptr->dbconn, sptr, slen);
                if (*session)
                {
                    lptr->sess = lptr->dbconn + slen;
                    mem_copy(lptr->sess, session, xlen);
                    slen += xlen;
                }
                if ((authuser) && (authpass))
                {
                    lptr->luser = lptr->dbconn + slen;
                    mem_copy(lptr->luser, authuser, ulen);
                    lptr->lpass = lptr->luser + ulen;
                    mem_copy(lptr->lpass, authpass, plen);
                }

                /* Add to LDAP results cache */
                mowa_acquire_mutex(octx);
                lptr->next = octx->ldap_cache;
                octx->ldap_cache = lptr;
                mowa_release_mutex(octx);
            }
        }
    }

ldaperr:
    status = sql_disconnect(c);
    return(sptr);
}

/*
** Scan a cookie string for a match with the configured session cookie name.
** If found, parse off the value for that cookie and return its value
** in the designated buffer (up to 4000 bytes).
*/
void owa_find_session(owa_context *octx, char *cookieval, char *session)
{
    char *sptr;
    char *cptr;
    char *nptr;
    int   clen;

    *session = '\0';
    if (!(octx->session)) return;
    clen = str_length(octx->session);
    if (clen == 0) return;

    cptr = cookieval;

    while (1)
    {
        while (*cptr == ' ') ++cptr;
        sptr = cptr;
        while (*sptr != '=')
        {
            if (*sptr == '\0') return; /* ### Premature end of cookie string */
            ++sptr;
        }
        nptr = sptr;
        while (*nptr != ';')
        {
            if (*nptr == '\0') break;
            ++nptr;
        }

        if ((sptr - cptr) == clen)
            if (!str_compare(octx->session, cptr, clen, 1))
            {
                /*
                ** Cookie name matches session identifier;
                ** copy cookie contents into session string buffer
                */
                ++sptr;
                clen = (int)(nptr - sptr);
                if (clen >= HTBUF_HEADER_MAX) clen = HTBUF_HEADER_MAX - 1;
                mem_copy(session, sptr, clen);
                session[clen] = '\0';
                break;
            }

        if (*nptr == '\0') break;

        cptr = nptr + 1;
    }
}

/*
** Form an XML tag from the tag name and optional prefix and nsuri
*/
static int create_tag(char *outbuf, char *tag, char *prefix, char *nsuri)
{
    int   plen = 0;
    int   tlen;
    char *optr = outbuf;

    /* Prepend the prefix */
    if (prefix)
    {
        plen = str_length(prefix);
        mem_copy(optr, prefix, plen);
        optr += plen;
        *(optr++) = ':';
    }

    tlen = util_ncname_convert(optr, tag);
    optr += tlen;

    /* Append optional nsuri string */
    if (nsuri)
    {
        mem_copy(optr, " xmlns", 6);
        optr += 6;
        if (plen > 0)
        {
            *(optr++) = ':';
            mem_copy(optr, prefix, plen);
            optr += plen;
        }
        *(optr++) = '=';
        *(optr++) = '"';
        tlen = (int)str_length(nsuri);
        mem_copy(optr, nsuri, tlen);
        optr += tlen;
        *(optr++) = '"';
    }
    *optr = '\0';

    return((int)(optr - outbuf));
}

/*
** Render an opening, closing, or empty XML tag
*/
static void write_tag(request_rec *r, int close_flag, int nl_flag, int bare_ns,
                      char *tag, int tag_len)
{
    morq_write(r, "</", (long)((close_flag > 0) ? 2 : 1));
    if ((close_flag > 0) && bare_ns)
    {
        char *sptr = str_char(tag, ' ', 0);
        if (sptr) tag_len = (int)(sptr - tag);
    }
    morq_write(r, tag, (long)tag_len);
    if (close_flag < 0)
      morq_write(r, "/>\n", (long)((nl_flag) ? 3 : 2));
    else
      morq_write(r, ">\n", (long)((nl_flag) ? 2 : 1));
}

/*
** Render XML element or attribute content, escaping entities as necessary
*/
static void owa_render_xml(owa_context *octx, request_rec *r,
                           char *buf, int blen, int attr_flag, int cs_flag)
{
  char *ptr = buf;
  char *eptr = buf + blen;
  char *entity;
  long  slen;

  while (ptr < eptr)
  {
    int   ch = *ptr & 0xFF;

    /* Check for known entities that require escapes */
    switch (ch)
    {
    case '<':  entity = "&lt;";    break;
    case '>':  entity = "&gt;";    break;
    case '&':  entity = "&amp;";   break;
    case '"':
      if (attr_flag)
      {
        entity = "&quot;";
        break;
      }
    case '\'':
      if (attr_flag)
      {
        entity = "&apos;";
        break;
      }
    default:   entity = (char *)0; break;
    }
    /* ### Note: the above may fail for non-byte-unique character sets ### */

    /* If a substitution is needed */
    if (entity)
    {
      slen = (long)(ptr - buf);
      if (slen > 0) morq_write(r, buf, slen);
      morq_write(r, entity, str_length(entity));
      buf = ++ptr;
    }
    else
    {
      ++ptr;
      if (cs_flag < 0)
      {
        /*
        ** Non-byte-unique character set may have an ASCII character
        ** following a leading-byte. Force the pointer ahead past the
        ** next byte so the entity escape won't see it.
        */
        if ((ch >= 0x80) && (ptr < eptr)) ++ptr;
      }
    }
  }

  /* Write out the trailing fragment */
  slen = (long)(ptr - buf);
  if (slen > 0) morq_write(r, buf, slen);
}

/*
** Run a REF cursor return and render the results
*/
static int owa_getrows(connection *c, owa_context *octx, request_rec *r,
                       char *outbuf, char *xtags, int cs_expand, int mode)
{
    sword status;
    sword cstatus;
    ub4   ncolumns = 0;
    ub4   n;
    int   maxwidth = 0;
    int   totwidth = 0;
    char *names[SQL_MAX_COLS];
    int   nlens[SQL_MAX_COLS];
    char *buffers[SQL_MAX_COLS];
    int   widths[SQL_MAX_COLS];
    char  tempname[SQL_NAME_MAX + 1];
    ub2   inds[OWAROWS_MAXFETCH];
    int   tlen;
    int   first_row = 1;
    ub4   lob_column = 0;
    ub2   cs_id = (ub2)0;
    int   cs_flag = 0;
    char *root_tag = octx->ref_root_tag;
    char *row_tag  = octx->ref_row_tag;
    char *prefix   = octx->ref_prefix;
    char *nsuri    = octx->ref_nsuri;
    int   root_len = 0;
    int   row_len = 0;
    int   bare_ns = 0;
    ub4   totalrows = 0;  /* Cumulative rows fetched */
    ub4   rowcount = 0;   /* Net rows fetched per block */
    int   numfetch = 1;   /* Array fetch default block size */
    int   position = 0;   /* Position within fetch block */
    char *json_jack = (char *)0;  /* Prevent JSON hijacking attacks */

    /* For now use outbuf for data expansions */

    if (octx->dad_csid)
    {
        /* Capture the CSID */
        cs_id = nls_csid(octx->dad_csid);
        /* Record whether it's an unsafe multibyte */
        if (cs_id > 0)
        {
          cs_flag = nls_cstype(cs_id);
          if (cs_flag < 1) cs_flag = 0;
          else if (cs_flag > 2) cs_flag = -1;
        }
    }
    /*
    ** cs_flag: -1 = unsafe non-byte-unique multibyte
    ** cs_flag:  1 = unicode-based
    ** cs_flag:  0 = byte-unique non-unicode character set
    */
    if ((cs_flag != 1) && (mode == OWAROWS_MODE_JSON))
    {
        /* ### Unsafe to render non-Unicode character data ### */
        debug_out(octx->diagfile,
                  "Warning: REF cursor returned JSON"
                  " in a non-Unicode character set\n",
                  (char *)0, (char *)0, 0, 0);
    }

    /* Get JSON wrapper object name (if any) */
    if (mode == OWAROWS_MODE_JSON)
    {
        if (*xtags)
        {
            /* Read and pre-escape the wrapper object field name */
            char *tempbuf = outbuf;
            if (xtags == tempbuf)
                tempbuf += str_length(xtags) + 1;
           tlen = util_json_escape(tempbuf, xtags, 1, 0) + 1;
           json_jack = (char *)morq_alloc(r, (size_t)tlen, 0);
           if (!json_jack) return(-tlen);
           mem_copy(json_jack, tempbuf, tlen);
        }
        else
        {
            debug_out(octx->diagfile,
                      "Warning: JSON return without object wrapper\n",
                      (char *)0, (char *)0, 0, 0);
        }
    }
    /* Pre-calculate the collection and row tags */
    else if (mode == OWAROWS_MODE_XML)
    {
        /* Optional override tag information */
        if (*xtags)
        {
            char *eptr = xtags;
            char *sptr;

            root_tag = row_tag = prefix = nsuri = (char *)0;

            if (*eptr)
            {
                sptr = eptr;
                while (*eptr > ' ') ++eptr;
                if (*eptr) *(eptr++) = '\0';
                root_tag = sptr;
            }

            if (*eptr)
            {
                sptr = eptr;
                while (*eptr > ' ') ++eptr;
                if (*eptr) *(eptr++) = '\0';
                row_tag = sptr;
            }

            if (*eptr)
            {
                sptr = eptr;
                while (*eptr > ' ') ++eptr;
                if (*eptr) *(eptr++) = '\0';
                prefix = sptr;
            }

            if (*eptr)
            {
                sptr = eptr;
                while (*eptr > ' ') ++eptr;
                if (*eptr) *(eptr++) = '\0';
                nsuri = sptr;
            }
        }

        /* Optional XML override prefix */
        if (prefix)
          if (*prefix == '\0')
            prefix = (char *)0;

        bare_ns = ((!prefix) && (nsuri));

        if (!root_tag) root_tag = "collection";
        if (!row_tag)  row_tag = "row";

        if (*root_tag == '\0')
        {
          root_tag = (char *)0; /* Omit root tag */
          bare_ns = 1;
        }
        else
        {
          /* Root tag with optional xmlns and prefix definition */
          root_len = create_tag(outbuf, root_tag, prefix, nsuri);
          ++root_len;
          root_tag = (char *)morq_alloc(r, (size_t)root_len, 0);
          if (!root_tag) return(-root_len);
          mem_copy(root_tag, outbuf, root_len);
          --root_len;
        }

        if (root_tag)
          /* Row tag with optional xmlns, shares prefix with root */
          row_len = create_tag(outbuf, row_tag, prefix,
                               (prefix) ? (char *)0 : nsuri);
        else
          /* Row tag with optional xmlns and prefix, no root */
          row_len = create_tag(outbuf, row_tag, prefix, nsuri);
        ++row_len;
        row_tag = (char *)morq_alloc(r, (size_t)row_len, 0);
        if (!row_tag) return(-row_len);
        mem_copy(row_tag, outbuf, row_len);
        --row_len;
    }

    /* Describe the columns from the REF cursor */
    while (ncolumns < SQL_MAX_COLS)
    {
        status = sql_describe_col(c, c->rset, ncolumns + 1,
                                  widths + ncolumns, tempname, cs_expand);
        if (status != OCI_SUCCESS) break;

        if (mode == OWAROWS_MODE_XML)
          tlen = create_tag(outbuf, tempname, prefix,
                            (prefix) ? (char *)0 : nsuri);
        else if (mode == OWAROWS_MODE_CSV)
          tlen = util_csv_escape(outbuf, tempname, ',');
        else /* JSON or other */
          tlen = util_json_escape(outbuf, tempname, 1, 0);

        nlens[ncolumns] = (tlen++);
        names[ncolumns] = (char *)morq_alloc(r, (size_t)tlen, 0);
        if (!names[ncolumns]) return(-tlen);
        mem_copy(names[ncolumns], outbuf, tlen);

        /* ### Should sub-divide outbuf if possible? ### */
        tlen = widths[ncolumns];
        if (tlen == 0)
        {
          /* This is a LOB */
          lob_column = ncolumns + 1; /* Only one should be allowed */
          break; /* No need to look further */
        }

        totwidth += tlen;
        if (tlen > maxwidth) maxwidth = tlen;

        ++ncolumns;
    }

    /* If a LOB is found, it's the only relevant column per row */
    if (lob_column > 0)
    {
        /* ### For now, only CLOB is supported, on a single column ### */
        status = sql_define_lob(c, c->rset, lob_column, (ub2)SQLT_CLOB);
    }
    /* No LOB columns so define all selected columns as strings */
    else
    {
        /* Compute max rows for array fetch */
        if (totwidth > 0)
        {
            numfetch = OWAROWS_BUFSIZE/totwidth;
            if (numfetch <= 0)
                numfetch = 1;
            else if (numfetch > OWAROWS_MAXFETCH)
                numfetch = OWAROWS_MAXFETCH;
        }

        for (n = 0; n < ncolumns; ++n)
        {
            int alen = widths[n];
            if (alen <= 0) continue;

            /* Allocate enough for an array fetch */
            tlen = alen * numfetch;
            buffers[n] = (char *)morq_alloc(r, (size_t)tlen, 0);
            if (!buffers[n]) return(-tlen);

            status = sql_define(c, c->rset, n + 1,
                                buffers[n], (sb4)alen, (ub2)SQLT_STR, inds);
            if (status != OCI_SUCCESS) break;
        }
    }

    /* Run the fetch loop */
    while (status == OCI_SUCCESS)
    {
        /* If necessary fetch another block of rows */
        if (rowcount == (ub4)0)
        {
            if (numfetch == 0) break; /* Fetch was exhausted */

            status = sql_fetch(c, c->rset, (ub4)numfetch);
            if (status == OCI_SUCCESS)
            {
                /* Got requested number of rows */
                rowcount = (ub4)numfetch;
            }
            else if (status == OCI_NO_DATA)
            {
                /* Get how many actual rows were returned */
                status = sql_get_rowcount(c, c->rset, &rowcount);
                numfetch = 0; /* Mark fetch as exhausted */
                rowcount -= totalrows;
            }
            if (status != OCI_SUCCESS) break;

            /* Compute net rows from this fetch */
            totalrows += rowcount;

            /* Fetch array position */
            position = 0;
        }

        /*
        ** Exit the loop if we're out of rows,
        ** unless we need to render the first row
        */
        if ((rowcount == (ub4)0) && (!first_row)) break;

        /* Open the first (or next) row object */
        switch (mode)
        {
        case OWAROWS_MODE_JSON:
            tlen = (lob_column > 0) ? 2 : 3;
            if (first_row) /* Opening bracket for collection plus first obj */
            {
                if (rowcount == (ub4)0) tlen = 1;
                if (json_jack)
                {
                    /* Wrapper object to prevent JSON hijacking */
                    morq_write(r, "{", (long)1);
                    morq_write(r, json_jack, (long)str_length(json_jack));
                    morq_write(r, ":", (long)1);
                }
                morq_write(r, "[\n{", (long)tlen);
            }
            else           /* Comma plus open next obj */
                morq_write(r, ",\n{", (long)tlen);
            break;
        case OWAROWS_MODE_XML:
            /* Open the collection */
            if ((first_row) && (root_tag))
            {
                morq_write(r, "<?xml version=\"1.0", (long)18);
                if (octx->dad_csid)
                {
                    char *csname = nls_iana(octx->dad_csid);
                    morq_write(r, "\" encoding=\"", (long)12);
                    morq_write(r, csname, (long)str_length(csname));
                }
                morq_write(r, "\"?>\n", (long)4);
                write_tag(r, 0, (rowcount > (ub4)0) ? 1 : 0,
                          bare_ns, root_tag, root_len);
            }
            /* Open the row */
            if ((lob_column == 0) && (rowcount > (ub4)0))
                write_tag(r, 0, 1, bare_ns, row_tag, row_len);
            break;
        case OWAROWS_MODE_CSV:
              /* Output the column names as the first row */
              if ((first_row) && (lob_column == 0))
              {
                for (n = 0; n < ncolumns; ++n)
                {
                    morq_write(r, names[n], (long)nlens[n]);
                    if ((n + 1) == ncolumns) morq_write(r, "\r\n", (long)2);
                    else                     morq_write(r, ",", (long)1);
                }
              }
            break;
        case OWAROWS_MODE_PLAIN:
            /* text/plain is the default */
        default:
            /* Nothing to output */
            break;
        }

        /* Exit the loop if we're out of rows */
        if (rowcount == (ub4)0) break;

        /* Count off another row fetched */
        --rowcount;
        first_row = 0;
        if (ncolumns == 0) continue;

        if (lob_column > (ub4)0)
        {
            /* ### Currently only CLOB is supported ### */
            OCILobLocator *plob = c->pclob;
            ub4            nbytes;
            ub4            total;
            ub4            buflen = HTBUF_BLOCK_READ;
            int            last_flag;

            /* Get the total length (in characters) */
            status = OCILobGetLength(c->svchp, c->errhp, plob, &total);
            if (status != OCI_SUCCESS) break;

            last_flag = (total == 0);

            /* Open the LOB for reading */
            status = OCILobOpen(c->svchp, c->errhp, plob, OCI_LOB_READONLY);
            if (status != OCI_SUCCESS) break;
            /* ### Is the above necessary? ### */

            /* Read loop */
            while (!last_flag)
            {
                /*
                ** Read data in LOB streaming mode
                ** This does on-the-fly character set conversion to the
                ** client character set. The chunks returned are measured
                ** in bytes, not characters, and only complete characters
                ** are returned in each chunk.
                */
                nbytes = 0;
                status = OCILobRead(c->svchp, c->errhp, plob,
                                    &nbytes, (ub4)1, (dvoid *)outbuf, buflen,
                                    (dvoid *)0, NULL, cs_id, (ub1)0);
                if (status == NEED_READ_DATA) status = OCI_SUCCESS;
                else                         last_flag = 1;
                if (status != OCI_SUCCESS) break;
                if (nbytes == 0) break; /* ### SOME SORT OF ERROR ### */

                /* Output is presumed valid without escaping */
                morq_write(r, outbuf, (long)nbytes);
            }

            /* Close the LOB locator */
            {
              int is_temp = 0; /* ### Should be of Oracle type "boolean" */
              sword lstatus = OCILobIsTemporary(c->envhp, c->errhp,
                                                plob, &is_temp);
              if (lstatus != OCI_SUCCESS) is_temp = 0;
              if (is_temp)
                lstatus = OCILobFreeTemporary(c->svchp, c->errhp, plob);
              else
                lstatus = OCILobClose(c->svchp, c->errhp, plob);

              if (status == OCI_SUCCESS) status = lstatus;
            }
            if (status != OCI_SUCCESS) break;

            morq_write(r, "\n", (long)1);
            continue;
        }

        for (n = 0; n < ncolumns; ++n)
        {
          char *pdata;

          if (widths[n] <= 0)
            pdata = "";
          else
            pdata = buffers[n] + (widths[n] * position);

          switch (mode)
          {
          case OWAROWS_MODE_JSON:
            /* Quoted name */
            morq_write(r, names[n], (long)nlens[n]);
            morq_write(r, ":", (long)1);

            /* Quoted value, or null */
            tlen = util_json_escape(outbuf, pdata, 1, 0);
            if (tlen > 2)
              morq_write(r, outbuf, (long)tlen);
            else
              morq_write(r, "null", (long)4);

            /* Write the columns, closing the row object on the last one */
            if ((n + 1) == ncolumns) morq_write(r, "}", (long)1);
            else                     morq_write(r, ",\n ", (long)3);
            break;

          case OWAROWS_MODE_XML:
            tlen = str_length(pdata);
            if (tlen == 0)
                /* Null tag */
                write_tag(r, -1, 1, bare_ns, names[n], nlens[n]);
            else
            {
                /* Opening tag */
                write_tag(r, 0, 0, bare_ns, names[n], nlens[n]);

                /* Entity-escaped XML */
                owa_render_xml(octx, r, pdata, tlen, 0, cs_flag);

                /* Closing tag */
                write_tag(r, 1, 1, bare_ns, names[n], nlens[n]);
            }

            /* Close the row after the last column is rendered */
            if ((n + 1) == ncolumns)
                write_tag(r, 1, 1, bare_ns, row_tag, row_len);

            break;

          case OWAROWS_MODE_CSV:
            tlen = util_csv_escape(outbuf, pdata, ',');
            if (tlen > 2)
              morq_write(r, outbuf, (long)tlen);
            if ((n + 1) == ncolumns) morq_write(r, "\r\n", (long)2);
            else                     morq_write(r, ",", (long)1);
            break;

          case OWAROWS_MODE_PLAIN:
            /* text/plain is the default */
          default:
            /* Emit unescaped concatenation of columns */
            tlen = str_length(pdata);
            /* ### Should we at least escape control characters? ### */
            if (tlen > 0)
              morq_write(r, pdata, (long)tlen);
            /* Newline terminator for the last column */
            if ((n + 1) == ncolumns) morq_write(r, "\n", (long)1);
            break;
          }
        }

        ++position;
    }
    
    /* Close the collection if query successful */
    if (!status)
    {
        switch (mode)
        {
        case OWAROWS_MODE_JSON:
            if (totalrows == (ub4)0)
                morq_write(r, "]", (long)1);
            else
                morq_write(r, "\n]", (long)2);
            if (json_jack)
                morq_write(r, "}", (long)1);
            break;
        case OWAROWS_MODE_XML:
            if (root_tag)
                write_tag(r, 1, 0, 1, root_tag, root_len);
            break;
        default:
            break;
        }

        if (octx->diagflag & DIAG_RESPONSE)
            debug_out(octx->diagfile, "  Wrote %d rows from REF cursor\n",
                      (char *)0, (char *)0, (int)totalrows, 0);
    }

    /* Close the cursor regardless */
    cstatus = sql_close_rset(c);
    if (!status) status = cstatus;

    return(status);
}

/*
** Get and print results from OWA
*/
int owa_getpage(connection *c, owa_context *octx, request_rec *r,
                char *physical, char *outbuf, int *rstatus,
                owa_request *owa_req, int rset_flag)
{
    sword    status;
    sb4      oerrno;
    int      diagflag;
    char    *optr;
    char    *sptr;
    char    *eptr;
    char    *rawbuf;
    ub2      rlen;
    int      i, j;
    int      bufwidth;
    int      arrlength;
    sb4      maxwidth;
    long_64  clen;
    int      olen = 0;
    int      dbcs, apcs;
    int      content_flag;
    int      errhdr_code;
    char    *header_type;
    char    *header_len;
    char    *header_status;
    char    *headval;
    char    *status_string = (char *)0;
    sb4      ecount;
    ub4      asize;
    ub4      amax;
    ub4      total = 0;
    char     pmore[LONG_MAXSTRLEN];
    sb2      inds[HTBUF_ARRAY_SIZE * 4];
    ub2      lens[HTBUF_ARRAY_SIZE * 4];
    char     ctype[HTBUF_LINE_LENGTH];
    char     session[HTBUF_HEADER_MAX];
    sb2      loginds[HTBUF_ARRAY_SIZE];
    ub2      loglens[HTBUF_ARRAY_SIZE];
    char     xtags[HTBUF_HEADER_MAX + 1];
    char    *logbuffer = (char *)0;
    ub4      logasize;
    ub4      logwidth;
    ub4      logamax;
    sb4      logecount;
    int      cs_expand = 1;
    int      rset_mode = 0;
#ifndef NO_FILE_CACHE
    char       *tempname = session;
    os_objhand  fp = os_nullfilehand;
#endif

    diagflag = octx->diagflag;

    c->lastsql = octx->get_stmt;
    c->errbuf[0] = '\0';

    /* If a REF cursor, transfer optional return tags from output string */
    if (rset_flag)
    {
        int fstatus = 0;
        status = sql_get_stmt_state(c, c->rset, &fstatus);
        if ((status != OCI_SUCCESS) || (fstatus != 1))
            rset_flag = 0;
        else if (*outbuf)
        {
            olen = str_length(outbuf);
            if (olen > (sizeof(xtags) - 1)) olen = sizeof(xtags) - 1;
            mem_copy(xtags, outbuf, olen);
        }
    }
    xtags[olen] = '\0';

    status = OCI_SUCCESS;

    /*
    ** Get character set IDs for client and server
    */
    dbcs = (octx->db_csid > 0)  ? nls_csid(octx->db_csid)  : 0;
    apcs = (octx->dad_csid > 0) ? nls_csid(octx->dad_csid) : 0;

    /*
    ** For now assume worst-case character expansion for strings if the
    ** character sets don't match and the local set is multi-byte.
    */
    if ((dbcs != apcs) && (nls_cstype(apcs) != 0)) cs_expand = 4;

    /*
    ** If running single-byte mode, reduce the width of the buffer
    ** (in bytes) to the maximum required, and increase the array
    ** size correspondingly.
    */
    arrlength = HTBUF_ARRAY_SIZE;
    bufwidth = HTBUF_LINE_LENGTH;
    if ((octx->altflags & ALT_MODE_GETRAW) ||
        ((nls_cstype(dbcs) == 0) && (nls_cstype(apcs) == 0)))
    {
        arrlength = (bufwidth * arrlength)/HTBUF_LINE_CHARS;
        if (arrlength > sizeof(lens)/sizeof(*lens))
            arrlength = sizeof(lens)/sizeof(*lens);
        bufwidth = HTBUF_LINE_CHARS;
    }
    maxwidth = (sb4)(HTBUF_BLOCK_SIZE * 4); /* Allow for 4x expansion */

    rawbuf = outbuf + maxwidth;

    asize = (ub4)arrlength;
    amax = (ub4)util_round((un_long)asize, octx->arr_round);

    /* Disable file system caching for REF cursors */
    if (rset_flag) physical = (char *)0;

    /* Set indicators for unbound arguments to null */
    c->bfile_ind = (ub2)-1;
    c->nlob_ind  = (ub2)-1;
    c->clob_ind  = (ub2)-1;
    c->blob_ind  = (ub2)-1;

    if (octx->altflags & ALT_MODE_GETRAW)
    {
        status = sql_bind_rarr(c, c->stmhp4, (ub4)1, outbuf, lens,
                               (sb4)bufwidth, inds, &asize, amax);
        if (status != OCI_SUCCESS) goto geterr;

        status = sql_bind_int(c, c->stmhp4, (ub4)2, &ecount);
        if (status != OCI_SUCCESS) goto geterr;
    }
    else
    if (octx->altflags & ALT_MODE_LOBS)
    {
        status = sql_bind_str(c, c->stmhp4, (ub4)1, outbuf, maxwidth);
        if (status != OCI_SUCCESS) goto geterr;

        status = sql_bind_lob(c, c->stmhp4, (ub4)2, SQLT_BLOB);
        if (status != OCI_SUCCESS) goto geterr;

        status = sql_bind_lob(c, c->stmhp4, (ub4)3, SQLT_CLOB);
        if (status != OCI_SUCCESS) goto geterr;
    }
    else
    if (octx->altflags & ALT_MODE_RAW)
    {
        status = sql_bind_str(c, c->stmhp4, (ub4)1, outbuf, maxwidth);
        if (status != OCI_SUCCESS) goto geterr;

        status = sql_bind_raw(c, c->stmhp4, (ub4)2,
                              rawbuf, &rlen, (sb4)HTBUF_PLSQL_MAX);
        if (status != OCI_SUCCESS) goto geterr;

        status = sql_bind_str(c, c->stmhp4, (ub4)3, pmore, (sb4)sizeof(pmore));
        if (status != OCI_SUCCESS) goto geterr;
    }
    else
    {
        status = sql_bind_arr(c, c->stmhp4, (ub4)1, outbuf, lens,
                              (sb4)bufwidth, inds, &asize, amax);
        if (status != OCI_SUCCESS) goto geterr;

        status = sql_bind_int(c, c->stmhp4, (ub4)2, &ecount);
        if (status != OCI_SUCCESS) goto geterr;

        /* Added to support logging returns */
        if (owa_req->logbuffer)
        {
            logwidth = (ub4)HTBUF_LINE_LENGTH;
            logamax = logasize = (ub4)HTBUF_ARRAY_SIZE;
            logecount = (sb4)logasize;
            logbuffer = owa_req->logbuffer;

            status = sql_bind_arr(c, c->stmhp4, (ub4)3, logbuffer, loglens,
                                  (sb4)logwidth, loginds, &logasize, logamax);
            if (status != OCI_SUCCESS) goto geterr;

            status = sql_bind_int(c, c->stmhp4, (ub4)4, &logecount);
            if (status != OCI_SUCCESS) goto geterr;
        }
    }

    clen = -1;

    *ctype = '\0';

    content_flag = 0;
    errhdr_code = 0;
    ecount = (sb4)arrlength;
    while (ecount == (sb4)arrlength)
    {
        *pmore = '\0';

        if (!(octx->altflags & ALT_MODE_GETRAW) &&
            (octx->altflags & (ALT_MODE_LOBS | ALT_MODE_RAW)))
        {
            outbuf[0] = '\n';
            outbuf[1] = '\0';
            rawbuf[0] = '\0';
        }
        else
        {
            rawbuf = (char *)"";
            optr = outbuf;
            for (i = 0; i < (int)asize; ++i)
            {
                *optr = '\0';
                optr += bufwidth;
                lens[i] = 0;
                inds[i] = -1;
            }
            asize = 0;

            if (owa_req->logbuffer)
            {
                optr = owa_req->logbuffer;
                for (i = 0; i < (int)logamax; ++i)
                {
                    *optr = '\0';
                    optr += logwidth;
                    loglens[i] = 0;
                    loginds[i] = -1;
                }
                logasize = 0;
            }
        }

        rlen = (ub2)0;
        status = sql_exec(c, c->stmhp4, (ub4)1, 0);
        if (status != OCI_SUCCESS) goto geterr;

        /*
        ** Concatenate all the returned elements into a single string.
        ** This will make it easier to guarantee that pieces that
        ** belong together are kept together.  It will also allow
        ** a single write to the content stream to be used later.
        */
        if (!(octx->altflags & ALT_MODE_GETRAW) &&
            (octx->altflags & (ALT_MODE_LOBS | ALT_MODE_RAW)))
        {
            if ((rlen) && (content_flag))
            {
                optr = rawbuf;
                olen = (int)rlen;
            }
            else
            {
                optr = outbuf;
                olen = str_length(outbuf);
            }
        }
        else
        {
            sptr = outbuf;
            for (i = 0; i < (int)ecount; ++i)
            {
                optr = outbuf + (i * bufwidth);
                for (j = 0; j < lens[i]; ++j) *(sptr++) = *(optr++);
            }
            *sptr = '\0';
            olen = (int)(sptr - outbuf);
            optr = outbuf;

            /*
            ** We limit the amount of log buffering in the OWA toolkit so that
            ** we can assume that all logging data is returned in the first
            ** call to get_page.  The contents are re-packed in place to
            ** eliminate trailing null bytes.
            */
            if (logbuffer)
            {
                owa_req->loglength = 0;
                sptr = logbuffer;
                for (i = 0; i < (int)logecount; ++i)
                {
                    owa_req->loglength += (int)(loglens[i]);
                    optr = logbuffer + (i * logwidth);
                    for (j = 0; j < loglens[i]; ++j) *(sptr++) = *(optr++);
                }
                *sptr = '\0';
                logbuffer = (char *)0;
            }
        }

        /* Search the result for content header */
        if (!content_flag)
        {
            /*
            ** No data returned from the OWA.  This could have happened
            ** as a result of a document read that failed to find a match.
            ** The best result in that case is "Document not found".
            ** In other cases, an error page might make more sense, but
            ** I can't distinguish those cases here.
            **
            ** If REST operation is configured then an OK is returned.
            */
            if (ecount == 0)
            {
                if (octx->dav_mode > 0)
                    *rstatus = OK;
                else
                    *rstatus = HTTP_NOT_FOUND;
                return(status);
            }

            /*
            ** Unfortunately the OWA doesn't cleanly separate the header
            ** from the content, and doesn't separate the lines of the
            ** header from each other.  Instead, the returned result is
            ** a series of string fragments representing parts of a
            ** continuous stream.  This is fine when transferring the
            ** content but Apache will require us to separate out each
            ** header element and put it in a table.  Complicating matters,
            ** the OWA doesn't always return a header.  If it does, then
            ** the header will be separated from the content by a single
            ** blank line.  However, blank lines can also occur in the
            ** content, too, so it's important to look for clues in the
            ** preceeding lines that indicate it is in fact a header.
            */

            /*
            ** 1. Parse for newlines, which are taken to be separating
            **    header elements.  Look for a few key indications that
            **    these lines might be part of a header.  Stop at the
            **    first blank line.
            */
            sptr = outbuf;
            eptr = (char *)0;
            header_type = (char *)0;
            header_len = (char *)0;
            header_status = (char *)0;
            while (1)
            {
                headval = (char *)0;
                for (eptr = sptr; *eptr != '\0'; ++eptr)
                {
                    if (*eptr == '\n')     break;
                    else if (*eptr == ':') headval = eptr;
                }
                if (*eptr == '\0') break; /* No more newlines found */
                if (eptr == sptr)  break; /* Blank line encountered */
                if (!headval)      break; /* Can't be a header line */

                if (!str_compare(sptr, "Content-Type:", 13, 1))
                {
                    header_type = sptr;
                    sptr += 13;
                    while (*sptr == ' ') ++sptr;
                    j = (int)(eptr - sptr);
                    if (j >= (int)sizeof(ctype)) j = sizeof(ctype) - 1;
                    j = str_concat(ctype, 0, sptr, j);
                    if ((octx->dad_csid) &&
                        (!str_substr(ctype, "charset=", 1)))
                    {
                        j = str_concat(ctype, j, "; charset=",
                                       sizeof(ctype) - 1);
                        str_concat(ctype, j, nls_iana(octx->dad_csid),
                                   sizeof(ctype) - 1);
                    }
                    content_flag = 1;
                }
                else if (!str_compare(sptr, "Content-Length:", 15, 1))
                {
                    header_len = sptr;
                    sptr += 15;
                    while (*sptr == ' ') ++sptr;
                    for (clen = 0; (*sptr >= '0') && (*sptr <= '9'); ++sptr)
                        clen = clen * 10 + (long_64)(*sptr - '0');
                    content_flag = 1;
                }
                else if ((!str_compare(sptr, "Location:", 9, 1)) ||
                         (!str_compare(sptr, "Refresh:", 8, 1)))
                {
                    content_flag = 1;
                    if (errhdr_code != HTTP_MOVED_PERMANENTLY)
                        errhdr_code = HTTP_MOVED_TEMPORARILY;
                }
                else if (!str_compare(sptr, "WWW-Authenticate:", 17, 1))
                {
                    content_flag = 1;
                    errhdr_code = HTTP_UNAUTHORIZED;
                }
                else if (!str_compare(sptr, "Set-Cookie:", 11, 1))
                {
                    /*
                    ** If sessioning is being used, check to see if
                    ** the PL/SQL code is returning a session cookie of
                    ** some sort.  If so, capture it and if necessary
                    ** update the connection to reflect it.
                    */
                    content_flag = 1;
                    if (octx->session)
                    {
                        for (sptr += 11; *sptr == ' '; ++sptr);
                        owa_find_session(octx, sptr, session);
                        if (*session)
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
                        else
                        {
                            mem_free((void *)(c->session));
                            c->session = (char *)0;
                        }
                    }
                }
                else if (!str_compare(sptr, "Status:", 7, 1))
                {
                    header_status = sptr;
                    content_flag = 1;
                    for (sptr += 7; *sptr == ' '; ++sptr);

                    status_string = sptr;
                    while (*status_string > ' ') ++status_string;
                    if (*status_string == ' ')
                      *(status_string++) = '\0';
                    else
                      status_string = (char *)0;

                    errhdr_code = str_atoi(sptr);
                    if (errhdr_code == HTTP_OK)
                        errhdr_code = 0;

                    if (status_string)
                    {
                      int ilen;
                      for (ilen = 0; status_string[ilen] >= ' '; ++ilen);
                      if (ilen == 0)
                        status_string = (char *)0;
                      else
                      {
                        char *tempstr = morq_alloc(r, (size_t)(ilen + 1), 0);
                        if (tempstr)
                        {
                          mem_copy(tempstr, status_string, ilen);
                          tempstr[ilen] = '\0';
                        }
                        status_string = tempstr;
                      }
                      *sptr = '\0';
                    }
                }
                sptr = eptr + 1;
            }

            /* Disable REF cursor handling for error pages */
            if (errhdr_code) rset_flag = 0;

            if (*ctype == '\0')
            {
                /* Add a default return type */
                char *default_ctype = (errhdr_code) ? octx->error_ctype :
                                                      octx->default_ctype;
                if (!default_ctype) default_ctype = "text/plain";

                /* In ref cursor mode the default is JSON */
                if (rset_flag) default_ctype = "text/csv";

                j = str_concat(ctype, 0, default_ctype, sizeof(ctype) - 1);

                if (octx->dad_csid)
                {
                    j = str_concat(ctype, j, "; charset=", sizeof(ctype) - 1);
                    str_concat(ctype, j, nls_iana(octx->dad_csid),
                               sizeof(ctype) - 1);
                }
            }

            /* For REF cursor choose from among the known content types */
            if (rset_flag)
            {
                /* Disable REF cursor mode for HTML content type */
                if (!str_compare(ctype, "text/html", 9, 0))
                    rset_flag = 0;
                else if (!str_compare(ctype, "application/json", 16, 0))
                    rset_mode = OWAROWS_MODE_JSON;
                else if (!str_compare(ctype, "text/xml", 8, 0))
                    rset_mode = OWAROWS_MODE_XML;
                else if (!str_compare(ctype, "text/csv", 8, 0))
                    rset_mode = OWAROWS_MODE_CSV;
                else if (!str_compare(ctype, "text/plain", 10, 0))
                    rset_mode = OWAROWS_MODE_PLAIN;
                /* ### Otherwise mode is unknown? ### */
            }

            morq_set_mimetype(r, ctype);

            if (diagflag & DIAG_RESPONSE)
                debug_out(octx->diagfile, "Response\n  Content-Type: %s\n",
                          ctype, (char *)0, 0, 0);

            /*
            ** 2. If we found a blank line preceeded by one of the
            **    key indicators, then we've found a header.
            */
            if ((sptr == eptr) && (content_flag))
            {
                /* Transfer the header elements to the table */
                sptr = outbuf;
                optr = outbuf;
                while (sptr < eptr)
                {
                    for (optr = sptr; *optr != '\n'; ++optr);
                    *(optr++) = '\0';
                    if ((sptr != header_type) && (sptr != header_len) &&
                        (sptr != header_status))
                    {
                        for (headval = sptr; *sptr != '\0'; ++sptr)
                            if (*sptr == ':')
                            {
                                *(sptr++) = '\0';
                                while (*sptr == ' ') ++sptr;
                                break;
                            }
                        nls_sanitize_header(sptr);
                        if (errhdr_code != 0)
                            morq_table_put(r,OWA_TABLE_HEADERR,0,headval,sptr);
                        else
                            morq_table_put(r,OWA_TABLE_HEADOUT,0,headval,sptr);
                        if (diagflag & DIAG_RESPONSE)
                            debug_out(octx->diagfile,
                                      "  %s: %s\n", headval, sptr, 0, 0);
                    }
                    sptr = optr;
                }
                ++optr;
		olen -= (int)(optr - outbuf);
            }
            else
            {
                optr = outbuf;
            }

            /*
            ** ### THE LENGTH IS VIRTUALLY CERTAIN TO BE WRONG FOR
            ** ### MULTIBYTE CHARACTER SETS.  IN THE CASE WHERE THE
            ** ### CLIENT IS MULTIBYTE BUT THE SERVER IS NOT, THE ERROR
            ** ### WILL OCCUR BECAUSE THE BYTE LENGTHS WILL BE DIFFERENT.
            ** ### EVEN IF BOTH CLIENT AND SERVER ARE SET TO THE SAME
            ** ### MULTIBYTE CHARACTER SET, IF YOU'RE USING AN OLDER OWA
            ** ### YOU'RE LIKELY TO GET AN ERROR BECAUSE THE OWA PL/SQL
            ** ### CODE MEASURES USING length(), SO IT'S COUNTING
            ** ### CHARACTERS, NOT BYTES.  AS A TEMPORARY WORK-AROUND
            ** ### THIS CODE IGNORES CONTENT-LENGTHS WHEN THE CLIENT IS
            ** ### RUNNING A MULTI-BYTE SET (UNLESS THE CLIENT IS DOING
            ** ### A RAW TRANSFER).
            */
            if (!(octx->altflags & ALT_MODE_GETRAW) &&
                (clen >= 0) && (rlen == (ub2)0) && (c->blob_ind == (ub2)-1))
            {
                if ((dbcs != apcs) &&
                    ((nls_cstype(dbcs)) || (nls_cstype(apcs))))
                {
                    clen = -1;
                    if (diagflag & DIAG_RESPONSE)
                        debug_out(octx->diagfile,
                                  "  DAD CS = %d, DB CS = %d\n",
                                  (char *)0, (char *)0, apcs, dbcs);
                }
            }
            /* We also don't know the content length if it's a REF cursor */
            if (rset_flag) clen = -1;

            /*
            ** If transferring data from the raw buffer, reset
            ** the buffer pointer and length value.
            */
            if ((octx->altflags & ALT_MODE_RAW) && (rlen > (ub2)0))
            {
                olen = (int)rlen;
                optr = rawbuf;
            }

            /*
            ** If more flag indicates end of content,
            ** set ecount to ensure loop exit.
            */
            if ((octx->altflags & ALT_MODE_RAW) && (*pmore == '\0'))
                ecount = 0;

            /*
            ** If this is the only block of content, use the client-side
            ** length (to account for any character-set conversion);
            ** otherwise just hope clen is not set or was set properly
            ** on the server side.
            */
            if ((olen > 0) && (ecount < (sb4)arrlength))
                clen = (long_64)olen;

            /*
            ** Set content length (if known) and return the header
            */
            if (clen >= 0)
            {
                if (mowa_check_keepalive(octx->keepalive_flag))
                    morq_set_length(r, (size_t)clen, 0);

                if (diagflag & DIAG_RESPONSE)
                    debug_out(octx->diagfile, "  Content-Length: %d\n",
                              (char *)0, (char *)0, (int)clen, 0);
            }

#ifndef NO_FILE_CACHE
            if (physical)
            {
                /* Make sure file extension matches mime type */
                *tempname = '\0';
                util_set_mime(physical, tempname,
                              ((rlen > 0) || (c->blob_ind != (ub2)-1)));
                sptr = str_char(ctype, ';', 0);
                j = (sptr) ? ((int)(sptr - ctype)) : str_length(ctype);
                if (str_compare(tempname, ctype, j, 1) || (tempname[j]))
                    physical = (char *)0;
            }
#endif

            if (errhdr_code != 0)
            {
                if (errhdr_code == HTTP_MOVED_TEMPORARILY)
                {
#ifdef NEVER
                    /* ### WHY IS THIS NECESSARY FOR POST REQUESTS? ### */
                    *rstatus = errhdr_code;
                    return(status);
#endif
                }
                else if (((olen == 0) || (!header_len)) && (!header_type))
                {
                    /* Allow Apache to return the error page */
                    *rstatus = errhdr_code;
                    /* ### How to set the status_string? ### */
                    return(status);
                }
                /* Otherwise handle error page in this routine */
                morq_set_status(r, errhdr_code, status_string);
            }

            /*
            ** 3. Now send the remainder of this content block.
            **    - For LOB mode, use LOB streaming
            **    - For RAW mode, transfer RAW content
            **    - Otherwise transfer remainder of character content
            */
            if (!(octx->altflags & ALT_MODE_GETRAW) &&
                (octx->altflags & ALT_MODE_LOBS))
            {
                olen = 0;   /* Avoid transferring any more from header */
                ecount = 0; /* Ensure exit from surrounding fetch loop */
                status = owa_readlob(c, octx, r, (char *)0, (char *)0,
                                     physical, outbuf);
                physical = (char *)0;
            }
            else
            /*
            ** LOB mode sent the header itself (to allow owa_readlob
            ** to compute the LOB length and add it to the header).
            ** For all other modes, transmit the header now.
            */
            morq_send_header(r);

#ifndef NO_FILE_CACHE
            if (physical)
            {
                fp = file_open_temp(physical, tempname, HTBUF_HEADER_MAX);
                if (InvalidFile(fp)) physical = (char *)0;
            }
#endif

            content_flag = 1;

            /*
            ** Fall through to transfer the remaining data, if any
            ** At this point optr should point to either the remainder
            ** of the character data, or to the raw buffer, and olen
            ** should be the length (in bytes).
            */
        }

        /* If in REF cursor mode, run the fetch loop and then exit */
        if (rset_flag)
        {
            olen = 0;   /* Avoid transferring any more from header */
            ecount = 0; /* Ensure exit from surrounding fetch loop */
            status = owa_getrows(c, octx, r, outbuf,
                                 xtags, cs_expand, rset_mode);
            /* ### Ambiguous status returned here ### */
        }

        if (olen > 0)
        {
            /*
            ** Transfer block of raw or character data
            */
            if ((diagflag & DIAG_CONTENT) && (optr != rawbuf))
                debug_out(octx->diagfile, "%s\n", optr, (char *)0, 0, 0);
            morq_write(r, optr, (long)olen);
#ifndef NO_FILE_CACHE
            if (!InvalidFile(fp)) file_write_data(fp, optr, olen);
#endif
            total += (ub4)olen;
            if (diagflag & DIAG_RESPONSE)
                debug_out(octx->diagfile, "  Wrote block of %d bytes\n",
                          (char *)0, (char *)0, olen, 0);
        }
    }

geterr:
    if (status != OCI_SUCCESS)
    {
        oerrno = sql_get_error(c); 
        if (oerrno) status = oerrno;
#ifndef NO_FILE_CACHE
        if (!InvalidFile(fp)) total = (~(ub4)0); /* Ensure delete */
#endif
    }
    else
    {
        c->lastsql = (char *)0;
    }

#ifndef NO_FILE_CACHE
    if (!InvalidFile(fp))
    {
        if (total > CACHE_MAX_SIZE) physical = (char *)0;

        file_close(fp);
        file_move(tempname, physical);
    }
#endif

    return(status);
}

/*
** Get header-only return from OWA via GET_PAGE
*/
int owa_getheader(connection *c, owa_context *octx, request_rec *r,
                  char *ctype, char *cdisp, char *outbuf, int *rstatus)
{
    sword    status;
    sb4      oerrno;
    char    *optr;
    char    *sptr;
    char    *eptr;
    int      i, j;
    int      bufwidth;
    int      arrlength;
    long_64  clen;
#ifdef OBSOLETE
    int      olen;
    sb4      maxwidth;
#endif
    int      dbcs, apcs;
    int      errhdr_code;
    char    *headval;
    char    *status_string = (char *)0;
    sb4      ecount;
    ub4      asize;
    ub4      amax;
    sb2      inds[HTBUF_ARRAY_SIZE * 4];
    ub2      lens[HTBUF_ARRAY_SIZE * 4];

    status = OCI_SUCCESS;

    c->lastsql = octx->get_stmt;
    c->errbuf[0] = '\0';

    /*
    ** Get character set IDs for client and server
    */
    dbcs = (octx->db_csid > 0)  ? nls_csid(octx->db_csid)  : 0;
    apcs = (octx->dad_csid > 0) ? nls_csid(octx->dad_csid) : 0;

    /*
    ** If running single-byte mode, reduce the width of the buffer
    ** (in bytes) to the maximum required, and increase the array
    ** size correspondingly.
    */
    arrlength = HTBUF_ARRAY_SIZE;
    bufwidth = HTBUF_LINE_LENGTH;
    if ((nls_cstype(dbcs) == 0) && (nls_cstype(apcs) == 0))
    {
        arrlength = (bufwidth * arrlength)/HTBUF_LINE_CHARS;
        if (arrlength > sizeof(lens)/sizeof(*lens))
            arrlength = sizeof(lens)/sizeof(*lens);
        bufwidth = HTBUF_LINE_CHARS;
    }
#ifdef OBSOLETE
    maxwidth = (sb4)(HTBUF_BLOCK_SIZE * 4); /* Allow for 4x expansion */
#endif

    asize = (ub4)arrlength;
    amax = (ub4)util_round((un_long)asize, octx->arr_round);

    status = sql_bind_arr(c, c->stmhp4, (ub4)1, outbuf, lens,
                          (sb4)bufwidth, inds, &asize, amax);
    if (status != OCI_SUCCESS) goto headerr;

    status = sql_bind_int(c, c->stmhp4, (ub4)2, &ecount);
    if (status != OCI_SUCCESS) goto headerr;

    clen = -1;

    *ctype = '\0';
    *cdisp = '\0';

    errhdr_code = 0;
    ecount = (sb4)arrlength;

    optr = outbuf;
    for (i = 0; i < (int)asize; ++i)
    {
        *optr = '\0';
        optr += bufwidth;
        lens[i] = 0;
    }
    asize = 0;

    status = sql_exec(c, c->stmhp4, (ub4)1, 0);
    if (status != OCI_SUCCESS) goto headerr;

    /*
    ** Concatenate all the returned elements into a single string.
    ** This will make it easier to guarantee that pieces that
    ** belong together are kept together.  It will also allow
    ** a single write to the content stream to be used later.
    */
    sptr = outbuf;
    for (i = 0; i < (int)ecount; ++i)
    {
        optr = outbuf + (i * bufwidth);
        for (j = 0; j < lens[i]; ++j) *(sptr++) = *(optr++);
    }
    *sptr = '\0';
#ifdef OBSOLETE
    olen = sptr - outbuf;
#endif
    optr = outbuf;

    /* No data returned from the OWA. */
    if (ecount == 0)
    {
        *rstatus = HTTP_NOT_FOUND;
        return(status);
    }

    /* Scan for header elements */
    sptr = outbuf;
    eptr = (char *)0;
    while (1)
    {
        headval = (char *)0;
        for (eptr = sptr; *eptr != '\0'; ++eptr)
        {
            if (*eptr == '\n')     break;
            else if (*eptr == ':') headval = eptr;
        }
        if (*eptr == '\0') break; /* No more newlines found */
        if (eptr == sptr)  break; /* Blank line encountered */
        if (!headval)      break; /* Can't be a header line */

        if (!str_compare(sptr, "Content-Type:", 13, 1))
        {
            sptr += 13;
            while (*sptr == ' ') ++sptr;
            j = (int)(eptr - sptr);
            if (j >= HTBUF_HEADER_MAX) j = HTBUF_HEADER_MAX - 1;
            j = str_concat(ctype, 0, sptr, j);
            if ((octx->dad_csid) &&
                (!str_substr(ctype, "charset=", 1)))
            {
                j = str_concat(ctype, j, "; charset=",
                               HTBUF_HEADER_MAX - 1);
                str_concat(ctype, j, nls_iana(octx->dad_csid),
                           HTBUF_HEADER_MAX - 1);
            }
        }
        else if (!str_compare(sptr, "Content-Length:", 15, 1))
        {
            sptr += 15;
            while (*sptr == ' ') ++sptr;
            for (clen = 0; (*sptr >= '0') && (*sptr <= '9'); ++sptr)
                clen = clen * 10 + (long_64)(*sptr - '0');
        }
        else if (!str_compare(sptr, "Content-Disposition:", 20, 1))
        {
            sptr += 20;
            while (*sptr == ' ') ++sptr;
            headval = sptr;
            while (*sptr >= ' ') ++sptr;
            *sptr = '\0';
            j = (int)(sptr - headval);
            if (j >= HTBUF_HEADER_MAX) j = HTBUF_HEADER_MAX - 1;
            j = str_concat(cdisp, 0, headval, j);
            nls_sanitize_header(headval);
            morq_table_put(r,OWA_TABLE_HEADOUT,0,"Content-Disposition",headval);
            if (octx->diagflag & DIAG_RESPONSE)
              debug_out(octx->diagfile,
                        "  Content-Disposition: %s\n", headval, (char *)0, 0, 0);
        }
        else if (!str_compare(sptr, "Last-Modified:", 14, 1))
        {
            sptr += 14;
            while (*sptr == ' ') ++sptr;
            headval = sptr;
            while (*sptr >= ' ') ++sptr;
            *sptr = '\0';
            nls_sanitize_header(headval);
            morq_table_put(r,OWA_TABLE_HEADOUT,0,"Last-Modified",headval);
            if (octx->diagflag & DIAG_RESPONSE)
              debug_out(octx->diagfile,
                        "  Last-Modified: %s\n", headval, (char *)0, 0, 0);
        }
        else if (!str_compare(sptr, "Cache-Control:", 14, 1))
        {
            sptr += 14;
            while (*sptr == ' ') ++sptr;
            headval = sptr;
            while (*sptr >= ' ') ++sptr;
            *sptr = '\0';
            nls_sanitize_header(headval);
            morq_table_put(r,OWA_TABLE_HEADOUT,0,"Cache-Control",headval);
            if (octx->diagflag & DIAG_RESPONSE)
              debug_out(octx->diagfile,
                        "  Cache-Control: %s\n", headval, (char *)0, 0, 0);
        }
        else if (!str_compare(sptr, "Expires:", 8, 1))
        {
            sptr += 8;
            while (*sptr == ' ') ++sptr;
            headval = sptr;
            while (*sptr >= ' ') ++sptr;
            *sptr = '\0';
            nls_sanitize_header(headval);
            morq_table_put(r,OWA_TABLE_HEADOUT,0,"Expires",headval);
            if (octx->diagflag & DIAG_RESPONSE)
              debug_out(octx->diagfile,
                        "  Expires: %s\n", headval, (char *)0, 0, 0);
        }
        else if (!str_compare(sptr, "ETag:", 5, 1))
        {
            sptr += 5;
            while (*sptr == ' ') ++sptr;
            headval = sptr;
            while (*sptr >= ' ') ++sptr;
            *sptr = '\0';
            nls_sanitize_header(headval);
            morq_table_put(r,OWA_TABLE_HEADOUT,0,"ETag",headval);
            if (octx->diagflag & DIAG_RESPONSE)
              debug_out(octx->diagfile,
                        "  ETag: %s\n", headval, (char *)0, 0, 0);
        }
        else if ((!str_compare(sptr, "Location:", 9, 1)) ||
                 (!str_compare(sptr, "Refresh:", 8, 1)))
        {
            if (errhdr_code != HTTP_MOVED_PERMANENTLY)
                errhdr_code = HTTP_MOVED_TEMPORARILY;
        }
        else if (!str_compare(sptr, "WWW-Authenticate:", 17, 1))
        {
            errhdr_code = HTTP_UNAUTHORIZED;
        }
        else if (!str_compare(sptr, "Status:", 7, 1))
        {
            for (sptr += 7; *sptr == ' '; ++sptr);

            status_string = sptr;
            while (*status_string > ' ') ++status_string;
            if (*status_string == ' ')
            {
              *(status_string++) = '\0';
            }
            else
              status_string = (char *)0;

            errhdr_code = str_atoi(sptr);
            if (errhdr_code == HTTP_OK)
                errhdr_code = 0;

                    if (status_string)
                    {
                      int ilen;
                      for (ilen = 0; status_string[ilen] >= ' '; ++ilen);
                      if (ilen == 0)
                        status_string = (char *)0;
                      else
                      {
                        char *tempstr = morq_alloc(r, (size_t)(ilen + 1), 0);
                        if (tempstr)
                        {
                          mem_copy(tempstr, status_string, ilen);
                          tempstr[ilen] = '\0';
                        }
                        status_string = tempstr;
                      }
                      *sptr = '\0';
                    }
        }
        sptr = eptr + 1;
    }

    *rstatus = errhdr_code;

headerr:
    if (status != OCI_SUCCESS)
    {
        oerrno = sql_get_error(c); 
        if (oerrno) status = oerrno;
    }
    else
    {
        c->lastsql = (char *)0;
    }

    return(status);
}

void owa_set_statements(owa_context *octx)
{
    char *alternate;
    char *sptr;
    char *aptr;
    char *nptr;

    switch (octx->rsetflag)
    {
    case RSET_MODE_FULL:  sptr = sql_stmt5b; break;
    case RSET_MODE_LAZY:  sptr = sql_stmt5c; break;
    case RSET_MODE_INIT:  sptr = sql_stmt5d; break;
    default:              sptr = sql_stmt5a; break;
    }
    octx->reset_stmt = sptr;

    alternate = octx->alternate;
    if (!(octx->altflags & ALT_MODE_LOGGING))
      if (!str_compare(octx->alternate, "OWA", -1, 1))
        alternate = (char *)0;

    if (alternate)
    {
        sptr = octx->cgi_stmt;

        if (octx->altflags & (ALT_MODE_CGITIME | ALT_MODE_CGIPOST))
            aptr = sql_stmt1b;
        else if (octx->altflags & ALT_MODE_IPADDR)
            aptr = sql_stmt1c;
        else
            aptr = sql_stmt1a;

        do
        {
            nptr = str_substr(aptr, "OWA.", 0);
            if (nptr)
            {
                sptr += str_concat(sptr, 0, aptr, (int)(nptr - aptr));
                sptr += str_concat(sptr, 0, alternate, 32);
                aptr = nptr + 3;
            }
            else
                str_copy(sptr, aptr);
        }
        while (nptr);

        sptr = octx->sec_stmt;
        if (octx->altflags & ALT_MODE_GETRAW)       aptr = sql_stmt2c;
        else if (octx->altflags & ALT_MODE_SETSEC)  aptr = sql_stmt2b;
        else                                        aptr = sql_stmt2a;

        do
        {
            nptr = str_substr(aptr, "OWA.", 0);
            if (nptr)
            {
                sptr += str_concat(sptr, 0, aptr, (int)(nptr - aptr));
                sptr += str_concat(sptr, 0, alternate, 32);
                aptr = nptr + 3;
            }
            else
                str_copy(sptr, aptr);
        }
        while (nptr);

        sptr = octx->get_stmt;
        if (octx->altflags & ALT_MODE_GETRAW)
        {
          sptr += str_concat(sptr, 0, "begin ", -1);
          sptr += str_concat(sptr, 0, alternate, 32);
          sptr += str_concat(sptr, 0,
                             ".GET_PAGE_RAW(:linearr, :nlines); \n", -1);
        }
        else
        {
          sptr += str_concat(sptr, 0, "begin ", -1);
          sptr += str_concat(sptr, 0, alternate, 32);
          if (octx->altflags & ALT_MODE_LOBS)
            sptr += str_concat(sptr, 0,
                               ".GET_PAGE(:header, :bptr, :cptr); ", -1);
          else
          if (octx->altflags & ALT_MODE_RAW)
              sptr += str_concat(sptr, 0,
                         ".GET_PAGE(:cbuffer, :rbuffer, :moreflag); ", -1);
          else
              sptr += str_concat(sptr, 0,
                                 ".GET_PAGE(:linearr, :nlines); ", -1);
        }

        if (octx->altflags & ALT_MODE_LOGGING)
        {
            sptr += str_concat(sptr, 0, alternate, 32);
            sptr += str_concat(sptr, 0,
                               ".GET_LOGS(:logs, :ilogs); ", -1);
        }

        sptr += str_concat(sptr, 0, "end;", -1);
    }
    else
    {
        if (octx->altflags & ALT_MODE_IPADDR)
          str_copy(octx->cgi_stmt, sql_stmt1c);
        else
          str_copy(octx->cgi_stmt, sql_stmt1a);
        if (octx->altflags & ALT_MODE_GETRAW)
        {
            str_copy(octx->get_stmt, sql_stmt4b);
            str_copy(octx->sec_stmt, sql_stmt2c);
        }
        else
        {
            str_copy(octx->get_stmt, sql_stmt4a);
            str_copy(octx->sec_stmt,
                     (octx->altflags & ALT_MODE_SETSEC) ? sql_stmt2b:
                                                          sql_stmt2a);
        }
    }
}

/*
** Set up output arguments for special operations
**   0 - Returning <32K statement>, <4K bind>, <integer content-length>
**   1 - Returning <blob>, <clob>, <nclob>, <bfile>
**   2 - Returning <4K URI>, <32K checksum string>
**   3 - Returning <4K file name>
**   4 - Returning <4K bind>, <blob>
**   5 - Returning <ref cursor>
*/
void owa_out_args(char *argarr[], int mode_flag)
{
    switch (mode_flag)
    {
    case 0:
        argarr[0] = var_stmt;
        argarr[1] = var_bind;
        argarr[2] = var_clen;
        break;
    case 1:
        argarr[0] = var_blob;
        argarr[1] = var_clob;
        argarr[2] = var_nlob;
        argarr[3] = var_bfile;
        break;
    case 2:
        argarr[0] = var_curi;
        argarr[1] = var_csum;
        break;
    case 3:
        argarr[0] = var_fname;
        break;
    case 4:
        argarr[0] = var_bind;
        argarr[1] = var_blob;
        break;
    case 5:
        argarr[0] = var_rset;
        argarr[1] = var_xtags;
        break;
    default:
        break;
    }
}
