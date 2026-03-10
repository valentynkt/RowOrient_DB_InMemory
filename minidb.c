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

// Frees the entire ownership tree: db → tables → rows → values → strings.
// Trap: every level must be freed, and inner before outer.
void db_destroy(Database *db)
{
    if (!db)
        return;
    for (int i = 0; i < db->table_count; i++) {
        table_destroy(db->tables[i]);
    }
    free((void *)db->tables);
    free(db);
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
        if (!temp)
            goto cleanup;
        db->tables = temp;
        db->table_capacity = new_cap;
    }
    db->tables[db->table_count++] = table;
    return table;

cleanup:
    table_destroy(table);
    return NULL;
}

static int db_find_index(Database *db, const char *name)
{

    for (int i = 0; i < db->table_count; i++) {
        if (strcmp(db->tables[i]->name, name) == 0) {
            return i;
        }
    }
    return -1;
}
// Linear scan by name. strcmp returns 0 on match (not true).
Table *db_find_table(Database *db, const char *name)
{
    if (!db || !name)
        return NULL;
    int idx = db_find_index(db, name);
    return idx >= 0 ? db->tables[idx] : NULL;
}

// Finds, destroys, and removes a table from the db.
// Trap: after removing, compact the array (shift or swap-with-last).
bool db_drop_table(Database *db, const char *name)
{
    if (!db || !name)
        return false;
    int table_idx = db_find_index(db, name);
    if (table_idx == -1)
        return false;
    Table *table_to_delete = db->tables[table_idx];
    table_destroy(table_to_delete);
    db->tables[table_idx] = db->tables[--db->table_count];
    return true;
}

// ============================================================================
// Row operations
// ============================================================================

// Validates types against schema, deep-copies values into a new row, appends it.
// Rejects type mismatches — print what went wrong and return false.
bool table_insert(Table *table, const Value values[], int value_count)
{
    if (!table || table->col_count != value_count)
        return false;
    for (int i = 0; i < table->col_count; i++) {
        if (table->columns[i].type != values[i].type) {
            return false;
        }
    }
    if (!table_ensure_capacity(table))
        return false;
    Row *row = row_create(values, table->col_count);
    if (!row)
        return false;
    table->rows[table->row_count++] = row;
    return true;
}

// ============================================================================
// Query / display
// ============================================================================

// Prints all rows. Switch on each Value's type tag to format correctly.
// Trap: reading the wrong union member is undefined behavior.
static void table_print_col_helper(const Table *table, const int row_idx)
{
    for (int j = 0; j < table->col_count; j++) {
        Value cur_value = table->rows[row_idx]->values[j];

        printf("Col: type - %s\n", column_type_name(cur_value.type));
        switch (cur_value.type) {
        case COL_INT:
            printf("%d\n", cur_value.as.int_val);
            break;
        case COL_BOOL:
            printf("%s\n", cur_value.as.bool_val ? "true" : "false");
            break;
        case COL_FLOAT:
            printf("%.2f\n", cur_value.as.float_val);
            break;
        case COL_STRING:
            printf("%s\n", cur_value.as.str_val);
            break;
        }
    }
}
void table_print(const Table *table)
{
    if (!table)
        return;
    for (int i = 0; i < table->row_count; i++) {
        printf("Row: %d\n", i);
        table_print_col_helper(table, i);
    }
}

// Prints only rows where predicate returns true.
void table_print_where(const Table *table, RowPredicate predicate)
{
    if (!table)
        return;
    for (int i = 0; i < table->row_count; i++) {
        if (predicate(table->rows[i], table->columns, table->col_count)) {
            printf("Row: %d\n", i);
            table_print_col_helper(table, i);
        }
    }
}

// Counts rows matching the predicate.
int table_count_where(const Table *table, RowPredicate predicate)
{
    if (!table)
        return 0;
    int count = 0;
    for (int i = 0; i < table->row_count; i++) {
        if (predicate(table->rows[i], table->columns, table->col_count)) {
            count++;
        }
    }
    return count;
}

// Deletes all rows matching the predicate. Frees their memory.
// Returns the number of deleted rows.
// Trap: iterating an array while removing elements from it.
// Think about read-pointer vs write-pointer.
int table_delete_where(Table *table, RowPredicate predicate)
{
    if (!table)
        return 0;
    int count = 0;
    for (int i = 0; i < table->row_count; i++) {
        if (predicate(table->rows[i], table->columns, table->col_count)) {
            row_destroy(table->rows[i]);
            table->rows[i] = table->rows[--table->row_count];
            i--;
            count++;
        }
    }
    return count;
}
