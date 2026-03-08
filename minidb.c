#include "minidb.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Internal helpers (not exposed in header)
// ============================================================================

// Deep-copies src onto the heap. Caller must free the result.
// Trap: strlen does NOT count the null terminator.
static char *str_dup(const char *src)
{
    size_t len_str = strlen(src);
    char *copy = malloc(len_str + 1);

    if (copy == NULL) {
        return NULL;
    }

    strcpy(copy, src);

    return copy;
}

// Frees heap resources inside a single Value.
// Only one variant of the union has heap data — which one?
static void value_free(Value *val)
{
    if (val->type == COL_STRING) {
        free(val->as.str_val);
    }
}

// Deep-copies a Value. Most variants are just a struct copy.
// Trap: what happens if two Values point to the same heap string?
static Value value_dup(const Value *src)
{
    Value copy = *src;
    if (src->type == COL_STRING) {
        copy.as.str_val = str_dup(src->as.str_val);
    }
    return copy;
}

// Allocates a Row and deep-copies all values into it.
// Returns NULL on failure.
// Trap: if allocation fails partway, you own everything you already allocated.
static Row *row_create(const Value values[], int col_count)
{
    Row *row = malloc(sizeof(Row));
    if (row == NULL) {
        return NULL;
    }
    Value *val_copies = malloc(sizeof(Value) * col_count);
    if (val_copies == NULL) {
        free(row);
        return NULL;
    }
    for (int i = 0; i < col_count; i++) {
        val_copies[i] = value_dup(&values[i]);
    }
    row->values = val_copies;
    row->col_count = col_count;
    return row;
}

// Frees a Row and everything it owns.
// Trap: free order matters — inner before outer.
static void row_destroy(Row *row)
{
    for (int i = 0; i < row->col_count; i++) {
        value_free(&row->values[i]);
    }
    free(row->values);
    free(row);
}

// Grows the rows array if row_count == row_capacity.
// Trap: if you assign realloc's result directly to the original pointer
// and it returns NULL, you've lost the original memory.
static bool table_ensure_capacity(Table *table)
{
    if (table->row_count < table->row_capacity) {
        return true;
    }
    int new_capacity = table->row_capacity == 0 ? 4 : table->row_capacity * 2;
    Row **temp = (Row **)realloc((void *)table->rows, sizeof(Row *) * new_capacity);
    if (temp == NULL) {
        return false;
    }
    table->rows = temp;
    table->row_capacity = new_capacity;
    return true;
}

// ============================================================================
// Value constructors
// ============================================================================

// These return Value structs on the stack — no heap allocation.
// The heap copy happens later during insert.

Value val_int(int v)
{
    Value val;
    val.type = COL_INT;
    val.as.int_val = v;
    return val;
}

Value val_float(float v)
{
    Value val;
    val.type = COL_FLOAT;
    val.as.float_val = v;
    return val;
}

Value val_str(const char *v)
{
    Value val;
    val.type = COL_STRING;
    val.as.str_val = (char *)v;
    return val;
}

Value val_bool(bool v)
{
    Value val;
    val.type = COL_BOOL;
    val.as.bool_val = v;
    return val;
}

// ============================================================================
// Utility
// ============================================================================

// Returns a string literal for the type name.
// String literals live in static memory — safe to return.
const char *column_type_name(ColumnType type)
{
    switch (type) {
    case COL_INT:
        return "INT";
    case COL_FLOAT:
        return "FLOAT";
    case COL_BOOL:
        return "BOOL";
    case COL_STRING:
        return "STRING";
    }
    return "UNKNOWN";
}

// ============================================================================
// Database lifecycle
// ============================================================================

// Allocates an empty Database on the heap.
Database *db_create(void)
{
    Database *db = malloc(sizeof(*db));
    if (db == NULL) {
        return NULL;
    }
    db->tables = NULL;
    db->table_count = 0;
    db->table_capacity = 0;
    return db;
}

// Frees the entire ownership tree: db → tables → rows → values → strings.
// Trap: every level must be freed, and inner before outer.
void db_destroy(Database *db)
{
    // TODO: implement
    (void)db;
}

// ============================================================================
// Table operations
// ============================================================================

// Frees a table and everything it owns. Does NOT remove it from the db.
static void table_destroy(Table *table)
{
    if (!table)
        return;
    free(table->name);
    for (int i = 0; i < table->col_count; i++) {
        free(table->columns[i].name);
    }
    free(table->columns);
    table->col_count = 0;
    for (int i = 0; i < table->row_count; i++) {
        row_destroy(table->rows[i]);
    }
    free((void *)table->rows);
    free(table);
}

// Creates a table, deep-copies all column names, adds it to the database.
// Returns the table pointer (owned by db) or NULL on failure.
// Trap: if allocation fails midway, clean up what you already allocated.
Table *db_create_table(Database *db, const char *name, const char *col_names[],
                       const ColumnType col_types[], int col_count)
{
    Table *table = calloc(1, sizeof(*table));
    if (!table)
        goto cleanup;
    table->name = str_dup(name);
    if (!table->name)
        goto cleanup;

    table->columns = calloc(col_count, sizeof(Column));

    if (!table->columns)
        goto cleanup;

    for (int i = 0; i < col_count; i++) {
        table->columns[i].name = str_dup(col_names[i]);
        if (!table->columns[i].name)
            goto cleanup;
        table->columns[i].type = col_types[i];
        table->col_count++;
    }

    if (db->table_count == db->table_capacity) {
        int new_cap = db->table_capacity == 0 ? 4 : db->table_capacity * 2;
        Table **temp = (Table **)realloc((void *)db->tables, sizeof(Table *) * new_cap);
        if (!temp) {
            db->table_count--;
            goto cleanup;
            db->tables = temp;
            db->table_capacity = new_cap;
        }
    }
    db->tables[db->table_count++] = table;
    return table;

cleanup:
    table_destroy(table);
    return NULL;
}

// Linear scan by name. strcmp returns 0 on match (not true).
Table *db_find_table(Database *db, const char *name)
{
    // TODO: implement
    (void)db;
    (void)name;
    return NULL;
}

// Finds, destroys, and removes a table from the db.
// Trap: after removing, compact the array (shift or swap-with-last).
bool db_drop_table(Database *db, const char *name)
{
    // TODO: implement
    (void)db;
    (void)name;
    return false;
}

// ============================================================================
// Row operations
// ============================================================================

// Validates types against schema, deep-copies values into a new row, appends it.
// Rejects type mismatches — print what went wrong and return false.
bool table_insert(Table *table, const Value values[], int value_count)
{
    // TODO: implement
    (void)table;
    (void)values;
    (void)value_count;
    return false;
}

// Deletes all rows matching the predicate. Frees their memory.
// Returns the number of deleted rows.
// Trap: iterating an array while removing elements from it.
// Think about read-pointer vs write-pointer.
int table_delete_where(Table *table, RowPredicate predicate)
{
    // TODO: implement
    (void)table;
    (void)predicate;
    return 0;
}

// ============================================================================
// Query / display
// ============================================================================

// Prints all rows. Switch on each Value's type tag to format correctly.
// Trap: reading the wrong union member is undefined behavior.
void table_print(const Table *table)
{
    // TODO: implement
    (void)table;
}

// Prints only rows where predicate returns true.
void table_print_where(const Table *table, RowPredicate predicate)
{
    // TODO: implement
    (void)table;
    (void)predicate;
}

// Counts rows matching the predicate.
int table_count_where(const Table *table, RowPredicate predicate)
{
    // TODO: implement
    (void)table;
    (void)predicate;
    return 0;
}
