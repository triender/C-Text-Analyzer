#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"

/**
 * @brief Hàm băm chuỗi để sử dụng trong bảng băm.
 * Sử dụng hàm băm djb2, một hàm băm phổ biến và hiệu quả.
 */
unsigned int hash(const char* key, int table_size) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash % table_size;
}

/**
 * @brief Tạo bảng băm với kích thước nhất định.
 * @return Con trỏ đến bảng băm mới được tạo.
 */
HashTable* create_table(int size) {
    HashTable* table = malloc(sizeof(HashTable));
    table->size = size;
    table->entries = calloc(size, sizeof(Entry*)); // calloc khởi tạo tất cả là NULL
    return table;
}

/**
 * @brief Chèn một từ vào bảng băm.
 * Nếu từ đã tồn tại, tăng số lần xuất hiện (count).
 * Nếu không, tạo một mục mới và chèn vào đầu chuỗi liên kết.
 */ 
void ht_insert(HashTable* table, const char* word) {
    unsigned int index = hash(word, table->size);
    Entry* current = table->entries[index];

    // Tìm kiếm trong chuỗi liên kết (chain)
    while (current != NULL) {
        if (strcmp(current->word, word) == 0) {
            current->count++; // Đã có, tăng count
            return;
        }
        current = current->next;
    }

    // Nếu không tìm thấy, tạo entry mới và chèn vào đầu chuỗi
    Entry* new_entry = malloc(sizeof(Entry));
    new_entry->word = strdup(word);
    new_entry->count = 1;
    new_entry->next = table->entries[index];
    table->entries[index] = new_entry;
}

/**
 * @brief Giải phóng bộ nhớ của bảng băm và các mục trong đó.
 * Giải phóng từng từ và mục trong chuỗi liên kết.
 */
void free_table(HashTable* table) {
    for (int i = 0; i < table->size; i++) {
        Entry* entry = table->entries[i];
        while (entry != NULL) {
            Entry* temp = entry;
            entry = entry->next;
            free(temp->word);
            free(temp);
        }
    }
    free(table->entries);
    free(table);
}