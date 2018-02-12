The original [project repository](https://github.com/brianb/mdbtools) seems to have been abandoned 
(no activity from author for over a year).

In this fork I merged a few PR requests to the original repository as well as added some of my own
fixes.

# Installing from packages
Since I use these tools in many systems, I packaged it for simpler installation. 

> Had to change package name to avoid collision with original author's package.

## Ubuntu/Debian
> Built on Ubuntu 16.04 LTS only, should work on Debian and similar linux flavors.

```bash
$ sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 7C2FDAFB
$ echo "deb http://apt.cyberemissary.com/ubuntu/ xenial main" | sudo btee -a /etc/apt/sources.list.d/apt.cyberemissary.com.list
$ sudo apt-update
$ sudo apt-get install cyber-mdbtools
```

# Contributing
Feel free to submit PR requests here. I will try to review and merge them regularly.

# mdbtools 
_version 0.8.1_

Welcome to the exciting world of MDB Tools! In short, MDB Tools is a set of 
programs to help you use Microsoft Access file in various settings.  The major
pieces are:

```
. libmdb    - the core library that allows access to MDB files programatically.
. libmdbsql - builds on libmdb to provide a SQL engine (aka Jet)
. utils     - provides command line utilities:
              mdb-ver    -- prints the version (JET 3 or 4) of an mdb file.
              mdb-schema -- prints DDL for the specified table.
              mdb-export -- export table to CSV format.
              mdb-tables -- a simple dump of table names to be used with shell
                            scripts
              mdb-count  -- a simple count of number of rows in a table, to be
                            used in shell scripts and ETL pipelines
              mdb-header -- generates a C header to be used in exporting mdb
                            data to a C prog.
              mdb-parsecsv -- generates a C program given a CSV file made with
                            mdb-export
              mdb-sql    -- if --enable-sql is specified, a simple SQL engine
                            (also used by ODBC and gmdb).
            - And some utilities useful for debugging:
              prcat      -- prints the catalog table from an mdb file.
              prkkd      -- dump of information about design view data given the offset to it.
              prtable    -- dump of a table definition.
              prdata     -- dump of the data given a table name.
              prole      -- dump of ole columns given a table name and sargs.
. mdb-sql   - a command line SQL tool that allows one to type sql queries and
              get results.
. odbc      - An ODBC driver for use with unixODBC or iODBC driver manager.
              Allows one to use MDB files with PHP for example.
. gmdb2     - The Gnome MDB File Viewer and debugger.  Still alpha, but making
              great progress.
. extras    - extra command line utilities
              mdb-dump   -- simple hex dump utility that I've been using to look
                            at mdb files.
```

If you are interested in helping, read the HACKING file for a description of 
where the code stands and what has been gleened of the file format.

The initial goal of these tools is to be able to extract data structures and 
data from mdb files.  This goal will of course expand over time as the file 
format becomes more well understood.

Files in libmdb, libmdbsql, and libmdbodbc are licensed under LGPL and the
utilities and gui program are under the GPL, see COPYING.LIB and COPYING files
respectively.


Requirements:
=============

First, you must have reasonably current installations of:
	`libtool`
	`automake`
	`autoconf` (version >= 2.58)
If you don't you should install them first. Sources are available at
ftp.gnu.org.

Second, you need glib. It may come as `glib2.0` and `glib2.0-dev` packages in your
distribution.

If you want to build the SQL engine, you'll need `bison` or `byacc`, and `flex`.

If you want to build the ODBC driver, you'll need `unixodbc-dev` (version 2.2.10 or
above) or iodbc.

If you want to build man pages, you'll need `txt2man`. Source is available at
http://mvertes.free.fr/download/.

If you want to generate the html version of the docbook, you'll need openjade
and basic dsl catalogs.


Installation from source:
=========================

Last version is available at https://github.com/cyberemissary/mdbtools

```bash
$ autoreconf -i -f
```

If you want to build the html version of the docbook documentation, you need to
set the environment variable DOCBOOK_DSL to the modular dsl translation file.
For exemple, before configure, you need something like:

```bash
$ export DOCBOOK_DSL=/usr/share/sgml/docbook/stylesheet/dsssl/modular/html/docbook.dsl

$ ./configure
```

OR for a complete install (requires bison, flex, and unixODBC):

```bash
$ ./configure --with-unixodbc=/usr/local
```

configure can be passed any of the following flags to turn on other 
capabilities.  Note that the options `--with-unixodbc` and `--with-iodbc` are
mutually exclusive.

```
--with-unixodbc  specifies the location of the unixODBC driver manager and 
                 causes the unixODBC driver to be built.
--with-iodbc     specifies the location of the iODBC driver manager and 
                 causes the iODBC driver to be built.
```

A list of general options is available in the INSTALL file, and
`configure --help` will give you the list of mdbtools specific options.

```bash
$ make
```

Once MDB Tools has been compiled, libmdb.[so|a] will be in the src/libmdb 
directory and the utility programs will be in the src/util directory.

You can then install (to /usr/local by default) by running the following as root:

```bash
$ make install
```

Some systems will also need the ld cache to be updated after installation;
You can do that running:

```bash 
$ ldconfig
```


Contacts
========

Please send bug reports to the new github repository.
https://github.com/cyberemissary/mdbtools/issues

