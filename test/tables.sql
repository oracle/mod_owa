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
REM This type is needed by Java interfaces
REM

create or replace type OWA_ARRAY as table of VARCHAR2(32767);
/

REM
REM Table to hold documents as LOBs
REM

create table DEMO_DOCUMENTS
             (
             ID               number                          not null,
             PARENT_ID        number         default 0        not null,
             NAME             varchar2(250)                   not null,
             LNAME            varchar2(250)                   not null,
             NODE_TYPE        char(1)        default 'F'      not null,
             USER_ID          number         default 0        not null,
             GROUP_ID         number         default 0        not null,
             LAST_MODIFIED    date           default SYSDATE  not null,
             LINK_ID          number                                  ,
             MIME_TYPE        varchar2(80)                            ,
             FILE_PATH        varchar2(2000)                          ,
             BIN_CONTENT      blob                                    ,
             CHAR_CONTENT     clob                                    ,
             constraint DEMO_DOCUMENTS_PK primary key (ID)            ,
             constraint DEMO_DOCUMENTS_C1 check (LNAME = lower(NAME)) ,
             constraint DEMO_DOCUMENTS_C2 check (NODE_TYPE in ('D','L','F'))
             )
             LOB (BIN_CONTENT) store as DEMO_BLOB,
             LOB (CHAR_CONTENT) store as DEMO_CLOB;

create unique index DEMO_DOCUMENTS_U1
    on DEMO_DOCUMENTS (LNAME, PARENT_ID);

create unique index DEMO_DOCUMENTS_U2
    on DEMO_DOCUMENTS (PARENT_ID, ID);

create unique index DEMO_DOCUMENTS_U3
    on DEMO_DOCUMENTS (ID, PARENT_ID);

create sequence DEMO_DOCUMENTS_S
          start with 100000 maxvalue 999999 increment by 1 nocycle;

REM
REM Tables used for LONG and LONG RAW testing
REM

create table DEMO_DOC_CHARS (ID       number  not null,
                             CONTENT  long            ,
                             constraint DEMO_DOC_CHARS_PK primary key (ID));

create table DEMO_DOC_RAWS  (ID       number  not null,
                             CONTENT  long raw        ,
                             constraint DEMO_DOC_RAWS_PK primary key (ID));

REM
REM Seed data for root directory and "/docs".
REM

insert into DEMO_DOCUMENTS (ID, PARENT_ID, NAME, LNAME, NODE_TYPE)
 values (0, -1, '/', '/', 'D');

insert into DEMO_DOCUMENTS (ID, PARENT_ID, NAME, LNAME, NODE_TYPE)
 select DEMO_DOCUMENTS_S.NEXTVAL, 0, 'docs', 'docs', 'D' from dual;

insert into DEMO_DOCUMENTS (ID, PARENT_ID, NAME, LNAME, NODE_TYPE)
 select DEMO_DOCUMENTS_S.NEXTVAL, 0, 'long', 'long', 'D' from dual;

insert into DEMO_DOCUMENTS (ID, PARENT_ID, NAME, LNAME, NODE_TYPE)
 select DEMO_DOCUMENTS_S.NEXTVAL, 0, 'file', 'file', 'D' from dual;

commit;

REM
REM Table of usernames/passwords for authorization checks
REM
create table DEMO_USERS
             (
             USERNAME         varchar2(250)                   not null,
             PASSWORD         varchar2(250)                   not null,
             constraint DEMO_USERS_PK primary key (USERNAME)
             );

insert into DEMO_USERS (USERNAME, PASSWORD) values ('scott', 'tiger');

commit;

REM
REM Table to demonstrate document upload/download using WebDB method
REM
create table DEMO_DOCLOAD
             (
             NAME         varchar2(256) not null,
             MIME_TYPE    varchar2(128)         ,
             DOC_SIZE     number                ,
             DAD_CHARSET  varchar2(128)         ,
             LAST_UPDATED date                  ,
             CONTENT_TYPE varchar2(128)         ,
             BLOB_CONTENT BLOB                  ,
             constraint DEMO_DOCLOAD_PK primary key (NAME)
             );
