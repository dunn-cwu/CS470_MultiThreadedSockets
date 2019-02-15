// ==============================
// School: Central Washington University
// Course: CS470 Operating Systems
// Instructor: Dr. Szilárd VAJDA
// Student: Andrew Dunn
// Assignment: Lab 3
// Description: Example program demonstrating 
// multi theading and sockets from the client side
// ==============================
// Provides thread safe wrapper functions
// for printf(). Ensures multiple threads
// can print to the terminal without issues.
// ==============================

#ifndef THEADSAFEPRINT_H
#define THEADSAFEPRINT_H

#include <stdio.h>
#include <pthread.h>
#include <stdarg.h>

pthread_mutex_t _printLock;

void safePrint(char* msg, ...)
{
    pthread_mutex_lock(&_printLock);

    va_list vargs;
    va_start(vargs, msg);
    vprintf(msg, vargs);
    va_end(vargs);
    fflush(stdout);

    pthread_mutex_unlock(&_printLock);
}

void safePrintLine(char* msg, ...)
{
    pthread_mutex_lock(&_printLock);

    va_list vargs;
    va_start(vargs, msg);
    vprintf(msg, vargs);
    
    printf("\n");

    va_end(vargs);
    fflush(stdout);

    pthread_mutex_unlock(&_printLock);
}

void printFromHost(char* msg, ...)
{
    pthread_mutex_lock(&_printLock);

    printf("[ Host ] ");

    va_list vargs;
    va_start(vargs, msg);
    vprintf(msg, vargs);

    printf("\n");

    va_end(vargs);
    fflush(stdout);

    pthread_mutex_unlock(&_printLock);
}

void printFromThread(pthread_t id, char* msg, ...)
{
    pthread_mutex_lock(&_printLock);

    printf("[0x%lx] ", id);

    va_list vargs;
    va_start(vargs, msg);
    vprintf(msg, vargs);

    printf("\n");

    va_end(vargs);
    fflush(stdout);

    pthread_mutex_unlock(&_printLock);
}

void printFromClient(int clientId, char* msg, ...)
{
    pthread_mutex_lock(&_printLock);

    printf("[ Client #%d ] ", clientId);

    va_list vargs;
    va_start(vargs, msg);
    vprintf(msg, vargs);

    printf("\n");

    va_end(vargs);
    fflush(stdout);

    pthread_mutex_unlock(&_printLock);
}

void lockPrintMutex()
{
    pthread_mutex_lock(&_printLock);
}

void unlockPrintMutex()
{
    fflush(stdout);
    pthread_mutex_unlock(&_printLock);
}

#endif