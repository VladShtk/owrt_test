#ifndef COUNTERS_H
#define COUNTERS_H

#include <time.h>

#include "client_manager.h"

/**
 * Counter info
 */
typedef struct
{
	const ClientSettings *pClient;   // Client for this counter
	ClientData currentData;			 // Current client data

	unsigned long int differenceCounter; // Difference data counter
	unsigned long int cumulatedNot100Counter; // Difference data counter
	long int          cumulatedCounter; // Cummulated Counter

	//time_t	   lastChangeTime;		// Last change time
	//time_t	   lastChangeInterval;  // Last change interval
}CounterInfo;

void counterAdd(const ClientSettings *pClient);
bool counterUpdate(const ClientSettings *pClient, const ClientData *pClientData);
bool counterGet(const ClientSettings *pClient, ClientData *pClientData);
void counterReset(const ClientSettings *pClient);

#endif //COUNTERS_H
