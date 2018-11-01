#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <Credentials\Credentials.h>


//---------------- INA226------------------
#include <Wire.h>
#include <INA226.h>



const char* mqtt_server = SERVER_IP;
WiFiClient espClient;
unsigned long reconnectionPeriod = 10000; //miliseconds
unsigned long lastBrokerConnectionAttempt = 0;
unsigned long lastWifiConnectionAttempt = 0;

PubSubClient client(espClient);
long lastTempMsg = 0;
char msg[50];

int sensorRequestPeriod = 10000; // 20 seconds

const int RELAY_PIN = 0; //GPIO 0 or D3

//---------------- INA226------------------
INA226 ina;
float busVoltage = 0.0;
float busPower = 0.0;
float shuntVoltage = 0.0;
float shuntCurrent = 0.0;

//---------------Deep Sleep--------------------------
bool useDeepSleep = false;
int sleepPeriod = 60; // Seconds



/* Over The Air automatic firmware update from a web server.  ESP8266 will contact the
*  server on every boot and check for a firmware update.  If available, the update will
*  be downloaded and installed.  Server can determine the appropriate firmware for this
*  device from combination of HTTP_OTA_FIRMWARE and firmware MD5 checksums.
*/


// Name of firmware
#define HTTP_OTA_FIRMWARE      String(String(__FILE__).substring(String(__FILE__).lastIndexOf('\\')) + ".bin").substring(1)


void setup()
{
	Serial.begin(115200);
	pinMode(RELAY_PIN, OUTPUT);
	pinMode(BUILTIN_LED, OUTPUT);
	digitalWrite(RELAY_PIN, HIGH);
	WiFi.mode(WIFI_STA);
	client.setServer(mqtt_server, 1883);
	client.setCallback(callback);
	setup_wifi();
	HTTP_OTA();
	initializeINA226();
	sendMessageToMqttOnce();
}



void setup_wifi() {

	delay(500);
	// We start by connecting to a WiFi network
	Serial.print(F("Connecting to "));
	Serial.println(SSID);
	WiFi.begin(SSID, PASSWORD);
	delay(3000);

	if (WiFi.waitForConnectResult() != WL_CONNECTED) {
		Serial.println(F("Connection Failed!"));
		return;
	}
	connectToBroker();
}

void HTTP_OTA() {
	ESPhttpUpdate.rebootOnUpdate(true);
	// Check server for firmware updates
	Serial.print("Checking for firmware updates from server http://");
	Serial.print(SERVER_IP);
	Serial.print(":");
	Serial.print(HTTP_OTA_PORT);
	Serial.println(HTTP_OTA_PATH);
	Serial.print("HTTP_OTA_FIRMWARE : ");
	Serial.println(HTTP_OTA_FIRMWARE);

	switch (ESPhttpUpdate.update("http://" + String(SERVER_IP) + ":" + String(HTTP_OTA_PORT) + String(HTTP_OTA_PATH) + String(HTTP_OTA_FIRMWARE))) {
	case HTTP_UPDATE_FAILED:
		Serial.printf("HTTP update failed: Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
		break;

	case HTTP_UPDATE_NO_UPDATES:
		Serial.println(F("No updates"));
		break;

	case HTTP_UPDATE_OK:
		Serial.println(F("Update OK"));
		break;
	}
}

void callback(char* topic, byte* payload, unsigned int length) {
	Serial.print("Message arrived [");
	Serial.print(topic);
	Serial.print("] ");
	for (int i = 0; i < length; i++) {
		Serial.print((char)payload[i]);
	}
	Serial.println("-----");
	if (strcmp(topic, "Battery/relay_1") == 0) {
		//Switch on the RELAY if an 1 was received as first character
		if ((char)payload[0] == '1') {
			digitalWrite(RELAY_PIN, LOW);   // Turn the RELAY on
		}
		if ((char)payload[0] == '0') {
			digitalWrite(RELAY_PIN, HIGH);  // Turn the RELAY off
		}
	}
	if (strcmp(topic, "Battery/sensorRequestPeriod") == 0) {
		String myString = String((char*)payload);
		Serial.println(myString);
		sensorRequestPeriod = myString.toInt();
		Serial.print("Sensor request period set to :");
		Serial.print(sensorRequestPeriod);
		Serial.println(" ms");
	}
}

//Connection to MQTT broker
void connectToBroker() {
	Serial.print("Attempting MQTT connection...");
	// Attempt to connect
	if (client.connect("Battery")) {
		Serial.println("connected");
		// Once connected, publish an announcement...
		client.publish("Battery/status", "Wemos D1 mini on battery connected");
		// ... and resubscribe
		client.subscribe("Battery/relay_1");
		client.subscribe("Battery/sensorRequestPeriod");
	}
	else {
		Serial.print("failed, rc=");
		Serial.print(client.state());
		Serial.println(" try again in 60 seconds");
	}
}

void reconnectToBroker() {
	long now = millis();
	if (now - lastBrokerConnectionAttempt > reconnectionPeriod) {
		lastBrokerConnectionAttempt = now;
		{
			if (WiFi.status() == WL_CONNECTED)
			{
				if (!client.connected()) {
					connectToBroker();
				}
			}
			else
			{
				reconnectWifi();
			}
		}
	}
}

void reconnectWifi() {
	long now = millis();
	if (now - lastWifiConnectionAttempt > reconnectionPeriod) {
		lastWifiConnectionAttempt = now;
		setup_wifi();
	}
}


void loop()
{
	if (!client.connected()) {
		reconnectToBroker();
	}
	client.loop();
	sendMessageToMqttInLoop();
}

void sendMessageToMqttInLoop() {
	long now = millis();
	if (now - lastTempMsg > sensorRequestPeriod) {
		lastTempMsg = now;
		getSensorData();
		sendMessageToMqtt();
	}
}

void sendMessageToMqttOnce() {
	if (useDeepSleep) {
		//digitalWrite(BUILTIN_LED, LOW);
		//delay(5000);
		//digitalWrite(BUILTIN_LED, HIGH);
		getSensorData();
		sendMessageToMqtt();
		sleep(sleepPeriod);
	}
}

void sendMessageToMqtt() {

	Serial.print("Publish message busVoltage: ");
	Serial.println(busVoltage, 5);
	client.publish("Battery/busVoltage", String(busVoltage, 5).c_str());

	Serial.print("Publish message busPower: ");
	Serial.println(busPower, 5);
	client.publish("Battery/busPower", String(busPower, 5).c_str());

	Serial.print("Publish message shuntCurrent: ");
	Serial.println(shuntCurrent, 5);
	client.publish("Battery/shuntCurrent", String(shuntCurrent, 5).c_str());

	Serial.print("Publish message shuntVoltage: ");
	Serial.println(shuntVoltage, 5);
	client.publish("Battery/shuntVoltage", String(shuntVoltage, 5).c_str());
}


void sleep(int sleepTimeInSeconds) {
	Serial.print("Go to deep sleep");
	ESP.deepSleep(sleepTimeInSeconds * 1000000);
}

void initializeINA226() {
	// Default INA226 address is 0x40
	ina.begin();
	// Configure INA226
	ina.configure(INA226_AVERAGES_128, INA226_BUS_CONV_TIME_8244US, INA226_SHUNT_CONV_TIME_8244US, INA226_MODE_SHUNT_BUS_CONT);
	// Calibrate INA226. Rshunt = 0.1 ohm, Max excepted current = 2A
	ina.calibrate(0.1, 2);
}



void getSensorData() {
	busVoltage = ina.readBusVoltage();
	Serial.print("Bus voltage:   ");
	Serial.print(busVoltage, 5);
	Serial.println(" V");

	busPower = ina.readBusPower();
	Serial.print("Bus power:     ");
	Serial.print(busPower, 5);
	Serial.println(" W");

	shuntVoltage = ina.readShuntVoltage();
	Serial.print("Shunt voltage: ");
	Serial.print(shuntVoltage, 5);
	Serial.println(" V");

	shuntCurrent = ina.readShuntCurrent();
	Serial.print("Shunt current: ");
	Serial.print(shuntCurrent, 5);
	Serial.println(" A");

	Serial.println("");
}