#ifndef MINIDB_H
#define MINIDB_H

#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// Type Definitions
// ============================================================================

// Tagged union discriminator — this is your "dynamic type system."
// In C# you'd use inheritance or generics. In C, you pair an enum tag
// with a union and switch on it manually. No compiler safety net.
typedef enum { COL_INT, COL_FLOAT, COL_STRING, COL_BOOL } ColumnType;

// A single cell value. The `type` tag tells you which union member is valid.
// Reading the wrong member is undefined behavior — the compiler won't stop you.
//
// sizeof(Value) = sizeof(type) + sizeof(largest union member) + padding.
// The union is as big as its largest member (char* pointer, likely 8 bytes).
// Smaller members (int, float, bool) share that same space.
typedef struct {
    ColumnType type;
    union {
        int int_val;
        float float_val;
        char *str_val; // heap-allocated, owned by the Row
        bool bool_val;
    } as; // named union so you access: val.as.int_val
} Value;

// Column schema definition — just metadata, no data.
typedef struct {
    char *name; // heap-allocated, owned by the Table
    ColumnType type;
} Column;

// A single row: an array of Values, one per column.
// The Row owns its Value array AND any heap-allocated strings inside Values.
typedef struct {
    Value *values; // heap array, length = table's col_count
    int col_count;
} Row;

// The table: schema + dynamic array of row pointers.
//
// rows is Row** (pointer to pointer) because:
//   - rows is a heap-allocated array of Row* pointers
//   - each Row* points to a heap-allocated Row
//   - realloc can move the array without moving the Rows themselves
//
// This is the C equivalent of List<Row> in C# — but you manage the
// backing array, count, and capacity yourself.
typedef struct {
    char *name;      // heap-allocated table name
    Column *columns; // heap array of column definitions
    int col_count;

    Row **rows; // dynamic array of Row pointers
    int row_count;
    int row_capacity; // allocated slots in rows[]
} Table;

// Top-level database: owns multiple tables.
typedef struct {
    Table **tables; // dynamic array of Table pointers
    int table_count;
    int table_capacity;
} Database;

// ============================================================================
// Function pointer types for queries
// ============================================================================

// Predicate for filtering rows — C's version of Func<Row, bool>.
// Receives the row, the column schema (so you can inspect by name/type),
// and the column count.
typedef bool (*RowPredicate)(const Row *row, const Column *columns, int col_count);

// ============================================================================
// Database lifecycle
// ============================================================================

// Creates an empty database. Returns NULL on allocation failure.
Database *db_create(void);

// Destroys the database and ALL tables/rows/values it owns.
// After this call, every pointer obtained from this db is invalid.
void db_destroy(Database *db);

// ============================================================================
// Table operations
// ============================================================================

// Creates a table with the given schema and adds it to the database.
// `col_names` and `col_types` are parallel arrays describing each column.
// The table deep-copies all names — caller retains ownership of the input arrays.
// Returns a pointer to the created Table (owned by db), or NULL on failure.
Table *db_create_table(Database *db, const char *name, const char *col_names[],
                       const ColumnType col_types[], int col_count);

// Finds a table by name. Returns NULL if not found.
// The returned pointer is owned by the database — don't free it.
Table *db_find_table(Database *db, const char *name);

// Drops (destroys) a table by name. Frees all its rows and column data.
// Returns true if found and dropped, false if not found.
bool db_drop_table(Database *db, const char *name);

// ============================================================================
// Row operations
// ============================================================================

// Inserts a row into the table.
// `values` is an array of Value structs, one per column.
// The table deep-copies everything (including strings).
// Returns true on success, false on type mismatch or allocation failure.
//
// IMPORTANT: values[i].type MUST match table->columns[i].type.
// This function validates types and rejects mismatches.
bool table_insert(Table *table, const Value values[], int value_count);

// Deletes all rows matching the predicate. Frees their memory.
// Returns the number of rows deleted.
//
// TRAP: Think carefully about how you iterate the array while removing
// elements. Shifting pointers left after each delete? Swap-with-last?
// Each approach has tradeoffs.
int table_delete_where(Table *table, RowPredicate predicate);

// ============================================================================
// Query / display
// ============================================================================

// Prints all rows in a formatted table to stdout.
void table_print(const Table *table);

// Prints only rows matching the predicate.
// This is your SELECT ... WHERE — the function pointer is the WHERE clause.
void table_print_where(const Table *table, RowPredicate predicate);

// Returns the count of rows matching the predicate.
int table_count_where(const Table *table, RowPredicate predicate);

// ============================================================================
// Value constructors — convenience helpers
// ============================================================================

// These create Value structs on the stack. No heap allocation here —
// the heap copy happens inside table_insert when the row is stored.
//
// Usage:  Value vals[] = { val_int(1), val_str("Alice"), val_bool(true) };
//         table_insert(table, vals, 3);

Value val_int(int v);
Value val_float(float v);
Value val_str(const char *v); // stores the pointer as-is (insert deep-copies)
Value val_bool(bool v);

// ============================================================================
// Utility
// ============================================================================

// Returns a human-readable type name: "INT", "FLOAT", "STRING", "BOOL".
const char *column_type_name(ColumnType type);

#endif // MINIDB_H
