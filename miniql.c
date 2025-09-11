// ************************************************
// miniql - Mini Query Language for CSV Databases
// 
// Author: Marc-Daniel DALEBA
// Date: 08-09-2025
// License: MIT
// Description:
//   miniql is a lightweight query engine for
//   custom-formatted CSV files.
//   Current features:
//     - Table name & schema parsing
//     - Column type/size handling
//     - Row buffer allocation
//     - Data access functions (set/get)
//     - PRINT and APPEND ROW commands
//     - Save output in db.csv
//
//
// ************************************************
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LENGTH  (64)
#define MAX_COLUMNS (10)

#define WHAT printf("???\n");

typedef enum DB_TYPES {
  TYPE_INT,
  TYPE_CHAR,
  TYPE_INVALID,
} DB_TYPES;

typedef struct Column {
  char name[MAX_LENGTH];
  DB_TYPES type;
  int type_count;
  int offset;
} Column;

typedef struct Table {
  char name[MAX_LENGTH];
  Column col[MAX_COLUMNS];
  int col_count;
  int row_count;
  char *row_buffer;
  int table_size;
  int row_size;
} Table;

int get_table_name(Table *table, char *line);
int get_columns_info(Table *table, char *line);
int get_row(Table *table, char *line, int row_start);

DB_TYPES type_name_to_type(char *type_name);
char *type_to_type_name(DB_TYPES type);
int type_to_sizeof(DB_TYPES type);
int set_int(Table *table, int value, int offset);
int set_char_array(Table *table, char *value, int offset, int arrsize);
int get_int(Table *table, int row, int column);
char *get_char_array(Table *table, int row, int column, int *arrsize);

char *cmd_token(char *ptr, char *buf);
int PRINT_Command(Table *table);

int main(int argc, char **argv)
{
  Table tabl;
  char s[1024];
  
  if (argc < 2) {
    printf("error: no CSV database provided.\n");
    printf("usage: miniql database <Command>\n");
    printf("Commands:\n");
    printf("  APPEND ROW (val1, val2, ...)   append a new row at the end of the table.\n");
    printf("  PRINT                          print the content of the table.\n");
  }
  
  FILE *f = fopen(argv[1], "r");
  if (!f) {
    printf("Could not open %s\n", argv[1]);
    return -1;
  }
  
  // GET TABLE NAME
  fgets(s, sizeof(s)-1, f);
  get_table_name(&tabl, s);
  printf("table %s\n", tabl.name);
  
  // GET COLUMNS INFO
  
  fgets(s, sizeof(s)-1, f);
  get_columns_info(&tabl, s);
  
  for (int i = 0; i < tabl.col_count; ++i) {
    printf("column %s type %s count %d offset %d\n",
      tabl.col[i].name,
      type_to_type_name(tabl.col[i].type),
      tabl.col[i].type_count,
      tabl.col[i].offset
    );
  }
  printf("\n");
  
  // ALLOCATE BUFFER
  
  tabl.row_buffer = (char*)malloc(0); // can you malloc(0) ?
  if (!tabl.row_buffer) {
    fclose(f);
    return -1;
  }
  
  // GET ROWS
  
  int row_start = 0;
  tabl.table_size = 0;
  tabl.row_count = 0;
  while (fgets(s, sizeof(s)-1, f)) {
    row_start = tabl.table_size;
    tabl.table_size += tabl.row_size;
    tabl.row_buffer = (char*)realloc(tabl.row_buffer, tabl.table_size);
    if (!tabl.row_buffer) {
      fclose(f);
      return -1;
    }
    get_row(&tabl, s, row_start);
    tabl.row_count++;
  }
  
  fclose(f);
  
  if (argc >= 3) {
    char buf[32];
    char *cmd = argv[2];
    cmd = cmd_token(cmd, buf);
    
    if (!strcmp("PRINT", buf)) {
      PRINT_Command(&tabl);
    } else
    if (!strcmp("APPEND", buf)) {
      cmd = cmd_token(cmd, buf);
      if (!strcmp("ROW", buf)) {
        cmd = cmd_token(cmd, buf);
        if (strcmp("(", buf)) {
          // failed.
        }
        
        // ALLOCATE SPACE
        
        int col = 0;
        int row = tabl.row_count;
        tabl.table_size += tabl.row_size;
        tabl.row_buffer = (char*)realloc(tabl.row_buffer, tabl.table_size);
        if (!tabl.row_buffer) {
          fclose(f);
          return -1;
        }
        tabl.row_count++;
        
        // ADD VALUES
        
        cmd = cmd_token(cmd, buf);
        while (1) {
          int offset = row * tabl.row_size + tabl.col[col].offset;
          DB_TYPES type = tabl.col[col].type;
          
          if (type == TYPE_INT) {
            set_int(&tabl, atoi(buf), offset);
          } else
          if (type == TYPE_CHAR) {
            if (tabl.col[col].type_count > 1) {
              set_char_array(&tabl, buf, offset, tabl.col[col].type_count);
            }
          }
          
          cmd = cmd_token(cmd, buf);
          if (!strcmp(")", buf)) {
            break;
          }
          if (strcmp(",", buf)) {
            // failed. expected ','
          }
          cmd = cmd_token(cmd, buf);
          col++;
        }
        
        PRINT_Command(&tabl);
      }
    }
  }
  
  // SAVE THE DATABASE
  
  FILE *fout = fopen("db.csv", "w");
  if (!f) {
    goto cleanup;
  }
  
  fprintf(fout, "%s", tabl.name);
  for (int j = 0; j < tabl.col_count-1; ++j) {
    fprintf(fout, "|");
  }
  fprintf(fout, "\n");
  
  for (int j = 0; j < tabl.col_count; ++j) {
    fprintf(fout, "%s %s", tabl.col[j].name, type_to_type_name(tabl.col[j].type));
    if (tabl.col[j].type_count > 1) {
      fprintf(fout, "(%d)", tabl.col[j].type_count);
    }
    if (j < tabl.col_count-1) {
      fprintf(fout, "|");
    }
  }
  fprintf(fout, "\n");
  
  for (int i = 0; i < tabl.row_count; ++i) {
    for (int j = 0; j < tabl.col_count; ++j) {
      if (tabl.col[j].type == TYPE_INT) {
        int value = get_int(&tabl, i, j);
        fprintf(fout, "%d", value);
      } else
      if (tabl.col[j].type == TYPE_CHAR) {
        if (tabl.col[j].type_count > 1) {
          int arrsize;
          char *value = get_char_array(&tabl, i, j, &arrsize);
          fprintf(fout, "%s", value);
        } else {
          fprintf(fout, "%c", 'A');
        }
      }
      if (j < tabl.col_count-1) {
        fprintf(fout, "|");
      }
    }
    fprintf(fout, "\n");
  }
  
  // bookbase||
  // id INT|name CHAR(25)|author CHAR(25)
  // 0|The Great Quest|Arnold
  // 1|Purple Sea|Christine
  // 2|Into the Moon|Arnold
  // 3|Road to Victory|blueberry

  
  fclose(f);
 
cleanup:
  printf("tabl.table_size = %d\n", tabl.table_size);
  free(tabl.row_buffer);
  return 0;
}

int get_table_name(Table *table, char *line)
{
  char table_name[MAX_LENGTH] = {'\0'};
  char *table_name_ptr = table_name;
  char *ptr = line;
  
  while (*ptr) {
    while (isspace(*ptr))
      ptr++;
    
    if (isalpha(*ptr)) {
      while (isalpha(*ptr)) {
        *table_name_ptr = *ptr;
        table_name_ptr++;
        ptr++;
      }
      *table_name_ptr = '\0';
    }
    ptr++;
  }
  
  strncpy(table->name, table_name, sizeof(table_name)-1);
  return 0;
}

#define set_char(buf, ch) *buf=ch; *(buf+1)='\0'
char *cmd_token(char *ptr, char *buf)
{
  while (isspace(*ptr)) {
    ptr++;
  }
  
  if (!*ptr)
    return NULL;
  
  if (isalpha(*ptr)) {
    int i = 0;
    while (isalpha(*ptr)) {
      buf[i] = *ptr;
      ptr++;
      i++;
    }
    buf[i] = '\0';
    return ptr;
  }
  
  if (isdigit(*ptr)) {
    int i = 0;
    while (isdigit(*ptr)) {
      buf[i++] = *ptr;
      ptr++;
    }
    buf[i] = '\0';
    return ptr;
  }
  
  if (*ptr == '"') {
    int i = 0;
    ptr++;
    while (*ptr != '"') {
      buf[i++] = *ptr;
      ptr++;
    }
    ptr++; // skip '"'
    buf[i] = '\0';
    return ptr;
  }
  
  set_char(buf, *ptr);
  return ptr+1;
}

int get_columns_info(Table *table, char *line)
{
  char *ptr = line;
  char name[MAX_LENGTH] = {'\0'};
  char type_name[MAX_LENGTH] = {'\0'};
  int type_count = 1;
  char *name_ptr = name;
  int done = 0;
  int state = 0; // 0=name, 1=type_name

  table->row_size = 0;
  table->col_count = 0;
  while (!done) {
    while (isspace(*ptr)) {
      ptr++;
    }
    if (isalpha(*ptr)) {
      while (isalpha(*ptr)) {
        *name_ptr = *ptr;
        name_ptr++;
        ptr++;
      }
      
      *name_ptr = '\0';
      name_ptr = type_name;
      state = 1;
    }
    if (*ptr == '|' || !*ptr) {
      Column *col = &table->col[table->col_count];
      strncpy(col->name, name, sizeof(col->name));
      col->type = type_name_to_type(type_name);
      col->type_count = type_count;
      col->offset = table->row_size;
      table->col_count++;
      table->row_size += type_to_sizeof(col->type) * col->type_count;
      
      // Reset
      name_ptr = name;
      type_count = 1;
      state = 0;
      
      if (!*ptr)
        done = 1;
    }
    if (state == 1) {
      if (*ptr == '(') {
        ptr++;
        if (isdigit(*ptr)) {
          type_count = 0;
          while (isdigit(*ptr)) {
            type_count *= 10;
            type_count += *ptr - '0';
            ptr++;
          }
        } else {
          printf("error unexpected %c\n", *ptr);
          return -1;
        }
        if (*ptr != ')') {
          printf("error!\n");
          return -1;
        }
        if (type_count <= 0) {
          printf("error!\n");
          return -1;
        }
      }
    }
    ptr++;
  }
}

int get_row(Table *table, char *line, int row_start)
{
  char buf[MAX_LENGTH] = {'\0'};
  char *ptr = NULL;
  int col_index = 0;
  
  // get values
  ptr = line;
  while (*ptr) {
    char *buf_ptr = buf;
    while (*ptr != '|' && *ptr != '\n') {
      *buf_ptr = *ptr;
      buf_ptr++;
      ptr++;
    }
    *buf_ptr = '\0';
    
    // add value
    int offset = row_start + table->col[col_index].offset;
    DB_TYPES type = table->col[col_index].type;
    
    if (type == TYPE_INT) {
      set_int(table, atoi(buf), offset);
    } else
    if (type == TYPE_CHAR) {
      if (table->col[col_index].type_count > 1) {
        set_char_array(table, buf, offset, table->col[col_index].type_count);
      }
    }
    
    col_index++;
    ptr++;
  }
  return 0;
}

DB_TYPES type_name_to_type(char *type_name)
{
  if (!strcmp(type_name, "INT")) {
    return TYPE_INT;
  }
  if (!strcmp(type_name, "CHAR")) {
    return TYPE_CHAR;
  }
  return TYPE_INVALID;
}

char *type_to_type_name(DB_TYPES type)
{
  if (type == TYPE_INT) {
    return "INT";
  }
  if (type == TYPE_CHAR) {
    return "CHAR";
  }
  return "INVALID";
}

int type_to_sizeof(DB_TYPES type)
{
  if (type == TYPE_INT) {
    return sizeof(int);
  }
  if (type == TYPE_CHAR) {
    return sizeof(char);
  }
  return 0;
}

int set_int(Table *table, int value, int offset)
{
  int *ptr = (int*)(table->row_buffer + offset);
  *ptr = value;
  return 0;
}

int set_char_array(Table *table, char *value, int offset, int arrsize)
{
  char *ptr = (char*)(table->row_buffer + offset);
  memcpy(ptr, value, arrsize);
  return 0;
}

int get_int(Table *table, int row, int column)
{
  int offset = row * table->row_size + table->col[column].offset;
  int *ptr = (int*)(table->row_buffer + offset);
  return *ptr;
}

char *get_char_array(Table *table, int row, int column, int *arrsize)
{
  int offset = row * table->row_size + table->col[column].offset;
  char *ptr = (char*)(table->row_buffer + offset);
  *arrsize = table->col[column].type_count;
  return ptr;
}

// ======================================

int PRINT_Command(Table *table)
{
  for (int j = 0; j < table->col_count; ++j) {
    printf("%25s ", table->col[j].name);
  }
  printf("\n");
  
  for (int i = 0; i < table->row_count; ++i) {
    for (int j = 0; j < table->col_count; ++j) {
      if (table->col[j].type == TYPE_INT) {
        int value = get_int(table, i, j);
        printf("%25d ", value);
      } else
      if (table->col[j].type == TYPE_CHAR) {
        if (table->col[j].type_count > 1) {
          int arrsize;
          char *value = get_char_array(table, i, j, &arrsize);
          printf("%25.*s ", arrsize, value);
        } else {
          printf("%25c ", 'A');
        }
      }
    }
    printf("\n");
  }
}