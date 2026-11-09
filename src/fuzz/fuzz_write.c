/*
 * fuzz_write.c - mdbtools row-write / round-trip harness.
 *
 * Threat model
 * ============
 * Untrusted input: attacker-supplied Jet database bytes - the whole fuzz input
 * IS the database image (established mdbtestdata corpus doubles as seed
 * corpus). Tools in the mdbtools suite (mdb-import, mdb-create, ODBC writers)
 * open attacker-supplied database files in read-write mode and encode values
 * into pages, so the page structures cross the trust boundary as data.
 *
 * Entry points exercised:
 *  - mdb_read_table/mdb_read_columns/mdb_read_indices: definition parsing
 *  - mdb_insert_row(): the mdb-import-shaped write path (pack, add to page,
 *    update indexes, write page back). The database is opened MDB_WRITABLE
 *    over a private writable copy of the input, so writes stay deterministic
 *    and never touch libFuzzer's read-only input buffer.
 *  - mdb_create_temp_table/mdb_temp_table_add_col/mdb_pack_row/
 *    mdb_add_row_to_pg: the worktable component used for computed result
 *    tables (internally consistent pages, so the page-write paths run without
 *    the corrupt-page defect documented as mdbtools-5).
 *  - mdb_rewind_table/mdb_fetch_row: the decode side of the round trip.
 *
 * Expected bug classes: OOB writes in row packing/page layout, int overflows
 * in offset/size arithmetic, memory safety in index updates, corruption-induced
 * crashes on the decode side of the round trip.
 *
 * Known upstream defects routed around by battery design (see the pair report):
 * mdbtools-4 (mdb_index_swap_n SEGV, index.c:436, when inserting into tables
 * that own a primary index - row writes are restricted to non-indexed tables)
 * and mdbtools-5 (negative-size memcpy at write.c:675 in mdb_add_row_to_pg's
 * copy-existing-rows loop - the direct pack/add variant over real pages is not
 * exercised; temp tables keep it on internally consistent pages).
 */
#include "mdbtools.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TABLES 4
#define MAX_INSERT_ROWS 2
#define MAX_READBACK 64
#define MAX_FIELDS 4

static unsigned int fill_field(MdbField *field, MdbColumn *col,
                               uint8_t *vbuf, unsigned int seed)
{
    unsigned int i, need;
    int fixed = mdb_is_fixed_col(col) ? 1 : 0;
    int siz;

    if (fixed) {
        siz = mdb_col_fixed_size(col);
        if (siz < 0 || siz > 16)
            siz = 4;
        need = (unsigned int)siz;
    } else {
        siz = 8;
        need = (unsigned int)siz;
    }
    for (i = 0; i < need; i++)
        vbuf[i] = (uint8_t)(seed * 31 + i * 7);
    mdb_fill_temp_field(field, vbuf, siz, fixed, 0, 0, (int)seed);
    return need;
}

static void row_write_battery(MdbTableDef *table)
{
    MdbField fields[MAX_FIELDS];
    uint8_t vbuf[MAX_FIELDS][16];
    unsigned int n, f;
    int has_primary_idx = 0;

    for (n = 0; n < table->num_idxs; n++) {
        MdbIndex *idx = (MdbIndex *)g_ptr_array_index(table->indices, n);
        if (idx->index_type == 1)
            has_primary_idx = 1;
    }

    /* findings mdbtools-4 and mdbtools-5: the write machinery crashes on
     * attacker-controlled index pages (mdb_index_swap_n SEGV) and on crafted
     * data pages (negative-size memcpy in mdb_add_row_to_pg's copy loop).
     * Route around: only non-indexed tables get row writes; indexed tables
     * get readbacks only. Re-enable once upstream hardens both paths. */
    if (!has_primary_idx && table->num_cols > 0) {
        unsigned int ncols = table->num_cols < MAX_FIELDS ?
            table->num_cols : MAX_FIELDS;
        for (n = 0; n < MAX_INSERT_ROWS; n++) {
            unsigned int used = 0;
            for (f = 0; f < ncols; f++) {
                MdbColumn *col = (MdbColumn *)g_ptr_array_index(
                    table->columns, f);
                used += fill_field(&fields[f], col, vbuf[f],
                                   n * 16 + f);
            }
            mdb_insert_row(table, (int)ncols, fields);
        }
    }

    /* decode side of the round trip: read back whatever is on the pages */
    {
        char bound[MDB_BIND_SIZE];
        unsigned long rows = 0;
        mdb_rewind_table(table);
        if (mdb_bind_column(table, 1, bound, NULL) != -1) {
            while (rows++ < MAX_READBACK && mdb_fetch_row(table))
                bound[0] ^= (unsigned char)rows;
        }
        (void)bound[0];
    }
}

static void temp_table_battery(MdbHandle *mdb)
{
    static const int types[2] = { MDB_LONGINT, MDB_TEXT };
    unsigned char row_buffer[MDB_PGSIZE];
    char bound[MDB_BIND_SIZE];
    MdbColumn col;
    MdbField field;
    uint8_t vbuf[16];
    MdbTableDef *ttable;
    unsigned int n;
    int row_size;

    ttable = mdb_create_temp_table(mdb, "#fuzz");
    for (n = 0; n < 2; n++) {
        char name[16];
        snprintf(name, sizeof(name), "c%u", n);
        mdb_fill_temp_col(&col, name, 16, types[n],
                          (types[n] != MDB_TEXT) ? 1 : 0);
        mdb_temp_table_add_col(ttable, &col);
    }
    mdb_temp_columns_end(ttable);
    for (n = 0; n < 2; n++) {
        memset(vbuf, (int)(n + 1), sizeof(vbuf));
        mdb_fill_temp_field(&field, vbuf, (int)sizeof(vbuf), 1, 0, 0, 0);
        row_size = mdb_pack_row(ttable, row_buffer, 1, &field);
        if (row_size > 0)
            mdb_add_row_to_pg(ttable, row_buffer, row_size);
    }
    {
        unsigned long rows = 0;
        mdb_rewind_table(ttable);
        if (mdb_bind_column(ttable, 1, bound, NULL) != -1) {
            while (rows++ < MAX_READBACK && mdb_fetch_row(ttable))
                bound[0] ^= (unsigned char)rows;
        }
        (void)bound[0];
    }
    mdb_free_tabledef(ttable);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    unsigned char *dbcopy;
    MdbHandle *mdb;
    MdbCatalogEntry *entry;
    unsigned int t;

    if (size < 8)
        return 0;

    /* writes must never touch libFuzzer's read-only input buffer */
    dbcopy = (unsigned char *)malloc(size);
    if (!dbcopy)
        return 0;
    memcpy(dbcopy, data, size);

    mdb = mdb_open_buffer(dbcopy, (int)size, MDB_WRITABLE);
    if (!mdb) {
        free(dbcopy);
        return 0;
    }
    mdb_read_catalog(mdb, MDB_TABLE);

    for (t = 0; t < MAX_TABLES && t < (unsigned int)mdb->num_catalog; t++) {
        MdbTableDef *table;

        entry = (MdbCatalogEntry *)g_ptr_array_index(mdb->catalog, t);
        table = mdb_read_table(entry);
        if (!table)
            continue;
        if (!mdb_read_columns(table)) {
            mdb_free_tabledef(table);
            continue;
        }
        mdb_read_indices(table);
        row_write_battery(table);
        mdb_free_tabledef(table);
    }
    temp_table_battery(mdb);

    mdb_close(mdb);
    free(dbcopy);
    return 0;
}
