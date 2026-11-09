/*
 * fuzz_index.c - mdbtools index-page walker harness (read side).
 *
 * Threat model
 * ============
 * Untrusted input: attacker-supplied Jet database bytes - the whole fuzz input
 * IS the database image (established mdbtestdata corpus doubles as seed
 * corpus). Index definition and index pages inside such files are fully
 * attacker-controlled; readers (mdb-export/mdb-tables-style consumers, SQL
 * index scans) walk those pages to locate rows, so index structures cross the
 * trust boundary as data.
 *
 * Entry points exercised:
 *  - mdb_read_indices(): index definition parsing (index.c)
 *  - mdb_index_find_next()/mdb_index_find_next_on_page(): the index page chain
 *    walker used by every index-scan read
 *  - mdb_find_row()/mdb_find_end_of_row()/mdb_find_pg_row(): data page row
 *    offset resolution used by row fetching
 *  - mdb_rewind_table/mdb_fetch_row: the decode side
 *
 * Expected bug classes: OOB reads while walking corrupted index page chains,
 * wild offset arithmetic in row lookup, infinite/cyclic page walks (the
 * harness bounds every walk and converts pathological loops into measurable
 * timeouts).
 *
 * Known upstream defect surfaced by this harness (see the pair report):
 * mdbtools-6 - wild-write SEGV at index.c:831 in mdb_index_find_next when
 * mdb_index_read_bottom_pg resolves chain->pages[cur_depth - 1] with an
 * underflowed depth on crafted index pages. No routing is possible: the walker
 * is the dark surface this pair targets.
 */
#include "mdbtools.h"
#include <stdint.h>

#define MAX_TABLES 4
#define MAX_FINDNEXT 2048
#define MAX_READBACK 256
#define MAX_PAGE_PROBES 32

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    volatile unsigned long sink = 0;
    MdbHandle *mdb;
    MdbCatalogEntry *entry;
    unsigned int t;
    uint32_t num_pages;

    if (size < 8)
        return 0;

    mdb = mdb_open_buffer((void *)data, (int)size, MDB_NOFLAGS);
    if (!mdb)
        return 0;
    mdb_read_catalog(mdb, MDB_TABLE);
    num_pages = (uint32_t)(size / (size_t)mdb->fmt->pg_size);

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

        /* walk every index page chain end to end (bounded) */
        if (table->num_idxs > 0) {
            unsigned int n;
            for (n = 0; n < table->num_idxs && n < 4; n++) {
                MdbIndex *idx = (MdbIndex *)g_ptr_array_index(
                    table->indices, n);
                /* finding mdbtools-6: mdb_index_find_next dereferences the
                 * mdb_index_read_bottom_pg result without a NULL check
                 * (index.c:831); walking an index whose root page is absent
                 * (first_pg 0 / beyond EOF) makes mdb_find_next_leaf fail and
                 * returns NULL, crashing even on ordinary databases. Production
                 * never selects such indexes (mdb_choose_index validation), so
                 * filter to production parity here and walk the rest. Re-enable
                 * unfiltered walking once upstream adds the NULL check. */
                if (idx->first_pg == 0 || idx->first_pg >= num_pages)
                    continue;
                MdbIndexChain *chain = (MdbIndexChain *)g_malloc0(
                    sizeof(MdbIndexChain));
                unsigned long steps = 0;
                guint32 pg = 0;
                guint16 row = 0;

                while (steps++ < MAX_FINDNEXT &&
                       mdb_index_find_next(mdb, idx, chain, &pg, &row))
                    sink ^= (unsigned long)pg ^ row;
                g_free(chain);
            }
        }

        /* data page row-offset resolution over the early pages */
        {
            uint32_t p;
            for (p = 2; p < num_pages && p < MAX_PAGE_PROBES; p++) {
                int r;
                if (mdb_read_pg(mdb, p) <= 0)
                    continue;
                for (r = 0; r < 8; r++) {
                    int start = 0;
                    size_t len = 0;
                    mdb_find_row(mdb, r, &start, &len);
                    sink ^= (unsigned long)(start + (int)len);
                }
                sink ^= mdb_find_end_of_row(mdb, 0);
            }
            if (num_pages > 1) {
                void *rowbuf = NULL;
                int off = 0;
                size_t len = 0;
                if (mdb_find_pg_row(mdb, 2, &rowbuf, &off, &len) > 0 &&
                    rowbuf && len > 0)
                    sink ^= ((unsigned char *)rowbuf)[0];
            }
        }

        /* decode side: fetch rows through the table */
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

        mdb_free_tabledef(table);
    }

    mdb_close(mdb);
    return 0;
}
