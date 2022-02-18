flags = -g -std=gnu11 -Werror -Wall -Wextra -Wpedantic -Wmissing-declarations -Wmissing-prototypes -Wold-style-definition

node: node.c main.c main.h node.h node_states.h node_states.c hash_table.c hash_table.h
	gcc node.c main.c node_states.c hash_table.c hash.c -I ./ -g -o node
