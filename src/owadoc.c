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
** File:         owadoc.c
** Description:  Gateway document-related routines.
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
** 06/04/2001   D. McMahon      Fix bug re. added headers in file uploads
** 06/16/2001   D. McMahon      Added support for RAW input/output bindings
** 09/01/2001   D. McMahon      Added array and scalar rounding
** 10/30/2001   D. McMahon      Fix bug with mime types and file caching
** 11/25/2001   D. McMahon      Return Content-Length:0 for empty docs
** 01/31/2002   D. McMahon      Always truncate LOB after write
** 02/11/2002   D. McMahon      Add OwaDocFile
** 03/27/2002   D. McMahon      Rework request read handling
** 09/18/2002   D. McMahon      Fix bug with width of reserved binding
** 11/14/2002   D. McMahon      Do not use RAW bind mode for OUT params
** 03/17/2003   D. McMahon      Merge URL arguments for POSTs
** 09/14/2004   D. McMahon      Use HTBUF_PLSQL_MAX for string buffers
** 01/13/2006   D. McMahon      Limit file upload arguments to 4000
** 06/14/2006   D. McMahon      Moved mem_find to owautil.c
** 04/15/2007   D. McMahon      Add owa_lobtable() for file download
** 07/04/2007   D. McMahon      Add owa_wlobtable() for file upload
** 03/11/2010   D. McMahon      Hack owa_readlob() for the WPG_DOCLOAD case
** 04/28/2010   D. McMahon      Fix owa_wlobtable() for null file uploads
** 04/30/2010   D. McMahon      (long) casts for morq_read and morq_write
** 05/21/2012   D. McMahon      Support flex-args mode for Apex docload
** 06/01/2012   D. McMahon      Fix multi-file Apex docload
** 11/27/2012   D. McMahon      GCC fixes, Win-64 porting
** 03/06/2013   D. McMahon      Avoid writing 0-length LOBs to WPG_DOCLOAD
** 03/07/2013   D. McMahon      Fix non-byte-unique file separator scan
** 06/10/2013   D. McMahon      Change interface to sql_fetch()
** 07/20/2013   D. McMahon      Fix a bug in the SQL of owa_rlobtable
** 07/25/2013   D. McMahon      Set Content-Disposition in owa_rlobtable
** 09/19/2013   D. McMahon      64-bit content length for OwaDocTable writes
** 09/20/2013   D. McMahon      64-bit content length for LOB writes
** 04/27/2014   D. McMahon      Resize name width for chunked parameters
** 12/23/2014   D. McMahon      Add marker argument to owa_wlobtable()
** 04/21/2015   D. McMahon      Switch on 64-bit LOBs (OCI 10g dependency)
** 05/07/2015   D. McMahon      Allow 64-bit LOBs for owa_writedata()
** 05/11/2015   D. McMahon      Streaming read/write for large REST bodies
** 09/11/2015   D. McMahon      Disallow control characters in headers
** 07/14/2016   D. McMahon      Avoid 1460 errors on 32-bit OCIs
** 09/09/2016   D. McMahon      Fix separator search for: Shift-JIS, BIG-5, GBK
*/

#define WITH_OCI
#define APACHE_LINKAGE
#include <modowa.h>

#ifndef OLD_LOBS
# ifndef OVERSIZED_LOBS  /* This is the default if OLD_LOBS isn't set */
#  define OVERSIZED_LOBS /* This flag enables 64-bit LOB lengths      */
# endif
#endif

/*
** Get file name for file-based transfer
*/
static int getfile(request_rec *r, connection *c, char *stmt, int first_flag,
                   sb4 nparams, char *pnams[], char *pvals[],
                   sb4 nam_width, sb4 val_width, char *pmimetype,
                   char **tmpbuf, int *tmpsize, char *outbuf, char *ret_file)
{
    sword    status;
    sb4      oerrno;
    int      i;
    char     reserved[HTBUF_ARRAY_SIZE*2];
    ub2      rlens[HTBUF_ARRAY_SIZE];
    ub4      counts[3];

    mem_zero(reserved, sizeof(reserved));

    counts[0] = nparams; /* Names */
    counts[1] = nparams; /* Values */
    counts[2] = nparams; /* Reserved */

    c->lastsql = stmt;

    /*
    ** ### first_flag SHOULD BE USED FOR EFFICIENCY BUT stmhp3 USED LATER
    */
    status = sql_parse(c, c->stmhp3, stmt, -1);
    if (status != OCI_SUCCESS) goto filerror;

    *ret_file = '\0';

    status = sql_bind_int(c, c->stmhp3, (ub4)1, &nparams);
    if (status != OCI_SUCCESS) goto filerror;

    status = sql_bind_ptrs(c, c->stmhp3, (ub4)2, pnams,
                           (sb4)(nam_width+1), counts + 2, counts[2]);
    if (status != OCI_SUCCESS) goto filerror;

    status = sql_bind_ptrs(c, c->stmhp3, (ub4)3, pvals,
                           (sb4)(val_width+1), counts + 1, counts[1]);
    if (status != OCI_SUCCESS) goto filerror;

    if (c->ncflag & UNI_MODE_RAW)
    {
      for (i = 0; i < HTBUF_ARRAY_SIZE; ++i) rlens[i] = 0;
      status = sql_bind_rarr(c, c->stmhp3, (ub4)4, reserved, rlens,
                             (sb4)2, (sb2 *)0, counts, *counts);
      if (status != OCI_SUCCESS) goto filerror;
    }
    else
    {
      status = sql_bind_arr(c, c->stmhp3, (ub4)4, reserved, (ub2 *)0,
                            (sb4)2, (sb2 *)0, counts, *counts);
      if (status != OCI_SUCCESS) goto filerror;
    }

    status = sql_bind_str(c, c->stmhp3, (ub4)5, pmimetype,
                          (sb4)HTBUF_LINE_LENGTH);
    if (status != OCI_SUCCESS) goto filerror;

    status = sql_bind_str(c, c->stmhp3, (ub4)6, ret_file,
                          (sb4)HTBUF_HEADER_MAX);
    if (status != OCI_SUCCESS) goto filerror;

    status = sql_exec(c, c->stmhp3, (ub4)1, 0);
    if (status != OCI_SUCCESS) goto filerror;

filerror:
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

/*
** Run PL/SQL to return a LOB handle for writing
*/
static int getlob(request_rec *r, connection *c, char *stmt,
                  int nargs, int first_flag,
                  sb4 nparams, char *pnams[], char *pvals[],
                  sb4 nam_width, sb4 val_width, char *pmimetype,
                  char **tmpbuf, int *tmpsize, char *outbuf)
{
    sword  status;
    sb4    oerrno;
    int    i;
    char   reserved[HTBUF_ARRAY_SIZE*2];
    ub2    rlens[HTBUF_ARRAY_SIZE];
    ub4    counts[3];

    mem_zero(reserved, sizeof(reserved));

    counts[0] = nparams; /* Names */
    counts[1] = nparams; /* Values */
    counts[2] = nparams; /* Reserved */

    c->lastsql = stmt;

    if (first_flag)
    {
        status = sql_parse(c, c->stmhp3, stmt, -1);
        if (status != OCI_SUCCESS) goto glberror;
    }


    status = sql_bind_int(c, c->stmhp3, (ub4)1, &nparams);
    if (status != OCI_SUCCESS) goto glberror;

    status = sql_bind_ptrs(c, c->stmhp3, (ub4)2, pnams,
                           (sb4)(nam_width+1), counts + 2, counts[2]);
    if (status != OCI_SUCCESS) goto glberror;

    status = sql_bind_ptrs(c, c->stmhp3, (ub4)3, pvals,
                           (sb4)(val_width+1), counts + 1, counts[1]);
    if (status != OCI_SUCCESS) goto glberror;

    if (c->ncflag & UNI_MODE_RAW)
    {
      for (i = 0; i < HTBUF_ARRAY_SIZE; ++i) rlens[i] = 0;
      status = sql_bind_rarr(c, c->stmhp3, (ub4)4, reserved, rlens,
                             (sb4)2, (sb2 *)0, counts, *counts);
      if (status != OCI_SUCCESS) goto glberror;
    }
    else
    {
      status = sql_bind_arr(c, c->stmhp3, (ub4)4, reserved, (ub2 *)0,
                            (sb4)2, (sb2 *)0, counts, *counts);
      if (status != OCI_SUCCESS) goto glberror;
    }

    status = sql_bind_str(c, c->stmhp3, (ub4)5,
                          pmimetype, (sb4)HTBUF_LINE_LENGTH);
    if (status != OCI_SUCCESS) goto glberror;

    /* Set indicators for unbound arguments to null */
    c->bfile_ind = (ub2)-1;
    c->nlob_ind  = (ub2)-1;
    c->clob_ind  = (ub2)-1;
    c->blob_ind  = (ub2)-1;

    if (nargs >= 6)
    {
        status = sql_bind_lob(c, c->stmhp3, (ub4)6, SQLT_BLOB);
        if (status != OCI_SUCCESS) goto glberror;
    }

    if (nargs >= 7)
    {
        status = sql_bind_lob(c, c->stmhp3, (ub4)7, SQLT_CLOB);
        if (status != OCI_SUCCESS) goto glberror;
    }

    if (nargs >= 8)
    {
        status = sql_bind_lob(c, c->stmhp3, (ub4)8, 0);
        if (status != OCI_SUCCESS) goto glberror;
    }

    if (nargs >= 9) /* ### BFILE writes aren't yet supported */
    {
        status = sql_bind_lob(c, c->stmhp3, (ub4)9, SQLT_BFILE);
        if (status != OCI_SUCCESS) goto glberror;
    }

    status = sql_exec(c, c->stmhp3, (ub4)1, 0);

glberror:
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

/*
** Open SQL statement for LONG or LONG RAW UPDATE statement
*/
static int openlong(request_rec *r, connection *c,
                    char *stmt, char *sbind, int bin_flag, ub4 nbytes)
{
    sword    status;
    sb4      oerrno;
    int      stmtlen;
    ub2      sql_type;
    ub2      rind = (ub2)-1;

    stmtlen = str_length(stmt);
    if (stmtlen == 0) return(OCI_SUCCESS); /* Nothing to do */

    /*
    ** Build the UPDATE (or INSERT) statement and execute it.
    */
    c->lastsql = stmt;
    status = sql_parse(c, c->stmhp3, stmt, stmtlen);
    if (status != OCI_SUCCESS) goto oplerror;

    if (nbytes == 0)
    {
        /*
        ** For 0-length transfer, bind a null
        */
        if (bin_flag)
            status = sql_bind_raw(c,c->stmhp3,(ub4)1, (char *)"",&rind,(sb4)0);
        else
            status = sql_bind_str(c,c->stmhp3,(ub4)1, (char *)"",(sb4)0);
    }
    else
    {
        /*
        ** SQL_LBI is a LONG RAW.  Another possibility is SQLT_BIN.
        ** SQL_LNG is a LONG.  Other possibilities are SQLT_CHR and SQLT_STR.
        ** In case needed later:
        **   SQLT_LVB is 4-byte leading long raw
        **   SQLT_LVC is 4-byte leading long varchar
        */
        sql_type = (bin_flag) ? SQLT_LBI : SQLT_LNG;

        status = sql_bind_stream(c, c->stmhp3, (ub4)1, sql_type);
    }
    if (status != OCI_SUCCESS) goto oplerror;

    if (*sbind)
    {
        status = sql_bind_str(c, c->stmhp3, (ub4)2, sbind,
                              (sb4)(str_length(sbind) + 1));
        if (status != OCI_SUCCESS) goto oplerror;
    }

    status = sql_exec(c, c->stmhp3, (ub4)1, 0);
    /* Statement should now be ready for piecewise insert */
    if (status == NEED_WRITE_DATA) status = OCI_SUCCESS;

oplerror:
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

/*
** Get SQL INSERT or UPDATE statement for LONG or LONG RAW transfer
*/
static int getcursor(request_rec *r, connection *c, char *stmt, int first_flag,
                     sb4 nparams, char *pnams[], char *pvals[],
                     sb4 nam_width, sb4 val_width, char *pmimetype,
                     char **tmpbuf, int *tmpsize, char *outbuf,
                     char *ret_sql, char *ret_bind, int *bin_flag)
{
    sword    status;
    sb4      oerrno;
    int      i;
    char     reserved[HTBUF_ARRAY_SIZE*2];
    ub2      rlens[HTBUF_ARRAY_SIZE];
    char     prawchar[10];
    ub4      counts[3];

    mem_zero(reserved, sizeof(reserved));

    counts[0] = nparams; /* Names */
    counts[1] = nparams; /* Values */
    counts[2] = nparams; /* Reserved */

    c->lastsql = stmt;

    /*
    ** ### first_flag SHOULD BE USED FOR EFFICIENCY BUT stmhp3 USED LATER
    */
    status = sql_parse(c, c->stmhp3, stmt, -1);
    if (status != OCI_SUCCESS) goto curerror;

    *ret_bind = '\0';
    *ret_sql = '\0';
    *prawchar = '\0';

    status = sql_bind_int(c, c->stmhp3, (ub4)1, &nparams);
    if (status != OCI_SUCCESS) goto curerror;

    status = sql_bind_ptrs(c, c->stmhp3, (ub4)2, pnams,
                           (sb4)(nam_width+1), counts + 2, counts[2]);
    if (status != OCI_SUCCESS) goto curerror;

    status = sql_bind_ptrs(c, c->stmhp3, (ub4)3, pvals,
                           (sb4)(val_width+1), counts + 1, counts[1]);
    if (status != OCI_SUCCESS) goto curerror;

    if (c->ncflag & UNI_MODE_RAW)
    {
      for (i = 0; i < HTBUF_ARRAY_SIZE; ++i) rlens[i] = 0;
      status = sql_bind_rarr(c, c->stmhp3, (ub4)4, reserved, rlens,
                             (sb4)(val_width+1), (sb2 *)0, counts, *counts);
      if (status != OCI_SUCCESS) goto curerror;
    }
    else
    {
      status = sql_bind_arr(c, c->stmhp3, (ub4)4, reserved, (ub2 *)0,
                            (sb4)(val_width+1), (sb2 *)0, counts, *counts);
      if (status != OCI_SUCCESS) goto curerror;
    }

    status = sql_bind_str(c, c->stmhp3, (ub4)5, pmimetype,
                          (sb4)HTBUF_LINE_LENGTH);
    if (status != OCI_SUCCESS) goto curerror;

    status = sql_bind_str(c, c->stmhp3, (ub4)6, ret_sql,
                          (sb4)HTBUF_PLSQL_MAX);
    if (status != OCI_SUCCESS) goto curerror;

    status = sql_bind_str(c, c->stmhp3, (ub4)7, ret_bind,
                          (sb4)HTBUF_HEADER_MAX);
    if (status != OCI_SUCCESS) goto curerror;

    status = sql_bind_str(c, c->stmhp3, (ub4)8, prawchar,
                          (sb4)sizeof(prawchar));
    if (status != OCI_SUCCESS) goto curerror;

    status = sql_exec(c, c->stmhp3, (ub4)1, 0);
    if (status != OCI_SUCCESS) goto curerror;

    *bin_flag = (*prawchar != '\0');

curerror:
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

/*
** Write a piece of a LONG or LONG RAW
*/
static int write_piece(connection *c, dvoid *lbuf, ub4 nbytes, int lob_flag)
{
    sword    status;
    status = sql_write_piece(c, c->stmhp3, lbuf, &nbytes, lob_flag);
    if (status == OCI_SUCCESS) status = sql_exec(c, c->stmhp3, (ub4)1, 0);
    return(status);
}

/*
** Read content data stream and write to LOBs or LONGs
**
** ### Add the compatibility mode that writes directly to document_table.
** ### Need to generate unique name from SYSDATE+OSPID/filename.
** ### Need directives "upload_blob", "upload_clob", etc. that
** ### classify upload type by file extension.
*/
int owa_writedata(connection *c, owa_context *octx, request_rec *r,
                  char *boundary, long_64 clen, int long_flag,
                  char *outbuf, char *stmt, char *pmimetype, int nargs,
                  int nuargs, char **pnames, char **pvalues)
{
    sword          status = OCI_SUCCESS;
    sb4            oerrno;
    int            i, j, n;
    int            end_data;
    int            lob_flag;
    int            first_flag;
    int            bound_flag;
    int            bin_flag = 0;
    int            blen;
    char          *aptr;
    char          *sptr;
    char          *filpath;
    char          *fldname;
    char          *ret_sql;
    char          *ret_bind;
    char          *ret_file;
    sb4            nparams;
    un_long        nbytes;
    long_64        total;
    ub4            amount;
    long_64        offset;
    ub2            cs_id;
    int            nfiles;
    long_64        file_sizes[HTBUF_ARRAY_SIZE];
    char          *param_name[HTBUF_ARRAY_SIZE];
    char          *param_value[HTBUF_ARRAY_SIZE];
    char          *tempname;
    char           cmax[32];
    sb4            nam_width;
    sb4            val_width;
    dvoid         *lbuf;
    char          *tmpbuf;
    int            tmpsize;
#ifndef NO_FILE_CACHE
    os_objhand  fp = os_nullfilehand;
#endif
    OCILobLocator *plob = (OCILobLocator *)0;

    switch (long_flag)
    {
    case 1:
        param_name[0] = (char *)"document_long";
        param_value[0] = octx->doc_long;
        break;
    case 2:
        param_name[0] = (char *)"document_file";
        param_value[0] = octx->doc_long;
        break;
    default:
        param_name[0] = (char *)"document_path";
        param_value[0] = octx->doc_path;
        break;
    }

    if (!param_value[0]) param_value[0] = (char *)"";
    nparams = 2;
    nam_width = str_length(*param_name);
    val_width = str_length(*param_value);
    if (val_width < 2) val_width = 2;

    /* Transfer the URL parameters */
    for (n = 0; n < nuargs; ++n)
    {
        param_name[nparams] = pnames[n];
        param_value[nparams] = pvalues[n];
        i = str_length(param_name[nparams]);
        if (i > nam_width) nam_width = i;
        i = str_length(param_value[nparams]);
        if (i > val_width) val_width = i;
        if (++nparams == HTBUF_ARRAY_SIZE) break;
    }

    /* Initialize the temporary buffer in case it's needed later */
    tmpbuf = (char *)0;
    tmpsize = 0;

    /*
    ** Assumes the caller only needs space for 2X the block size,
    ** and that there is another 2X available.
    */
    ret_sql  = outbuf + (HTBUF_BLOCK_SIZE << 1);
    ret_bind = ret_sql + HTBUF_BLOCK_SIZE;
    tempname = ret_bind;
    ret_file = ret_sql;

    blen = str_length(boundary);

    cs_id = nls_csid(octx->dad_csid);

    first_flag = 1;

    offset = (long_64)1;

    lob_flag = OCI_LAST_PIECE;
    end_data = 0;

    c->mem_err = 0;

    nfiles = 0;

    /* Limit content upload */
    if ((octx->upmax > 0) && (clen > octx->upmax))
    {
        str_ltoa(octx->upmax, cmax);
        param_name[0] = (char *)"upload_max";
        param_value[0] = cmax;
        goto max_err;
    }
    /* Force streaming read of chunked data */
    DEFAULT_MAX(clen);

    morq_stream(r, 0);
    str_copy(outbuf, "\r\n");
    i = 2;
    while ((i > 0) || (!end_data))
    {
        if ((i < HTBUF_BLOCK_SIZE) && (!end_data))
        {
            if (clen < HTBUF_BLOCK_SIZE) j = (int)clen;
            else                         j = HTBUF_BLOCK_SIZE;
            n = morq_read(r, outbuf + i, (long)j);
            if (n < 0)
            {
                n = 0;
                clen = 0;
            }
            clen -= (long_64)n;
            end_data = (clen == 0);
            n += i;
        }
        else
        {
            n = i;
        }

        sptr = mem_find(outbuf, (long_64)n, boundary, blen);
        if (sptr)
        {
            i = (int)(sptr - outbuf);
            bound_flag = ((n - i) >= blen);
        }
        else
        {
            sptr = outbuf + n;
            bound_flag = 0;
        }
        i = (int)(sptr - outbuf);

        /* If boundary at top of buffer, ensure adequate data has been read */
        if ((n < HTBUF_BLOCK_SIZE) && (!end_data) && (i == 0))
        {
            i = n;
            continue;
        }

        if (lob_flag != OCI_LAST_PIECE)
        {
            amount = i;
            if ((i > 0) && (nls_cstype(cs_id)) && (!bin_flag))
            {
                nbytes = (un_long)i;
                amount = nls_count_chars((int)cs_id, outbuf, &nbytes);
                j = (int)nbytes;
                if (j < i)
                {
                    if (!end_data)
                        i = j; /* Leave fractional character for next pass */
                    else
                        amount += (i - j); /* Error - trailing fragment! */
                }
            }

            /* Consume all data and continue */
            if (lob_flag == OCI_ONE_PIECE)
            {
                if (long_flag == 1)
                {
                    status = openlong(r, c, ret_sql, ret_bind,
                                      bin_flag, (ub4)i);
                    if (status != OCI_SUCCESS) goto wrterror;
                }
#ifdef NEVER /* ### DOESN'T WORK FOR UPDATE CASE! ### */
                else if (!long_flag)
                {
                    status = OCILobOpen(c->svchp, c->errhp, plob,
                                        (ub4)OCI_LOB_READWRITE);
                    if (status != OCI_SUCCESS) goto wrterror;
                }
#endif
                offset = (long_64)1;
                if ((i > 0) && (!end_data) && (!bound_flag))
                    lob_flag = OCI_FIRST_PIECE;
            }
            else /* OCI_FIRST_PIECE || OCI_NEXT_PIECE */
            {
                if ((i == 0) || (bound_flag))
                    lob_flag = OCI_LAST_PIECE;
                else
                    lob_flag = OCI_NEXT_PIECE;
            }

            if ((i == 0) && (!long_flag))
            {
                lbuf = (dvoid *)"";
                nbytes = 1;
            }
            else
            {
                lbuf = (dvoid *)outbuf;
                nbytes = i;
            }

            total = (lob_flag == OCI_ONE_PIECE) ? (long_64)nbytes : 0;

#ifndef NO_FILE_CACHE
            if (long_flag == 2)
            {
                if ((nbytes > 0) && (!InvalidFile(fp)))
                    file_write_data(fp, (char *)lbuf, nbytes);
            }
            else
#endif
            if (long_flag)
            {
                if (nbytes > 0)
                    status = write_piece(c, lbuf, (ub4)nbytes, lob_flag);
            }
            else
            {
#ifdef OVERSIZED_LOBS
                if (sizeof(size_t) == sizeof(long_64))
                {
                    oraub8 lsize = (oraub8)total;

                    status = OCILobWrite2(c->svchp, c->errhp, plob, &lsize,
                                          (oraub8 *)0, (oraub8)offset,
                                          lbuf, (oraub8)nbytes,
                                          (ub1)lob_flag, (dvoid *)0,
                                          NULL, c->csid, (ub1)0);

                    total = (long_64)lsize;
                }
                else
#endif
                {
                    ub4 lsize = (ub4)total;

                    status = OCILobWrite(c->svchp, c->errhp, plob, &lsize,
                                         (ub4)offset, lbuf, (ub4)nbytes,
                                         (ub1)lob_flag, (dvoid *)0,
                                         NULL, c->csid, (ub1)0);

                    total = (long_64)lsize;
                }
            }

            if (status == NEED_WRITE_DATA) status = OCI_SUCCESS;
            if (status != OCI_SUCCESS) goto wrterror;

            offset += (long_64)amount;

            if (lob_flag == OCI_ONE_PIECE) lob_flag = OCI_LAST_PIECE;

            if (lob_flag == OCI_LAST_PIECE)
            {
                /* Save the file size for later */
                file_sizes[nfiles++] = --offset;
#ifndef NO_FILE_CACHE
                if (long_flag == 2)
                {
                    file_close(fp);
                    file_move(tempname, ret_file);
                }
#endif
                if (!long_flag)
                {
                    /*
                    ** Trim the LOB (in case this was an update to a LOB that
                    ** was previously larger), then close it and commit it.
                    */
                    /*
                    ** ### THERE IS UNFORTUNATELY NO WAY TO TRIM A LONG
                    ** ### OR LONG RAW.  TO CLOSE THE TRANSACTION
                    ** ### YOU HAVE TO HAVE CALLED write_piece, WHICH
                    ** ### WON'T TAKE A NON-ZERO VALUE!  TO FIX THIS,
                    ** ### WE'LL HAVE TO DEFER THE BIND FOR THE LONG
                    ** ### STATEMENT UNTIL THE FIRST TRANSFER NEEDS TO
                    ** ### BE DONE.  AT THAT TIME, IF THE FIRST TRANSFER
                    ** ### IS ZERO BYTES, WE WOULD BIND IN THE IMMEDIATE
                    ** ### MODE WITH A NULL BUFFER.  UGH!
                    */
#ifdef OVERSIZED_LOBS
                    if (offset > LONG_MAXSZ)
                    {
                      status = OCILobTrim2(c->svchp, c->errhp,
                                           plob, (oraub8)offset);
                    }
                    else
#endif
                    {
                      status = OCILobTrim(c->svchp, c->errhp,
                                          plob, (ub4)offset);
                    }
                    if (status != OCI_SUCCESS) goto wrterror;
#ifdef NEVER /* ### DOESN'T WORK FOR UPDATE CASE! ### */
                    status = OCILobClose(c->svchp, c->errhp, plob);
                    if (status != OCI_SUCCESS) goto wrterror;
#endif
                }

                status = sql_transact(c, 0); /* Commit */
                if (status != OCI_SUCCESS) goto wrterror;
            }
        }

        if (i < n) /* Boundary or boundary fragment has been encountered */
        {
            outbuf[n] = '\0';
            j = n - i;
            if ((j >= blen) && (i == 0))
            {
                /*
                ** Boundary has been found
                ** Boundary aligned at beginning of buffer
                */
                sptr += blen;
                if (!str_compare(sptr, "--", 2, 0))
                {
                    /* End of content stream, exit loop */
                    break;
                }
                if (str_compare(sptr,
                                "\r\nContent-Disposition: form-data;", 33, 1))
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
                if (!fldname)
                {
                    c->mem_err = j;
                    return(OCI_SUCCESS);
                }
                filpath = (char *)0;

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
                i = (int)(aptr - fldname);
                if (i > nam_width) nam_width = i;
                *(aptr++) = '\0';
                sptr += 2;
                while (*sptr == ' ') ++sptr;
                if (!str_compare(sptr, "filename=\"", 10, 1))
                {
                    sptr += 10;
                    filpath = aptr;
                    while (*sptr != '\"') *(aptr++) = *(sptr++);
                    *aptr = '\0';
                    i = (int)(aptr - filpath);
                    if (i > val_width) val_width = i;
                }
                /* Now look for optional Content-type */
                *pmimetype = '\0';
                sptr = outbuf + j;
                if (!str_compare(sptr, "Content-Type: ", 14, 1))
                {
                    aptr = str_substr(sptr, "\r\n", 0);
                    if (!aptr) break; /* Another streaming error ### */
                    *aptr = ' ';
                    j = (int)(aptr - outbuf) + 2;
                    sptr += 14;
                    *aptr = '\0';
                    if ((*sptr) && ((aptr - sptr) < HTBUF_HEADER_MAX))
                        str_copy(pmimetype, sptr);
                    sptr = outbuf + j;
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

                if (!filpath)
                {
                    /*
                    ** Data until next boundary is content for this field
                    ** Contents limited to about 32K by this code
                    */
                    aptr = mem_find(sptr, (long_64)(n - j), boundary, blen);
                    /*
                    ** ### If the end of the field cannot be found,
                    ** ### the code just fails right now.  This is
                    ** ### suboptimal but since that means the end
                    ** ### doesn't occur for around 32K bytes
                    ** ### forward, it's hard to do anything else.
                    ** ### Possibly convert this to an improved
                    ** ### chunking model, or to a "fake file"
                    ** ### approach that streams to a LOB.
                    */
                    if (!aptr) break; /* Error - field item too large */
                    i = (int)(aptr - sptr);
                    /* If field width > 4000, chunk the data */
                    if (i >= HTBUF_ENV_MAX)
                    {
                        int   ncbytes = 0;
                        int   nchunks = 0;
                        int   chunklen;
                        char *pname, *pchunk;
                        int   fnamlen = str_length(fldname);

                        while (ncbytes < i)
                        {
                            /*
                            ** Allocate and construct parameter name
                            ** of the form "ARG.1", "ARG.2", etc.
                            */
                            pname = (char *)morq_alloc(r, fnamlen + 10, 0);
                            if (!pname)
                            {
                                c->mem_err = fnamlen + 10;
                                return(OCI_SUCCESS);
                            }
                            mem_copy(pname, fldname, fnamlen);
                            pname[fnamlen] = '.';
                            str_itoa(++nchunks, pname + fnamlen + 1);
                            fnamlen = str_length(pname);
                            if (fnamlen > nam_width) nam_width = fnamlen;

                            /*
                            ** Compute size of next chunk, allocate,
                            ** and copy data into null-terminated string.
                            */
                            chunklen = (i - ncbytes);
                            if (chunklen >= HTBUF_ENV_MAX)
                            {
                                chunklen = nls_length(cs_id, sptr + ncbytes,
                                                      HTBUF_ENV_MAX - 1);
                            }
                            pchunk = (char *)morq_alloc(r, chunklen + 1, 0);
                            if (!pchunk)
                            {
                                c->mem_err = chunklen + 1;
                                return(OCI_SUCCESS);
                            }
                            mem_copy(pchunk, sptr + ncbytes, chunklen);
                            pchunk[chunklen] = '\0';
                            ncbytes += chunklen;

                            if (nparams < HTBUF_ARRAY_SIZE)
                            {
                                param_name[nparams] = pname;
                                param_value[nparams] = pchunk;
                                if (chunklen > val_width) val_width = chunklen;
                                ++nparams;
                            }
                        }
                    }
                    /* Else add simple scalar parameter */
                    else if (nparams < HTBUF_ARRAY_SIZE)
                    {
                        param_name[nparams] = fldname;
                        param_value[nparams] = (char *)morq_alloc(r, i + 1, 0);
                        if (!param_value[nparams])
                        {
                            c->mem_err = i + 1;
                            return(OCI_SUCCESS);
                        }
                        mem_copy(param_value[nparams], sptr, i);
                        param_value[nparams][i] = '\0';
                        if (i > val_width) val_width = i;
                        ++nparams;
                    }
                    sptr = aptr;
                    j = n - j - i;
                }
                else
                {
                    /* Commence a file upload */
                    param_name[1] = fldname;
                    param_value[1] = filpath;

                    if (long_flag == 2)
                    {
                        status = getfile(r, c, stmt, first_flag,
                                         nparams, param_name, param_value,
                                         nam_width, val_width, pmimetype,
                                         &tmpbuf, &tmpsize, outbuf, ret_file);
                        if (c->mem_err > 0) return(status);
                        lob_flag = OCI_ONE_PIECE;
                        first_flag = 0;
                        bin_flag = 1;
#ifndef NO_FILE_CACHE
                        /* Open/Create file for writing */
                        /* ### Need to check directory path
                           ### and create directories if necessary */
                        fp = os_nullfilehand;
                        if ((status == OCI_SUCCESS) && (*ret_file))
                        {
                            str_prepend(ret_file, octx->doc_file);
                            if (!owa_create_path(ret_file, tempname))
                                fp = file_open_temp(ret_file, tempname,
                                                    HTBUF_HEADER_MAX);
                        }
#endif
                    }
                    else if (long_flag)
                    {
                        status = getcursor(r, c, stmt, first_flag,
                                           nparams, param_name, param_value,
                                           nam_width, val_width, pmimetype,
                                           &tmpbuf, &tmpsize, outbuf,
                                           ret_sql, ret_bind, &bin_flag);
                        if (c->mem_err > 0) return(status);
                        lob_flag = OCI_ONE_PIECE;
                        first_flag = 0;
                    }
                    else
                    {
                        status = getlob(r, c, stmt, nargs, first_flag,
                                        nparams, param_name, param_value,
                                        nam_width, val_width, pmimetype,
                                        &tmpbuf, &tmpsize, outbuf + n);
                        if (status == OCI_SUCCESS)
                        {
                            if (c->mem_err > 0) return(status);

                            if (c->blob_ind != (ub2)-1)
                            {
                                plob = c->pblob;
                                lob_flag = OCI_ONE_PIECE;
                                bin_flag = 1;
                            }
                            else if (c->clob_ind != (ub2)-1)
                            {
                                plob = c->pclob;
                                lob_flag = OCI_ONE_PIECE;
                                bin_flag = 0;
                            }
                            else if (c->nlob_ind != (ub2)-1)
                            {
                                plob = c->pnlob;
                                lob_flag = OCI_ONE_PIECE;
                                bin_flag = 0;
                            }
                            first_flag = 0;
                        }
                    }

                    /* else LOB operation failed, skip this content */
                    j = n - j;
                }
            }

            /* Shift buffer and continue */
            for (i = 0; i < j; ++i) outbuf[i] = sptr[i];
        }
        else
        {
            i = 0;
        }
    }

max_err:
    /* Final call to LOB writer to generate return page */
    param_name[1] = (char *)"";
    param_value[1] = (char *)"";
    *pmimetype = '\0';

    /*
    ** Add arguments for file sizes
    */
    aptr = (char *)"content_length";
    n = str_length(aptr);
    if (n > nam_width) nam_width = n;
    n = 16;
    if (n > val_width) val_width = n;
    sptr = (char *)morq_alloc(r, n * nfiles, 0);
    if (sptr)
      for (i = 0; i < nfiles; ++i)
        if (nparams < HTBUF_ARRAY_SIZE)
        {
            param_name[nparams] = aptr;
            param_value[nparams] = sptr;
            str_ltoa(file_sizes[i], sptr);
            sptr += n;
            ++nparams;
        }

    if (long_flag == 2)
    {
        status = getfile(r, c, stmt, first_flag,
                         nparams, param_name, param_value,
                         nam_width, val_width, pmimetype,
                         &tmpbuf, &tmpsize, outbuf, ret_file);
    }
    else if (long_flag)
        status = getcursor(r, c, stmt, first_flag,
                           nparams, param_name, param_value,
                           nam_width, val_width, pmimetype,
                           &tmpbuf, &tmpsize, outbuf,
                           ret_sql, ret_bind, &bin_flag);
    else
        status = getlob(r, c, stmt, nargs, first_flag,
                        nparams, param_name, param_value,
                        nam_width, val_width, pmimetype,
                        &tmpbuf, &tmpsize, outbuf);

wrterror:
    if (status != OCI_SUCCESS)
    {
        oerrno = sql_get_error(c); 
        if (oerrno) status = oerrno;
        sql_transact(c, 1); /* Rollback */
    }
    else
    {
        c->lastsql = (char *)0;
    }

    return(status);
}

/*
** Streaming range transfer, returning new range length
*/
static void transfer_ranges(request_rec *r, char *outbuf,
                            long_64 offset, long_64 nbytes,
                            long_64 *rlen, long_64 *roff)
{
    long_64 range_length = *rlen;
    long_64 range_offset = *roff;
    long_64 range_start;
    long_64 range_nbytes;

    /*
    ** For range transfers, continue looping until either the
    ** range_length goes negative (indicating that there are no
    ** more ranges to transfer) or the range_offset is beyond
    ** the end of the current block.
    */
    while ((range_length >= 0) && (range_offset < (offset + nbytes)))
    {
        /*
        ** This should never happen, unless the ranges are out of
        ** order.  Since it's too late to transfer this range,
        ** mark it as done and get the next range.
        */
        if (range_offset < offset) range_length = 0;

        if (range_length > 0)
        {
            /*
            ** See if the bytes of this buffer overlap with the current
            ** range.  If so, compute how many bytes of the current block
            ** can be used to satisfy the range, transmit them, and
            ** subtract them.
            */
            range_start = range_offset - offset;
            range_nbytes = nbytes - range_start;
            if (range_nbytes > range_length) range_nbytes = range_length;
            morq_write(r, outbuf + range_start, (long)range_nbytes);
            range_length -= range_nbytes;
            range_offset += range_nbytes;
        }
        /*
        ** If range length is zero bytes, it's time to get the next
        ** range, which might also be inside this buffered region.
        */
        if (range_length == 0)
            if (!morq_get_range(r, &range_offset, &range_length))
                range_length = -1;
    }
    *rlen = range_length;
    *roff = range_offset;
}

/*
** Parse mimetype buffer for additional header elements
*/
static void parse_mime(owa_context *octx, request_rec *r, char *pmimetype)
{
    char *sptr, *eptr, *headval;

    sptr = str_char(pmimetype, '\n', 0);
    if (!sptr) return;
    *(sptr++) = '\0'; /* Null-terminate after mime type */
    while (*sptr)
    {
        headval = sptr;
        eptr = str_char(headval, '\n', 0);
        if (eptr) *eptr = '\0';
        while (*sptr != '\0')
        {
            if (*sptr == ':')
            {
                *(sptr++) = '\0';
                while (*sptr == ' ') ++sptr;
                break;
            }
            else
                ++sptr;
        }
        /* ### No need to sanitize because it was already split at newline */
        morq_table_put(r, OWA_TABLE_HEADOUT, 0, headval, sptr);
        sptr = (eptr) ? eptr + 1 : (char *)"";
    }
}

/*
** Read LOB data and download file
*/
int owa_readlob(connection *c, owa_context *octx, request_rec *r,
                char *fpath, char *pmimetype, char *physical, char *outbuf)
{
    sword          status;
    sb4            oerrno;
    ub4            nbytes;
    ub4            amount;
    long_64        total = 0;
    long_64        offset;
    ub4            buflen = (ub4)HTBUF_BLOCK_READ;
    ub2            cs_id;
    int            bin_flag;
    int            last_flag;
    ub2            n;
    int            i;
    char          *sptr;
    ub2           *unistr;
    OCILobLocator *plob;
    int            range_flag = 0;
    long_64        range_offset = 0;
    long_64        range_length = -1;
#ifdef OVERSIZED_LOBS
    int            oversized_lob = 0;
    ub1            piece = OCI_FIRST_PIECE;
#endif
#ifndef NO_FILE_CACHE
    char          *tempname = (char *)0;
    os_objhand     fp = os_nullfilehand;
#endif

    if (c->blob_ind != (ub2)-1)
    {
        plob = c->pblob;
        bin_flag = 1;
    }
    else if (c->clob_ind != (ub2)-1)
    {
        plob = c->pclob;
        bin_flag = 0;
    }
    else if (c->nlob_ind != (ub2)-1)
    {
        plob = c->pnlob;
        bin_flag = 0;
    }
    else if (c->bfile_ind != (ub2)-1)
    {
        plob = c->pbfile;
        bin_flag = 1;
    }
    else
    {
        /* Nothing to do - send header and exit */
        morq_send_header(r);
        return(OCI_SUCCESS);
    }

    /*
    ** Default character-set conversions for CLOBs and NCLOBs:
    */
    cs_id = (ub2)0;
    if ((octx->dad_csid) && (!bin_flag))
    {
        cs_id = nls_csid(octx->dad_csid);
#ifdef COUNT_CHARACTERS
        if (nls_cstype(cs_id) < 0)
        {
            /*
            ** Multibyte character sets not known to count_chars
            ** are fetched as UCS2 and stripped to ASCII.
            **
            ** ### THIS CODE IS NO LONGER USED BECAUSE WE USE LOB STREAMING
            ** ### TO AVOID THE NEED TO COUNT CHARACTERS DURING READS.
            */
            cs_id = (ub2)OCI_UCS2ID;
        }
#endif
    }

    /* Treat single-byte character sets as if they were binary */
    if (nls_cstype(nls_csid(octx->dad_csid)) == 0) bin_flag = 1;

#ifdef OVERSIZED_LOBS
    if (sizeof(size_t) == sizeof(long_64))
    {
      oraub8 utotal;
      status = OCILobGetLength2(c->svchp, c->errhp, plob, &utotal);
      if (status != OCI_SUCCESS) goto readerr;
      total = (long_64)utotal;
      oversized_lob = (total > LONG_MAXSZ);
    }
    else
#endif /* The original code can't support a LOB > 2G */
    {
      ub4 utotal;
      status = OCILobGetLength(c->svchp, c->errhp, plob, &utotal);
      if (status != OCI_SUCCESS) goto readerr;
      total = (long_64)utotal;
    }

    status = OCILobOpen(c->svchp, c->errhp, plob, OCI_LOB_READONLY);
    if (status != OCI_SUCCESS) goto readerr;

    /*
    ** Set header Content-Type and status (unless called from owa_getpage)
    */
    if ((fpath) && (pmimetype))
    {
      if (fpath != pmimetype)
      {
        /* Parse mimetype buffer for additional header elements */
        parse_mime(octx, r, pmimetype);

        sptr = pmimetype;
        if ((physical) && (*sptr))
        {
            sptr += (str_length(sptr) + 1);
            *sptr = '\0';
        }
        util_set_mime(fpath, sptr, bin_flag);
        if (sptr > pmimetype)
        {
          i = str_length(sptr);
          if (str_compare(sptr, pmimetype, i, 1) ||
              ((pmimetype[i] > ' ') && (pmimetype[i] != ';')))
            physical = (char *)0;
        }
        if ((!bin_flag) && (octx->dad_csid) &&
            (!str_substr(pmimetype, "charset=", 1)))
        {
            i = str_length(pmimetype);
            i = str_concat(pmimetype, i, "; charset=", HTBUF_LINE_LENGTH-1);
            str_concat(pmimetype, i, nls_iana(octx->dad_csid),
                       HTBUF_LINE_LENGTH - 1);
        }
      }

      /*
      ** Set up the return header
      */
      morq_set_mimetype(r, pmimetype);
    }

    /*
    ** Content-Length returned only for binary LOBs; for character
    ** LOBs, the value of total indicates the number of characters,
    ** not bytes, so only return it if the DB character-set is single-byte.
    */
    if (bin_flag)
    {
        if (mowa_check_keepalive(octx->keepalive_flag))
        {
            morq_set_length(r, (size_t)total, 0);
            range_flag = morq_check_range(r);
            if (range_flag) range_length = 0;
        }
    }

    /*
    ** Send the header
    */
    morq_send_header(r);

    /*
    ** For range-based transfers from binary or single-byte data,
    ** transfer LOB data using seek-like semantics (for performance).
    ** This works only if you assume bytes == characters for the
    ** return from OCILobRead.  Otherwise, the more complex
    ** streaming version of the code must be used.
    **
    ** ### Range transfers not supported on oversized LOBs.
    */
    if ((bin_flag) && (range_flag))
    {
        while (morq_get_range(r, &range_offset, &range_length))
        {
            offset = range_offset + 1;
            for (total = range_length;
                 total > 0;
                 total -= (long_64)nbytes)
            {
                nbytes = buflen;
                nbytes = ((long_64)buflen > total) ? (ub4)total : buflen;
                status = OCILobRead(c->svchp, c->errhp, plob,
                                    &nbytes, (ub4)offset,
                                    (dvoid *)outbuf, buflen,
                                    (dvoid *)0, NULL, cs_id, (ub1)0);
                if (status == NEED_READ_DATA) status = OCI_SUCCESS;
                if (status != OCI_SUCCESS) goto readerr;
                if (nbytes == 0) break; /* ### SOME SORT OF ERROR ### */
                morq_write(r, outbuf, (long)nbytes);
                offset += (long_64)nbytes;
            }
        }
        goto closelob;
    }

#ifndef NO_FILE_CACHE
    if ((physical) && (total <= CACHE_MAX_SIZE))
    {
        tempname = outbuf + HTBUF_BLOCK_READ;
        owa_create_path(physical, tempname);
        fp = file_open_temp(physical, tempname, HTBUF_HEADER_MAX);
    }
#endif

    offset = 0;
    last_flag = 0;

    while (total > 0)
    {
        if ((bin_flag) && ((long_64)buflen > total)) buflen = (ub4)total;
        /*
        ** Read data in LOB streaming mode
        ** For character LOBS, this does on-the-fly character set conversion
        ** to the client character set. The chunks returned are measured
        ** in bytes, not characters, and only complete characters
        ** are returned in each chunk. For binary LOBs, it's just chunks
        ** of raw data. Note that the offset is always 1 because it's
        ** relative to the current location within the stream.
        */
        nbytes = 0; /* Read data in LOB streaming mode */

#ifdef OVERSIZED_LOBS
        if (oversized_lob)
        {
          oraub8 byte_amt = nbytes;
          status = OCILobRead2(c->svchp, c->errhp, plob,
                               &byte_amt, (oraub8 *)0, (oraub8)1,
                               (dvoid *)outbuf, (oraub8)buflen,
                               piece, (dvoid *)0, NULL, cs_id, (ub1)0);
          piece = OCI_NEXT_PIECE;
          nbytes = (ub4)byte_amt;
        }
        else /* Original code capable of only 2G reads */
#endif
        {
          status = OCILobRead(c->svchp, c->errhp, plob,
                              &nbytes, (ub4)1, (dvoid *)outbuf, buflen,
                              (dvoid *)0, NULL, cs_id, (ub1)0);
        }

        if (status == NEED_READ_DATA) status = OCI_SUCCESS;
        else                          last_flag = 1;
        if (status != OCI_SUCCESS) goto readerr;
        if (nbytes == 0) break; /* ### SOME SORT OF ERROR ### */

        if (cs_id == OCI_UCS2ID)
        {
            amount = nbytes; /* UCS2 returns character count */
            nbytes <<= 1;
        }
#ifdef COUNT_CHARACTERS
        else if (nls_cstype(cs_id))
            amount = (ub4)nls_count_chars((int)cs_id, outbuf, &nbytes);
#endif
        else
            amount = nbytes;

        if (bin_flag) total -= (long_64)amount;

        /*
        ** ### UCS2 fetch is used for all multi-byte client character
        ** ### sets because there's no other way to measure the return
        ** ### lengths in characters.  After the transfer, the UCS2
        ** ### string is stripped to plain ASCII.
        */
        if (cs_id == OCI_UCS2ID)
        {
            unistr = (ub2 *)(dvoid *)outbuf;
            sptr = outbuf;
            for (i = 0; i < (int)amount; ++i)
            {
                n = *(unistr++);
                *(sptr++) = (n < 0x80) ? (char)n : '?';
            }
            nbytes = amount;
        }

#ifndef NO_FILE_CACHE
        if (!InvalidFile(fp)) file_write_data(fp, outbuf, (int)nbytes);
#endif
        if (!range_flag) /* Do normal file download */
            morq_write(r, outbuf, (long)nbytes);
        else             /* Do range transfer */
            transfer_ranges(r, outbuf, offset, (long_64)nbytes,
                            &range_length, &range_offset);

        offset += (long_64)nbytes;
        if (last_flag) total = 0; /* Ensure loop exit */
    }

closelob:
    {
      int is_temp = 0; /* ### Should be of Oracle type "boolean" */
      status = OCILobIsTemporary(c->envhp, c->errhp, plob, &is_temp);
      if (status != OCI_SUCCESS) is_temp = 0;
      if (is_temp) status = OCILobFreeTemporary(c->svchp, c->errhp, plob);
      else         status = OCILobClose(c->svchp, c->errhp, plob);
    }

readerr:
    if (status != OCI_SUCCESS)
    {
        oerrno = sql_get_error(c); 
        if (oerrno) status = oerrno;
        physical = (char *)0; /* Ensure file_move does a delete */
    }
    else
    {
        c->lastsql = (char *)0;
    }

#ifndef NO_FILE_CACHE
    if (!InvalidFile(fp))
    {
        file_close(fp);
        file_move(tempname, physical);
    }
#endif
    return(status);
}

/*
** Read LONG or LONG RAW data and download file
** ### LONG data type can't support > 2G values
*/
int owa_readlong(connection *c, owa_context *octx, request_rec *r,
                 char *fpath, char *pmimetype, char *physical, char *outbuf,
                 int bin_flag)
{
    sword       status;
    sb4         oerrno;
    ub4         nbytes;
    long_64     total = (ub4)0;
    int         slen;
    sb4         clen = (sb4)-1;
    ub2         sql_type;
    char       *sptr;
    char       *stmt;
    char       *sbind;
    char       *piecebuf;
    int         range_flag = 0;
    long_64     range_offset = 0;
    long_64     range_length = -1;
#ifndef NO_FILE_CACHE
    char       *tempname = (char *)0;
    os_objhand  fp = os_nullfilehand;
#endif

    if (bin_flag)
        sql_type = SQLT_LBI; /* SQLT_LVB is 4-byte leading long raw */
                             /* Another possibility is SQLT_BIN */
    else
        sql_type = SQLT_LNG; /* SQLT_LVC is 4-byte leading long varchar */
                             /* Other possibilities are SQLT_CHR, SQLT_STR */

    /* Parse mimetype buffer for additional header elements */
    parse_mime(octx, r, pmimetype);

    sptr = pmimetype;
    if ((physical) && (*sptr))
    {
        sptr += (str_length(sptr) + 1);
        *sptr = '\0';
    }
    util_set_mime(fpath, sptr, bin_flag);
    if (sptr > pmimetype)
      if (str_compare(sptr, pmimetype, -1, 1))
        physical = (char *)0;

    if (!bin_flag)
    {
         if ((octx->dad_csid) &&
             (!str_substr(pmimetype, "charset=", 1)))
         {
             slen = str_length(pmimetype);
             slen = str_concat(pmimetype, slen,
                               "; charset=", HTBUF_LINE_LENGTH - 1);
             str_concat(pmimetype, slen, nls_iana(octx->dad_csid),
                        HTBUF_LINE_LENGTH - 1);
         }
         if (!nls_cstype(octx->dad_csid)) bin_flag = 1;
    }

    stmt = outbuf;
    sbind = outbuf + HTBUF_BLOCK_SIZE;

    if (octx->lontypes == LONG_MODE_RETURN_LEN)
    {
        /* Get content length returned by document procedure */
        sptr = sbind + HTBUF_HEADER_MAX;
        while (*sptr == ' ') ++sptr;
        clen = (sb4)0;
        while ((*sptr >= '0') && (*sptr <= '9'))
            clen = clen * 10 + (*(sptr++) - '0');
    }
    c->lastsql = stmt;
    status = sql_parse(c, c->stmhp3, stmt, -1);
    if (status != OCI_SUCCESS) goto longerr;

    piecebuf = sbind;
    if (*sbind)
    {
        slen = str_length(sbind) + 1;
        status = sql_bind_str(c, c->stmhp3, (ub4)1, sbind, (sb4)slen);
        if (status != OCI_SUCCESS) goto longerr;
        piecebuf += slen;
    }

    status = sql_exec(c, c->stmhp3, (ub4)0, 0);
    if ((status != NEED_READ_DATA) && (status != OCI_SUCCESS)) goto longerr;

    if (octx->lontypes == LONG_MODE_FETCH_LEN)
    {
        status = sql_define(c, c->stmhp3, (ub4)2,
                            (dvoid *)&clen, (sb4)sizeof(clen),
                            (ub2)SQLT_INT, (dvoid *)0);
        if (status != OCI_SUCCESS) goto longerr;
    }

    status = sql_define(c, c->stmhp3, (ub4)1,
                        (dvoid *)0, (sb4)LONG_MAXSZ, sql_type, (dvoid *)0);
    if (status != OCI_SUCCESS) goto longerr;

    status = sql_fetch(c, c->stmhp3, (ub4)1);

    /*
    ** Set up the return header
    */
    morq_set_mimetype(r, pmimetype);

    if ((bin_flag) && (clen >= 0))
        if (mowa_check_keepalive(octx->keepalive_flag))
        {
            morq_set_length(r, (size_t)clen, 0);
            range_flag = morq_check_range(r);
            if (range_flag) range_length = 0;
        }

    morq_send_header(r);

#ifndef NO_FILE_CACHE
    if ((physical) && (clen < CACHE_MAX_SIZE))
    {
        tempname = outbuf + HTBUF_BLOCK_READ;
        fp = file_open_temp(physical, tempname, HTBUF_HEADER_MAX);
    }
#endif

    while (status == NEED_READ_DATA)
    {
        nbytes = HTBUF_BLOCK_READ;

        status = sql_read_piece(c, c->stmhp3, (dvoid *)piecebuf, &nbytes);
        if (status != OCI_SUCCESS) goto longerr;

        status = sql_fetch(c, c->stmhp3, (ub4)1);
        if ((status != NEED_READ_DATA) && (status != OCI_SUCCESS))
            goto longerr;

#ifndef NO_FILE_CACHE
        if (!InvalidFile(fp)) file_write_data(fp, piecebuf, (int)nbytes);
#endif
        if (!range_flag) /* Do normal file download */
            morq_write(r, piecebuf, (long)nbytes);
        else             /* Do range transfer */
            transfer_ranges(r, piecebuf, total, (long_64)nbytes,
                            &range_length, &range_offset);

        total += (long_64)nbytes;
    }

longerr:
    if (status != OCI_SUCCESS)
    {
        oerrno = sql_get_error(c); 
        if (oerrno) status = oerrno;
        physical = (char *)0; /* Ensure file_move does a delete */
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
** Read from document table of the following spec:
**
** <table_name>
**   NAME         varchar2(256) unique not null,
**   MIME_TYPE    varchar2(128)
**   DOC_SIZE     number
**   DAD_CHARSET  varchar2(128)
**   LAST_UPDATED date
**   CONTENT_TYPE varchar2(128)
**   <content_column> BLOB
**
** Returns: pblob handle set to read table.
**
** ### Assumes CONTENT_TYPE = 'B' for BLOB, no other content column
** ### types are supported.
** ###
** ### Legacy table has NAME, MIME_TYPE, DOC_SIZE, and:
** ###    CONTENT      long raw
*/
int owa_rlobtable(connection *c, owa_context *octx, request_rec *r,
                  char *name, char *pmimetype, char *pcdisp, char *outbuf)
{
    sword       status = OCI_SUCCESS;
    sb4         oerrno;
    sb4         nlen;
    int         slen;
    char       *charset;
    char       *stmt = outbuf;

    if ((!(octx->doc_column)) || (!(octx->doc_table)))
        return(status); /* ### Really should be an error ### */

    /* Build the select statement */
    str_copy(stmt, "begin select MIME_TYPE, DAD_CHARSET, ");
    slen = str_length(stmt);
    slen = str_concat(stmt, slen, octx->doc_column, -1);
    slen = str_concat(stmt, slen, " into :B1, :B2, :B3 from ", -1);
    slen = str_concat(stmt, slen, octx->doc_table, -1);
    slen = str_concat(stmt, slen, " where NAME = :B4; end;", -1);

    c->lastsql = stmt;
    status = sql_parse(c, c->stmhp3, stmt, (ub4)slen);
    if (status != OCI_SUCCESS) goto rtaberr;

    nlen = (sb4)util_round((un_long)(str_length(name)+1), octx->scale_round);

    charset = stmt + slen + 1;

    status = sql_bind_str(c, c->stmhp3, (ub4)1,
                          pmimetype, (sb4)HTBUF_HEADER_MAX);
    if (status != OCI_SUCCESS) goto rtaberr;

    status = sql_bind_str(c, c->stmhp3, (ub4)2,
                          charset, (sb4)HTBUF_HEADER_MAX);
    if (status != OCI_SUCCESS) goto rtaberr;

    status = sql_bind_lob(c, c->stmhp3, (ub4)3, SQLT_BLOB);
    if (status != OCI_SUCCESS) goto rtaberr;

    status = sql_bind_str(c, c->stmhp3, (ub4)4, name, nlen);
    if (status != OCI_SUCCESS) goto rtaberr;

    *pmimetype = *charset = '\0';

    status = sql_exec(c, c->stmhp3, (ub4)1, 0);
    if (status != OCI_SUCCESS) goto rtaberr;

    c->lastsql = (char *)0;

    /* Get mimetype and charset */
    util_set_mime(name, pmimetype, 1);
    if (*pmimetype)
    {
        if (*charset)
        {
            slen = str_length(pmimetype);
            slen = str_concat(pmimetype, slen,
                              "; charset=", HTBUF_HEADER_MAX);
            str_concat(pmimetype, slen, charset, HTBUF_HEADER_MAX);
        }
    }

    /* Set the content disposition if it wasn't already done */
    if ((*name) && (!*pcdisp))
    {
        int  slen;
        str_copy(pcdisp, "filename=\"");
        slen = str_length(pcdisp);
        slen = str_concat(pcdisp, slen, name, HTBUF_HEADER_MAX-2);
        pcdisp[slen++] = '"';
        pcdisp[slen] = '\0';
        nls_sanitize_header(pcdisp);
        morq_table_put(r, OWA_TABLE_HEADOUT, 0, "Content-Disposition", pcdisp);
    }

rtaberr:
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

/*
** Write to document table of the following spec:
**
** <table_name>
**   NAME         varchar2(256) unique not null,
**   MIME_TYPE    varchar2(128)
**   DOC_SIZE     number
**   DAD_CHARSET  varchar2(128)
**   LAST_UPDATED date
**   CONTENT_TYPE varchar2(128)
**   <content_column> BLOB
**
** Writes <guid>/<filename> to the NAME column
**
** ### Assumes CONTENT_TYPE = 'B' for BLOB, no other content column
** ### types are supported.
** ###
** ### Legacy table has NAME, MIME_TYPE, DOC_SIZE, and:
** ###    CONTENT      long raw
*/
int owa_wlobtable(connection *c, owa_context *octx, request_rec *r,
                  char *outbuf, file_arg *filelist,
                  char **vals, char **valptrs, char *marker)
{
    sword       status = OCI_SUCCESS;
    sb4         oerrno;
#ifdef OVERSIZED_LOBS
    long_64     fsize;
#else /* Avoid ORA-1460 errors on older OCIs */
    sb4         fsize;
#endif
    int         slen;
    char        mime[HTBUF_HEADER_MAX];
    char        tempname[256 + 1];
    char        charset[128 + 1];
    char       *sptr;
    char       *stmt = outbuf;
    randstate   arand, brand;
    un_long     seed[4];
    int         last_param = -1;
    int         last_idx = -1;

    if ((!(octx->doc_column)) || (!(octx->doc_table)))
        return(status); /* ### Really should be an error ### */

    /* Build the insert statement */
    str_copy(stmt, "begin insert into ");
    slen = str_length(stmt);
    slen = str_concat(stmt, slen, octx->doc_table, -1);
    slen = str_concat(stmt, slen,
                       " (NAME, MIME_TYPE, DOC_SIZE, DAD_CHARSET,"
                        " LAST_UPDATED, CONTENT_TYPE, ", -1);
    slen = str_concat(stmt, slen, octx->doc_column, -1);
    slen = str_concat(stmt, slen,
                       ") values "
                       "(:B1, :B2, :B3, :B4, SYSDATE, 'BLOB', empty_blob())"
                      " returning ", -1);
    slen = str_concat(stmt, slen, octx->doc_column, -1);
    slen = str_concat(stmt, slen, " into :B5; end;", -1);

    c->lastsql = stmt;
    status = sql_parse(c, c->stmhp3, stmt, (ub4)slen);
    if (status != OCI_SUCCESS) goto wtaberr;

    status = sql_bind_str(c, c->stmhp3, (ub4)1, tempname, sizeof(tempname));
    if (status != OCI_SUCCESS) goto wtaberr;

    status = sql_bind_str(c, c->stmhp3, (ub4)2, mime, (sb4)sizeof(mime));
    if (status != OCI_SUCCESS) goto wtaberr;

#ifdef OVERSIZED_LOBS
    status = sql_bind_long(c, c->stmhp3, (ub4)3, &fsize);
#else /* Avoid ORA-1460 errors on older OCIs */
    status = sql_bind_int(c, c->stmhp3, (ub4)3, &fsize);
#endif
    if (status != OCI_SUCCESS) goto wtaberr;

    status = sql_bind_str(c, c->stmhp3, (ub4)4, charset, (sb4)sizeof(charset));
    if (status != OCI_SUCCESS) goto wtaberr;

    status = sql_bind_lob(c, c->stmhp3, (ub4)5, SQLT_BLOB);
    if (status != OCI_SUCCESS) goto wtaberr;

    seed[0] = os_get_pid();
    seed[2] = os_get_tid();
    seed[1] = os_get_time(seed + 3);
    util_init_rand(&arand, &brand, (void *)seed, sizeof(seed));

    while (filelist)
    {
      if (!(filelist->filepath))
        sptr = "";
      else if (filelist->filepath[0] == '\0')
        sptr = "";
      else /* It's a real file being uploaded */
      {
        *mime = *charset = '\0';
#ifdef OVERSIZED_LOBS
        fsize = (long_64)(filelist->total);
#else /* Avoid ORA-1460 errors on older OCIs */
        fsize = (sb4)(filelist->total);
#endif
        if (filelist->content_type)
        {
            slen = str_concat(mime, 0, filelist->content_type, sizeof(mime)-1);
            mime[sizeof(mime)-1] = '\0';
        }
        if (octx->dad_csid)
        {
            sptr = (char *)nls_iana(octx->dad_csid);
            if (sptr) str_copy(charset, sptr);
        }

        /* Fill temporary file name with random prefix */
        str_itox(util_get_rand(&arand, &brand), tempname);
        str_itox(util_get_rand(&arand, &brand), tempname + 8);
        str_itox(util_get_rand(&arand, &brand), tempname + 16);
        str_itox(util_get_rand(&arand, &brand), tempname + 24);
        slen = 32;
        tempname[slen++] = '/';
        /* Append the trailing file name of the inbound path */
        slen = str_concat(tempname, slen,
                          nls_find_delimiter(filelist->filepath,
                                             nls_csid(octx->dad_csid)),
                          sizeof(tempname) - slen - 1);
        tempname[slen++] = '\0';

        sptr = morq_alloc(r, slen, 0);
        if (!sptr)
        {
            sptr = "modowa_malloc_failed";
            str_copy(tempname, sptr);
        }
        else
        {
            mem_copy(sptr, tempname, slen);
        }

        status = sql_exec(c, c->stmhp3, (ub4)1, 0);
        if (status != OCI_SUCCESS) goto wtaberr;

        /* If there's any data for the file, write it to the LOB */
        if (fsize > 0)
        {
          /* If the file length is 0, it has to be read from the request. */
          if (filelist->len == 0)
          {
            int     n;
            ub1     piece_flag = OCI_FIRST_PIECE;
            long    chunksz = HTBUF_BLOCK_SIZE;
            long_64 position = (long_64)1;
            long_64 bytes_remaining = (long_64)(filelist->total);

            /*
            ** Streaming reads from the request input stream
            ** and streaming writes to the LOB
            */
            while (bytes_remaining > 0)
            {
              if ((long_64)chunksz > bytes_remaining)
                chunksz = (long)bytes_remaining;
              n = morq_read(r, outbuf, chunksz);
              if (n <= 0) break; /* ### Should be a hard error ### */

              bytes_remaining -= (long_64)n;

              if (bytes_remaining == 0)
              {
                if (piece_flag == OCI_FIRST_PIECE)
                  piece_flag = OCI_ONE_PIECE;
                else
                  piece_flag = OCI_LAST_PIECE;
              }

              {
#ifdef OVERSIZED_LOBS
                oraub8 bamt = (oraub8)n;
                oraub8 offset = (oraub8)position;
                status = OCILobWrite2(c->svchp, c->errhp, c->pblob,
                                      &bamt, (oraub8 *)0, offset,
                                      outbuf, bamt, piece_flag,
                                      (dvoid *)0, NULL, c->csid, (ub1)0);
#else
                ub4 bamt = (ub4)n;
                ub4 offset = (ub4)position;
                status = OCILobWrite(c->svchp, c->errhp, c->pblob,
                                     &bamt, offset, outbuf, bamt, piece_flag,
                                     (dvoid *)0, NULL, c->csid, (ub1)0);
#endif

                if (status != OCI_SUCCESS) break;
              }

              piece_flag = OCI_NEXT_PIECE;

              position += (long_64)n;
            }
          }
          /* Write data to the open lob in a single piece */
#ifdef OVERSIZED_LOBS
          else if (fsize > LONG_MAXSZ)
          {
            oraub8 lsize = (oraub8)fsize;

            status = OCILobWrite2(c->svchp, c->errhp, c->pblob,
                                  &lsize, (oraub8 *)0, (oraub8)1,
                                  filelist->data, lsize,
                                  (ub1)OCI_ONE_PIECE, (dvoid *)0, NULL,
                                  c->csid, (ub1)0);
          }
#endif
          else /* Original code limited to 2G bytes */
          {
            ub4 lsize = (ub4)fsize;

            status = OCILobWrite(c->svchp, c->errhp, c->pblob,
                                 &lsize, 1, filelist->data, lsize,
                                 (ub1)OCI_ONE_PIECE, (dvoid *)0,
                                 NULL, c->csid, (ub1)0);
          }

          if (status != OCI_SUCCESS) goto wtaberr;
        }
      }

      /* Do fixup on the parameters bound to PL/SQL */
      if (filelist->param_num != last_param)
      {
        last_param = filelist->param_num;
        last_idx = 0;
      }

      if (valptrs)
          valptrs[last_param] = sptr;
      else if (vals[last_param] == marker)
          vals[last_param] = sptr;
      else if (vals[last_param])
      {
          mem_copy(vals[last_param] + last_idx * HTBUF_LINE_CHARS,
                   sptr, str_length(sptr) + 1);
          ++last_idx;
      }
      /* ### Else unclear what to do ### */

      filelist = filelist->next;
    }

    c->lastsql = (char *)0;

wtaberr:
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
