# RowOrient_DB

A tiny in-memory, row-oriented database in C: tables with typed columns, rows as
tagged-union cells, runtime type-checking on insert, and `SELECT` / `DELETE …
WHERE` expressed as C function pointers. Built to understand how a row store and a
query predicate actually work underneath — with manual memory ownership from top
to bottom. No dependencies beyond libc.

## Demo

```sh
make && ./minidb
```

```c
const char *cols[]  = {"id", "name", "age", "active"};
ColumnType  types[] = {COL_INT, COL_STRING, COL_INT, COL_BOOL};
Table *users = db_create_table(db, "users", cols, types, 4);

table_insert(users, (Value[]){val_int(1), val_str("Alice"), val_int(28), val_bool(true)}, 4);

table_print_where(users, where_age_over_30);    // SELECT * WHERE age > 30
int n = table_count_where(users, where_active);  // SELECT COUNT(*) WHERE active
```

## Data model

```
Database → Table → Row → Value
```

- **Value** is a tagged union (`COL_INT | COL_FLOAT | COL_STRING | COL_BOOL`) — an
  enum tag plus a `union`, switched on by hand. A dynamic cell type with no
  compiler safety net.
- **Table** owns a schema (`Column[]`) and a `realloc`-grown array of `Row*`; each
  **Row** owns its `Value` array and any heap strings inside it.
- **Database** owns multiple tables. `db_destroy` frees the whole tree — tables,
  rows, values, strings — and the demo runs under ASan/UBSan to prove zero leaks.

## Queries are function pointers

C has no lambdas, so a `WHERE` clause is a named predicate:

```c
typedef bool (*RowPredicate)(const Row *row, const Column *columns, int col_count);
```

`table_print_where`, `table_count_where`, and `table_delete_where` each take one —
`Func<Row,bool>` built from a raw function pointer, the mechanism a query engine
hides behind SQL.

## Type safety

`table_insert` validates every cell against the column schema and rejects
mismatches at runtime (a `STRING` into an `INT` column fails) — a hand-rolled
stand-in for what a typed query layer enforces.

## Out of scope

An in-memory study of row-store internals, not a database you'd ship. No SQL
parser (predicates are C functions), no persistence, no indexes (queries are full
scans), no transactions, single-threaded. The point is the layout, the ownership,
and the predicate mechanism.
