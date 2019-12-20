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

#include <stdio.h>
#include <modowa.h>

int main(argc, argv)
int   argc;
char *argv[];
{
  char     *evname = "MODOWA_SCRAMBLE";
  char     *scramble = os_env_get(evname);
  char     *connstr;
  char     *tempstr;
  char     *hexstr;
  int       slen;
  int       xlen;
  un_long   sec;
  un_long   musec;

  if (!scramble)
  {
    printf("%s can't read environment variable %s\n", argv[0], evname);
    return(0);
  }
  xlen = str_length(scramble);

  if (xlen == 0)
  {
    printf("%s required %s to be set\n", argv[0], evname);
    return(0);
  }

  if (argc < 2)
  {
    printf("Usage: %s <dbconnectstr>\n", argv[0]);
    return(0);
  }

  sec = os_get_time(&musec);
  sec ^= (musec << 24);
  sec ^= ((musec << 16) & 0xFF0000);
  sec ^= ((musec << 8) & 0xFF00);
  sec ^= (musec & 0xFF);

  slen = str_length(argv[1]);

  connstr = (char *)os_alloca(slen + 4 + 1);
  tempstr = (char *)os_alloca(slen + 4 + 1);
  hexstr = (char *)os_alloca((slen + 4) * 2 + 1);

  connstr[3] = (char)(sec & 0xFF);
  sec >>= 8;
  connstr[2] = (char)(sec & 0xFF);
  sec >>= 8;
  connstr[1] = (char)(sec & 0xFF);
  sec >>= 8;
  connstr[0] = (char)(sec & 0xFF);

  mem_copy(connstr + 4, argv[1], slen + 1);

  slen += 4;

  util_scramble(scramble, xlen, connstr, slen, tempstr, 0);
  str_btox((void *)tempstr, hexstr, slen);
  hexstr[slen * 2] = '\0';

  mem_zero(tempstr, slen);
  slen = str_xtob(hexstr, (void *)tempstr);
  mem_zero(connstr, slen);

  util_scramble(scramble, xlen, tempstr, slen, connstr, 1);
  connstr[slen] = '\0';

  printf("Original  = [%s]\n", connstr + 4);
  printf("Scrambled = [%s]\n", hexstr);

  return(0);
}
