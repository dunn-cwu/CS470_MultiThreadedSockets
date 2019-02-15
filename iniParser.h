// ==============================
// School: Central Washington University
// Course: CS470 Operating Systems
// Instructor: Dr. Szilárd VAJDA
// Student: Andrew Dunn
// Assignment: Lab 3
// Description: Example program demonstrating 
// multi theading and sockets from the client side
// ==============================
// Very simple ini file reader
// ==============================
#ifndef INIPARSER_H
#define INIPARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Finds the given ini key in the file and places the value in resultBuffer
// if the key is found and returns 0. Otherwise returns a non-zero error code.
int getIniValue(FILE* fp, char* key, char* resultBuffer, int bufferLen)
{
    if (fp == NULL) return 2;

    // Seek to the start of the file
    fseek(fp, 0, SEEK_SET);

    const char delim[2] = "=";
    char* token;
    char* lineBuffer = NULL;
    size_t lineBufferSize = 0;
    ssize_t bytesRead;

    // Read the file line by line looking for the key
    while ((bytesRead = getline(&lineBuffer, &lineBufferSize, fp)) != -1) 
    {
        if (strstr(lineBuffer, key) != NULL)
        {
            // Split line around '=' character
            token = strtok(lineBuffer, delim);
            if (token == NULL) { free(lineBuffer); return 1; }

            // Get token on right side of '=' character
            token = strtok(NULL, delim);
            if (token == NULL) { free(lineBuffer); return 1; }
            
            // Replace new line char with null char
            int tokLen = strlen(token);
            if (token[tokLen - 1] == '\n') token[tokLen - 1] = '\0';

            // If token is longer than our buffer, return with error
            if (tokLen > bufferLen) { free(lineBuffer); return 3; }

            // Copy value into return buffer and return
            strcpy(resultBuffer, token);

            free(lineBuffer); 
            return 0;
        }
    }

    free(lineBuffer);
    return 1;
}

#endif