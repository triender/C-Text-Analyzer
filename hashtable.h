#ifndef HASHTABLE_H
#define HASHTABLE_H

// Cấu trúc cho một mục trong bảng băm (dùng cho chaining)
typedef struct Entry {
    char *word;
    int count;
    struct Entry *next;
} Entry;

// Cấu trúc cho bảng băm
typedef struct {
    Entry **entries;
    int size;
} HashTable;

unsigned int hash(const char* key, int table_size);
HashTable* create_table(int size);
void ht_insert(HashTable* table, const char* word);
void free_table(HashTable* table);

#endif // _HASHTABLE_H
