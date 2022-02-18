/**
 * hash_table.c
 *
 * This file represents the implementation of hash table data structure that stores values depending on a
 * hash key.
 *
 * @author Casper Hägglund, Jesper Byström
 * @date 2021-10-26
 *
 */

#include "hash_table.h"
#include <stdio.h>

static int hash_table_lookup_index(bucket *b, const char *ssn);

/**
 *
 * Creates a hash table and returns a pointer to said data structure.
 *
 * Memory is allocated inside the function and needs to be deallocated with hash_table_destroy
 *
 * @param min
 * @param max
 * @return The hash table pointer
 */
hash_table* hash_table_create(uint8_t min, uint8_t max){
    hash_table *table = calloc(1, sizeof(*table));
    table->minHash = min;
    table->maxHash = max;
    table->buckets = calloc(max-min+1, sizeof(*table->buckets));

    if(!table->buckets) {
        perror("table->buckets || calloc");
    }

    for (int i = 0; i < (max-min)+1; i++) {
        table->buckets[i].length = 0;
    }

    return table;
}

/**
 * Resizes the hash table by creating a new hash table and destroying the old one
 *
 * @param table
 * @param newMin
 * @param newMax
 * @return The hash table pointer
 */
hash_table* hash_table_resize(hash_table *table, uint8_t newMin, uint8_t newMax){
    hash_table *newTable = hash_table_create(newMin, newMax);

    int len = newMax - newMin;

    for(int i = 0; i < len; i++){
        if(newMin + i >= table->minHash && newMin + i <= table->maxHash){
            int bucketLen = table->buckets[newMin + i - table->minHash].length;

            newTable->buckets[i].list = calloc(bucketLen, sizeof(newTable->buckets[i].list));

            if(!newTable->buckets[i].list) {
                perror("newTable->buckets[i].list || calloc");
            }

            int oldIndexMapped = newMin + i - table->minHash;
            for(int j = 0; j < bucketLen; j++){

                char* ssn = table->buckets[oldIndexMapped].list[j]->ssn;
                char* name = table->buckets[oldIndexMapped].list[j]->name;
                char* email = table->buckets[oldIndexMapped].list[j]->email;
                newTable->buckets[i].list[j] = hash_table_create_entry(ssn, name, email);
            }
            newTable->buckets[i].length = table->buckets[oldIndexMapped].length;
        }
    }

    hash_table_destroy(table);

    return newTable;
}

/**
 * Frees the hash table and all of its contents
 *
 * @param table
 */
void hash_table_destroy(hash_table *table) {

    int len = table->maxHash - table->minHash;

    for(int i = 0; i < len; i++ ) {
        int bucketLen = table->buckets[i].length;

        for(int j = 0; j < bucketLen; j++) {
            free(table->buckets[i].list[j]->ssn);
            free(table->buckets[i].list[j]->email);
            free(table->buckets[i].list[j]->name);
            free(table->buckets[i].list[j]);
        }
        table->buckets[i].length = 0;
        free(table->buckets[i].list);
    }

    free(table->buckets);
    free(table);
}

/**
 * Inserts a new entry to the hash table
 *
 * @param table
 * @param entry
 * @return a status, -1 means the hash index is outside the hash range and 0 means success.
 */
int hash_table_insert(hash_table *table, hash_table_entry *entry) {
    hash_t hash = hash_ssn(entry->ssn);

    if(hash < table->minHash || hash > table->maxHash) {
        return -1;
    }

    hash_t index = hash - table->minHash;

    int listLen = 0;

    if(table->buckets[index].list != NULL){
        listLen = table->buckets[index].length;
    }

    table->buckets[index].list = realloc(table->buckets[index].list, (listLen+1) * sizeof(hash_table_entry*));

    if(!table->buckets[index].list) {
        perror("table->buckets[index].list || calloc");
    }

    table->buckets[index].length += 1;

    table->buckets[index].list[table->buckets[index].length-1] = entry;

    return 0;
}

/**
 * Removes an entry from the hash table
 *
 * @param table
 * @param ssn
 * @return a status, -1 means the hash index is outside of the hash range. 0 means success
 */
int hash_table_remove(hash_table *table, char *ssn) {
    hash_t hash = hash_ssn(ssn);

    if(hash < table->minHash || hash > table->maxHash) {
        return -1;
    }

    hash_t hashIndex = hash-table->minHash;

    int listIndex = hash_table_lookup_index(&table->buckets[hashIndex], ssn);

    if(listIndex >= 0) {
        free(table->buckets[hashIndex].list[listIndex]->ssn);
        free(table->buckets[hashIndex].list[listIndex]->email);
        free(table->buckets[hashIndex].list[listIndex]->name);
        free(table->buckets[hashIndex].list[listIndex]);

        for(int i = listIndex; i < table->buckets[hashIndex].length - 1; i++) {
            table->buckets[hashIndex].list[i] = table->buckets[hashIndex].list[i+1];
        }

        table->buckets[hashIndex].list = realloc(table->buckets[hashIndex].list, (table->buckets[hashIndex].length - 1) * sizeof(hash_table_entry*));
        table->buckets[hashIndex].length -= 1;
    }

    return 0;
}

/**
 * Prints the hash table
 *
 * @param table
 */
void hash_table_print(hash_table *table) {
    int len = table->maxHash - table->minHash;

    printf("--------------TABLE-------------\n");
    printf("Amount of buckets: %d\n", len);


    for(int i = 0; i < len; i++) {
        int bucketLen = table->buckets[i].length;

        printf("Buckets[%d] length: %d\n", i, bucketLen);

        if(table->buckets[i].list) {
            for(int j = 0; j < bucketLen; j++) {
                printf("Buckets[%d][%d] ssn: %s\n", i, j, table->buckets[i].list[j]->ssn);
                printf("Buckets[%d][%d] email: %s\n", i, j, table->buckets[i].list[j]->email);
                printf("Buckets[%d][%d] name: %s\n", i, j, table->buckets[i].list[j]->name);
            }
        }

    }
    printf("--------------------------------------\n");
}

/**
 * Creates an entry for the hash table
 *
 * @param ssn
 * @param name
 * @param email
 * @return a new entry pointer for the hash table
 */
hash_table_entry *hash_table_create_entry(const char *ssn, char *name, char *email) {
    hash_table_entry *entry = malloc(sizeof(*entry));

    entry->ssn = calloc(12, sizeof(char));
    entry->name = calloc(strlen(name) + 1, sizeof(char));
    entry->email = calloc(strlen(email) + 1, sizeof(char));

    for(int i = 0; i < 12; i++) {
        entry->ssn[i] = ssn[i];
    }

    strcpy(entry->name, name);
    strcpy(entry->email, email);

    return entry;
}

/**
 * Looks up a value in the hash table depending on the ssn
 *
 * @param table
 * @param ssn
 * @param entry
 * @return status, -1 means hash is outside the hash range and 0 means success
 */
int hash_table_lookup(hash_table *table, char* ssn, hash_table_entry *entry){
    hash_t hash = hash_ssn(ssn);

    if (hash < table->minHash || hash > table->maxHash){
        return -1;
    }

    int index = hash_table_lookup_index(&table->buckets[hash - table->minHash], ssn);

    if (index != -1){
        *entry = *table->buckets[hash-table->minHash].list[index];
    }

    return 0;
}

/**
 * Returns buckets in range between min parameter and max hash
 *
 * @param table
 * @param min
 * @return the buckets in range between minparameter and max hash
 */
bucket *hash_table_get_buckets_from(hash_table *table, uint8_t min) {
    return table->buckets + min;
}

/**
 * Returns the span between min and max
 *
 * @param table
 * @return the span between min and max
 */
int hash_table_get_span(hash_table *table) {
    return (table->maxHash - table->minHash) + 1;
}

/**
 * Returns the index for an ssn inside a bucket
 *
 * @param b
 * @param ssn
 * @returnan index for an ssn inside a bucket
 */
static int hash_table_lookup_index(bucket *b, const char *ssn){
    for(int i = 0; i < b->length; i++){
        int found = 0;
        for(int j = 0; j < 12; j++) {
            if(ssn[j] != b->list[i]->ssn[j]) {
                found = -1;
                break;
            }
        }
        if(found == 0) {
            return i;
        }
    }
    return -1;
}