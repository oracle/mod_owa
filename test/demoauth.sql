Rem
Rem mod_owa
Rem
Rem Copyright (c) 1999-2019 Oracle Corporation, All rights reserved.
Rem
Rem The Universal Permissive License (UPL), Version 1.0
Rem
Rem Subject to the condition set forth below, permission is hereby granted
Rem to any person obtaining a copy of this software, associated documentation
Rem and/or data (collectively the "Software"), free of charge and under any
Rem and all copyright rights in the Software, and any and all patent rights
Rem owned or freely licensable by each licensor hereunder covering either
Rem (i) the unmodified Software as contributed to or provided by such licensor,
Rem or (ii) the Larger Works (as defined below), to deal in both
Rem 
Rem (a) the Software, and
Rem (b) any piece of software and/or hardware listed in the lrgrwrks.txt file
Rem if one is included with the Software (each a "Larger Work" to which the
Rem Software is contributed by such licensors),
Rem 
Rem without restriction, including without limitation the rights to copy,
Rem create derivative works of, display, perform, and distribute the Software
Rem and make, use, sell, offer for sale, import, export, have made, and have
Rem sold the Software and the Larger Work(s), and to sublicense the foregoing
Rem rights on either these or other terms.
Rem 
Rem This license is subject to the following condition:
Rem The above copyright notice and either this complete permission notice or
Rem at a minimum a reference to the UPL must be included in all copies or
Rem substantial portions of the Software.
Rem 
Rem THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
Rem IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
Rem FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
Rem THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
Rem LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
Rem FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
Rem DEALINGS IN THE SOFTWARE.
Rem

REM
REM A very simple example of Basic authentication.  This package is
REM designed to be run with per-package authorization, though the
REM same principles could be used if the AUTHORIZE function was moved
REM to OWA_CUSTOM.
REM
REM You can run the test by typing
REM
REM   http://yourmachine/yourowalocation/demo_auth_pkg.test_page
REM
REM and you will get challenged.  You can sign in as "scott",
REM password "tiger".
REM
REM The logic for Basic challenge is all in the AUTHORIZE function.
REM Calls to PL/SQL are always stateless, so this means that the
REM username/password need to be checked on every call.  If there
REM is no username, then that means the user has never logged in
REM before and the function will return FALSE after setting the
REM OWA.PROTECTION_REALM, and this will cause mod_owa to issue the
REM Basic challenge.  If there is a username, the password is
REM checked against the table DEMO_USERS.  If the user is found but
REM the password is blank or doesn't match, then again the code will
REM use OWA.PROTECTION_REALM to cause mod_owa to issue the challenge.
REM
REM If the username is not found, then the code uses the coded
REM technique for issuing the challenge (just to demonstrate it).
REM The coded technique prints out a header with the "WWW-Authenticate"
REM element.  This also gives the code the opportunity to set other
REM header elements, so in this example it sets or updates a cookie
REM to count the number of times the user has tried to log in.  If
REM this count exceeds 3, then instead of sending the challenge
REM the code generates an error page.  This isn't a very nice thing
REM to do, but it demonstrates that with the coded technique mod_owa
REM just uses GET_PAGE and doesn't generate a Basic challenge, so
REM that means you have to do it yourself or generate whatever page
REM you like.
REM
REM The TEST_PAGE procedure displays the values from both the
REM CGI variable HTTP_AUTHORIZATION and the values of the OWA
REM USER_ID and PASSWORD variables.  The CGI variable is of the
REM form "Basic <encoded username:password>", with the encoded
REM values in base-64 format.  mod_owa will decode these values
REM for you and put them in the OWA variables, where they can be
REM read either directly (e.g. OWA.USER_ID) or by calling OWA_SEC
REM routines.  The TEST_PAGE also dumps the contents of the
REM DEMO_USER table so you can see them, and gives you a form
REM where you can delete or add rows to the DEMO_USER table for
REM testing.  Try logging in as one of the users and then deleting
REM that user, you will see that on the redirect response your
REM credentials will no longer be considered valid and you will
REM get challenged again!  (Don't delete the last row in the table,
REM though, otherwise you will not be able to log back in unless
REM you add it back in sqlplus!)
REM

create or replace package DEMO_AUTH_PKG is
  --
  function AUTHORIZE return boolean;
  --
  procedure TEST_PAGE;
  --
  procedure CREATE_USER(USR in varchar2, PWD in varchar2);
  --
  procedure DELETE_USER(USR in varchar2);
  --
  procedure NEW_LOGON(USR in varchar2);
  --
end DEMO_AUTH_PKG;
/

create or replace package body DEMO_AUTH_PKG is
  --
  function AUTHORIZE return boolean is
    USR     varchar2(250);
    PWD     varchar2(250);
    ACOOKIE owa_cookie.cookie;
    CNT     number;
  begin
    if (OWA.USER_ID is null) then
      -- Case where no username is present (first time from this client?)
      -- Basic challenge is issued by mod_owa in OWS-compatible mode
      OWA_SEC.SET_PROTECTION_REALM('mod_owa');
      return(FALSE);
    else
      begin
        USR := OWA.USER_ID;
        select PASSWORD into PWD from DEMO_USERS where USERNAME = lower(USR);
        if ((OWA.PASSWORD is null) or (PWD <> lower(OWA.PASSWORD))) then
          -- Case where username is valid but password is missing/wrong
          -- Also uses OWS-compatible mode for the challenge
          OWA_SEC.SET_PROTECTION_REALM('mod_owa');
          return(FALSE);
        end if;
      exception when NO_DATA_FOUND then
        -- Case where username is invalid
        -- Shows the use of the hand-coded Basic challenge
        -- Uses cookie to count unsuccessful logins
        ACOOKIE := OWA_COOKIE.GET('modowa_auth_cookie');
        if (ACOOKIE.NUM_VALS > 0) then
          CNT := to_number(ACOOKIE.VALS(1)) + 1;
        else
          CNT := 1;
        end if;
        OWA_UTIL.MIME_HEADER('text/html', FALSE);
        OWA_COOKIE.SEND('modowa_auth_cookie', to_char(CNT));
        if (CNT < 3) then
          HTP.PRN('WWW-Authenticate: Basic realm="mod_owa"');
          OWA_UTIL.HTTP_HEADER_CLOSE;
        else
          OWA_UTIL.HTTP_HEADER_CLOSE;
          HTP.P('<html><body bgcolor="#ffffff" text="#c00000"><p>');
          HTP.P(to_char(CNT)||' login failures, access denied.');
          HTP.P('</p></body></html>');
        end if;
        return(FALSE);
      end;
    end if;
    return(TRUE);
  end AUTHORIZE;
  --
  function ENCODE_URI(URI in varchar2) return varchar2 is
    SCRIPT     varchar2(2000);
    FULLPATH   varchar2(2000);
    IDX        number;
  begin
    SCRIPT := OWA_UTIL.GET_CGI_ENV('SCRIPT_NAME');
    FULLPATH := OWA_UTIL.GET_CGI_ENV('REQUEST_URI');
    if (SCRIPT is null) then
      SCRIPT := '/';
    end if;
    IDX := instr(FULLPATH, SCRIPT);
    if (IDX > 0) then
      IDX := IDX + length(SCRIPT) - 1;
      SCRIPT := substr(FULLPATH,1,IDX);
    end if;
    if (substr(SCRIPT,length(SCRIPT),1) <> '/') then
      SCRIPT := SCRIPT||'/';
    end if;
    return(SCRIPT||URI);
  end;
  --
  procedure TEST_PAGE is
    AUTHSTRING varchar2(250);
    cursor C1 is select USERNAME, PASSWORD from DEMO_USERS order by USERNAME;
  begin
    AUTHSTRING := OWA_UTIL.GET_CGI_ENV('HTTP_AUTHORIZATION');
    OWA_UTIL.MIME_HEADER('text/html', FALSE);
    OWA_COOKIE.SEND('modowa_auth_cookie', '0');
    OWA_UTIL.HTTP_HEADER_CLOSE;
    HTP.P('<html><body bgcolor="#ddddee" text="#000000">');
    HTP.P('<p>');
    HTP.P('<strong>HTTP_AUTHORIZATION:</strong> '||AUTHSTRING||'<br>');
    HTP.P('<strong>OWA_SEC:</strong> '||
          OWA_SEC.GET_USER_ID()||'/'||OWA_SEC.GET_PASSWORD());
    HTP.P('</p>');
    HTP.P('<table cellspacing="0" cellpadding="0"');
    HTP.P(' border="0" bgcolor="#000000"><tr><td>');
    HTP.P('<table cellspacing="2" cellpadding="3" border="0">');
    HTP.P('<tr valign="top" bgcolor="#ffffc0"><td><b>Username</b></td>');
    HTP.P('<td><b>Password</b></td><td><b>Actions</b></td></tr>');
    for C1REC in C1 loop
      HTP.P('<tr valign="top" bgcolor="#ffffff"><td>'||
            C1REC.USERNAME||'</td>');
      HTP.P('<td>'||C1REC.PASSWORD||'</td>');
      HTP.P('<td><a href="'||ENCODE_URI('demo_auth_pkg.delete_user')||
            '?usr='||C1REC.USERNAME||'">delete</a></td></tr>');
    end loop;
    HTP.P('</table></td></tr></table>');
    HTP.P('<form action="'||
          ENCODE_URI('demo_auth_pkg.create_user')||'" method="POST">');
    HTP.P('<table cellspacing="3" cellpadding="3" border="0">');
    HTP.P('<tr valign="top"><td align="right">Username:</td><td></td>');
    HTP.P('<td><input type="text" size="30" name="USR"></td></tr>');
    HTP.P('<tr valign="top"><td align="right">Password:</td><td></td>');
    HTP.P('<td><input type="text" size="30" name="PWD"></td></tr>');
    HTP.P('<td colspan="3" align="right">');
    HTP.P('<input type="submit" value=" New User "></td></tr></table>');
    HTP.P('<p><a href="'||ENCODE_URI('demo_auth_pkg.new_logon')||
          '?usr='||OWA.USER_ID||'">Test sign on</a><br>');
    HTP.P('You must sign on as a different user, not '||OWA.USER_ID||'</p>');
    HTP.P('</form></body></html>');
  end TEST_PAGE;
  --
  procedure CREATE_USER(USR in varchar2, PWD in varchar2) is
  begin
    insert into DEMO_USERS (USERNAME, PASSWORD)
    values (lower(USR), lower(PWD));
    OWA_UTIL.REDIRECT_URL(ENCODE_URI('demo_auth_pkg.test_page'));
  exception when OTHERS then
    OWA_UTIL.REDIRECT_URL(ENCODE_URI('demo_auth_pkg.test_page'));
  end CREATE_USER;
  --
  procedure DELETE_USER(USR in varchar2) is
  begin
    delete from DEMO_USERS where USERNAME = lower(USR);
    OWA_UTIL.REDIRECT_URL(ENCODE_URI('demo_auth_pkg.test_page'));
  exception when OTHERS then
    OWA_UTIL.REDIRECT_URL(ENCODE_URI('demo_auth_pkg.test_page'));
  end DELETE_USER;
  --
  procedure NEW_LOGON(USR in varchar2) is
  begin
    if (OWA.USER_ID = USR) then
      OWA_UTIL.MIME_HEADER('text/html', FALSE);
      HTP.PRN('WWW-Authenticate: Basic realm="mod_owa"');
      OWA_UTIL.HTTP_HEADER_CLOSE;
    else
      OWA_UTIL.REDIRECT_URL(ENCODE_URI('demo_auth_pkg.test_page'));
    end if;
  end NEW_LOGON;
  --
end DEMO_AUTH_PKG;
/
