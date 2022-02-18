/**
 * hash_table.h
 *
 * This file represents the interface for the hash table data structure
 *
 * @author Casper Hägglund, Jesper Byström
 * @date 2021-10-26
 *
 */

#ifndef OU3_HASH_TABLE_H
#define OU3_HASH_TABLE_H

#include "hash.h"
#include <stdlib.h>
#include <string.h>

/**
 * A data structure for every hash table entry
 */
typedef struct {
    char* ssn;
    char* name;
    char* email;
} hash_table_entry;

/**
 * A data structure for a bucket part of the hash table data structure
 */
typedef struct {
    hash_table_entry **list;
    int length;
}bucket;

/**
 * A data structure representing the hash table
 */
typedef struct {
    uint8_t minHash;
    uint8_t maxHash;
    bucket *buckets;
} hash_table;

hash_table* hash_table_create(uint8_t min, uint8_t max);
hash_table* hash_table_resize(hash_table *table, uint8_t newMin, uint8_t newMax);
void hash_table_destroy(hash_table *table);
int hash_table_insert(hash_table *table, hash_table_entry *entry);
int hash_table_remove(hash_table *table, char *ssn);
void hash_table_print(hash_table *table);
bucket  *hash_table_get_buckets_from(hash_table *table, uint8_t min);
hash_table_entry *hash_table_create_entry(const char *ssn, char *name, char *email);
int hash_table_get_span(hash_table *table);
int hash_table_lookup(hash_table *table, char* ssn, hash_table_entry *entry);

#endif //OU3_HASH_TABLE_H
