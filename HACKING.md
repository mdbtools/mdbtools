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

Offset 0x14 contains the Jet version of this database:

- 0x00 for 3
- 0x01 for 4
- 0x02 for 5
- 0x03 for Access 2010
- 0x04 for Access 2013
- 0x05 for Access 2016
- 0x06 for Access 2019

This is used by the `mdb-ver` utility to determine the Jet version.

The 20 bytes (Jet3) or 40 bytes (Jet4) starting at 0x42 are the database
password.  In Jet4, there is an additional mask applied to this password
derived from the database creation date (also stored on this page as 8 bytes
starting at offset 0x72).

The 4 bytes at 0x3e on the Database Definition Page are the database key.

The 2 bytes at 0x3C are the default database code page (useless in Jet4?).

The 2 bytes at 0x3A (Jet3) or 4 bytes at 0x6E (Jet4) are the default text
collating sort order.

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
| 0x01 | 1 byte  | unknown    |                                            |
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

- In Jet4, an additional four-byte field was added after tdef_pg.  Its purpose
  is currently unknown.
- Offsets that have 0x40 in the high order byte point to a location within the
  page where a Data Pointer (4 bytes) to another data page (also known as an
  overflow page) is stored.  Called 'lookupflag' in source code.
- Offsets that have 0x80 in the high order byte are deleted rows.  Called
  'delflag' in source code.


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
| 0x00 | 4 bytes | unknown     |                                          |
+------+---------+-------------+------------------------------------------+
```

Values for the bitmask:

- 0x80 = the memo is in a string at the end of this header (memo_len bytes)
- 0x40 = the memo is in a unique LVAL page in a record type 1
- 0x00 = the memo is in n LVAL pages in a record type 2

If the memo is in a LVAL page, we use row_id of lval_dp to find the row.

```c
offset_start of memo = (int16*) LVAL_page[offset_num_rows + (row_id * 2) + 2]
if (row_id = 0)
     offset_stop of memo = 2048(jet3) or 4096(jet4)
else
     offset_stop of memo = (int16*) LVAL_page[offset_num_row + (row_id * 2)]
```

The length (partial if type 2) for the memo is:
memo_page_len = offset_stop - offset_start

Update: The bitmask can't be an entire byte long.
OLE fields can hold up to 1gig. That requires at least 30 bits, leaving only 2
bits for flags. Maybe sometimes 0xC0000000 is ignored?
See http://office.microsoft.com/en-us/access-help/access-2007-specifications-HA010030739.aspx
Number of characters in a Memo field: 65,535 when entering data through the
user interface; 2 gigabytes of character storage when entering data
programmatically. That would mean 31 bits for length.

Note: if a memo field is marked for compression, only at value which is at
most 1024 characters when uncompressed can be compressed.  fields longer than
that _must_ be stored uncompressed.


LVAL (Long Value) Pages
-----------------------

The header of a LVAL page is just like that of a regular data page,
except that in place of the tdef_pg is the word 'LVAL'.

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
| ???? | 2 bytes | num_var_cols| Number of variable columns in table      |
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
| 0x00 | 4 bytes | ???         |                                          |
| ???? | 4 bytes | num_idx_rows| (not sure)                               |
+-------------------------------------------------------------------------+
| Iterate for the number of num_cols (18 bytes per column)                |
+-------------------------------------------------------------------------+
| ???? | 1 byte  | col_type    | Column Type (see table below)            |
| ???? | 2 bytes | col_num     | Column Number (includes deleted columns) |
| ???? | 2 bytes | offset_V    | Offset for variable length columns       |
| ???? | 2 bytes | col_num     | Column Number                            |
| ???? | 2 bytes | sort_order  | textual column sort order(0x409=General) |
| ???? | 2 bytes | misc        | prec/scale (1 byte each), or code page   |
|      |         |             | for textual columns (0x4E4=cp1252)       |
| ???? | 2 bytes | ???         |                                          |
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
| 0x01 | 1 byte  | cascade_dels| flag indicating if deletes are cascaded  |
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
| ???? | 4 bytes | unknown     | unknown                                  |
| ???? | 4 bytes | num_rows    | Number of records in this table          |
| 0x00 | 4 bytes | autonumber  | value for the next value of the          |
|      |         |             | autonumber column, if any. 0 otherwise   |
| 0x01 | 1 byte  | autonum_flag| 0x01 makes autonumbers work in access    |
| ???? | 3 bytes | unknown     | unknown                                  |
| 0x00 | 4 bytes | ct_autonum  | autonumber value for complex type column(s) |
|      |         |             | (shared across all columns in the table) |
| ???? | 8 bytes | unknown     | unknown                                  |
| 0x4e | 1 byte  | table_type  | 0x4e: user table, 0x53: system table     |
| ???? | 2 bytes | max_cols    | Max columns a row will have (deletions)  |
| ???? | 2 bytes | num_var_cols| Number of variable columns in table      |
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
| 0x00 | 4 bytes | ???         |                                          |
| ???? | 4 bytes | num_idx_rows| (not sure)                               |
| 0x00 | 4 bytes | ???         |                                          |
+-------------------------------------------------------------------------+
| Iterate for the number of num_cols (25 bytes per column)                |
+-------------------------------------------------------------------------+
| ???? | 1 byte  | col_type    | Column Type (see table below)            |
| ???? | 4 bytes | unknown     | matches first unknown definition block   |
| ???? | 2 bytes | col_num     | Column Number (includes deleted columns) |
| ???? | 2 bytes | offset_V    | Offset for variable length columns       |
| ???? | 2 bytes | col_num     | Column Number                            |
| ???? | 2 bytes | misc        | prec/scale (1 byte each), or sort order  |
|      |         |             | for textual columns(0x409=General)       |
|      |         |             | or "complexid" for complex columns (4bytes)|
| ???? | 2 bytes | misc_ext    | text sort order version num is 2nd byte  |
| ???? | 1 byte  | bitmask     | See column flags below                   |
| ???? | 1 byte  | misc_flags  | 0x01 for compressed unicode              |
| 0000 | 4 bytes | ???         |                                          |
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
| ???? | 4 bytes | ???         |                                          |
+-------------------------------------------------------------------------+
| Iterate 10 times for 10 possible columns (10*3 = 30 bytes)              |
+-------------------------------------------------------------------------+
| ???? | 2 bytes | col_num     | number of a column (0xFFFF= none)        |
| ???? | 1 byte  | col_order   | 0x01 =  ascendency order                 |
+-------------------------------------------------------------------------+
| ???? | 4 bytes | used_pages  | Points to usage bitmap for index         |
| ???? | 4 bytes | first_dp    | Data pointer of the index page           |
| ???? | 1 byte  | flags       | See flags table for indexes              |
| ???? | 9 bytes | unknown     |                                          |
+-------------------------------------------------------------------------+
| Iterate for the number of num_idx (28 bytes)                            |
+-------------------------------------------------------------------------+
| ???? | 4 bytes | unknown     | matches first unknown definition block   |
| ???? | 4 bytes | index_num   | Number of the index                      |
|      |         |             |(warn: not always in the sequential order)|
| ???? | 4 bytes | index_num2  | Index into index cols list               |
| 0x00 | 1 byte  | rel_tbl_type| type of the other table in this fk       |
|      |         |             | (same values as index_type)              |
| 0xFF | 4 bytes | rel_idx_num | index number of other index in fk        |
|      |         |             | (or -1 if this index is not a fk)        |
| 0x00 | 4 bytes | rel_tbl_page| page number of other table in fk         |
| 0x01 | 1 byte  | cascade_ups | flag indicating if updates are cascaded  |
| 0x01 | 1 byte  | cascade_dels| flag indicating if deletes are cascaded  |
| ???? | 1 byte  | index_type  | 0x01 if index is primary, 0x02 if foreign|
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

Columns flags (not complete):

- 0x01: fixed length column
- 0x02: can be null (possibly related to joins?)
- 0x04: is auto long
- 0x10: replication related field (or hidden?). These columns start with "s_" or
      "Gen_" (the "Gen_" fields are for memo fields)
- 0x40: is auto guid
- 0x80: hyperlink. Syntax is "Link Title#http://example.com/somepage.html#" or
      "#PAGE.HTM#"

In Access 2007 and Access 2010, "Complex Columns" (multivalued fields, version
history, attachments) always have the flag byte set to exactly 0x07.

Index flags (not complete):
- 0x01 Unique
- 0x02 IgnoreNuls
- 0x08 Required

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
    UNKNOWN_0D      = 0x0D
    UNKNOWN_0E      = 0x0E
    REPID           = 0x0F /* GUID */
    NUMERIC         = 0x10 /* Scaled decimal  (17 bytes) */

```

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
| ???? | 2 bytes | pref_len    | Length of the shared entry prefix        |
+-------------------------------------------------------------------------+
```

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
 
Properties
----------

Design View table definitions are stored in LvProp column of MSysObjects as OLE
fields. They contain default values, description, format, required ...

They start with a 32 bits header: 'KKD\0' in Jet3 and 'MR2\0' in Jet 4.

Next come chunks. Each chunk starts with:

- 32 bits length value (this includes the length)
- 16 bits chunk type (0x0080 contains the names, 0x0000 and 0x0001 contain
	the values.  0x0000 seems to contain information about the "main" object,
	e.g. the table, and 0x0001 seems to contain information about other
	objects, e.g. the table columns)

```
Name chunk blocks (0x0080) simply contain occurences of:
16 bit name length
name
For instance: 
0x0d 0x00 and 'AccessVersion' (AccessVersion is 13 bytes, 0x0d 0x00 intel order)
```

Value chunk blocks (0x0000 and 0x0001) contain a header:
- 32 bits length value (this includes the length)
- 16 bits name length
- name  (0x0000 chunk blocks are not usually named, 0x0001 chunk blocks have the
      column name to which the properties belong)

Next comes one of more chunks of data:
- 16 bit length value    (this includes the length)
- 8 bit ddl flag
- 8 bit type
- 16 bit name (index in the name array of above chunk 0x0080)
- 16 bit value length field (non-inclusive)
  value (07.53 for the AccessVersion example above)

See ``props.c``` for an example.


Text Data Type
--------------

In Jet3, the encoding of text depends on the machine on which it was created.
So for databases created on U.S. English systems, it can be expected that text
is encoded in CP1252.  This is the default used by mdbtools.  If you know that
another encoding has been used, you can override the default by setting the
environment variable MDB_JET3_CHARSET.  To find out what encodings will work on
your system, run 'iconv -l'.

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
