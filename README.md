# 🗂️ miniql
![Lang](https://img.shields.io/badge/language-C-blue?style=flat-square&logo=c) ![License](https://img.shields.io/badge/license-MIT-green?style=flat-square) ![Status](https://img.shields.io/badge/status-WIP-orange?style=flat-square)

**miniql** is a lightweight query engine written in C, designed to work with custom-formatted CSV files. It allows you to parse tables, manage rows and columns, and perform basic query operations.

## Features

- Row Buffer Management: Efficient allocation for row storage.
- Data Access Functions: set and get values from rows.
- Commands:
    - PRINT – Display table contents
    - APPEND ROW – Add new rows to a table
- CSV Output: Save query results or table data to db.csv.

## Usage

Run miniql from the command line:

```bash
miniql database <Command>
```

### Examples

```bash
miniql books.csv PRINT
miniql books.csv "APPEND ROW (John, 25, john@example.com)"
```

## Future Improvements

- Advanced query support (SET, GET, APPEND COLUMN, ...)
