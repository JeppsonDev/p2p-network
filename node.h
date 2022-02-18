/**
 * node.h
 *
 * This file represents the interface for the node
 *
 * @author Casper Hägglund, Jesper Byström
 * @date 2021-10-26
 *
 */

#ifndef OU3_NODE_H
#define OU3_NODE_H
#include <stdint.h>
#include <netinet/in.h>
#include <pdu.h>
#include "hash_table.h"

/**
 * The data structure for the socket buffer
 */
typedef struct {
    char *buffer;
    int len;
} socket_buffer;

/**
 * The data structure for the node
 */
typedef struct {
    struct sockaddr_in *successor;
    struct sockaddr_in *predecessor;
    struct sockaddr_in tracker;
    struct sockaddr_in *addr;
    int *listeningPort;
    struct pollfd *sockets;
    hash_table *table;
    void *lastPdu;
    socket_buffer *socketBuffers;
    time_t lastAlive;
} node;

int create_socket(int type);
void connect_socket(int st, struct sockaddr_in st_addr);
int parse_pdu_type(const char *bytes);
int parse_stun_response(int type, const char *bytes, struct STUN_RESPONSE_PDU *pdu);
int parse_get_node_response(int type, const char *bytes, struct NET_GET_NODE_RESPONSE_PDU *pdu);
int parse_net_join_response(int type, const char *bytes, struct NET_JOIN_RESPONSE_PDU *pdu);
int parse_net_join(int type, const char *pdu, struct NET_JOIN_PDU *resp);
int parse_net_leaving(int type, const char *pdu, struct NET_LEAVING_PDU *resp);
int parse_net_new_range(int type, const char *pdu, struct NET_NEW_RANGE_PDU *resp);
int parse_val_lookup_pdu(int type, const char *pdu, struct VAL_LOOKUP_PDU *resp);
int parse_val_remove_pdu(int type, const char *pdu, struct VAL_REMOVE_PDU *resp);
int parse_val_insert_pdu(int type, const char *pdu, struct VAL_INSERT_PDU *resp);
void serialize_net_join_pdu(char bytes[], struct NET_JOIN_PDU s);
void serialize_net_join_response_pdu(char bytes[], struct NET_JOIN_RESPONSE_PDU s);
int serialize_val_insert_pdu(char bytes[], struct VAL_INSERT_PDU s);
int serialize_val_lookup_response_pdu(char bytes[], struct VAL_LOOKUP_RESPONSE_PDU s);
int serialize_net_leaving_pdu(char bytes[], struct NET_LEAVING_PDU s);
void serialize_val_lookup_pdu(char bytes[], struct  VAL_LOOKUP_PDU s);
void serialize_val_remove_pdu(char bytes[], struct  VAL_REMOVE_PDU s);
int serialize_net_new_range_pdu(char bytes[], struct NET_NEW_RANGE_PDU s);
int listen_socket(int fd);

#endif
