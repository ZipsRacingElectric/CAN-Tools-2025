// CAN Database TUI -----------------------------------------------------------------------------------------------------------
//
// Author: Cole Barach
// Date Created: 2024.10.27
//
// Description: TODO(Barach)

// Includes -------------------------------------------------------------------------------------------------------------------

// Includes
#include "can_database.h"

// NCurses
#include <ncurses.h>

// C Standard Library
#include <stdio.h>

// Entrypoint -----------------------------------------------------------------------------------------------------------------

void printTuiDatabaseLine (canDatabase_t* database, uint16_t lineIndex);

int main (int argc, char** argv)
{
	if (argc != 3)
	{
		printf ("Format: can-dbc-tui <device name> <DBC file path>\n");
		return -1;
	}

	const char* deviceName = argv [1];
	const char* dbcPath = argv [2];

	// Initialize the database.
	canDatabase_t database;
	if (!canDatabaseInit (&database, deviceName, dbcPath))
		return -1;

	initscr ();

	// Non-blocking input
	cbreak ();
	nodelay (stdscr, true);
	keypad(stdscr, true);

	uint16_t offset = 0;

	int row, col;
	getmaxyx(stdscr, row, col);

	while (true)
	{
		mvprintw (0, 0, "");

		int ret = getch ();
		if (ret != ERR)
		{
			if (ret == KEY_UP)
				--offset;
			else if (ret == KEY_DOWN)
				++offset;
		}

		for (uint16_t index = 0; index < row; ++index)
			printTuiDatabaseLine (&database, index + offset);

		refresh ();
		napms(1);
	}

	endwin ();

	return 0;
}

void printTuiDatabaseLine (canDatabase_t* database, uint16_t lineIndex)
{
	size_t signalOffset = 0;

	if (lineIndex == 0)
	{
		printw ("%32s | %10s | %8s | %10s | %12s | %12s | %12s | %9s | %6s\n\n",
			"Signal Name", "Value", "Bit Mask", "Bit Length", "Bit Position",
			"Scale Factor", "Offset", "Is Signed", "Endian");
		return;
	}

	uint16_t currentIndex = 0;
	for (size_t messageIndex = 0; messageIndex < database->messageCount; ++messageIndex)
	{
		canMessage_t* message = database->messages + messageIndex;

		++currentIndex;
		if (currentIndex == lineIndex)
		{
			printw ("%s - ID: %3X\n", message->name, message->id);
			return;
		}

		for (size_t signalIndex = 0; signalIndex < message->signalCount; ++signalIndex)
		{
			++currentIndex;

			canSignal_t* signal = message->signals + signalIndex;
			bool valid = database->signalsValid [signalOffset + signalIndex];
			float value = database->signalValues [signalOffset + signalIndex];

			if (currentIndex == lineIndex)
			{
				if (valid)
				{
					printw ("%32s | %10.3f | %8lX | %10i | %12i | %12f | %12f | %9u | %6u\n",
						signal->name, value, signal->bitmask, signal->bitLength, signal->bitPosition,
						signal->scaleFactor, signal->offset, signal->signedness, signal->endianness);
					return;
				}
				else
				{
					printw ("%32s | %10s | %8lX | %10i | %12i | %12f | %12f | %9u | %6u\n",
						signal->name, "--", signal->bitmask, signal->bitLength, signal->bitPosition,
						signal->scaleFactor, signal->offset, signal->signedness, signal->endianness);
					return;
				}
			}
		}

		++currentIndex;
		if (currentIndex == lineIndex)
		{
			printw ("\n");
			return;
		}

		signalOffset += message->signalCount;
	}

	printw ("\n");
}