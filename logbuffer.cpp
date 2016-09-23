#include "Particle.h"
#include "logbuffer.h"

extern const char *CELL_SEPARATOR;

LogBuffer::LogBuffer(uint8_t *retainedData, size_t size, const char *eventName) :
	retainedData((LogBufferData *)retainedData), size(size), eventName(eventName) {

}

LogBuffer::~LogBuffer() {

}

void LogBuffer::setup() {
	bool isValid = false;

	if (retainedData->magic == LogBuffer::MAGIC_BYTES && retainedData->dataSize == size) {
		// Validate existing data
		char *end = getEnd();

		if (&retainedData->data[retainedData->next] <= end) {
			// The next pointer is at least within the buffer
			if (retainedData->next == 0 || retainedData->data[retainedData->next - 1] == 0) {
				// And the buffer is either empty, or ends after a valid cstring

				// We could validate each cstring and make sure it's < 256 bytes, but that's
				// probably not necessary. As long as there's a good null terminator at the
				// end the worst thing is that we'd get a corrupted message

				isValid = true;
			}

		}
	}

	if (!isValid) {
		// Reinitialize the buffer
		retainedData->magic = LogBuffer::MAGIC_BYTES;
		retainedData->dataSize = size;
		retainedData->next = 0;
	}
}

void LogBuffer::loop() {
	if (hasDataToPublish() && Particle.connected()) {
		if (millis() - lastPublish >= PUBLISH_PERIOD_MS) {
			lastPublish = millis();
			publish();
		}
	}
}

bool LogBuffer::hasDataToPublish() const {
	return (retainedData->next > 0);
}

void LogBuffer::publish() {
	Serial.printlnf("published %s", retainedData->data);
	Particle.publish(eventName, retainedData->data, PRIVATE);
	removeFirst();
}

void LogBuffer::removeFirst() {

	size_t firstSize = strlen(retainedData->data) + 1;
	size_t remainder = retainedData->next - firstSize;
	if (remainder > 0) {
		memmove(retainedData->data, &retainedData->data[firstSize], remainder);
	}
	retainedData->next -= firstSize;
}

void LogBuffer::queue(bool prependTimestamp, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vqueue(prependTimestamp, fmt, ap);
	va_end(ap);
}

void LogBuffer::vqueue(bool prependTimestamp, const char *fmt, va_list ap) {
	size_t sizeLeft;

	// Make sure there is enough space
	char *end = getEnd();
	while(true) {
		sizeLeft = end - &retainedData->data[retainedData->next];
		if (sizeLeft >= MINIMUM_BUFFER) {
			break;
		}
		removeFirst();
	}

	char *dst = &retainedData->data[retainedData->next];

	if (prependTimestamp) {
		// Add the timestamp
		String dateStr = Time.format("%Y-%m-%d");
		String timeStr = Time.format("%H:%M:%S");

		size_t len = snprintf(dst, sizeLeft, "%s%s%s%s", dateStr.c_str(), CELL_SEPARATOR, timeStr.c_str(), CELL_SEPARATOR);
		dst += len;
		sizeLeft -= len;
	}

	// Add the rest of the parameters
	dst += vsnprintf(dst, sizeLeft, fmt, ap);

	Serial.printlnf("logged offset=%d data=%s", retainedData->next, &retainedData->data[retainedData->next]);

	retainedData->next = (dst - retainedData->data + 1);
}

char *LogBuffer::getEnd() const {
	return &((char *)retainedData)[retainedData->dataSize];
}


