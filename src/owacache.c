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
** File:         owacache.c
** Description:  Memory and file-system cache routines.
** Author:       Doug McMahon      Doug.McMahon@oracle.com
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
**
** History
**
** 08/27/2000   D. McMahon      Created
** 05/31/2001   D. McMahon      Began revisioning with 1.3.9
** 06/25/2002   D. McMahon      Fix tree-walking code to ignore "." and ".."
** 08/15/2002   D. McMahon      Moved owa_pool_purge from owahand.c
** 01/19/2004   D. McMahon      Update owa_pool_purge for sessioning
** 09/30/2006   D. McMahon      Protect owa_pool_purge from exit processing
** 04/30/2010   D. McMahon      (long) casts for morq_read and morq_write
** 11/27/2012   D. McMahon      GCC fixes, Win-64 porting
** 09/19/2013   D. McMahon      64-bit content lengths
** 05/07/2015   D. McMahon      Make morq_get_range use 64-bit ints
*/

#define WITH_OCI
#define APACHE_LINKAGE
#include <modowa.h>

/*
** Keep global statistics for multi-process configurations
*/
#define KEEP_GLOBAL_STATS

typedef struct pool_record
{
    int pid;                       /* Process ID */
    int location;                  /* Offset to location name string */
    ub1 poolstats[C_LOCK_MAXIMUM]; /* ### 0-255, NOT QUITE 256 ### */
} pool_record;

#ifndef NO_FILE_CACHE
/*
** Search the alias table for a key/value pair that matches the path.
** If found, builds a physical name with the path prefix substituted
** and (on Win32) with any "/" characters converted to "\".
*/
char *owa_map_cache(owa_context *octx, request_rec *r,
                    char *fpath, ub4 *life)
{
    char *key;
    char *val;
    char *physical = (char *)0;
    int   plen;
    int   klen;
    int   vlen;
    int   i, j;

    if (octx->nlifes > 0)
    {
        plen = str_length(fpath);
        for (i = 0; i < octx->nlifes; ++i)
        {
            *life = (ub4)(octx->lifes[i].lifespan);
            key = octx->lifes[i].logname;
            klen = str_length(key);
            if (klen < plen)
            {
                if ((fpath[klen] == '/') &&
                    (!str_compare(key, fpath, klen, 0)))
                {
                    val = octx->lifes[i].physname;
                    vlen = str_length(val);
                    plen = plen + vlen - klen;
                    physical = (char *)morq_alloc(r, (size_t)(plen + 1), 0);
                    if (physical)
                    {
                        for (j = 0; j < vlen; ++j)
                            physical[j] = *(val++);
                        physical[j++] = os_dir_separator;
                        for (val = fpath + klen + 1; j < plen; ++j)
                        {
                            physical[j] = *(val++);
#ifdef MODOWA_WINDOWS
                            if (physical[j] == '/')
                                physical[j] = os_dir_separator;
#endif
                        }
                        physical[j] = '\0';
                    }
                    break;
                }
            }
        }
    }
    return(physical);
}

/*
** Append file name to path
*/
static int dir_concat(char *fpath, char *fname, int slen)
{
    if (slen < (HTBUF_HEADER_MAX - 1)) fpath[slen++] = os_dir_separator;
    while (*fname)
    {
        if (slen >= (HTBUF_HEADER_MAX - 1)) return(0);
        fpath[slen++] = *(fname++);
    }
    fpath[slen] = '\0';
    return(slen);
}

/*
** Purge old files from a directory (and all decendants)
*/
static void purge_directory(char *fpath,
                            un_long life, un_long curtime, un_long interval)
{
    un_long     fage;
    un_long     rtime;
    int         slen;
    dir_record  dir;

    slen = str_length(fpath);
    dir.dh = os_nulldirhand;
    while (file_readdir(fpath, &dir))
        if (dir.dir_flag)
        {
            /* Ignore "." and ".." */
            if ((dir.fname[0] == '.') &&
                ((dir.fname[1] == '\0') ||
                 ((dir.fname[1] == '.') && (dir.fname[2] == '\0'))))
                continue;

            if (dir_concat(fpath, dir.fname, slen))
                purge_directory(fpath, life, curtime, interval);
            fpath[slen] = '\0';
        }
        else if (dir.attrs == 0)
        {
            /*
            ** Compute age, but allow 1 second/Kbyte in case a
            ** download was in progress when we started cleanup.
            */
            rtime = (dir.fsize + 1023) >> 10;
            fage = (curtime - dir.ftime);
            if (fage > rtime) fage -= rtime;
            else              fage = 0;
            if ((fage > (un_long)interval) && (fage > life))
            {
                if (dir_concat(fpath, dir.fname, slen)) file_delete(fpath);
                fpath[slen] = '\0';
            }
        }
}

/*
** Iterate through file system cache directories and age out files
*/
void owa_file_purge(owa_context *octx, int interval)
{
    char        fpath[HTBUF_HEADER_MAX];
    un_long     life;
    int         i;
    un_long     curtime;

    curtime = os_get_time((un_long *)0);

    for (i = 0; i < octx->nlifes; ++i)
    {
        life = octx->lifes[i].lifespan;
        if (life > 0)
        {
            str_concat(fpath, 0, octx->lifes[i].physname, sizeof(fpath) - 1);
            purge_directory(fpath, life, curtime, interval);
        }
    }
}

/*
** Download directly from a flat-file
*/
int owa_download_file(owa_context *octx, request_rec *r,
                      char *fpath, char *pmimetype, ub4 life, char *outbuf)
{
    os_objhand  fp;
    un_long     fage;
    un_long     clen, ctot;
    un_long     nbytes;
    int         i;
    int         status = 0;
    os_objhand  hnd = os_nullfilehand;
    void       *ptr = (void *)0;
    int         range_flag = 0;
    long_64     range_offset;
    long_64     range_length;

    fp = file_open_read(fpath, &clen, &fage);
    if (InvalidFile(fp)) goto down_err;
    if ((life != (ub4)0) && (fage > life)) goto down_err;
    if (clen <= (un_long)CACHE_MAX_SIZE)
    {
        hnd = file_map(fp, clen, (char *)0, 0);
        ptr = file_view(hnd, clen, 0);
    }

    /*
    ** If the mime-type is not specified by the caller,
    ** then assume it can be determined from the file extension
    */
    if (*pmimetype == '\0') util_set_mime(fpath, pmimetype, 1);

    /*
    ** Add the character set to the Content-Type
    ** ### SHOULD THIS BE OMITTED FOR BINARY DONWLOADS? ###
    */
    i = str_length(pmimetype);
    i = str_concat(pmimetype, i, "; charset=", HTBUF_LINE_LENGTH - 1);
    str_concat(pmimetype, i, nls_iana(octx->dad_csid),
               HTBUF_LINE_LENGTH - 1);

    /*
    ** Set header Status and Content-Type
    */
    morq_set_mimetype(r, pmimetype);

    /*
    ** Set Content-Length (if available)
    */
    if (clen > 0)
        if (mowa_check_keepalive(octx->keepalive_flag))
        {
            morq_set_length(r, (size_t)clen, 0);
            range_flag = morq_check_range(r);
        }

    /*
    ** Send the header
    */
    morq_send_header(r);

    /*
    ** If requested, perform a range-based transfer
    */
    if (range_flag)
    {
        clen = 0;
        while (morq_get_range(r, &range_offset, &range_length))
        {
            /* ### Range length limited to 2G ### */
            if (ptr)
                morq_write(r, (char *)ptr + range_offset, (long)range_length);
            else
            {
                file_seek(fp, (long)range_offset);
                for (ctot = (un_long)range_length; ctot > 0; ctot -= nbytes)
                {
                    nbytes = HTBUF_BLOCK_READ;
                    if (nbytes > ctot) nbytes = ctot;
                    i = file_read_data(fp, outbuf, (int)nbytes);
                    if (i != (int)nbytes) goto down_err;
                    morq_write(r, outbuf, (long)nbytes);
                }
            }
            clen += (un_long)range_length;
            if (octx->diagflag & DIAG_RESPONSE)
            {
                debug_out(octx->diagfile,
                          "Range transfer of %d bytes from %d\n",
                          (char *)0, (char *)0,
                          (int)range_length, (int)range_offset);
            }
        }
    }
    /*
    ** Transfer the file contents using memory-mapped buffer
    */
    else if (ptr)
        morq_write(r, (char *)ptr, (long)clen);
    /*
    ** Transfer the file contents using a series of buffered reads
    */
    else
    {
        for (ctot = clen; ctot > 0; ctot -= nbytes)
        {
            nbytes = HTBUF_BLOCK_READ;
            if (nbytes > ctot) nbytes = ctot;
            i = file_read_data(fp, outbuf, (int)nbytes);
            if (i != (int)nbytes) goto down_err;
            morq_write(r, outbuf, (long)nbytes);
        }
    }

    if (octx->diagflag & DIAG_RESPONSE)
        debug_out(octx->diagfile,
                  "Transferred %d bytes from cached file [%s]\n",
                  fpath, (char *)0, clen, 0);

    status = 1;

down_err:
    file_unmap(hnd, ptr, clen);
    file_close(fp);
    return(status);
}
#endif


/*
** Parse the given path and ensure all directory elements in it exist,
** creating directories as necessary.
*/
int owa_create_path(char *fpath, char *tempbuf)
{
#ifndef NO_FILE_CACHE
    char *sptr;
    int   i;

    sptr = fpath;
    if (*sptr == os_dir_separator) ++sptr;

    while (sptr)
    {
        sptr = str_char(sptr, os_dir_separator, 0);
        if (sptr)
        {
            i = (int)((sptr++) - fpath);
            str_concat(tempbuf, 0, fpath, i);
            if (!file_mkdir(tempbuf, 0700)) return(1);
        }
    }
    return(0);
#else
    return(1);
#endif
}

/*
** Iterate through file system cache directories and age out files
*/
void owa_file_show(owa_context *octx, request_rec *r)
{
#ifndef NO_FILE_CACHE
    char       *fpath;
    char       *lpath;
    un_long     life;
    int         i;
    dir_record  dir;
    char        tbuf[LONG_MAXSTRLEN];

    morq_write(r, "<table cellspacing=\"2\" cellpadding=\"2\""
                  " border=\"0\">\n", -1);
    for (i = 0; i < octx->nlifes; ++i)
    {
        life = octx->lifes[i].lifespan;
        if (life > 0)
        {
            lpath = octx->lifes[i].logname;
            fpath = octx->lifes[i].physname;
            dir.dh = os_nulldirhand;
            morq_write(r, "<tr valign=\"top\">\n", -1);
            morq_print_str(r, "<td colspan=\"2\"><b>%s</b></td>",
                           lpath);
            morq_print_str(r, "<td colspan=\"4\">(%s)</td></tr>\n",
                           fpath);
            while (file_readdir(fpath, &dir))
                if (!(dir.dir_flag) && (dir.attrs == 0))
                {
                    util_print_time(dir.ftime, tbuf, (un_long *)0);
                    morq_write(r, "<tr valign=\"top\">\n", -1);
                    morq_print_str(r, "<td>&nbsp;</td><td>%s</td>\n",
                                   dir.fname);
                    morq_print_int(r, "<td>&nbsp;</td><td>%d</td>\n",
                                   dir.fsize);
                    morq_print_str(r, "  <td>&nbsp;</td><td>%s</td></tr>\n",
                                   tbuf);
                }
        }
    }
    morq_write(r, "</table>\n", -1);
#endif
}

/*
** Initialize shared memory segment
*/
un_long owa_shmem_init(shm_context *map)
{
    un_long      sz = 0;
#ifndef NO_FILE_CACHE
    size_t       thresh;
    os_objhand   hnd;
    pool_record *prec;

    /*
    ** Initialize global shared memory segment
    ** ### Will these objects be inherited by all the workers?
    */
    if ((map->mapsize > 0) && (InvalidFile(map->f_mutex)))
    {
        sz = os_page_round((un_long)1);
        map->pagesize = sz;
        sz = os_page_round(sz + sz + (un_long)(map->mapsize));
        map->f_mutex = os_sem_create((char *)0, 1, 0);
        if (!InvalidFile(map->f_mutex))
            if (os_sem_acquire(map->f_mutex, SHMEM_WAIT_INFINITE))
            {
                /* Now, really map it */
                hnd = os_shm_get((char *)0, sz);
                if (!InvalidFile(hnd))
                {
                    map->map_fp = hnd;
                    map->map_ptr = os_shm_attach(hnd, sz);
                }
                if (map->map_ptr)
                {
                    /* ### HACK: initialize memory on the first call ### */
                    prec = (pool_record *)(map->map_ptr);
                    prec->pid = 0;
                    prec->location = -1;
                    ((char *)(map->map_ptr))[map->pagesize] = '\0';
#ifndef NO_MARK_FOR_DESTRUCT
                    /* Mark memory for destruction when last process exits */
                    os_shm_destroy(hnd);
#endif
                }
                os_sem_release(map->f_mutex);
            }
        if (map->map_ptr == (void *)0) sz = 0;
        map->mapsize = (size_t)sz;
    }

    /*
    ** Memory cache threshold must be <= mapsize/16.
    ** File cache threshold must be >= memory cache threshold.
    */
    thresh = (size_t)(map->mapsize >> 4);
    if (map->memthresh > thresh) map->memthresh = thresh;
    if (map->filthresh < map->memthresh) map->filthresh = map->memthresh;
#endif
    return(sz);
}

/*
** Update global pool statistics
*/
void owa_shmem_update(shm_context *map, int *mapoff, int realpid,
                      char *location, int *poolstats)
{
#ifdef KEEP_GLOBAL_STATS
    volatile pool_record *prec;
    int          llen;
    int          i;
    char        *lname;
    int          locidx;
    int          maxrecs;
    int          recnum;
    int          empty_flag;

    if (map)
      if (map->map_ptr)
      {
          prec = (pool_record *)(map->map_ptr);
          lname = (char *)(map->map_ptr) + map->pagesize;

          /* Check to see if the pool is entirely unused */
          empty_flag = 1;
          for (i = 0; i < C_LOCK_MAXIMUM; ++i)
              if ((i > 0) && (poolstats[i] != 0)) empty_flag = 0;

          if (*mapoff >= 0)
          {
              prec += *mapoff;
              if (!str_compare(location, lname + prec->location, -1, 0))
              {
                if (prec->pid == realpid)
                {
                  for (i = 0; i < C_LOCK_MAXIMUM; ++i)
                      prec->poolstats[i] = (ub1)(poolstats[i] & 0xFF);
                  if (empty_flag)
                  {
                      *mapoff = -1;
                      prec->pid = 0;
                      prec->location = 0;
                  }
                  return;
                }
              }
              prec -= *mapoff;
              *mapoff = -1;
          }

          /*
          ** First, scan for a match to location string;
          ** if not found, get the latch and add it to the list.
          */
          locidx = 0;
          while (lname[locidx] != '\0')
          {
              llen = str_length(lname + locidx) + 1;
              if (!str_compare(location, lname + locidx, -1, 0)) break;
              locidx += llen;
          }
          if (lname[locidx] == '\0')
          {
              /* ### IF THE MUTEX IS UNAVAILABLE, ABORT ### */
              if (!os_sem_acquire(map->f_mutex, SHMEM_WAIT_MAX)) return;
              while (lname[locidx] != '\0')
              {
                  llen = str_length(lname + locidx) + 1;
                  if (!str_compare(location, lname + locidx, -1, 0)) break;
                  locidx += llen;
              }
              if (lname[locidx] == '\0')
              {
                  llen = str_length(location) + 1;
                  if ((map->pagesize - prec->location) > llen)
                  {
                      str_copy(lname + locidx, location);
                      lname[locidx + llen] = '\0';
                  }
              }
              os_sem_release(map->f_mutex);
              /* ### INSUFFICIENT SPACE IN SHARED AREA, ABORT ### */
              if (lname[locidx] == '\0') return;
          }

          maxrecs = (map->pagesize)/sizeof(pool_record);

          /* Scan the list for the slot unique to this PID and location */
          for (recnum = 0; recnum < maxrecs; ++recnum)
          {
              if (prec->pid == 0) break;
              if ((prec->pid == realpid) && (prec->location == locidx)) break;
              ++prec;
          }
          if (recnum == maxrecs) return; /* ### Out of shared memory slots */

          /*
          ** If slot found, update status and return
          ** (but, if empty, mark the slot reusable to handle the
          ** case where a process is shut down -- avoid "leaking"
          ** shared memory slots due to clean shutdowns).
          */
          if (prec->pid == realpid)
          {
              for (i = 0; i < C_LOCK_MAXIMUM; ++i)
                  prec->poolstats[i] = (ub1)(poolstats[i] & 0xFF);
              if (empty_flag)
              {
                  *mapoff = -1;
                  prec->pid = 0;
                  prec->location = 0;
              }
              else
                  *mapoff = (int)(prec - (pool_record *)(map->map_ptr));
              return;
          }
          /* If not found, no need to bother updating, just return */
          if (empty_flag) return;

          /* Otherwise, attempt to create a new record */
          if (os_sem_acquire(map->f_mutex, SHMEM_WAIT_MAX))
          {
              prec = (pool_record *)(map->map_ptr);
              for (recnum = 0; recnum < maxrecs; ++recnum, ++prec)
                  if (prec->pid == 0) break;
              if (recnum < maxrecs)
              {
                  *mapoff = recnum;
                  prec->pid = realpid;
                  prec->location = locidx;
                  for (i = 0; i < C_LOCK_MAXIMUM; ++i)
                      prec->poolstats[i] = (ub1)(poolstats[i] & 0xFF);
                  if (++recnum < maxrecs)
                  {
                      prec[1].pid = 0;
                      prec[1].location = -1;
                  }
              }
              os_sem_release(map->f_mutex);
          }
      }
#endif
}

/*
** Read and summarize pool statistics
*/
int owa_shmem_stats(shm_context *map, char *location, int *poolstats)
{
#ifdef KEEP_GLOBAL_STATS
    pool_record *prec;
    int          maxrecs;
    char        *lname;
    int          i;
    int          locidx = -1;
    int          nrecs = 0;

    if (map)
      if (map->map_ptr)
      {
          for (i = 0; i < C_LOCK_MAXIMUM; ++i) poolstats[i] = 0;

          prec = (pool_record *)(map->map_ptr);
          lname = (char *)(map->map_ptr) + map->pagesize;

          maxrecs = (map->pagesize)/sizeof(pool_record);

          while (maxrecs > 0)
          {
              if (prec->location < 0) break; /* No more entries */
              if (prec->pid != 0)
              {
                if (locidx < 0)
                  if (!str_compare(location, lname + prec->location, -1, 0))
                    locidx = prec->location;
                if (prec->location == locidx)
                {
                  for (i = 0; i < C_LOCK_MAXIMUM; ++i)
                      poolstats[i] += (prec->poolstats[i] & 0xFF);
                  ++nrecs;
                }
              }
              --maxrecs;
              ++prec;
           }
          return(nrecs);
      }
#endif
    return(-1);
}

/*
** Check for old connections and close them
*/
void owa_pool_purge(owa_context *octx, int interval)
{
    int         i;
    long_64     curtime;
    sword       status;
    volatile connection *c_pool;

    if (!(octx->init_complete)) return;

    curtime = util_component_to_stamp(os_get_component_time(0));

    mowa_acquire_mutex(octx);

    /*
    ** Reverse search the list, since the normal process of connection
    ** assignment works from the head of the list.  At the first live
    ** available connection, check the age; if it's been used within
    ** the interval, then exit the loop immediately and wait for the
    ** next pass.  Otherwise, close it, and continue looping.
    **
    ** ### AT SOME POINT WHEN "LOCKED" SESSIONING IS SUPPORTED, NEED TO
    ** ### MAKE SOME CONNECTIONS IMMUNE FROM THIS PROCESS.  FOR NOW,
    ** ### USE THE EXISTING LRU ALGORITHM, WITH A SMALL MODIFICATION.
    */
    i = octx->poolsize;
    while (i > 0)
    {
        c_pool = octx->c_pool + (--i);
        if (c_pool->c_lock == C_LOCK_AVAILABLE)
        {
            un_long elapsed = (un_long)
              ((curtime - c_pool->timestamp)/(long_64)1000000);
            if (elapsed <= (un_long)interval)
            {
                /*
                ** If sessioning is being used, the code can no longer
                ** rely on the sessions being in LRU order, so the search
                ** has to continue through the entire list.
                */
                if (octx->session)
                    continue;
                else
                    break;
            }
            status = sql_disconnect((connection *)c_pool);
            if (octx->diagflag  & DIAG_POOL)
              debug_out(octx->diagfile,
                        "Cleanup thread %d "
                        "closed connection for %s with status %d\n",
                        octx->location, 0, octx->realpid, status);
            --(octx->poolstats[C_LOCK_AVAILABLE]);
            c_pool->c_lock = C_LOCK_UNUSED;
            ++(octx->poolstats[c_pool->c_lock]);
        }
    }

    owa_shmem_update(octx->mapmem, &(octx->shm_offset),
                     octx->realpid, octx->location, octx->poolstats);

    mowa_release_mutex(octx);
}
