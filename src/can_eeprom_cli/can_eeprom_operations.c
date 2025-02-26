// Header
#include "can_eeprom_operations.h"

// Includes
#include "error_codes.h"

// C Standard Library
#include <errno.h>
#include <string.h>
#include <sys/time.h>

// Macros / Constants ---------------------------------------------------------------------------------------------------------

#define EEPROM_COMMAND_MESSAGE_RW(rnw)		(((uint16_t) (rnw))	<< 15)
#define EEPROM_RESPONSE_MESSAGE_RW(word)	((((word) >> 15) & 0b1) == 0b1)

#define RESPONSE_ATTEMPT_COUNT 500
static const struct timeval RESPONSE_ATTEMPT_TIMEOUT =
{
	.tv_sec		= 0,
	.tv_usec	= 1000
};

// Function Prototypes --------------------------------------------------------------------------------------------------------

/// @brief Encodes the command frame for a write operation.
struct can_frame writeCommandEncode (uint16_t canId, uint16_t address, uint8_t count, void* buffer);

/// @brief Encodes the command frame for a read operation.
struct can_frame readCommandEncode (uint16_t canId, uint16_t address, uint8_t count);

/// @brief Parses a CAN frame to check whether it is the response to a specific command.
int responseParse (uint16_t canId, struct can_frame* frame, uint16_t address, uint8_t count);

/// @brief Performs a single write operation on the EEPROM.
int writeSingle (uint16_t canId, canSocket_t* socket, uint16_t address, uint8_t count, void* buffer);

/// @brief Performs a single read operation on the EEPROM.
int readSingle (uint16_t canId, canSocket_t* socket, uint16_t address, uint8_t count, void* buffer);

// Functions ------------------------------------------------------------------------------------------------------------------

int canEepromWrite (uint16_t canId, canSocket_t* socket, uint16_t address, uint16_t count, void* buffer)
{
	// Write all of the full messages
	while (count > 4)
	{
		if (writeSingle (canId, socket, address, 4, buffer) != 0)
			return errno;

		buffer += 4;
		address += 4;
		count -= 4;
	}

	// Write the last message
	return writeSingle (canId, socket, address, count, buffer);
}

int canEepromRead (uint16_t canId, canSocket_t* socket, uint16_t address, uint16_t count, void* buffer)
{
	// Read all of the full messages
	while (count > 4)
	{
		if (readSingle (canId, socket, address, 4, buffer) != 0)
			return errno;

		buffer += 4;
		address += 4;
		count -= 4;
	}

	// Read the last message
	return readSingle (canId, socket, address, count, buffer);
}

struct can_frame writeCommandEncode (uint16_t canId, uint16_t address, uint8_t count, void* buffer)
{
	address |= EEPROM_COMMAND_MESSAGE_RW (false);

	struct can_frame frame =
	{
		.can_id		= canId,
		.can_dlc	= count + sizeof (address),
		.data		=
		{
			address,
			address >> 8
		}
	};
	memcpy (frame.data + sizeof (address), buffer, count);

	return frame;
}

struct can_frame readCommandEncode (uint16_t canId, uint16_t address, uint8_t count)
{
	address |= EEPROM_COMMAND_MESSAGE_RW (true);

	struct can_frame frame =
	{
		.can_id		= canId,
		.can_dlc	= count + sizeof (address),
		.data		=
		{
			address,
			address >> 8
		}
	};

	return frame;
}

int responseParse (uint16_t canId, struct can_frame* frame, uint16_t address, uint8_t count)
{
	// Check message ID is correct
	if (frame->can_id != (uint16_t) (canId + 1))
	{
		errno = ERRNO_CAN_EEPROM_BAD_RESPONSE_ID;
		return errno;
	}

	// Check message is a read response
	uint16_t responseAddress = frame->data [0] | (frame->data [1] << 8);
	if (!EEPROM_RESPONSE_MESSAGE_RW (responseAddress))
	{
		errno = ERRNO_CAN_EEPROM_MALFORMED_RESPONSE;
		return errno;
	}

	// Check message is the correct address
	if (responseAddress != address)
	{
		errno = ERRNO_CAN_EEPROM_MALFORMED_RESPONSE;
		return errno;
	}

	// Check count is correct
	uint8_t responseCount = frame->len - 2;
	if (responseCount != count)
	{
		errno = ERRNO_CAN_EEPROM_MALFORMED_RESPONSE;
		return errno;
	}

	return 0;
}

int writeSingle (uint16_t canId, canSocket_t* socket, uint16_t address, uint8_t count, void* buffer)
{
	struct can_frame command = writeCommandEncode (canId, address, count, buffer);

	for (uint16_t attempt = 0; attempt < RESPONSE_ATTEMPT_COUNT; ++attempt)
	{
		// Transmit the command message
		if (canSocketTransmit (socket, &command) != 0)
			return errno;

		// Track the response timeout
		struct timeval deadline;
		struct timeval timeCurrent;
		gettimeofday (&timeCurrent, NULL);
		timeradd (&timeCurrent, &RESPONSE_ATTEMPT_TIMEOUT, &deadline);

		while (timercmp (&timeCurrent, &deadline, <))
		{
			gettimeofday (&timeCurrent, NULL);

			// Read the response frame, ignore all others.
			struct can_frame response;
			if (canSocketReceive (socket, &response) != 0 || responseParse (canId, &response, address, count) != 0)
				continue;

			// If the response contains the incorrect data, retransmit the command.
			if (memcmp (buffer, response.data + sizeof (address), count) != 0)
				break;

			// Otherwise, success
			return 0;
		}
	}

	// If we didn't succeed, we failed.
	errno = ERRNO_CAN_EEPROM_WRITE_TIMEOUT;
	return errno;
}

int readSingle (uint16_t canId, canSocket_t* socket, uint16_t address, uint8_t count, void* buffer)
{
	struct can_frame command = readCommandEncode (canId, address, count);

	for (uint16_t attempt = 0; attempt < RESPONSE_ATTEMPT_COUNT; ++attempt)
	{
		// Transmit the command message
		if (canSocketTransmit (socket, &command) != 0)
			return errno;

		// Track the response timeout
		struct timeval deadline;
		struct timeval timeCurrent;
		gettimeofday (&timeCurrent, NULL);
		timeradd (&timeCurrent, &RESPONSE_ATTEMPT_TIMEOUT, &deadline);

		while (timercmp (&timeCurrent, &deadline, <))
		{
			gettimeofday (&timeCurrent, NULL);

			// Read the response frame, ignore all others.
			struct can_frame response;
			if (canSocketReceive (socket, &response) != 0 || responseParse (canId, &response, address, count) != 0)
				continue;

			// If we got a response, copy the data into the buffer.
			memcpy (buffer, response.data + sizeof (address), count);
			return 0;
		}
	}

	// If we didn't succeed, we failed.
	errno = ERRNO_CAN_EEPROM_READ_TIMEOUT;
	return errno;
}