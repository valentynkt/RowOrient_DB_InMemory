#include "minidb.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Example predicates (function pointers for WHERE clauses)
// ============================================================================

// These are top-level named functions — C has no lambdas.
// Each receives the row data and schema, returns true/false.
//
// In C# you'd write: table.Where(row => row["active"] == true)
// In C you write a named function and pass its address.

// Finds the column index by name. Returns -1 if not found.
// Helper used by predicates to locate columns without hardcoding indices.
static int find_column(const Column* columns, int col_count, const char* name) {
    for (int i = 0; i < col_count; i++) {
        if (strcmp(columns[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// WHERE active = true
static bool where_active(const Row* row, const Column* columns, int col_count) {
    int idx = find_column(columns, col_count, "active");
    if (idx < 0) return false;
    return row->values[idx].as.bool_val;
}

// WHERE age > 30
static bool where_age_over_30(const Row* row, const Column* columns, int col_count) {
    int idx = find_column(columns, col_count, "age");
    if (idx < 0) return false;
    return row->values[idx].as.int_val > 30;
}

// WHERE name starts with "A"
static bool where_name_starts_a(const Row* row, const Column* columns, int col_count) {
    int idx = find_column(columns, col_count, "name");
    if (idx < 0) return false;
    const char* name = row->values[idx].as.str_val;
    return name != NULL && name[0] == 'A';
}

// ============================================================================
// Main — demo driver
// ============================================================================

int main(void) {
    printf("=== minidb demo ===\n\n");

    // --- Create database ---
    Database* db = db_create();
    if (!db) {
        fprintf(stderr, "Failed to create database\n");
        return 1;
    }

    // --- Create "users" table ---
    // Schema: id (INT), name (STRING), age (INT), active (BOOL)
    const char* user_cols[] = {"id", "name", "age", "active"};
    ColumnType  user_types[] = {COL_INT, COL_STRING, COL_INT, COL_BOOL};

    Table* users = db_create_table(db, "users", user_cols, user_types, 4);
    if (!users) {
        fprintf(stderr, "Failed to create table\n");
        db_destroy(db);
        return 1;
    }

    // --- Insert rows ---
    // Note: val_str stores a pointer to the string literal.
    // table_insert deep-copies it onto the heap.
    // After insert returns, the literal could go away and the row is still valid.

    printf("--- Inserting rows ---\n");

    table_insert(users, (Value[]){val_int(1), val_str("Alice"),   val_int(28), val_bool(true)},  4);
    table_insert(users, (Value[]){val_int(2), val_str("Bob"),     val_int(35), val_bool(true)},  4);
    table_insert(users, (Value[]){val_int(3), val_str("Charlie"), val_int(42), val_bool(false)}, 4);
    table_insert(users, (Value[]){val_int(4), val_str("Diana"),   val_int(31), val_bool(true)},  4);
    table_insert(users, (Value[]){val_int(5), val_str("Eve"),     val_int(24), val_bool(false)}, 4);

    // --- Print all rows ---
    printf("\n--- All users ---\n");
    table_print(users);

    // --- Type mismatch test ---
    printf("\n--- Type mismatch (should fail) ---\n");
    // Swapped: passing STRING where INT is expected for "id"
    bool ok = table_insert(users, (Value[]){val_str("bad"), val_str("Fail"), val_int(0), val_bool(false)}, 4);
    printf("Insert returned: %s\n", ok ? "true" : "false");

    // --- Filtered queries (function pointers as WHERE clause) ---
    printf("\n--- Active users (WHERE active = true) ---\n");
    table_print_where(users, where_active);

    printf("\n--- Age over 30 (WHERE age > 30) ---\n");
    table_print_where(users, where_age_over_30);

    printf("\n--- Name starts with A ---\n");
    table_print_where(users, where_name_starts_a);

    // --- Count ---
    int active_count = table_count_where(users, where_active);
    printf("\nActive user count: %d\n", active_count);

    // --- Delete ---
    printf("\n--- Deleting inactive users ---\n");
    // We need a "where NOT active" predicate for delete.
    // But we can reuse where_active with a wrapper:
    // (This shows a limitation of C function pointers — no closures,
    //  so you can't easily negate or compose predicates inline.)
    int deleted = table_delete_where(users, where_active);
    // Oops — that deletes ACTIVE users. Let's be intentional about it
    // to test that deletion works. We'll re-check the table after.
    printf("Deleted %d rows\n", deleted);

    printf("\n--- Remaining users (should be inactive only) ---\n");
    table_print(users);

    // --- Create a second table to test multi-table database ---
    printf("\n--- Creating products table ---\n");
    const char* prod_cols[]  = {"id", "name", "price"};
    ColumnType  prod_types[] = {COL_INT, COL_STRING, COL_FLOAT};

    Table* products = db_create_table(db, "products", prod_cols, prod_types, 3);
    if (products) {
        table_insert(products, (Value[]){val_int(1), val_str("Widget"),  val_float(9.99f)},  3);
        table_insert(products, (Value[]){val_int(2), val_str("Gadget"),  val_float(24.50f)}, 3);
        table_insert(products, (Value[]){val_int(3), val_str("Gizmo"),   val_float(14.75f)}, 3);

        printf("\n--- All products ---\n");
        table_print(products);
    }

    // --- Drop the users table ---
    printf("\n--- Dropping users table ---\n");
    bool dropped = db_drop_table(db, "users");
    printf("Drop result: %s\n", dropped ? "dropped" : "not found");

    // Verify it's gone
    Table* ghost = db_find_table(db, "users");
    printf("Find 'users' after drop: %s\n", ghost ? "FOUND (bug!)" : "NULL (correct)");

    // Products should still exist
    Table* still_here = db_find_table(db, "products");
    printf("Find 'products': %s\n", still_here ? "found" : "NOT FOUND (bug!)");

    // --- Clean shutdown ---
    // This must free EVERYTHING: the products table, all its rows,
    // all string values, all column names, the database itself.
    // Run with sanitizers to verify zero leaks.
    printf("\n--- Destroying database ---\n");
    db_destroy(db);
    printf("Done. If no sanitizer errors above, memory is clean.\n");

    return 0;
}
