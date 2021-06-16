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
** File:         owafile.c
** Description:  Operating-system-dependent file I/O and IPC routines.
** Author:       Doug McMahon      Doug.McMahon@oracle.com
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
**
** File I/O functions
**
** os_exit()
**   Exit process and return status.
**
** os_get_pid()
**   Return the OS process ID.
**
** os_get_tid()
**   Return the OS thread ID (or -1 if not multi-threaded).
**
** os_milli_sleep()
**   Sleep for specified number of milliseconds.
**
** os_get_time()
**   Return the system time in seconds since 01/01/1970.
**
** os_get_component_time()
**   Return the system time as bit field components (64-bit value)
**
** os_page_round()
**   Round a size up to a page boundary.
**
** os_get_errno()
**   Return last system error.
**
** os_set_errno()
**   Resets system error number.
**
** os_fd_close_exec()
**   Set a file descriptor to close-on-exec mode.
**
** os_sem_create()
**   Create a semaphore of specified size.
**
** os_sem_acquire()
**   Wait for one resource from semaphore.
**
** os_sem_release()
**   Release one resource to semaphore.
**
** os_sem_destroy()
**   Destroy semaphore.
**
** os_mutex_create()
**   Create a shared mutex.
**
** os_mutex_acquire()
**   Acquire mutex.
**
** os_mutex_release()
**   Release mutex.
**
** os_mutex_destroy()
**   Destroy mutex.
**
** os_cond_init()
**   Create a semaphore-like condition variable.
**
** os_cond_wait()
**   Wait for the condition variable, then decrement.
**
** os_cond_signal()
**   Increment condition variable, then signal.
**
** os_cond_destroy()
**   Destroy the condition variable.
**
** mem_alloc()
**   Allocate memory from process heap.
**
** mem_zalloc()
**   Allocate zero-initialized memory from process heap.
**
** mem_realloc()
**   Re-allocate memory from process heap.
**
** mem_free()
**   Free memory to process heap.
**
** os_get_user()
**   Get the current user name as a string.
**
** os_env_get()
**   Get value for environment variable.
**
** os_env_set()
**   Set value for environment variable.
**
** file_open_temp()
**   Generate a temporary file name given a path as a template.  The
**   generated name will be in the same directory as the target file.
**   Opens the file for writing and returns the handle.
**
** file_open_write()
**   Open a file for writing.
**
** file_seek()
**   Seek to specified offset in readable file.
**
** file_write_data()
**   Write a data block to a file.
**
** file_write_text()
**   Write a text string to a file.  If the string is null-terminated,
**   stops at the trailing null.  Translates \n to \r\n on Windows.
**
** file_read_data()
**   Read a data block from a file.
**
** file_close()
**   Close a file.
**
** file_open_read()
**   Given a file path, attempts to open the file for read
**   and "stat" it to get the size in bytes and the age in seconds.
**
** file_map()
**   Open a file mapping object.
**
** file_view()
**   Map a file into the process space for reading.
**
** file_unmap()
**   Unmap a memory-mapped file, or shared memory segment.
**
** file_delete()
**   Unlinks/deletes a file.
**
** file_move()
**   Normally, unlinks the target file, then renames the source
**   file to the target.  If no target file, unlinks the source
**   (deleting it).
**
** file_readdir()
**   Read a list of files from a directory.
**
** file_mkdir()
**   Create a directory.
**
** file_rmdir()
**   Remove a directory.
**
** os_virt_alloc()
**   Allocate virtual pages.
**
** os_virt_free()
**   Free virtual pages.
**
** os_shm_get
**   Get a shared memory region.
**
** os_shm_attach
**   Attach/map shared memory address.
**
** os_shm_detach
**   Detach/unmap shared memory address/region.
**
** os_shm_destroy
**   Destroy a shared memory segment.
**
** thread_spawn()
**   Create a new native thread.
**
** thread_exit()
**   Exit from current thread.
**
** thread_shutdown()
**   Stop a thread.
**
** thread_suspend
**   Suspend a thread (or suspend current thread).
**
** thread_resume
**   Restart a thread.
**
** thread_cancel
**   Post cancel request for a thread.
**
** thread_check
**   Check for thread cancel request.
**
** thread_block_signals
**   Block certain signals for Apache compatibility.
**
** socket_init
**   Initialize socket system (needed for Windows sockets).
**
** socket_close
**   Close a socket.
**
** socket_listen
**   Create a socket and bind it for listening (server).
**
** socket_accept
**   Wait for and accept a new connection on a socket.
**
** socket_connect
**   Create, bind, and connect to a socket (client).
**
** socket_write
**   Write data to a socket.
**
** socket_read
**   Read data from a socket.
**
** debug_out()
**   Write string to diagnostic file.
**
** Semaphores
**   On unix, the implementation of an IPC mutex is via a semaphore
**   with the following states:
**     0  newly created, state exists for very brief period
**     1  the locked state, object is in use
**     3  the available state, object is not in use
**   The create operation does a get/create.  It looks at the value
**   of the semaphore, and if 0 or an invalid (> 3) value, assumes
**   that the process is the first caller, and sets the semaphore
**   value to 3.  Otherwise, it just returns it, assuming some other
**   process has initialized it and/or that it may already be in use.
**   The locking operation attempts a decrement by 2; this will be
**   blocked if the object is already locked, otherwise the object
**   state will drop to 1.
** ### This tri-state logic attempts to handle the start-up case;
** ### otherwise with a traditional two-state system it's unclear
** ### which process should initialize the semaphore.  It seems
** ### rather kludgey, though.  One concern is a race condition
** ### between two processes (or threads) both attempting to
** ### set it to 3 (which would be OK) and with one attempting to
** ### lock it shortly thereafter.  If the sequence of events is:
** ###   (1) Process A sees value == 0
** ###   (2) Process B sees value == 0
** ###   (3) Process A sets value = 3
** ###   (4) Process A locks it, value = 1
** ###   (5) Process B sets value = 3
** ### then the semaphore will end up in an incorrect state.  I guess
** ### if there's a small enough delay between the test and the set
** ### operations, while there's a long enough delay between the set
** ### and the lock operations, this scenario is unlikely...
**
** Shared memory
** ### On Unix, I can't figure out how to initialize the shared memory
** ### segment.  The segment lives even after all processes have
** ### exited (unless they exit cleanly and do the shmctl() in unmap).
** ### How do I know that a process is the first one and that it's safe
** ### to run the init logic?
**
** History
**
** 08/17/2000   D. McMahon      Created
** 05/31/2001   D. McMahon      Began revisioning with 1.3.9
** 02/11/2002   D. McMahon      Add functions to remove directories
** 06/11/2002   D. McMahon      Added condition variable support
** 06/19/2002   D. McMahon      Add backlog argument to socket_listen
** 07/29/2002   D. McMahon      Always use readdir() on Unix
** 08/22/2002   D. McMahon      Change names for thread_create/thread_kill
** 10/10/2002   D. McMahon      Added socket_test
** 09/30/2006   D. McMahon      Change to os_thrhand for threads
** 04/27/2007   D. McMahon      Add file_lock()
** 05/07/2007   D. McMahon      Add mem_realloc()
** 09/18/2007   D. McMahon      Remove file_lock()
** 09/28/2007   D. McMahon      Use MODOWA_FILE_MODE for file create security
** 05/15/2011   D. McMahon      Fix bug with oversized timeouts to os_cond_wait
** 05/21/2012   D. McMahon      Add socket_flush
** 11/27/2012   D. McMahon      GCC fixes, Win-64 porting
** 09/19/2013   D. McMahon      Add os_get_component_time, IPV6 sockets
** 09/19/2013   D. McMahon      Use size_t for memory allocators
** 09/26/2013   D. McMahon      Get client IP address after socket_accept
** 04/24/2015   D. McMahon      Add os_get_user()
** 10/26/2015   J. Chung        OSX port from John T. Chung (nyquest.com)
** 02/25/2017   D. McMahon      Add os_env_dump()
** 12/19/2017   D. McMahon      Avoid 0-length writes in file_write_data()
** 10/18/2018   D. McMahon      Replace fstat() with stat()
*/


#ifdef WIN32
# define MODOWA_WINDOWS
#else
# ifdef WIN64
#  define MODOWA_WINDOWS
# endif
#endif

#ifdef MODOWA_WINDOWS
# ifdef ENABLE_CRYPT
#  ifndef NO_SOCKETS
#   define NO_SOCKETS /* ### This is incompatible with <wincrypt.h> ### */
#  endif
# endif
# ifndef NO_SOCKETS
#  include <winsock2.h>
# ifdef WIN64
#  include <Ws2tcpip.h> /* For InetNtop */
# endif
# endif
#endif

#include <modowa.h>

#ifndef MODOWA_WINDOWS

#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#ifndef AIX
#include <sys/fcntl.h> /* For O_CREAT, O_RDONLY, O_WRONLY, O_APPEND */
#endif
#include <sys/poll.h>  /* For poll() */
#include <pwd.h>       /* For getpwuid() */

extern char **environ;

# ifndef NO_FILE_CACHE  /* Less portable Unix headers */
#  include <dirent.h>
#  include <sys/stat.h>
#  include <sys/mman.h>
#  include <sys/shm.h>
#  ifndef WITH_IPC
#   define WITH_IPC
#  endif
# endif

# ifndef NO_SOCKETS
#  include <sys/socket.h>
#  include <netinet/in.h> /* For htons() */
#  include <arpa/inet.h>  /* For inet_addr(), inet_aton(), inet_ntop() */
# endif

/* Less portable Unix headers */
# include <pthread.h>
# include <signal.h>
# ifndef WITH_IPC
#  define WITH_IPC
# endif

#define USE_SOCKLENT /* Linux, Solaris, HP/UX */
#define USE_PTON     /* Linux, Solaris */

# ifdef WITH_IPC  /* Less portable Unix headers */
#  include <sys/ipc.h>
#  include <sys/sem.h>

#ifdef OSX

/* Apple OSX provides a definition of semun */
#define owasem union semun

#else
/*
** ### HORRIBLY, POSIX LEAVES THIS DEFINITION TO YOU, AND THE
** ### COMPILER ON SOLARIS WON'T DO THE RIGHT THING WITHOUT IT.
*/

typedef union semun
{
    int   val;
    void *buf;
} owasem;

#endif

typedef struct owa_cond
{
    int              val;
    pthread_cond_t   cond;
    pthread_mutex_t  mp;
} owa_cond;

# endif

#else /* WINDOWS */

/*
** NT version should rely exclusively on Win32 APIs, no calls to the
** C run-time library so that modowa has no dependency on installing it.
*/
# ifndef MODOWA_WINDOWS_H
#  include <windows.h>
#  define MODOWA_WINDOWS_H
# endif

#endif /* UNIX */

#define OWA_LOG_FILE "mod_owa.log"

/*
** Exit current process
*/
void os_exit(int status)
{
#ifdef MODOWA_WINDOWS
    ExitProcess(status);
#else
    exit(status);
#endif
}

/*
** Get process ID
*/
int os_get_pid(void)
{
#ifdef MODOWA_WINDOWS
    DWORD pid = GetCurrentProcessId();
#else
    int   pid = getpid();
#endif
    return(pid);
}

/*
** Get thread ID
*/
un_long os_get_tid(void)
{
#ifdef MODOWA_WINDOWS
    DWORD tid = GetCurrentThreadId();
#else
    un_long tid = (un_long)pthread_self();
#endif
    return(tid);
}

/*
** Sleep for specified number of milliseconds
*/
void os_milli_sleep(int ms)
{
#ifdef MODOWA_WINDOWS
    Sleep((DWORD)ms);
#else
    /* ### UNIX IMPLEMENTATION - CHECK NULL STRUCT PTR OK ### */
    poll((struct pollfd *)0, 0, ms);
#endif
}

#ifdef MODOWA_WINDOWS
static un_long os_unix_time(DWORD lo, DWORD hi, un_long *microsecs)
{
    __int64 tim, sec;

    /* Convert to long long integer */
    tim = (((__int64)hi & 0xFFFFFFFF) << 32) | ((__int64)lo & 0xFFFFFFFF);
    /* Convert to seconds */
    sec = tim/10000000;
    if (microsecs)
        *microsecs = (un_long)((tim - (sec * (__int64)10000000))/(__int64)10);
    /* 1601 -> 1970 */
    sec -= (((__int64)(2440588-2305814))*(__int64)(24*60*60));

    return((un_long)(sec & 0xFFFFFFFF));
}
#endif

/*
** Get system time
*/
un_long os_get_time(un_long *musec)
{
#ifdef MODOWA_WINDOWS
    FILETIME stim;
    GetSystemTimeAsFileTime(&stim);
    return(os_unix_time(stim.dwLowDateTime, stim.dwHighDateTime, musec));
#else /* Unix */
    struct timeval tv;
    gettimeofday(&tv, (struct timezone *)0);
    if (musec) *musec = (un_long)(tv.tv_usec);
    return((un_long)(tv.tv_sec));
#endif
}

/*
** Get system time as bit field components
*/
long_64 os_get_component_time(int gmt_flag)
{
  int     yr, mon, day, hr, min, sec, usec;
  long_64 result;

#ifdef MODOWA_WINDOWS
  SYSTEMTIME st;

  if (gmt_flag) GetSystemTime(&st);
  else          GetLocalTime(&st);

  yr   = st.wYear;
  mon  = st.wMonth;
  day  = st.wDay;
  hr   = st.wHour;
  min  = st.wMinute;
  sec  = st.wSecond;
  usec = st.wMilliseconds * (WORD)1000;
#else
  struct timeval  tv;
  struct tm       tinf;

  gettimeofday(&tv, (void *)0); /* ### Timezone is deprecated ### */

  if (gmt_flag) gmtime_r(&(tv.tv_sec), &tinf);
  else          localtime_r(&(tv.tv_sec), &tinf);

  yr   = tinf.tm_year + 1900;
  mon  = tinf.tm_mon + 1;
  day  = tinf.tm_mday;
  hr   = tinf.tm_hour;
  min  = tinf.tm_min;
  sec  = tinf.tm_sec;
  usec = (int)(tv.tv_usec);
#endif

  result  = ((long_64)yr)   << 46;
  result |= ((long_64)mon)  << 42;
  result |= ((long_64)day)  << 37;
  result |= ((long_64)hr)   << 32;
  result |= ((long_64)min)  << 26;
  result |= ((long_64)sec)  << 20;
  result |= ((long_64)usec);
  return(result);
}

un_long os_page_round(un_long sz)
{
    un_long pgsz;
#ifdef MODOWA_WINDOWS
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    pgsz = si.dwPageSize;
#else
    pgsz = getpagesize();
#endif
    --pgsz; /* Assumes page size is a power of 2 */
    return((sz + pgsz) & ~pgsz);
}

int os_get_errno(void)
{
#ifdef MODOWA_WINDOWS
    return((int)GetLastError());
#else
    return(errno);
#endif
}

void os_set_errno(int e)
{
#ifdef MODOWA_WINDOWS
    SetLastError((DWORD)e);
#else
    errno = e;
#endif
}

void os_fd_close_exec(int fd)
{
#ifndef MODOWA_WINDOWS
    if (!InvalidFile(fd)) fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
}

#ifdef WITH_IPC
#define MODOWA_PROJ_NUM 37 /* ### TOTALLY ARBITRARY ### */

/*
** Attempts to stat the file name given.  If this works,
** the key generated is used.  Otherwise, checksum is
** used to compute a key with as much entropy as possible,
** though the "project number" is still placed in the high byte.
*/
static key_t os_gen_key(char *name)
{
    key_t   key;
    un_long csum;
    if (name)
        key = ftok(name, MODOWA_PROJ_NUM);
    else
        key = IPC_PRIVATE;
    if (key == (key_t)-1)
    {
        csum = util_checksum(name, str_length(name));
        csum = (csum & 0xFFFFFF) ^ ((csum >> 8) & 0xFF0000);
        csum |= (MODOWA_PROJ_NUM << 24);
        key = (key_t)csum;
    }
    return(key);
}
#endif

os_objhand os_sem_create(char *name, int sz, int secure_flag)
{
#ifdef MODOWA_WINDOWS
    HANDLE              mh;
    SECURITY_ATTRIBUTES sattrs;

    sattrs.nLength = sizeof(sattrs);
    sattrs.lpSecurityDescriptor = (void *)0;
    sattrs.bInheritHandle = (secure_flag) ? FALSE : TRUE;
    mh = CreateSemaphore(&sattrs, sz, sz, NULL);
    if (!mh) mh = INVALID_HANDLE_VALUE;
    return(mh);
#else
#ifdef WITH_IPC
    key_t  key;
    int    mh;
    owasem su;

    key = os_gen_key(name);
    mh = semget(key, 1, IPC_CREAT | ((secure_flag) ? 0600 : 0666));
    if (mh >= 0)
    {
        su.val = sz;
        if (semctl(mh, 0, SETVAL, su) == -1) return(os_nullfilehand);
    }
    return(mh);
#else
    return(os_nullfilehand);
#endif
#endif
}

int os_sem_acquire(os_objhand mh, int timeout)
{
#ifdef MODOWA_WINDOWS
    if (WaitForSingleObject(mh, (DWORD)timeout) == WAIT_OBJECT_0)
        return(1);
#else
#ifdef WITH_IPC
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = -1;
    sb.sem_flg = SEM_UNDO;
    if (timeout != SHMEM_WAIT_INFINITE)
        sb.sem_flg |= IPC_NOWAIT; /* ### TIMEOUT NOT IMPLEMENTED! ### */
    if (semop(mh, &sb, 1) == 0) return(1);
#endif
#endif
    return(0);
}

int os_sem_release(os_objhand mh)
{
#ifdef MODOWA_WINDOWS
    if (ReleaseSemaphore(mh, 1, NULL)) return(1);
#else
#ifdef WITH_IPC
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = 1;
    sb.sem_flg = SEM_UNDO;
    if (semop(mh, &sb, 1) == 0) return(1);
#endif
#endif
    return(0);
}

int os_sem_destroy(os_objhand mh)
{
#ifdef MODOWA_WINDOWS
    return(1);
#else
#ifdef WITH_IPC
    if (semctl(mh, 0, IPC_RMID, 0) != -1) return(1);
#endif
    return(0);
#endif
}

os_objptr os_mutex_create(char *name, int secure_flag)
{
#ifdef MODOWA_WINDOWS
    HANDLE              mh;
    SECURITY_ATTRIBUTES sattrs;
    sattrs.nLength = sizeof(sattrs);
    sattrs.lpSecurityDescriptor = (void *)0;
    sattrs.bInheritHandle = (secure_flag) ? FALSE : TRUE;
    mh = CreateMutex(&sattrs, FALSE, name);
    if (!mh) mh = INVALID_HANDLE_VALUE;
    return(mh);
#else /* Unix */
    /* Implement using Posix thread mutexes */
    pthread_mutex_t *mp;
    mp = (pthread_mutex_t *)malloc(sizeof(*mp));
    if (mp)
        if (pthread_mutex_init(mp, (pthread_mutexattr_t *)0))
        {
            free((void *)mp);
            return((void *)0);
        }
    return((void *)mp);
#endif
}

int os_mutex_acquire(os_objptr mh, int timeout)
{
#ifdef MODOWA_WINDOWS
    if (WaitForSingleObject(mh, (DWORD)timeout) == WAIT_OBJECT_0)
        return(1);
    return(0);
#else /* Unix */
    /* Implement using Posix thread mutexes */
    if (timeout != SHMEM_WAIT_INFINITE)
      /* ### TIMEOUT NOT IMPLEMENTED */
      return(pthread_mutex_trylock((pthread_mutex_t *)mh) == 0);
    return(pthread_mutex_lock((pthread_mutex_t *)mh) == 0);
#endif
}

int os_mutex_release(os_objptr mh)
{
#ifdef MODOWA_WINDOWS
    return(ReleaseMutex(mh) ? 1 : 0);
#else /* Unix */
    /* Implement using Posix thread mutexes */
    return(pthread_mutex_unlock((pthread_mutex_t *)mh) == 0);
#endif
}

int os_mutex_destroy(os_objptr mh)
{
#ifdef MODOWA_WINDOWS
    return(1);
#else /* Unix */
    /* Implement using Posix thread mutexes */
    /* ### SHOULD ALSO FREE IT ### */
    return(pthread_mutex_destroy((pthread_mutex_t *)mh) == 0);
#endif
}

os_objptr os_cond_init(char *name, int sz, int secure_flag)
{
#ifdef MODOWA_WINDOWS
    HANDLE              mh;
    SECURITY_ATTRIBUTES sattrs;

    /* ### Windows implementation could use (Create/Set/Reset)Event ### */
    sattrs.nLength = sizeof(sattrs);
    sattrs.lpSecurityDescriptor = (void *)0;
    sattrs.bInheritHandle = (secure_flag) ? FALSE : TRUE;
    mh = CreateSemaphore(&sattrs, sz, sz, NULL);
    if (!mh) mh = INVALID_HANDLE_VALUE;
    return(mh);
#else
    owa_cond *cond;

    cond = (owa_cond *)malloc(sizeof(*cond));
    if (cond)
    {
        cond->val = sz;

        if (!pthread_cond_init(&(cond->cond), (pthread_condattr_t *)0))
            if (!pthread_mutex_init(&(cond->mp), (pthread_mutexattr_t *)0))
                return((void *)cond);

        free((void *)cond);
    }
    return((void *)0);
#endif
}

int os_cond_wait(os_objptr mh, int timeout)
{
#ifdef MODOWA_WINDOWS
    if (WaitForSingleObject(mh, (DWORD)timeout) == WAIT_OBJECT_0)
        return(1);
#else
    struct timeval   now;
    struct timespec  tm;
    owa_cond        *cond;

    cond = (owa_cond *)mh;
    if (!pthread_mutex_lock(&(cond->mp)))
    {
        if (timeout != SHMEM_WAIT_INFINITE)
        {
            gettimeofday(&now, (struct timezone *)0);
            tm.tv_sec = now.tv_sec + timeout/1000;
            tm.tv_nsec = (now.tv_usec + (timeout % 1000) * 1000) * 1000;
            while (tm.tv_nsec > 1000000000)
            {
                ++tm.tv_sec;
                tm.tv_nsec -= 1000000000;
            }
        }
        while (cond->val == 0)
        {
            if (timeout == SHMEM_WAIT_INFINITE)
            {
                if (pthread_cond_wait(&(cond->cond), &(cond->mp)))
                {
                    pthread_mutex_unlock(&(cond->mp));
                    return(0);
                }
            }
            else
            {
                if (pthread_cond_timedwait(&(cond->cond), &(cond->mp), &tm))
                {
                    /* ### Should check for ETIMEDOUT here? ### */
                    pthread_mutex_unlock(&(cond->mp));
                    return(0);
                }
            }
        }
        --(cond->val);
        pthread_mutex_unlock(&(cond->mp));
        return(1);
    }
#endif
    return(0);
}

int os_cond_signal(os_objptr mh)
{
#ifdef MODOWA_WINDOWS
    if (ReleaseSemaphore(mh, 1, NULL)) return(1);
#else
    owa_cond *cond;

    cond = (owa_cond *)mh;
    if (!pthread_mutex_lock(&(cond->mp)))
    {
        ++(cond->val);
        pthread_mutex_unlock(&(cond->mp));
        pthread_cond_signal(&(cond->cond));
        return(1);
    }
#endif
    return(0);
}

int os_cond_destroy(os_objptr mh)
{
#ifndef MODOWA_WINDOWS
    owa_cond *cond;
    cond = (owa_cond *)mh;
    pthread_mutex_destroy(&(cond->mp));
    pthread_cond_destroy(&(cond->cond));
    free(mh);
#endif
    return(1);
}

void *mem_alloc(size_t sz)
{
#ifdef MODOWA_WINDOWS
    return(HeapAlloc(GetProcessHeap(), (DWORD)0, (DWORD)sz));
#else
    return(malloc(sz));
#endif
}

void *mem_zalloc(size_t sz)
{
#ifdef MODOWA_WINDOWS
    return(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (DWORD)sz));
#else
    return(calloc(1, sz));
#endif
}

void *mem_realloc(void *ptr, size_t sz)
{
#ifdef MODOWA_WINDOWS
    return(HeapReAlloc(GetProcessHeap(), (DWORD)0, ptr, (DWORD)sz));
#else
    return(realloc(ptr, sz));
#endif
}

void mem_free(void *ptr)
{
#ifdef MODOWA_WINDOWS
    HeapFree(GetProcessHeap(), (DWORD)0, ptr);
#else
    free(ptr);
#endif
}

/*
** Fill buffer with current user name
*/
int os_get_user(char *username, int bufsize)
{
#ifdef MODOWA_WINDOWS
  DWORD bsz = (DWORD)bufsize;
  if (!GetUserName(username, &bsz))
    return(-((int)bsz));
  return((int)bsz - 1);
#else
  uid_t  uid = geteuid();
  size_t bsz;
  struct passwd *pw = getpwuid(uid);
  if (!pw) return(-bufsize);
  bsz = str_length(pw->pw_name);
  if (bsz >= bufsize) return(-((int)(bsz + 1)));
  mem_copy(username, pw->pw_name, bsz + 1);
  return((int)bsz);
#endif
}

/*
** Return a pointer to the environment value for the specified name.
** Null pointer return indicates failure, empty string indicates not
** found or no value.  On windows the returned pointer for non-empty
** strings must be freed.
*/
char *os_env_get(char *name)
{
#ifdef MODOWA_WINDOWS
    DWORD  elen;
    char   buffer[256];
    char  *sptr;

    if (!name) return((char *)0);

    elen = (DWORD)sizeof(buffer);
    elen = GetEnvironmentVariable(name, buffer, elen);
    if (elen == 0) return((char *)"");

    sptr = (char *)mem_alloc((int)++elen);
    if (!sptr) return((char *)0);

    if (elen > sizeof(buffer))
        elen = GetEnvironmentVariable(name, sptr, elen);
    else 
        mem_copy(sptr, buffer, (int)elen);
    return(sptr);
#else
    char *sptr;
    if (!name) return((char *)0);
    sptr = getenv(name);
    if (!sptr) sptr = (char *)"";
    return(sptr);
#endif
}

/*
** NULL pointer for value requests unsetenv operation.
** Returns null pointer on failure, pointer to name if set/unset
** successfully, pointer to old value if set/unset successfully
** over existing value.
*/
char *os_env_set(char *name, char *value)
{
#ifdef MODOWA_WINDOWS
    if (name)
      if (SetEnvironmentVariable(name, value))
        return(name);
    return((char *)0);
#else
    char *sptr;
    char *oldval;
    int   nlen;
    int   vlen;

    if (!name) return((char *)0);
    oldval = getenv(name);
    if (!value)
    {
        if (!oldval) return(name);
        value = (char *)"";
    }

    nlen = 0;
    for (sptr = name; *sptr; ++sptr) ++nlen;
    vlen = 0;
    for (sptr = value; *sptr; ++sptr) ++vlen;

    sptr = (char *)mem_alloc(nlen + vlen + 2);
    if (!sptr) return((char *)0);

    mem_copy(sptr, name, nlen);
    sptr[nlen++] = '=';
    mem_copy(sptr + nlen, value, vlen);
    sptr[nlen + vlen] = '\0';

    if (putenv(sptr)) return((char *)0);
    if (!oldval) return(name);
    return(oldval - nlen);
#endif
}

/*
** Dump the state of the environment one variable at a time.
** Returns NULL when the last variable has been read.
*/
char *os_env_dump(char *prior, un_long position)
{
  un_long  count = 0;
  char    *vptr = (position == 0) ? NULL : prior;
#ifdef MODOWA_WINDOWS
  /*
  ** If no prior variable, walk through all of them until position is found
  ** or the end is reached. If a prior is given, set position so that it
  ** will just increment to the next item.
  */
  if (!vptr)
    vptr = (char *)GetEnvironmentStrings();
  else
    position = 1;
  /* Advance pointer by <position> items */
  while (vptr)
  {
    int vlen = (int)strlen(vptr);
    if (vlen == 0)
      vptr = NULL;
    else if (count++ == position)
      break;
    else
      vptr += (++vlen);
  }
#else
  /*
  ** If a prior is given, make sure it matches the previous position.
  ** If so, we can just return the requested position.
  */
  if (vptr)
  {
    if (environ[position-1] == vptr)
      vptr = environ[position];
    else
      vptr = NULL;
  }
  /*
  ** Otherwise we need to loop through the previous <position> values to
  ** make sure we're not overreading the array.
  */
  if (!vptr)
  {
    for (vptr = NULL; environ[count]; ++count)
    {
      if (count == position)
      {
        vptr = environ[count];
        break;
      }
    }
  }
#endif
  return(vptr);
}


#ifdef MODOWA_WINDOWS

os_objhand file_open_temp(char *fpath, char *tempname, int tmax)
{
    os_objhand  fh;
    char       *sptr;
    int         slen;
    char        pid[LONG_MAXSTRLEN];

    sptr = str_char(fpath, os_dir_separator, 1);
    if (sptr) slen = (int)(sptr - fpath);
    else      slen = str_length(fpath);

    sptr = tempname + MAX_PATH;
    tmax -= MAX_PATH;
    if (slen >= tmax) slen = tmax;
    str_concat(sptr, 0, fpath, slen);
    slen = str_itoa(os_get_pid(), pid);
    pid[slen++] = '_';
    slen += str_itox(os_get_tid(), pid + slen);
    pid[slen++] = '_';
    pid[slen] = '\0';

    GetTempFileName(sptr, pid, 0, tempname);
    fh = CreateFile(tempname, GENERIC_WRITE, 0,
                    0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

    return(fh);
}

os_objhand file_open_write(char *fpath, int append_flag, int share_flag)
{
    os_objhand fh;
    int        flags = 0;
    if (share_flag) flags = FILE_SHARE_READ | FILE_SHARE_WRITE;
    fh = CreateFile(fpath, GENERIC_WRITE, flags,
                    0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (append_flag)
      if (fh != INVALID_HANDLE_VALUE)
        SetFilePointer(fh, 0, NULL, FILE_END);
    return(fh);
}

void file_seek(os_objhand fp, long offset)
{
    if (fp != os_nullfilehand)
        SetFilePointer(fp, (LONG)offset, NULL, FILE_BEGIN);
}

int file_write_data(os_objhand fp, char *buffer, int buflen)
{
    DWORD nbytes;
    if (InvalidFile(fp)) return(-1);
    nbytes = (DWORD)buflen;
    if (nbytes > 0)
        if (!WriteFile(fp, buffer, nbytes, &nbytes, NULL))
            return(-1);
    return((int)nbytes);
}

int file_write_text(os_objhand fp, char *str, int slen)
{
    char  *send;
    char  *sptr;
    char  *aptr;
    DWORD  nbytes;

    if ((!str) || (InvalidFile(fp))) return(-1);

    sptr = str;
    if (slen < 0) slen = str_length(str);
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
    return(slen);
}

int file_read_data(os_objhand fp, char *buffer, int buflen)
{
    DWORD nbytes;
    if (InvalidFile(fp)) return(-1);
    nbytes = (DWORD)buflen;
    if (!ReadFile(fp, buffer, nbytes, &nbytes, NULL)) return(-1);
    return((int)nbytes);
}

void file_close(os_objhand fp)
{
    if (fp != INVALID_HANDLE_VALUE) CloseHandle(fp);
}

int file_lock(os_objhand fp, int shared, int blocking, int unlock)
{
    BOOL  result;
    int   status;
    DWORD flags = 0;
    OVERLAPPED overlap;

    FillMemory((void *)&overlap, (DWORD)sizeof(overlap), (BYTE)0);
    /* overlap.Offset = overlap.OffsetHigh = overlap.hEvent = 0; */

    if (unlock)
        result = UnlockFileEx(fp, (DWORD)0, (DWORD)0, (DWORD)0, &overlap);
    else
    {
        if (!shared) flags |= LOCKFILE_EXCLUSIVE_LOCK;
        if (!blocking) flags |= LOCKFILE_FAIL_IMMEDIATELY;

        result = LockFileEx(fp, flags, (DWORD)0, (DWORD)0, (DWORD)0, &overlap);
    }

    if (result) return(0);                /* success */
    status = GetLastError();
                                          /* blocked */
    return(-1);                           /* failed  */
}

#ifndef NO_FILE_CACHE

os_objhand file_open_read(char *fpath, un_long *fsz, un_long *fage)
{
    os_objhand                 fh;
    __int64                    lo, hi, delta_time;
    FILETIME                   stim;
    BY_HANDLE_FILE_INFORMATION st;

    fh = CreateFile(fpath, GENERIC_READ, FILE_SHARE_READ,
                    0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (fh != INVALID_HANDLE_VALUE)
    {
        GetFileInformationByHandle(fh, &st);
        if (st.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            CloseHandle(fh);
            return(INVALID_HANDLE_VALUE);
        }
        *fsz = (DWORD)st.nFileSizeLow;
        GetSystemTimeAsFileTime(&stim);

        /* This is the number of 100ns intervals since 01/01/1601 */
        lo = ((__int64)stim.dwLowDateTime) & 0xFFFFFFFF;
        hi = ((__int64)stim.dwHighDateTime) & 0xFFFFFFFF;
        delta_time = (hi << 32) | lo;
        lo = ((__int64)st.ftLastWriteTime.dwLowDateTime) & 0xFFFFFFFF;
        hi = ((__int64)st.ftLastWriteTime.dwHighDateTime) & 0xFFFFFFFF;
        delta_time -= ((hi << 32) | lo);
        delta_time /= (__int64)10000000;
        *fage = (un_long)(delta_time & 0xFFFFFFFF);
    }

    return(fh);
}

os_objhand file_map(os_objhand fp, un_long fsz,
                    char *mapname, int write_flag)
{
    os_objhand hnd;
    DWORD      prot;

    /*
    ** First, attempt to open an existing file mapping with the
    ** specified name.  If this succeeds, return the handle.
    */
    if (mapname)
    {
        hnd = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, mapname);
        /* ### Documentation says the return is null on failure! ### */
        if ((hnd != INVALID_HANDLE_VALUE) && (hnd)) return(hnd);
    }

    /*
    ** Next, create a new file mapping.  Note that an invalid fp is
    ** allowed here, since the semantics are to create the mapping
    ** in the OS page file.
    */
    if (write_flag)
        prot = PAGE_READWRITE | SEC_NOCACHE | SEC_COMMIT;
        /* ### SEC_RESERVE didn't work ### */
    else
        prot = PAGE_READONLY;
    hnd = CreateFileMapping(fp, 0, prot, 0, (DWORD)fsz, mapname);
    if (!hnd) hnd = INVALID_HANDLE_VALUE;

    return(hnd);
}

void *file_view(os_objhand hnd, un_long fsz, int write_flag)
{
    void *ptr;
    if (InvalidFile(hnd)) return((void *)0);
    ptr = MapViewOfFile(hnd, (write_flag) ? FILE_MAP_WRITE : FILE_MAP_READ,
                        (DWORD)0, (DWORD)0, (size_t)fsz);
    return(ptr);
}

void file_unmap(os_objhand hnd, void *ptr, un_long fsz)
{
    if (ptr) UnmapViewOfFile(ptr);
    if (hnd != INVALID_HANDLE_VALUE) CloseHandle(hnd);
}

void file_delete(char *fname)
{
    DeleteFile(fname);
}

void file_move(char *src, char *dest)
{
    if (!dest)
    {
        DeleteFile(src);
    }
    else
    {
        DeleteFile(dest); /* ### PROBABLY SHOULD JUST RENAME IT ### */
        MoveFile(src, dest);
    }
}

int file_readdir(char *dirname, dir_record *dr)
{
    HANDLE          dh;
    WIN32_FIND_DATA dent; /* ### Is this OK on WIN64? ### */
    char            fname[MAX_PATH + 3];
    int             plen;

    dh = dr->dh;

    if (dirname == (char *)0)
    {
        if (!InvalidFile(dh)) FindClose(dh);
        dr->dh = INVALID_HANDLE_VALUE;
        return(0);
    }

    if (dh == INVALID_HANDLE_VALUE)
    {
        plen = str_concat(fname, 0, dirname, MAX_PATH);
        fname[plen++] = os_dir_separator;
        fname[plen++] = '*';
        fname[plen] = '\0';
        dr->dh = dh = FindFirstFile(fname, &dent);
        if (dh == INVALID_HANDLE_VALUE) return(0);
    }
    else
    {
        if (!FindNextFile(dh, &dent))
        {
            FindClose(dh);
            dr->dh = INVALID_HANDLE_VALUE;
            return(0);
        }
    }
    dr->fsize = dent.nFileSizeLow; /* High is ignored for now */
    str_concat(dr->fname, 0, dent.cFileName, sizeof(dr->fname)-1);
    dr->dir_flag = (dent.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    if (dent.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN   |
                                 FILE_ATTRIBUTE_READONLY |
                                 FILE_ATTRIBUTE_SYSTEM   ))
        dr->attrs = 1;
    else
        dr->attrs = 0;

    dr->ftime = os_unix_time(dent.ftLastWriteTime.dwLowDateTime,
                             dent.ftLastWriteTime.dwHighDateTime,
                             (un_long *)0);
    return(1);
}

int file_mkdir(char *name, int mode)
{
    DWORD attrs;
    attrs = GetFileAttributes(name);
    if (attrs == 0xFFFFFFFF)
        /* ### For now, "mode" argument ignored ### */
        return((CreateDirectory(name, NULL)) ? 1 : 0);
    else
        return((attrs & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0);
}

int file_rmdir(char *name)
{
    return((RemoveDirectory(name)) ? 1 : 0);
}

os_objhand os_shm_get(char *mapname, un_long fsz)
{
    return(file_map(INVALID_HANDLE_VALUE, fsz, mapname, 1));
}

void *os_shm_attach(os_objhand hnd, un_long fsz)
{
    return(file_view(hnd, fsz, 1));
}

void os_shm_detach(void *ptr)
{
    if (ptr) UnmapViewOfFile(ptr);
}

void os_shm_destroy(os_objhand hnd)
{
    if (hnd != INVALID_HANDLE_VALUE) CloseHandle(hnd);
}

#endif /* FILE_CACHE */

#else /* !MODOWA_WINDOWS == UNIX */

os_objhand file_open_temp(char *fpath, char *tempname, int tmax)
{
    os_objhand  fd;
    char       *sptr;
    int         slen;

    sptr = str_char(fpath, os_dir_separator, 1);
    if (sptr) slen = (sptr - fpath) + 1;
    else      slen = str_length(fpath);

    if ((slen + LONG_MAXSTRLEN) > tmax) slen = tmax - LONG_MAXSTRLEN;

    str_concat(tempname, 0, fpath, slen);
    slen += str_itoa(os_get_pid(), tempname + slen);
    str_copy(tempname + slen, "_XXXXXX");

    fd = mkstemp(tempname);

    return(fd);
}

os_objhand file_open_write(char *fpath, int append_flag, int share_flag)
{
    os_objhand fd;
    char      *fmodeval;
    int        flags = O_WRONLY | O_CREAT;
    int        fmode = 0;

    if (append_flag) flags |= O_APPEND;

    fmodeval = getenv("MODOWA_FILE_MODE");
    if (!fmodeval)
        fmode = 0600;
    else
        while ((*fmodeval >= '0') && (*fmodeval < '8'))
            fmode = (fmode << 6) | (*(fmodeval++) - '0');

    fd = open(fpath, flags, fmode);
    return(fd);
}

void file_seek(os_objhand fp, long offset)
{
    if (fp >= 0) lseek(fp, offset, SEEK_SET);
}

int file_write_data(os_objhand fp, char *buffer, int buflen)
{
    int nbytes = 0;
    if (InvalidFile(fp)) return(-1);
    if (buflen > 0) nbytes = write(fp, buffer, buflen);
    return(nbytes);
}

int file_write_text(os_objhand fp, char *str, int slen)
{
    if (!str) return(-1);
    if (slen < 0) slen = str_length(str);
    return(file_write_data(fp, str, slen));
}

int file_read_data(os_objhand fp, char *buffer, int buflen)
{
    int nbytes;
    if (InvalidFile(fp)) return(-1);
    nbytes = read(fp, buffer, buflen);
    return(nbytes);
}

void file_close(os_objhand fp)
{
    if (fp >= 0) close(fp);
}

#ifndef NO_FILE_CACHE

os_objhand file_open_read(char *fpath, un_long *fsz, un_long *fage)
{
    os_objhand  fd;
    time_t      tm;
    struct stat st;

    if (stat(fpath, &st) != 0)
        return(-1);
    if (st.st_mode & S_IFDIR)
        return(-1);
    fd = open(fpath, O_RDONLY);
    if (InvalidFile(fd)) return(-1);
    /*
    ** ### WHICH OF THESE SHOULD BE USED:
    ** ### st_mtime (time of last modify to contents)
    ** ### st_ctime (time of last change to inode)
    ** ### st_atime (time of last access for read)
    */
    *fage = (un_long)(time(&tm) - st.st_mtime);
    *fsz  = (un_long)st.st_size;

    return(fd);
}

os_objhand file_map(os_objhand fp, un_long fsz,
                    char *mapname, int write_flag)
{
    return(fp); /* On Unix, no need to do anything to mem-map a file */
}

void *file_view(os_objhand hnd, un_long fsz, int write_flag)
{
    int   prot;
    void *ptr = (void *)0;

    if (hnd >= 0)
    {
        prot = PROT_READ;
        if (write_flag) prot |= PROT_WRITE;
        ptr = mmap((void *)0, (size_t)fsz, prot, MAP_SHARED, hnd, (off_t)0);
    }
    return(ptr);
}

void file_unmap(os_objhand hnd, void *ptr, un_long fsz)
{
    if ((fsz > 0) && (ptr != (void *)0)) munmap(ptr, fsz);
}

void file_delete(char *fname)
{
    unlink(fname);
}

void file_move(char *src, char *dest)
{
    if (!dest)
    {
        unlink(src);
    }
    else
    {
        unlink(dest);
        rename(src, dest);
    }
}

int file_readdir(char *dirname, dir_record *dr)
{
    DIR           *dh;
    struct dirent *dptr;
    struct stat    st;
    int            i;
    char           fpath[HTBUF_HEADER_MAX];

    dh = (DIR *)(dr->dh);

    if (dirname == (char *)0)
    {
        if (dh) closedir(dh);
        dr->dh = (void *)0;
        return(0);
    }

    if (!dh)
    {
        dh = opendir(dirname);
        dr->dh = (void *)dh;
        if (!dh) return(0);
    }

    dptr = readdir(dh); /* Presumed per Solaris doc to be thread-safe */
    if (!dptr)
    {
        closedir(dh);
        dr->dh = (void *)0;
        return(0);
    }

    str_copy(dr->fname, dptr->d_name);
    i = str_concat(fpath, 0, dirname, sizeof(fpath)-1);
    fpath[i++] = os_dir_separator;
    str_concat(fpath, i, dr->fname, sizeof(fpath)-1);

    dr->dir_flag = 0;
    dr->attrs = 0;
    /* ### Slows down this API, but necessary to match MODOWA_WINDOWS ### */
    if (stat(fpath, &st) == 0)
    {
        if (st.st_mode & S_IFDIR) dr->dir_flag = 1;
        if ((st.st_mode & ~0777) != S_IFREG) dr->attrs = 1;
        dr->fsize = (un_long)st.st_size;
        dr->ftime = (un_long)st.st_mtime;
    }
    else
    {
        dr->fsize = (un_long)0;
        dr->ftime = (un_long)0;
    }

    return(1);
}

int file_mkdir(char *name, int mode)
{
    struct stat    st;
    if (stat(name, &st) == 0)
        return((st.st_mode & S_IFDIR) ? 1 : 0);
    return((mkdir(name, (mode_t)mode) == 0) ? 1 : 0);
}

int file_rmdir(char *name)
{
    return((rmdir(name) == 0) ? 1 : 0);
}

os_objhand os_shm_get(char *mapname, un_long fsz)
{
    key_t key;
    int   hnd;
    int   flags;

    key = os_gen_key(mapname);
    flags = IPC_CREAT | 0666;
    if (!mapname) flags |= IPC_EXCL;
    hnd = shmget(key, fsz, flags);
    return(hnd);
}

void *os_shm_attach(os_objhand hnd, un_long fsz)
{
    void *ptr = (void *)0;
    ptr = shmat(hnd, (void *)0, 0);
    return(ptr);
}

void os_shm_detach(void *ptr)
{
    if (ptr != (void *)0) shmdt(ptr);
}

void os_shm_destroy(os_objhand hnd)
{
    if (!InvalidFile(hnd)) shmctl(hnd, IPC_RMID, 0);
}

#endif /* FILE_CACHE */

#endif /* UNIX */

#ifndef NO_FILE_CACHE

/*
** Allocate pages of virtual memory
*/
void *os_virt_alloc(size_t sz)
{
   void *ptr = (void *)0;
#ifdef MODOWA_WINDOWS
   if (sz)
     ptr = VirtualAlloc(ptr, sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
     /* ### MEM_RESET is another way to use this routine ### */
#else
   if (sz)
   {
     ptr = mmap(ptr, sz, PROT_READ | PROT_WRITE,
#ifdef OSX
                MAP_ANONYMOUS | MAP_PRIVATE,
#else /* !OSX */
                MAP_ANON | MAP_PRIVATE,
#endif
                os_nullfilehand, (off_t)0);

     if (ptr == MAP_FAILED) ptr = (void *)0;
   }
#endif
   return(ptr);
}

/*
** Free pages of virtual memory
*/
void os_virt_free(void *ptr, size_t sz)
{
#ifdef MODOWA_WINDOWS
   if ((sz > 0) && (ptr != (void *)0))
      VirtualFree(ptr, sz, MEM_RELEASE);
#else
   if ((sz > 0) && (ptr != (void *)0)) munmap(ptr, sz);
#endif
}

#endif /* FILE_CACHE */

os_thrhand thread_spawn(void (*thread_start)(void *),
                        void *thread_context,
                        un_long *out_tid)
{
#ifdef MODOWA_WINDOWS
    HANDLE th;
    DWORD  tid;

    th = CreateThread(0,            /* security attrs */
                      (DWORD)0,     /* stack size, default is 1M */
                      (LPTHREAD_START_ROUTINE)thread_start,
                      thread_context,
                      (DWORD)0,     /* CREATE_SUSPENDED is the only value */
                      &tid);

    if (out_tid) *out_tid = (un_long)tid;
#else
    int       status;
    un_long   tid;
    pthread_t th;

    status = pthread_create(&th, (pthread_attr_t *)0,
                            (void *(*)(void *))thread_start, thread_context);
    if (status == 0)
      tid = (un_long)th;
    else
      tid = (un_long)os_nullthrhand;

    if (out_tid) *out_tid = tid;
#endif
    return(th);
}

void thread_exit(void)
{
#ifdef MODOWA_WINDOWS
    ExitThread((DWORD)0);
#else
    pthread_exit((void *)0);
#endif
}

void thread_shutdown(os_thrhand th)
{
#ifdef MODOWA_WINDOWS
    TerminateThread(th, (DWORD)0);
#else
    pthread_kill(th, SIGKILL);
#endif
}

void thread_join(os_thrhand th)
{
#ifndef MODOWA_WINDOWS
    pthread_join(th, (void **)0);
#endif
}

void thread_detach(os_thrhand th)
{
#ifndef MODOWA_WINDOWS
    pthread_detach(th);
#endif
}

void thread_suspend(os_thrhand th)
{
#ifdef MODOWA_WINDOWS
    if (th == INVALID_HANDLE_VALUE) th = GetCurrentThread(); /* Suspend self */
    SuspendThread(th);
#endif
}

void thread_resume(os_thrhand th)
{
#ifdef MODOWA_WINDOWS
    ResumeThread(th);
#endif
}

void thread_cancel(os_thrhand th)
{
#ifndef MODOWA_WINDOWS
    pthread_cancel(th);
#endif
}

void thread_check(void)
{
#ifndef MODOWA_WINDOWS
    pthread_testcancel();
#endif
}

void thread_block_signals(void)
{
#ifndef MODOWA_WINDOWS
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM); /* Block signal from the keepalive timer  */
    sigaddset(&set, SIGTERM); /* Block Apache 2 clean shutdown signal   */
    sigaddset(&set, SIGHUP);  /* Block another Apache 2 shutdown signal */
    pthread_sigmask(SIG_BLOCK, &set, NULL);
#endif
}

/*
** Open the debug file, dump the requested output, and close it again
** Understands only the simplest possible formatting commands, e.g.
** %s, %d, %x, and %%.
*/
void debug_out(char *diagfile, char *fmt,
               char *s1, char *s2, int i1, int i2)
{
    os_objhand  fp;
    char       *sptr;
    char       *aptr;
    int         scnt = 0;
    int         icnt = 0;
    char        ibuf[LONG_MAXSTRLEN];

    sptr = diagfile;
    if (!sptr) sptr = (char *)OWA_LOG_FILE;
    fp = file_open_write(sptr, 1, 1);
    if (InvalidFile(fp)) return;

    sptr = fmt;

    for (aptr = sptr; *aptr; ++aptr)
    {
        if (*aptr == '%')
        {
            file_write_text(fp, sptr, (int)(aptr - sptr));
            ++aptr;
            if (*aptr == 's')
            {
                if (scnt == 0) file_write_text(fp, s1, -1);
                if (scnt == 1) file_write_text(fp, s2, -1);
                ++scnt;
            }
            else if (*aptr == 'd')
            {
                if (icnt == 0) file_write_text(fp, ibuf, str_itoa(i1, ibuf));
                if (icnt == 1) file_write_text(fp, ibuf, str_itoa(i2, ibuf));
                ++icnt;
            }
            else if (*aptr == 'x')
            {
                if (icnt == 0) file_write_text(fp, ibuf,
                                               str_itox((un_long)i1, ibuf));
                if (icnt == 1) file_write_text(fp, ibuf,
                                               str_itox((un_long)i2, ibuf));
                ++icnt;
            }
            else if (*aptr == '%')
                --aptr;
            sptr = aptr + 1;
        }
    }
    file_write_text(fp, sptr, (int)(aptr - sptr));
    file_close(fp);
}

#ifndef NO_SOCKETS

/*
** Socket initialization
*/
void socket_init(void)
{
#ifdef MODOWA_WINDOWS
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2,0), &wsa_data); /* Winsock 2.0 */
#endif
}

/*
** Close a socket
*/
void socket_close(os_socket sock)
{
#ifdef MODOWA_WINDOWS
    shutdown(sock, SD_BOTH);
    closesocket(sock);
#else
    shutdown(sock, SHUT_RDWR);
    close(sock);
#endif
}

/*
** Socket creation and binding
*/
os_socket socket_listen(int port, char *ipaddr, int backlog)
{
    os_socket          sfd;
    struct sockaddr_in saddr;

    saddr.sin_family = AF_INET;
    saddr.sin_port   = htons((unsigned short)port);
#ifdef MODOWA_WINDOWS
    saddr.sin_addr.S_un.S_addr = inet_addr(ipaddr);
#else
#ifdef USE_PTON
    if (inet_pton(AF_INET, ipaddr, &saddr.sin_addr) == 0)
    {
        saddr.sin_family = AF_INET6;
        inet_pton(AF_INET6, ipaddr, &saddr.sin_addr);
    }
#else
    /* Obsolete interface (Mac?) */
    inet_aton(ipaddr, &saddr.sin_addr);
#ifdef NEVER /* Solaris */
    saddr.sin_addr.s_addr = inet_addr(ipaddr);
#endif
#endif
#endif

    sfd = socket(PF_INET, SOCK_STREAM, 0);

    if (sfd != os_badsocket)
    {
        if (bind(sfd, (struct sockaddr *)&saddr, sizeof(saddr)) != 0)
        {
            socket_close(sfd);
            sfd = os_badsocket;
        }
        else if (listen(sfd, backlog) != 0)
        {
            socket_close(sfd);
            sfd = os_badsocket;
        }
    }

    return(sfd);
}

/*
** Accept a new socket connection
*/
os_socket socket_accept(os_socket sock, char *buf, int bufsz)
{
    os_socket          sfd;
    struct sockaddr_in saddr;
#ifdef USE_SOCKLENT
    socklen_t          n;
#else
    int                n;
#endif

    n = sizeof(saddr);
    mem_zero((void *)&saddr, n);
    sfd = accept(sock, (struct sockaddr *)&saddr, &n);

    if ((buf) && (bufsz > 0))
    {
#ifdef MODOWA_WINDOWS
#ifdef WIN64
      if (InetNtop(AF_INET, &saddr.sin_addr, buf, bufsz) == NULL)
        if (InetNtop(AF_INET6, &saddr.sin_addr, buf, bufsz) == NULL)
          *buf = '\0';
#else
      char *ip = inet_ntoa(saddr.sin_addr); /* ### Not thread safe ### */
      if (!ip)
        *buf = '\0';
      else
      {
        int iplen = str_concat(buf, 0, ip, bufsz);
        if (iplen < bufsz) buf[iplen] = '\0';
      }
#endif
#else
      /* Get string for socket */
      if (inet_ntop(AF_INET, &saddr.sin_addr, buf, bufsz) == NULL)
        if (inet_ntop(AF_INET6, &saddr.sin_addr, buf, bufsz) == NULL)
          *buf = '\0';
#endif
    }

    return(sfd);
}

/*
** Socket connect
*/
os_socket socket_connect(int port, char *ipaddr)
{
    os_socket          sfd;
    struct sockaddr_in saddr;

    saddr.sin_family = AF_INET;
    saddr.sin_port   = htons((unsigned short)port);
#ifdef MODOWA_WINDOWS
    saddr.sin_addr.S_un.S_addr = inet_addr(ipaddr);
#else
#ifdef USE_PTON
    if (inet_pton(AF_INET, ipaddr, &saddr.sin_addr) == 0)
    {
        saddr.sin_family = AF_INET6;
        inet_pton(AF_INET6, ipaddr, &saddr.sin_addr);
    }
#else
    /* Obsolete MacOS? */
    inet_aton(ipaddr, &saddr.sin_addr);

#ifdef NEVER /* Obsolete Solaris? */
    saddr.sin_addr.s_addr = inet_addr(ipaddr);
#endif

#endif
#endif

    sfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sfd != os_badsocket)
    {
        if (connect(sfd, (struct sockaddr *)&saddr, sizeof(saddr)))
        {
            socket_close(sfd);
            sfd = os_badsocket;
        }
    }
    return(sfd);
}

/*
** Write to a socket
*/
int socket_write(os_socket sock, char *buffer, int buflen)
{
    int n = 0;
    int m;
    while (n < buflen)
    {
#ifdef MODOWA_WINDOWS
      m = send(sock, buffer + n, buflen - n, 0);
#else
      m = write(sock, buffer + n, buflen - n);
#endif
      if (m > 0)
        n += m;
      else if (m < 0)
        return(m);
      else
        break;
    }
    return(n);
}

/*
** Read from a socket
*/
int socket_read(os_socket sock, char *buffer, int buflen)
{
    int n;
#ifdef MODOWA_WINDOWS
    n = recv(sock, buffer, buflen, 0);
#else
    n = read(sock, buffer, buflen);
#endif
    return(n);
}

/*
** Flush a socket
*/
int socket_flush(os_socket sock)
{
    int n;
#ifdef MODOWA_WINDOWS
    /* ### Unclear what the implementation is ### */
    n = 0;
#else
    n = fsync(sock);
#endif
    return(n);
}

/*
** Check for input available on a set of sockets
*/
int socket_test(os_socket *sock, int nsock, int ms)
{
    int            i;
    int            status;
#ifdef MODOWA_WINDOWS
    fd_set         rfds;
    struct timeval tv;

    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;

    FD_ZERO(&rfds);
    for (i = 0; i < nsock; ++i) FD_SET(sock[i], &rfds);

    status = select(0, &rfds, (fd_set *)0, (fd_set *)0, &tv);
    if (status > 0)
    {
        if (nsock == 1) return(1);
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        for (i = 0; i < nsock; ++i)
        {
            FD_ZERO(&rfds);
            FD_SET(sock[i], &rfds);
            if (select(0, &rfds, (fd_set *)0, (fd_set *)0, &tv)) return(i + 1);
        }
    }
#else
    struct pollfd  pfd[10];
    struct pollfd *ppfds;

    i = sizeof(pfd)/sizeof(*pfd);
    if (nsock > i)
    {
        ppfds = (struct pollfd *)mem_alloc(sizeof(*pfd) * nsock);
        if (!ppfds) nsock = i;
    }
    if (nsock <= i) ppfds = pfd;

    for (i = 0; i < nsock; ++i)
    {
        ppfds[i].fd = sock[i];
        ppfds[i].events = POLLIN;
        ppfds[i].revents = 0;
    }

    status = poll(ppfds, nsock, ms);
    if (status > 0)
      for (i = 0; i < nsock; ++i)
        if ((ppfds[i].revents & POLLIN) == POLLIN)
          break;

    if (ppfds != pfd) mem_free((void *)ppfds);
    if (i < nsock) return(i + 1);
#endif
    if (status < 0) return(0); /* ### Should return -1 for error? ### */
    return(0);
}

#endif /* SOCKETS */
