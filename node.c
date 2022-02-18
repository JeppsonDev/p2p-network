/**
 * node.c
 *
 * This file represents the implementation for the node
 *
 * @author Casper Hägglund, Jesper Byström
 * @date 2021-10-26
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pdu.h>
#include "node.h"

static void serialize_uint32(char bytes[], uint32_t s);
static void serialize_uint16(char bytes[], uint16_t s);
static void serialize_string(char *bytes, char* s, int len);

/**
 * Creates a socket
 *
 * @param type
 * @return the file descriptor for the socket
 */
int create_socket(int type) {
    int st = socket(AF_INET, type, 0);

    if(st == -1) {
        perror("socket");
        exit(1);
    }

    return st;
}

/**
 * Connects a socket to a socket address
 *
 * @param st
 * @param st_addr
 */
void connect_socket(int st, struct sockaddr_in st_addr) {

    int status = connect(st, (struct sockaddr*)&st_addr, sizeof(st_addr));

    if( status == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
}

/**
 * Listens on a socket
 *
 * @param fd
 * @return the port for the socket
 */
int listen_socket(int fd) {
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = 0;
    socklen_t len = (socklen_t)sizeof(address);

    if(bind(fd, (struct sockaddr*)&address, len) < 0){
        perror("binding");
        exit(EXIT_FAILURE);
    }

    if(listen(fd, 1) < 0){
        perror("Listener");
        exit(EXIT_FAILURE);
    }

    if (getsockname(fd, (struct sockaddr*)&address, &len) != 0){
        perror("getSocketName");
        exit(EXIT_FAILURE);
    }

    return address.sin_port;
}

/**
 * Parses the type for a PDU data structure
 *
 * @param bytes
 * @return the type
 */
int parse_pdu_type(const char *bytes) {
    return *(uint8_t*)(bytes);
}

/**
 * Parses a byte array representing the STUN_RESPONSE_PDU data structure
 * @param type
 * @param bytes
 * @param resp
 * @return the length of the packet
 */
int parse_stun_response(int type, const char *bytes, struct STUN_RESPONSE_PDU *resp) {
    resp->type = type;
    resp->address = *(uint32_t*)(bytes+1);
    return STUN_RESPONSE_BASE_LENGTH;
}

/**
 * Parses a byte array representing the NET_GET_NODE_RESPONSE_PDU data structure
 * @param type
 * @param bytes
 * @param resp
 * @return the length of the packet
 */
int parse_get_node_response(int type, const char *bytes, struct NET_GET_NODE_RESPONSE_PDU *resp) {
    resp->type = type;
    resp->address = *(uint32_t*)(bytes+1);
    resp->port = *(uint16_t*)(bytes+5);
    return GET_NODE_RESPONSE_BASE_LENGTH;
}

/**
 * Parses a byte array representing the NET_JOIN_RESPONSE_PDU data structure
 * @param type
 * @param bytes
 * @param resp
 * @return the length of the packet
 */
int parse_net_join_response(int type, const char *pdu, struct NET_JOIN_RESPONSE_PDU *resp){
    resp->type = type;
    resp->next_address = *(uint32_t*)(pdu+1);
    resp->next_port = *(uint16_t*)(pdu+5);
    resp->range_start = *(uint8_t*)(pdu+7);
    resp->range_end = *(uint8_t*)(pdu+8);
    return NET_JOIN_RESPONSE_BASE_LENGTH;
}

/**
 * Parses a byte array representing the NET_JOIN_PDU data structure
 * @param type
 * @param bytes
 * @param resp
 * @return the length of the packet
 */
int parse_net_join(int type, const char *pdu, struct NET_JOIN_PDU *resp) {
    resp->type = type;
    resp->src_address = *(uint32_t*)(pdu+1);
    resp->src_port = *(uint16_t*)(pdu+5);
    resp->max_span = *(uint8_t*)(pdu+7);
    resp->max_address = *(uint32_t *)(pdu+8);
    resp->max_port = *(uint16_t *)(pdu+12);

    return NET_JOIN_BASE_LENGTH;
}

/**
 * Parses a byte array representing the NET_LEAVING_PDU data structure
 * @param type
 * @param bytes
 * @param resp
 * @return the length of the packet
 */
int parse_net_leaving(int type, const char *pdu, struct NET_LEAVING_PDU *resp) {
    resp->type = type;
    resp->new_address = *(uint32_t*)(pdu+1);
    resp->new_port = *(uint16_t*)(pdu+5);
    return NET_LEAVING_BASE_LENGTH;
}

/**
 * Parses a byte array representing the NET_NEW_RANGE_PDU data structure
 * @param type
 * @param bytes
 * @param resp
 * @return the length of the packet
 */
int parse_net_new_range(int type, const char *pdu, struct NET_NEW_RANGE_PDU *resp) {
    resp->type = type;
    resp->range_start = *(uint8_t*)(pdu+1);
    resp->range_end = *(uint8_t*)(pdu+2);

    return NET_NEW_RANGE_BASE_LENGTH;
}

/**
 * Parses a byte array representing the VAL_LOOKUP_PDU data structure
 * @param type
 * @param bytes
 * @param resp
 * @return the length of the packet
 */
int parse_val_lookup_pdu(int type, const char *pdu, struct VAL_LOOKUP_PDU *resp) {
    resp->type = type;
    int offset = 1;

    for(int i = 0; i < SSN_LENGTH; i++){
        resp->ssn[i] = pdu[offset + i];
    }
    offset += SSN_LENGTH;

    resp->sender_address = *(uint32_t*)(pdu+offset);
    offset += 4;

    resp->sender_port = *(uint16_t*)(pdu+offset);
    offset += 2;

    return offset;
}

/**
 * Parses a byte array representing the VAL_REMOVE_PDU data structure
 * @param type
 * @param bytes
 * @param resp
 * @return the length of the packet
 */
int parse_val_remove_pdu(int type, const char *pdu, struct VAL_REMOVE_PDU *resp) {
    resp->type = type;
    int offset = 1;

    for(int i = 0; i < SSN_LENGTH; i++){
        resp->ssn[i] = pdu[offset + i];
    }
    offset += SSN_LENGTH;

    return offset;
}

/**
 * Parses a byte array representing the VAL_INSERT_PDU data structure
 * @param type
 * @param bytes
 * @param resp
 * @return the length of the packet
 */
int parse_val_insert_pdu(int type, const char *pdu, struct VAL_INSERT_PDU *resp) {

    //Type
    resp->type = type;
    int offset = 1;

    //SSN
    for(int i = 0; i < SSN_LENGTH; i++){
        resp->ssn[i] = pdu[offset + i];
    }
    offset += SSN_LENGTH;

    //name_length
    resp->name_length = *(uint8_t*)(pdu+offset);
    offset += 1;

    //name
    resp->name = calloc(resp->name_length + 1, sizeof(char));

    for(int i = 0; i < resp->name_length; i++){
        resp->name[i] = pdu[offset + i];
    }
    //resp->name[resp->name_length] = '\0';
    offset += resp->name_length;

    //email_length
    resp->email_length = *(uint8_t*)(pdu + offset);
    offset += 1;

    //email
    resp->email = calloc(resp->email_length + 1, sizeof(char));

    for(int i = 0; i < resp->email_length; i++){
        resp->email[i] = pdu[offset + i];
    }

    offset += resp->email_length;

    return offset;
}

/**
 * Serializes the data structure NET_JOIN_PDU into a byte array
 *
 * @param bytes
 * @param s
 */
void serialize_net_join_pdu(char bytes[], struct NET_JOIN_PDU s) {
    bytes[0] = s.type;
    serialize_uint32(bytes+1, s.src_address);
    serialize_uint16(bytes+5, s.src_port);
    bytes[7] = s.max_span;
    serialize_uint32(bytes+8, s.max_address);
    serialize_uint16(bytes+12, s.max_port);
}

/**
 * Serializes the data structure NET_JOIN_RESPONSE_PDU into a byte array
 *
 * @param bytes
 * @param s
 */
void serialize_net_join_response_pdu(char bytes[], struct NET_JOIN_RESPONSE_PDU s) {
    bytes[0] = s.type;
    serialize_uint32(bytes+1, s.next_address);
    serialize_uint16(bytes+5, s.next_port);
    bytes[7] = s.range_start;
    bytes[8] = s.range_end;
}

/**
 * Serializes the data structure VAL_INSERT_PDU into a byte array
 *
 * @param bytes
 * @param s
 */
int serialize_val_insert_pdu(char bytes[], struct VAL_INSERT_PDU s) {
    bytes[0] = s.type;

    int offset = 1;

    serialize_string(bytes + offset, s.ssn, SSN_LENGTH);
    offset += SSN_LENGTH;

    bytes[offset] = s.name_length;
    offset += 1;

    serialize_string(bytes + offset, s.name, s.name_length);
    offset += s.name_length;

    bytes[offset] = s.email_length;
    offset += 1;

    serialize_string(bytes + offset, s.email, s.email_length);
    offset += s.email_length;

    return offset;
}

/**
 * Serializes the data structure VAL_LOOKUP_PDU into a byte array
 *
 * @param bytes
 * @param s
 */
void serialize_val_lookup_pdu(char bytes[], struct  VAL_LOOKUP_PDU s) {
    bytes[0] = s.type;

    serialize_string(bytes + 1, s.ssn, SSN_LENGTH);

    serialize_uint32(bytes + SSN_LENGTH + 1, s.sender_address);

    serialize_uint16(bytes + SSN_LENGTH + 5, s.sender_port);
}

/**
 * Serializes the data structure VAL_REMOVE_PDU into a byte array
 *
 * @param bytes
 * @param s
 */
void serialize_val_remove_pdu(char bytes[], struct  VAL_REMOVE_PDU s) {
    bytes[0] = s.type;

    serialize_string(bytes + 1, s.ssn, SSN_LENGTH);
}

/**
 * Serializes the data structure VAL_LOOKUP_RESPONSE_PDU into a byte array
 *
 * @param bytes
 * @param s
 */
int serialize_val_lookup_response_pdu(char bytes[], struct VAL_LOOKUP_RESPONSE_PDU s) {
    bytes[0] = s.type;
    int offset = 1;

    serialize_string(bytes + offset, s.ssn, SSN_LENGTH);
    offset += SSN_LENGTH;

    bytes[offset] = s.name_length;
    offset += 1;

    serialize_string(bytes + offset, s.name, s.name_length);
    offset += s.name_length;

    bytes[offset] = s.email_length;
    offset += 1;

    serialize_string(bytes + offset, s.email, s.email_length);
    offset += s.email_length;

    return offset;
}

/**
 * Serializes the data structure NET_LEAVING_PDU into a byte array
 *
 * @param bytes
 * @param s
 */
int serialize_net_leaving_pdu(char bytes[], struct NET_LEAVING_PDU s) {
    bytes[0] = s.type;
    serialize_uint32(bytes + 1, s.new_address);
    serialize_uint16(bytes + 5, s.new_port);
    return NET_LEAVING_BASE_LENGTH;
}

/**
 * Serializes the data structure NET_NEW_RANGE_PDU into a byte array
 *
 * @param bytes
 * @param s
 */
int serialize_net_new_range_pdu(char bytes[], struct NET_NEW_RANGE_PDU s){
    bytes[0] = s.type;
    bytes[1] = s.range_start;
    bytes[2] = s.range_end;
    return 3;
}

/**
 * Serializes a string
 *
 * @param bytes
 * @param s
 * @param len
 */
static void serialize_string(char *bytes, char* s, int len) {
    for(int i = 0; i < len; i++){
        bytes[i] = s[i];
    }
}

/**
 * Serializes a uint32
 *
 * @param bytes
 * @param s
 */
static void serialize_uint32(char bytes[], uint32_t s) {
    bytes[0] = s & 0xFF;
    bytes[1] = (s >> 8) & 0xFF;
    bytes[2] = (s >> 16) & 0xFF;
    bytes[3] = (s >> 24) & 0xFF;
}

/**
 * Serializes a uint16
 * @param bytes
 * @param s
 */
static void serialize_uint16(char bytes[], uint16_t s) {
    bytes[0] = s & 0xFF;
    bytes[1] = (s >> 8) & 0xFF;
}