This file documents the Microsoft MDB file format for Jet3 and Jet4 databases.

[TOC]

General Notes
-------------

Access (Jet) does not in general initialize pages to zero before writing them,
so the file will contains a lot of unititialized data.  This makes the task of
figuring out the format a bit more difficult than it otherwise would be.

This document will, generally speaking, provide all offsets and constants in 
hex format.  

Most multibyte pointer and integers are stored in little endian (LSB-MSB) order.
There is an exception in the case of indexes, see the section on index pages for
details.

Terminology
-----------

This section contains a mix of information about data structures used in the MDB
file format along with general database terminology needed to explain these 
structures.
```
Page          - A fixed size region within the file on a 2 or 4K boundry. All 
                data in the file exists inside pages.

System Table  - Tables in Access generally starting with "MSys".  The 'Flags'
                field in the table's Catalog Entry will contain a flag in one
                of two positions (0x80000000 or 0x00000002).  See also the TDEF
		        (table definition) pages for "System Table" field.
		        
Catalog Entry - A row from the MSysObjects table describing another database
                object.  The MSysObjects table definition page is always at 
                page 2 of the database, and a phony tdef structure is
                bootstrapped to initially read the database.
                
Page Split    - A process in which a row is added to a page with no space left.
                A second page is allocated and rows on the original page are 
		        split between the two pages and then indexes are updated. Pages
                can use a variety of algorithms for splitting the rows, the 
                most popular being a 50/50 split in which rows are divided 
                evenly between pages.
                
Overflow Page - Instead of doing a full page split with associated index writes,
                a pointer to an "overflow" page can be stored at the original
                row's location. Compacting a database would normally rewrite
                overflow pages back into regular pages.
                
Leaf Page     - The lowest page on an index tree.  In Access, leaf pages are of
                a different type than other index pages.
                
UCS-2         - a two byte unicode encoding used in Jet4 files.

Covered Query - a query that can be satisfied by reading only index pages.  For
                instance if the query 
		        "SELECT count(*) from Table1 where Column3 = 4" were run and 
                Column3 was indexed, the query could be satisfied by reading
                only indexes.  Because of the way Access hashes text columns
                in indexes, covered queries on text columns are not possible.
```

Pages
-----

At its topmost level, a MDB file is organized into a series of fixed-size
pages.  These are 2K in size for Jet3 (Access 97) and 4K for Jet4 (Access
2000/2002).  All data in MDB files exists within pages, of which there are 
a number of types.

The first byte of each page identifies the page type as follows.

```
0x00 Database definition page.  (Always page 0)
0x01 Data page
0x02 Table definition
0x03 Intermediate Index pages
0x04 Leaf Index pages 
0x05 Page Usage Bitmaps (extended page usage)
0x08 ??
```

Database Definition Page
------------------------

Each MDB database has a single definition page located at beginning of the
file.  Not a lot is known about this page, and it is one of the least
documented page types.  However, it contains things like Jet version,
encryption keys, and name of the creating program.  Note, this page is
"encrypted" with a simple rc4 key starting at offset 0x18 and extending for
126 (Jet3) or 128 (Jet4) bytes.

The 15 bytes at offset 0x04 name the engine which wrote the file, "Standard
Jet DB" for Jet 3 and Jet 4, "Standard ACE DB" for Access 2007 and later, and
"MSISAM Database" for the Money file variant, which uses its own encryption.

Offset 0x14 begins a 4 byte file format version.  The first byte is the format
code:

- 0x00 for Jet 3, which Access 97 writes
- 0x01 for Jet 4, which Access 2000 through 2003 write
- 0x02 for the format Access 2007 writes
- 0x03 for the format Access 2010 writes
- 0x04 for Access 2013, which wrote what Access 2010 wrote, so no file has
  been seen carrying this code
- 0x05 for the format Access 2016 writes
- 0x06 for the format Access 2019 writes.  Access 2019 and Access 2021 both
  report themselves as Access version 16, so there is no Access 17

The first byte is what the `mdb-ver` utility reads.  The second byte, at 0x15,
is a sub-version, and ms access tests it only for codes 3 and 4.  Together the
two are a forward compatibility gate with two levels, raised by what the
database holds and never lowered:

- `02 00` a database with nothing past Access 2007 in it
- `03 00` one which gained an Access 2010 feature that Access 2007 SP2 opens
  with that object read only, such as a calculated column
- `03 01` one which gained a feature Access 2007 SP2 must refuse, such as one
  of the Access 2010 sort orders, Access 2010 encryption, or publication to
  Access Services

The other two bytes have been seen only as zero.

The 20 bytes (Jet3) or 40 bytes (Jet4) starting at 0x42 are the database
password.  In Jet4, there is an additional mask applied to this password
derived from the database creation date (also stored on this page as 8 bytes
starting at offset 0x72).

The 4 bytes at 0x3e on the Database Definition Page are the database key.
If it is non-zero the other pages are encrypted.  Jet3 and Jet4 encrypt each
page with RC4, keyed by the database key XOR the page number.  Access 2007 and
later instead use Office (RC4 CryptoAPI or AES) encryption keyed by the
password, described by a structure whose 2 byte length is at 0x299; mdbtools
does not support it.

The 2 bytes at 0x3C are the default database code page.  It names the charset
of all text in a Jet3 file.  Jet4 stores text as unicode, so the code page
describes nothing there, though it still reads 1252.

The 2 bytes at 0x3A (Jet3) or 4 bytes at 0x6E (Jet4) are the default text
collating sort order.

The 4 bytes at 0x6A (Jet4 and later) are the build number of the engine which
created the database.  A Jet3 file holds 0 there.

Over 1337 databases gathered from the open web the field takes only 20 values,
and every one is a released build:

| 0x6A | engine |
|---|---|
| 2927, 3714, 4431, 6218, 8015, 8618 | Jet 4.0 SP3 through SP8 |
| 9025, 9511, 9635, 9704, 9756, 9765, 9801 | the Jet 4.0 updates after SP8 |
| 4214 | a pre-release Access 2007 |
| 4518 | Access 2007, and every later ace release |

These are the build part of the `msjet40.dll` version, so 9801 means the file
was made by `msjet40.dll` 4.0.9801.  It is the commonest value, because it is
the last Jet 4.0 build and every later windows has shipped it.

The ace engine is different.  It writes 4518, which is the Access 2007 release
build, and it has never changed the value, so an Access 2010, 2016 or 2019 file
reads 4518 as well.  159 of the Jet4 files carry 4518 too, which are the ones an
ace engine wrote in the older format.

So the field names the engine exactly for a Jet 4 file, and only says "ace" for
anything newer.  The value is informational and nothing else in the file
depends on it.

The byte at 0x38 (Jet3), after the header mask is removed, is 0 or 1.  A hex
dump shows it as 0x63 for 0 and 0x62 for 1.  Every column descriptor in the file
carries the same value in its own 2 byte `file_flag`.

The byte records which writer created the structure it sits in.  Over 83 Jet 3
databases, 54 hold 0 everywhere and 21 hold 1 everywhere, 7 hold 0 in the header
with 1 on some columns, and none holds 1 in the header with 0 on any column.  In
every mixed file the 1s are on the tables and columns made last: the last column
of a table, the last three columns of another, or whole tables at higher table
definition page numbers than the 0 tables beside them.  So a file where
everything is 1 was created by the later writer, and a file where only the last
few columns are 1 was created by the earlier one and added to afterwards.

A larger corpus of 228 Jet 3 files supports that reading.  Grouped by the site
which published them, 49 sites hold only 0, 18 hold only 1, and one holds both,
so the value agrees across the files of one publisher nearly always.  Grouping
is necessary rather than tidy, because 108 of the 228 files come from a single
site and the ungrouped tally is mostly that one.

Which two writers is not established.  It is not the Access version, which reads
07.53 in both groups, and it does not follow the sort order or the code page.

What is known about the two is their order in time.  `MSysObjects.DateCreate`
dates the objects a file holds, and no file with the byte set to 1 holds an
object made before October 2000, while the files holding 0 run from August 1997
onward.  Access 2000 is the first release on Jet 4.0, and it writes a Jet 3 file
when told to keep the older format, so 0 for Jet 3.5 and 1 for Jet 4.0 or later
fits what is measured.  It is not proved: only about seven files in that corpus
are older than Access 2000, which is too few to make the absence conclusive.

Data Pages
----------

Data rows are all stored in data pages.

The header of a Jet3 data page looks like this:
```
+--------------------------------------------------------------------------+
| Jet3 Data Page Definition                                                |
+------+---------+---------------------------------------------------------+
| data | length  | name       | description                                |
+------+---------+---------------------------------------------------------+
| 0x01 | 1 byte  | page_type  | 0x01 indicates a data page.                |
| 0x01 | 1 byte  | constant 1 | 1 on every page type                       |
| ???? | 2 bytes | free_space | Free space in this page                    |
| ???? | 4 bytes | tdef_pg    | Page pointer to table definition           |
| ???? | 2 bytes | num_rows   | number of records on this page             |
+--------------------------------------------------------------------------+
| Iterate for the number of records                                        |
+--------------------------------------------------------------------------+
| ???? | 2 bytes | offset_row | The record's location on this page         |
+--------------------------------------------------------------------------+
```

Notes:

- In Jet4, an additional four-byte field was added after tdef_pg.  It is a
  write time stamp, the GetTickCount() value when the page was written, which
  is the machine uptime in milliseconds.  The ms access page initializer sets
  it to zero and the write path fills it in, so most pages carry zero.  No
  reader of it is known.
- Offsets that have 0x40 in the high order byte point to a location within the
  page where a Data Pointer (4 bytes) to another data page (also known as an
  overflow page) is stored.  Called 'lookupflag' in source code.  The Data
  Pointer has the same layout as the LVAL pointers described below (row number
  in the low byte, page number in the upper three bytes).  The relocated row on
  the overflow page has the 0x80 flag set, so it is only reached through the
  pointer and is not read again when the overflow page is scanned.
- Offsets that have 0x80 in the high order byte are deleted rows.  Called
  'delflag' in source code.  A row can be both deleted and an overflow
  pointer.
- The offset itself is the low 13 bits of the field, mask 0x1FFF.  The
  remaining bit, 0x2000, has not been seen set on any row offset examined.


Rows are stored from the end of the page to the top of the page.  So, the first
row stored runs from the row's offset to page_size - 1.  The next row runs from
its offset to the previous row's offset - 1, and so on.

Decoding a row requires knowing the number and types of columns from its TDEF
page. Decoding is handled by the routine mdb_crack_row().

```
+--------------------------------------------------------------------------+
| Jet3 Row Definition                                                      |
+------+---------+---------------------------------------------------------+
| data | length  | name       | description                                |
+------+---------+---------------------------------------------------------+
| ???? | 1 byte  | num_cols   | Number of columns stored on this row.      |
| ???? | n bytes | fixed_cols | Fixed length columns                       |
| ???? | n bytes | var_cols   | Variable length columns                    |
| ???? | 1 byte  | eod        | length of data from begining of record     |
| ???? | n bytes | var_table[]| offset from start of row for each var_col  |
| ???? | n bytes | jump_table | Jump table (see description below)         |
| ???? | 1 byte  | var_len    | number of variable length columns          |
| ???? | n bytes | null_mask  | Null indicator.  See notes.                |
+--------------------------------------------------------------------------+
```

```
+--------------------------------------------------------------------------+
| Jet4 Row Definition                                                      |
+------+---------+---------------------------------------------------------+
| data | length  | name       | description                                |
+------+---------+---------------------------------------------------------+
| ???? | 2 bytes | num_cols   | Number of columns stored on this row.      |
| ???? | n bytes | fixed_cols | Fixed length columns                       |
| ???? | n bytes | var_cols   | Variable length columns                    |
| ???? | 2 bytes | eod        | length of data from begining of record     |
| ???? | n bytes | var_table[]| offset from start of row for each var_col  |
| ???? | 2 bytes | var_len    | number of variable length columns          |
| ???? | n bytes | null_mask  | Null indicator.  See notes.                |
+--------------------------------------------------------------------------+
```

Notes:

- A row will always have the number of fixed columns as specified in the table
  definition, but may have fewer variable columns, as rows are not updated when
  columns are added.
- All fixed-length columns are stored first to last, followed by non-null
  variable-length columns stored first to last.
- If the number of variable columns, as given in the TDEF, is 0, then the
  only items in the row are num_cols, fixed_cols, and null_mask.
- The var_len field indicates the number of entries in the var_table[].
- The var_table[] and jump_table[] are stored in reverse order.
- The eod field points at the first byte after the var_cols field.  It is used
  to determine where the last var_col ends.
- The size of the null mask is computed by (num_cols + 7)/8.
- Fixed columns can be null (unlike some other databases).
- The null mask stores one bit for each column, starting with the
  least-significant bit of the first byte.
- In the null mask, 0 represents null, and 1 represents not null.
- Values for boolean fixed columns are in the null mask: 0 - false, 1 - true.

In Jet3, offsets are stored as 1-byte fields yielding a maximum of 256 bytes.
To get around this, offsets are computed using a jump table.  The jump table
stores the number of the first column in each jump segment.  If the size of the
row is less than 256 then the jump table will not be present.  Also, eod is
treated as an additional entry of the var_table[].

For example, if the row contains 45 columns and the 15th column is the first
with an offset of 256 or greater, then the first entry in the jump table will be
0xe (14).  If the 24th column is the first one at offset >= 512, the second
entry of the jump table would be 0x17 (23).  If eod is the first entry >= 768,
the last entry in this case will be 0x2d (45).

The number of jump table entries is calculated based on the size of the row,
rather than the location of eod.  As a result, there may be a dummy entry that
contains 0xff.  In this case, and using the example above, the values in the
jump table would be 0x2d 0x17 0x0e 0xff.

In Jet4 all offsets are stored as 2 byte fields, including the var_table
entries.  Thus, the jump table was (thankfully) ditched in Jet4.


Each memo column (or other long binary data) in a row

```
+-------------------------------------------------------------------------+
| Memo Field Definition (12 bytes)                                        |
+------+---------+-------------+------------------------------------------+
| data | length  | name        | description                              |
+------+---------+-------------+------------------------------------------+
| ???? | 3 bytes | memo_len    | Total length of the memo                 |
| ???? | 1 bytes | bitmask     | See values                               |
| ???? | 4 bytes | lval_dp     | Data pointer to LVAL page (if needed)    |
| 0x00 | 4 bytes | stamp       | The same write time stamp as the one in  |
|      |         |             | the data page header.  Both this and     |
|      |         |             | lval_dp are zero when the value is       |
|      |         |             | inline (bitmask 0x80)                    |
+------+---------+-------------+------------------------------------------+
```

Values for the bitmask:

- 0x80 = the memo is in a string at the end of this header (memo_len bytes)
- 0x40 = the memo is in a unique LVAL page in a record type 1
- 0x00 = the memo is in n LVAL pages in a record type 2

If the memo is in a LVAL page, we use row_id of lval_dp to find the row.

```c
offset_start of memo = (int16*) LVAL_page[offset_num_rows + (row_id * 2) + 2] & offset_mask
if (row_id = 0)
     offset_stop of memo = 2048(jet3) or 4096(jet4)
else
     offset_stop of memo = (int16*) LVAL_page[offset_num_row + (row_id * 2)] & offset_mask // offset_mask = 0x1fff
```

The length (partial if type 2) for the memo is:
memo_page_len = offset_stop - offset_start

The bitmask is not a whole byte.  memo_len and the bitmask are one 32 bit
little endian field: the top two bits, mask 0xC0000000, hold the type, and the
low 30 bits hold the length.  That is why the maximum length of an OLE or memo
value is 0x3FFFFFFF, which is the 1 gigabyte the Access specifications give.

Note: if a memo field is marked for compression, only at value which is at
most 1024 characters when uncompressed can be compressed.  fields longer than
that _must_ be stored uncompressed.


LVAL (Long Value) Pages
-----------------------

The header of a LVAL page is just like that of a regular data page,
except that in place of the tdef_pg is the word 'LVAL'.  The four bytes after
it are the same write time stamp as on a data page.

Each memo record type 1 looks like this:

```
+------+---------+-------------+------------------------------------------+
| data | length  | name        | description                              |
+------+---------+-------------+------------------------------------------+
| ???? | n bytes | memo_value  | A string which is the memo               |
+-------------------------------------------------------------------------+
```

Each memo record type 2 looks like this:

```
+------+---------+-------------+------------------------------------------+
| data | length  | name        | description                              |
+------+---------+-------------+------------------------------------------+
| ???? | 4 bytes | lval_dp     | Next page LVAL type 2 if memo is too long|
| ???? | n bytes | memo_value  | A string which is the memo (partial)     |
+-------------------------------------------------------------------------+
```

In a LVAL type 2 data page, you have 
- 10 or 14 bytes for the header of the data page,
- 2 bytes for an offset,
- 4 bytes for the next lval_pg

So there is a block of 2048 - (10+2+4) = 2032(jet3) 
or 4096 - (14+2+4) = 4076(jet4) bytes max in a page.


TDEF (Table Definition) Pages
-----------------------------

Every table in the database has a TDEF page.  It contains a definition of 
the columns, types, sizes, indexes, and similar information.

```
+-------------------------------------------------------------------------+
| Jet3/Jet4 TDEF Header
+------+---------+-------------+------------------------------------------+
| data | length  | name        | description                              |
+------+---------+-------------+------------------------------------------+
| 0x02 | 1 bytes | page_type   | 0x02 indicate a tabledef page            |
| 0x01 | 1 bytes | unknown     |                                          |
| ???? | 2 bytes | tdef_id     | (jet3) The word 'VC'                     |
|      |         |             | (jet4) Free space in this page minus 8   |
| 0x00 | 4 bytes | next_pg     | Next tdef page pointer (0 if none)       |
+------+---------+-------------+------------------------------------------+
```

TDEFs can span multiple pages for large tables, this is accomplished using the
next_pg field.

```
+-------------------------------------------------------------------------+
| Jet3 Table Definition Block (35 bytes)                                  |
+------+---------+-------------+------------------------------------------+
| data | length  | name        | description                              |
+------+---------+-------------+------------------------------------------+
| ???? | 4 bytes | tdef_len    | Length of the data for this page         |
| ???? | 4 bytes | num_rows    | Number of records in this table          |
| 0x00 | 4 bytes | autonumber  | value for the next value of the          |
|      |         |             | autonumber column, if any. 0 otherwise   |
| 0x4e | 1 byte  | table_type  | 0x4e: user table, 0x53: system table     |
| ???? | 2 bytes | max_cols    | Max columns a row will have (deletions)  |
| ???? | 2 bytes | num_var_cols| Number of variable column slots.  A      |
|      |         |             | deleted column keeps its slot, as with   |
|      |         |             | max_cols, so this can be more than the   |
|      |         |             | variable columns the table now has       |
| ???? | 2 bytes | num_cols    | Number of columns in table (repeat)      |
| ???? | 4 bytes | num_idx     | Number of logical indexes in table       |
| ???? | 4 bytes | num_real_idx| Number of index entries                  |
| ???? | 4 bytes | used_pages  | Points to a record containing the        |
|      |         |             | usage bitmask for this table.            |
| ???? | 4 bytes | free_pages  | Points to a similar record as above,     |
|      |         |             | listing pages which contain free space.  |
+-------------------------------------------------------------------------+
| Iterate for the number of num_real_idx (8 bytes per idxs)               |
+-------------------------------------------------------------------------+
| ???? | 4 bytes | idx_rows1   | A row count statistic, kept loosely      |
| ???? | 4 bytes | idx_rows2   | A second row count statistic             |
+-------------------------------------------------------------------------+
| Iterate for the number of num_cols (18 bytes per column)                |
+-------------------------------------------------------------------------+
| ???? | 1 byte  | col_type    | Column Type (see table below)            |
| ???? | 2 bytes | col_num     | Column Number (includes deleted columns) |
| ???? | 2 bytes | offset_V    | Offset for variable length columns       |
| ???? | 2 bytes | col_id      | Id given to the column when it was       |
|      |         |             | created.  Access never renumbers it      |
| ???? | 2 bytes | file_flag   | a copy of the byte at 0x38 of the        |
|      |         |             | Database Definition Page, 0 or 1, the    |
|      |         |             | same on every column of the file.  What  |
|      |         |             | it records is unknown                    |
| ???? | 2 bytes | sort_order  | textual column sort order(0x409=General) |
| ???? | 2 bytes | misc        | prec/scale (1 byte each), or code page   |
|      |         |             | for textual columns (0x4E4=cp1252)       |
| ???? | 1 byte  | bitmask     | See Column flags bellow                  |
| ???? | 2 bytes | offset_F    | Offset for fixed length columns          |
| ???? | 2 bytes | col_len     | Length of the column (0 if memo)         |
+-------------------------------------------------------------------------+
| Iterate for the number of num_cols (n bytes per column)                 |
+-------------------------------------------------------------------------+
| ???? | 1 byte  | col_name_len| len of the name of the column            |
| ???? | n bytes | col_name    | Name of the column                       |
+-------------------------------------------------------------------------+
| Iterate for the number of num_real_idx (30+9 = 39 bytes)                |
+-------------------------------------------------------------------------+
|     Iterate 10 times for 10 possible columns (10*3 = 30 bytes)          |
+-------------------------------------------------------------------------+
| ???? | 2 bytes | col_num     | number of a column (0xFFFF= none)        |
| ???? | 1 byte  | col_order   | 0x01 =  ascendency order                 |
+-------------------------------------------------------------------------+
| ???? | 4 bytes | used_pages  | Points to usage bitmap for index         |
| ???? | 4 bytes | first_dp    | Data pointer of the index page           |
| ???? | 1 byte  | flags       | See flags table for indexes              |
+-------------------------------------------------------------------------+
| Iterate for the number of num_idx (20 bytes)                            |
+-------------------------------------------------------------------------+
| ???? | 4 bytes | index_num   | Number of the index                      |
|      |         |             |(warn: not always in the sequential order)|
| ???? | 4 bytes | index_num2  | Index into index cols list               |
| 0x00 | 1 byte  | rel_tbl_type| type of the other table in this fk       |
|      |         |             | (same values as index_type)              |
| 0xFF | 4 bytes | rel_idx_num | index number of other index in fk        |
|      |         |             | (or -1 if this index is not a fk)        |
| 0x00 | 4 bytes | rel_tbl_page| page number of other table in fk         |
| 0x01 | 1 byte  | cascade_ups | flag indicating if updates are cascaded  |
| 0x01 | 1 byte  | cascade_dels| 0x01 cascades deletes, 0x02 sets the     |
|      |         |             | foreign key to null on delete            |
| ???? | 1 byte  | index_type  | 0x01 if index is primary, 0x02 if foreign|
+-------------------------------------------------------------------------+
| Iterate for the number of num_idx                                       |
+-------------------------------------------------------------------------+
| ???? | 1 byte  | idx_name_len| len of the name of the index             |
| ???? | n bytes | idx_name    | Name of the index                        |
+-------------------------------------------------------------------------+
| Iterate while col_num != 0xffff                                         |
+-------------------------------------------------------------------------+
| ???? | 2 bytes | col_num     | Column number with variable length       |
| ???? | 4 bytes | used_pages  | Points to a record containing the        |
|      |         |             | usage bitmask for this column.           |
| ???? | 4 bytes | free_pages  | Points to a similar record as above,     |
|      |         |             | listing pages which contain free space.  |
+-------------------------------------------------------------------------+

+-------------------------------------------------------------------------+
| Jet4 Table Definition Block (55 bytes)                                  |
+------+---------+-------------+------------------------------------------+
| data | length  | name        | description                              |
+------+---------+-------------+------------------------------------------+
| ???? | 4 bytes | tdef_len    | Length of the data for this page         |
| 1625 | 4 bytes | tag         | A structure tag, repeated at the head of |
|      |         |             | each column block and each logical index |
|      |         |             | block.  Index column blocks use 1923     |
| ???? | 4 bytes | num_rows    | Number of records in this table          |
| 0x00 | 4 bytes | autonumber  | value for the next value of the          |
|      |         |             | autonumber column, if any. 0 otherwise   |
| 0x01 | 1 byte  | autonum_flag| 0x01 enables autonumbering               |
| 0x00 | 3 bytes | unknown     | zero in every definition seen            |
| 0x00 | 4 bytes | ct_autonum  | autonumber value for complex type column(s) |
|      |         |             | (shared across all columns in the table) |
| ???? | 8 bytes | unknown     | uninitialized page residue, not a field  |
| 0x4e | 1 byte  | table_type  | 0x4e: user table, 0x53: system table     |
| ???? | 2 bytes | max_cols    | Max columns a row will have (deletions)  |
| ???? | 2 bytes | num_var_cols| Number of variable column slots.  A      |
|      |         |             | deleted column keeps its slot, as with   |
|      |         |             | max_cols, so this can be more than the   |
|      |         |             | variable columns the table now has       |
| ???? | 2 bytes | num_cols    | Number of columns in table (repeat)      |
| ???? | 4 bytes | num_idx     | Number of logical indexes in table       |
| ???? | 4 bytes | num_real_idx| Number of index entries                  |
| ???? | 4 bytes | used_pages  | Points to a record containing the        |
|      |         |             | usage bitmask for this table.            |
| ???? | 4 bytes | free_pages  | Points to a similar record as above,     |
|      |         |             | listing pages which contain free space.  |
+-------------------------------------------------------------------------+
| Iterate for the number of num_real_idx (12 bytes per idxs)              |
+-------------------------------------------------------------------------+
| ???? | 4 bytes | idx_rows1   | A row count statistic, kept loosely      |
| ???? | 4 bytes | idx_rows2   | A second row count statistic             |
| 0x00 | 4 bytes | ???         | zero in every block seen                 |
+-------------------------------------------------------------------------+
| Iterate for the number of num_cols (25 bytes per column)                |
+-------------------------------------------------------------------------+
| ???? | 1 byte  | col_type    | Column Type (see table below)            |
| 1625 | 4 bytes | tag         | the same structure tag as the definition |
| ???? | 2 bytes | col_num     | Column Number (includes deleted columns) |
| ???? | 2 bytes | offset_V    | Offset for variable length columns       |
| ???? | 2 bytes | col_id      | Id given to the column when it was       |
|      |         |             | created.  Access never renumbers it, so  |
|      |         |             | a table which has had a column deleted   |
|      |         |             | has gaps here while col_num shifts       |
| ???? | 2 bytes | misc        | prec/scale (1 byte each), or sort order  |
|      |         |             | for textual columns(0x409=General)       |
|      |         |             | or "complexid" for complex columns (4bytes)|
| ???? | 2 bytes | misc_ext    | the collation variant in the 1st byte,   |
|      |         |             | the text sort order version in the 2nd.  |
|      |         |             | together with the sort order above these |
|      |         |             | make the ms access 32 bit sort id: lcid, |
|      |         |             | variant, weight table family             |
| ???? | 1 byte  | bitmask     | See column flags below                   |
| ???? | 1 byte  | misc_flags  | See column ext flags below               |
| 0x00 | 4 bytes | ???         | zero in every column seen                |
| ???? | 2 bytes | offset_F    | Offset for fixed length columns          |
| ???? | 2 bytes | col_len     | Length of the column (0 if memo/ole)     |
+-------------------------------------------------------------------------+
| Iterate for the number of num_cols (n*2 bytes per column)               |
+-------------------------------------------------------------------------+
| ???? | 2 bytes | col_name_len| len of the name of the column            |
| ???? | n bytes | col_name    | Name of the column (UCS-2 format)        |
+-------------------------------------------------------------------------+
| Iterate for the number of num_real_idx (30+22 = 52 bytes)               |
+-------------------------------------------------------------------------+
| 1923 | 4 bytes | tag         | the structure tag of an index column     |
|      |         |             | block                                    |
+-------------------------------------------------------------------------+
| Iterate 10 times for 10 possible columns (10*3 = 30 bytes)              |
+-------------------------------------------------------------------------+
| ???? | 2 bytes | col_num     | number of a column (0xFFFF= none)        |
| ???? | 1 byte  | col_order   | 0x01 =  ascendency order                 |
+-------------------------------------------------------------------------+
| ???? | 4 bytes | used_pages  | Points to usage bitmap for index         |
| ???? | 4 bytes | first_dp    | Data pointer of the index page           |
| ???? | 4 bytes | unknown     | uninitialized page residue               |
| ???? | 1 byte  | flags       | See flags table for indexes              |
| ???? | 1 byte  | complex_idx | 2 if the index is over a complex column. |
|      |         |             | 0 on every other index seen              |
| 0x00 | 4 bytes | unknown     | zero in every block seen                 |
+-------------------------------------------------------------------------+
| Iterate for the number of num_idx (28 bytes)                            |
+-------------------------------------------------------------------------+
| 1625 | 4 bytes | tag         | the same structure tag as the definition |
| ???? | 4 bytes | index_num   | Number of the index                      |
|      |         |             |(warn: not always in the sequential order)|
| ???? | 4 bytes | index_num2  | Index into index cols list               |
| 0x00 | 1 byte  | rel_tbl_type| type of the other table in this fk       |
|      |         |             | (same values as index_type)              |
| 0xFF | 4 bytes | rel_idx_num | index number of other index in fk        |
|      |         |             | (or -1 if this index is not a fk)        |
| 0x00 | 4 bytes | rel_tbl_page| page number of other table in fk         |
| 0x01 | 1 byte  | cascade_ups | flag indicating if updates are cascaded  |
| 0x01 | 1 byte  | cascade_dels| 0x01 cascades deletes, 0x02 sets the     |
|      |         |             | foreign key to null on delete            |
| ???? | 1 byte  | index_type  | 0x01 if index is primary, 0x02 if foreign|
| 0x00 | 4 bytes | ???         | zero in every block seen                 |
+-------------------------------------------------------------------------+
| Iterate for the number of num_idx                                       |
+-------------------------------------------------------------------------+
| ???? | 2 bytes | idx_name_len| len of the name of the index             |
| ???? | n bytes | idx_name    | Name of the index (UCS-2)                |
+-------------------------------------------------------------------------+
| Iterate while col_num != 0xffff                                         |
+-------------------------------------------------------------------------+
| ???? | 2 bytes | col_num     | Column number with variable length       |
| ???? | 4 bytes | used_pages  | Points to a record containing the        |
|      |         |             | usage bitmask for this column.           |
| ???? | 4 bytes | free_pages  | Points to a similar record as above,     |
|      |         |             | listing pages which contain free space.  |
+-------------------------------------------------------------------------+
```

Columns flags:

- 0x01: fixed length column
- 0x02: updatable
- 0x04: is auto long
- 0x10: ms access maintains the column and hides it.  The columns of the
      system catalog tables, `MSysComplexColumns` included, and the
      replication columns ("s_" and "Gen_") carry it
- 0x20: the column holds a windows security identifier.  The only columns seen
      carrying it are `MSysObjects.Owner` and `MSysACEs.SID`
- 0x40: is auto guid
- 0x80: hyperlink. Syntax is "Link Title#http://example.com/somepage.html#" or
      "#PAGE.HTM#"

Bit 0x08 has not been seen set on any column examined.

The Jet4 misc_flags byte, which the Jet3 column block has no room for, holds a
second set:

- 0x01: the text of the column is stored in the compressed unicode form
- 0x02: a column added to make the database replicable.  These are the row
      tracking columns `s_GUID`, `s_Lineage`, `s_Generation` and
      `s_ColLineage`, and a `Gen_<column>` or `Aen_<column>`, which tracks one
      column for conflict resolution.  The types vary, because the types of the
      columns they track vary
- 0x08: the column is the complex value foreign key of a complex column's flat
      table.  Access refuses to open a table whose flat table does not carry
      it
- 0x10: a value column of an attachment flat table, which are `FileData`,
      `FileFlags`, `FileName`, `FileType`, `FileTimeStamp` and `FileURL`.
      Access writes it and does not require it
- 0x20: a version history complex column.  Access requires it, and without it
      shows that column in the datasheet as a column of its own
- 0xC0: the column is calculated.  The expression is the `Expression` property
      of the column, and each row stores the last computed value inside a 23
      byte wrapper which gives the length of the value as 4 bytes at offset
      16, the value itself following.  A calculated column of fixed type
      declares a length of 39

In Access 2007 and Access 2010, "Complex Columns" (multivalued fields, version
history, attachments) always have the flag byte set to exactly 0x07.

Attachment data is stored with an 8 byte wrapper header, a 4 byte flag telling
whether the content is deflated and a 4 byte length, then the content.  The
content begins with its own header:

- 32 bits header length (this includes the length)
- 32 bits constant 1
- 32 bits character count of the string which follows
- the file extension as UCS-2 with a null terminator

Ms access stores `jpg jpeg gif png zip cab docx xlsx xlsb pptx` raw and
deflates everything else, archives and mp3 files included.

Index flags:
- 0x01 Unique
- 0x02 IgnoreNuls
- 0x08 Required
- 0x80 set by Access 2000 and later on every index it writes

Bits 0x04, 0x10, 0x20 and 0x40 have not been seen set on any index examined.
The col_order byte of an index column has been seen only as 0x01 for ascending
and 0x00 for descending.

Column Type may be one of the following (not complete):
```
    BOOL            = 0x01 /* Boolean         ( 1 bit ) */
    BYTE            = 0x02 /* Byte            ( 8 bits) */
    INT             = 0x03 /* Integer         (16 bits) */
    LONGINT         = 0x04 /* Long Integer    (32 bits) */
    MONEY           = 0x05 /* Currency        (64 bits) */
    FLOAT           = 0x06 /* Single          (32 bits) */
    DOUBLE          = 0x07 /* Double          (64 bits) */
    DATETIME        = 0x08 /* Date/Time       (64 bits) */
    BINARY          = 0x09 /* Binary        (255 bytes) */
    TEXT            = 0x0A /* Text          (255 bytes) */
    OLE             = 0x0B /* OLE = Long binary */
    MEMO            = 0x0C /* Memo = Long text*/
    REPID           = 0x0F /* GUID */
    NUMERIC         = 0x10 /* Scaled decimal  (17 bytes) */
    BIGBINARY       = 0x11 /* Fixed binary, declared length 4000 */
    COMPLEX         = 0x12 /* Key of a complex column  (32 bits) */
    BIGINT          = 0x13 /* Long Integer    (64 bits), ace only */
    EXTDATETIME     = 0x14 /* Date/Time Extended    (42 bytes) */

```

0x0D and 0x0E are not column types.  The column factory in both the jet and
the ace engine sends those two codes to its error path, so neither engine can
create such a column or read one, and no file examined holds one.

BIGBINARY is a fixed length binary column longer than the 255 byte BINARY.
The only use seen is the `Data` column of `MSysAccessObjects`, which holds the
forms, reports and vba of the database as a chunked OLE compound file.  It is Jet 4
only: Jet 3 used BINARY for the same column and the ace engine dropped the
table, so it cannot appear in an accdb.

BIGINT arrived in Access 2016 at build 16.0.7812, so an earlier ace engine
rejects it.

DATETIME is a 64 bit double counting days from 1899-12-30.  The fractional
part is the time, and it is **always a positive fraction of the day, whatever
the sign of the double**, rather than a distance from zero.  So a date before
the epoch has a negative whole day part and a positive time, and a value whose
whole day part is zero reads the same whether the double is positive or
negative, which makes the sign of such a value unrecoverable.  Access records a
time to second precision, but a file can hold a double carrying far more.  One
observed value is `-0x1.70681cf4d859ap-12`, which is 30.355776 seconds into
1899-12-30, and values have been seen which resolve finer than a nanosecond.

EXTDATETIME is the "Date/Time Extended" type of Access 2019, which keeps a
date from year 1 with 7 digits of fractional seconds.  It is stored as 42
bytes of ascii: 19 digits of days since 0001-01-01, a colon, 12 digits of
seconds, 7 digits of 100 nanosecond units, then `:7` and a null byte.

Notes on reading index metadata:

There are 2 types of index metadata, "physical" index info (denoted by
num_real_idx) and "logical" index info (denoted by num_idx).  Normally, there
is a 1 to 1 relationship between these 2 types of information.  However there
can be more logical index infos than physical index infos (currently only seen
for foreign key indexes).  In this situation, one or more of the logical
indexes actually share the same underlying physical index (the index_num2
indicates which physical index backs which logical index).

As noted in the previous paragraph, physical index sharing is generally only
seen when a foreign key index has been created.  When access creates a
relationship between 2 tables with "enforce referential integrity" enabled,
each of the tables gets an extra logical index with type 2 (foreign key).
These logical indexes contain extra information, primarily pointers to the
related table (rel_tbl_page) and logical index (rel_idx_num).  Also, the
rel_tbl_type value indicates which table in the relationship is the "primary"
table (the one one from which cascaded updates/deletes flow).  If the indexed
columns for the foreign key are already indexed by another logical index in
the table (e.g. an index which the user has explicitly created), then the
logical foreign key index will simply share the underlying physical index
data.

Notes on deleted and added columns: (sort of Jet4 specific)

If a fixed length column is deleted the offset_F field will contain the offsets 
of the original row definition.  Thus if the number of columns on the row does 
not match the number in the tdef, the offset_F field could be used to return 
the proper data. Columns are never really deleted in the row data.  The deleted
column will forever exist and be set to null for new rows. 

A row may have less than max_cols columns but will never have more, as max_cols
is never decremented.  If you have a table with 6 columns, delete one, and add 
one, then max_cols will be 7.

For variable length columns, offset_V will hold the position in the offset table
of that column.  Missing columns are set to null for new rows.


Page Usage Maps
---------------

There are three uses for the page usage bitmaps.  There is a global page usage 
stored on page 1 which tracks allocated pages throughout the database.  

Tables store two page usage bitmaps.  One is a straight map of which pages are 
owned by the table.  The second is a map of the pages owned by the table which 
have free space on them (used for inserting data).

The table bitmaps appear to be of a fixed size for both Jet 3 and 4 (128 and 64
bytes respectively).  The first byte of the map is a type field.

```
+--------------------------------------------------------------------------+
| Type 0 Page Usage Map                                                    |
+------+---------+---------------------------------------------------------+
| data | length  | name       | description                                |
+------+---------+---------------------------------------------------------+
| 0x00 | 1 byte  | map_type   | 0x00 indicates map stored within.          |
| ???? | 4 byte  | page_start | first page for which this map applies      |
+------+---------+---------------------------------------------------------+
| Iterate for the length of map                                            |
+--------------------------------------------------------------------------+
| ???? | 1 byte  | bitmap     | each bit encodes the allocation status of a|
|      |         |            | page. 1 indicates allocated to this table. |
|      |         |            | Pages are stored starting with the low     |
|      |         |            | order bit of the first byte.               |
+--------------------------------------------------------------------------+
```

If you're paying attention then you'll realize that the relatively small size of
the map (128*8*2048 or 64*8*4096 = 2 Meg) means that this scheme won't work with
larger database files although the initial start page helps a bit.  To overcome
this there is a second page usage map scheme with the map_type of 0x01.

```
+--------------------------------------------------------------------------+
| Type 1 Page Usage Map                                                    |
+------+---------+---------------------------------------------------------+
| data | length  | name       | description                                |
+------+---------+---------------------------------------------------------+
| 0x01 | 1 byte  | map_type   | 0x01 indicates this is a indirection list. |
+------+---------+---------------------------------------------------------+
| Iterate for the length of map                                            |
+--------------------------------------------------------------------------+
| ???? | 4 bytes | map_page   | pointer to page type 0x05 containing map   |
+--------------------------------------------------------------------------+
```

Note that the initial start page is gone and is reused for the first page 
indirection.  The 0x05 type page header looks like:

```
+--------------------------------------------------------------------------+
| Usage Map Page (type 0x05)                                               |
+------+---------+---------------------------------------------------------+
| data | length  | name       | description                                |
+------+---------+---------------------------------------------------------+
| 0x05 | 1 byte  | page_type  | allocation map page                        |
| 0x01 | 1 byte  | unknown    | always 1 as with other page types          |
| 0x00 | 2 bytes | unknown    |                                            |
+------+---------+---------------------------------------------------------+
```

The rest of the page is the allocation bitmap following the same scheme (lsb
to msb order, 1 bit per page) as a type 0 map.  This yields a maximum of
2044*8=16352 (jet3) or 4092*8 = 32736 (jet4) pages mapped per type 0x05 page.
Given 128/4+1 = 33 or 64/4+1 = 17 page pointers per indirection row (remember
the start page field is reused, thus the +1), this yields 33*16352*2048 = 1053
Meg (jet3) or 17*32736*4096 = 2173 Meg (jet4) or enough to cover the maximum
size of each of the database formats comfortably, so there is no reason to
believe any other page map schemes exist.


Indices
-------

Indices are not completely understood but here is what we know.

```
+-------------------------------------------------------------------------+
| Index Page (type 0x03)                                                  |
+------+---------+-------------+------------------------------------------+
| data | length  | name        | description                              |
+------+---------+-------------+------------------------------------------+
| 0x03 | 1 bytes | page_type   | 0x03 indicate an index page              |
| 0x01 | 1 bytes | unknown     |                                          |
| ???? | 2 bytes | free_space  | The free space at the end this page      |
| ???? | 4 bytes | parent_page | The page number of the TDEF for this idx |
| ???? | 4 bytes | prev_page   | Previous page at this index level        |
| ???? | 4 bytes | next_page   | Next page at this index level            |
| ???? | 4 bytes | tail_page   | Pointer to tail leaf page                |
| ???? | 1 byte  | pref_len    | Length of the shared entry prefix        |
| ???? | 1 byte  | level       | Depth of this page above the leaves      |
+-------------------------------------------------------------------------+
```

The layout above is the Jet3 one.  Jet4 adds a 4 byte write stamp after
parent_page, the same field a data page carries, and widens pref_len to 2
bytes, which puts its level at offset 26.

The level is the depth of the page above the leaves of the index tree: 0 on a
leaf page and one more than its children on a node page.  Over 5904 Jet 3 index
pages it is 0 on all 5683 leaf pages, 1 on 212 node pages and 2 on 9 more, so
reading pref_len as 2 bytes on Jet 3 returns 256 or 512 on every node page.

Index pages come in two flavors.

0x04 pages are leaf pages which contain one entry for each row in the table.  
Each entry is composed of a flag, the indexed column values and a page/row 
pointer to the data.

0x03 index pages make up the rest of the index tree and contain a flag, the 
indexed columns, the page/row that contains this entry, and the leaf page or 
intermediate (another 0x03 page) page pointer for which this is the first 
entry on.

Both index types have a bitmask starting at 0x16(jet3) or 0x1b(jet4) which
identifies the starting location of each index entry on this page.  The first
entry begins at offset 0xf8(jet3) or 0x1e0(jet4), and is not explicitly
indicated in the bitmask.  Note that the count in each byte begins with the
low order bit.  For example take the data:

```
00 20 00 04 80 00 ...
```

Convert the bytes to binary starting with the low order bit in each byte.  v's
mark where each entry begins:
```
v                v                 v                v
0000 0000  0000 0100  0000 0000  0010 0000  0000 0001  0000 0000
-- 00 ---  -- 20 ---  -- 00 ---  -- 04 ---  -- 80 ---  -- 00 ---
```

As noted earlier, the first entry is implicit.  The second entry begins at an
offset of 13 (0xd) bytes from the first.  The third entry 26 (0x1a) bytes from
the first.  The final entry starts at an offset of 39 (0x27) bytes from the
first.  In this example the rest of the mask (up to offset 0xf8/0x1e0) would be
zero-filled and thus this last entry isn't an actual entry, but the stopping
point of the data.

For Jet3, (0xf8 - 0x16) * 8 = 0x710 and 0x800 - 0xf8 = 0x708.
For Jet4, (0x1e0 - 0x1b) * 8 = 0xe28 and 0x1000 - 0x1e0 = 0xe20.
So the mask just covers the page, including space to indicate if the last entry
goes to the end of the page.  One wonders why MS didn't use a row offset table
like they did on data pages.  It seems like it would have been easier and more
flexible.

So now we come to the index entries for type 0x03 pages which look like this:

```
+-------------------------------------------------------------------------+
| Index Record                                                            |
+------+---------+-------------+------------------------------------------+
| data | length  | name        | description                              |
+------+---------+-------------+------------------------------------------+
| 0x7f | 1 byte  | flags       | 0x80 LSB, 0x7f MSB, 0x00 null?           |
| ???? | variable| indexed cols| indexed column data                      |
| ???? | 3 bytes | data page   | page containing row referred to by this  |
|      |         |             | index entry                              |
| ???? | 1 byte  | data row    | row number on that page of this entry    |
| ???? | 4 bytes | child page  | next level index page containing this    |
|      |         |             | entry as last entry.  Could be a leaf    |
|      |         |             | node.                                    |
+-------------------------------------------------------------------------+
```

The flag field is generally either 0x00, 0x7f, 0x80, or 0xFF.  0x80 is the
one's complement of 0x7f and all text data in the index would then need to be
negated.  The reason for this negation is descending order.  The 0x00 flag
indicates that the key column is null (or 0xFF for descending order), and no
data will follow, only the page pointer.  In multicolumn indexes the flag
field plus data is repeated for the number of columns participating in the
key.  Index entries are always sorted based on the lexicographical order of
the entry bytes of the entire index entry (thus descending order is achieved
by negating the bytes).  The flag field ensures that null values are always
sorted at the beginning (for ascending) or end (for descending) of the index.

Note, there is a compression scheme utilizing a shared entry prefix.  If an
index page has a shared entry prefix (idicated by a pref_len > 0), then the
first pref_len bytes from the first entry need to be pre-pended to every
subsequent entry on the page to get the full entry bytes.  For example,
normally an index entry with an integer primary key would be 9 bytes (1 for
the flags field, 4 for the integer, 4 for page/row).  If the pref_len on the
index page were 4, every entry after the first would then contain only 5
bytes, where the first byte is the last octet of the encoded primary key field
(integer) and the last four are the page/row pointer.  Thus if the first key
value on the page is 1 and it points to page 261 (00 01 05) row 3, it becomes:

```
7f 00 00 00 01 00 01 05 03
```
and the next index entry can be:

```
02 00 01 05 04
```
That is, the shared prefix is [7f 00 00 00], so the actual next entry is:

```
[7f 00 00 00] 02 00 01 05 04
```

so the key value is 2 (the last octet changes to 02) page 261 row 4.

An index entry is limited to 255 characters of key, which is 255 bytes in a
jet 3 file and 510 in a jet 4 one, since a text field is measured in single
bytes in the first and in 2 byte units in the second.  A longer key keeps all
but the last 2 bytes of that and ends with a 2 byte digest of the bytes the
truncation discards, which makes two long keys with the same start compare
unequal.  The digest is a CRC-16/ARC, polynomial 0x8005, with the register
held in on-disk byte order.  Every discarded byte feeds it, but the last is
xored into the register instead of being clocked through it.  This holds
whatever the last byte is: the terminator of a text key, a weight an odd nibble
left behind, or the low byte of a number in the last column of a multi column
index.

A text column is indexed up to 255 characters, memo columns included, and
trailing spaces are dropped before the value is encoded.

Access stores an 'alphabetic sort order' version of the text key columns in the
index.  Here is the encoding as we know it:

```
0-9: 0x56-0x5f
A-Z: 0x60-0x79
a-z: 0x60-0x79
```

Once converted into this (non-ascii) character set, the text value can be
sorted in 'alphabetic' order using the lexicographical order of the entry
bytes.  A text column will end with a NULL (0x00 or 0xff if negated).

Note, this encoding is the "General" sort order in Access 2000-2007 (1033,
version 0).  As of Access 2010, this is now called the "General legacy" sort
order, and the 2010 "General" sort order is a new encoding (1033, vesion 1).

A collation is named by the whole 32 bit sort id rather than by the LCID, and
ms access resolves the language chosen in the options into one of those ids
before writing it.  The list it offers is a list of collations, not of
languages: Russian cannot be chosen at all, German only as German Phone Book,
Danish only as Norwegian/Danish.  58 LCIDs resolve to 1033 and need no
tailoring, so a weight table is the whole of their collation.

A locale which does not resolve to 1033 has a tailoring: a few characters
which take a different weight from the one the table of its family gives
them.  Every case of a character moves together, so a tailoring which moves
`z` moves `Z` with it.  Turkish is the exception measured so far, giving
capital I the weights of the dotless `ı` and the dotted capital `İ` the
weights of `i`.

Some collations also weigh a sequence of characters as a unit, such as the
czech `ch`, which takes one weight of its own between h and i.  Three things
about a sequence have been measured:

- it counts as one character in everything which follows it, so the diacritic
  list, the unprintable offsets and the flag codes all see one unit fewer than
  the value has characters
- it is weighed as a unit only when it is written all in lower case, all in
  upper case, or in title case.  `cha`, `Cha` and `CHa` contract, `cHa` does
  not, and a sequence barred by its case falls back to the longest sequence
  left, so the hungarian `DZsa` is the sequence `DZ` and then an `s`
- a character which writes no inline bytes, such as an apostrophe or a hyphen,
  stops a sequence, so `c'ha` is a `c` and an `h`

A few characters go the other way and are weighed as more than one, writing
the weights of the characters they stand for.  **These count as that many in
everything which follows**, the same offsets a sequence counts as one in, so a
diacritic after one of them sits that many places later than the character
count suggests:

| weight table family | weighed as two | as three | as four |
|---|---|---|---|
| general legacy | 32, among them `Æ æ` (ae), `Þ þ` (th), `ß` (ss), `Ĳ ĳ Œ œ`, the croatian and serbian digraphs `Ǆ` to `ǌ` and `Ǳ ǲ ǳ`, the hebrew yiddish digraphs, and the latin ligatures `ﬀ ﬁ ﬂ ﬆ` | 2, the ligatures `ﬃ ﬄ` | none |
| general | 579, the same ones, `Ü ü` (ue), the arabic ligature presentation forms, and the seven named below | 56, more arabic ligatures and the square symbols U+3378, U+33A0 and their kin | 4, U+06D6, U+06D7, U+FD24 and U+FD6F |

The two families disagree about `ü`.  It writes "ue" in the general family and
is an ordinary character with a diacritic weight in the general legacy one.

They can be told apart in a weight table without knowing the list: such a
character holds the whole weight of each character it stands for, where a
character which simply has a long weight, such as `^`, `¼` or `©`, starts with
a prefix byte which is never a weight on its own.  The weight is split at every
width which divides it, widest first, and the widest split whose every part is
some character's whole weight gives the count.

A weight entry is one character, in both families, and the cjk ideographs are
the one exception: their weight is the lead entry `fdff` and then the
ideograph's own, two entries standing for one character.  A repeat mark after
one writes `05` and the first byte of the last entry rather than the whole
weight.  Every other weight of two entries stands for two characters, the
japanese digraphs U+309F, U+30FF and U+FC28 among them, and a mark after one of
those writes the marker.

A run of three or more of the four characters whose weight is four entries,
U+06D6, U+06D7, U+FD24 and U+FD6F, makes ms access write the end of the weights
over the start of the diacritic weights.  The length is
`2 * (entries - 3 * characters)` bytes, nothing where that is not positive, and
every character counts, including one which writes no weight at all.  A weight
shorter than its entries is padded out with zeroes in the weights ms access
holds, and the overrun copies the padding.  What lands in the diacritic weights
is the weight of whichever character ends the value, so this is ms access
reading past the end of its own weights, but an entry has to match what it
wrote.

A flag code is a two bit code which some characters carry and most do not.
What it means is not known.  A second run of two bit codes follows them and
says what each character which took a flag code is: 1 for a character of its
own, 2 for a repeat mark standing for the one before it, and 3 for a prolonged
sound mark.  A prolonged sound mark carries 3 even after the moraic nasal,
where the weight it writes is the whole of the kana before it, so the code
does not depend on writing a vowel.  The flag codes sit in the same memory as
the tertiary weights, straight after a run of three tertiary weights for each
character of the value, and the tertiary run takes one weight for each
character unit.  So a value of more units than `3 * characters` writes its last
tertiary weights over the head of the flag codes: the weight of unit
`(3 * characters) + j` lands on the flag code of the jth character which took
one.  Whichever of the two is written last is the one which stays, and a
character writes its flag code before its own tertiary weight, so one which
lands on its own code loses it.

The unit count here is the same one the diacritic weights carry, which is why a
character with no weight at all takes no part: it writes no tertiary weight
either, and a repeat mark writes one only when it has a weight to copy.

A tertiary weight is 0, 1, 2 or 3.  Almost every character weighs 1, and 482
characters do not: the superscripts weigh 3, the subscripts and the hangul jamo
weigh 2, and the lao tone marks and some modifier letters weigh 0.  They were
measured one character at a time, by putting each unit of each character on the
index in turn and reading the code which came back.  A trailing flag code of 3
is trimmed away, but a trailing tertiary weight of 3 is not, since ms access
trims a flag rather than a weight.

Only the general family can reach any of this.  The general legacy table gives
no character more than three units, so a value there can never pass `3 *
characters` and never writes past any of the three runs.

The diacritic weights give a second count, because a character carries at most
one of them for each character it stands for.  Seven characters of the general
family need it, U+0385, U+1B3B, U+FC25, U+FC33, U+FCC2, U+FD3C and U+FD3D.
The dialytika tonos U+0385 is the clear one: its weight `07 53 01` does not
split, and it carries the two diacritic weights `02 0c` of the diaeresis and
the tonos, so it is weighed as two.  Taking the larger of the two counts names
every character measured and nothing else.

The unprintable offset uses the same count, measured with the hyphen U+2010,
whose unprintable weight is `84` in both families:

| value | entry | offset |
|---|---|---|
| `k` `a` `‐` | `7f 5c 4a 01 01 01 01 800f 06 84 00` | 2 |
| `k` `ß` `‐` | `7f 5c 6b6b 01 01 01 01 8013 06 84 00` | 3 |
| `k` `ﬃ` `‐` | `7f 5c 535359 01 01 01 01 8017 06 84 00` | 4 |

The arabic ligature presentation forms carry the weights of the letters they
join.  The entry below is `k` and then U+FBEA twice, where the second
ligature's secondary weight `d6` lands at offset 3 rather than 2:

| value | entry |
|---|---|
| `k` + U+FBEA | `7f 0e36 29d7 290b 01 02 d6 00` |
| `k` + U+FBEA + U+FBEA | `7f 0e36 29d7 290b 29d7 290b 01 02 d6 02 d6 00` |
| `k` + U+06D7 + U+06D7 | `7f 0e36 298c 2920 0e48 0e35 298c 2920 0e48 0e35 01 02 14 02 02 02 14 00` |

A character weighed as more than one can carry a secondary weight for each of
the characters it stands for rather than one for the pair, and then the extra
weights fill those places themselves instead of taking the placeholder:

| value | entry |
|---|---|
| `k` + U+01C4 + U+01C4 | `7f 5c 4f78 4f78 01 02 02 14 02 14 00` |
| `k` + U+01E2 + U+01E2 | `7f 5c 4a51 4a51 01 02 17 17 17 17 00` |

The diacritic weights are written into a run of one byte per character, and
what a character writes there moves that run on by what it wrote rather than by
the number of characters it is weighed as.  A weight after the character shows
how far, in the general family:

| value | entry |
|---|---|
| `k` + `a` + `m` + `ä` | `7f 0e36 0e02 0e51 0e02 01 02 02 02 13 00` |
| `k` + U+FBEA + `m` + `ä` | `7f 0e36 29d7 290b 0e51 0e02 01 02 d6 02 02 13 00` |
| `k` + U+01E2 + `m` + `ä` | `7f 0e36 0e02 0e21 0e51 0e02 01 02 17 17 02 13 00` |
| `k` + U+06D7 + `m` + `ä` | `7f 0e36 298c 2920 0e48 0e35 0e51 0e02 01 02 14 02 02 02 02 13 00` |
| `k` + U+0385 + `m` + `ä` | `7f 0e36 0753 01 0e51 0e02 01 02 02 0c 02 13 00` |

U+FBEA is weighed as two and writes one weight, which leaves the second place
to be filled.  U+01E2 is weighed as two and writes a weight for each, which
fills both.  U+06D7 is weighed as four and writes one weight, which leaves
three places.  In every one of them the a-umlaut's weight `13` lands at the
character count of the a-umlaut.

A collation which moves characters has its own list.  The german phone book
collation moves `Ä`, `Ö` and `Ü` to the weights of "ae", "oe" and "ue", so
those three are weighed as two in it and as one in the table it tailors.  The
same test names them, read against the weights of that table.

A third kind of character stands for the one before it and is weighed as a
copy of it.  These are the repeat marks:

| family | repeat marks |
|---|---|
| general legacy | U+0651 arabic shadda and its two presentation forms U+FE7C and U+FE7D, U+3005, U+3031, U+3032, U+309D, U+309E, U+30FD, U+30FE |
| general | U+3005, U+3031, U+3032, U+3033, U+3034, U+3035, U+303B, U+309D, U+309E, U+30A0, U+30FD, U+30FE, U+A015 |

The shadda is a repeat mark in the general legacy family only.  In the general
family all three of its code points write an unprintable weight instead.

The shadda doubles the consonant it follows and the japanese marks mean the
character before them again, so a copy of the weight before is what each one
writes:

| value | entry |
|---|---|
| `k` + shadda | `7f 5c 5c 01 00` |
| `k` + shadda + shadda | `7f 5c 5c 5c 01 00` |
| shadda + `k` | `7f ff ff 5c 01 00` |
| `ب` + `ت` + shadda | `7f 79ac 79ae 79ae 01 08 08 00` |

Two marks give two copies, and where there is no weight to copy the mark writes
`ff ff`.  That marker **stands for no character at all**: it takes no place in
the offsets the diacritic weights carry, it carries no diacritic weight of its
own, and a mark after it has nothing to copy either, so it writes the marker
again.

| value | entry |
|---|---|
| U+309E + `k` + `ä` | `7f ffff 5c 4a 01 02 13 00` |
| shadda + shadda + `k` | `7f ffff ffff 5c 01 00` |
| U+309D + U+309D + U+309D + `k` | `7f ffff ffff ffff 5c 01 00` |

The a-umlaut's weight `13` sits at place 1 in the first row, where the `k` is,
because the mark took no place.

A character weighed as more than one leaves nothing to copy either, so a mark
after one of those writes the marker as well.  The weights after it do not
move, which the control row shows:

| value | entry |
|---|---|
| `k` + `ß` + `m` + `ä` | `7f 5c 6b6b 60 4a 01 02 02 02 02 13 00` |
| `k` + `ß` + shadda + `m` + `ä` | `7f 5c 6b6b ffff 60 4a 01 02 02 02 02 13 00` |

What gets copied is the last weight of a character weighed as one, inline or
unprintable.  After a character which writes no inline weight but an
unprintable one, such as an arabic vowel mark, the copy is the unprintable
midfix `06` and that mark's weight:

| value | entry |
|---|---|
| `ب` + fatha + shadda | `7f 79ac 06a3 01 08 01 01 01 80 0b 06a3 00` |
| `ب` + damma + shadda | `7f 79ac 06a4 01 08 01 01 01 80 0b 06a4 00` |

A diacritic weight is not copied, so `a` with a combining acute and then a
shadda copies the `a`, and an ignored character is passed over, so a shadda
after a tatweel copies the letter before the tatweel.

Some repeat marks carry a secondary weight of their own as well.  The voiced
iteration marks U+3032, U+309E and U+30FE and the ideographic marks U+3005,
U+303B and U+A015 copy the weight before them and add one diacritic weight:

| value | entry |
|---|---|
| `k` + U+309E | `7f 5c 5c 01 02 03 00` |
| `k` + U+309E + U+309E | `7f 5c 5c 5c 01 02 03 03 00` |
| U+309E + `k` | `7f ff ff 5c 01 00` |

A mark with no weight to copy gives no secondary weight either, which the third
row shows.

The prolonged sound marks U+30FC and its halfwidth form U+FF70 take the place
of the kana before them the same way, but write the weight of the vowel that
kana ends on rather than a copy of the whole kana.  The halfwidth and the
fullwidth mark behave alike in both families, and so do the halfwidth and the
fullwidth kana:

| value | entry |
|---|---|
| `コ` | `7f 7f0e 01 01 01 ff 02 80 ff 80 00` |
| `コ` + U+30FC | `7f 7f0e 7f06 01 01 01 ff 9c 02 80 ff 80 00` |
| `コ` + U+30FD | `7f 7f0e 7f0e 01 01 01 ff 98 02 80 ff 80 00` |
| `ピ` + U+30FC | `7f 7f2b 7f03 01 04 01 01 ff 9c 02 80 ff 80 00` |
| `ン` + U+30FC | `7f 7f80 7f80 01 01 01 ff 9c 02 80 ff 80 00` |

`コ` is an o row kana and the mark takes `06`, the weight of `オ`.  `ピ` is an i
row kana and it takes `03`, the weight of `イ`.  The moraic nasal `ン` ends on
no vowel and the mark copies its weight whole.  The iteration mark U+30FD in
the third row copies `0e` whole, which is what tells the two kinds of mark
apart.  The entries for `コ` + U+30FC and `コ` + U+30FD also differ in the byte
`9c` against `98`, which is in the copy run described with the flag codes.
The `ン` row, where the weights cannot differ from a copy, carries `9c` all the
same.

The prolonged sound mark is the only mark which needs a weight of its own this
way.  Everything the other marks add lives in the diacritic weights, which a
copy carries along: `か` and `が` share the inline weight `7f0a` and differ only
in `が`'s diacritic weight `03`, so `か` + U+309E, which reads `かが`, is a copy
of `7f0a` plus the mark's own diacritic weight.  A vowel is not reachable that
way, because `コ` is `0e` and `オ` is `06` and no copy turns one into the
other.

The vowel cannot be read off the character.  The rows of the syllabary are not
contiguous, and a voiced kana, a small kana, a circled kana and a halfwidth
kana all carry the weight of the plain kana they sort as, so each weight has
to be measured.  The vowel follows the weight and not the character.  The
circled katakana show the difference: `㋕` weighs as `ヘ` in the general legacy
family and as `カ` in the general one, and the mark writes the `e` of `ヘ` in
the first and the `a` of `カ` in the second.

The sixteen katakana phonetic extensions U+31F0 to U+31FF, which write the
closing consonant of an ainu syllable, give the mark the vowel their full size
kana gives.  They differ from it only in the tertiary weight `a8` they carry
as small kana:

| value | entry |
|---|---|
| `ㇰ` + U+30FC | `7f 7f0c 7f04 01 03 01 01 a8 ff 9c 02 80 ff 80 00` |
| `ク` + U+30FC | `7f 7f0c 7f04 01 01 01 ff 9c 02 80 ff 80 00` |
| `ㇿ` + U+30FC | `7f 7f46 7f06 01 01 01 a8 ff 9c 02 80 ff 80 00` |
| `ロ` + U+30FC | `7f 7f46 7f06 01 01 01 ff 9c 02 80 ff 80 00` |

The general legacy family ignores all sixteen, so the mark after one of them
has no weight to lengthen and writes `7f ffff 01 00`, the bytes it writes at
the start of a value.

A weight outside the range the kana use gives no vowel, and the mark copies
it.  The squared katakana words U+3300 to U+3357 are that case, which the
general legacy family weighs `7fa0` upward:

| value | entry |
|---|---|
| `㌀` | `7f 7fa8 01 00` |
| `㌀` + U+30FC | `7f 7fa8 7fa8 01 00` |

The general family weighs the two kana digraphs `ゟ` and `ヿ` as two characters
each, which a mark cannot stand for, so the mark after one writes `ffff`, the
weight which stands for no character:

| value | entry |
|---|---|
| `ゟ` | `7f 033e 0343 01 00` |
| `ゟ` + U+30FC | `7f 033e 0343 ffff 01 00` |
| `ヿ` | `7f 030e 031e 01 00` |
| `ヿ` + U+30FC | `7f 030e 031e ffff 01 00` |

With no kana before it the mark acts as a repeat mark.  Alone and at the start
of a value it writes the two bytes which stand for no character, and after a
letter, a digit, a space or an ideograph it writes a copy:

| value | entry |
|---|---|
| U+30FC | `7f ffff 01 00` |
| `k` + U+30FC | `7f 0e36 0e36 01 00` |

The mark leaves the weight of the kana it lengthens for the next mark, and not
the vowel it wrote.  So a mark after a prolonged sound mark or after an
iteration mark reads through to the kana:

| value | entry |
|---|---|
| `コ` + U+30FC + U+30FC | `7f 7f0e 7f06 7f06 01 01 01 ff 9f 02 80 ff 80 00` |
| `コ` + U+30FD + U+30FC | `7f 7f0e 7f0e 7f06 01 01 01 ff 9b 02 80 ff 80 00` |
| `ハ` + U+30FC + U+30FD | `7f 7f2a 7f02 7f2a 01 01 01 ff 9e 02 80 ff 80 00` |
| `ハ` + U+30FC + U+30FD + U+30FC | `7f 7f2a 7f02 7f2a 7f02 01 01 01 ff 9e b0 02 80 ff 80 00` |

A character which writes no weight of its own between the kana and the mark is
read through as well.  A voiced mark written as a separate character only adds
a diacritic weight, so halfwidth and decomposed text give the same key as the
composed word:

| value | entry |
|---|---|
| `ﾃﾞｰﾀ` | `7f 7f1d 7f05 7f1a 01 03 01 01 ff 9c 02 80 ff 80 00` |
| `テ` + U+3099 + `ータ` | `7f 7f1d 7f05 7f1a 01 03 01 01 ff 9c 02 80 ff 80 00` |
| `データ` | `7f 7f1d 7f05 7f1a 01 03 01 01 ff 9c 02 80 ff 80 00` |

An unprintable character is not read through, because it leaves an unprintable
weight for the mark to copy.  The general legacy family weighs the soft hyphen
that way, so `テ` + U+00AD + U+30FC copies `06 83` where the general family,
which ignores the soft hyphen, writes the `e`.

The two families differ over the shadda.  It is a repeat mark in the general
legacy family and an ordinary unprintable character in the general one, which
carries a diacritic weight and no inline weight at all.  The japanese marks are
repeat marks in both, except U+3033, U+3034, U+3035, U+303B, U+30A0 and
U+A015, which the general legacy family ignores entirely.

No other combining mark measured behaves like this: hebrew, thai, greek,
cyrillic and the combining diacritical marks all write no inline weight in
either family.

A combining mark can still carry a weight modifier, which is added to the last
secondary weight written rather than written on its own.  When nothing has been
written yet the modifier is written by itself, and the modifiers close to `00`
are invisible after a character which has no secondary weight, because the sum
falls in the `00` to `02` range the tail is trimmed of.  U+1DCA is `ff` and
U+1DFF is `fe` in the general family:

| value | entry |
|---|---|
| U+1DCA + `k` | `7f 0e36 01 ff 00` |
| `k` + U+1DCA | `7f 0e36 01 00` |
| `k` + U+1DFF + U+1DFF | `7f 0e36 01 fe 00` |
| U+1DCA + `k` + `ä` | `7f 0e36 0e02 01 ff 02 13 00` |
| U+1DCA + U+1DCA + `k` + `ä` | `7f 0e36 0e02 01 fe 02 13 00` |

A modifier written by itself takes a place of the secondary weight list, which
the a-umlaut's `13` shows: it lands after the modifier and the placeholder for
the `k`.  The character offset does not count it, so a following unprintable
weight carries the same offset as it would without the modifier.  A second
modifier adds to the first one's byte rather than taking a place of its own.

The hungarian doubled digraphs behave differently again.  `ccs` is the
repeated first letter and then the digraph `cs`, it writes the weight of `cs`
twice, and it reads the case rule over the digraph alone, so `cCSa` is weighed
as a unit where `cHa` is not.

Croatian differs between the two weight table families over which form is the
unit.  In the general legacy family the two characters `dž` take the weight of
the single character `ǆ` U+01C6, and in the general family the two characters
take a weight of their own and U+01C6 decomposes into `d` and `ž`.

French is unusual in changing no weight.  It orders the diacritics
of a value from the end rather than the start, which sorts `cote` before `côte`
before `coté` before `côté`.  Build the list with one entry per character which
wrote index bytes, reverse it, then cut the trailing placeholders.  An
apostrophe or a hyphen takes no place in the list, and the record it writes
elsewhere in the key keeps the position it has in the general legacy order.

The leaf page entries store the key column and the 3 byte page and 1 byte row
number.

The value of the index root page in the index definition may be an index page
(type 0x03), an index leaf page (type 0x04) if there is only one index page, 
or (in the case of tables small enough to fit on one page) a data page 
(type 0x01).

So to search the index, you need to convert your value into the alphabetic 
character set, compare against each index entry, and on successful comparison
follow the page and row number to the data.  Because text data is managled 
during this conversion there is no 'covered querys' possible on text columns.

To conserve on frequent index updates, Jet also does something special when
creating new leaf pages at the end of a primary key index (or other index
where new values are generally added to the end of the index).  The tail leaf
page pointer of the last leaf node points to the new leaf page but the index
tree is not otherwise updated.  Since index entries in type 0x03 index pages
point to the last entry in the page, adding a new entry to the end of a large
index would cause updates all the way up the index tree.  Instead, the tail
page can be updated in isolation until it is full, and then moved into the
index proper.  In src/libmdb/index.c, the last leaf read is stored, once the
index search has been exhausted by the normal search routine, it enters a
"clean up mode" and reads the next leaf page pointer until it's null.
 
The Catalog
-----------

`MSysObjects` names every object in the database.  `Id` is the page number of
the object's TDEF page for a table, `ParentId` groups the rows under the
system objects `Tables`, `Databases` and `Relationships`, and `Name` is unique
within a parent.

The `Type` column says what the object is:

```
1 table
4 linked ODBC table
5 query
6 linked table
```

A linked table has no TDEF page of its own.  `Database` and `ForeignName` name
the file and the table it stands for, and a linked ODBC table carries a
`Connect` string instead.

The `Flags` column marks a system object with either 0x80000000 or 0x00000002,
and a hidden object with 0x08.

Complex Columns
---------------

A complex column (multivalued field, attachment, version history) is stored as
a LONG autonumber whose value is the key of the rows which hold the real data.
The column type is 0x12 and its 4 byte misc field holds a complex id.  The
field exists only in the Access 2007 format and later, so a Jet 3 or Jet 4
file has none.

`MSysComplexColumns` maps that complex id to the supporting tables:
`ConceptualTableID` is the TDEF page of the table which declares the column,
`ColumnName` its name, `FlatTableID` the table which holds the values, and
`ComplexTypeObjectID` a type table.

The name of the type table says which kind of complex column it is, and Access
reserves each name:

```
MSysComplexType_Attachment    attachment
MSysComplexType_<other>       multivalued field
MSysComplexTypeVH_<guid>      version history
```

The type tables of the first two kinds are shared, one per database, and
Access creates them whether or not a column uses them.  A version history type
table is created per column, which is why its name carries a guid.  It holds a
memo column named after the memo column whose versions it keeps, and a date
column, in either order.

**A version history column and its date column have fixed names.**  The column
in the table which declares it is always

```
VersionHistory_F5F8918F-0A3F-4DA9-AE71-184EE5012880
```

and the date column of its type table is always

```
Modified_F9B5E312-4155-4c59-9AAE-391C1B295827
```

Both hold across every database seen.  Only the type table's own guid varies
per column.  A fixed column name means a table can keep the version history of
at most one memo column.

The flat table is named `f_<32 hex digits>_<column>`, cut to the 64 characters a
table name may be, which the long name of a version history column needs.  Its
indexes are `MSysComplexPKIndex` on the primary key, `_<column>` on the foreign
key, and for a multivalued field or an attachment `IdxFKPrimaryScalar` on the
foreign key plus a value column, `Value` for a multivalued field and `FileName`
for an attachment.  Access also writes a unique `<column>_<32 hex digits>` index
on the complex column itself, cut to 64 characters the same way.  Only the
foreign key index is needed to read the values, and Access opens a file which
has neither of the other two.

Each row of the flat table carries the complex id of the row it belongs to in
a foreign key column marked by ext flag 0x08.  The flat table also holds a LONG
autonumber primary key, which is the id of one value, and the columns of the
type table by the same names.

The complex id is a per row autonumber of the table which declares the column,
and every complex column of one row shares it.  A row written before a complex
column was added holds none, and Access shows and edits such a row but fails to
save the edit.

`MSysObjects.Flags` says which part an object plays: 0x00040000 on the table
which declares the column, without which Access never looks for the values, and
0x000A0000 on the flat table, without which it will not open that table at all.
A type table is 0x00030000.  Access also sets 0x80000000 on both supporting
kinds, which only hides them from the navigation pane.

Access reads the row source of a multivalued field from the `RowSourceType` and
`RowSource` properties of the flat table's value column, not of the column
itself, and offers no way to add a value unless the row source names at least
one.  An attachment column carries no properties.  A version history column
carries none either, and instead the memo column it shadows carries
`AppendOnly`, as does the table.

Properties
----------

Design View table definitions are stored in LvProp column of MSysObjects as OLE
fields. They contain default values, description, format, required ...

They start with a 32 bits header: 'KKD\0' in Jet3 and 'MR2\0' in Jet 4.

Next come chunks. Each chunk starts with:

- 32 bits length value (this includes the length)
- 16 bits chunk type.  0x0080 contains the names shared by all the other
	chunks.  The rest hold values, and the type says which kind of object
	the values belong to: 0x0000 the object itself, e.g. the table, 0x0001 a
	named column of it, and 0x0002 a named index of it

```
Name chunk blocks (0x0080) simply contain occurences of:
16 bit name length
name
For instance: 
0x0d 0x00 and 'AccessVersion' (AccessVersion is 13 bytes, 0x0d 0x00 intel order)
```

Value chunk blocks (0x0000, 0x0001 and 0x0002) contain a header:
- 32 bits length value (this includes the length)
- 16 bits name length
- name  (0x0000 chunk blocks are not usually named, 0x0001 chunk blocks have the
      column name and 0x0002 chunk blocks the index name to which the
      properties belong)

Next comes one of more chunks of data:
- 16 bit length value    (this includes the length)
- 8 bit flags (see below)
- 8 bit type
- 16 bit name (index in the name array of above chunk 0x0080)
- 16 bit value length field (non-inclusive)
  value (07.53 for the AccessVersion example above)

The flag byte is a bit field, not a boolean.  Two of the bits are known:

- 0x01 is the DDL argument of the DAO CreateProperty method.  A user cannot
	change or delete a DDL property without dbSecWriteDef permission.
- 0x80 tells ms access to store the value without running the handler which
	the property name is bound to, so that writing the property records a
	state instead of bringing it about.  Ms access sets this on its own
	account, for instance when a replication conversion stamps `Replicable`
	on each table after the conversion has already happened, and no DAO call
	produces it.

The whole byte has to be written back unchanged.

See ``props.c``` for an example.

Name Maps
---------

A name map is the data behind the Name AutoCorrect option.  It records the
tables and columns an object depends on, and ties each one to the guid in the
`GUID` property of that table or column.  When a table or column is renamed,
Access uses the guid to find the old name and changes the places which use it.

Only Jet 4 and later files have name maps.  They are stored as OLE properties
in the default property block of the MSysObjects row of the object:

- a table has a `NameMap` property.  It has a table record for the table
  itself, a column record for each column and a reference record for each
  name that an expression in the table (a calculated column, a validation
  rule) uses
- a query has a `DOL` property in the same format.  It has a table record for
  each table or query the query reads and a column record for each column it
  uses.  A query which tracks no names (union, pass-through, data definition)
  has the magic value alone
- a linked table also has one

The database properties `Track Name AutoCorrect Info` and `Perform Name
AutoCorrect` (LONG) turn the option on and off.  If a property is missing,
Access takes the option as on.

```
NameMap := u32 magic 0x550ECC0A [u32 reserved, record*]
record  := guid[16], u32 type, payload[16], name, u32 zero
```

The name is little-endian UCS-2 with a null terminator, whatever the charset
of the database.  The trailing zero is left off the last record.  There is no
record count, so a reader stops at the end of the data.

```
type  record     payload
0     table      8 byte date, then 8 bytes of writer state
6     reference  guid of the table
7     column     guid of the table
12    end        tag of the Access build which wrote the map, older builds
                 did not write this record
```

The guid of a reference or an end record is zero.

The date in a table record is the DateUpdate of the table in MSysObjects at
the time Access wrote the name map.  Access rewrites the name map of a table
when a design save changes a column or an index.  A table property change, a
rename in the Navigation Pane, data entry and a compact do not rewrite it.
When a column is renamed, Access changes the name in its record and keeps its
guid.  A table renamed in the Navigation Pane keeps its old name in its table
record until the next design save.

Access repairs a query when it next opens it, not when the table changes.  It
finds each record of the `DOL` by guid, and if the object now has a different
name, it changes the sql and the `DOL`.  A table record for a query which the
query reads carries the guid of that source query.  The column records under
it carry the guids of the table columns the source query reads.  So a query
name map must not be changed without its sql, because Access could then not
find the old names to repair.

Name maps often disagree with their tables, even in files which only Access
wrote.  It is more reliable to find a column record by name than by guid.


Text Data Type
--------------

In Jet3, text is encoded in the code page of the machine on which the database
was created, and the file records which one: the 2 bytes at 0x3C of the
Database Definition Page.  It reads 1252 in every Jet 3 file examined so far,
which is what a U.S. English system gives.  Mdbtools defaults to CP1252 and
takes an override from the environment variable MDB_JET3_CHARSET.  Note that
windows-1252 leaves 0x81, 0x8D, 0x8F, 0x90 and 0x9D undefined, so a file which
stores one of those bytes has text no unicode charset round trips.

In Jet4, the encoding can be either little-endian UCS-2, or a special
compressed form of it.  This compressed format begins with 0xff 0xfe.
The string then starts in compressed mode, where characters with 0x00 for the
most-significant byte do not encode it.  In the compressed format, a 0x00 byte
signals a change from compressed mode to uncompressed mode, or from
uncompressed mode back to compressed mode.  The string may end in either mode.
Note that a string containing any character 0x##00 (UCS-2) will not be
compressed.  Also, the string will only be compressed if it really does make
the string shorter as compared to uncompressed UCS-2.

Programs that use mdbtools libraries will receive strings encoded in UTF-8 by
default.  This default can by overridden by setting the environment variable
MDBICONV to the desired encoding.
