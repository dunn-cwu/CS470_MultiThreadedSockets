// ==============================
// School: Central Washington University
// Course: CS470 Operating Systems
// Instructor: Dr. Szilárd VAJDA
// Student: Andrew Dunn
// Assignment: Lab 3
// Description: Example program demonstrating 
// multi theading and sockets from the client side
// ==============================
// Constant defines for the various network msg ids
// used by both the server and client
// ==============================

#ifndef NETWORKMSG_H
#define NETWORKMSG_H

#define NETWORK_MSG_DELIM "|"

#define SERVER_DISCONNECT 1
#define SERVER_MSG_INVALID 2
#define SERVER_TICKET_RANGE 3
#define SERVER_TICKET_INVALID 4
#define SERVER_TICKET_AVAILABLE 5
#define SERVER_TICKET_NOT_AVAILABLE 6
#define SERVER_TICKET_TRANSACTION_FAILED 7
#define SERVER_TICKET_TRANSACTION_SUCCESS 8

#define CLIENT_DISCONNECT 10
#define CLIENT_TICKET_REQUESTAVAILABILITY 11
#define CLIENT_TICKET_REQUESTSTATUS 12
#define CLIENT_TICKET_REQUESTPURCHASE 13

#endif