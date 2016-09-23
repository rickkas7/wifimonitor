#include "Particle.h"
#include "logbuffer.h"

STARTUP(System.enableFeature(FEATURE_RETAINED_MEMORY));

SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_THREAD(ENABLED);

const char *LOGBUFFER_EVENTNAME = "wifiMonitor";
const unsigned long REPORT_RSSI_PERIOD = 10000;
const unsigned long CLOUD_CHECK_PERIOD = 60000;
const unsigned long PERIODIC_PERIOD = 15 * 60000;

// This is the separator between cells/data members. When using IFTTT to push data into a Google spreadsheet,
// ||| is the default separator. You may want use comma or tab or something else if not using IFFFT.
const char *CELL_SEPARATOR = "|||";

// You can edit these to change the default behavior
bool logCredentials = true;
bool logWiFiScan = true;

// Stuff you probably won't need to edit
retained uint8_t logBufferData[3000];
LogBuffer logBuffer(logBufferData, sizeof(logBufferData), LOGBUFFER_EVENTNAME);

bool lastWifiConnectedState = false;
bool lastCloudConnectedState = false;
int lastRssi = 0;
unsigned long lastReportRssi = 0 - REPORT_RSSI_PERIOD;
unsigned long lastCloudCheck = 0;
unsigned long lastPeriodic = 0;

// Forward declarations
void periodicCheck();
void noCloudCheck();
void wifiScanCallback(WiFiAccessPoint* wap, void* data);
const char *securityString(int value);


void setup() {
	Serial.begin(9600);

	logBuffer.setup();
	logBuffer.queue(true, "boot%s%s", CELL_SEPARATOR, System.version().c_str());

	WiFi.on();

	// Log information about currently configured credentials
	if (logCredentials && WiFi.hasCredentials()) {
		WiFiAccessPoint ap[5];
		int found = WiFi.getCredentials(ap, 5);
		for(int ii = 0; ii < found; ii++) {
			logBuffer.queue(true, "credentials%sssid=%s security=%s cipher=%d", CELL_SEPARATOR, ap[ii].ssid, securityString(ap[ii].security), ap[ii].cipher);
		}
	}
	if (logWiFiScan) {
		// Log information about nearby Wi-Fi networks
		WiFi.scan(wifiScanCallback, NULL);
	}
	Particle.connect();
}

void loop() {
	// Allow the log buffer to do its publishes and cleanup tasks
	logBuffer.loop();

	bool wifiConnectedState = WiFi.ready();
	if (wifiConnectedState != lastWifiConnectedState) {
		lastWifiConnectedState = wifiConnectedState;

		// Change of cloud connected state, report
		logBuffer.queue(true, "wifi%s%s", CELL_SEPARATOR, wifiConnectedState ? "connected" : "disconnected");

	}
	if (wifiConnectedState) {
		// When connected by wifi, report RSSI (signal strength) when it changes by more than 10,
		// at most every 10 seconds
		int rssi = WiFi.RSSI();
		int delta = rssi - lastRssi;
		if (delta < 0) {
			delta = -delta;
		}
		if (delta > 10) {
			if (millis() - lastReportRssi >= REPORT_RSSI_PERIOD) {
				lastReportRssi = millis();
				logBuffer.queue(true, "rssi%s%d", CELL_SEPARATOR, rssi);
				lastRssi = rssi;
			}
		}
	}
	else {
		lastRssi = 0;
	}

	bool cloudConnectedState = Particle.connected();
	if (cloudConnectedState != lastCloudConnectedState) {
		lastCloudConnectedState = cloudConnectedState;

		// Change of cloud connected state, report
		logBuffer.queue(true, "cloud%s%s", CELL_SEPARATOR, cloudConnectedState ? "connected" : "disconnected");
	}

	if (lastWifiConnectedState && !lastCloudConnectedState) {
		// If we have wifi but not cloud, log some pings and DNS
		if (millis() - lastCloudCheck >= CLOUD_CHECK_PERIOD) {
			lastCloudCheck = millis();
			noCloudCheck();
		}
	}

	if (millis() - lastPeriodic >= PERIODIC_PERIOD) {
		lastPeriodic = millis();
		periodicCheck();
	}

}

// Called when there is wifi, but no cloud connectivity
void noCloudCheck() {
	logBuffer.queue(true, "rssi%s%d", CELL_SEPARATOR, WiFi.RSSI());

	IPAddress addr = IPAddress(8,8,8,8);
	logBuffer.queue(true, "ping%s%s%s%d", CELL_SEPARATOR, addr.toString().c_str(), CELL_SEPARATOR, WiFi.ping(addr, 1));

	const char *host = "api.particle.io";
	addr = WiFi.resolve(host);
	logBuffer.queue(true, "dns%s%s%s%s", CELL_SEPARATOR, host, CELL_SEPARATOR, addr.toString().c_str());

	// api.particle.io doesn't respond to ping, so this would always fail
	// logBuffer.queue(true, "ping,%s,%d", addr.toString().c_str(), WiFi.ping(addr, 1));
}

// Called periodically, currently every 15 minutes
void periodicCheck() {
	logBuffer.queue(true, "check%s%s", CELL_SEPARATOR, lastCloudConnectedState ? "connected" : "disconnected");
}


// Logs nearby wifi networks, currently only done at startup
void wifiScanCallback(WiFiAccessPoint* wap, void* data) {
	logBuffer.queue(true, "wifiscan%sSSID=%s security=%s channel=%d rssi=%d",
			CELL_SEPARATOR, wap->ssid, securityString(wap->security), wap->channel, wap->rssi);
}

// Converts a number from WiFiAccessPoint's security member to a readable string
const char *securityString(int value) {
	const char *sec = "unknown";
	switch(value) {
	case WLAN_SEC_UNSEC:
		sec = "unsecured";
		break;

	case WLAN_SEC_WEP:
		sec = "wep";
		break;

	case WLAN_SEC_WPA:
		sec = "wpa";
		break;

	case WLAN_SEC_WPA2:
		sec = "wpa2";
		break;

	case WLAN_SEC_NOT_SET:
		sec = "not_set";
		break;

	default:
		break;
	}
	return sec;
}
