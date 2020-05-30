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
** File:         owanls.c
** Description:  Utility functions
** Author:       Doug McMahon      Doug.McMahon@oracle.com
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
**
** NLS character-set functions
**
** nls_cstype
**   Returns 0 for single-byte, 1-3 for supported multi-byte,
**   and -1 for unsupported or questionable multi-byte.
**
** nls_csx_from_oracle
**   Get index for character set given Oracle name, -1 if not found.
**
** nls_csx_from_iana(char *csname);
**   Get index for character set given IANA name, -1 if not found.
**
** nls_csid
**   Return Oracle character-set ID given index.
**
** nls_iana
**   Return IANA character set name given index.
**
** nls_check_utf8
**   Determine if a string is a valid utf-8 byte sequence.
**
** nls_conv_utf8(char *inbuf, char *outbuf)
**   Convert a string from iso-8859-1 to utf-8.
**
** nls_utf8_char
**   Extract UCS4 code point from UTF-8 string.
**
** nls_count_chars
**   Determine how many complete characters are in a byte-limited string.
**   Also returns number of bytes consumed by the count found.
**
** nls_find_delimiter
**   Find the last file delimiter in a string given the character set id.
**
** nls_length
**   Compute string length given a maximum number of bytes.
**
** ### This is a kludge.  Oracle should provide functions in the OCI to
** ### convert between character-set IDs and names, as well as the
** ### functions needed to count characters for multi-byte character sets.
**
** History
**
** 08/17/2000   D. McMahon      Created
** 05/31/2001   D. McMahon      Began revisioning with 1.3.9
** 08/20/2001   D. McMahon      Added AL32UTF8 for Oracle 9i
** 06/12/2002   D. McMahon      Add nls_check_utf8 and nls_conv_utf8
** 03/17/2003   D. McMahon      Fix a bug in nls_check_utf8
** 01/16/2006   D. McMahon      Moved nls_length from owahand.c
** 03/06/2013   D. McMahon      nls_cstype reports non-byte-unique sets
** 06/06/2013   D. McMahon      Add nls_utf8_char
** 09/11/2015   D. McMahon      Add nls_sanitize_header and nls_copy_identifier
** 09/09/2015   D. McMahon      GBK is reclassified as non-byte-unique
** 03/08/2018   D. McMahon      Added WE8ISO8859P15 (latin-9)
** 05/29/2020   D. McMahon      Treat unknown SJIS bytes as single-byte
*/

#include <modowa.h>

#define MB_CSID       800                 /* First multibyte character set */
#define ASCII_CSID    1                   /* US 7-bit ASCII                */
#define LATIN_CSID    31                  /* Oracle char set ID for Latin  */
#define UNI1_CSID     870                 /* Unicode 1.x character set ID  */
#define UNI2_CSID     871                 /* Unicode 2.x character set ID  */
#define UNI3_CSID     873                 /* Unicode 3.x character set ID  */
#define SJIS1_CSID    832                 /* SJIS character set ID         */
#define SJIS2_CSID    834                 /* SJIS+YEN character set ID     */
#define SJIS3_CSID    836                 /* SJIS Mac character set ID     */
#define SJIS4_CSID    838                 /* SJIS+TILDE character set ID   */
#define BIG5_CSID     865                 /* BIG5 character set ID         */
#define GBK_CSID      852                 /* GBK character set ID          */
#define GB2312_CSID   850                 /* GB2312 character set ID       */
#define ZHT32EUC_CSID 860                 /* Chinese 4-byte character set  */
#define JEUC1_CSID    830                 /* JEUC character set ID         */
#define JEUC2_CSID    831                 /* JEUC+YEN character set ID     */
#define JEUC3_CSID    837                 /* JEUC+TILDE character set ID   */
#define JA16VMS_CSID  829                 /* VMS 2-byte Japanese           */

typedef struct cspair
{
    const char *ora_cs;  /* Oracle character set name */
    const char *htp_cs;  /* IANA character set name */
    int   ocs_id;        /* Oracle character set ID */
} cspair;

/*
** Table relating Oracle character sets to IANA character sets
**
**   Oracle character set name (null for duplicates after the first)
**   IANA character set name
**   Oracle character set ID
*/
static char c_null[] = "";
static cspair cs_table[] =
{
{c_null,              c_null,                   0},
{"UTF8",              "utf-8",                871}, /* 3 */
{c_null,              "unicode-2-0-utf-8",    871}, /* duplicate */
{c_null,              "x-unicode-2-0-utf-8",  871}, /* duplicate */
/*
** ### Note that "AL32UTF8" uses a duplicate of the IANA name for "UTF8".
** ### So, the order in which they occur is significant.  By placing
** ### AL32UTF8 second in this list, situations where "utf-8" is
** ### specified as the DAD character set will cause the client to use
** ### UTF8 instead of the newer AL32UTF8.  This ensure compatibility
** ### with 8.x client-side stacks, but at the expense of not providing
** ### any means (other than NLS_LANG) of setting the newer 3.0-based
** ### Unicode as the client's DAD character set.
*/
{"AL32UTF8",          "utf-8",                873}, /* 4 */
{"US7ASCII",          "us-ascii",               1},
{c_null,              "iso-ir-6",               1}, /* duplicate */
{c_null,              "us",                     1}, /* duplicate */
{c_null,              "ansi_x3.4-1968",         1}, /* duplicate */
{c_null,              "ansi_x3.4-1986",         1}, /* duplicate */
{c_null,              "ascii",                  1}, /* duplicate */
{c_null,              "cp367",                  1}, /* duplicate */
{c_null,              "csascii",                1}, /* duplicate */
{c_null,              "ibm367",                 1}, /* duplicate */
{c_null,              "iso646-us",              1}, /* duplicate */
{c_null,              "iso_646.irv:1991",       1}, /* duplicate */
{"WE8ISO8859P1",      "iso-8859-1",            31},
{c_null,              "cp819",                 31}, /* duplicate */
{c_null,              "ibm819",                31}, /* duplicate */
{c_null,              "iso-ir-100",            31}, /* duplicate */
{c_null,              "iso8859-1",             31}, /* duplicate */
{c_null,              "iso_8859-1",            31}, /* duplicate */
{c_null,              "iso_8859-1:1987",       31}, /* duplicate */
{c_null,              "latin1",                31}, /* duplicate */
{c_null,              "l1",                    31}, /* duplicate */
{c_null,              "csisolatin1",           31}, /* duplicate */
{"EE8ISO8859P2",      "iso-8859-2",            32},
{c_null,              "l2",                    32}, /* duplicate */
{c_null,              "latin2",                32}, /* duplicate */
{c_null,              "csisolatin2",           32}, /* duplicate */
{c_null,              "iso-ir-101",            32}, /* duplicate */
{c_null,              "iso8859-2",             32}, /* duplicate */
{c_null,              "iso_8859-2",            32}, /* duplicate */
{c_null,              "iso_8859-2:1987",       32}, /* duplicate */
{"SE8ISO8859P3",      "iso-8859-3",            33},
{c_null,              "iso_8859-3",            33}, /* duplicate */
{c_null,              "iso_8859-3:1988",       33}, /* duplicate */
{c_null,              "iso-ir-109",            33}, /* duplicate */
{c_null,              "latin3",                33}, /* duplicate */
{c_null,              "l3",                    33}, /* duplicate */
{c_null,              "csisolatin3",           33}, /* duplicate */
{"NEE8ISO8859P4",     "iso-8859-4",            34},
{c_null,              "iso_8859-4:1988",       34}, /* duplicate */
{c_null,              "l4",                    34}, /* duplicate */
{c_null,              "latin4",                34}, /* duplicate */
{c_null,              "csisolatin4",           34}, /* duplicate */
{c_null,              "iso-ir-110",            34}, /* duplicate */
{c_null,              "iso_8859-4",            34}, /* duplicate */
{"CL8ISO8859P5",      "iso-8859-5",            35},
{c_null,              "csisolatincyrillic",    35}, /* duplicate */
{c_null,              "cyrillic",              35}, /* duplicate */
{c_null,              "iso-ir-144",            35}, /* duplicate */
{c_null,              "iso_8859-5",            35}, /* duplicate */
{c_null,              "iso_8859-5:1988",       35}, /* duplicate */
{"AR8ISO8859P6",      "iso-8859-6",            36},
{c_null,              "iso-ir-127",            36}, /* duplicate */
{c_null,              "iso_8859-6",            36}, /* duplicate */
{c_null,              "ecma-114",              36}, /* duplicate */
{c_null,              "asmo-708",              36}, /* duplicate */
{c_null,              "arabic",                36}, /* duplicate */
{c_null,              "csisolatinarabic",      36}, /* duplicate */
{c_null,              "iso_8859-6:1987",       36}, /* duplicate */
{"EL8ISO8859P7",      "iso-8859-7",            37},
{c_null,              "iso_8859-7:1987",       37}, /* duplicate */
{c_null,              "csisolatingreek",       37}, /* duplicate */
{c_null,              "ecma-118",              37}, /* duplicate */
{c_null,              "elot_928",              37}, /* duplicate */
{c_null,              "greek",                 37}, /* duplicate */
{c_null,              "greek8",                37}, /* duplicate */
{c_null,              "iso-ir-126",            37}, /* duplicate */
{"IW8ISO8859P8",      "iso-8859-8",            38},
{c_null,              "so_8859-8:1988",        38}, /* duplicate */
{c_null,              "csisolatinhebrew",      38}, /* duplicate */
{c_null,              "hebrew",                38}, /* duplicate */
{c_null,              "iso-ir-138",            38}, /* duplicate */
{c_null,              "iso_8859-8",            38}, /* duplicate */
{c_null,              "visual",                38}, /* duplicate */
{c_null,              "iso-8859-8 visual",     38}, /* duplicate */
{"WE8ISO8859P9",      "iso-8859-9",            39},
{c_null,              "latin5",                39}, /* duplicate */
{c_null,              "l5",                    39}, /* duplicate */
{c_null,              "csisolatin5",           39}, /* duplicate */
{c_null,              "iso_8859-9",            39}, /* duplicate */
{c_null,              "iso-ir-148",            39}, /* duplicate */
{c_null,              "iso_8859-9:1989",       39}, /* duplicate */
{"WE8ISO8859P15",     "iso-8859-15",           46},
{"NE8ISO8859P10",     "iso-8859-10",           40},
{"US8PC437",          "ibm437",                 4},
{c_null,              "cp437",                  4}, /* duplicate */
{c_null,              "437",                    4}, /* duplicate */
{c_null,              "cspc8codepage437",       4}, /* duplicate */
{"WE8PC850",          "ibm850",                10},
{c_null,              "cp850",                 10}, /* duplicate */
{c_null,              "850",                   10}, /* duplicate */
{c_null,              "cspc850multilingual",   10}, /* duplicate */
{c_null,              "ibm860",               160},
{c_null,              "cp860",                160}, /* duplicate */
{c_null,              "860",                  160}, /* duplicate */
{c_null,              "csibm860",             160}, /* duplicate */
{"WE8MSWIN1252",      "Windows-1252",         178},
{c_null,              "x-ansi",               178}, /* duplicate */
{"AR8MSAWIN",         "Windows-1256",         560},
{"BLT8MSWIN1257",     "Windows-1257",         179},
{"CL8MSWIN1251",      "Windows-1251",         171},
{c_null,              "x-cp1251",             171}, /* duplicate */
{"EE8MSWIN1250",      "Windows-1250",         170},
{c_null,              "x-cp1250",             170}, /* duplicate */
{"EL8MSWIN1253",      "Windows-1253",         174},
{"IW8MSWIN1255",      "Windows-1255",         175},
{"LT8MSWIN921",       "Windows-921",          176},
{"TR8MSWIN1254",      "Windows-1254",         177},
{"VN8MSWIN1258",      "Windows-1258",          45},
{"AL24UTFFSS",        "unicode-1-1-utf-8",    870}, /* 3 */
{"TH8TISASCII",       "tis-620",               41},
{c_null,              "Windows-874",           41}, /* duplicate */
{"BLT8PC775",         "ibm775",               197},
{c_null,              "cp775",                197}, /* duplicate */
{c_null,              "cspc775baltic",        197}, /* duplicate */
{"CDN8PC863",         "ibm863",               390},
{c_null,              "cp863",                390}, /* duplicate */
{c_null,              "863",                  390}, /* duplicate */
{c_null,              "csibm863",             390}, /* duplicate */
{"CL8KOI8R",          "koi8-r",               196},
{c_null,              "cskoi8r",              196}, /* duplicate */
{c_null,              "koi",                  196}, /* duplicate */
{"EE8PC852",          "ibm852",               150},
{c_null,              "cp852",                150}, /* duplicate */
{c_null,              "852",                  150}, /* duplicate */
{c_null,              "cspcp852",             150}, /* duplicate */
{"EL8PC737",          "ibm737",               382},
{"EL8PC869",          "ibm869",               385},
{c_null,              "cp869",                385}, /* duplicate */
{c_null,              "csibm869",             385}, /* duplicate */
{c_null,              "869",                  385}, /* duplicate */
{c_null,              "cp-gr",                385}, /* duplicate */
{"N8PC865",           "ibm865",               190},
{c_null,              "cp865",                190}, /* duplicate */
{c_null,              "865",                  190}, /* duplicate */
{c_null,              "csibm865",             190}, /* duplicate */
{c_null,              "cp865",                190}, /* duplicate */
{"RU8PC855",          "ibm855",               155},
{c_null,              "cp855",                155}, /* duplicate */
{c_null,              "855",                  155}, /* duplicate */
{c_null,              "csibm855",             155}, /* duplicate */
{"RU8PC866",          "ibm866",               152},
{c_null,              "cp866",                152}, /* duplicate */
{c_null,              "866",                  152}, /* duplicate */
{c_null,              "csibm866",             152}, /* duplicate */
{"TR8PC857",          "ibm857",               156},
{c_null,              "cp857",                156}, /* duplicate */
{c_null,              "csibm857",             156}, /* duplicate */
{c_null,              "857",                  156}, /* duplicate */
{"IS8PC861",          "ibm861",               161},
{c_null,              "cp861",                161}, /* duplicate */
{c_null,              "cp-is",                161}, /* duplicate */
{c_null,              "861",                  161}, /* duplicate */
{c_null,              "csibm861",             161}, /* duplicate */
{"AR8ARABICMAC",      "MacArabic",            565},
{"AR8ARABICMACS",     "MacArabic",            566},
{"CL8MACCYRILLIC",    "MacCyrillic",          158},
{"CL8MACCYRILLICS",   "MacCyrillic",          159},
{"EE8MACCE",          "MacCentralEurope",     262},
{"EE8MACCES",         "MacCentralEurope",     162},
{"EE8MACCROATIAN",    "MacCroatian",          263},
{"EE8MACCROATIANS",   "MacCroatian",          163},
{"EL8MACGREEK",       "MacGreek",             266},
{"EL8MACGREEKS",      "MacGreek",             166},
{"IS8MACICELANDIC",   "MacIceland",           265},
{"IS8MACICELANDICS",  "MacIceland",           165},
{"IW8MACHEBREW",      "MacHebrew",            267},
{"IW8MACHEBREWS",     "MacHebrew",            167},
{"TH8MACTHAI",        "MacThai",              353},
{"TH8MACTHAIS",       "MacThai",              354},
{"TR8MACTURKISH",     "MacTurkish",           264},
{"TR8MACTURKISHS",    "MacTurkish",           164},
{"WE8MACROMAN8",      "MacRoman",             351},
{"WE8MACROMAN8S",     "MacRoman",             352},
{"JA16EUC",           "euc-jp",               830}, /* 3 */
{c_null,              "cseucpkdfmtjapanese",  830}, /* duplicate */
{c_null,              "x-euc",                830}, /* duplicate */
{c_null,              "x-euc-jp",             830}, /* duplicate */
{"JA16EUCYEN",        "EUCJIS",               831}, /* 3 */
{"JA16EUCTILDE",      "EUCJIS",               837}, /* 3 */
{"JA16SJIS",          "shift-jis",            832}, /* 2 not byte-unique */
{c_null,              "csshiftjis",           832}, /* duplicate */
{c_null,              "cswindows31j",         832}, /* duplicate */
{c_null,              "ms_kanji",             832}, /* duplicate */
{c_null,              "shift_jis",            832}, /* duplicate */
{c_null,              "x-ms-cp932",           832}, /* duplicate */
{c_null,              "x-sjis",               832}, /* duplicate */
{"JA16SJISYEN",       "shift_jis",            834}, /* 2 not byte-unique */
{"JA16SJISTILDE",     "shift_jis",            838}, /* 2 not byte-unique */
{"KO16KSC5601",       "ks_c_5601",            840}, /* 2 */
{c_null,              "korean",               840}, /* duplicate */
{c_null,              "ks_c_5601-1987",       840}, /* duplicate */
{c_null,              "euc-kr",               840}, /* duplicate */
{c_null,              "cseuckr",              840}, /* duplicate */
{c_null,              "csksc56011987",        840}, /* duplicate */
{"ZHS16CGB231280",    "gb2312",               850}, /* 2 */
{c_null,              "iso-ir-58",            850}, /* duplicate */
{c_null,              "chinese",              850}, /* duplicate */
{c_null,              "csgb2312",             850}, /* duplicate */
{c_null,              "csiso58gb231280",      850}, /* duplicate */
{c_null,              "gb_2312-80",           850}, /* duplicate */
{"ZHS16GBK",          "gbk",                  852}, /* 2 not byte-unique */
{"ZHT16BIG5",         "big5",                 865}, /* 2 not byte-unique */
{c_null,              "x-x-big5",             865}, /* duplicate */
{c_null,              "csbig5",               865}, /* duplicate */
{"JA16MACSJIS",       "shift_jis",            836}, /* 2 not byte-unique */
{"JA16VMS",           "JIS0208",              829}, /* 2 not byte-unique */
{"ZHT32EUC",          "euc-tw",               860}, /* 4 */
};

/* Table of flags indicating bytes valid in procedure names */
static unsigned char valid_bytes[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                                      0,0,0,1,1,0,0,0,0,0,0,0,0,0,1,0,
                                      1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
                                      0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
                                      1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,
                                      0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
                                      1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
                                      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
                                      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
                                      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
                                      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
                                      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
                                      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
                                      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
                                      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

/*
** Return basic type of character set:
**  0 for single-byte
**  1 for unicode
**  2 for supported multi-byte
**  3 for supported multi-byte non-byte-unique
** -1 for unsupported multi-byte
*/
int nls_cstype(int cs_id)
{
    if (cs_id < MB_CSID) return(0); /* Single-byte */
    switch (cs_id)
    {
    case UNI1_CSID:
    case UNI2_CSID:
    case UNI3_CSID:
        return(1); /* Unicode */
        break;
    case GB2312_CSID:
    case ZHT32EUC_CSID:
    case JEUC1_CSID:
    case JEUC2_CSID:
    case JEUC3_CSID:
        return(2); /* Multi-byte set supported by nls_count_chars */
        break;
    case SJIS1_CSID:
    case SJIS2_CSID:
    case SJIS3_CSID:
    case SJIS4_CSID:
    case BIG5_CSID:
    case GBK_CSID:
        return(3); /* Not byte-unique multi-byte set supported */
        break;
    default:
        break;
    }
    return(-1); /* Unknown multi-byte */
}

/*
** Given an Oracle character-set name, return the CS table index
*/
int nls_csx_from_oracle(char *csname)
{
    int     i;

    for (i = 1; i < (int)(sizeof(cs_table)/sizeof(*cs_table)); ++i)
        if (!str_compare(csname, cs_table[i].ora_cs, -1, 1))
            return(i);

    return(-1);
}

/*
** Given an IANA character-set name, return the CS table index
*/
int nls_csx_from_iana(char *csname)
{
    int     i;

    for (i = 1; i < (int)(sizeof(cs_table)/sizeof(*cs_table)); ++i)
        if (!str_compare(csname, cs_table[i].htp_cs, -1, 1))
            return(i);

    return(-1);
}

/*
** Given character-set index, get Oracle character set ID
*/
int nls_csid(int idx)
{
    return(cs_table[idx].ocs_id);
}

/*
** Given character-set index, get IANA character set name
*/
char *nls_iana(int idx)
{
    return((char *)cs_table[idx].htp_cs);
}

/*
** Check a string to see if it's valid in utf-8
*/
int nls_check_utf8(char *buf, int nbytes)
{
    int i, j, k, n;
    n = (nbytes < 0) ? str_length(buf) : nbytes;
    i = 0;
    while (i < n)
    {
        k = buf[i++] & 0xFF;
        if (k < 0x80)       continue;
        else if (k < 0xC0)  return(0); /* Naked trailing byte */
        else if (k < 0xE0)  j = 1;     /* 1 follower */
        else if (k < 0xF0)  j = 2;     /* 2 followers */
        else if (k < 0xF8)  j = 3;     /* 3 followers */
        else if (k < 0xFC)  j = 4;     /* 4 followers */
        else if (k < 0xFE)  j = 5;     /* 5 followers */
        else                return(0); /* Unknown leading byte */

        j += i;
        if (j > n) return(0); /* Premature end of string */

        while (i < j)
        {
            k = buf[i++] & 0xFF;
            if ((k < 0x80) || (k >= 0xC0)) return(0); /* Invalid follower */
        }
    }
    return(1); /* String is valid */
}

/*
** Convert a string from iso-8859-1 to utf-8; returns the length of the
** new string.  If outbuf is null, it still returns the buffer size
** required for conversion.
*/
int nls_conv_utf8(char *inbuf, char *outbuf)
{
    int   count;
    int   k;
    char *iptr, *optr;

    iptr = inbuf;
    optr = outbuf;

    for (count = 0; *iptr != '\0'; ++iptr)
    {
        k = (*iptr) & 0xFF;
        if (optr)
        {
            if (k < 0x80)
                *(optr++) = k;
            else
            {
                *(optr++) = 0xC0 | (k >> 6);
                *(optr++) = 0x80 | (k & 0x3F);
            }
        }
        count += ((k >> 7) + 1);
    }

    if (optr) *optr = '\0';
    return(count);
}

/*
** Extract a UCS4 code point from a UTF-8 string. Returns -1 if the data
** isn't valid UTF-8. Optionally returns the number of bytes read from the
** input string.
*/
int nls_utf8_char(char *buf_in, int *nbytes)
{
    int n;
    int ch;
    int unichr;

    ch = (*(buf_in++) & 0xFF);

    if (ch < 0x80)
    {
      if (nbytes) *nbytes = 1;
      return(ch);
    }
    else if (ch < 0xC0)
    {
        return(-1);  /* Naked trailing byte - error */
    }
    else if (ch < 0xE0)
    {
        n = 1;  /* 1 follower */
        unichr = ch & 0x1F;
    }
    else if (ch < 0xF0)
    {
        n = 2;  /* 2 followers */
        unichr = ch & 0x0F;
    }
    else if (ch < 0xF8)
    {
        n = 3;  /* 3 followers */
        unichr = ch & 0x07;
    }
    else if (ch < 0xFC)
    {
        n = 4;  /* 4 followers */
        unichr = ch & 0x03;
    }
    else if (ch < 0xFE)
    {
        n = 5;  /* 5 followers */
        unichr = ch & 0x01;
    }
    else
    {
        return(-1);  /* Unknown leading byte - error */
    }

    if (nbytes) *nbytes = n + 1;

    while (n > 0)
    {
        ch = (*(buf_in++) & 0xFF);
        if ((ch < 0x80) || (ch >= 0xC0)) return(-1); /* Invalid follower */
        unichr = (unichr << 6) | (ch & 0x3F);
        --n;
    }

    return(unichr);
}


/*
** Count the number of characters in a multibyte string given byte count
*/
int nls_count_chars(int cs_id, char *outbuf, un_long *nbytes)
{
    int   amount;
    int   i, j, k, n;

    if (cs_id < MB_CSID) return((int)*nbytes);

    amount = 0;
    i = (int)*nbytes;
    j = 0;

    if ((cs_id == UNI1_CSID) || (cs_id == UNI2_CSID) || (cs_id == UNI3_CSID))
    {
        /*
        ** Algorithm for all variable-width Unicode variants
        */
        while (j < i)
        {
            k = outbuf[j] & 0xFF;
            if (k < 0x80)
            {
                ++j;
                ++amount;
            }
            else if (k < 0xC0) /* Error: naked trailing byte */
            {
                ++j;
                ++amount;
            }
            else
            {
                if (k < 0xE0)      n = 2;
                else if (k < 0xF0) n = 3;
                else if (k < 0xF8) n = 4;
                else if (k < 0xFC) n = 5;
                else if (k < 0xFE) n = 6;
                else               n = 1; /* Error: unknown byte */

                if ((i - j) >= n)
                {
                    j += n;
                    ++amount;
                }
                else
                    i = j; /* Trailing fragment not consumed */
            }
        }
    }
    else
    {
        /*
        ** The default implementation assumes single-byte below 0x80,
        ** all others are leaders of double-byte sequences.  There are
        ** special cases coded for:
        **  1) the 2/4-byte Chinese sets, which use 0x8e as a 4-byte leader
        **  2) the 2/3-byte JEUC sets, which use 0x8f as a 3-byte leader
        **  3) the 2-byte Asian sets, which don't use 0x80 or 0xff
        **  4) the Shift-JIS sets, which have some 1-byte values above 0x7f
        */
        while (j < i)
        {
            k = outbuf[j] & 0xFF;
            if (k < 0x80)
            {
                ++j;
                ++amount;
            }
            else
            {
                /*
                ** Seems to apply to:  JA16VMS, KO16KSC5601
                **
                ** ### Old experiments show it also appears to work for:
                **     GB2312, JA16EUC
                */
                n = 2;
                /*
                ** This algorithm is used for the 4-byte sets.  Again there
                ** is a discrepancy between the documentation and experiment.
                ** Documentation:
                **
                **   Range      ZHT32EUC   ZHT32TRIS  ZHT32SOPS  Implementation
                **   ----------------------------------------------------------
                **   0x80-0x8d  undefined  undefined  2?         2-byte
                **   0x8e       4-byte     4-byte     4-byte     4-byte
                **   0x8f-0xa0  undefined  undefined  2?         2-byte
                **   0xa1-0xfd  2-byte     undefined  2?         2-byte
                **   0xfe-0xff  undefined  undefined  2?         2-byte
                **
                ** The implementation maps all undefined values to 2.
                ** ### For ZHT32TRIS, undefined bytes should be 1.
                */
                if ((k == 0x8E) && (cs_id == ZHT32EUC_CSID))
                    n = 4;
                /*
                ** Once again, there are some discrepancies between experiment
                ** and documentation:
                **
                **   Range      Experiment   Documentation   Implementation
                **   ------------------------------------------------------
                **   0x80-0x8e  2-byte lead  undefined       2-byte
                **   0x8f       3-byte lead  3-byte lead     3-byte
                **   0xa0       2-byte lead  undefined       2-byte
                **   0xa1-0xfe  2-byte lead  2-byte lead     2-byte
                **   0xff       2-byte lead  undefined       2-byte
                */
                else if ((k == 0x8F) &&
                         ((cs_id == JEUC1_CSID) || (cs_id == JEUC2_CSID) ||
                          (cs_id == JEUC3_CSID)))
                    n = 3;
                /*
                ** For GB2312, this is an approximation
                ** (because 80-a0 and f8-ff are undefined).
                **
                ** An identical algorithm also works for:
                **   ZHT16MSWIN950, KO16MSWIN949, ZHT16HKCSC
                **
                ** Plus, the algorithm can be used for:
                **   KO16KSCCS (although 80-83 and fa-ff are undefined)
                **
                ** ZHS32GB18030 uses a second-byte signalling mechanism
                ** and can therefore be treated as if it were a 2-byte set.
                */
                else if (((k == 0x80) || (k == 0xFF)) &&
                         ((cs_id == BIG5_CSID) ||
                          (cs_id == GBK_CSID)  ||
                          (cs_id == GB2312_CSID)))
                    n = 1;
                /*
                ** All forms of Shift-JIS.  Unfortunately there appear to be
                ** some discrepancies between documentation and experiment:
                **
                **   Range      Experiment   Documentation   Implementation
                **   ------------------------------------------------------
                **   0x80       single-byte  undefined       1-byte
                **   0x81-0x9f  2-byte lead  2-byte lead     2-byte
                **   0xa0       single-byte  undefined       1-byte
                **   0xa1-0xdf  single-byte  single-byte     1-byte
                **   0xe0-0xfc  2-byte lead  2-byte lead     2-byte
                **   0xfd-0xff  single-byte  undefined       1-byte
                **
                ** ### May be incorrect for MAC Shift-JIS
                */
                else if (((k == 0x80) || (k >= 0xFD) ||
                          ((k >= 0xA0) && (k < 0xE0))) &&
                         ((cs_id == SJIS1_CSID) || (cs_id == SJIS2_CSID) ||
                          (cs_id == SJIS3_CSID) || (cs_id == SJIS4_CSID)))
                    n = 1;

                if ((i - j) >= n)
                {
                    j += n;
                    ++amount;
                }
                else
                    i = j; /* Trailing fragment not consumed */
            }
        }
    }

    *nbytes = i;
    return(amount);
}

/*
** Find the delimiter for the file name
*/
char *nls_find_delimiter(char *filepath, int cs_id)
{
  char *fname = filepath;

  /* Character set is presumed byte-unique, including UTF-8 */
  if (nls_cstype(cs_id) < 3)
  {
    /* Safe to look for the Windows-style backslash */
    fname = str_char(filepath, '\\', 1);
    /* If delimiter not found look for the last forward slash in the string */
    if (!fname) fname = str_char(filepath, '/', 1);
    /* Increments past separator if found, else use whole filename */
    if (fname)
      ++fname;
    else
      fname = filepath;
  }
  /* Else unfortunately backslash can be a trailing byte */
  else
  {
    char *sptr = fname;
    while (*sptr)
    {
      int ch = (int)(*(sptr++) & 0xFF);
      if ((ch == '\\') || (ch == '/'))
        fname = sptr;
      else if ((ch > 0x80) && (*sptr)) /* If a possible multi-byte char */
      {
        if ((cs_id == BIG5_CSID) || (cs_id == GBK_CSID))
        {
          if (ch != 0xFF) /* Leader of a 2-byte sequence */
            ++sptr;
        }
        else /* Otherwise a Shift-JIS variant */
        {
          if ((ch < 0xA0) || (ch >= 0xE0)) /* Leader of a 2-byte sequence */
            ++sptr;
        }
      }
    }
  }

  return(fname);
}

/*
** Compute string length to trailing null or specified maximum byte count,
** respecting NLS character boundaries
*/
int nls_length(int cs_id, char *s, int maxbytes)
{
    char    *sptr = s;
    un_long  nbytes = 0;

    while ((*sptr != '\0') && (nbytes < (un_long)maxbytes))
    {
        ++sptr;
        ++nbytes;
    }

    if (*sptr != '\0')
        if (nls_count_chars(cs_id, s, &nbytes) == 0)
            nbytes = maxbytes;

    return((int)nbytes);
}

/*
** Check path for valid procedure name
*/
void nls_validate_path(char *spath, int cs_id)
{
    char   *sptr;

    /*
    ** If the CS is US7ASCII, bit-strip it
    **
    ** ### Unknown character sets should probably be bit-stripped for
    ** ### safety reasons, but that seems pretty unfriendly.  For now,
    ** ### treat this as an 8-bit set since that's the most common
    ** ### deployment anyway.
    */
    if (cs_id == ASCII_CSID)
      for (sptr = spath; *sptr; ++sptr)
        *sptr = *sptr & 0x7F;

#ifdef NEVER /* ### NOT NEEDED - ASCII-BASED BYTE-UNIQUENESS ASSUMED? ### */
    /*
    ** If the CS is multi-byte, check to see if it's known to be
    ** non-byte-unique, and run a character-by-character scan if so.
    */ 
    if (nls_cstype(cs_id) > 0)
    {
        int     ch;
        int     n;
        un_long nbytes;

        switch (cs_id)
        {
        case SJIS1_CSID:
        case SJIS2_CSID:
        case SJIS3_CSID:
        case SJIS4_CSID:
        case GBK_CSID:
        case BIG5_CSID:
        case JA16VMS_CSID:
            sptr = spath;
            while (*sptr)
            {
                ch = *sptr & 0xFF;
                if (ch < 0x80)
                {
                    /* Check ASCII character for validity */
                    if (!valid_bytes[ch]) break;
                    ++sptr;
                }
                else
                {
                    /* Skip over multibyte character */
                    for (n = 0; sptr[n] != '\0'; ++n)
                    {
                        nbytes = (un_long)(n + 1);
                        if (nls_count_chars(cs_id, sptr, &nbytes) > 0)
                            break;
                    }
                    sptr += nbytes;
                }
            }
            *sptr = '\0';
            return;
        default:
            /*
            ** For unknown multi-byte sets, or byte-unique sets,
            ** drop through to the ASCII-based scanning logic used
            ** for single-byte
            */
            break;
        }
    }
#endif

    /*
    ** Cut off the path at the first ASCII punctuation or space byte.
    */
    for (sptr = spath; valid_bytes[(*sptr) & 0xFF]; ++sptr);
    *sptr = '\0';
}

/*
** Ensure header string doesn't contain control characters,
** especially carriage returns and newlines.
*/
void nls_sanitize_header(char *sptr)
{
    while (*sptr)
    {
        if (*sptr < ' ') *sptr = ' ';
        ++sptr;
    }
}

/*
** Copy an identifier and sanitize it
*/
int nls_copy_identifier(char *dest, char *src, int cs_id)
{
    int ch;
    int len = 0;
    int needs_quotes = 0;

    for (len = 0; src[len]; ++len)
    {
        ch = src[len] & 0xFF;

        if ((ch >= 0x7F) || (!valid_bytes[ch]))
        {
            needs_quotes = 1;
            break; /* Stop at the first disallowed byte */
        }

        dest[len] = ch;
    }

    if (needs_quotes)
    {
        /*
        ** If using a non-byte-unique or unsupported multibyte,
        ** this code is obliged to strip non-ASCII characters.
        */
        int cs_type  = nls_cstype(cs_id);
        int cs_strip = ((cs_type < 0) || (cs_type > 2));

        len = 0;

        dest[len++] = '"';

        while (*src)
        {
            ch = *(src++) & 0xFF;

            /* ### May miss a Unicode quote that gets downconverted later ### */
            if (ch == '"') ch = ' '; /* double quote is disallowed */
            if (ch < ' ')  ch = ' '; /* control characters disallowed */
            if ((cs_strip) && (ch >= 0x7F)) ch = ' ';

            /* Convert to uppercase */
            if ((ch >= 'a') && (ch <= 'z')) ch -= ('a' - 'A');
            /* ### Misses Unicode uppercasing ### */

            dest[len++] = ch;
        }

        dest[len++] = '"';
    }

    dest[len] = '\0';

    return(len);
}
