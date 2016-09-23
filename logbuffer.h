#ifndef __LOGBUFFER_H
#define __LOGBUFFER_H

/**
 * Structure for the header to the data in the retained data buffer
 */
typedef struct { // 12, not counting the data
	uint32_t magic; 	// LogBuffer::MAGIC_BYTES, to determine if the buffer is valid
	size_t dataSize;	// Size of all data (header + strings in data)
	size_t next;		// Offset of next byte to write in data (0 = buffer is empty)
	char data[1];		// Packed c-strings
} LogBufferData;

/**
 * Class for managing a logging buffer of data stored in retained memory
 */
class LogBuffer {
public:
	static const uint32_t MAGIC_BYTES = 0xdd2da730;
	static const unsigned long PUBLISH_PERIOD_MS = 1010;
	static const size_t MINIMUM_BUFFER = 128;

	LogBuffer(uint8_t *retainedData, size_t size, const char *eventName);
	virtual ~LogBuffer();

	void setup();
	void loop();

	bool hasDataToPublish() const;
	void publish();
	void removeFirst();
	void queue(bool prependTimestamp, const char *fmt, ...);
	void vqueue(bool prependTimestamp, const char *fmt, va_list ap);
	char *getEnd() const;

private:
	// Parameters passed into constructor
	LogBufferData *retainedData;
	size_t size;
	const char *eventName;

	// Other variables
	unsigned long lastPublish = 0;
};


#endif /* __LOGBUFFER_H */

