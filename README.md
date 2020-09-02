[![Build Status](https://travis-ci.org/evanmiller/mdbtools.svg?branch=master)](https://travis-ci.org/evanmiller/mdbtools)
[![Build status](https://ci.appveyor.com/api/projects/status/22wwy5d0rrmk6e3c/branch/master?svg=true)](https://ci.appveyor.com/project/evanmiller/mdbtools/branch/master)

Welcome to the exciting world of MDB Tools! MDB Tools is a set of programs to
help you extract data from Microsoft Access files in various settings.

After several years of neglect, MDB Tools is under new management as of 2020.
Update your bookmarks and tell your favorite search engine that this is the new
address:

    https://github.com/mdbtools/mdbtools

A brief history: the last *official* release (version 0.7.1) occurred in 2016.
[cyberemissary](https://github.com/cyberemissary) (whose work this fork is
based on) made a release in December 2018 and called it 0.8.2. In September
2020, @brianb moved his repository to the current address, and gave
administrative access to @evanmiller, who acts as the present maintainer.

An official 0.9 release is planned for late 2020. Areas of improvement since
the 0.7/0.8 days:

- [x] Security / stability / fuzz testing
- [x] Thread safety
- [x] In-memory database API
- [x] GLib is now optional
- [x] Improved ODBC compliance
- [x] Continuous integration with Travis and AppVeyor
- [x] New `mdb-queries` tool

The rest of this README explains what you can find in the project, how to
install it, and how to contribute.

# Components

The major pieces of MDB Tools are:

## libmdb

The core library that allows access to MDB files programatically.

## libmdbsql

Builds on libmdb to provide a SQL engine (aka Jet)

## utils

Provides command line utilities, including:

| Command | Description |
| ------- | ----------- |
| `mdb-ver` | Prints the version (JET 3 or 4) of an mdb file. |
| `mdb-schema` | Prints DDL for the specified table. |
| `mdb-export` | Export table to CSV format. |
| `mdb-tables` | A simple dump of table names to be used with shell scripts. |
| `mdb-count` | A simple count of number of rows in a table, to be used in shell scripts and ETL pipelines. |
| `mdb-header` | Generates a C header to be used in exporting mdb data to a C prog. |
| `mdb-parsecsv` | Generates a C program given a CSV file made with mdb-export. |
| `mdb-sql` | A simple SQL engine (also used by ODBC and gmdb). |
| `mdb-queries` | List and print queries stored in the database. |
| `prcat` | Prints the catalog table from an mdb file. |
| `prkkd` | Dump of information about design view data given the offset to it. |
| `prtable` | Dump of a table definition. |
| `prdata` | Dump of the data given a table name. |
| `prole` | Dump of ole columns given a table name and sargs. |
| `mdb-hexdump` | (in src/extras) Simple hex dump utility that I've been using to look at mdb files. |

## odbc

An ODBC driver for use with unixODBC or iODBC driver manager. Allows one to use MDB files with PHP for example.

## gmdb2

The Gnome MDB File Viewer and debugger. Still alpha and may be removed before 0.9.

# License

Files in libmdb, libmdbsql, and libmdbodbc are licensed under LGPL and the
utilities and gui program are under the GPL, see [COPYING.LIB](./COPYING.LIB)
and [COPYING](./COPYING) files respectively.


# Requirements

First, you must have reasonably current installations of:

* [libtool](https://www.gnu.org/software/libtool/)
* [automake](https://www.gnu.org/software/automake/)
* [autoconf](https://www.gnu.org/software/autoconf/) (version >= 2.58)

If you want to build the SQL engine, you'll need
[bison](https://www.gnu.org/software/bison/) (version >= 3.0) or
[byacc](https://invisible-island.net/byacc/byacc.html), and
[flex](https://github.com/westes/flex).

If you want to build the ODBC driver, you'll need `unixodbc-dev` (version
2.2.10 or above) or [iodbc](http://www.iodbc.org/dataspace/doc/iodbc/wiki/iodbcWiki/WelcomeVisitors).

If you want to build man pages, you'll need
[GNU awk](https://www.gnu.org/software/gawk/).

If you want to generate the html version of the docbook, you'll need
[openjade](http://openjade.sourceforge.net) and basic dsl catalogs.


# Installation

Latest version is available at https://github.com/mdbtools/mdbtools

## Debian

```
apt-get install mdbtools
```

## Homebrew

```bash
brew install mdbtools
```

## From source

```bash
$ autoreconf -i -f -Wno-portability
```

If you want to build the html version of the docbook documentation, you need to
set the environment variable `DOCBOOK_DSL` to the modular dsl translation file.
For example, before configure, you need something like:

```bash
$ export DOCBOOK_DSL=/usr/share/sgml/docbook/stylesheet/dsssl/modular/html/docbook.dsl

$ ./configure
```

OR for a complete install (requires bison, flex, and unixODBC):

```bash
$ ./configure --with-unixodbc=/usr/local
```

By default, MDB Tools is linked against the copy of
[GLib](https://developer.gnome.org/glib/) returned by pkg-config. You can
point to a different GLib installation using the `GLIB_CFLAGS` and `GLIB_LIBS`
enivornment variables. Or, you can disable GLib entirely with the
`--disable-glib` flag, in which case MDB Tools will use an internal
implementation of GLib's functions.

configure can be passed any of the following flags to turn on other 
capabilities.  Note that the options `--with-unixodbc` and `--with-iodbc` are
mutually exclusive.

```
--with-unixodbc  specifies the location of the unixODBC driver manager and 
                 causes the unixODBC driver to be built.
--with-iodbc     specifies the location of the iODBC driver manager and 
                 causes the iODBC driver to be built.
```

A list of general options is available in the [INSTALL](./INSTALL) file, and
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

# Hacking  

If you are interested in helping, read the [HACKING](./HACKING) file for a description of 
where the code stands and what has been gleened of the file format.

# Contact

Please send bug reports to the new github repository.
https://github.com/mdbtools/mdbtools/issues
