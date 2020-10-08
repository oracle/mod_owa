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
** File:         owautil.c
** Description:  Utility functions
** Author:       Doug McMahon      Doug.McMahon@oracle.com
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
**
** String functions
**   str_concat       length-limited string concatenation
**   str_compare      length-limited case-controlled comparison
**   str_char         reversible search for character in string
**   str_substr       case-controlled search for substring in string
**   str_dup          duplicate string
**   str_chrcmp       case-aware comparison of two characters
**   str_prepend      prepend a string to another string
**   str_ltoa         convert 64-bit long integer to string
**   str_itoa         convert integer to string
**   str_itox         convert integer to hex string
**   str_atoi         convert decimal string to integer
**   str_atox         convert hex string to integer
**   str_btox         convert binary array to hex string
**   str_xtob         convert hex string to binary array
**   mem_find         find binary pattern in binary buffer
**   mem_compare      compare two binary arrays
**
** Misc. functions
**   util_get_method  get HTTP method name given numeric code
**   util_print_time  print formatted date/time
**   util_iso_time    print iso formatted date/time
**   util_header_time print header formatted date/time
**   util_set_mime    determine mime type based on file extension
**   util_delta_time  compute microseconds between two times
**   util_checksum    compute 32-bit checksum of input buffer
**   util_ipaddr      convert IP address/mask string to integer
**   util_decode64    decode a base-64 encoded string
**   util_make_rand   create linear additive feedback random number generator
**   util_gen_rand    generate a value from random number generator
**   util_seed_rand   seed the state of a random number generator
**   util_get_rand    get a random value from a pair of generators
**   util_init_rand   initialize a pair of generators from a key
**   util_scramble    use openssl to unscramble a value
**   util_json_escape JSON escape from buffer to buffer
**   util_csv_escape  CSV escape from buffer to buffer
**
** History
**
** 08/17/2000   D. McMahon      Created
** 05/31/2001   D. McMahon      Began revisioning with 1.3.9
** 09/07/2001   D. McMahon      Added TIMING diagnostic
** 11/26/2001   D. McMahon      Added str_chrcmp
** 08/16/2002   D. McMahon      Add method name/number conversions
** 05/26/2003   D. McMahon      Add random number generator code
** 01/19/2004   D. McMahon      Added str_dup
** 03/07/2006   D. McMahon      Updated mime types
** 06/14/2006   D. McMahon      Moved mem_find from owadoc.c
** 09/15/2006   D. McMahon      Updated mime types again
** 05/07/2007   D. McMahon      Added mem_compare
** 05/10/2007   D. McMahon      Add str_btox, str_xtob
** 05/14/2008   D. McMahon      Fix a leap-year bug in util_print_time
** 09/02/2008   D. McMahon      Added util_delta_time
** 04/14/2011   D. McMahon      Avoid bad keys in util_init_rand
** 04/18/2011   D. McMahon      util_scramble
** 03/21/2012   D. McMahon      Add application/json content type.
** 05/14/2012   D. McMahon      Fix a bug in util_get_method
** 11/27/2012   D. McMahon      GCC fixes, Win-64 porting
** 12/17/2012   D. McMahon      Add util_iso_time
** 06/06/2013   D. McMahon      Add util_json_escape
** 06/21/2013   D. McMahon      Add util_csv_escape
** 09/19/2013   D. McMahon      Add str_ltoa
** 10/07/2020   D. McMahon      New mime types
*/

#include <modowa.h>

#ifdef USE_OPEN_SSL
#include <openssl/rc4.h>
#endif

static char owautil_hexstr[] = "0123456789ABCDEFabcdef";

/*
** Length-limited string concatenation, always null-terminates.
** Starts concatenation within the destination string at the
** point specified.  The maxlen protects the total size of the
** output buffer.  Returns the new total length of the string in
** dest.  With 0 for the starting offset, behaves like strncpy.
*/
int str_concat(char *dest, int dstart, const char *src, int maxlen)
{
    int i;

    if (maxlen < 0) maxlen = LONG_MAXSZ;

    if (!dest) return(-1);
    if (!src) src = "";

    dest += dstart;
    for (i = dstart; i < maxlen; ++i)
    {
        if (*src == '\0') break;
        *(dest++) = *(src++);
    }
    *dest = '\0';
    return(i);
}

/*
** Length-limited string comparison with case-control flag.
*/
int str_compare(const char *s1, const char *s2, int maxlen, int caseflag)
{
    int i;
    int n, m;

    if (maxlen < 0) maxlen = LONG_MAXSZ;

    if (s1 == s2) return(0);
    if (!s1) return(-1);
    if (!s2) return(+1);
    for (i = 0; i < maxlen; ++i, ++s1, ++s2)
    {
        n = ((int)*s1 & 0xFF);
        m = ((int)*s2 & 0xFF);
        if (caseflag)
        {
            if ((n >= 'A') && (n <= 'Z')) n += ('a' - 'A');
            if ((m >= 'A') && (m <= 'Z')) m += ('a' - 'A');
        }
        n -= m;
        if (n != 0) return(n);
        if (*s1 == '\0') break;
    }
    return(0);
}

/*
** Search for character in string, either from the beginning or
** in reverse from the end.
*/
char *str_char(const char *sptr, int ch, int reverse_flag)
{
    char *s;

    if (reverse_flag)
    {
        s = (char *)sptr + str_length(sptr);
        while (s > sptr)
            if (*(--s) == ch) break;
    }
    else
    {
        for (s = (char *)sptr; *s != ch; ++s)
            if (*s == '\0') break;
    }
    if (*s != ch) s = (char *)0;

    return(s);
}

/*
** Search for substring within string, optionally without case sensitivity.
*/
char *str_substr(const char *sptr, const char *ssub, int caseflag)
{
    int i;

    if ((sptr) && (ssub))
    {
        for (i = str_length(ssub); *sptr != '\0'; ++sptr)
            if (!str_compare(sptr, ssub, i, caseflag))
                return((char *)sptr);
    }
    return((char *)0);
}

/*
** Duplicate string
*/
char *str_dup(const char *sptr)
{
    char *newstr;

    if (!sptr) return((char *)0);
    newstr = (char *)mem_alloc(str_length(sptr) + 1);
    if (newstr) str_copy(newstr, sptr);
    return(newstr);
}

/*
** Case-aware comparison of two characters
*/
int str_chrcmp(char *a, char *b, int caseflag)
{
    int la = ((int)*a & 0xFF);
    int lb = ((int)*b & 0xFF);
    if (caseflag)
    {
        if ((la >= 'A') && (la <= 'Z')) la += ('a' - 'A');
        if ((lb >= 'A') && (lb <= 'Z')) lb += ('a' - 'A');
    }
    return(la - lb);
}

/*
** Prepend a string to another string; shift the base string to
** make room for the prefix, then copy it in.
*/
void str_prepend(char *base_str, char *prefix_str)
{
    int i, j;

    j = str_length(prefix_str);
    i = str_length(base_str) + 1;

    while (i > 0)
    {
        --i;
        base_str[j + i] = base_str[i];
    }

    mem_copy(base_str, prefix_str, j);
}

/*
** Convert 64-bit integer to string
*/
int str_ltoa(long_64 i, char *s)
{
    long_64  j;
    int      n;
    char    *sptr;
    char     buf[LONG_MAXSTRLEN];

    sptr = s;
    if (i <= 0)
    {
        j = -i;
        *(sptr++) = ((i == 0) ? '0' : '-');
    }
    else
    {
        j = i;
    }
    for (n = 0; j != 0; j = j/10) buf[n++] = (char)('0' + (j % 10));
    for (j = n - 1; j >= 0; --j) *(sptr++) = buf[j];
    *sptr = '\0';
    return((int)(sptr - s));
}

/*
** Convert integer to string
*/
int str_itoa(int i, char *s)
{
    int   j;
    int   n;
    char *sptr;
    char  buf[LONG_MAXSTRLEN];

    sptr = s;
    if (i <= 0)
    {
        j = -i;
        *(sptr++) = ((i == 0) ? '0' : '-');
    }
    else
    {
        j = i;
    }
    for (n = 0; j != 0; j = j/10) buf[n++] = '0' + (j % 10);
    for (j = n - 1; j >= 0; --j) *(sptr++) = buf[j];
    *sptr = '\0';
    return((int)(sptr - s));
}

/*
** Convert integer to hex string
*/
int str_itox(un_long i, char *s)
{
    int   j;

    for (j = 8; j > 0; i >>= 4) s[--j] = owautil_hexstr[i & 0xF];
    s[8] = '\0';
    return(8);
}

/*
** Convert decimal string to integer
*/
int str_atoi(char *s)
{
    int i;
    for (i = 0; (*s >= '0') && (*s <= '9'); ++s)
        i = (i * 10) + (*s - '0');
    return(i);
}

/*
** Convert hex string to integer
*/
un_long str_atox(char *s)
{
    char    *hptr;
    un_long  i;
    un_long  val;

    i = 0;
    while (*s)
    {
        hptr = str_char(owautil_hexstr, *(s++), 0);
        if (!hptr) break;
        val = (un_long)(hptr - owautil_hexstr);
        if (val >= 16) val -= 6;
        i = (i * 16) + val;
    }
    return(i);
}

/*
** Convert binary array to hex string
*/
char *str_btox(void *p, char *s, int len)
{
    unsigned char *ptr = (unsigned char *)p;
    char          *sptr;
    int            x;
    int            i;

    if (!s) return(s);

    sptr = s;

    for (i = 0; i < len; ++i)
    {
        x = (ptr[i] & 0xFF);
        *(sptr++) = owautil_hexstr[x >> 4];
        *(sptr++) = owautil_hexstr[x & 0xF];
    }
    *sptr = '\0';

    return(s);
}

/*
** Convert hex string to binary array
*/
int str_xtob(char *s, void *p)
{
    char *hptr;
    unsigned char *ptr = (unsigned char *)p;
    int   n;
    int   uval, lval;

    n = 0;
    while (*s)
    {
        hptr = str_char(owautil_hexstr, *(s++), 0);
        if (!hptr) break;
        uval = (int)(hptr - owautil_hexstr);
        if (uval >= 16) uval -= 6;
        hptr = str_char(owautil_hexstr, *(s++), 0);
        if (!hptr) break;
        lval = (int)(hptr - owautil_hexstr);
        if (lval >= 16) lval -= 6;
        ptr[n++] = (uval << 4) | lval;
    }
    return(n);
}

/*
** Scan buffer for pattern or part of pattern
*/
char *mem_find(char *mptr, long_64 mlen, const char *mpat, int plen)
{
    int i, n;

    n = plen;

    if ((plen > 0) && (mptr) && (mpat))
        while (mlen > 0)
        {
            if ((long_64)n > mlen) n = (int)mlen;
            for (i = 0; i < n; ++i)
                if (mptr[i] != mpat[i])
                    break;
            if (i == n) return(mptr);
            ++mptr;
            --mlen;
        }
    return((char *)0);
}

/*
** Compare two binary arrays
*/
int mem_compare(char *ptr1, int len1, char *ptr2, int len2)
{
    int            i, n;
    int            d1, d2;

    n = (len1 > len2) ? len2 : len1;

    for (i = 0; i < n; ++i)
    {
        d1 = ptr1[i] & 0xFF;
        d2 = ptr2[i] & 0xFF;
        if (d1 != d2) return(d1-d2);
    }

    if (len1 > len2) return(1);
    if (len2 > len1) return(-1);

    return(0);
}

static char *method_list[] = {"GET","PUT","POST","DELETE","CONNECT","OPTIONS",
                              "TRACE","PATCH","PROPFIND","PROPPATCH","MKCOL",
                              "COPY","MOVE","LOCK","UNLOCK","VERSION_CONTROL",
                              "CHECKOUT","UNCHECKOUT","CHECKIN","UPDATE",
                              "LABEL","REPORT","MKWORKSPACE","MKACTIVITY",
                              "BASELINE_CONTROL","MERGE"};
                              /* ### What about "HEAD"? ### */

/*
** Convert Apache numeric method number to HTTP string
*/
char *util_get_method(int m)
{
    if ((m < 0) || (m >= sizeof(method_list)/sizeof(*method_list))) return("");
    return(method_list[m]);
}

static const int days_in_year[]  = {365,365,366,365};
static const int days_in_month[] = {31,28,31,30,31,30,31,31,30,31,30,31,
                                    31,29,31,30,31,30,31,31,30,31,30,31};
static const char *mon_names[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                  "Jul","Aug","Sep","Oct","Nov","Dec"};
static const char *day_names[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

/*
** Print Unix time as YYYY/MM/DD HH24:MI:SS
**                 or YYYY/MM/DD HH24:MI:SS.FFFFFF
*/
void util_print_time(un_long t, char *outbuf, un_long *ms)
{
    int i;
    int leap;
    int secs;
    int days;
    int year;
    int mon;
    int hr;
    int min;

    days = (int)(t/(24*60*60));
    secs = (int)(t - ((un_long)days * 24*60*60));
    year = days/(365*4 + 1);
    days -= (year * (365*4 + 1));
    year = 1970 + year * 4;
    for (i = 0; i < 4; ++i)
        if (days < days_in_year[i])
            break;
        else
            days -= days_in_year[i];
    leap = (i == 2) ? 12 : 0;
    year += i;
    for (i = 0; i < 12; ++i)
      if (days < days_in_month[i + leap])
          break;
      else
          days -= days_in_month[i + leap];
    mon = i + 1;
    ++days;
    hr = secs/(60*60);
    secs -= (hr * 60*60);
    min = secs/60;
    secs -= (min * 60);
    os_str_print(outbuf, "%4d/%2.2d/%2.2d %2.2d:%2.2d:%2.2d",
                 year, mon, days, hr, min, secs);
    if (ms) os_str_print(outbuf + 19, ".%6.6ld", *ms);
}

void util_print_component_time(long_64 tval, char *outbuf)
{
    int secs;
    int days;
    int year;
    int mon;
    int hr;
    int min;
    int ms;

    /* Extract the time components */
    ms = (int)(tval & (long_64)0x000FFFFF);
    tval >>= 20;
    secs = (int)(tval & (long_64)0x003F);
    tval >>= 6;
    min = (int)(tval & (long_64)0x003F);
    tval >>= 6;
    hr = (int)(tval & (long_64)0x001F);
    tval >>= 5;
    days = (int)(tval & (long_64)0x001F);
    tval >>= 5;
    mon = (int)(tval & (long_64)0x000F);
    tval >>= 4;
    year = (int)(tval & (long_64)0x3FFF);

    os_str_print(outbuf, "%4d/%2.2d/%2.2d %2.2d:%2.2d:%2.2d.%6.6d",
                 year, mon, days, hr, min, secs, ms);
}

/*
** Print unit time in ISO time format, e.g. 2000-01-30T12:34:56.999999
*/
void util_iso_time(long_64 tval, char *outbuf)
{
  util_print_component_time(tval, outbuf);
  if (outbuf[4] == '/') outbuf[4] = '-';
  if (outbuf[6] == '/') outbuf[6] = '-';
  if (outbuf[10] == ' ') outbuf[10] = 'T';
}

/*
** Print Unix time as in this example:
**
** Mon, 09 Oct 2000 16:20:00 GMT
*/
void util_header_time(long_64 tval, char *outbuf, char *tz)
{
    int secs;
    int days;
    int year;
    int mon;
    int hr;
    int min;
    char *dname;

    /* Extract the time components */
    tval >>= 20;
    secs = (int)(tval & (long_64)0x003F);
    tval >>= 6;
    min = (int)(tval & (long_64)0x003F);
    tval >>= 6;
    hr = (int)(tval & (long_64)0x001F);
    tval >>= 5;
    days = (int)(tval & (long_64)0x001F);
    tval >>= 5;
    mon = (int)(tval & (long_64)0x000F);
    tval >>= 4;
    year = (int)(tval & (long_64)0x3FFF);

    dname = (char *)day_names[(days + 3) % 7]; /* Thursday */

    if (!tz) tz = "GMT";

    os_str_print(outbuf, "%s, %2.2d %s %4.4d %2.2d:%2.2d:%2.2d %s",
                 dname, days, mon_names[mon - 1], year, hr, min, secs, tz);
}

/*
** Mime type determination
** ### This is a kludge.  Apache should provide some means for
** ### getting a mime type based on a file extension.
*/

/*
** Sort-of order by frequency because underlying search is linear
*/
static const char mime_list[] =
"[text/html]"                       " html htm "
"[image/gif]"                       " gif "
"[image/jpeg]"                      " jpeg jpg jpe "
"[text/plain]"                      " asc txt "
"[text/css]"                        " css "
"[text/xml]"                        " xml "
"[text/csv]"                        " csv "
"[application/json]"                " json "
"[application/x-javascript]"        " js "
"[image/png]"                       " png "
"[image/tiff]"                      " tiff tif "
"[image/bmp]"                       " bmp "
"[image/x-icon]"                    " ico "
"[image/svg+xml]"                   " svg "
"[application/x-bzip]"              " bz "
"[application/x-bzip2]"             " bz2 "
"[application/x-7z-compressed]"     " 7z "
"[application/java-archive]"        " jar "
"[application/vnd.amazon.ebook]"    " azw "
"[application/x-shockwave-flash]"   " swf "
"[application/msword]"              " doc "
"[application/vnd.ms-excel]"        " xls "
"[application/vnd.ms-powerpoint]"   " ppt "
"[application/vnd.mozilla.xul+xml]" " xul "
"[application/pdf]"                 " pdf "
"[application/rtf]"                 " rtf "
"[application/x-gunzip]"            " gz "
"[application/zip]"                 " zip "
"[application/x-tar]"               " tar "
"[application/vnd.openxmlformats-officedocument.presentationml.presentation]"
                                    " pptx "
"[application/vnd.openxmlformats-officedocument.wordprocessingml.document]"
                                    " docx "
"[application/vnd.ms-fontobject]"   " eot "
"[application/epub+zip]"            " epub "
"[audio/aac]"                       " aac "
"[audio/x-wav]"                     " wav "
"[audio/mpeg]"                      " mpga mp2 mp3 "
"[audio/basic]"                     " au snd "
"[audio/midi]"                      " mid midi kar "
"[audio/x-aiff]"                    " aif aiff aifc "
"[audio/x-pn-realaudio]"            " ram rm "
"[audio/x-pn-realaudio-plugin]"     " rpm "
"[audio/x-realaudio]"               " ra "
"[audio/x-mpegurl]"                 " m3u "
"[video/mpeg]"                      " mpeg mpg mpe "
"[video/quicktime]"                 " qt mov "
"[video/vnd.mpegurl]"               " mxu m4u "
"[text/javascript]"                 " mjs "
"[text/richtext]"                   " rtx "
"[text/rtf]"                        " rtf "
"[text/sgml]"                       " sgml sgm "
"[text/tab-separated-values]"       " tsv "
"[text/vnd.wap.wml]"                " wml "
"[text/vnd.wap.wmlscript]"          " wmls "
"[text/x-setext]"                   " etx "
"[text/calendar]"                   " ics ifb "
"[image/ief]"                       " ief "
"[image/cgm]"                       " cgm "
"[image/vnd.wap.wbmp]"              " wbmp "
"[image/vnd.djvu]"                  " djvu djv "
"[image/x-cmu-raster]"              " ras "
"[image/x-portable-anymap]"         " pnm "
"[image/x-portable-bitmap]"         " pbm "
"[image/x-portable-graymap]"        " pgm "
"[image/x-portable-pixmap]"         " ppm "
"[image/x-rgb]"                     " rgb "
"[image/x-xbitmap]"                 " xbm "
"[image/x-xpixmap]"                 " xpm "
"[image/x-xwindowdump]"             " xwd "
"[application/xml]"                 " xsl "
"[application/xml-dtd]"             " dtd "
"[application/xhtml+xml]"           " xhtml xht "
"[application/xslt+xml]"            " xslt "
"[application/andrew-inset]"        " ez "
"[application/mathml+xml]"          " mathml "
"[application/mac-binhex40]"        " hqx "
"[application/mac-compactpro]"      " cpt "
"[application/octet-stream]"        " bin dms lha lzh exe class so dll dmg "
"[application/oda]"                 " oda "
"[application/ogg]"                 " ogg "
"[application/rdf+xml]"             " rdf "
"[application/postscript]"          " ai eps ps "
"[application/smil]"                " smi smil "
"[application/vnd.mif]"             " mif "
"[application/vnd.wap.wbxml]"       " wbxml "
"[application/vnd.wap.wmlc]"        " wmlc "
"[application/vnd.wap.wmlscriptc]"  " wmlsc "
"[application/srgs]"                " gram "
"[application/srgs+xml]"            " grxml "
"[application/voicexml+xml]"        " vxml "
"[application/x-bcpio]"             " bcpio "
"[application/x-cdlink]"            " vcd "
"[application/x-chess-pgn]"         " pgn "
"[application/x-cpio]"              " cpio "
"[application/x-csh]"               " csh "
"[application/x-director]"          " dcr dir dxr "
"[application/x-dvi]"               " dvi "
"[application/x-futuresplash]"      " spl "
"[application/x-gtar]"              " gtar "
"[application/x-hdf]"               " hdf "
"[application/x-koan]"              " skp skd skt skm "
"[application/x-latex]"             " latex "
"[application/x-netcdf]"            " nc cdf "
"[application/x-sh]"                " sh "
"[application/x-shar]"              " shar "
"[application/x-stuffit]"           " sit "
"[application/x-sv4cpio]"           " sv4cpio "
"[application/x-sv4crc]"            " sv4crc "
"[application/x-tcl]"               " tcl "
"[application/x-tex]"               " tex "
"[application/x-texinfo]"           " texinfo texi "
"[application/x-troff]"             " t tr roff "
"[application/x-troff-man]"         " man "
"[application/x-troff-me]"          " me "
"[application/x-troff-ms]"          " ms "
"[application/x-ustar]"             " ustar "
"[application/x-wais-source]"       " src "
"[chemical/x-pdb]"                  " pdb "
"[chemical/x-xyz]"                  " xyz "
"[model/iges]"                      " igs iges "
"[model/mesh]"                      " msh mesh silo "
"[model/vrml]"                      " wrl vrml "
"[video/x-msvideo]"                 " avi "
"[video/x-sgi-movie]"               " movie "
"[x-conference/x-cooltalk]"         " ice "
"\0";

/*
** Determine mime type based on file name extension
*/
void util_set_mime(char *fpath, char *pmimetype, int bin_flag)
{
    int   i;
    char *nptr, *sptr;
    char *ext;

#ifdef DOESNT_WORK
    request_rec   *subr;
    subr = ap_sub_req_lookup_uri(fpath, r);
    if (subr)
      if (subr->content_type)
      {
        str_copy(pmimetype, (char *)(subr->content_type));
        return;
      }
#endif

    if (*pmimetype) return;

    sptr = str_char(fpath, '.', 1);
    if (sptr)
    {
        ++sptr;
        ext = pmimetype;
        *ext = ' ';
        i = str_concat(ext + 1, 0, sptr, 64); /* ### ARBITRARY MAXIMUM ### */
        ext[++i] = ' ';
        ext[++i] = '\0';

        sptr = str_substr(mime_list, ext, 1);
        if (sptr)
        {
            while (*sptr != '[') --sptr;
            for (nptr = ++sptr; *nptr != ']'; ++nptr);
            str_concat(pmimetype, 0, sptr, (int)(nptr - sptr));
            return;
        }
    }

    str_copy(pmimetype, (bin_flag) ? "application/octet-stream": "text/plain");
}

/*
** Julian dates
*/
#define UTIL_JDATE_0001  1721424 /* January 1, 0001 - first day AD   */
#define UTIL_JDATE_1601  2305814 /* January 1, 1601 - after glitch   */
#define UTIL_JDATE_1970  2440588 /* January 1, 1970 - unix time base */

/*
** Convert a component (bit-field) time to a tick count (stamp)
*/
long_64 util_component_to_stamp(long_64 tval)
{
    long_64 result;
    int     yr, mon, day, hr, min, sec, ms;

    /* Extract the time components */
    ms = (int)(tval & (long_64)0x000FFFFF);
    tval >>= 20;
    sec = (int)(tval & (long_64)0x003F);
    tval >>= 6;
    min = (int)(tval & (long_64)0x003F);
    tval >>= 6;
    hr = (int)(tval & (long_64)0x001F);
    tval >>= 5;
    day = (int)(tval & (long_64)0x001F);
    tval >>= 5;
    mon = (int)(tval & (long_64)0x000F);
    tval >>= 4;
    yr = (int)(tval & (long_64)0x3FFF);

    /* Sanity check on the fields */
    if ((yr < 1601) || (yr > 9999)) return((long_64)0);
    if ((mon < 1) || (mon > 12))    return((long_64)0);
    if ((day < 1) || (day > 31))    return((long_64)0);
    if ((hr < 0) || (hr > 23))      return((long_64)0);
    if ((min < 0) || (min > 59))    return((long_64)0);
    if ((sec < 0) || (sec > 59))    return((long_64)0);

    /* Compute days since 1601, adjusting for leap years */
    yr -= 1601;
    result = yr * 365 + yr/4 - yr/100 + yr/400;
    if (((yr % 4) == 3) && (((yr % 100) != 99) || (yr % 400) == 399))
      /* If this is a leap year, add a day if we're past Feb */
      if (mon > 2) ++result;

    --mon; /* Months are 1-based, so subtract one */
    /* Add in the days from the elapsed months */
    while (mon > 0) result += days_in_month[--mon];
    --day; /* Days are 1-based, so subtract one */
    /* Add the days */
    result += day;
    /* Convert to hours and add hours */
    result = (result * (long_64)24) + (long_64)hr;
    /* Convert to minutes and add minutes */
    result = (result * (long_64)60) + (long_64)min;
    /* Convert to seconds and add seconds */
    result = (result * (long_64)60) + (long_64)sec;
    /* Add the microseconds */
    result = (result * (long_64)1000000) + (long_64)ms;
    /* Adjust back from 1601 baseline */
    result += ((long_64)UTIL_JDATE_1601 *
               (long_64)(24*60*60) * (long_64)1000000);
    return(result);
}

/*
** Get microseconds elapsed between two component times
*/
long_64 util_delta_time(long_64 stime, long_64 etime)
{
    long_64 start_ticks = util_component_to_stamp(stime);
    long_64 end_ticks   = util_component_to_stamp(etime);
    return(end_ticks - start_ticks);
}

/*
** 32-bit CRC, pre-calculated based on the polynomial:
**
**   x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11 +
**          x^10 + x^8  + x^7  + x^5  + x^4  + x^2  + x    + 1
**
*/
static const un_long util_crc32[] = {
    0x00000000,0x77073096,0xee0e612c,0x990951ba,0x076dc419,0x706af48f,
    0xe963a535,0x9e6495a3,0x0edb8832,0x79dcb8a4,0xe0d5e91e,0x97d2d988,
    0x09b64c2b,0x7eb17cbd,0xe7b82d07,0x90bf1d91,0x1db71064,0x6ab020f2,
    0xf3b97148,0x84be41de,0x1adad47d,0x6ddde4eb,0xf4d4b551,0x83d385c7,
    0x136c9856,0x646ba8c0,0xfd62f97a,0x8a65c9ec,0x14015c4f,0x63066cd9,
    0xfa0f3d63,0x8d080df5,0x3b6e20c8,0x4c69105e,0xd56041e4,0xa2677172,
    0x3c03e4d1,0x4b04d447,0xd20d85fd,0xa50ab56b,0x35b5a8fa,0x42b2986c,
    0xdbbbc9d6,0xacbcf940,0x32d86ce3,0x45df5c75,0xdcd60dcf,0xabd13d59,
    0x26d930ac,0x51de003a,0xc8d75180,0xbfd06116,0x21b4f4b5,0x56b3c423,
    0xcfba9599,0xb8bda50f,0x2802b89e,0x5f058808,0xc60cd9b2,0xb10be924,
    0x2f6f7c87,0x58684c11,0xc1611dab,0xb6662d3d,0x76dc4190,0x01db7106,
    0x98d220bc,0xefd5102a,0x71b18589,0x06b6b51f,0x9fbfe4a5,0xe8b8d433,
    0x7807c9a2,0x0f00f934,0x9609a88e,0xe10e9818,0x7f6a0dbb,0x086d3d2d,
    0x91646c97,0xe6635c01,0x6b6b51f4,0x1c6c6162,0x856530d8,0xf262004e,
    0x6c0695ed,0x1b01a57b,0x8208f4c1,0xf50fc457,0x65b0d9c6,0x12b7e950,
    0x8bbeb8ea,0xfcb9887c,0x62dd1ddf,0x15da2d49,0x8cd37cf3,0xfbd44c65,
    0x4db26158,0x3ab551ce,0xa3bc0074,0xd4bb30e2,0x4adfa541,0x3dd895d7,
    0xa4d1c46d,0xd3d6f4fb,0x4369e96a,0x346ed9fc,0xad678846,0xda60b8d0,
    0x44042d73,0x33031de5,0xaa0a4c5f,0xdd0d7cc9,0x5005713c,0x270241aa,
    0xbe0b1010,0xc90c2086,0x5768b525,0x206f85b3,0xb966d409,0xce61e49f,
    0x5edef90e,0x29d9c998,0xb0d09822,0xc7d7a8b4,0x59b33d17,0x2eb40d81,
    0xb7bd5c3b,0xc0ba6cad,0xedb88320,0x9abfb3b6,0x03b6e20c,0x74b1d29a,
    0xead54739,0x9dd277af,0x04db2615,0x73dc1683,0xe3630b12,0x94643b84,
    0x0d6d6a3e,0x7a6a5aa8,0xe40ecf0b,0x9309ff9d,0x0a00ae27,0x7d079eb1,
    0xf00f9344,0x8708a3d2,0x1e01f268,0x6906c2fe,0xf762575d,0x806567cb,
    0x196c3671,0x6e6b06e7,0xfed41b76,0x89d32be0,0x10da7a5a,0x67dd4acc,
    0xf9b9df6f,0x8ebeeff9,0x17b7be43,0x60b08ed5,0xd6d6a3e8,0xa1d1937e,
    0x38d8c2c4,0x4fdff252,0xd1bb67f1,0xa6bc5767,0x3fb506dd,0x48b2364b,
    0xd80d2bda,0xaf0a1b4c,0x36034af6,0x41047a60,0xdf60efc3,0xa867df55,
    0x316e8eef,0x4669be79,0xcb61b38c,0xbc66831a,0x256fd2a0,0x5268e236,
    0xcc0c7795,0xbb0b4703,0x220216b9,0x5505262f,0xc5ba3bbe,0xb2bd0b28,
    0x2bb45a92,0x5cb36a04,0xc2d7ffa7,0xb5d0cf31,0x2cd99e8b,0x5bdeae1d,
    0x9b64c2b0,0xec63f226,0x756aa39c,0x026d930a,0x9c0906a9,0xeb0e363f,
    0x72076785,0x05005713,0x95bf4a82,0xe2b87a14,0x7bb12bae,0x0cb61b38,
    0x92d28e9b,0xe5d5be0d,0x7cdcefb7,0x0bdbdf21,0x86d3d2d4,0xf1d4e242,
    0x68ddb3f8,0x1fda836e,0x81be16cd,0xf6b9265b,0x6fb077e1,0x18b74777,
    0x88085ae6,0xff0f6a70,0x66063bca,0x11010b5c,0x8f659eff,0xf862ae69,
    0x616bffd3,0x166ccf45,0xa00ae278,0xd70dd2ee,0x4e048354,0x3903b3c2,
    0xa7672661,0xd06016f7,0x4969474d,0x3e6e77db,0xaed16a4a,0xd9d65adc,
    0x40df0b66,0x37d83bf0,0xa9bcae53,0xdebb9ec5,0x47b2cf7f,0x30b5ffe9,
    0xbdbdf21c,0xcabac28a,0x53b39330,0x24b4a3a6,0xbad03605,0xcdd70693,
    0x54de5729,0x23d967bf,0xb3667a2e,0xc4614ab8,0x5d681b02,0x2a6f2b94,
    0xb40bbe37,0xc30c8ea1,0x5a05df1b,0x2d02ef8d};

/*
** Compute 32-bit CRC on input buffer
*/
un_long util_checksum(char *buffer, int buflen)
{
    int     i;
    un_long crc;

    crc = (~(un_long)0);

    for (i = 0; i < buflen; ++i)
        crc = util_crc32[(crc ^ buffer[i]) & 0xFF] ^ (crc >> 8);

    crc ^= (~(un_long)0);

    return(crc);
}

/*
** Convert IP address or mask from string to integer
*/
un_long util_ipaddr(char *ipstr)
{
    un_long ip;
    for (ip = 0; *ipstr != '\0'; ++ipstr)
    {
        if ((*ipstr >= '0') && (*ipstr <= '9'))
            ip = (ip & ~0xFF) | (((ip & 0xFF) * 10) + (*ipstr - '0'));
        else if (*ipstr == '.')
            ip <<= 8;
    }
    return(ip);
}

/*
** Round an integer to next highest multiple of another integer
*/
un_long util_round(un_long n, un_long r)
{
    un_long i;
    if (!r) return(n);
    i = n % r;
    if (i > 0) i = r - i;
    return(n + i);
}

/*
** Decode a base-64 encoded string
*/
int util_decode64(char *inbuf, char *outbuf)
{
    int   i, j;
    int   val;
    int   bits;
    char *bptr;
    char *b64;

    b64 = (char *)
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    bits = 0;
    val = 0;
    j = 0;

    for (i = 0; inbuf[i] != '\0'; ++i)
    {
        bptr = str_char(b64, inbuf[i], 0);
        if (!bptr) break;
        val = (val << 6) | ((int)(bptr - b64));
        bits += 6;
        if (bits >= 8)
        {
            outbuf[j++] = (val >> (bits - 8)) & 0xFF;
            bits -= 8;
            val &= (0xFF >> (8 - bits));
        }
    }
    outbuf[j] = '\0';
    return(j);
}

/*
** Array of default polynomials indexed by sequence length.  The
** default polynomial for any length n is always (X[n] + X[k] + 0),
** where k is the constant from the table (i.e., these are all
** polynomials with three coefficients, one of which is n and the
** other obtained from the table).  In cases where no polynomial
** exists with two coefficients, the table contains a 0 -- in these
** cases, the next-lower non-zero value should be used.
*/
static int util_poly64[] =
       {0,0,1,1,1,2,1,1,0,4,3,2,0,0,0,1,0,3,7,0,3,2,1,5,0,3,0,0,3,2,0,3,0,
        13,0,2,11,0,0,4,0,3,0,0,0,0,0,5,0,9,0,0,3,0,0,24,0,7,19,0,1,0,0,1};

/*
** util_make_rand
**
** Make an additive feedback generator.  An additive feedback generator
** generates a sequence of values from:
**
**   X[i] = f(X[i - a], X[i - b], ..., X[i - m]) mod 2^n
**
** In this implementation, the generator works by keeping previous
** states up to a given cycle length, and adding back one tap.  The
** function f() can be non-linear, though the default combination
** function is addition.  n = 32 (so that the generator works on 32-bit
** machines).  Thus, the default sequence is of the form:
**
**   X[i] = (X[i - a] + X[i - b]) mod 2^32
**
** where "a" is the cycle length, and "b" is a feedback tap location.
** To maximize the cycle length for a given "a", "b" should be chosen
** so that together (a,b,0) is a primitive polynomial.
**
**      prand           Pointer to the structure to be initialized
**      length          Desired cycle length, up to 63
**      tap             Desired tap location, or 0 to use an internal default
*/
void util_make_rand(randstate *prand, int length, int tap)
{
    int cycle;

    if ((!prand) || (length < 2)) return; /* Fatal errors */

    cycle = length;

    if ((tap == 0) || (tap >= cycle))
    {
        /* Find a default from the table */
        cycle = sizeof(util_poly64)/sizeof(*util_poly64) - 1;
        if (cycle > length) cycle = length;
        while (util_poly64[cycle] == 0) --cycle;
        prand->tap = util_poly64[cycle];
    }
    else
        prand->tap = tap;

    prand->length = cycle;
    prand->ind = 0;

    /* Zero the state vector */
    while (cycle > 0) prand->state[--cycle] = 0;
}

/*
** util_gen_rand
**
** Additive feedback generator.  This generates a sequence of numbers:
**
**   X[i] = (X[i - a] + X[i - b]) mod 2^32
**
**      prand           Pointer to the state structure
**
**      returns:        Next sequence value
*/
un_long util_gen_rand(randstate *prand)
{
    int     length;
    un_long nxt, tap;
    un_long result;

    length = prand->length;

    nxt = prand->state[prand->ind];
    tap = prand->state[((prand->ind + length) - prand->tap) % length];

    result = nxt + tap;
    prand->carry = ((result < nxt) || (result < tap));

    prand->state[prand->ind] = result;
    prand->ind = (prand->ind + 1) % length;

    return(result);
}

/*
** util_seed_rand
**
** Seed the data for an additive feedback system.  The contents of the
** seed array are XORed to the state array, and then the generator is
** clocked through an equivalent number of steps.  Multiple calls can
** be made to seed successive values.
**
**      prand           Pointer to the state structure
**      srand           Pointer to seed data generator
*/
void util_seed_rand(randstate *prand, randstate *srand)
{
    int      i;

    for (i = 0; i < prand->length; ++i)
    {
        prand->state[prand->ind] ^= util_gen_rand(srand);
        util_gen_rand(prand);
    }
}

/*
** util_get_rand
**
** Generate random number from a pair of generators (a, b) according
** to the following algorithm:
**
**   1. If carry(a), clock(b)
**   2. If carry(b), clock(a)
**   3. Clock(a), set carry bit
**   4. Clock(b), set carry bit
**   5. Output (A XOR B)
**
**      arand           Pointer to the state structure for generator a
**      brand           Pointer to the state structure for generator b
**
**      returns:        Next random value
*/
un_long util_get_rand(randstate *arand, randstate *brand)
{
    int bcarry;

    if (!arand) return(util_gen_rand(brand));
    if (!brand) return(util_gen_rand(arand));

    bcarry = brand->carry;
    if (arand->carry) util_gen_rand(brand);
    if (bcarry) util_gen_rand(arand);

    return(util_gen_rand(arand) ^ util_gen_rand(brand));
}

/*
** util_init_rand
**
** Seed a pair of random number generators from a key value.
**
**      arand           Pointer to the state structure for generator a
**      brand           Pointer to the state structure for generator b
**      key             Seed value (array of bytes)
**      keylen          Number of bytes in the seed
*/
void util_init_rand(randstate *arand, randstate *brand, char *key, int keylen)
{
    int       i;
    int       keymax;
    randstate trand;

    keymax = (keylen < RAND_MAX_STATES) ? keylen : RAND_MAX_STATES;
    if (keymax < 2) keymax = 2;

    /* Spread whatever randomness exists across more bits */
    util_make_rand(&trand, keymax, 0);
    for (i = 0; i < keylen; ++i)
        trand.state[i % trand.length] ^= util_checksum(key, i + 1);
    for (i = 0; i < trand.length; ++i)
        util_gen_rand(&trand);

    util_make_rand(arand, 55, 24);
    util_make_rand(brand, 52, 19);

    util_seed_rand(arand, &trand);
    util_seed_rand(brand, &trand);
}

#ifndef USE_OPEN_SSL
#ifdef ENABLE_CRYPT
#ifdef MODOWA_WINDOWS

#include <wincrypt.h>

/*
** Microsoft Q228786
*/

static BYTE dummy_blob[] =
{
  0x07, 0x02, 0x00, 0x00, 0x00, 0xA4, 0x00, 0x00,
  0x52, 0x53, 0x41, 0x32, 0x00, 0x02, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0xAB, 0xEF, 0xFA, 0xC6,
  0x7D, 0xE8, 0xDE, 0xFB, 0x68, 0x38, 0x09, 0x92,
  0xD9, 0x42, 0x7E, 0x6B, 0x89, 0x9E, 0x21, 0xD7,
  0x52, 0x1C, 0x99, 0x3C, 0x17, 0x48, 0x4E, 0x3A,
  0x44, 0x02, 0xF2, 0xFA, 0x74, 0x57, 0xDA, 0xE4,
  0xD3, 0xC0, 0x35, 0x67, 0xFA, 0x6E, 0xDF, 0x78,
  0x4C, 0x75, 0x35, 0x1C, 0xA0, 0x74, 0x49, 0xE3,
  0x20, 0x13, 0x71, 0x35, 0x65, 0xDF, 0x12, 0x20,
  0xF5, 0xF5, 0xF5, 0xC1, 0xED, 0x5C, 0x91, 0x36,
  0x75, 0xB0, 0xA9, 0x9C, 0x04, 0xDB, 0x0C, 0x8C,
  0xBF, 0x99, 0x75, 0x13, 0x7E, 0x87, 0x80, 0x4B,
  0x71, 0x94, 0xB8, 0x00, 0xA0, 0x7D, 0xB7, 0x53,
  0xDD, 0x20, 0x63, 0xEE, 0xF7, 0x83, 0x41, 0xFE,
  0x16, 0xA7, 0x6E, 0xDF, 0x21, 0x7D, 0x76, 0xC0,
  0x85, 0xD5, 0x65, 0x7F, 0x00, 0x23, 0x57, 0x45,
  0x52, 0x02, 0x9D, 0xEA, 0x69, 0xAC, 0x1F, 0xFD,
  0x3F, 0x8C, 0x4A, 0xD0,

  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  0x64, 0xD5, 0xAA, 0xB1, 0xA6, 0x03, 0x18, 0x92,
  0x03, 0xAA, 0x31, 0x2E, 0x48, 0x4B, 0x65, 0x20,
  0x99, 0xCD, 0xC6, 0x0C, 0x15, 0x0C, 0xBF, 0x3E,
  0xFF, 0x78, 0x95, 0x67, 0xB1, 0x74, 0x5B, 0x60,

  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static BYTE template_blob[] =
{
  0x01, 0x02, 0x00, 0x00,        /* bType, bVersion, reserved x 2     */
  0x01, 0x68, 0x00, 0x00,        /* key algorithm: CALG_RC4           */
  0x00, 0xA4, 0x00, 0x00,        /* CALG_RSA_KEYX                     */
  /*
  ** PKCS #1, type 2 encryption block; MS Base CP requires 64 bytes
  */
  0x05, 0x04, 0x03, 0x02, 0x01,  /* RC4 key goes in reverse from here */
  0x00,                          /* null terminator byte              */
  /* non-zero padding bytes out to 64 total */
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x02, 0x00,                    /* Block type; reserved              */
};
#endif
#endif
#endif

/*
** util_json_escape
**
** Escape a JSON string using escaping rules, optionally
** adding surrounding quotes. The output buffer is assumed to be
** large enough.
**
**      json_out        JSON escaped output
**      buf_in          Input buffer (null-terminated)
**      quote_flag      non-0 for double quotes
**      uni_flag        non-0 for unicode escaping of non-ASCII characters
*/
int util_json_escape(char *json_out, char *buf_in, int quote_flag, int uni_flag)
{
  char *optr = json_out;

  if (quote_flag) *(optr++) = '"';

  while (*buf_in)
  {
    int ch = *buf_in & 0xFF;

    /* Mandatory escapes of syntax characters */
    if ((ch == '"') || (ch == '\\'))
    {
      *(optr++) = '\\';
      *(optr++) = ch;
      ++buf_in;
    }
    /* Escapes of control characters */
    else if (ch < ' ')
    {
      *(optr++) = '\\';
      switch (ch)
      {
      case '\n': *(optr++) = 'n'; break;
      case '\t': *(optr++) = 't'; break;
      case '\r': *(optr++) = 'r'; break;
      case '\b': *(optr++) = 'b'; break;
      case '\f': *(optr++) = 'f'; break;
      default:
        *(optr++) = 'u';
        *(optr++) = '0';
        *(optr++) = '0';
        *(optr++) = owautil_hexstr[ch >> 4];
        *(optr++) = owautil_hexstr[(ch & 0xF)];
        break;
      }
      ++buf_in;
    }
    /* Escape utf-8 unicode if requested */
    else if ((ch & 0x80) && (uni_flag))
    {
      int unichr;
      int nbytes;

      unichr = nls_utf8_char(buf_in, &nbytes);
      if (unichr < 0)
      {
        /* ### Error - invalid byte sequence ### */
        *(optr++) = ch;
        ++buf_in;
      }
      else if (unichr > 0xFFFF)
      {
        /*
        ** ### This UTF-16 conversion logic should be in owanls.c
        */
        int hichr, lochr;
        unichr -= 0x10000;
        hichr = ((unichr >> 10) & 0x3FF) + 0xD800;
        lochr = (unichr & 0x3FF) + 0xDC00;

        *(optr++) = '\\';
        *(optr++) = 'u';
        *(optr++) = owautil_hexstr[(hichr >> 12) & 0xF];
        *(optr++) = owautil_hexstr[(hichr >> 8) & 0xF];
        *(optr++) = owautil_hexstr[(hichr >> 4) & 0xF];
        *(optr++) = owautil_hexstr[hichr & 0xF];

        *(optr++) = '\\';
        *(optr++) = 'u';
        *(optr++) = owautil_hexstr[(lochr >> 12) & 0xF];
        *(optr++) = owautil_hexstr[(lochr >> 8) & 0xF];
        *(optr++) = owautil_hexstr[(lochr >> 4) & 0xF];
        *(optr++) = owautil_hexstr[lochr & 0xF];

        buf_in += nbytes;
      }
      else
      {
        *(optr++) = '\\';
        *(optr++) = 'u';
        *(optr++) = owautil_hexstr[(unichr >> 12) & 0xF];
        *(optr++) = owautil_hexstr[(unichr >> 8) & 0xF];
        *(optr++) = owautil_hexstr[(unichr >> 4) & 0xF];
        *(optr++) = owautil_hexstr[unichr & 0xF];

        buf_in += nbytes;
      }
    }
    else
    {
      *(optr++) = ch;
      ++buf_in;
    }
  }

  if (quote_flag) *(optr++) = '"';
  *optr = '\0';

  return((int)(optr - json_out));
}

/*
** util_csv_escape
**
** Escape a CSV string using escaping rules, adding surrounding quotes.
** The output buffer is assumed to be large enough.
**
**      csv_out         CSV escaped output
**      buf_in          Input buffer (null-terminated)
*/
int util_csv_escape(char *csv_out, char *buf_in, int delim)
{
  int   ch;
  int   quote_flag = 0;
  char *optr = csv_out;

  char *iptr;
  for (iptr = buf_in; *iptr; ++iptr)
  {
    ch = *iptr & 0xFF;
    if ((ch < ' ') || (ch & 0x80) || (ch == '"') || (ch == delim))
      break;
  }

  quote_flag = (*iptr != '\0');

  if (quote_flag) *(optr++) = '"';

  while (*buf_in)
  {
    ch = *(buf_in++) & 0xFF;

    /* Mandatory escapes of the double quote */
    if (ch == '"') *(optr++) = ch;
    *(optr++) = ch;
  }

  if (quote_flag) *(optr++) = '"';
  *optr = '\0';

  return((int)(optr - csv_out));
}

/*
** util_ncname_convert
**
** Convert a string to a valid NCNAME. Brutally replaces spaces with "_"
** and replaces invalid punctuation marks with "-". Skips non-ASCII
** and control characters entirely. Prepends an "_" if the name starts
** with a digit. Valid characters are:  _ - . A-Z a-z 0-9
**
**      ncname_out      Output
**      buf_in          Input buffer (null-terminated)
*/
int util_ncname_convert(char *ncname_out, char *buf_in)
{
  char *optr = ncname_out;
  int   first_flag = 1;

  while (*buf_in)
  {
    int ch = *(buf_in++) & 0xFF;

    /* Skip control characters and non-ASCII characters */
    if ((ch < ' ') || (ch > '~')) continue;

    if ((ch == ' ') || (ch == '_'))
      ch = '_'; /* Replace spaces with underscores */
    else if ((ch == '-') || (ch == '.') || ((ch >= '0') && (ch <= '9')))
    {
      if (first_flag)
        *(optr++) = '_'; /* Prepend an underscore if start isn't valid */
    }
    else if (((ch >= 'A') && (ch <= 'Z')) || ((ch >= 'a') && (ch <= 'z')))
    {
      /* Nothing to do, these are fine anywhere in the name */
    }
    else
    {
      ch = '-'; /* Replace all other characters with dashes */
      if (first_flag)
        *(optr++) = '_'; /* Prepend an underscore if start isn't valid */
    }

    *(optr++) = ch;

    first_flag = 0;
  }

  *optr = '\0';

  return((int)(optr - ncname_out));
}

/*
** util_scramble
**
** Use openssl to unscramble a value
**
**      scramble        String from environment
**      klen            Length of string from environment
**      instr           Original string
**      ilen            Length of original
**      outstr          Result string
**      direction       direction of conversion
*/
int util_scramble(char *scramble, int klen,
                  char *instr, int ilen, char *outstr,
                  int direction)
{
#ifdef USE_OPEN_SSL
  int       i;
  int       pval;
  int       nval;
  char     *tempstr;
  char      rcbuf[5];
  RC4_KEY   rcstate;
  un_long   crcval;

  crcval = util_checksum(scramble, klen);

  rcbuf[3] = (char)(crcval & 0xFF);
  crcval >>= 8;
  rcbuf[2] = (char)(crcval & 0xFF);
  crcval >>= 8;
  rcbuf[1] = (char)(crcval & 0xFF);
  crcval >>= 8;
  rcbuf[0] = (char)(crcval & 0xFF);
  rcbuf[4] = (char)(klen & 0xFF);

  tempstr = (char *)os_alloca(ilen + 1);

  pval = 0;

  if (direction != 0)
    mem_copy(tempstr, instr, ilen);
  else
    for (i = 0; i < ilen; ++i)
    {
      nval = ((int)instr[i] & 0xFF);
      pval ^= nval;
      tempstr[i] = (char)pval;
    }

  RC4_set_key(&rcstate, sizeof(rcbuf), (void *)rcbuf);
  RC4(&rcstate, ilen, (void *)tempstr, (void *)outstr);
  outstr[ilen] = '\0';

  if (direction != 0)
    for (i = 0; i < ilen; ++i)
    {
      nval = ((int)outstr[i] & 0xFF);
      pval ^= nval;
      outstr[i] = (char)pval;
      pval = nval;
    }

  return(1);
#else
#ifdef ENABLE_CRYPT
#ifdef MODOWA_WINDOWS
  HCRYPTPROV hCryptProv = {0};
  HCRYPTKEY  dummy = {0};
  HCRYPTKEY  rcstate = {0};
  LPCSTR     UserName = "mod_owa";
  DWORD      nbytes;
  BYTE       rc4blob[sizeof(template_blob)];
  int        i;
  int        pval;
  int        nval;
  char      *tempstr;
  char       rcbuf[5];
  un_long    crcval;

  crcval = util_checksum(scramble, klen);

  rcbuf[3] = (char)(crcval & 0xFF);
  crcval >>= 8;
  rcbuf[2] = (char)(crcval & 0xFF);
  crcval >>= 8;
  rcbuf[1] = (char)(crcval & 0xFF);
  crcval >>= 8;
  rcbuf[0] = (char)(crcval & 0xFF);
  rcbuf[4] = (char)(klen & 0xFF);

  if (!CryptAcquireContext(&hCryptProv, UserName, NULL,
                           PROV_RSA_FULL, 0))
    if (!CryptAcquireContext(&hCryptProv, UserName, NULL,
                             PROV_RSA_FULL, CRYPT_NEWKEYSET))
      if (!CryptAcquireContext(&hCryptProv, UserName, NULL,
                               PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
        return(0);

  if (!CryptImportKey(hCryptProv, dummy_blob, (DWORD)sizeof(dummy_blob),
		      0, 0, &dummy))
    goto crypterr;

  /* Insert RC4 key into template with null terminator */
  mem_copy(rc4blob, template_blob, sizeof(template_blob));
  for (i = 0; i < 16; ++i)
    rc4blob[12 + i] = rcbuf[i % sizeof(rcbuf)] | (1<<(i/2));
  i += 12;
  rc4blob[i++] = 0;

  if (!CryptImportKey(hCryptProv, rc4blob, (DWORD)sizeof(rc4blob),
                      dummy, 0, &rcstate))
    goto crypterr;

  tempstr = (char *)os_alloca(ilen + sizeof(rc4blob) + 1);

  pval = 0;

  if (direction != 0)
    mem_copy(tempstr, instr, ilen);
  else
    for (i = 0; i < ilen; ++i)
    {
      nval = ((int)instr[i] & 0xFF);
      pval ^= nval;
      tempstr[i] = (char)pval;
    }

  nbytes = (DWORD)ilen;

  if (direction == 0)
  {
    /* Conversion done in place */
    if (!CryptEncrypt(rcstate, 0, TRUE, 0, tempstr, &nbytes, nbytes))
      goto crypterr;
  }
  else
  {
    /* Conversion done in place */
    if (!CryptDecrypt(rcstate, 0, TRUE, 0, tempstr, &nbytes))
      goto crypterr;
  }

  if (nbytes != (DWORD)ilen) goto crypterr;
  mem_copy(outstr, tempstr, ilen);
  outstr[ilen] = '\0';

  if (direction != 0)
    for (i = 0; i < ilen; ++i)
    {
      nval = ((int)outstr[i] & 0xFF);
      pval ^= nval;
      outstr[i] = (char)pval;
      pval = nval;
    }

  return(1);

crypterr:
  if (dummy)   CryptDestroyKey(dummy);
  if (rcstate) CryptDestroyKey(rcstate);
  CryptReleaseContext(hCryptProv, 0);
#endif /* MODOWA_WINDOWS alternative */
#endif
  return(0);
#endif
}
