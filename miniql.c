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
//
//
//
// ************************************************
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LENGTH  (64)
#define MAX_COLUMNS (10)

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

DB_TYPES type_name_to_type(char *type_name);
char *type_to_type_name(DB_TYPES type);
int type_to_sizeof(DB_TYPES type);
int set_int(Table *table, int value, int offset);
int set_char_array(Table *table, char *value, int offset, int arrsize);
int get_int(Table *table, int row, int column);
char *get_char_array(Table *table, int row, int column, int *arrsize);

int main(int argc, char **argv)
{
  Table tabl;
  char s[1024];
  
  FILE *f = fopen(argv[1], "r");
  if (!f) {
    printf("Could not open %s\n", argv[1]);
    return -1;
  }
  
  // GET TABLE NAME
  {
    fgets(s, sizeof(s)-1, f);
    
    char table_name[MAX_LENGTH];
    char *table_name_ptr = table_name;
    char *ptr = s;
    
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
    
    strncpy(tabl.name, table_name, sizeof(table_name)-1);
    printf("table %s\n", tabl.name);
  }
  
  
  // GET COLUMNS INFO
  
  fgets(s, sizeof(s)-1, f);
  
  char *ptr = s;
  char name[MAX_LENGTH] = {'\0'};
  char type_name[MAX_LENGTH] = {'\0'};
  int type_count = 1;
  char *name_ptr = name;
  int done = 0;
  int state = 0; // 0=name, 1=type_name
  
  tabl.row_size = 0;
  tabl.col_count = 0;
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
      Column *col = &tabl.col[tabl.col_count];
      strncpy(col->name, name, sizeof(col->name));
      col->type = type_name_to_type(type_name);
      col->type_count = type_count;
      col->offset = tabl.row_size;
      tabl.col_count++;
      tabl.row_size += type_to_sizeof(col->type) * col->type_count;
      
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
  
  tabl.row_buffer = (char*)malloc(1); // can you malloc(0) ?
  if (!tabl.row_buffer) {
    fclose(f);
    return -1;
  }
  tabl.table_size = 0;
  
  // GET ROWS
  
  {
    char _s[256];
    char buf[MAX_LENGTH] = {'\0'};
    char *ptr = NULL;
    int row_start = 0;
    // int row_index = 0;
    
    tabl.row_count = 0;
    while (fgets(_s, sizeof(_s)-1, f)) {
      int col_index = 0;
      
      tabl.row_count++;
      row_start = tabl.table_size;
      tabl.table_size += tabl.row_size;
      tabl.row_buffer = (char*)realloc(tabl.row_buffer, tabl.table_size);
      if (!tabl.row_buffer) {
        fclose(f);
        return -1;
      }
      
      // get values
      ptr = _s;
      while (*ptr) {
        char *buf_ptr = buf;
        while (*ptr != '|' && *ptr != '\n') {
          *buf_ptr = *ptr;
          buf_ptr++;
          ptr++;
        }
        *buf_ptr = '\0';
        // printf("buf=%s\n", buf);
        
        // add value
        
        int offset = row_start + tabl.col[col_index].offset;
        DB_TYPES type = tabl.col[col_index].type;
        
        if (type == TYPE_INT) {
          set_int(&tabl, atoi(buf), offset);
        } else
        if (type == TYPE_CHAR) {
          if (tabl.col[col_index].type_count > 1) {
            set_char_array(&tabl, buf, offset, tabl.col[col_index].type_count);
          }
        }
        
        col_index++;
        ptr++;
      }
    }
  }
  
  fclose(f);
  
  // Read table
  
  for (int j = 0; j < tabl.col_count; ++j) {
    printf("%25s ", tabl.col[j].name);
  }
  printf("\n");
  
  for (int i = 0; i < tabl.row_count; ++i) {
    for (int j = 0; j < tabl.col_count; ++j) {
      if (tabl.col[j].type == TYPE_INT) {
        int value = get_int(&tabl, i, j);
        printf("%25d ", value);
      } else
      if (tabl.col[j].type == TYPE_CHAR) {
        if (tabl.col[j].type_count > 1) {
          int arrsize;
          char *value = get_char_array(&tabl, i, j, &arrsize);
          printf("%25.*s ", arrsize, value);
        } else {
          printf("%25c ", 'A');
        }
      }
    }
    printf("\n");
  }
  
  //
  
  printf("tabl.table_size = %d\n", tabl.table_size);
  free(tabl.row_buffer);
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