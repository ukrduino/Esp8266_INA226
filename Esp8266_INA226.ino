#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <Credentials\Credentials.h>


const char* mqtt_server = SERVER_IP;
WiFiClient espClient;
unsigned long reconnectionPeriod = 10000; //miliseconds
unsigned long lastBrokerConnectionAttempt = 0;
unsigned long lastWifiConnectionAttempt = 0;

PubSubClient client(espClient);
long lastTempMsg = 0;
char msg[50];

int sensorRequestPeriod = 60000; // 60 seconds

const int RELAY_PIN = 16;

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
	WiFi.mode(WIFI_STA);
	client.setServer(mqtt_server, 1883);
	client.setCallback(callback);
	setup_wifi();
	HTTP_OTA();
}

void setup_wifi() {

	delay(500);
	// We start by connecting to a WiFi network
	Serial.print(F("Connecting to "));
	Serial.println(SSID);
	WiFi.begin(SSID, PASSWORD);
	delay(1000);
	Serial.print(F("IP address: "));
	Serial.println(WiFi.localIP());
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
	if (strcmp(topic, "Sender_1/topic_1") == 0) {
		//Switch on the RELAY if an 1 was received as first character
		if ((char)payload[0] == '1') {
			digitalWrite(RELAY_PIN, LOW);   // Turn the RELAY on
		}
		if ((char)payload[0] == '0') {
			digitalWrite(RELAY_PIN, HIGH);  // Turn the RELAY off
		}
	}
	if (strcmp(topic, "Sender_2/topic_2") == 0) {
		String myString = String((char*)payload);
		Serial.println(myString);
		sensorRequestPeriod = myString.toInt();
		Serial.print("Sensor request period set to :");
		Serial.print(sensorRequestPeriod);
		Serial.println("ms");
	}
}

//Connection to MQTT broker
void connectToBroker() {
	Serial.print("Attempting MQTT connection...");
	// Attempt to connect
	if (client.connect("Client_Name")) {
		Serial.println("connected");
		// Once connected, publish an announcement...
		client.publish("Client_Name/status", "Wemos D1 mini connected");
		// ... and resubscribe
		client.subscribe("Sender_1/topic_1");
		client.subscribe("Sender_2/topic_2");
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
	sendMessageToMqtt();
}

void sendMessageToMqtt() {
	long now = millis();
	if (now - lastTempMsg > sensorRequestPeriod) {
		lastTempMsg = now;
		float sensor_value = 30;
		// Request sensot here
		//float temp = sensors.getTempCByIndex(0);
		Serial.print("Publish message temp: ");
		Serial.println(sensor_value);
		if (-20 < sensor_value && sensor_value < 50) // do not send failed read data
		{
			client.publish("Battery/voltage", String(sensor_value).c_str());
		}
	}
}
