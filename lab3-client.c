// ==============================
// School: Central Washington University
// Course: CS470 Operating Systems
// Instructor: Dr. Szilárd VAJDA
// Student: Andrew Dunn
// Assignment: Lab 3
// Description: Example program demonstrating 
// multi theading and sockets from the client side
// ==============================
//
// Build and Run instructions:
//
// Compiled and tested on Ubuntu 18.10 with GCC 8.2.0
//
// To build, open a terminal and execute:
// gcc -o client lab3-client.c -pthread
//
// To run, open a terminal and execute:
// ./client
//
// Optional command line parameters:
// ./client [settings_file] [-manual | -automatic]
//
// ==============================
//
// Usage:
//
// -manual mode will privide a simple menu allowing
// you to select and purchase seats.
//
// -automatic mode will put the client into an automatic
// loop where it tries to randomly buy seats until the
// server tells us to disconnect.
//
// ==============================

#include <unistd.h>
#include <time.h>
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include "networkmsg.h"
#include "threadsafeprint.h"
#include "iniParser.h"

// Default server information
#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 5432
#define DEFAULT_TIMEOUT 5

// Size of network messages buffer
#define MSG_BUFFER_SIZE 1024

// Global variables because this is just an example program:
char receiveBuffer[MSG_BUFFER_SIZE];
char sendBuffer[MSG_BUFFER_SIZE];
int socketHandle = 0;
int socketStatus = 0;
int manualMode = 1;
int seatRows = 0;
int seatCols = 0;

// Client thread handle and mutex lock
pthread_t clientThread;
pthread_mutex_t socketLock;

// Linebuffer for user terminal input
char* linebuffer = NULL;
size_t lineSize = 0;
ssize_t lineLen = 0;

// Disconnects from the server and closes the socket connection
void disconnectFromServer()
{
    if (!socketStatus)
    {
        pthread_mutex_unlock(&socketLock);
        return;
    }

    safePrintLine("Disconnecting from server ...");
    shutdown(socketHandle, SHUT_RDWR);
    socketStatus = 0;
}

// Takes a c-string message from the server and processes it accordingly
void processServerMsg(char* msg)
{
    printFromThread(clientThread, "Processing server message. Data: '%s'", msg);

    char *saveptr;
    char* msgArgument;
    char* sRow;
    char* sCol;
    char* sAvail;
    char* reqIdStr = strtok_r(msg, NETWORK_MSG_DELIM, &saveptr); // strtok_r is threadsafe
    int reqId = atoi(reqIdStr);
    int avail;

    // Every server message starts with an integer request id
    switch (reqId)
    {
        case SERVER_DISCONNECT:
            msgArgument = strtok_r(NULL, NETWORK_MSG_DELIM, &saveptr);
            printFromThread(clientThread, "Server requested us to disconnect. Reason: %s", msgArgument);
            disconnectFromServer();
            if (manualMode) safePrintLine("~~~ PRESS ENTER TO EXIT FROM MAIN MENU ~~~");
            break;
        case SERVER_MSG_INVALID:
            msgArgument = strtok_r(NULL, NETWORK_MSG_DELIM, &saveptr);
            printFromThread(clientThread, "Server says we sent an invalid message. Reason: %s", msgArgument);
            break;
        case SERVER_TICKET_RANGE:
            printFromThread(clientThread, "Server is telling us the range of available tickets.");

            sRow = strtok_r(NULL, NETWORK_MSG_DELIM, &saveptr);
            if (sRow == NULL)
            {
                printFromThread(clientThread, "Server response is missing row argument.");
                break;
            }

            sCol = strtok_r(NULL, NETWORK_MSG_DELIM, &saveptr);
            if (sCol == NULL)
            {
                printFromThread(clientThread, "Server response is missing column argument.");
                break;
            }

            sAvail = strtok_r(NULL, NETWORK_MSG_DELIM, &saveptr);
            if (sAvail == NULL)
            {
                printFromThread(clientThread, "Server response is missing # available argument.");
                break;
            }

            seatRows = atoi(sRow);
            seatCols = atoi(sCol);
            avail = atoi(sAvail);

            safePrintLine("=========================\n"
                          "|   Available Seating   |\n"
                          "| Rows:    %2d           |\n"
                          "| Columns: %2d           |\n"
                          "| # Available: %2d       |\n"
                          "=========================", seatRows, seatCols, avail);
            break;
        case SERVER_TICKET_INVALID:
            msgArgument = strtok_r(NULL, NETWORK_MSG_DELIM, &saveptr);
            printFromThread(clientThread, "Server says we referenced an invalid ticket. Reason: %s", msgArgument);
            break;
        case SERVER_TICKET_AVAILABLE:
            printFromThread(clientThread, "Server says that ticket is available for us to purchase.");
            break;
        case SERVER_TICKET_NOT_AVAILABLE:
            printFromThread(clientThread, "Server says that ticket is not available for us to purchase.");
            break;
        case SERVER_TICKET_TRANSACTION_FAILED:
            msgArgument = strtok_r(NULL, NETWORK_MSG_DELIM, &saveptr);
            printFromThread(clientThread, "Server says our transaction failed. Reason: %s", msgArgument);
            break;
        case SERVER_TICKET_TRANSACTION_SUCCESS:
            msgArgument = strtok_r(NULL, NETWORK_MSG_DELIM, &saveptr);
            printFromThread(clientThread, "Server says our transaction was a success: %s", msgArgument);
            break;
        default:
            printFromThread(clientThread, "Server sent an unknown request id: %d", reqId);
            break;
    }
}

// Runs the client server message recieve loop which is executed in a separate thread
void* runClient(void *unused)
{
    int bytesRead = 0;
    printFromThread(clientThread, "Client thread is now running and ready to process network messages.");

    // Request available seating info from server
    char intStr[20];
    sprintf(sendBuffer, "%d", CLIENT_TICKET_REQUESTAVAILABILITY);
    send(socketHandle, sendBuffer, strlen(sendBuffer), 0);

    // Continue until we are disconnected
    while (socketStatus)
    {
        // Block until the next server message is received
        bytesRead = read(socketHandle , receiveBuffer, MSG_BUFFER_SIZE); 
        if (bytesRead > 0)
        {
            // Make sure message is null terminated
            receiveBuffer[bytesRead] = '\0';

            pthread_mutex_lock(&socketLock);
            processServerMsg(receiveBuffer);
            pthread_mutex_unlock(&socketLock);
        }
    }

    shutdown(socketHandle, SHUT_RDWR);
    close(socketHandle);

    printFromThread(clientThread, "Client thread is exiting ....");
}

// Creates the client socket, attempts to connect to the server, and then
// spins up a new thread for the network msg receive loop. Returns a non-zero
// value on failure.
int startClient(char* ip, unsigned int port, unsigned int timeoutRetrys)
{
    struct sockaddr_in address;
    struct sockaddr_in serv_addr;

    safePrintLine("Attempting to connect to server %s:%d, %d retrys", ip, port, timeoutRetrys);
    safePrintLine("Creating socket file descriptor ...");

    if ((socketHandle = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        perror("Socket failed");

        pthread_mutex_lock(&socketLock);
        socketStatus = -1;
        pthread_mutex_unlock(&socketLock);

        return EXIT_FAILURE;
    } 

    safePrintLine("Setting socket info ...");

    memset(&serv_addr, '0', sizeof(serv_addr)); 
   
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(port); 
       
    // Convert IPv4 and IPv6 addresses from text to binary form 
    if(inet_pton(AF_INET, ip, &serv_addr.sin_addr)<=0)  
    { 
        perror("Invalid address or address not supported");

        pthread_mutex_lock(&socketLock);
        socketStatus = -1;
        pthread_mutex_unlock(&socketLock);

        return EXIT_FAILURE;
    }

    safePrintLine("Connecting to server ...");

    int cResult = connect(socketHandle, (struct sockaddr *)&serv_addr, sizeof(serv_addr));;
    while (timeoutRetrys > 0 && cResult < 0)
    {  
        // Keep trying to connect until we run out of timout retrys
        safePrintLine("Connection attempt failed. Retry in 1 second.");
        sleep(1);
        timeoutRetrys--;
        cResult = connect(socketHandle, (struct sockaddr *)&serv_addr, sizeof(serv_addr)); 
    }

    if (cResult < 0) 
    { 
        perror("Connection Failed");

        pthread_mutex_lock(&socketLock);
        socketStatus = -1;
        pthread_mutex_unlock(&socketLock);

        return EXIT_FAILURE;
    }

    pthread_mutex_lock(&socketLock);
    socketStatus = 1;
    pthread_mutex_unlock(&socketLock);

    // Spin up client network handling thread
    safePrintLine("Starting client thread ...");
    int err = pthread_create(&clientThread, NULL, runClient, NULL);
    if (err)
    {
        perror("Unable to create client thread");

        pthread_mutex_lock(&socketLock);
        socketStatus = -1;
        pthread_mutex_unlock(&socketLock);

        return EXIT_FAILURE;
    }

    return 0;
}

// Handles the main menu display and input while in manual mode
void showMainMenu()
{
    safePrint("\n_____________________\n"
                "===== Main Menu =====\n"
                "1. Get seating information\n"
                "2. Check ticket availability\n"
                "3. Purchase a ticket\n"
                "4. Disconnect and exit\n\n"
                "Selection: ");

    lineLen = getline(&linebuffer, &lineSize, stdin);

    pthread_mutex_lock(&socketLock);

    // Strip new-line char at end of string
    if (linebuffer[lineLen - 1] == '\n')
        linebuffer[lineLen - 1] = '\0';

    if (strlen(linebuffer) <= 0 || !socketStatus){
        pthread_mutex_unlock(&socketLock);
        return;
    }

    int selection = atoi(linebuffer);
    int row = -1;
    int col = -1;
    char intStr[20];

    // Process user selection
    switch (selection)
    {
        case 1:
            // Request available seating info from server
            safePrintLine("Sending server request ...");
            sprintf(sendBuffer, "%d", CLIENT_TICKET_REQUESTAVAILABILITY);
            send(socketHandle, sendBuffer, strlen(sendBuffer), 0);
            break;
        case 2:
            safePrint("Enter the row and column of the seat you wish to check: ");
            scanf("%d %d", &row, &col);

            // flush stdin
            while ((selection = getchar()) != '\n' && selection != EOF) { }

            safePrintLine("Sending server request ...");
            sprintf(sendBuffer, "%d", CLIENT_TICKET_REQUESTSTATUS);

            strcat(sendBuffer, NETWORK_MSG_DELIM);
            sprintf(intStr, "%d", row);
            strcat(sendBuffer, intStr);

            strcat(sendBuffer, NETWORK_MSG_DELIM);
            sprintf(intStr, "%d", col);
            strcat(sendBuffer, intStr);
            send(socketHandle, sendBuffer, strlen(sendBuffer), 0);
            break;
        case 3:
            safePrint("Enter the row and column of the seat you wish to purchase: ");
            scanf("%d %d", &row, &col);

            // flush stdin
            while ((selection = getchar()) != '\n' && selection != EOF) { }

            safePrintLine("Sending server request ...");
            sprintf(sendBuffer, "%d", CLIENT_TICKET_REQUESTPURCHASE);

            strcat(sendBuffer, NETWORK_MSG_DELIM);
            sprintf(intStr, "%d", row);
            strcat(sendBuffer, intStr);

            strcat(sendBuffer, NETWORK_MSG_DELIM);
            sprintf(intStr, "%d", col);
            strcat(sendBuffer, intStr);
            send(socketHandle, sendBuffer, strlen(sendBuffer), 0);
            break;
        case 4:
            safePrintLine("Sending server request ...");
            sprintf(sendBuffer, "%d", CLIENT_DISCONNECT);
            send(socketHandle, sendBuffer, strlen(sendBuffer), 0);
            disconnectFromServer();
            break;
        default:
            safePrintLine("Error: Invalid selection.");
            break;
    }

    pthread_mutex_unlock(&socketLock);
}

// Executes the manual mode ticket purchasing loop, which
// displays the main menu to the user
void runManualLoop()
{
    struct timespec tim, tim2;
    tim.tv_sec  = 0;
    tim.tv_nsec = 500000000L;

    // Give client thread enough time to spin up (0.5 secs)
    nanosleep(&tim , &tim2);

    while (socketStatus)
    {
        showMainMenu();
        nanosleep(&tim , &tim2); // Give server enough time to respond (0.5 secs)
    }
}

// Called in automatic mode. Attempts to buy a random seat ticket from the server
void buyRandomTicket()
{
    pthread_mutex_lock(&socketLock);

    char intStr[20];
    int row = rand() % seatRows;
    int col = rand() % seatCols;

    safePrintLine("\nBuying random ticket. Row: %2d, Col: %2d", row, col);

    sprintf(sendBuffer, "%d", CLIENT_TICKET_REQUESTPURCHASE);
    strcat(sendBuffer, NETWORK_MSG_DELIM);

    sprintf(intStr, "%d", row);
    strcat(sendBuffer, intStr);
    strcat(sendBuffer, NETWORK_MSG_DELIM);

    sprintf(intStr, "%d", col);
    strcat(sendBuffer, intStr);
    send(socketHandle, sendBuffer, strlen(sendBuffer), 0);

    pthread_mutex_unlock(&socketLock);
}

// Executed in automatic mode, runs a loop that calls buyRandomTicket()
// every half a second
void runAutomaticLoop()
{
    srand(time(NULL));

    struct timespec tim, tim2;
    tim.tv_sec  = 0;
    tim.tv_nsec = 500000000L; // 0.5 seconds

    // Give client thread enough time to spin up (0.5 secs)
    nanosleep(&tim , &tim2);

    while (socketStatus)
    {
        buyRandomTicket();
        nanosleep(&tim , &tim2); // Wait 0.5 seconds between purchase attempts
    }
}

// Attempts to read the ip, port, and timeout settings from the given ini file path
int readIniSettings(const char* filePath, char* ip, unsigned int* port, unsigned int* timeout)
{
    safePrintLine("Reading settings from: %s", filePath);
    
    // Open file
    FILE* fp;
    fp = fopen(filePath, "r");
    if (fp == NULL)
    {
        safePrintLine("Error opening file. Using default settings.");
        return 1;
    }

    char resultBuffer[64];

    // Read ip value
    int ret = getIniValue(fp, "ip", resultBuffer, 64);
    if (ret == 0)
    {
        safePrintLine("Using ip from ini file: '%s'", resultBuffer);
        ip = strcpy(ip, resultBuffer);
    }
    else
    {
        safePrintLine("Error reading ip from ini file. Using default.");
    }

    // Read port value
    ret = getIniValue(fp, "port", resultBuffer, 64);
    if (ret == 0)
    {
        unsigned int p = atoi(resultBuffer);
        safePrintLine("Using port # from ini file: '%d'", p);
        *port = p;
    }
    else
    {
        safePrintLine("Error reading port # from ini file. Using default.");
    }
    
    // Read timout value
    ret = getIniValue(fp, "timeout", resultBuffer, 64);
    if (ret == 0)
    {
        unsigned int t = atoi(resultBuffer);
        safePrintLine("Using timeout # from ini file: '%d'", t);
        *timeout = t;
    }
    else
    {
        safePrintLine("Error reading timeout # from ini file. Using default.");
    }

    fclose(fp);
    return 0;
}

// Program entry point
int main(int argc, char const *argv[]) 
{ 
    char ipAddress[64] = DEFAULT_IP;
    unsigned int port = DEFAULT_PORT;
    unsigned int timeoutRetrys = DEFAULT_TIMEOUT;

    // Process command line arguments
    int curArg = 1;
    while (curArg < argc)
    {
        if (strstr(argv[curArg], "-manual") != NULL)
            manualMode = 1;
        else if (strstr(argv[curArg], "-automatic") != NULL)
            manualMode = 0;
        else
        {
            if (readIniSettings(argv[curArg], ipAddress, &port, &timeoutRetrys) > 0)
            {
                safePrintLine("Unknown or invalid command line arguments.");
                safePrintLine("Correct usage: %s [settings_file] [-manual | -automatic]", argv[0]);
            }
        }
        curArg++;
    }

    // Connect to server and spin up client thread
    int err = startClient(ipAddress, port, timeoutRetrys);
    if (err) return err;

    safePrintLine("Successfully connected to the server.");

    // Run in either manual or automatic mode
    if (manualMode)
        runManualLoop();
    else
        runAutomaticLoop();
    
    pthread_mutex_lock(&socketLock);
    disconnectFromServer();
    pthread_mutex_unlock(&socketLock);

    free(linebuffer);
    linebuffer = NULL;

    safePrintLine("Exiting main thread ...");

    return 0; 
}
