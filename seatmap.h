// ==============================
// School: Central Washington University
// Course: CS470 Operating Systems
// Instructor: Dr. Szilárd VAJDA
// Student: Andrew Dunn
// Assignment: Lab 3
// Description: Example program demonstrating 
// multi theading and sockets from the client side
// ==============================
// Creates, manages, and syncs the seat map between
// different threads
// ==============================

#ifndef SEATMAP_H
#define SEATMAP_H

#include <stdlib.h>
#include <pthread.h>
#include "threadsafeprint.h"

// Created a struct in case I wanted to add more fields later,
// like the buyer's name
typedef struct seatInfo_
{
    int taken;
} seatInfo;

// Stores the seat map array, size, number sold, and a mutex
// for thread sync
typedef struct seatMap_
{
    seatInfo** seatArr;
    unsigned int rows;
    unsigned int cols;
    unsigned int numSold;

    pthread_mutex_t mutex;
} seatMap;

// Allocates and returns a new 2d seatInfo array
seatInfo** _allocSeatsArr(int rows, int cols)
{
    seatInfo** _newSeatArr = (seatInfo**)malloc(sizeof(seatInfo*) * rows);

    for (int y = 0; y < rows; y++)
    {
        _newSeatArr[y] = (seatInfo*)malloc(sizeof(seatInfo) * cols);
        for (int x = 0; x < cols; x++)
        {
            // All seats are initially available to purchase
            _newSeatArr[y][x].taken = 0;
        }
    }

    return _newSeatArr;
}

// Frees memory created for the 2d seatInfo array
// Not thread safe, use freeSeatsData() instead
void _freeSeatsData(seatMap* seats)
{
    if (seats->seatArr == NULL) return;

    for (int y = 0; y < seats->rows; y++)
    {
        free(seats->seatArr[y]);
    }

    free(seats->seatArr);
    seats->seatArr = NULL;
}

// Helper function that ensures thread safety
void freeSeatsData(seatMap* seats)
{
    pthread_mutex_lock(&(seats->mutex));
    _freeSeatsData(seats);
    pthread_mutex_unlock(&(seats->mutex));
}

// Initializes a given seatMap with the specified rows and cols.
// Is thead safe.
void initSeatsData(seatMap* seats, int rows, int cols)
{
    pthread_mutex_lock(&(seats->mutex));

    _freeSeatsData(seats);
    seats->seatArr = _allocSeatsArr(rows, cols);

    seats->rows = rows;
    seats->cols = cols;
    seats->numSold = 0;

    pthread_mutex_unlock(&(seats->mutex));
}

// Deletes a given seatMap from memory
void deleteSeatMap(seatMap** seats)
{
    if (*seats == NULL) return;

    freeSeatsData(*seats);
    free(*seats);

    *seats = NULL;
}

// Allocates and returns a new seatMap
seatMap* createSeatMap(int rows, int cols)
{
    seatMap* newSeats = malloc(sizeof(seatMap));
    initSeatsData(newSeats, rows, cols);

    return newSeats;
}

// Returns the number of rows in the given seat map
// while being thread safe
unsigned int getSeatRows(seatMap* seats)
{
    pthread_mutex_lock(&(seats->mutex));
    unsigned int retVal = seats->rows;
    pthread_mutex_unlock(&(seats->mutex));

    return retVal;
}

// Sets the number of rows in the given seat map
// while being thread safe. resizes the seat map
// array automatically.
void setSeatRows(seatMap* seats, int newRows)
{
    if (seats->rows == newRows) return;

    pthread_mutex_lock(&(seats->mutex));

    // Reallocate seat array if necessary
    if (seats->seatArr != NULL)
    {
        seatInfo** _newSeatArr = _allocSeatsArr(newRows, seats->cols);
        
        for (int y = 0; y < seats->rows && y < newRows; y++) {
            for (int x = 0; x < seats->cols; x++) {
                _newSeatArr[y][x] = seats->seatArr[y][x];
            }
        }

        _freeSeatsData(seats);
        seats->seatArr = _newSeatArr;
    }

    seats->rows = newRows;

    pthread_mutex_unlock(&(seats->mutex));
}

// Returns the number of columns in the given seat map
// while being thread safe
unsigned int getSeatCols(seatMap* seats)
{
    pthread_mutex_lock(&(seats->mutex));
    unsigned int retVal = seats->cols;
    pthread_mutex_unlock(&(seats->mutex));

    return retVal;
}

// Sets the number of cols in the given seat map
// while being thread safe. resizes the seat map
// array automatically.
void setSeatCols(seatMap* seats, int newCols)
{
    if (seats->cols == newCols) return;

    pthread_mutex_lock(&(seats->mutex));

    // Reallocate seat array if necessary
    if (seats->seatArr != NULL)
    {
        seatInfo** _newSeatArr = _allocSeatsArr(seats->rows, newCols);
        
        for (int y = 0; y < seats->rows; y++) {
            for (int x = 0; x < seats->cols && x < newCols; x++) {
                _newSeatArr[y][x] = seats->seatArr[y][x];
            }
        }

        _freeSeatsData(seats);
        seats->seatArr = _newSeatArr;
    }

    seats->cols = newCols;

    pthread_mutex_unlock(&(seats->mutex));
}

// Returns the total number of sold and unsold seats
// in the given seat map while being thread safe
unsigned int getNumSeatsTotal(seatMap* seats)
{
    pthread_mutex_lock(&(seats->mutex));
    unsigned int retVal = seats->rows * seats->cols;
    pthread_mutex_unlock(&(seats->mutex));

    return retVal;
}

// Returns the total number of unsold seats
// in the given seat map while being thread safe
unsigned int getNumSeatsAvailable(seatMap* seats)
{
    pthread_mutex_lock(&(seats->mutex));
    unsigned int retVal = (seats->rows * seats->cols) - seats->numSold;
    pthread_mutex_unlock(&(seats->mutex));

    return retVal;
}

// Returns the total number of sold seats
// in the given seat map while being thread safe
unsigned int getNumSeatsSold(seatMap* seats)
{
    pthread_mutex_lock(&(seats->mutex));
    unsigned int retVal = seats->numSold;
    pthread_mutex_unlock(&(seats->mutex));

    return retVal;
}

// Returns a pointer to the seatInfo struct for the given seat
// in the seat map while being thread safe
const seatInfo* getSeatInfo(seatMap* seats, int row, int col)
{
    pthread_mutex_lock(&(seats->mutex));

    if (row < 0 || row >= seats->rows ||
        col < 0 || col >= seats->cols ||
        seats->seatArr == NULL) 
        {
            pthread_mutex_unlock(&(seats->mutex));
            return NULL;
        }
    
    seatInfo* retVal = &(seats->seatArr[row][col]);

    pthread_mutex_unlock(&(seats->mutex));
    
    return retVal;
}

// Returns 1 if the specified seat has been sold
// in the given seat map, or 0 if it has not been sold.
// If the row or col is invalid, returns -1. Is thread safe.
int seatSold(seatMap* seats, int row, int col)
{
    pthread_mutex_lock(&(seats->mutex));

    if (row < 0 || row >= seats->rows ||
        col < 0 || col >= seats->cols ||
        seats->seatArr == NULL) 
    {
        pthread_mutex_unlock(&(seats->mutex));
        return -1;
    }

    seatInfo* selectedSeat = &(seats->seatArr[row][col]);
    int retVal = selectedSeat->taken;

    pthread_mutex_unlock(&(seats->mutex));

    return retVal;
}

// Attempts to purchase the given row and col seat in
// the seat map, returns -1 if row or col is invalid.
// Returns 0 if the specified seat is already sold.
// Returns 1 if the transaction was successful.
// Is thread safe.
int buySeat(seatMap* seats, int row, int col)
{
    pthread_mutex_lock(&(seats->mutex));

    if (row < 0 || row >= seats->rows ||
        col < 0 || col >= seats->cols ||
        seats->seatArr == NULL) 
    {
        pthread_mutex_unlock(&(seats->mutex));
        return -1;
    }
    
    seatInfo* selectedSeat = &(seats->seatArr[row][col]);
    if (selectedSeat->taken) 
    {
        pthread_mutex_unlock(&(seats->mutex));
        return 0;
    }

    selectedSeat->taken = 1;
    seats->numSold++;

    pthread_mutex_unlock(&(seats->mutex));

    return 1;
}

// Prints the grid of seats to the terminal.
// Is thread safe.
void printSeatMap(seatMap* seats)
{
    pthread_mutex_lock(&(seats->mutex));

    if (seats->seatArr == NULL) 
    {
        pthread_mutex_unlock(&(seats->mutex));
        return;
    }

    lockPrintMutex();

    printf("\n====== Seats Sold =======\n");
    printf("-------------------------\n");
    printf("   | ");

    for (int x = 0; x < seats->cols; x++)
        printf("%2d ", x);

    printf("\n---|--");

    for (int x = 0; x < seats->cols; x++)
        printf("---");

    printf("\n");

    for (int y = 0; y < seats->rows; y++)
    {
        printf("%2d | ", y);
        for (int x = 0; x < seats->cols; x++)
        {
            printf("%2d ", seats->seatArr[y][x].taken);
        }
        printf("|\n");
    }
    
    printf("\n~~~~~~");

    for (int x = 0; x < seats->cols; x++)
        printf("~~~");

    printf("\n\n");

    unlockPrintMutex();
    pthread_mutex_unlock(&(seats->mutex));
}

#endif