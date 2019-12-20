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
** Save this file as ocitest.c
**
** Add these lines to your existing mod_owa Makefile:
**
** ocitest: ocitest.o
**	$(LD) -o $@ ocitest.o $(ORALINK) $(CLIBS)
**
** make -kf modowa.mk ocitest
**
** ocitest username/password@yourdatabase
**
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <malloc.h>
#include <ctype.h>
#include <oratypes.h>
#include <oci.h>

/* Handle to hold OCI connection information */
typedef struct connection
{
    OCIEnv        *envhp;
    OCIError      *errhp;
    OCIServer     *srvhp;
    OCISvcCtx     *svchp;
    OCISession    *seshp;
    OCIStmt       *stmhp1;
    char           errbuf[OCI_ERROR_MAXMSG_SIZE + 1];
} connection;

/*
** Testing statement
*/
char sql_stmt[] = "begin :B := to_char(SYSDATE,'YYYY/MM/DD HH24:MI:SS'); end;";

/* Execute PL/SQL statement through OCI */
sword sql_exec(connection *c, OCIStmt *stmhp)
{
    return(OCIStmtExecute(c->svchp, stmhp, c->errhp, (ub4)1, (ub4)0,
                          (OCISnapshot *)0, (OCISnapshot *)0,
                          (ub4)OCI_DEFAULT));
}

/* Get a normal error from the OCI error handle and return the code */
sb4 sql_get_error(connection *c)
{
    sb4 oerrno;

    OCIErrorGet((dvoid *)(c->errhp), (ub4)1, (text *)0,
                &oerrno, (text *)(c->errbuf),
                (ub4)OCI_ERROR_MAXMSG_SIZE, (ub4)OCI_HTYPE_ERROR);

    return(oerrno);
}

/* Create OCI connection */
sword sql_connect(connection *c, char *oracle_userid)
{
    sword  status;
    sb4    oerrno;
    char  *sptr;
    char  *optr;
    char  *username;
    char  *password;
    char  *database;
    int    ulen, plen, dlen;
    char   buf[1024];

    optr = oracle_userid;
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
    for (plen = 0; *optr != '@'; ++plen)
    {
        *(sptr++) = *(optr++);
        if ((plen + ulen + 2) == sizeof(buf)) return(OCI_ERROR);
        if (*optr == '\0')
        {
            ++plen;
            break;
        }
    }
    *(sptr++) = '\0';
    if (*optr != '\0') ++optr;
    if (*optr == '\0')
    {
        database = (char *)0;
        dlen = 0;
    }
    else
    {
        database = optr;
        dlen = strlen(database);
    }

    /*
    ** Initialize OCI handles
    */
    status = OCIEnvCreate(&(c->envhp), (ub4)OCI_THREADED, (dvoid *)0,
                          (dvoid * (*)())0, (dvoid * (*)())0, (dvoid (*)())0,
                          (size_t)0, (dvoid *)0);
    if (status != OCI_SUCCESS)
    {
        sprintf(c->errbuf, "OCIEnvCreate error %d", status);
        return(status);
    }
    status = OCIHandleAlloc(c->envhp, (dvoid *)&(c->errhp),
                            (ub4)OCI_HTYPE_ERROR, (size_t)0, (dvoid *)0);
    if (status != OCI_SUCCESS) goto handerr;
    status = OCIHandleAlloc(c->envhp, (dvoid *)&(c->svchp),
                            (ub4)OCI_HTYPE_SVCCTX, (size_t)0, (dvoid *)0);
    if (status != OCI_SUCCESS) goto handerr;
    status = OCIHandleAlloc(c->envhp, (dvoid *)&(c->srvhp),
                            (ub4)OCI_HTYPE_SERVER, (size_t)0, (dvoid *)0);
    if (status != OCI_SUCCESS) goto handerr;

    /*
    ** Attach to specified database server
    */
    status = OCIServerAttach(c->srvhp, c->errhp, (text *)database, (sb4)dlen,
                             (ub4)OCI_DEFAULT);
    if (status != OCI_SUCCESS) goto connerr;

    /* Set the server for use with the service context */
    status = OCIAttrSet(c->svchp, (ub4)OCI_HTYPE_SVCCTX, c->srvhp, (ub4)0,
                        (ub4)OCI_ATTR_SERVER, c->errhp);
    if (status != OCI_SUCCESS) goto connerr;

    /*
    ** Log on and create a new session
    */
    status = OCIHandleAlloc(c->envhp, (dvoid *)&(c->seshp),
                            (ub4)OCI_HTYPE_SESSION, (size_t)0, (dvoid *)0);
    if (status != OCI_SUCCESS) goto handerr;
    status = OCIAttrSet(c->seshp, (ub4)OCI_HTYPE_SESSION, username, (ub4)ulen,
                        (ub4)OCI_ATTR_USERNAME, c->errhp);
    if (status != OCI_SUCCESS) goto connerr;
    status = OCIAttrSet(c->seshp, (ub4)OCI_HTYPE_SESSION, password, (ub4)plen,
                        (ub4)OCI_ATTR_PASSWORD, c->errhp);
    if (status != OCI_SUCCESS) goto connerr;
    status = OCISessionBegin(c->svchp, c->errhp, c->seshp,
                             (ub4)OCI_CRED_RDBMS, (ub4)OCI_DEFAULT);
    if (status != OCI_SUCCESS) goto connerr;
    status = OCIAttrSet(c->svchp, (ub4)OCI_HTYPE_SVCCTX, c->seshp, (ub4)0,
                        (ub4)OCI_ATTR_SESSION, c->errhp);
    if (status != OCI_SUCCESS) goto connerr;

    status = OCIHandleAlloc(c->envhp, (dvoid *)&(c->stmhp1),
                            (ub4)OCI_HTYPE_STMT, (size_t)0, (dvoid *)0);
    if (status != OCI_SUCCESS) goto handerr;

    status = OCIStmtPrepare(c->stmhp1, c->errhp, (text *)sql_stmt,
                            (ub4)strlen(sql_stmt),
                            (ub4)OCI_NTV_SYNTAX, (ub4)OCI_DEFAULT);
    if (status != OCI_SUCCESS) goto connerr;

    return(status);

connerr:
    if (status != OCI_SUCCESS)
    {
        oerrno = sql_get_error(c); 
        if (oerrno) status = oerrno;
    }
    return(status);
handerr:
    if (status != OCI_SUCCESS)
    {
        OCIErrorGet((dvoid *)(c->envhp), (ub4)1, (text *)0, &oerrno,
                    (text *)(c->errbuf),
                    (ub4)OCI_ERROR_MAXMSG_SIZE, (ub4)OCI_HTYPE_ENV);
        if (oerrno) status = oerrno;
    }
    return(status);
}

/* Bind a string buffer to an OCI statement */
sword sql_bind_str(connection *c, OCIStmt *stmhp, ub4 pos, char *str, sb4 slen)
{
    OCIBind *bhand;
    return(OCIBindByPos(stmhp, &bhand, c->errhp, pos,
                        (dvoid *)str, slen,
                        (ub2)SQLT_STR, (dvoid *)0,
                        (ub2 *)0, (ub2 *)0, 0, (ub4 *)0, (ub4)OCI_DEFAULT));
}

/*
** Get SYSDATE from PL/SQL
*/
sword runstmt(connection *c)
{
    sword  status;
    char   sysdate[100];

    *sysdate = '\0';
    status = sql_bind_str(c, c->stmhp1, (ub4)1, sysdate, sizeof(sysdate));
    if (status != OCI_SUCCESS) goto cgierr;

    status = sql_exec(c, c->stmhp1);
    if (status != OCI_SUCCESS) goto cgierr;

    printf("SYSDATE = [%s]\n", sysdate);
    return(status);

cgierr:
    sql_get_error(c);
    return(status);
}

/*
** Usage: program <username/password@database>
*/
int main(argc, argv)
int   argc;
char *argv[];
{
    sword       status;
    connection  conn;
    connection *c = &conn;

    if (argc < 2)
    {
        printf("usage: %s <username/password@database>\n", argv[0]);
        return(0);
    }

    memset(c, 0, sizeof(*c));
    status = sql_connect(c, argv[1]);
    if (status != OCI_SUCCESS) goto reterr;

    printf("Connected successfully\n");

    status = runstmt(c);
    if (status != OCI_SUCCESS) goto reterr;

    return(0);

reterr:
    printf("%s\n", c->errbuf);
    return(1);
}
