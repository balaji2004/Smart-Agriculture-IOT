#include <ESP8266WiFi.h>
#include "HTTPSRedirect.h"
#include "DebugMacros.h"
#include <DHT.h>

#define DHTPIN D4                                                           // what digital pin we're connected to
#define DHTTYPE DHT11                                                       // select dht type as DHT 11 or DHT22
#define MOISTURE_ANALOG_PIN A0                                              // Analog pin for moisture sensor
#define MOISTURE_DIGITAL_PIN D2                                             // Digital pin for moisture sensor

DHT dht(DHTPIN, DHTTYPE);

float h;
float t;
int moistureAnalog;
bool moistureDigital;
String sheetHumid = "";
String sheetTemp = "";
String sheetMoistureAnalog = "";
String sheetMoistureDigital = "";

const char* ssid = "Jarvis1";               
const char* password = "Pixies@1915";       

const char* host = "script.google.com";
const char *GScriptId = "AKfycbxRaWc-Z8qQ5_hfc1HYGA0NUZK6DsG42NPUweA8rfIAGvEaPI9oSbYj7RBxkosKhCYE-g"; 
const int httpsPort = 443; 


const char* fingerprint = "";

String url = String("/macros/s/") + GScriptId + "/exec?value=Temperature";  // Write Temperature to Google Spreadsheet at cell A1
// Fetch Google Calendar events for 1 week ahead
String url2 = String("/macros/s/") + GScriptId + "/exec?cal";  // Write to Cell A continuously

// replace with sheet name not with spreadsheet file name taken from google
String payload_base =  "{\"command\": \"appendRow\", \
                    \"sheet_name\": \"temphum\", \
                       \"values\": ";
String payload = "";

HTTPSRedirect* client = nullptr;

void setup() {
  delay(1000);
  Serial.begin(115200);
  dht.begin();     // initialise DHT11

  pinMode(MOISTURE_DIGITAL_PIN, INPUT); // Initialize the digital pin as input

  Serial.println();
  Serial.print("Connecting to wifi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Use HTTPSRedirect class to create a new TLS connection
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");
  Serial.print("Connecting to ");
  Serial.println(host); // try to connect with "script.google.com"

  // Try to connect for a maximum of 5 times then exit
  bool flag = false;
  for (int i = 0; i < 5; i++) {
    int retval = client->connect(host, httpsPort);
    if (retval == 1) {
      flag = true;
      break;
    }
    else
      Serial.println("Connection failed. Retrying...");
  }

  if (!flag) {
    Serial.print("Could not connect to server: ");
    Serial.println(host);
    Serial.println("Exiting...");
    return;
  }

  Serial.println("\nWrite into cell 'A1'");
  Serial.println("------>");
  // fetch spreadsheet data
  client->GET(url, host);

  Serial.println("\nGET: Fetch Google Calendar Data:");
  Serial.println("------>");
  // fetch spreadsheet data
  client->GET(url2, host);

  Serial.println("\nStart Sending Sensor Data to Google Spreadsheet");

  // delete HTTPSRedirect object
  delete client;
  client = nullptr;
}

void loop() {

  h = dht.readHumidity(); // Reading temperature or humidity takes about 250 milliseconds!
  t = dht.readTemperature(); // Read temperature as Celsius (the default)
  moistureAnalog = analogRead(MOISTURE_ANALOG_PIN); // Read analog moisture value
  moistureDigital = digitalRead(MOISTURE_DIGITAL_PIN); // Read digital moisture value

  if (isnan(h) || isnan(t)) { // Check if any reads failed and exit early (to try again).
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  Serial.print("Humidity: ");  Serial.print(h);
  sheetHumid = String(h) + String("%"); // convert integer humidity to string humidity
  Serial.print("%  Temperature: ");  Serial.print(t);  Serial.println("°C ");
  sheetTemp = String(t) + String("°C");

  Serial.print("Analog Moisture: "); Serial.println(moistureAnalog);
  sheetMoistureAnalog = String(moistureAnalog);

  // Print digital moisture value with explanation
  if (moistureDigital == LOW) {
    Serial.println("Digital Moisture: Wet (LOW)");
    sheetMoistureDigital = "Wet";
  } else {
    Serial.println("Digital Moisture: Dry (HIGH)");
    sheetMoistureDigital = "Dry";
  }

  static int error_count = 0;
  static int connect_count = 0;
  const unsigned int MAX_CONNECT = 20;
  static bool flag = false;

  payload = payload_base + "\"" + sheetTemp + "," + sheetHumid + "," + sheetMoistureAnalog + "," + sheetMoistureDigital + "\"}";

  if (!flag) {
    client = new HTTPSRedirect(httpsPort);
    client->setInsecure();
    flag = true;
    client->setPrintResponseBody(true);
    client->setContentTypeHeader("application/json");
  }

  if (client != nullptr) {
    if (!client->connected()) {
      client->connect(host, httpsPort);
      client->POST(url2, host, payload, false);
      Serial.print("Sent: ");  Serial.println("Temp, Humid, and Moisture");
    }
  }
  else {
    DPRINTLN("Error creating client object!");
    error_count = 5;
  }

  if (connect_count > MAX_CONNECT) {
    connect_count = 0;
    flag = false;
    delete client;
    return;
  }

  Serial.println("POST or SEND Sensor data to Google Spreadsheet:");
  if (client->POST(url2, host, payload)) {
    ;
  }
  else {
    ++error_count;
    DPRINT("Error-count while connecting: ");
    DPRINTLN(error_count);
  }

  if (error_count > 3) {
    Serial.println("Halting processor...");
    delete client;
    client = nullptr;
    Serial.printf("Final free heap: %u\n", ESP.getFreeHeap());
    Serial.printf("Final stack: %u\n", ESP.getFreeContStack());
    Serial.flush();
    ESP.deepSleep(0);
  }

  delay(2000); // keep delay of minimum 2 seconds as dht allow reading after 2 seconds interval and also for google sheet
}
