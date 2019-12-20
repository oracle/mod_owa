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
** File:         owasql.c
** Description:  OCI structures and functions
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
** 02/18/2002   D. McMahon      Ensure oerrno value is zero-initialized
** 11/14/2002   D. McMahon      Add sql_bind_vcs
** 05/22/2003   D. McMahon      Scan for last @ in connect string
** 12/17/2003   D. McMahon      Normal describe skips signature mismatches
** 01/19/2004   D. McMahon      Free session string on disconnect
** 01/29/2004   D. McMahon      Patch sql_bind_ptrs to avoid OCI indicator bug
** 02/17/2006   D. McMahon      Add synonym support to describe logic
** 02/02/2007   D. McMahon      sql_connect ignores authrealm for null args
** 03/21/2007   D. McMahon      Improve OCIEnvCreate error messages
** 04/12/2007   D. McMahon      Fix describe for PUBLIC synonyms
** 04/07/2008   D. McMahon      Fix NLS handling for reconnections
** 04/08/2008   D. McMahon      Fix OCI session reuse password compare
** 08/20/2008   C. Fischer      Added OwaOptmizer
** 09/02/2009   D. McMahon      Add support for external credentials
** 04/07/2011   D. McMahon      Update check for timezone files
** 04/14/2011   D. McMahon      Unscramble DB connect string (moved)
** 11/15/2011   D. McMahon      Prevent a crash if null SQL passed to parse
** 11/27/2012   D. McMahon      GCC fixes, Win-64 porting
** 04/12/2013   D. McMahon      Add support for REST operations
** 06/06/2013   D. McMahon      Add REF cursor operations
** 09/19/2013   D. McMahon      Add sql_bind_long for 64-bit content types
** 09/24/2013   D. McMahon      Check SUCCESS_WITH_INFO status from sql_connect
** 04/27/2014   D. McMahon      Fix uninitialized variables on connection
** 03/06/2015   D. McMahon      Common error codes due to file permissions
** 04/26/2015   D. McMahon      Report external auth errors differently
** 10/13/2015   D. McMahon      Add sql_bind_chr()
** 02/25/2017   D. McMahon      Dump environment on connect failures
** 01/02/2018   D. McMahon      More info for environment handle failure errors
*/

#define WITH_OCI
#include <modowa.h>

#define MAX_CSNAME    64                  /* Longest character set name    */

/*
** Get size of array of connection objects
*/
int sql_conn_size(int arrsz)
{
    return(arrsz * sizeof(connection));
}

/*
** One-time initialization of OCI.  This is the old technique that is
** compatible with 8.0; for 8i and beyond we don't need it.
*/
int sql_init(void)
{
    return(0); /* ### ASSUMES ERROR STATUS IS ALWAYS NON-ZERO */
}

/*
** Parse SQL statement
*/
sword sql_parse(connection *c, OraCursor stmhp, char *stmt, int slen)
{
    sword status;
    ub4   stmtlen;
    if (!stmt) stmt = "";
    stmtlen = (slen < 0) ? (ub4)str_length(stmt) : (ub4)slen;
    status = OCIStmtPrepare(stmhp, c->errhp, (text *)stmt, stmtlen,
                            (ub4)OCI_NTV_SYNTAX, (ub4)OCI_DEFAULT);
    return(status);
}

/*
** Fetch next row or piece
** Always pass 1
*/
sword sql_fetch(connection *c, OraCursor stmhp, ub4 numrows)
{
    return(OCIStmtFetch(stmhp, c->errhp, numrows,
                        (ub2)OCI_FETCH_NEXT, (ub4)OCI_DEFAULT));
}

/*
** Execute PL/SQL statement through OCI
*/
sword sql_exec(connection *c, OraCursor stmhp, ub4 niters, int exact)
{
    return(OCIStmtExecute(c->svchp, stmhp, c->errhp, niters, (ub4)0,
                          (OCISnapshot *)0, (OCISnapshot *)0,
                          (exact) ? (ub4)OCI_EXACT_FETCH : (ub4)OCI_DEFAULT));
}

/*
** Close a statement (for REF cursors)
*/
sword sql_close_rset(connection *c)
{
    sword status = (sword) 0;
    if (c->rset)
    {
      status = OCIHandleFree((dvoid *)(c->rset), (ub4)OCI_HTYPE_STMT);
      c->rset = (OraCursor)0;
    }
    return(status);
}

/*
** Commit or roll back a transaction
*/
sword sql_transact(connection *c, int rollback_flag)
{
    sword status;
    if (rollback_flag)
        status = OCITransRollback(c->svchp, c->errhp, (ub4)OCI_DEFAULT);
    else
        status = OCITransCommit(c->svchp, c->errhp, (ub4)OCI_DEFAULT);
    return(status);
}

/*
** Get NLS character set from OCI
*/
void sql_get_nls(connection *c, owa_context *octx)
{
    sword      status;
    char       buf[MAX_CSNAME + 1];
    int        i;
    char      *sptr;

    if (octx->nls_init) return;

    octx->ora_csid = 0;

    /*
    ** First, attempt to find a match to the OCI's NLS character set
    */
    status = OCINlsGetInfo(c->envhp, c->errhp, (text *)buf, sizeof(buf) - 1,
                           (ub2)OCI_NLS_CHARACTER_SET);
    if (status == OCI_SUCCESS)
    {
        i = nls_csx_from_oracle(buf);
        if (i > 0) octx->ora_csid = i;
    }

    /*
    ** If that fails, then default to the character set from httpd.conf
    */
    if ((octx->ora_csid == 0) && (octx->nls_cs[0] != '\0'))
    {
        i = nls_csx_from_oracle(octx->nls_cs);
        if (i > 0) octx->ora_csid = i;
    }

    if (octx->ora_csid)
    {
      if (octx->dad_csid)
      {
        if (nls_csid(octx->dad_csid) == nls_csid(octx->ora_csid))
            octx->ora_csid = octx->dad_csid;
        else
            c->csid = nls_csid(octx->dad_csid);
      }
      else
        octx->dad_csid = octx->ora_csid;
    }

    /*
    ** Get database character set from V$NLS_PARAMETERS
    */
    octx->db_csid = octx->ora_csid;
    sptr = (char *)"select VALUE from V$NLS_PARAMETERS"
                   " where PARAMETER='NLS_CHARACTERSET'";
    status = sql_parse(c, c->stmhp3, sptr, -1);
    if (status == OCI_SUCCESS)
    {
        status = sql_define(c, c->stmhp3, (ub4)1, (dvoid *)buf,
                            (sb4)MAX_CSNAME, (ub2)SQLT_STR, (dvoid *)0);
        if (status == OCI_SUCCESS)
        {
            status = sql_exec(c, c->stmhp3, (ub4)1, 1);
            if (status == OCI_SUCCESS)
            {
                i = nls_csx_from_oracle(buf);
                if (i > 0) octx->db_csid = i;
            }
        }
    }

    octx->nls_init = 1;
}

/*
** Get a normal error from the OCI error handle and return the code
*/
sb4 sql_get_error(connection *c)
{
    sb4   oerrno = 0;
    char  msgbuf[OCI_ERROR_MAXMSG_SIZE];
    char *errbuf;

    errbuf = c->errbuf;
    if (!errbuf) errbuf = msgbuf;
    OCIErrorGet((dvoid *)(c->errhp), (ub4)1, (text *)0,
                &oerrno, (text *)errbuf,
                (ub4)OCI_ERROR_MAXMSG_SIZE, (ub4)OCI_HTYPE_ERROR);

    return(oerrno);
}

/*
** Get statement error position
*/
int sql_get_errpos(connection *c, OraCursor stmhp)
{
    ub4   errpos;
    sword status;

    status = OCIAttrGet(stmhp, (ub4)OCI_HTYPE_STMT, (dvoid *)&errpos,
                        (ub4 *)0, (ub4)OCI_ATTR_PARSE_ERROR_OFFSET, c->errhp);

    return((status == OCI_SUCCESS) ? (int)errpos : 0);
}

/*
** Check to see if a file under ORACLE_HOME is readable
*/
static int sql_check_access(char *ohome, char *fname, char *fpath)
{
#ifndef NO_FILE_CACHE
    char       *sptr;
    un_long     fsz, fage;
    os_objhand  fp;
    int         olen;

    olen = str_length(ohome);
    mem_copy(fpath, ohome, olen);
    sptr = fpath + olen;
    str_concat(sptr, 0, fname, (HTBUF_HEADER_MAX-olen-1));
#ifdef MODOWA_WINDOWS
    while (*sptr)
    {
        if (*sptr == '/') *sptr = os_dir_separator;
        ++sptr;
    }
#endif
    fp = file_open_read(fname, &fsz, &fage);
    if (fp == os_nullfilehand) return((sword)1);
    file_close(fp);
#endif
    return((sword)0);
}

/*
** Dump the environment after a connection failure
*/
static void sql_dump_env(char *phase)
{
    os_objhand  fp;
    char       *var = "----------------------------------------\n";
    un_long     i;
    int         oulen;
    char        osusr[SQL_NAME_MAX+1];
    char       *dumpfile = os_env_get("MODOWA_DUMP_ENV");

    if (!dumpfile) return;

    fp = file_open_write(dumpfile, 1, 1);
    if (InvalidFile(fp)) return;

    file_write_text(fp, var, str_length(var));
    file_write_text(fp, "phase [", 7);
    file_write_text(fp, phase, str_length(phase));
    file_write_text(fp, "]", 1);

    oulen = os_get_user(osusr, (int)sizeof(osusr));
    if (oulen > 0)
    {
        file_write_text(fp, " user [", 7);
        file_write_text(fp, osusr, oulen);
        file_write_text(fp, "]", 1);
    }
    file_write_text(fp, "\n\n", 2);

    var = NULL;

    for (i = 0; i < 1000; ++i)
    {
        var = os_env_dump(var, i);
        if (!var) break;
        file_write_text(fp, var, str_length(var));
        file_write_text(fp, "\n", 1);
    }

    file_close(fp);
}

/*
** Create OCI connection
*/
sword sql_connect(connection *c, owa_context *octx,
                  char *authuser, char *authpass, sb4 *errinfo)
{
    sword  status;
    sb4    oerrno = 0;
    char  *phase = "Create OCI context";
    char  *sptr;
    char  *optr;
    char  *username;
    char  *password;
    char  *database = (char *)0;
    char  *connstr = (char *)0;
    int    ulen, plen, dlen;
    int    extauth = 0;
    ub4    lobempty;
    ub4    buflen;
    char   osusr[SQL_NAME_MAX+1];
    char   buf[2048];

    c->mem_err = 0;
    c->lastsql = (char *)0;
    c->errbuf[0] = '\0';

    if (authuser == authpass) connstr = authuser;

    if (errinfo) *errinfo = (sb4)0;

    if (c->c_lock == C_LOCK_INUSE)
    {
        if ((octx->authrealm) && (authuser) && (authpass) && (!connstr))
        {
            /* Check current session username/password */
            username = (char *)0;
            status = OCIAttrGet(c->seshp, (ub4)OCI_HTYPE_SESSION,
                                (dvoid *)&username, &buflen,
                                (ub4)OCI_ATTR_USERNAME, c->errhp);
            if (status != OCI_SUCCESS) username = (char *)0;
            ulen = (username) ? (int)buflen : 0;
            password = (char *)0;
            buflen = sizeof(password);
            status = OCIAttrGet(c->seshp, (ub4)OCI_HTYPE_SESSION,
                                (dvoid *)&password, &buflen,
                                (ub4)OCI_ATTR_PASSWORD, c->errhp);
            if (status != OCI_SUCCESS) password = (char *)0;
            plen = (password) ? (int)buflen : 0;

            /* If they match, just reset and reuse this session */
            if ((ulen > 0) && (plen > 0))
              if ((!str_compare(authuser, username, ulen, 0)) &&
                  (!str_compare(authpass, password, plen, 0)))
                if ((!authuser[ulen]) && (!authpass[plen]))
                  return(owa_reset(c, octx));

            username = authuser;
            password = authpass;
            ulen = str_length(authuser);
            plen = str_length(password);

            phase = "End prior OCI session";

            /* Otherwise close old session and create new session for user */
            status = OCISessionEnd(c->svchp, c->errhp, c->seshp,
                                   (ub4)OCI_DEFAULT);
            if (status != OCI_SUCCESS) goto connerr;
            ++phase;
            status = OCIAttrSet(c->seshp, (ub4)OCI_HTYPE_SESSION, username,
                                (ub4)ulen, (ub4)OCI_ATTR_USERNAME, c->errhp);
            if (status != OCI_SUCCESS) goto connerr;
            status = OCIAttrSet(c->seshp, (ub4)OCI_HTYPE_SESSION, password,
                                (ub4)plen, (ub4)OCI_ATTR_PASSWORD, c->errhp);
            if (status != OCI_SUCCESS) goto connerr;

            phase = "Begin OCI session";

            status = OCISessionBegin(c->svchp, c->errhp, c->seshp,
                                     (ub4)OCI_CRED_RDBMS, (ub4)OCI_DEFAULT);
            if (status == OCI_SUCCESS_WITH_INFO)
            {
                if (errinfo) *errinfo = sql_get_error(c);
                status = OCI_SUCCESS;
            }
            if (status != OCI_SUCCESS) goto connerr;

            phase = "Set server in service context";
            status = OCIAttrSet(c->svchp, (ub4)OCI_HTYPE_SVCCTX, c->seshp,
                                (ub4)0, (ub4)OCI_ATTR_SESSION, c->errhp);
            if (status != OCI_SUCCESS) goto connerr;

            goto setup_connection;
        }
        return(OCI_SUCCESS);
    }

    c->envhp = (OCIEnv *)0;
    c->errhp = (OCIError *)0;
    c->srvhp = (OCIServer *)0;
    c->svchp = (OCISvcCtx *)0;
    c->seshp = (OCISession *)0;
    c->stmhp1 = (OCIStmt *)0;
    c->stmhp2 = (OCIStmt *)0;
    c->stmhp3 = (OCIStmt *)0;
    c->stmhp4 = (OCIStmt *)0;
    c->stmhp5 = (OCIStmt *)0;
    c->pblob = (OCILobLocator *)0;
    c->pclob = (OCILobLocator *)0;
    c->pnlob = (OCILobLocator *)0;
    c->pbfile = (OCILobLocator *)0;
    c->rset = (OCIStmt *)0;
    c->session = (char *)0; /* ### Unclear if this line is safe ### */

    optr = connstr;
    if (!optr) optr = octx->oracle_userid;
    if (!optr) return(OCI_ERROR);
    sptr = buf;
    username = sptr;
    for (ulen = 0; *optr != '/'; ++ulen)
    {
        *(sptr++) = *(optr++);
        if ((ulen + 1) == sizeof(buf)) return(OCI_ERROR);
        if (*optr == '\0') return(OCI_ERROR);
    }
    *(sptr++) = '\0';
    password = sptr;
    if (*(++optr) == '\0') return(OCI_ERROR);
    sptr = optr;
    database = (char *)0;
    while (*sptr != '\0')
    {
        if (*sptr == '@') database = sptr;
        ++sptr;
    }
    if (database)
    {
        sptr = (database++);
        if (*database == '\0') database = (char *)0;
    }
    plen = (int)(sptr - optr);
    if ((plen + ulen + 2) >= sizeof(buf)) return(OCI_ERROR);
    mem_copy(password, optr, plen);
    password[plen] = '\0';
    dlen = (database) ? str_length(database) : 0;

    if (octx->authrealm)
    {
        if ((authuser) && (authpass) && (!connstr))
        {
            username = authuser;
            password = authpass;
            ulen = str_length(username);
            plen = str_length(password);
        }
        /* Else special call bypassing the authuser/authpass login */
    }

    /*
    ** Initialize OCI handles
    */
    status = OCIEnvCreate(&(c->envhp), (ub4)OCI_THREADED, (dvoid *)0,
                          (dvoid * (*)(dvoid *, size_t))0,
                          (dvoid * (*)(dvoid *, dvoid *, size_t))0,
                          (dvoid (*)(dvoid *, dvoid *))0,
                          (size_t)0, (dvoid **)0);
    if (status != OCI_SUCCESS) goto handerr;

    phase = "Allocate basic handles",

    status = OCIHandleAlloc(c->envhp, (dvoid **)&(c->errhp),
                            (ub4)OCI_HTYPE_ERROR, (size_t)0, (dvoid **)0);
    if (status != OCI_SUCCESS) goto handerr;
    status = OCIHandleAlloc(c->envhp, (dvoid **)&(c->svchp),
                            (ub4)OCI_HTYPE_SVCCTX, (size_t)0, (dvoid **)0);
    if (status != OCI_SUCCESS) goto handerr;
    status = OCIHandleAlloc(c->envhp, (dvoid **)&(c->srvhp),
                            (ub4)OCI_HTYPE_SERVER, (size_t)0, (dvoid **)0);
    if (status != OCI_SUCCESS) goto handerr;

    phase = "Attach to specified server";
    /*
    ** Attach to specified database server
    */
    status = OCIServerAttach(c->srvhp, c->errhp, (text *)database, (sb4)dlen,
                             (ub4)OCI_DEFAULT);
    if (status != OCI_SUCCESS) goto connerr;

    phase = "Set server in service context";
    /* Set the server for use with the service context */
    status = OCIAttrSet(c->svchp, (ub4)OCI_HTYPE_SVCCTX, c->srvhp, (ub4)0,
                        (ub4)OCI_ATTR_SERVER, c->errhp);
    if (status != OCI_SUCCESS) goto connerr;

    /*
    ** Log on and create a new session
    */
    status = OCIHandleAlloc(c->envhp, (dvoid **)&(c->seshp),
                            (ub4)OCI_HTYPE_SESSION, (size_t)0, (dvoid **)0);
    if (status != OCI_SUCCESS) goto handerr;
    /*
    ** If a username or password is given, use RDBMS authentication
    */
    if ((ulen > 0) || (plen > 0))
    {
      phase = "Set username and password";
      status = OCIAttrSet(c->seshp, (ub4)OCI_HTYPE_SESSION, username, (ub4)ulen,
                          (ub4)OCI_ATTR_USERNAME, c->errhp);
      if (status != OCI_SUCCESS) goto connerr;
      status = OCIAttrSet(c->seshp, (ub4)OCI_HTYPE_SESSION, password, (ub4)plen,
                          (ub4)OCI_ATTR_PASSWORD, c->errhp);
      if (status != OCI_SUCCESS) goto connerr;
      phase = "Begin OCI session";
      status = OCISessionBegin(c->svchp, c->errhp, c->seshp,
                               (ub4)OCI_CRED_RDBMS, (ub4)OCI_DEFAULT);
    }
    /*
    ** Otherwise rely on external authentication (e.g. Oracle wallet)
    */
    else
    {
      phase = "Begin OCI session";
      status = OCISessionBegin(c->svchp, c->errhp, c->seshp,
                               (ub4)OCI_CRED_EXT, (ub4)OCI_DEFAULT);
      /* Remember external authentication failure */
      if ((status != OCI_SUCCESS) && (status != OCI_SUCCESS_WITH_INFO))
        extauth = 1;
    }
    if (status == OCI_SUCCESS_WITH_INFO)
    {
        if (errinfo) *errinfo = sql_get_error(c);
        status = OCI_SUCCESS;
    }
    if (status != OCI_SUCCESS) goto connerr;

    phase = "Set session in service context";
    status = OCIAttrSet(c->svchp, (ub4)OCI_HTYPE_SVCCTX, c->seshp, (ub4)0,
                        (ub4)OCI_ATTR_SESSION, c->errhp);
    if (status != OCI_SUCCESS) goto connerr;

    phase = "Create cursor handles";
    /*
    ** Create reusable cursor handles
    */
    status = OCIHandleAlloc(c->envhp, (dvoid **)&(c->stmhp1),
                            (ub4)OCI_HTYPE_STMT, (size_t)0, (dvoid **)0);
    if (status != OCI_SUCCESS) goto handerr;
    status = OCIHandleAlloc(c->envhp, (dvoid **)&(c->stmhp2),
                            (ub4)OCI_HTYPE_STMT, (size_t)0, (dvoid **)0);
    if (status != OCI_SUCCESS) goto handerr;
    status = OCIHandleAlloc(c->envhp, (dvoid **)&(c->stmhp3),
                            (ub4)OCI_HTYPE_STMT, (size_t)0, (dvoid **)0);
    if (status != OCI_SUCCESS) goto handerr;
    status = OCIHandleAlloc(c->envhp, (dvoid **)&(c->stmhp4),
                            (ub4)OCI_HTYPE_STMT, (size_t)0, (dvoid **)0);
    if (status != OCI_SUCCESS) goto handerr;
    status = OCIHandleAlloc(c->envhp, (dvoid **)&(c->stmhp5),
                            (ub4)OCI_HTYPE_STMT, (size_t)0, (dvoid **)0);
    if (status != OCI_SUCCESS) goto handerr;
#ifdef NEVER
    status = OCIHandleAlloc(c->envhp, (dvoid **)&(c->rset),
                            (ub4)OCI_HTYPE_STMT, (size_t)0, (dvoid **)0);
    if (status != OCI_SUCCESS) goto handerr;
#endif

    /*
    ** Create reusable BLOB and CLOB descriptors
    */
    status = OCIDescriptorAlloc(c->envhp, (dvoid **)(dvoid *)&(c->pblob),
                                (ub4)OCI_DTYPE_LOB, (size_t)0, (dvoid **)0);
    if (status != OCI_SUCCESS) goto handerr;
    status = OCIDescriptorAlloc(c->envhp, (dvoid **)(dvoid *)&(c->pclob),
                                (ub4)OCI_DTYPE_LOB, (size_t)0, (dvoid **)0);
    if (status != OCI_SUCCESS) goto handerr;
    status = OCIDescriptorAlloc(c->envhp, (dvoid **)(dvoid *)&(c->pnlob),
                                (ub4)OCI_DTYPE_LOB, (size_t)0, (dvoid **)0);
    if (status != OCI_SUCCESS) goto handerr;
    status = OCIDescriptorAlloc(c->envhp, (dvoid **)(dvoid *)&(c->pbfile),
                                (ub4)OCI_DTYPE_FILE, (size_t)0, (dvoid **)0);
    if (status != OCI_SUCCESS) goto handerr;

    lobempty = (ub4)0;
    status = OCIAttrSet(c->pblob, (ub4)OCI_DTYPE_LOB, &lobempty, (ub4)0,
                        (ub4)OCI_ATTR_LOBEMPTY, c->errhp);
    if (status != OCI_SUCCESS) goto connerr;
    status = OCIAttrSet(c->pclob, (ub4)OCI_DTYPE_LOB, &lobempty, (ub4)0,
                        (ub4)OCI_ATTR_LOBEMPTY, c->errhp);
    if (status != OCI_SUCCESS) goto connerr;
    status = OCIAttrSet(c->pnlob, (ub4)OCI_DTYPE_LOB, &lobempty, (ub4)0,
                        (ub4)OCI_ATTR_LOBEMPTY, c->errhp);
    if (status != OCI_SUCCESS) goto connerr;

setup_connection:

    phase = "Alter session";
    /*
    ** If NLS_LANGUAGE and NLS_TERRITORY specified, do ALTER SESSION
    */
    c->lastsql = (char *)"ALTER SESSION";
    if (octx->nls_lang[0])
    {
        sptr = buf + str_concat(buf, 0,
                                (char *)"alter session set NLS_LANGUAGE='",-1);
        sptr += str_concat(sptr, 0, octx->nls_lang, -1);
        sptr += str_concat(sptr, 0, (char *)"'", -1);
        status = sql_parse(c, c->stmhp3, buf, (int)(sptr - buf));
        if (status == OCI_SUCCESS)
            status = sql_exec(c, c->stmhp3, (ub4)1, 0);
        status = OCI_SUCCESS;
    }
    if (octx->nls_terr[0])
    {
        sptr = buf + str_concat(buf, 0,
                                "alter session set NLS_TERRITORY='", -1);
        sptr += str_concat(sptr, 0, octx->nls_terr, -1);
        sptr += str_concat(sptr, 0, "'", -1);
        status = sql_parse(c, c->stmhp3, buf, (int)(sptr - buf));
        if (status == OCI_SUCCESS)
            status = sql_exec(c, c->stmhp3, (ub4)1, 0);
        status = OCI_SUCCESS;
    }
    /*
    ** If an optimizer mode is specified, set it up
    */
    if (octx->optimizer_mode)
    {
        sptr = buf + str_concat(buf, 0,
                                "alter session set OPTIMIZER_MODE=", -1);
        sptr += str_concat(sptr, 0, octx->optimizer_mode, -1);
        status = sql_parse(c, c->stmhp3, buf, (int)(sptr - buf));
        if (status == OCI_SUCCESS)
            status = sql_exec(c, c->stmhp3, (ub4)1, 0);
        status = OCI_SUCCESS;
    }
    /* ### Add user-specified init routine here ### */

    /*
    ** If TIME_ZONE is specified, do ALTER SESSION (for Oracle 9i)
    ** If date format specified, also do ALTER SESSION to set up formats.
    */
    if (octx->nls_tz[0])
    {
        sptr = buf + str_concat(buf, 0,
                                "alter session set TIME_ZONE='", -1);
        sptr += str_concat(sptr, 0, octx->nls_tz, -1);
        sptr += str_concat(sptr, 0, "'", -1);
        status = sql_parse(c, c->stmhp3, buf, (int)(sptr - buf));
        if (status == OCI_SUCCESS)
            status = sql_exec(c, c->stmhp3, (ub4)1, 0);
        status = OCI_SUCCESS;
    }
    if (octx->nls_datfmt[0])
    {
        sptr = buf + str_concat(buf, 0,
                                "alter session set NLS_DATE_FORMAT='", -1);
        optr = sptr;
        sptr += str_concat(sptr, 0, octx->nls_datfmt, -1);
        optr = str_char(optr, ' ', 0);
        if (optr) sptr = optr; /* Truncate time portion */
        sptr += str_concat(sptr, 0, "'", -1);
        status = sql_parse(c, c->stmhp3, buf, (int)(sptr - buf));
        if (status == OCI_SUCCESS)
            status = sql_exec(c, c->stmhp3, (ub4)1, 0);
        status = OCI_SUCCESS;
    }
    if (octx->nls_datfmt[0])
    {
        sptr = buf + str_concat(buf, 0,
                                "alter session set NLS_TIMESTAMP_FORMAT='",-1);
        optr = sptr;
        sptr += str_concat(sptr, 0, octx->nls_datfmt, -1);
        optr = str_char(optr, ' ', 0);
        if (optr) optr = str_char(++optr, ' ', 0);
        if (optr) sptr = optr; /* Truncate zone portion */
        sptr += str_concat(sptr, 0, "'", -1);
        status = sql_parse(c, c->stmhp3, buf, (int)(sptr - buf));
        if (status == OCI_SUCCESS)
            status = sql_exec(c, c->stmhp3, (ub4)1, 0);
        status = OCI_SUCCESS;
    }
    if (octx->nls_datfmt[0])
    {
        sptr = buf + str_concat(buf, 0,
                                "alter session set NLS_TIMESTAMP_TZ_FORMAT='",
                                -1);
        sptr += str_concat(sptr, 0, octx->nls_datfmt, -1);
        sptr += str_concat(sptr, 0, "'", -1);
        status = sql_parse(c, c->stmhp3, buf, (int)(sptr - buf));
        if (status == OCI_SUCCESS)
            status = sql_exec(c, c->stmhp3, (ub4)1, 0);
        status = OCI_SUCCESS;
    }

    if ((octx->dav_handler) && (octx->dav_mode > 1))
    {
        /*
        ** Prepare cursors for the five DAV request statements
        */
        c->lastsql = octx->cgi_stmt;
        status = sql_parse(c, c->stmhp1, c->lastsql, -1);
        if (status != OCI_SUCCESS) goto connerr;

        c->lastsql = octx->lob_stmt;
        status = sql_parse(c, c->stmhp2, c->lastsql, -1);
        if (status != OCI_SUCCESS) goto connerr;

        c->lastsql = octx->cpmv_stmt;
        status = sql_parse(c, c->stmhp3, c->lastsql, -1);
        if (status != OCI_SUCCESS) goto connerr;

        c->lastsql = octx->res_stmt;
        status = sql_parse(c, c->stmhp4, c->lastsql, -1);
        if (status != OCI_SUCCESS) goto connerr;

        c->lastsql = octx->prop_stmt;
        status = sql_parse(c, c->stmhp5, c->lastsql, -1);
        if (status != OCI_SUCCESS) goto connerr;
    }
    else
    {
        /*
        ** Prepare cursors for the four statements used on every request:
        */
        c->lastsql = octx->reset_stmt;
        status = sql_parse(c, c->stmhp5, c->lastsql, -1);
        if (status != OCI_SUCCESS) goto connerr;

        c->lastsql = octx->cgi_stmt;
        status = sql_parse(c, c->stmhp1, c->lastsql, -1);
        if (status != OCI_SUCCESS) goto connerr;

        c->lastsql = octx->sec_stmt;
        status = sql_parse(c, c->stmhp2, c->lastsql, -1);
        if (status != OCI_SUCCESS) goto connerr;

        c->lastsql = octx->get_stmt;
        status = sql_parse(c, c->stmhp4, c->lastsql, -1);
        if (status != OCI_SUCCESS) goto connerr;
    }

    /*
    ** Set alternate character set for strings (if necessary for DAD)
    */
    if ((octx->ora_csid) && (octx->dad_csid != octx->ora_csid))
        c->csid = nls_csid(octx->dad_csid);
    else if ((octx->ncflag & (~UNI_MODE_RAW)) && (octx->dad_csid))
        c->csid = nls_csid(octx->dad_csid);
    c->ncflag = (octx->ncflag & UNI_MODE_FULL);

    return(status);

connerr:
    if (status != OCI_SUCCESS)
    {
        oerrno = sql_get_error(c);
        if (oerrno) status = oerrno;

        /*
        ** These error codes commonly mean that the OCI can't access certain
        ** flat-files found in the ORACLE_HOME, generally due to file system
        ** permissions. This also often means a useless error message is
        ** returned because the message file can't be read. Special-case
        ** those codes and check file access.
        **   1804  - can't load the zoneinfo data files
        **   12705 - can't load the NLS character data files
        */
        if ((oerrno == 1804) || (oerrno == 12705))
          if (str_substr(c->errbuf,
                         "Error while trying to retrieve text for error",
                         1) == c->errbuf)
            goto accesserr;

        /*
        ** This error code means OCI failed to connect using external
        ** authentication. Instead of the unhelpful ORA-1017, report
        ** the OS username.
        */
        if ((oerrno == 1017) && (extauth))
        {
          int oulen = os_get_user(osusr, (int)sizeof(osusr));
          if (oulen > 0)
          {
            if (database)
              os_str_print(c->errbuf,
                           "User [%s] failed external login to db [%s]",
                           osusr, database);
            else
              os_str_print(c->errbuf,
                           "User [%s] failed to authenticate externally",
                           osusr);
          }
        }

        sql_dump_env(phase);
    }

    return(status);

handerr:
    if (status != OCI_SUCCESS)
    {
        if (!(c->envhp))
        {
            /*
            ** Attempt to produce a better error message for file access issues
            */
            char       *fname;
            char       *ohome;
            int         elen;
            int         readable;
            int         oserr;

accesserr:
            sql_dump_env(phase);

            ohome = os_env_get("ORACLE_HOME");
            if (!ohome) ohome = "";
            str_copy(c->errbuf, "OCI Error: Environment not created");
            elen = str_length(c->errbuf);
            if (*ohome == '\0')
            {
                str_copy(c->errbuf + elen, " - ORACLE_HOME not set");
            }
            else
            {
                char fpath[HTBUF_HEADER_MAX];
                int  oulen = os_get_user(osusr, (int)sizeof(osusr));
                if (oulen <= 0) *osusr = '\0';

                fname = "/nls/data/lx1boot.nlb";
                if (sql_check_access(ohome, fname, fpath))
                {
                    oserr = os_get_errno();
                    os_str_print(c->errbuf + elen,
                                 " - %s not readable (error %d) by %s",
                                 fpath, oserr, osusr);
                    goto badaccess;
                }
                fname = "/oracore/zoneinfo/timezlrg.dat";
                readable = sql_check_access(ohome, fname, fpath);
                if (!readable)
                {
                    fname = "/oracore/zoneinfo/timezlrg_1.dat";
                    readable = sql_check_access(ohome, fname, fpath);
                }
                if (!readable)
                {
                    oserr = os_get_errno();
                    os_str_print(c->errbuf + elen,
                                 " - %s not readable (error %d) by %s",
                                 fpath, oserr, osusr);
                    goto badaccess;
                }
                fname = "/rdbms/mesg/oraus.msb";
                if (sql_check_access(ohome, fname, fpath))
                {
                    oserr = os_get_errno();
                    os_str_print(c->errbuf + elen,
                                 " - %s not readable (error %d) by %s",
                                 fpath, oserr, osusr);
                    goto badaccess;
                }
            }
        }
        else
        {
            OCIErrorGet((dvoid *)(c->envhp), (ub4)1, (text *)0, &oerrno,
                        (text *)(c->errbuf),
                        (ub4)OCI_ERROR_MAXMSG_SIZE, (ub4)OCI_HTYPE_ENV);
            if (oerrno) status = oerrno;
        }
    }
badaccess:
    return(status);
}

/*
** Close OCI connection
*/
sword sql_disconnect(connection *c)
{
    sword status;
    sb4   oerrno = 0;

    c->c_lock = C_LOCK_NEW;
    c->mem_err = 0;

    if (c->session)
    {
        mem_free((void *)(c->session));
        c->session = (char *)0;
    }

    status = OCISessionEnd(c->svchp, c->errhp, c->seshp, (ub4)OCI_DEFAULT);
    if (status != OCI_SUCCESS) goto closeerr;
    status = OCIServerDetach(c->srvhp, c->errhp, (ub4)OCI_DEFAULT);
    if (status != OCI_SUCCESS) goto closeerr;
    if (c->stmhp1)
        status = OCIHandleFree((dvoid *)(c->stmhp1), (ub4)OCI_HTYPE_STMT);
    if (status != OCI_SUCCESS) goto closehand;
    if (c->stmhp2)
        status = OCIHandleFree((dvoid *)(c->stmhp2), (ub4)OCI_HTYPE_STMT);
    if (status != OCI_SUCCESS) goto closehand;
    if (c->stmhp3)
        status = OCIHandleFree((dvoid *)(c->stmhp3), (ub4)OCI_HTYPE_STMT);
    if (status != OCI_SUCCESS) goto closehand;
    if (c->stmhp4)
        status = OCIHandleFree((dvoid *)(c->stmhp4), (ub4)OCI_HTYPE_STMT);
    if (status != OCI_SUCCESS) goto closehand;
    if (c->stmhp5)
        status = OCIHandleFree((dvoid *)(c->stmhp5), (ub4)OCI_HTYPE_STMT);
    if (status != OCI_SUCCESS) goto closehand;
    if (c->rset)
        status = OCIHandleFree((dvoid *)(c->rset), (ub4)OCI_HTYPE_STMT);
    if (status != OCI_SUCCESS) goto closehand;
    if (c->pblob)
        status = OCIDescriptorFree((dvoid *)(c->pblob), (ub4)OCI_DTYPE_LOB);
    if (status != OCI_SUCCESS) goto closehand;
    if (c->pclob)
        status = OCIDescriptorFree((dvoid *)(c->pclob), (ub4)OCI_DTYPE_LOB);
    if (status != OCI_SUCCESS) goto closehand;
    if (c->pnlob)
        status = OCIDescriptorFree((dvoid *)(c->pnlob), (ub4)OCI_DTYPE_LOB);
    if (status != OCI_SUCCESS) goto closehand;
    if (c->pbfile)
        status = OCIDescriptorFree((dvoid *)(c->pbfile), (ub4)OCI_DTYPE_FILE);
    if (status != OCI_SUCCESS) goto closehand;
    status = OCIHandleFree((dvoid *)(c->seshp), (ub4)OCI_HTYPE_SESSION);
    if (status != OCI_SUCCESS) goto closehand;
    status = OCIHandleFree((dvoid *)(c->srvhp), (ub4)OCI_HTYPE_SERVER);
    if (status != OCI_SUCCESS) goto closehand;
    status = OCIHandleFree((dvoid *)(c->svchp), (ub4)OCI_HTYPE_SVCCTX);
    if (status != OCI_SUCCESS) goto closehand;
    status = OCIHandleFree((dvoid *)(c->errhp), (ub4)OCI_HTYPE_ERROR);
    if (status != OCI_SUCCESS) goto closehand;
    status = OCIHandleFree((dvoid *)(c->envhp), (ub4)OCI_HTYPE_ENV);

    return(status);

closeerr:
    if ((status != OCI_SUCCESS) && (c->errbuf))
    {
        oerrno = sql_get_error(c);
        if (oerrno) status = oerrno;
    }
    return(status);

closehand:
    if (status != OCI_SUCCESS)
    {
        OCIErrorGet((dvoid *)(c->envhp), (ub4)1, (text *)0, &oerrno,
                    (text *)(c->errbuf),
                    (ub4)OCI_ERROR_MAXMSG_SIZE, (ub4)OCI_HTYPE_ENV);
        if (oerrno) status = oerrno;
    }
    return(status);
}

/*
** Define fetch column
*/
sword sql_define(connection *c, OraCursor stmhp, ub4 pos,
                 dvoid *buf, sb4 buflen, ub2 dtype, dvoid *inds)
{
    sword      status;
    OCIDefine *dhand;

    /* If no indicators provided, use the default (1-row) indicator */
    if (!inds) inds = (dvoid *)&(c->out_ind);

    status = OCIDefineByPos(stmhp, &dhand, c->errhp, pos,
                            buf, buflen, dtype,
                            (buf ? (dvoid *)inds : (dvoid *)0),
                            (ub2 *)0, (ub2 *)0,
                            (ub4)(buf ? OCI_DEFAULT : OCI_DYNAMIC_FETCH));

    if (((dtype == SQLT_STR) || (dtype == SQLT_LNG)) &&
        (status == OCI_SUCCESS) && (c->csid))
        status = OCIAttrSet(dhand, (ub4)OCI_HTYPE_DEFINE, &(c->csid), (ub4)0,
                            (ub4)OCI_ATTR_CHARSET_ID, c->errhp);

    return(status);
}

/*
** Set character set ID and form-of-use flag
*/
static sword set_cs_info(connection *c, OCIBind *bhand)
{
    sword status;
    ub1   cs_form;

    status = OCI_SUCCESS;

    if (c->csid)
    {
        status = OCIAttrSet(bhand, (ub4)OCI_HTYPE_BIND, &(c->csid), (ub4)0,
                            (ub4)OCI_ATTR_CHARSET_ID, c->errhp);
        if (status != OCI_SUCCESS) return(status);
    }

    if (c->ncflag & (~UNI_MODE_RAW))
    {
        cs_form = (ub1)SQLCS_NCHAR;
        status = OCIAttrSet(bhand, (ub4)OCI_HTYPE_BIND, &cs_form, (ub4)0,
                            (ub4)OCI_ATTR_CHARSET_FORM, c->errhp);
    }

    return(status);
}

/*
** Bind a signed 4-byte integer to an OCI statement
*/
sword sql_bind_int(connection *c, OraCursor stmhp, ub4 pos, sb4 *val)
{
    sword    status;
    OCIBind *bhand;

    status = OCIBindByPos(stmhp, &bhand, c->errhp, pos,
                          (dvoid *)val, (sb4)sizeof(*val),
                          (ub2)SQLT_INT, (dvoid *)0,
                          (ub2 *)0, (ub2 *)0, 0, (ub4 *)0, (ub4)OCI_DEFAULT);

    return(status);
}

/*
** Bind a signed 8-byte integer to an OCI statement
*/
sword sql_bind_long(connection *c, OraCursor stmhp, ub4 pos, long_64 *val)
{
    sword    status;
    OCIBind *bhand;

    status = OCIBindByPos(stmhp, &bhand, c->errhp, pos,
                          (dvoid *)val, (sb4)sizeof(*val),
                          (ub2)SQLT_INT, (dvoid *)0,
                          (ub2 *)0, (ub2 *)0, 0, (ub4 *)0, (ub4)OCI_DEFAULT);

    return(status);
}

/*
** Bind a null-terminated string buffer to an OCI statement
*/
sword sql_bind_str(connection *c, OraCursor stmhp, ub4 pos,
                   char *str, sb4 slen)
{
    sword    status;
    OCIBind *bhand;

    /* ### ROUND slen? ### */
    if (slen < 0) slen = str_length(str) + 1;

    c->out_ind = 0;
    status = OCIBindByPos(stmhp, &bhand, c->errhp, pos,
                          (dvoid *)str, slen,
                          (ub2)SQLT_STR, (dvoid *)&(c->out_ind),
                          (ub2 *)0, (ub2 *)0, 0, (ub4 *)0, (ub4)OCI_DEFAULT);

    if (status == OCI_SUCCESS) status = set_cs_info(c, bhand);

    return(status);
}

/*
** Bind a character array to an OCI statement (not null terminated)
*/
sword sql_bind_chr(connection *c, OraCursor stmhp, ub4 pos,
                   char *str, sb4 slen)
{
    sword    status;
    OCIBind *bhand;

    /* ### ROUND slen? ### */
    if (slen < 0) slen = str_length(str);

    c->out_ind = 0;
    status = OCIBindByPos(stmhp, &bhand, c->errhp, pos,
                          (dvoid *)str, slen,
                          (ub2)SQLT_CHR, (dvoid *)&(c->out_ind),
                          (ub2 *)0, (ub2 *)0, 0, (ub4 *)0, (ub4)OCI_DEFAULT);

    if (status == OCI_SUCCESS) status = set_cs_info(c, bhand);

    return(status);
}

/*
** Bind a RAW buffer and length variable to an OCI statement
*/
sword sql_bind_raw(connection *c, OraCursor stmhp, ub4 pos,
                   char *rawbuf, ub2 *rlen, sb4 buflen)
{
    sword    status;
    OCIBind *bhand;

    c->out_ind = 0;

    /* ### COULD USE SQLT_LBI BELOW ### */
    status = OCIBindByPos(stmhp, &bhand, c->errhp, pos,
                          (dvoid *)rawbuf, buflen,
                          (ub2)SQLT_BIN, (dvoid *)&(c->out_ind),
                          rlen, (ub2 *)0, 0, (ub4 *)0, (ub4)OCI_DEFAULT);

    return(status);
}

/*
** Bind an array of null-terminated strings to an OCI statement
*/
sword sql_bind_arr(connection *c, OraCursor stmhp, ub4 pos,
                   char *aptr, ub2 *lens, sb4 awidth, sb2 *inds,
                   ub4 *asize, ub4 amax)
{
    sword    status;
    OCIBind *bhand;

    status = OCIBindByPos(stmhp, &bhand, c->errhp, pos,
                          (dvoid *)aptr, (sb4)awidth, (ub2)SQLT_STR,
                          (dvoid *)inds, lens, (ub2 *)0,
                          amax, asize, (ub4)OCI_DEFAULT);

    if (status == OCI_SUCCESS) status = set_cs_info(c, bhand);

    return(status);
}

/*
** Bind an array of raws to an OCI statement
*/
sword sql_bind_rarr(connection *c, OraCursor stmhp, ub4 pos,
                    char *aptr, ub2 *lens, sb4 awidth, sb2 *inds,
                    ub4 *asize, ub4 amax)
{
    sword    status;
    OCIBind *bhand;

    status = OCIBindByPos(stmhp, &bhand, c->errhp, pos,
                          (dvoid *)aptr, (sb4)awidth, (ub2)SQLT_BIN,
                          (dvoid *)inds, lens, (ub2 *)0,
                          amax, asize, (ub4)OCI_DEFAULT);

    return(status);
}

/*
** Bind an array of integers to an OCI statement
*/
sword sql_bind_iarr(connection *c, OraCursor stmhp, ub4 pos,
                    sb4 *ints, ub4 *asize, ub4 amax)
{
    sword    status;
    OCIBind *bhand;

    status = OCIBindByPos(stmhp, &bhand, c->errhp, pos,
                          (dvoid *)ints, (sb4)sizeof(*ints), (ub2)SQLT_INT,
                          (dvoid *)0, (ub2 *)0, (ub2 *)0,
                          amax, asize, (ub4)OCI_DEFAULT);

    return(status);
}

/*
** Bind an array of length-leading string structures to an OCI statement
*/
sword sql_bind_vcs(connection *c, OraCursor stmhp, ub4 pos,
                   dvoid *buf, ub2 *lens, sb4 awidth, sb2 *inds,
                   ub4 *asize, ub4 amax, ub4 swidth)
{
    sword    status;
    int      dtype = (c->ncflag & UNI_MODE_RAW) ? SQLT_VBI : SQLT_VCS;
    OCIBind *bhand;

    status = OCIBindByPos(stmhp, &bhand, c->errhp, pos,
                          buf, (sb4)awidth, (ub2)dtype,
                          (dvoid *)inds, lens, (ub2 *)0,
                          amax, asize, (ub4)OCI_DEFAULT);

    if ((status == OCI_SUCCESS) && (dtype == SQLT_VCS))
        status = set_cs_info(c, bhand);

    if ((status == OCI_SUCCESS) && ((sb4)swidth > awidth) && (amax > (ub4)1))
        status = OCIBindArrayOfStruct(bhand, c->errhp, swidth,
                                      (ub4)sizeof(*inds), (ub4)sizeof(*lens),
                                      (ub4)0);

    return(status);
}

/*
** Bind for piecewise fetch or bind with callback
*/
sword sql_bind_stream(connection *c, OraCursor stmhp, ub4 pos, ub2 sql_type)
{
    sword    status;
    OCIBind *bhand;

    status = OCIBindByPos(stmhp, &bhand, c->errhp, pos,
                          (dvoid *)0, (sb4)LONG_MAXSZ, sql_type,
                          (dvoid *)0, (ub2 *)0, (ub2 *)0, (ub4)0, (ub4 *)0,
                          (ub4)OCI_DATA_AT_EXEC);

    return(status);
}

/*
** Dynamic data binding for arrays of string pointers:
**   sql_get_input()  - callback for output values
**   sql_put_output() - callback for return values (not implemented)
*/

static sb4 sql_get_input(dvoid *ctx, OCIBind *bindp, ub4 iter, ub4 index,
                         dvoid **bufpp, ub4 *alenp, ub1 *piecep, dvoid **indpp)
{
    char **arr = (char **)ctx;
    ub4    alen;

    alen = (arr[index]) ? str_length(arr[index]) : 0;
    *piecep = OCI_ONE_PIECE;
    *bufpp = (alen == 0) ? (dvoid *)"\0\0" : (dvoid *)arr[index];
    *indpp = (dvoid *)0; /* No indicator */
    *alenp = alen; /* (alen + 1) required for null-terminated strings */
    return(OCI_CONTINUE);
}

static sb4 sql_put_output(dvoid *ctx, OCIBind *bindp, ub4 iter, ub4 index,
                          dvoid **bufpp, ub4 **alenpp,
                          ub1 *piecep, dvoid **indpp, ub2 **rcodepp)
{
    /* ### NOT IMPLEMENTED ### */
    return(OCI_CONTINUE);
}

/*
** Bind array of string pointers
*/
sword sql_bind_ptrs(connection *c, OraCursor stmhp, ub4 pos,
                    char **aptr, sb4 awidth, ub4 *asize, ub4 amax)
{
    sword    status;
    sb2      sqltype;
    OCIBind *bhand;

    /* Bind strings as CHR to avoid need to pass the null terminator */
    sqltype = (c->ncflag & UNI_MODE_RAW) ? SQLT_BIN : SQLT_CHR;

    status = OCIBindByPos(stmhp, &bhand, c->errhp, pos,
                          (dvoid *)0, awidth, sqltype,
                          (dvoid *)0, (ub2 *)0, (ub2 *)0,
                          amax, asize, (ub4)OCI_DATA_AT_EXEC);

    if (!(c->ncflag & UNI_MODE_RAW))
      if (status == OCI_SUCCESS)
        status = set_cs_info(c, bhand);

#ifdef NEVER
    if (status == OCI_SUCCESS)
        status = OCIBindArrayOfStruct(bhand, c->errhp,
                                      (ub4)0, (ub4)0, (ub4)0, (ub4)0);
#endif

    if (status == OCI_SUCCESS)
        status = OCIBindDynamic(bhand, c->errhp,
                                (dvoid *)aptr, sql_get_input,
                                (dvoid *)aptr, sql_put_output);

    return(status);
}

/*
** Bind a LOB locator to an OCI statement
*/
sword sql_bind_lob(connection *c, OraCursor stmhp, ub4 pos, ub2 flag)
{
    sword           status;
    ub2             cs_id;
    ub2            *indp;
    ub1             cs_form;
    OCIBind        *bhand;
    OCILobLocator **pl;

    if (flag == SQLT_BLOB)
    {
        pl = &(c->pblob);
        indp = &(c->blob_ind);
        c->blob_ind = 0;
        cs_form = (ub1)SQLCS_IMPLICIT;
        cs_id = 0;
    }
    else if (flag == SQLT_CLOB)
    {
        pl = &(c->pclob);
        indp = &(c->clob_ind);
        c->clob_ind = 0;
        cs_form = (ub1)SQLCS_IMPLICIT;
        cs_id = c->csid;
    }
    else if (flag == SQLT_BFILE)
    {
        pl = &(c->pbfile);
        indp = &(c->bfile_ind);
        c->bfile_ind = 0;
        cs_form = (ub1)SQLCS_IMPLICIT;
        cs_id = 0;
    }
    else
    {
        pl = &(c->pnlob);
        indp = &(c->nlob_ind);
        c->nlob_ind = 0;
        flag = SQLT_CLOB;
        cs_form = (ub1)SQLCS_NCHAR;
        cs_id = c->csid;
    }

    status = OCIBindByPos(stmhp, &bhand, c->errhp, pos, (dvoid *)pl,
                          (sb4)sizeof(*pl), flag, indp, (ub2 *)0, (ub2 *)0,
                          (ub4)0, (ub4 *)0, (ub4)OCI_DEFAULT);

    if ((status == OCI_SUCCESS) && (cs_form != (ub1)SQLCS_IMPLICIT))
        status = OCIAttrSet(bhand, (ub4)OCI_HTYPE_BIND, &cs_form, (ub4)0,
                            (ub4)OCI_ATTR_CHARSET_FORM, c->errhp);

    if ((status == OCI_SUCCESS) && (cs_id > 0))
    {
#ifdef NEVER
        /* ### cs_id NOT USED FOR LOB BINDINGS -- ELIMINATE THIS CODE? ### */
        status = OCIAttrSet(bhand, (ub4)OCI_HTYPE_BIND, &cs_id, (ub4)0,
                            (ub4)OCI_ATTR_CHARSET_ID, c->errhp);
#endif
    }

    return(status);
}

/*
** Define a LOB locator for a SELECT statement column
*/
sword sql_define_lob(connection *c, OraCursor stmhp, ub4 pos, ub2 flag)
{
    sword           status;
    ub2             cs_id;
    ub2            *indp;
    ub1             cs_form;
    OCIDefine      *dhand;
    OCILobLocator **pl;

    if (flag == SQLT_BLOB)
    {
        pl = &(c->pblob);
        indp = &(c->blob_ind);
        c->blob_ind = 0;
        cs_form = (ub1)SQLCS_IMPLICIT;
        cs_id = 0;
    }
    else if (flag == SQLT_CLOB)
    {
        pl = &(c->pclob);
        indp = &(c->clob_ind);
        c->clob_ind = 0;
        cs_form = (ub1)SQLCS_IMPLICIT;
        cs_id = c->csid;
    }
    else if (flag == SQLT_BFILE)
    {
        pl = &(c->pbfile);
        indp = &(c->bfile_ind);
        c->bfile_ind = 0;
        cs_form = (ub1)SQLCS_IMPLICIT;
        cs_id = 0;
    }
    else
    {
        pl = &(c->pnlob);
        indp = &(c->nlob_ind);
        c->nlob_ind = 0;
        flag = SQLT_CLOB;
        cs_form = (ub1)SQLCS_NCHAR;
        cs_id = c->csid;
    }

    status = OCIDefineByPos(stmhp, &dhand, c->errhp, pos, (dvoid *)pl,
                            (sb4)sizeof(*pl), flag, indp, (ub2 *)0, (ub2 *)0,
                            (ub4)OCI_DEFAULT);

    if ((status == OCI_SUCCESS) && (cs_form != (ub1)SQLCS_IMPLICIT))
        status = OCIAttrSet(dhand, (ub4)OCI_HTYPE_BIND, &cs_form, (ub4)0,
                            (ub4)OCI_ATTR_CHARSET_FORM, c->errhp);

    if ((status == OCI_SUCCESS) && (cs_id > 0))
    {
#ifdef NEVER
        /* ### cs_id NOT USED FOR LOB BINDINGS -- ELIMINATE THIS CODE? ### */
        status = OCIAttrSet(bhand, (ub4)OCI_HTYPE_BIND, &cs_id, (ub4)0,
                            (ub4)OCI_ATTR_CHARSET_ID, c->errhp);
#endif
    }

    return(status);
}

/*
** Get cumulative count of rows
*/
sword sql_get_rowcount(connection *c, OraCursor stmhp, ub4 *rowcount)
{
    return(OCIAttrGet(stmhp, (ub4)OCI_HTYPE_STMT,
                      (dvoid *)rowcount, (ub4 *)0,
                      (ub4)OCI_ATTR_ROW_COUNT, c->errhp));
}

/*
** Get the statement state
*/
sword sql_get_stmt_state(connection *c, OraCursor stmhp, int *fstatus)
{
    ub4 stmt_state;

    sword status = OCIAttrGet(stmhp, (ub4)OCI_HTYPE_STMT,
                              (dvoid *)&stmt_state, (ub4 *)0,
                              (ub4)OCI_ATTR_STMT_STATE, c->errhp);
    if (status == OCI_SUCCESS)
    {
        switch (stmt_state)
        {
        case OCI_STMT_STATE_INITIALIZED:  *fstatus =  0; break;
        case OCI_STMT_STATE_EXECUTED:     *fstatus =  1; break;
        case OCI_STMT_STATE_END_OF_FETCH: *fstatus =  2; break;
        default:                          *fstatus = -1; break;
        }
    }

    return(status);
}

/*
** Bind a REF cursor (statement return)
*/
sword sql_bind_cursor(connection *c, OraCursor stmhp, ub4 pos)
{
    sword    status = OCI_SUCCESS;
    OCIBind *bhand;

    /* Create the ref cursor on demand */
    if (!(c->rset))
      status = OCIHandleAlloc(c->envhp, (dvoid **)&(c->rset),
                              (ub4)OCI_HTYPE_STMT, (size_t)0, (dvoid **)0);

    if (status == OCI_SUCCESS)
      status = OCIBindByPos(stmhp, &bhand, c->errhp, pos,
                            (dvoid *)&(c->rset), (sb4)0, (ub2)SQLT_RSET,
                            (dvoid *)0, (ub2 *)0, (ub2 *)0,
                            (ub4)0, (ub4)0, (ub4)OCI_DEFAULT);
    return(status);
}

/*
** Describe a column from a cursor
*/
sword sql_describe_col(connection *c, OraCursor stmhp, ub4 pos,
                       int *width, char *colname, int cs_expand)
{
    sword     status;
    OCIParam *mypard = (OCIParam *)0;
    ub2       dtype;
    ub2       dsize;
    ub4       dlen;
    oratext  *dname;

    status = OCIParamGet((void *)stmhp, (ub4)OCI_HTYPE_STMT, c->errhp,
                         (void **)&mypard, pos);
    if (status == OCI_SUCCESS)
      status = OCIAttrGet((void *)mypard, (ub4)OCI_DTYPE_PARAM,
                          (void *)&dtype, (ub4 *)0, (ub4)OCI_ATTR_DATA_TYPE,
                          c->errhp);
    if (status == OCI_SUCCESS)
      status = OCIAttrGet((void *)mypard, (ub4)OCI_DTYPE_PARAM,
                          (void *)&dname, (ub4 *)&dlen, (ub4)OCI_ATTR_NAME,
                          c->errhp);
    if (status == OCI_SUCCESS)
      status = OCIAttrGet((void *)mypard, (ub4)OCI_DTYPE_PARAM,
                          (void *)&dsize, (ub4 *)0, (ub4)OCI_ATTR_DATA_SIZE,
                          c->errhp);

    if (status != OCI_SUCCESS)
    {
      *colname = '\0';
      *width = -1; /* Signals error */
    }
    else
    {
      /* colname buffer must be at least 128*3+1 bytes long */
      if (dlen > (SQL_NAME_MAX * 3)) dlen = SQL_NAME_MAX * 3;
      mem_copy(colname, dname, dlen);
      colname[dlen] = '\0';

      /* Width adjustment based on datatype */
      switch (dtype)
      {
      case OCI_TYPECODE_RAW:
        /* Allow for conversion of RAW to hex */
        *width = (int)dsize * 2 + 1;
        break;
      case OCI_TYPECODE_VARCHAR2:
      case OCI_TYPECODE_VARCHAR:
      case OCI_TYPECODE_CHAR:
        /* Increase width for character-set expansion? */
        if (cs_expand < 1) cs_expand = 1;
        /* ### Iffy since columns may be 32k in future ### */
        if (dsize > (ub2)HTBUF_HEADER_MAX) dsize = (ub2)HTBUF_HEADER_MAX;
        *width = (int)dsize * cs_expand + 1;
        break;
      case OCI_TYPECODE_CLOB:
      case OCI_TYPECODE_BLOB:
        *width = 0; /* Signals LOB */
        break;
      default:
        /* Impose minimum width for number/date/timestamp/rowid columns */
        if (dsize < (ub2)60) dsize = (ub2)60;
        *width = (int)dsize;
      }
    }

    return(status);
}

/*
** Set next piece to be written to database
*/
sword sql_write_piece(connection *c, OraCursor stmhp,
                      dvoid *piecebuf, ub4 *nbytes, int pieceflag)
{
    sword    status;
    ub1      piece;
    ub4      htype = OCI_HTYPE_BIND;
    ub1      in_out = OCI_PARAM_IN;
    ub4      iter;
    ub4      idx;
    OCIBind *bhand;

    status = OCIStmtGetPieceInfo(c->stmhp3, c->errhp, (dvoid **)&bhand,
                                 &htype, &in_out, &iter, &idx, &piece);
    if (status != OCI_SUCCESS) return(status);

    piece = pieceflag;

    c->out_ind = (ub2)0;
    c->rcode = (ub2)0;
    status = OCIStmtSetPieceInfo(bhand, htype, c->errhp,
                                 piecebuf, nbytes, piece,
                                 &(c->out_ind), &(c->rcode));

    return(status);
}

/*
** Set next piece to be read from database
*/
sword sql_read_piece(connection *c, OraCursor stmhp,
                     dvoid *piecebuf, ub4 *nbytes)
{
    sword      status;
    ub1        piece;
    ub4        iter;
    ub4        idx;
    ub4        htype = OCI_HTYPE_DEFINE;
    ub1        in_out = OCI_PARAM_OUT;
    OCIDefine *dhand;

    status = OCIStmtGetPieceInfo(c->stmhp3, c->errhp, (dvoid **)&dhand,
                                 &htype, &in_out, &iter, &idx, &piece);

    if (status == OCI_SUCCESS)
    {
        c->out_ind = (ub2)0;
        c->rcode = (ub2)0;
        status = OCIStmtSetPieceInfo(dhand, htype, c->errhp,
                                     piecebuf, nbytes, piece,
                                     &(c->out_ind), &(c->rcode));
    }
    return(status);
}

/*
** Compare the call to PL/SQL arguments by name and type,
** attempt to adjust array-bound parameters.  Returns TRUE
** if any array-bound parameter is promoted by the process.
*/
static int check_normal(connection *c, dvoid *arg_list,
                        int nargs, char *names[], ub4 counts[])
{
    sword  status;
    ub4    pos;
    ub4    acount;
    int    argsfound = 0;
    int    i;
    char  *namep;
    ub4    namsz;
    dvoid *argp;
    ub2    dtype;
    int    result = 0;

    status = OCIAttrGet(arg_list, (ub4)OCI_DTYPE_PARAM, (dvoid *)&acount,
                        (ub4 *)0, (ub4)OCI_ATTR_NUM_PARAMS, c->errhp);
    if (status != OCI_SUCCESS) return(result);

    for (pos = 1; pos <= acount; ++pos)
    {
        status = OCIParamGet(arg_list, (ub4)OCI_DTYPE_PARAM,
                             c->errhp, (dvoid **)&argp, (ub4)pos);
        if (status != OCI_SUCCESS) break;

        status = OCIAttrGet(argp, (ub4)OCI_DTYPE_PARAM,
                            (dvoid *)&namep, (ub4 *)&namsz,
                            (ub4)OCI_ATTR_NAME, c->errhp);
        if (status != OCI_SUCCESS) break;

        status = OCIAttrGet(argp, (ub4)OCI_DTYPE_PARAM,
                            (dvoid *)&dtype, (ub4 *)0,
                            (ub4)OCI_ATTR_DATA_TYPE, c->errhp);
        if (status != OCI_SUCCESS) break;

        /*
        ** Look for a bound argument with a matching name that's bound
        ** as a scalar and promote it to an array bind type.
        */
        for (i = 0; i < nargs; ++i)
        {
            /* If the names match */
            if ((!str_compare(names[i], namep, namsz, 1)) &&
                (names[i][namsz] == '\0'))
            {
                ++argsfound;
                if (dtype != PLSQL_TAB)
                {
                    /* Array binding to scalar, signatures don't match */
                    if (counts[i] > 0) return(0);
                }
                else if (counts[i] == 0)
                {
                    counts[i] = 1;
                    result = 1;
                }
                break;
            }
        }
    }

    if (argsfound < nargs) result = 0; /* Some arguments not found */

    return(result);
}

/*
** Compare the call to PL/SQL arguments by name and type,
** attempt to adjust array-bound parameters.  Returns TRUE
** if any array-bound parameter is promoted by the process.
*/
static int check_relaxed(connection *c, dvoid *arg_list,
                         int nargs, char *names[], ub4 counts[])
{
    sword  status;
    ub4    pos;
    ub4    acount;
    int    i;
    char  *namep;
    ub4    namsz;
    dvoid *argp;
    ub2    dtype;
    int    result = 0;

    status = OCIAttrGet(arg_list, (ub4)OCI_DTYPE_PARAM, (dvoid *)&acount,
                        (ub4 *)0, (ub4)OCI_ATTR_NUM_PARAMS, c->errhp);
    if (status != OCI_SUCCESS) return(result);

    for (pos = 1; pos <= acount; ++pos)
    {
        status = OCIParamGet(arg_list, (ub4)OCI_DTYPE_PARAM,
                             c->errhp, (dvoid **)&argp, (ub4)pos);
        if (status != OCI_SUCCESS) break;

        status = OCIAttrGet(argp, (ub4)OCI_DTYPE_PARAM,
                            (dvoid *)&namep, (ub4 *)&namsz,
                            (ub4)OCI_ATTR_NAME, c->errhp);
        if (status != OCI_SUCCESS) break;

        status = OCIAttrGet(argp, (ub4)OCI_DTYPE_PARAM,
                            (dvoid *)&dtype, (ub4 *)0,
                            (ub4)OCI_ATTR_DATA_TYPE, c->errhp);
        if (status != OCI_SUCCESS) break;

        /*
        ** Look for a bound argument with a matching name
        */
        for (i = 0; i < nargs; ++i)
        {
            if ((!str_compare(names[i], namep, namsz, 1)) &&
                (names[i][namsz] == '\0'))
            {
                /*
                ** If this is an array type but the binding is scalar,
                ** promote it to an array bind type.
                */
                if ((dtype == PLSQL_TAB) && (counts[i] == 0))
                {
                    counts[i] = 1;
                    result = 1;
                }
                else if ((dtype != PLSQL_TAB) && (counts[i] > 0))
                {
                    counts[i] = 0; /* Mark argument for demotion */
                    result = 1;
                }
            }
        }
    }

    return(result);
}

/*
** Compare the call to PL/SQL arguments by position and type,
** attempt to adjust array-bound parameters.  Returns TRUE
** if any array-bound parameter is promoted by the process.
*/
static int check_positional(connection *c, dvoid *arg_list,
                            int nargs, char *names[], ub4 counts[])
{
    sword  status;
    ub4    pos;
    ub4    acount;
    char  *namep;
    ub4    namsz;
    dvoid *argp;
    ub2    dtype;
    int    result = 0;

    status = OCIAttrGet(arg_list, (ub4)OCI_DTYPE_PARAM, (dvoid *)&acount,
                        (ub4 *)0, (ub4)OCI_ATTR_NUM_PARAMS, c->errhp);
    if (status != OCI_SUCCESS) return(result);

    for (pos = 1; pos <= acount; ++pos)
    {
        status = OCIParamGet(arg_list, (ub4)OCI_DTYPE_PARAM,
                             c->errhp, (dvoid **)&argp, (ub4)pos);
        if (status != OCI_SUCCESS) break;

        status = OCIAttrGet(argp, (ub4)OCI_DTYPE_PARAM,
                            (dvoid *)&namep, (ub4 *)&namsz,
                            (ub4)OCI_ATTR_NAME, c->errhp);
        if (status != OCI_SUCCESS) break;

        status = OCIAttrGet(argp, (ub4)OCI_DTYPE_PARAM,
                            (dvoid *)&dtype, (ub4 *)0,
                            (ub4)OCI_ATTR_DATA_TYPE, c->errhp);
        if (status != OCI_SUCCESS) break;

        /*
        ** If this is an array type but the variable in this postion
        ** is a scalar,  promote it to an array bind type.
        */
        if ((dtype == PLSQL_TAB) && (counts[pos] == 0))
        {
            counts[pos] = 1;
            result = 1;
        }
    }

    return(result);
}

/*
** Describe PL/SQL procedure and attempt to adjust array parameters
** Returns TRUE for successful conversion, FALSE otherwise.
*/
int sql_describe(connection *c, char *pname, int descmode, char *schema,
                 int nargs, char *names[], ub4 counts[])
{
    int          result;
    sword        status;
    int          tmpresult;
    ub4          slen;
    ub1          ptype;
    ub4          cnt;
    char        *sptr;
    dvoid       *arg_list;
    dvoid       *argp;
    dvoid       *proc_list;
    ub2          pcount;
    OCIParam    *parmp;
    OCIDescribe *dschp;
    char        *namep;
    ub4          namsz;
    char         tempname[1000]; /* 2x identifier size is much smaller */

    result = 0;
    if (descmode == DESC_MODE_STRICT) return(result);

    sptr = str_char(pname, '.', 1);
    if (sptr)
    {
        ptype = OCI_PTYPE_PKG;
        slen = (ub4)(sptr - pname);
        ++sptr;
    }
    else
    {
        ptype = OCI_PTYPE_PROC;
        slen = str_length(pname);
    }

    /*
    ** Create the describe handle
    */
    status = OCIHandleAlloc(c->envhp, (dvoid **)&dschp,
                            (ub4)OCI_HTYPE_DESCRIBE, 0, (dvoid **)0);
    if (status != OCI_SUCCESS) return(result);

    status = OCIDescribeAny(c->svchp, c->errhp,
                            (dvoid *)pname, (ub4)slen, (ub1)OCI_OTYPE_NAME,
                            (ub1)OCI_DEFAULT, ptype, dschp);
    if (status != OCI_SUCCESS)
    {
        /* Make sure to find any public synonym definitions */
        status = OCIAttrSet(dschp, (ub4)OCI_HTYPE_DESCRIBE, (dvoid *)0, (ub4)0,
                            (ub4)OCI_ATTR_DESC_PUBLIC, c->errhp);
        if (status != OCI_SUCCESS) goto descexit;

        /* This could be because the pname in question is a synonym */
        status = OCIDescribeAny(c->svchp, c->errhp,
                                (dvoid *)pname, (ub4)slen, (ub1)OCI_OTYPE_NAME,
                                (ub1)OCI_DEFAULT, OCI_PTYPE_SYN, dschp);
        if (status != OCI_SUCCESS) goto descexit;

        /* Synonym describe successful */
        status = OCIAttrGet(dschp, (ub4)OCI_HTYPE_DESCRIBE, (dvoid *)&parmp,
                            (ub4 *)0, (ub4)OCI_ATTR_PARAM, c->errhp);
        if (status != OCI_SUCCESS) goto descexit;

        status = OCIAttrGet(parmp, (ub4)OCI_DTYPE_PARAM,
                            (dvoid *)&namep, &namsz,
                            (ub4)OCI_ATTR_SCHEMA_NAME, c->errhp);
        if (status != OCI_SUCCESS) goto descexit;
        slen = namsz;
        mem_copy(tempname, namep, (size_t)namsz);
        tempname[slen++] = '.';
        status = OCIAttrGet(parmp, (ub4)OCI_DTYPE_PARAM,
                            (dvoid *)&namep, &namsz,
                            (ub4)OCI_ATTR_NAME, c->errhp);
        if (status != OCI_SUCCESS) goto descexit;
        mem_copy(tempname + slen, namep, (size_t)namsz);
        slen += namsz;
        tempname[slen] = '\0';

        /* Retry describe using resolved synonym */
        status = OCIDescribeAny(c->svchp, c->errhp,
                                (dvoid *)tempname, (ub4)slen,
                                (ub1)OCI_OTYPE_NAME,
                                (ub1)OCI_DEFAULT, ptype, dschp);
        if (status != OCI_SUCCESS) goto descexit;
    }

    status = OCIAttrGet(dschp, (ub4)OCI_HTYPE_DESCRIBE, (dvoid *)&parmp,
                        (ub4 *)0, (ub4)OCI_ATTR_PARAM, c->errhp);
    if (status != OCI_SUCCESS) goto descexit;

    if (ptype == OCI_PTYPE_PROC)
    {
        status = OCIAttrGet(parmp, (ub4)OCI_DTYPE_PARAM, (dvoid *)&arg_list,
                            (ub4 *)0, (ub4)OCI_ATTR_LIST_ARGUMENTS, c->errhp);
        if (status != OCI_SUCCESS) goto descexit;

        switch (descmode)
        {
        case DESC_MODE_NORMAL:
            result = check_normal(c, arg_list, nargs, names, counts);
            break;
        case DESC_MODE_RELAXED:
            result = check_relaxed(c, arg_list, nargs, names, counts);
            break;
        case DESC_MODE_POSITIONAL:
            result = check_positional(c, arg_list, nargs, names, counts);
            break;
        default:
            break;
        }
    }
    else /* ptype == OCI_PTYPE_PKG */
    {
        slen = str_length(sptr);
        status = OCIAttrGet(parmp, (ub4)OCI_DTYPE_PARAM, (dvoid *)&proc_list,
                            (ub4 *)0, (ub4)OCI_ATTR_LIST_SUBPROGRAMS,
                            c->errhp);
        if (status != OCI_SUCCESS) goto descexit;

        status = OCIAttrGet(proc_list, (ub4)OCI_DTYPE_PARAM, (dvoid *)&pcount,
                            (ub4 *)0, (ub4)OCI_ATTR_NUM_PARAMS, c->errhp);
        if (status != OCI_SUCCESS) goto descexit;

        for (cnt = 0; cnt < pcount; ++cnt)
        {
            status = OCIParamGet(proc_list, (ub4)OCI_DTYPE_PARAM,
                                 c->errhp, (dvoid **)&argp, (ub4)cnt);
            if (status != OCI_SUCCESS) break;

            status = OCIAttrGet(argp, (ub4)OCI_DTYPE_PARAM,
                                (dvoid *)&namep, &namsz,
                                (ub4)OCI_ATTR_NAME, c->errhp);
            if (status != OCI_SUCCESS) break;

            if ((slen == namsz) && (!str_compare(namep, sptr, namsz, 1)))
            {
                status = OCIAttrGet(argp, (ub4)OCI_DTYPE_PARAM,
                                    (dvoid *)&arg_list, (ub4 *)0,
                                    (ub4)OCI_ATTR_LIST_ARGUMENTS, c->errhp);
                if (status != OCI_SUCCESS) break;

                switch (descmode)
                {
                case DESC_MODE_NORMAL:
                  tmpresult = check_normal(c,arg_list,nargs,names,counts);
                  break;
                case DESC_MODE_RELAXED:
                  tmpresult = check_relaxed(c,arg_list,nargs,names,counts);
                  break;
                case DESC_MODE_POSITIONAL:
                  tmpresult = check_positional(c,arg_list,nargs,names,counts);
                  break;
                default:
                  tmpresult = 0;
                  break;
                }
                if (tmpresult)
                {
                    result = 1;
                    break; /* Stop at first successful conversion */
                }
            }
        }
    }

descexit:
    status = OCIHandleFree((dvoid *)dschp, (ub4)OCI_HTYPE_DESCRIBE);

    return(result);
}
