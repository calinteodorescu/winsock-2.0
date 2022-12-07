//
// CountC.c -- Tracks the number of active client threads
//

#include "httpMT.h"

DWORD            s_gdwClientCount = 0;
CRITICAL_SECTION s_gcriticalClients;
HANDLE           s_ghNoClients;

////////////////////////////////////////////////////////////

HANDLE InitClientCount(void)
{
    s_gdwClientCount = 0;

    InitializeCriticalSection( & s_gcriticalClients );

    //
    // Create the "no clients" signal event object
    //
    s_ghNoClients = CreateEvent(NULL,        // Security
                                TRUE,        // Manual reset
                                TRUE,        // Initial State
                                NULL);       // Name
    return s_ghNoClients;
}

////////////////////////////////////////////////////////////

void IncrementClientCount(void)
{
    EnterCriticalSection( & s_gcriticalClients);
    s_gdwClientCount++;
    LeaveCriticalSection(&s_gcriticalClients);
    ResetEvent( s_ghNoClients );
}

////////////////////////////////////////////////////////////

void DecrementClientCount(void)
{
    EnterCriticalSection(&s_gcriticalClients);
    if (s_gdwClientCount > 0)
        s_gdwClientCount--;
    LeaveCriticalSection(&s_gcriticalClients);
    if (s_gdwClientCount < 1)
        SetEvent(s_ghNoClients);
}

////////////////////////////////////////////////////////////

void DeleteClientCount(void)
{
    DeleteCriticalSection( & s_gcriticalClients );
    CloseHandle( s_ghNoClients );
}

