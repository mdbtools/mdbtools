Summary: Several utilities for using MS-Access .mdb files. 
Name: mdbtools
Version: 0.3
Release: 1
Copyright: GPL
Group: Development/Tools
Source: http://download.sourceforge.net/mdbtools/mdbtools-0.3.tgz

%description
mdb-dump   -- simple hex dump utility for looking at mdb files
mdb-schema -- prints DDL for the specified table
mdb-export -- export table to CSV format
mdb-tables -- a simple dump of table names to be used with shell scripts
mdb-header -- generates a C header to be used in exporting mdb data to a C prog.
mdb-parsecvs -- generates a C program given a CSV file made with mdb-export
mdb-sql -- demo SQL engine program

%prep
%setup

%build
cd src/
./configure 
make

%install
cd src/
make install

%files
%doc AUTHORS COPYING COPYING.LIB HACKERS INSTALL README TODO NEWS
/usr/local/lib/libmdb.a
/usr/local/bin/mdb-schema
/usr/local/bin/mdb-export
/usr/local/bin/mdb-tables
/usr/local/bin/mdb-header
/usr/local/bin/mdb-parsecsv
/usr/local/bin/mdb-dump
/usr/local/bin/mdb-sql
/usr/local/include/mdbtools.h
/usr/local/include/mdbsql.h

