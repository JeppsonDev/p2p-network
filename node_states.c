/**
 * node_states.c
 *
 * The implementation for the state machine representing a network of nodes communicating with each other 
 * through data packets 
 *
 * @author Casper Hägglund, Jesper Byström
 * @date 2021-10-27
 *
 */

#include "node_states.h"
#include <arpa/inet.h>
#include "hash_table.h"
#include <signal.h>

static states Q1_handler(node *args);
static states Q2_handler(node *args);
static states Q3_handler(node *args);
static states Q4_handler(node *args);
static states Q5_handler(node *args);
static states Q6_handler(node *args);
static states Q7_handler(node *args);
static states Q8_handler(node *args);
static states Q9_handler(node *args);
static states Q10_handler(node *args);
static states Q11_handler(node *args);
static states Q12_handler(node *args);
static states Q13_handler(node *args);
static states Q14_handler(node *args);
static states Q15_handler(node *args);
static states Q16_handler(node *args);
static states Q17_handler(node *args);
static states Q18_handler(node *args);
static int read_pdu_type(struct pollfd *fd, int expectedType, socket_buffer *pdu, int protocol);
static void read_udp_pdu(struct pollfd *fd, socket_buffer *buff, int timeout);
static void read_pdu(struct pollfd* fd, socket_buffer *buff, int len, int timeout);
static void accept_predacessor(node *args);
static void save_last_pdu(node *n, void *pdu);
static void clear_buffer(socket_buffer *buffer, int bytes);
static void transfer_entry_range(node *args, int fd, int rangeMin);

state stateMachine[] = {
        {Q1_handler},
        {Q2_handler},
        {Q3_handler},
        {Q4_handler},
        {Q5_handler},
        {Q6_handler},
        {Q7_handler},
        {Q8_handler},
        {Q9_handler},
        {Q10_handler},
        {Q11_handler},
        {Q12_handler},
        {Q13_handler},
        {Q14_handler},
        {Q15_handler},
        {Q16_handler},
        {Q17_handler},
        {Q18_handler}
};

static int shouldClose = 0;

state* node_states_get_state_machine() {
    return stateMachine;
}

static void handle_abort(int sig) {
    shouldClose = 1;
}

/**
 * Handles the state Q1 which initilizes everything
 *
 * @param args
 * @returns Q2
 */
static states Q1_handler(node *args){
    printf("[Q1]\n");

    signal(SIGINT, handle_abort);

    // Connect to tracker
    printf("    Create sockets\n");
    args->sockets[0].fd  = create_socket(SOCK_DGRAM);

    struct STUN_LOOKUP_PDU pkt = {STUN_LOOKUP};

    printf("Send STUN_LOOKUP to tracker\n");
    ssize_t status = sendto(args->sockets[0].fd, &pkt, sizeof(pkt), 0, (struct sockaddr*)&args->tracker, sizeof(args->tracker));

    if(status != 1){
        perror("send");
        exit(EXIT_FAILURE);
    }

    //Successor
    args->sockets[1].fd = create_socket(SOCK_STREAM);

    // Listen to incoming connections
    args->sockets[2].fd = create_socket(SOCK_STREAM);
    *args->listeningPort = listen_socket(args->sockets[2].fd);

    //Predecessor
    args->sockets[3].fd = create_socket(SOCK_STREAM);

    return Q2;
}


/**
 * Handles the state Q2 which connected to tracker and stores local IP
 *
 * @param args
 * @returns Q3
 */
static states Q2_handler(node *args){
    printf("[Q2]\n");
    int type = read_pdu_type(&args->sockets[0], STUN_RESPONSE, &args->socketBuffers[0], UDP);

    struct STUN_RESPONSE_PDU *resp = malloc(sizeof(*resp));
    int len = parse_stun_response(type, args->socketBuffers[0].buffer, resp);

    save_last_pdu(args, resp);
    clear_buffer(&args->socketBuffers[0], len);

    printf("    Init self address\n");

    args->addr->sin_addr.s_addr = resp->address;

    return Q3;
}


/**
 * Handles the state Q3 which fetches nodes from the tracker
 *
 * @param args
 * @returns Q4 or Q7
 */
static states Q3_handler(node *args){
    printf("[Q3]\n");

    struct NET_GET_NODE_PDU u = {NET_GET_NODE};

    printf("    Send NET_GET_NODE to tracker\n");

    sendto(args->sockets[0].fd, &u, sizeof(u), 0, (struct sockaddr*)&args->tracker, sizeof(args->tracker));

    int type = read_pdu_type(&args->sockets[0], NET_GET_NODE_RESPONSE, &args->socketBuffers[0], UDP);
    struct NET_GET_NODE_RESPONSE_PDU *response = malloc(sizeof(*response));
    int len = parse_get_node_response(type, args->socketBuffers[0].buffer, response);

    save_last_pdu(args, response);
    clear_buffer(&args->socketBuffers[0], len);

    if (response->address == 0 && response->port == 0){
        return Q4;
    } else {
        return Q7;
    }
}


/**
 * Handles the state Q4 which initilizes the network size
 *
 * @param args
 * @returns Q6
 */
static states Q4_handler(node *args) {
    printf("[Q4]\n");

    printf("    Initilize table to network size\n");

    args->table = hash_table_create(0, 255);

    return Q6;
}

/**
 * Handles the state Q5 which connects to successor and accepts a predacessor
 *
 * @param args
 * @returns Q6
 */
static states Q5_handler(node *args) {
    printf("[Q5]\n");

    struct NET_JOIN_PDU *lastPdu = args->lastPdu;

    //Connect to successor
    args->successor->sin_family = AF_INET;
    args->successor->sin_addr.s_addr = lastPdu->src_address;
    args->successor->sin_port = lastPdu->src_port;

    printf("    Connecting to %s:%d\n", inet_ntoa(args->successor->sin_addr), ntohs(args->successor->sin_port));

    connect_socket(args->sockets[1].fd, *args->successor);

    //Send NET_JOIN_RESPONSE
    struct NET_JOIN_RESPONSE_PDU package = {
        NET_JOIN_RESPONSE,
        args->addr->sin_addr.s_addr,
        *args->listeningPort,
        args->table->minHash + args->table->maxHash/2,
        args->table->maxHash
    };

    char buff[NET_JOIN_RESPONSE_BASE_LENGTH];

    serialize_net_join_response_pdu(buff, package);

    ssize_t status = send(args->sockets[1].fd, buff, NET_JOIN_RESPONSE_BASE_LENGTH, 0);

    if (status < 0){
        perror("Sending NET_JOIN_RESPONSE");
        exit(EXIT_FAILURE);
    }

    //Transfer upper half of entry range to successor
    transfer_entry_range(args, args->sockets[1].fd, args->table->maxHash/2);

    //Accept predacessor
    printf("    Accept predacessor\n");
    accept_predacessor(args);

    return Q6;
}

/**
 * Handles the state Q6 which is the main loop of the program, handles multiple states
 *
 * @param args
 * @returns Q6, Q9, Q10, Q12, Q15, Q16, Q17
 */
static states Q6_handler(node *args) {
    printf("[Q6]\n");

    time_t currentTime = time(NULL);

    if(currentTime - args->lastAlive > 5){
        struct NET_ALIVE_PDU pkt = {NET_ALIVE};

        int status = (int)sendto(args->sockets[0].fd, &pkt, sizeof(pkt), 0, (struct sockaddr*)&args->tracker, sizeof(args->tracker));

        if (status <= 0){
            perror("Sending net alive");
            exit(EXIT_FAILURE);
        }

        args->lastAlive = currentTime;
    }

    read_udp_pdu(&args->sockets[0], &args->socketBuffers[0], 500);
    read_pdu(args->sockets + 1, args->socketBuffers + 1, 3, 500);

    for(int i = 0; i < 4; i++) {
        char *buff = args->socketBuffers[i].buffer;
        int len = args->socketBuffers[i].len;

        if(len == 0){
            continue;
        }

        int type = parse_pdu_type(buff);

        switch (type) {
            case VAL_INSERT:
                if(len >= VAL_INSERT_BASE_LENGTH) {
                    int nameLength = ((uint8_t)buff[13]);
                    int expectedLength = VAL_INSERT_BASE_LENGTH + nameLength;

                    if(len >= expectedLength) {
                        expectedLength += (uint8_t)buff[14+nameLength];

                        if(len >= expectedLength) {
                            struct VAL_INSERT_PDU *resp = malloc(sizeof(*resp));
                            parse_val_insert_pdu(type, args->socketBuffers[i].buffer, resp);

                            clear_buffer(&args->socketBuffers[i], expectedLength);

                            save_last_pdu(args, resp);

                            return Q9;
                        }
                    }
                }
                break;
            case VAL_REMOVE:
                if(len >= VAL_REMOVE_BASE_LENGTH) {
                    struct VAL_REMOVE_PDU *resp = malloc(VAL_REMOVE_BASE_LENGTH);
                    parse_val_remove_pdu(type, args->socketBuffers[i].buffer, resp);

                    clear_buffer(&args->socketBuffers[i], VAL_REMOVE_BASE_LENGTH);

                    save_last_pdu(args, resp);

                    return Q9;
                }
                break;
            case VAL_LOOKUP:
                if(len >= VAL_LOOKUP_BASE_LENGTH) {
                    struct VAL_LOOKUP_PDU *resp = malloc(sizeof(*resp));
                    parse_val_lookup_pdu(type, args->socketBuffers[i].buffer, resp);

                    clear_buffer(&args->socketBuffers[i], VAL_LOOKUP_BASE_LENGTH);

                    save_last_pdu(args, resp);

                    args->lastPdu = resp;

                    return Q9;
                }
                break;
            case NET_NEW_RANGE:
                if(len >= NET_NEW_RANGE_BASE_LENGTH) {
                    struct NET_NEW_RANGE_PDU *resp = malloc(NET_NEW_RANGE_BASE_LENGTH);
                    parse_net_new_range(type, args->socketBuffers[i].buffer, resp);

                    clear_buffer(&args->socketBuffers[i], NET_NEW_RANGE_BASE_LENGTH);

                    save_last_pdu(args, resp);

                    return Q15;
                }
                break;

            case NET_LEAVING:
                if(len >= NET_LEAVING_BASE_LENGTH) {
                    struct NET_LEAVING_PDU *resp = malloc(sizeof(*resp));
                    parse_net_leaving(type, args->socketBuffers[i].buffer, resp);

                    clear_buffer(&args->socketBuffers[i], NET_LEAVING_BASE_LENGTH);

                    save_last_pdu(args, resp);

                    return Q16;
                }

            case NET_CLOSE_CONNECTION:
                clear_buffer(&args->socketBuffers[i], NET_CLOSE_CONNECTION_BASE_LENGTH);
                if(len >= NET_CLOSE_CONNECTION_BASE_LENGTH) {
                    return Q17;
                }
                break;

            case NET_JOIN:
                if (len >= NET_JOIN_BASE_LENGTH){
                    struct NET_JOIN_PDU *response = malloc(sizeof(*response));
                    int packetLen = parse_net_join(type, args->socketBuffers[i].buffer, response);

                    save_last_pdu(args, response);

                    clear_buffer(&args->socketBuffers[i], packetLen);

                    return  Q12;
                }
                break;
            default:
                printf("    What PDU is this?\n");
                clear_buffer(&args->socketBuffers[i], 1);
                break;
        }

    }

    if(shouldClose == 1) {
        return Q10;
    }

    return Q6;
}

/**
 * Handles the state Q7 which joins a network and accepts a predecessor
 *
 * @param args
 * @returns Q8
 */
static states Q7_handler(node *args){
    printf("[Q7]\n");

    struct NET_GET_NODE_RESPONSE_PDU *response = args->lastPdu;

    struct NET_JOIN_PDU pkt = {
            NET_JOIN,
            response->address,
            *args->listeningPort,
            0,
            0,
            0
    };

    struct sockaddr_in add = {};
    add.sin_family = AF_INET;
    add.sin_addr.s_addr = response->address;
    add.sin_port = response->port;

    char bytes[14];

    serialize_net_join_pdu(bytes, pkt);

    printf("    Send NET_JOIN to node in NET_GET_RESPONSE\n");

    int result = (int)sendto(args->sockets[0].fd, &bytes, 14, 0, (struct sockaddr*)&add, sizeof(add));

    if(result < 1) {
        perror("Q7_handler_send");
        exit(EXIT_FAILURE);
    }

    //Accept predacessor
    printf("    Accept predacessor\n");
    accept_predacessor(args);

    //Parse response
    int type = read_pdu_type(&args->sockets[3], NET_JOIN_RESPONSE, &args->socketBuffers[3], TCP);
    struct NET_JOIN_RESPONSE_PDU *resp = malloc(sizeof(struct NET_JOIN_RESPONSE_PDU));
    int len = parse_net_join_response(type, args->socketBuffers[3].buffer, resp);

    save_last_pdu(args, resp);

    clear_buffer(&args->socketBuffers[3], len);

    return Q8;
}

/**
 * Handles the state Q8 which connects to a successor
 *
 * @param args
 * @returns Q6
 */
static states Q8_handler(node *args) {
    printf("[Q8]\n");

    struct NET_JOIN_RESPONSE_PDU *resp = args->lastPdu;

    printf("    Initilize table\n");

    args->table = hash_table_create(resp->range_start, resp->range_end);

    printf("    Connect to successor\n");
    args->successor->sin_family = AF_INET;
    args->successor->sin_addr.s_addr = resp->next_address;
    args->successor->sin_port = resp->next_port;

    connect_socket(args->sockets[1].fd, *args->successor);

    return Q6;
}

/**
 * Handles the state Q9 which handles INSERT/REMOVE/LOOKUP operations
 *
 * @param args
 * @returns Q6
 */
static states Q9_handler(node *args){
    printf("[Q9]\n");

    int type = *(uint8_t *)args->lastPdu;

    if (type == VAL_INSERT) {
        printf("    Inserting hash table entry\n");
        struct VAL_INSERT_PDU *pdu = args->lastPdu;
        hash_table_entry *entry = hash_table_create_entry((char*)(&pdu->ssn[0]), (char*)pdu->name, (char*)pdu->email);
        int status = hash_table_insert(args->table, entry);

        if(status != 0) {
            printf("    Outside the hash range. Forwarding VAL_INSERT\n");

            int packetLen = VAL_INSERT_BASE_LENGTH + pdu->name_length + pdu->email_length;

            char bytes[packetLen];

            serialize_val_insert_pdu(bytes, *pdu);

            send(args->sockets[1].fd, bytes, packetLen, 0);
        }
        else {
            printf("    Insert {ssn: %.12s name: %s email: %s}\n", entry->ssn, entry->name, entry->email);
        }

        free(pdu->name);
        free(pdu->email);

    } else if(type == VAL_LOOKUP) {
        printf("    Looking up hash table entry\n");
        struct VAL_LOOKUP_PDU *pdu = args->lastPdu;

        struct VAL_LOOKUP_RESPONSE_PDU response = {
                VAL_LOOKUP_RESPONSE,
                0,
                0,
                0,
                0,
                0,
        };

        hash_table_entry entry = {NULL, NULL, NULL};
        int status = hash_table_lookup(args->table, (char*)pdu->ssn, &entry);

        if(status < 0) {
            printf("    Send to next\n");

            char buff[VAL_LOOKUP_BASE_LENGTH];

            serialize_val_lookup_pdu(buff, *pdu);

            send(args->sockets[1].fd, &buff, VAL_LOOKUP_BASE_LENGTH, 0);
            return Q6;
        }

        if(entry.ssn != NULL){
            response.name_length = strlen(entry.name);
            response.email_length = strlen(entry.email);

            for(int i = 0; i < 12; i++){
                response.ssn[i] = entry.ssn[i];
            }

            response.email = calloc(response.email_length + 1, sizeof(char));
            strcpy((char*)response.email, entry.email);
            response.name = calloc(response.name_length + 1, sizeof(char));
            strcpy((char*)response.name, entry.name);
        }

        char buff[VAL_LOOKUP_RESPONSE_BASE_LENGTH + response.email_length + response.name_length];

        int len = serialize_val_lookup_response_pdu(buff, response);

        printf("    VAL_LOOKUP_RESPONSE_PDU\n");
        printf("        response.type = %d\n", response.type);
        printf("        response.name_length = %d\n", response.name_length);
        printf("        response.email_length = %d\n", response.email_length);
        printf("        response.ssn = %.12s, expected = %.12s\n", response.ssn, entry.ssn);
        printf("        response.email = %s, expected = %s\n", response.email, entry.email);
        printf("        response.name = %s, expected = %s\n", response.name, entry.name);

        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = pdu->sender_address;
        addr.sin_port = pdu->sender_port;

        sendto(args->sockets[0].fd, &buff, len, 0, (struct sockaddr*)&addr, sizeof(addr));


        if (entry.ssn != NULL){
            free(response.name);
            free(response.email);
        }

    } else if (type == VAL_REMOVE) {
        printf("    Removing hash table entry\n");
        struct VAL_REMOVE_PDU *pdu = args->lastPdu;
        int status = hash_table_remove(args->table, (char*)pdu->ssn);

        if(status != 0) {
            printf("    Send to next\n");

            char buff[VAL_REMOVE_BASE_LENGTH];

            serialize_val_remove_pdu(buff, *pdu);

            send(args->sockets[1].fd, &buff, VAL_REMOVE_BASE_LENGTH, 0);
        }
    }

    return Q6;
}

/**
 * Handles the state Q10 which redirects to Q11 or exits the program
 *
 * @param args
 * @returns Q6
 */
static states Q10_handler(node *args){
    printf("[Q10]\n");

    if(args->table->minHash == 0 && args->table->maxHash == 255) {
        printf("    No one is connected, exiting\n");
        return EXIT;
    }

    return Q11;
}

/**
 * Handles the state Q11 which distributes a new range to successor or predecessor
 *
 * @param args
 * @returns Q18
 */
static states Q11_handler(node *args){
    printf("[Q11]\n");

    printf("    Send NET_NEW_RANGE to successor\n");

    struct NET_NEW_RANGE_PDU pdu = {
        NET_NEW_RANGE,
        args->table->minHash,
        args->table->maxHash
    };

    char buff[NET_NEW_RANGE_BASE_LENGTH];

    serialize_net_new_range_pdu(buff, pdu);

    int socket = 1;

    if(args->table->minHash != 0) {
        socket = 3;
    }

    int status = (int)send(args->sockets[socket].fd, buff, NET_NEW_RANGE_BASE_LENGTH, 0);

    if (status < 0){
        perror("[Q11] Sending response");
        exit(EXIT_FAILURE);
    }

    clear_buffer(&args->socketBuffers[socket], args->socketBuffers[socket].len);

    printf("    Read NET_NEW_RANGE_RESPONSE from successor\n");

    read_pdu_type(&args->sockets[socket], NET_NEW_RANGE_RESPONSE, &args->socketBuffers[socket], TCP);

    return Q18;
}


/**
 * Handles the state Q12 which redirects to either Q5, Q13 or Q14
 *
 * @param args
 * @returns Q5, Q13, Q14
 */
static states Q12_handler(node *args){
    printf("[Q12]\n");

    struct NET_JOIN_PDU *resp = args->lastPdu;

    if (args->table->minHash == 0 && args->table->maxHash == 255){
        printf("    No node connected, moving to Q5\n");
        return Q5;
    }

    if (resp->max_address == args->addr->sin_addr.s_addr && resp->max_port == *args->listeningPort){
        printf("    I am max. Moving to Q13\n");
        return Q13;
    }

    printf("    I am not max. Moving to Q14\n");

    return Q14;
}

/**
 * Handles the state Q13 which connects to a prospect coming from a NET_JOIN
 *
 * @param args
 * @returns Q6
 */
static states Q13_handler(node *args){
    printf("[Q13]\n");

    struct NET_JOIN_PDU *lastPdu = args->lastPdu;

    //Store successor addr
    struct sockaddr_in succ_add = {};
    succ_add.sin_addr.s_addr = args->successor->sin_addr.s_addr;
    succ_add.sin_port = args->successor->sin_port;

    //Send NET_CLOSE_CONNECTION to successor
    struct NET_CLOSE_CONNECTION_PDU pdu = {
        NET_CLOSE_CONNECTION
    };

    send(args->sockets[1].fd, &pdu, NET_CLOSE_CONNECTION_BASE_LENGTH, 0);
    close(args->sockets[1].fd);

    args->sockets[1].fd = create_socket(SOCK_STREAM);

    //Connect to prospect
    args->successor->sin_addr.s_addr = lastPdu->src_address;
    args->successor->sin_port = lastPdu->src_port;

    connect_socket(args->sockets[1].fd, *args->successor);

    int min = (args->table->maxHash - args->table->minHash)/2 + args->table->minHash;

    //Send NET_JOIN_RESPONSE to prospect
    struct NET_JOIN_RESPONSE_PDU respPdu = {
            NET_JOIN_RESPONSE,
            succ_add.sin_addr.s_addr,
            succ_add.sin_port,
            min,
            args->table->maxHash
    };

    char bytes[NET_JOIN_RESPONSE_BASE_LENGTH];
    serialize_net_join_response_pdu(bytes, respPdu);

    send(args->sockets[1].fd, &bytes, NET_JOIN_RESPONSE_BASE_LENGTH, 0);

    //Transfer upper half of entry range to successor
    transfer_entry_range(args, args->sockets[1].fd, min);

    return Q6;
}


/**
 * Handles the state Q14 which updates max parameters
 *
 * @param args
 * @returns Q6
 */
static states Q14_handler(node *args){
    printf("[Q14]\n");
    struct NET_JOIN_PDU *lastPdu = args->lastPdu;

    if(hash_table_get_span(args->table) > lastPdu->max_span) {
        //We are the max
        printf("    We are the new max, updating NET_JOIN max fields\n");

        lastPdu->max_span = hash_table_get_span(args->table);
        lastPdu->max_address = args->addr->sin_addr.s_addr;
        lastPdu->max_port = *args->listeningPort;
    }

    char bytes[NET_JOIN_BASE_LENGTH];
    serialize_net_join_pdu(bytes, *lastPdu);

    int result = (int)send(args->sockets[1].fd, &bytes, NET_JOIN_BASE_LENGTH, 0);

    if(result == -1) {
        perror("[Q14] send");
        exit(EXIT_FAILURE);
    }

    printf("    Return to Q6\n");
    return Q6;
}

/**
 * Handles the state Q15 which updates the hash range
 *
 * @param args
 * @returns Q6
 */
static states Q15_handler(node *args) {
    printf("[Q15]\n");

    struct NET_NEW_RANGE_PDU *lastPdu = args->lastPdu;

    printf("    Update hash range {range_start:%d, range_end:%d}, got {minHash:%d, maxHash:%d}\n", lastPdu->range_start, lastPdu->range_end, args->table->minHash, args->table->maxHash);

    int min = lastPdu->range_start;
    if (args->table->minHash < min){
        min = args->table->minHash;
    }

    int max = lastPdu->range_end;
    if (args->table->maxHash > max){
        max = args->table->maxHash;
    }

    printf("    New table range: [%d:%d]\n", min, max);

    struct NET_NEW_RANGE_RESPONSE_PDU resp = {
        NET_NEW_RANGE_RESPONSE
    };

    if(args->table->maxHash != 255 && lastPdu->range_start == args->table->maxHash+1) {
        printf("    Sent response to successor\n");
        send(args->sockets[1].fd, &resp, NET_NEW_RANGE_RESPONSE_BASE_LENGTH, 0);
    } else{
        printf("    Sent response to predacessor\n");
        send(args->sockets[3].fd, &resp, NET_NEW_RANGE_RESPONSE_BASE_LENGTH, 0);
    }

    args->table = hash_table_resize(args->table, min, max);

    return Q6;
}

/**
 * Handles the state Q16 which handles a node leaving
 *
 * @param args
 * @returns Q6
 */
static states Q16_handler(node *args) {
    printf("[Q16]\n");

    struct NET_LEAVING_PDU *lastPdu = args->lastPdu;

    //Disconnect from successor
    printf("    Disconnect from successor\n");
    close(args->sockets[1].fd);
    args->sockets[1].fd = create_socket(SOCK_STREAM);

    if (args->table->minHash != 0 || args->table->maxHash != 255){
        //Connect to new successor
        printf("    Connect to new successor\n");
        args->successor->sin_addr.s_addr = lastPdu->new_address;
        args->successor->sin_port = lastPdu->new_port;
        connect_socket(args->sockets[1].fd, *args->successor);
    }

    //Return to Q6
    return Q6;
}

/**
 * Handles the state Q17 which handles a conenction closing
 *
 * @param args
 * @returns Q6
 */
static states Q17_handler(node *args) {
    printf("[Q17]\n");

    printf("    Disconnect from predacessor\n");
    close(args->sockets[3].fd);
    args->sockets[3].fd = create_socket(SOCK_STREAM);

    if(args->table->minHash == 0 && args->table->maxHash == 255){
        args->predecessor->sin_addr.s_addr = 0;
        args->predecessor->sin_port = 0;
    } else {
        printf("    Accepting new predacessor\n");
        socklen_t addr_length = sizeof(*args->predecessor);
        args->sockets[3].fd = accept(args->sockets[2].fd, (struct sockaddr *)args->predecessor, &addr_length);

        if(args->sockets[3].fd < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
    }

    return Q6;
}

/**
 * Handles the state Q18 which sends out local table data to every other node in the network and disconnects 
 *
 * @param args
 * @returns Q6
 */
static states Q18_handler(node *args) {
    printf("[Q18]\n");

    //Send NET_CLOSE_CONNECTION to successor
    printf("    Send NET_CLOSE_CONNECTION to successor\n");

    if(args->table->minHash != 0) {
        transfer_entry_range(args, args->sockets[3].fd, args->table->minHash);
    } else {
        transfer_entry_range(args, args->sockets[1].fd, args->table->minHash);
    }

    struct NET_CLOSE_CONNECTION_PDU pdu = {
        NET_CLOSE_CONNECTION
    };

    int status = (int)send(args->sockets[1].fd, &pdu, NET_CLOSE_CONNECTION_BASE_LENGTH, 0);

    if (status < 0){
        perror("[Q18] send NET_CLOSE_CONNECTION");
        exit(EXIT_FAILURE);
    }

    //Send NET_LEAVING to predacessor
    printf("    Send NET_LEAVING to predacessor\n");

    struct NET_LEAVING_PDU netLeavingPdu = {
        NET_LEAVING,
        args->successor->sin_addr.s_addr,
        args->successor->sin_port
    };

    char bytes[NET_LEAVING_BASE_LENGTH];

    serialize_net_leaving_pdu(bytes, netLeavingPdu);

    status = (int)send(args->sockets[3].fd, bytes, NET_LEAVING_BASE_LENGTH, 0);

    if (status < 0){
        perror("[Q18] send NET_LEAVING");
        exit(EXIT_FAILURE);
    }

    return EXIT;
}

/**
 * Accepts the predecessor
 *
 * @param args
 * @returns void
 */
static void accept_predacessor(node *args) {
    //Accept predacessor
    socklen_t addr_length = sizeof(*args->predecessor);
    args->sockets[3].fd = accept(args->sockets[2].fd, (struct sockaddr *)args->predecessor, &addr_length);

    if(args->sockets[3].fd < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
}

/**
 * Free the last pdu and stores another pdu in last pdu
 *
 * @param n
 * @param pdu
 * @returns void
 */
static void save_last_pdu(node *n, void *pdu) {
    if(n->lastPdu){
        struct NET_ALIVE_PDU *lastPdu = n->lastPdu;
        if(lastPdu->type == NET_ALIVE) {
            struct VAL_INSERT_PDU *realPdu = n->lastPdu;

            free(realPdu->name);
            free(realPdu->email);
        }

        free(n->lastPdu);
    }
    n->lastPdu = pdu;
}

/**
 * Clears a socket buffer
 *
 * @param buffer
 * @param bytes
 * @returns void
 */
static void clear_buffer(socket_buffer *buffer, int bytes) {
    for(int i = 0; i < buffer->len; i++) {
        if (i + bytes >= buffer->len){
            buffer->buffer[i] = '\0';
        } else{
            buffer->buffer[i] = buffer->buffer[i + bytes];
        }
    }
    buffer->len -= bytes;
    printf("    Clearing buffer bytes: %d, new length: %d\n", bytes, buffer->len);
}

/**
 * Transfers the entry range from one node to another
 *
 * @param args
 * @param fd
 * @param rangeMin
 * @returns void
 */
static void transfer_entry_range(node *args, int fd, int rangeMin) {
    int bucketsLength = (args->table->maxHash - rangeMin) + 1;
    bucket *buckets = hash_table_get_buckets_from(args->table, rangeMin - args->table->minHash);

    for(int i = 0; i < bucketsLength; i++) {
        for(int j = 0; j < buckets[i].length; j++) {

            uint8_t nameLen = strlen(buckets[i].list[j]->name);
            uint8_t emailLength = strlen(buckets[i].list[j]->email);

            struct VAL_INSERT_PDU pdu = {
            };

            pdu.type = VAL_INSERT;

            for(int k = 0; k < SSN_LENGTH; k++) {
                pdu.ssn[k] = buckets[i].list[j]->ssn[k];
            }

            pdu.name_length = nameLen;

            pdu.name = calloc(pdu.name_length, sizeof(char));

            for(int k = 0; k < nameLen; k++) {
                pdu.name[k] = buckets[i].list[j]->name[k];
            }

            pdu.email_length = emailLength;

            pdu.email = calloc(pdu.email_length, sizeof(char));

            for(int k = 0; k < emailLength; k++) {
                pdu.email[k] = buckets[i].list[j]->email[k];
            }

            char bytes[VAL_INSERT_BASE_LENGTH + nameLen + emailLength];

            int len = serialize_val_insert_pdu((char*)&bytes, pdu);

            send(fd, &bytes, len, 0);

            hash_table_remove(args->table, buckets[i].list[j]->ssn);
        }
    }

    if(rangeMin > args->table->minHash) {
        args->table = hash_table_resize(args->table, args->table->minHash, rangeMin-1);
    }else {
        hash_table_destroy(args->table);
        args->table = NULL;
    }
}


/**
 * Reads the PDU and returns it's type, storing the data in the buffer
 *
 * @param fd
 * @param expectedType
 * @param pdu
 * @param protocol
 * @returns the type
 */
static int read_pdu_type(struct pollfd *fd, int expectedType, socket_buffer *pdu, int protocol) {
    int type;
    do {
        if(protocol == TCP) {
            read_pdu(fd, pdu, 1, -1);
        } else {
            read_udp_pdu(fd, pdu, -1);
        }
        type = parse_pdu_type(pdu->buffer);
    } while(type != expectedType);

    return type;
}

/**
 * Reads the PDU from the UDP, storing the data in the buffer
 *
 * @param fd
 * @param buff
 * @param timeout
 * @returns void
 */
static void read_udp_pdu(struct pollfd *fd, socket_buffer *buff, int timeout) {
    if(poll(fd, 1, timeout) < -1){
        perror("poll");
        exit(1);
    }

    struct sockaddr_in add = {};
    add.sin_family = AF_INET;

    if(fd->revents & POLLIN){
        while(1) {

            socklen_t l = sizeof(struct sockaddr_in);

            int readBytes = (int)recvfrom(fd->fd, buff->buffer + buff->len, BUFF_SIZE - buff->len, MSG_DONTWAIT | MSG_PEEK, (struct sockaddr*)&add, &l);

            if(readBytes < 0 && !(errno == EWOULDBLOCK || errno == ENOTCONN)){
                perror("TCP_READ_PEEK");
                exit(EXIT_FAILURE);
            }

            int result;

            if(readBytes + buff->len >= BUFF_SIZE) {
                break;
            }

            result = (int)recvfrom(fd->fd, buff->buffer + buff->len, BUFF_SIZE - buff->len, MSG_DONTWAIT, (struct sockaddr*)&add, &l);

            if(result == -1) {
                if (errno == EWOULDBLOCK || errno == ENOTCONN) {
                    break;
                } else {
                    perror("TCP_READ");
                    exit(EXIT_FAILURE);
                }
            } else if(result == 0) {
                break;
            } else {
                buff->len += result;
            }
        }
    }
}

/**
 * Reads the PDU from TCP and stores the data in the buffer
 *
 * @param fd
 * @param buff
 * @param len
 * @param timeout
 * @returns void
 */
static void read_pdu(struct pollfd *fd, socket_buffer *buff, int len, int timeout) {

    if(poll(fd, len, 0) < -1){
        perror("poll");
        exit(1);
    }

    socket_buffer *activeBuffers[4] = {};
    struct pollfd activeFd[4] = {};
    int size = 0;
    for(int i = 0; i < len; i++){
        if (!(fd[i].revents & POLLHUP)){
            activeBuffers[size] = &buff[i];
            activeFd[size] = fd[i];
            size++;
        }
    }

    if(poll(activeFd, size, timeout) < -1){
        perror("poll");
        exit(1);
    }

    for(int i = 0; i < size; i++){
        if(activeFd[i].revents & POLLIN){
            while(1) {

                int readBytes = (int)recv(activeFd[i].fd, activeBuffers[i]->buffer + activeBuffers[i]->len, BUFF_SIZE - activeBuffers[i]->len, MSG_DONTWAIT | MSG_PEEK);

                if(readBytes < 0 && !(errno == EWOULDBLOCK || errno == ENOTCONN)){
                    perror("TCP_READ_PEEK");
                    exit(EXIT_FAILURE);
                }

                if(readBytes + activeBuffers[i]->len > BUFF_SIZE) {
                    break;
                }

                int result = (int)recv(activeFd[i].fd, activeBuffers[i]->buffer + activeBuffers[i]->len, BUFF_SIZE - activeBuffers[i]->len, MSG_DONTWAIT);

                if(result == -1) {
                    if (errno == EWOULDBLOCK || errno == ENOTCONN) {
                        break;
                    } else {
                        perror("TCP_READ");
                        exit(EXIT_FAILURE);
                    }
                } else if(result == 0) {
                  break;
                } else {
                    activeBuffers[i]->len += result;
                }
            }
        }
    }
}