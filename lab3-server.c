// ==============================
// School: Central Washington University
// Course: CS470 Operating Systems
// Instructor: Dr. Szilárd VAJDA
// Student: Andrew Dunn
// Assignment: Lab 3
// Description: Example program demonstrating 
// multi theading and sockets from the server side.
// Supports multiple simultaneous client connections.
// Each client connection is handled in it's own thread.
// ==============================
//
// Build and Run instructions:
//
// Compiled and tested on Ubuntu 18.10 with GCC 8.2.0
//
// To build, open a terminal and execute:
// gcc -o server lab3-server.c -pthread
//
// To run, open a terminal and execute:
// ./server
//
// Optional command line parameters:
// ./server [seat map rows] [seat map columns]
//
// ==============================
//
// Usage:
//
// If you want to specify a specific seat map size,
// provide the number of rows and columns as command
// line arguemnts. Default size is 5x5.
//
// By default the server will not accept more than 5
// simultaneous client connections. You can change this
// value to whatever by editing MAX_CONNECTIONS below.
// ==============================

#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <stdarg.h>
#include <netinet/in.h> 
#include <pthread.h>
#include <string.h> 
#include "seatmap.h"
#include "networkmsg.h"
#include "threadsafeprint.h"

#define DEFAULT_SEATS_ROWS 5 // Default size of seat map rows
#define DEFAULT_SEATS_COLS 5 // Default size of seat map columns
#define MAX_SEATS_ROWS 25    // Max allowed size of seat map rows
#define MAX_SEATS_COLS 25    // Max allowed size of seat map columns

#define PORT 5432            // Listening port for server
#define MAX_CONNECTIONS 5    // Max number of allowed connected clients
#define MSG_BUFFER_SIZE 1024 // Size of network messages buffer

// Enums for different client connection status
#define CLIENT_STATUS_NONE 0
#define CLIENT_STATUS_ACTIVE 1
#define CLIENT_STATUS_DISCONNECT 2

// Stores information related to a single client
typedef struct clientInfo_ {
    pthread_t thread;
    int status;
    int socket;
} clientInfo;

// Global variables because this is just an example program.
seatMap* seatsMap = NULL;
clientInfo clientPool[MAX_CONNECTIONS];
unsigned int numConnections = 0;
unsigned int serverRunning = 0;
int server_fd = 0;

pthread_mutex_t socketLock;

// Helper function that will automatically exit the server
// if returnVal is non-zero
int exitOnError(int returnVal, char* errMsg) {
    if (returnVal != 0) 
    { 
        serverRunning = 0;
        perror(errMsg); 
        exit(1);
    }

    return returnVal; 
}

// Initializes the client connection pool
void initclientPool()
{
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        clientPool[i].thread = 0;
        clientPool[i].status = CLIENT_STATUS_NONE;
        clientPool[i].socket = 0;
    }
}

// Helper function that sends a response id and resonse message to a client
void sendMsgResponse(int socket, int msgId, char* msgBody, char* sendBuffer)
{
    sprintf(sendBuffer, "%d", msgId);
    strcat(sendBuffer, NETWORK_MSG_DELIM);
    strcat(sendBuffer, msgBody);
    send(socket, sendBuffer, strlen(sendBuffer), 0);
}

// Checks if all seats have been sold, and disconnects all clients if so
void checkSeatsFull(char* sendBuffer)
{
    if (getNumSeatsAvailable(seatsMap) <= 0)
    {
        printFromHost("All seats have been sold. Disconnecting clients ...");

        for (int i = 0; i < MAX_CONNECTIONS; i++)
        {
            if (clientPool[i].status == 1)
            {
                sendMsgResponse(clientPool[i].socket, SERVER_DISCONNECT, "No more seats available.", sendBuffer);
            }
        }

        shutdown(server_fd, SHUT_RDWR); // Forces server to stop accepting/blocking for new connections
        serverRunning = 0;
    }
}

// Processes a message recieved from a client
int processClientMsg(int clientIndex, char* msg, int msgSize, char* sendBuffer)
{
    clientInfo* cInfo = &(clientPool[clientIndex]);

    printFromClient(clientIndex, "Processing message. Data = '%s'", msg);

    char intStr[20];
    char *saveptr;
    char* sRow;
    char* sCol;
    char* reqIdStr = strtok_r(msg, NETWORK_MSG_DELIM, &saveptr); // strtok_r is threadsafe
    int row, col, taken;
    int reqId = atoi(reqIdStr);

    // All network messages start with a reqest id
    switch (reqId)
    {
        case CLIENT_DISCONNECT:
            printFromClient(clientIndex, "Client requested disconnection.");
            cInfo->status = CLIENT_DISCONNECT;
            sendMsgResponse(cInfo->socket, SERVER_DISCONNECT,"Client requested disconnection.", sendBuffer);
            break;
        case CLIENT_TICKET_REQUESTAVAILABILITY:
            printFromClient(clientIndex, "Client requested ticket availability. Sending response.");

            sprintf(sendBuffer, "%d", SERVER_TICKET_RANGE);
            strcat(sendBuffer, NETWORK_MSG_DELIM);
            
            sprintf(intStr, "%d", getSeatRows(seatsMap));
            strcat(sendBuffer, intStr);
            strcat(sendBuffer, NETWORK_MSG_DELIM);

            sprintf(intStr, "%d", getSeatCols(seatsMap));
            strcat(sendBuffer, intStr);
            strcat(sendBuffer, NETWORK_MSG_DELIM);

            sprintf(intStr, "%d", getNumSeatsAvailable(seatsMap));
            strcat(sendBuffer, intStr);
            send(cInfo->socket, sendBuffer, strlen(sendBuffer), 0);
            break;
        case CLIENT_TICKET_REQUESTSTATUS:
            printFromClient(clientIndex, "Client requested ticket status.");

            sRow = strtok_r(NULL, NETWORK_MSG_DELIM, &saveptr);
            if (sRow == NULL)
            {
                printFromClient(clientIndex, "Client request is missing Row arg.");
                sendMsgResponse(cInfo->socket, SERVER_TICKET_INVALID,"Missing row argument", sendBuffer);
                break;
            }

            sCol = strtok_r(NULL, NETWORK_MSG_DELIM, &saveptr);
            if (sCol == NULL)
            {
                printFromClient(clientIndex, "Client request is missing Row arg.");
                sendMsgResponse(cInfo->socket, SERVER_TICKET_INVALID, "Missing column argument", sendBuffer);
                break;
            }
            
            row = atoi(sRow);
            col = atoi(sCol);
            taken = seatSold(seatsMap, row, col);
            if (taken == -1)
            {
                printFromClient(clientIndex, "Ticket Row/Col is invalid. (row: %2d, col: %2d)", row, col);
                sendMsgResponse(cInfo->socket, SERVER_TICKET_INVALID, "Invalid row or column", sendBuffer);
            }
            else if (taken == 0)
            {
                printFromClient(clientIndex, "Sending response. Is Available (row: %2d, col: %2d)", row, col);
                sprintf(sendBuffer, "%d", SERVER_TICKET_AVAILABLE);
                send(cInfo->socket, sendBuffer, strlen(sendBuffer), 0);
            }
            else
            {
                printFromClient(clientIndex, "Sending response. Not Available (row: %2d, col: %2d)", row, col);
                sprintf(sendBuffer, "%d", SERVER_TICKET_NOT_AVAILABLE);
                send(cInfo->socket, sendBuffer, strlen(sendBuffer), 0);
            }
            break;
        case CLIENT_TICKET_REQUESTPURCHASE:
            printFromClient(clientIndex, "Client requested ticket purchase.");

            sRow = strtok_r(NULL, NETWORK_MSG_DELIM, &saveptr);
            if (sRow == NULL)
            {
                printFromClient(clientIndex, "Client request is missing Row arg.");
                sendMsgResponse(cInfo->socket, SERVER_TICKET_INVALID, "Missing row argument", sendBuffer);
                break;
            }

            sCol = strtok_r(NULL, NETWORK_MSG_DELIM, &saveptr);
            if (sCol == NULL)
            {
                printFromClient(clientIndex, "Client request is missing Row arg.");
                sendMsgResponse(cInfo->socket, SERVER_TICKET_INVALID, "Missing column argument", sendBuffer);
                break;
            }
            
            row = atoi(sRow);
            col = atoi(sCol);
            int success = buySeat(seatsMap, row, col);
            if (success == -1)
            {
                printFromClient(clientIndex, "Ticket Row/Col is invalid. (row: %2d, col: %2d)", row, col);
                sendMsgResponse(cInfo->socket, SERVER_TICKET_INVALID, "Invalid row or column", sendBuffer);
            }
            else if (success == 0)
            {
                printFromClient(clientIndex, "Ticket Row/Col is already taken. (row: %2d, col: %2d)", row, col);
                sendMsgResponse(cInfo->socket, SERVER_TICKET_TRANSACTION_FAILED, "Ticket already purchased", sendBuffer);
            }
            else
            {
                printFromClient(clientIndex, "Client successfully purchased a ticket. (row: %2d, col: %2d)", row, col);
                sendMsgResponse(cInfo->socket, SERVER_TICKET_TRANSACTION_SUCCESS, "Ticket purchased", sendBuffer);
                printSeatMap(seatsMap);
                checkSeatsFull(sendBuffer); // Closes server if all seats are full
            }
            break;
        default:
            printFromClient(clientIndex, "Message contains an invalid request id: %d", reqId);
            sendMsgResponse(cInfo->socket, SERVER_MSG_INVALID,"Unknown request id", sendBuffer);
            return 1;
    }

    return 0;
}

// Runs the client network request loop, executed in it's own thread
void* serveClient(void* _cIndex)
{
    int clientIndex = (int)_cIndex;
    clientInfo* cInfo = &(clientPool[clientIndex]);
    pthread_t threadId = cInfo->thread;

    char receiveBuffer[MSG_BUFFER_SIZE] = {0};
    char sendBuffer[MSG_BUFFER_SIZE] = {0};
    int bytesRead = 0;

    printFromThread(threadId, "Begin handling requests for Client #%d", clientIndex);

    // Run while client is connected and server is running
    while (serverRunning && cInfo->status == CLIENT_STATUS_ACTIVE)
    {
        // Block until next network message is received from the client
        bytesRead = read(cInfo->socket, receiveBuffer, MSG_BUFFER_SIZE);

        if (bytesRead > 0)
        {
            // Make sure message is null terminated
            receiveBuffer[bytesRead] = '\0';

            printFromThread(threadId, "%d bytes received from Client #%d", bytesRead, clientIndex);
            processClientMsg(clientIndex, receiveBuffer, bytesRead, sendBuffer);
        }
    }

    printFromThread(threadId, "Closing connection for Client #%d", clientIndex);

    pthread_mutex_lock(&socketLock);

    // Close client socket
    cInfo->status = CLIENT_STATUS_NONE;
    shutdown(cInfo->socket, SHUT_RDWR);
    close(cInfo->socket);

    numConnections -= 1;

    pthread_mutex_unlock(&socketLock);

    printFromThread(threadId, "Thread exiting ...");
    return 0;
}

// Finds the next available client in the client pool and
// spins up a new thread to handle all communications.
// If the client pool is full returns 1.
int startClientThread(int socket)
{
    if (numConnections >= MAX_CONNECTIONS) return 1;

    printFromHost("Finding next available socket in pool ...");

    pthread_mutex_lock(&socketLock);

    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (clientPool[i].status == CLIENT_STATUS_NONE)
        {
            printFromHost("Assigning Client #%d to incoming connection.", i);
            clientPool[i].status = CLIENT_STATUS_ACTIVE;
            clientPool[i].socket = socket;
            numConnections += 1;

            int err = pthread_create( &(clientPool[i].thread), NULL, serveClient, (void*)i);
            if (err)
                exitOnError(err, "Unable to create thread");
            else
            {
                err = pthread_detach(clientPool[i].thread);
                if (err)
                    exitOnError(err, "Unable to detach thread");
                else
                    printFromHost("Thread spawned for Client #%d with handle 0x%lx", i, clientPool[i].thread);
            }

            pthread_mutex_unlock(&socketLock); 
            return 0;
        }
    }

    pthread_mutex_unlock(&socketLock); 
    return 2;
}

// Program entry point
int main(int argc, char const *argv[]) 
{
    unsigned int seatMapRows = DEFAULT_SEATS_ROWS;
    unsigned int seatMapCols = DEFAULT_SEATS_COLS;

    // Get seat map rows from command line args
    if (argc > 1)
    {
        seatMapRows = atoi(argv[1]);
        if (seatMapRows == 0)
            seatMapRows = DEFAULT_SEATS_ROWS;
        else if (seatMapRows > MAX_SEATS_ROWS)
            seatMapRows = MAX_SEATS_ROWS;
    }

    // Get seat map cols from command line args
    if (argc > 2)
    {
        seatMapCols = atoi(argv[2]);
        if (seatMapCols == 0)
            seatMapCols = DEFAULT_SEATS_COLS;
        else if (seatMapCols > MAX_SEATS_COLS)
            seatMapCols = MAX_SEATS_COLS;
    }

    int new_socket, valread; 
    struct sockaddr_in address; 
    int opt = 1; 
    int addrlen = sizeof(address); 
    char buffer[1024] = {0}; 

    printFromHost("Creating socket file descriptor ...");

    // Creating socket file descriptor 
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
    { 
        perror("Socket failed"); 
        exit(EXIT_FAILURE); 
    } 
    
    printFromHost("Setting socket options ...");

    // Forcefully attaching socket to the port 8080 
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) 
    { 
        perror("Set socket options failed"); 
        exit(EXIT_FAILURE); 
    } 

    // Set up server address properties
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons( PORT ); 
    
    printFromHost("Binding socket to port ...");

    // Forcefully attach socket to the port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) 
    { 
        perror("Socket bind failed"); 
        exit(EXIT_FAILURE); 
    } 

    printFromHost("Starting to listen for new connections ...");

    if (listen(server_fd, 3) < 0) 
    { 
        perror("Server listen failed"); 
        exit(EXIT_FAILURE); 
    } 

    // Allocate new seat map with the given rows and cols
    seatsMap = createSeatMap(seatMapRows, seatMapCols);
    printSeatMap(seatsMap);
    initclientPool();
    serverRunning = 1;

    // While server is running, keep listening for new connections
    while (serverRunning)
    {
        printFromHost("Waiting for a new connection ...");
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address,  
                       (socklen_t*)&addrlen))<0) 
        { 
            if (serverRunning) perror("Error accepting connection");
            break;
        }

        printFromHost("~~~ New connection established ~~~");

        // Attempt to accept new client
        if (startClientThread(new_socket))
        {
            safePrintLine("Server full, unable to accept more connections. Disconnecting client.");
            sendMsgResponse(new_socket, SERVER_DISCONNECT, "Server full", buffer);
        }
    }

    serverRunning = 0;

    // Close all open client sockets
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (clientPool[i].status == 1)
        {
            // Shutdown all open sockets, which will force the client
            // threads to terminate
            clientPool[i].status = CLIENT_STATUS_DISCONNECT;
            shutdown(clientPool[i].socket, SHUT_RDWR);
        }
    }

    printFromHost("Server exiting ...");
    sleep(1);
    deleteSeatMap(&seatsMap);
    return 0; 
} 