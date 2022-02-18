#include <stdio.h>
#include "hash.h"
#include "hash_table.h"

int main(void) {
    //char *ssn = "aaaaabbbbbcc";
    //printf("%d", hash_ssn(ssn));


    hash_table *table = hash_table_create(0,255);

    hash_table_entry *entry = hash_table_create_entry("aaaaabbbbbcc", "Rolf", "rolf@gmail.com");
    if(hash_table_insert(table, entry) == -1 ){
        fprintf(stderr, "inserting element");
        exit(EXIT_FAILURE);
    }

    hash_table_entry *entry2 = hash_table_create_entry("aaabbbbbcccc", "Adam", "adam@gmail.com");
    hash_table_insert(table, entry2);


    hash_table_entry *entry3 = hash_table_create_entry("aaaaaccccccc", "Jerry", "jerry@gmail.com");
    hash_table_insert(table, entry3);

    hash_table_remove(table, "aaaaabbbbbcc");
    table = hash_table_resize(table, 10, 200);
    hash_table_remove(table, "aaaaaccccccc");
    table = hash_table_resize(table, 0, 30);
    hash_table_remove(table, "aaabbbbbcccc");
    table = hash_table_resize(table, 90, 100);
    hash_table_print(table);
    //table = hash_table_resize(table, 20, 30);

    hash_table_destroy(table);
}
