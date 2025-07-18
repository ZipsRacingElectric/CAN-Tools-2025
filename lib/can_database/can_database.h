#ifndef CAN_DATABASE_H
#define CAN_DATABASE_H

// CAN Database ---------------------------------------------------------------------------------------------------------------
//
// Author: Cole Barach
// Date Created: 2023.07.21
//
// Description: An database-oriented interface for CAN bus communication. Received messages are parsed using a DBC file and
//   stored in a relational database for random access.

// Includes -------------------------------------------------------------------------------------------------------------------

// Includes
#include "can_device/can_device.h"
#include "can_signals.h"

// POSIX Libraries
#include <pthread.h>

// C Standard Library
#include <stdio.h>
#include <float.h>

// Constants ------------------------------------------------------------------------------------------------------------------

#define CAN_DATABASE_MESSAGE_COUNT_MAX	128
#define CAN_DATABASE_SIGNAL_COUNT_MAX	1024

// Datatypes ------------------------------------------------------------------------------------------------------------------

struct canDatabase
{
	canDevice_t*	device;
	pthread_t		rxThread;
	canMessage_t	messages [CAN_DATABASE_MESSAGE_COUNT_MAX];
	size_t			messageCount;
	canSignal_t		signals [CAN_DATABASE_SIGNAL_COUNT_MAX];
	size_t			signalCount;
	float			signalValues [CAN_DATABASE_SIGNAL_COUNT_MAX];
	bool			signalsValid [CAN_DATABASE_SIGNAL_COUNT_MAX];
};

typedef struct canDatabase canDatabase_t;

// Function -------------------------------------------------------------------------------------------------------------------

// TODO(Barach): Docs
/**
 * @brief Initializes a CAN database bound to the specified device using the specified database file.
 * @param dbcPath The database file to import from.
 * @return 0 if successful, the error code otherwise.
 */
int canDatabaseInit (canDatabase_t* database, canDevice_t* device, const char* dbcPath);

/**
 * @brief Prints all of the data of a database.
 * @param stream The stream to write to.
 * @param database The database to print.
 */
void canDatabasePrint (FILE* stream, canDatabase_t* database);

/**
 * @brief Prompts the user to select a database message.
 * @param database The database to search from.
 * @return The index of the selected message.
 */
size_t canDatabaseMessageNamePrompt (canDatabase_t* database);

/**
 * @brief Prints the name of each message in the database.
 * @param stream The stream to print to.
 * @param database The database to search from.
 */
void canDatabaseMessagesPrint (FILE* stream, canDatabase_t* database);

/**
 * @brief Prints the last read values of a CAN message from the database.
 * @param stream The stream to print to.
 * @param database The database to read from.
 * @param index The index of the message to print.
 */
void canDatabaseMessageValuePrint (FILE* stream, canDatabase_t* database, size_t index);

/**
 * @brief Finds a signal's index based off name.
 * @param database The database to search from.
 * @param name The name of the signal to find.
 * @param index If successful, written to contain the index of the signal.
 * @return 0 if successful, the error code otherwise.
 */
int canDatabaseFindSignal (canDatabase_t* database, const char* name, size_t* index);

inline static bool signalToBool (float value)
{
	return value >= FLT_EPSILON || value <= -FLT_EPSILON;
}

#endif // CAN_DATABASE_H