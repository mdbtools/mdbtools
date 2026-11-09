/*
 * fuzz_sql.c - mdbtools SQL engine + search-argument harness.
 *
 * Threat model
 * ============
 * Untrusted input: attacker-supplied Jet database bytes - the whole fuzz input
 * IS the database image (same input model as the incumbent fuzz_mdb, so the
 * established mdbtestdata corpus doubles as this target's seed corpus). The
 * harness constructs its SQL statements from the database's own catalog, so
 * every statement references tables that exist in the fuzzed file and the
 * mutation pressure goes into the structures the SQL engine then walks.
 *
 * Entry points exercised:
 *  - mdb_open_buffer(): page/file structure parse (jet3/jet4/accdb detection)
 *  - mdb_read_catalog(): catalog and kkd properties parsing
 *  - mdb_sql_run_query(): lexer/parser, statement execution
 *  - mdb_sql_fetch_row(): value decode + binding over index/table scans
 *  - mdb_add_sarg_by_name()/mdb_fetch_row(): direct sarg sweep over all
 *    comparison ops (EQUAL/NEQ/GT/LT/GTEQ/LTEQ/LIKE/ILIKE) - LIKE/ILIKE are
 *    reachable only through this API in this release (the SQL grammar has no
 *    LIKE token), so the sweep covers sargs.c and like.c that query text cannot
 *    reach.
 *
 * Expected bug classes: memory safety in parser/lexer/sarg evaluation, OOB
 * reads in value decode/binding, leaks across mdb_sql_reset() cycles.
 *
 * Known upstream defects routed around by battery design (see the pair report):
 * the battery constructs no DESCRIBE statements (mdbtools-1, tabledef leak)
 * and no WHERE clauses (mdbtools-2, sarg-tree leak) - both were confirmed with
 * the query-text-driven predecessor of this harness. mdbtools-7 (lexer
 * NAME-token leaks) still accumulates with any parsed statement and is
 * documented for upstream.
 */
#include "mdbtools.h"
#include "mdbsql.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_BATTERY_TABLES 6
#define MAX_FETCH_ROWS 1000
#define MAX_SARG_ROWS 2000
#define MAX_CATALOG 16

/* run one statement and drain its rows (bounded) */
static void run_statement(MdbSQL *sql, const char *stmt)
{
    unsigned long rows = 0;
    unsigned int i;

    mdb_sql_run_query(sql, stmt);
    if (!mdb_sql_has_error(sql) && sql->cur_table) {
        while (rows++ < MAX_FETCH_ROWS &&
               mdb_sql_fetch_row(sql, sql->cur_table)) {
            for (i = 0; i < (uint32_t)sql->num_columns; i++) {
                char *v;
                if (i >= sql->bound_values->len)
                    break;
                v = (char *)g_ptr_array_index(sql->bound_values, i);
                if (v)
                    v[0] ^= (char)rows;
            }
        }
    }
    mdb_sql_reset(sql);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static const int ops[] = {
        MDB_EQUAL, MDB_NEQ, MDB_GT, MDB_LT,
        MDB_GTEQ, MDB_LTEQ, MDB_LIKE, MDB_ILIKE
    };
    char stmt[512];
    char table_names[MAX_BATTERY_TABLES][MDB_MAX_OBJ_NAME + 1];
    unsigned int n_tables = 0;
    volatile unsigned long sink = 0;
    MdbSQL *sql;
    MdbHandle *mdb;
    MdbCatalogEntry *entry;
    unsigned int i;
    int n;

    if (size < 8)
        return 0;

    sql = mdb_sql_init();
    mdb = mdb_open_buffer((void *)data, (int)size, MDB_NOFLAGS);
    sql->mdb = mdb;
    if (!mdb) {
        mdb_sql_exit(sql);
        return 0;
    }

    /* fixed battery prefix */
    run_statement(sql, "LIST TABLES");

    /* per-table battery: statements built from the database's own catalog.
     * Snapshot the table names first: running a statement re-reads the catalog
     * internally (mdb_sql_select -> mdb_read_table_by_name -> mdb_read_catalog
     * frees and rebuilds mdb->catalog), so catalog entry pointers must never be
     * held across run_statement. */
    mdb_read_catalog(mdb, MDB_TABLE);
    for (i = 0; i < (uint32_t)mdb->num_catalog && i < MAX_BATTERY_TABLES; i++) {
        entry = (MdbCatalogEntry *)g_ptr_array_index(mdb->catalog, i);
        snprintf(table_names[n_tables], sizeof(table_names[0]), "%s",
                 entry->object_name);
        n_tables++;
    }
    for (i = 0; i < n_tables; i++) {
        snprintf(stmt, sizeof(stmt), "SELECT * FROM [%s]", table_names[i]);
        run_statement(sql, stmt);
        snprintf(stmt, sizeof(stmt), "SELECT COUNT(*) FROM [%s]",
                 table_names[i]);
        run_statement(sql, stmt);
    }

    /* direct sarg sweep: ops the SQL grammar cannot produce (LIKE/ILIKE, ...) */
    for (i = 0; i < (uint32_t)mdb->num_catalog && i < MAX_CATALOG; i++) {
        MdbTableDef *table;
        MdbColumn *col;
        unsigned long rows = 0;

        entry = (MdbCatalogEntry *)g_ptr_array_index(mdb->catalog, i);
        table = mdb_read_table(entry);
        if (!table)
            continue;
        if (!mdb_read_columns(table) || table->num_cols < 1) {
            mdb_free_tabledef(table);
            continue;
        }
        col = (MdbColumn *)g_ptr_array_index(table->columns, 0);
        for (n = 0; n < (int)(sizeof(ops) / sizeof(ops[0])); n++) {
            MdbSarg sarg;

            memset(&sarg, 0, sizeof(sarg));
            sarg.op = ops[n];
            sarg.value.i = (int)(i + 1);
            mdb_add_sarg_by_name(table, col->name, &sarg);

            memset(&sarg, 0, sizeof(sarg));
            sarg.op = ops[n];
            snprintf(sarg.value.s, sizeof(sarg.value.s), "%s",
                     entry->object_name);
            mdb_add_sarg_by_name(table, col->name, &sarg);
        }
        mdb_rewind_table(table);
        if (mdb_bind_column(table, 1, stmt, NULL) != -1) {
            while (rows++ < MAX_SARG_ROWS && mdb_fetch_row(table))
                sink ^= (unsigned long)stmt[0];
        }
        mdb_free_tabledef(table);
        break; /* one table is enough to exercise the sarg engine */
    }

    mdb_sql_exit(sql);
    return 0;
}
