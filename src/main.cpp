#include <Arduino.h>
#include <IBMIOTF8266.h>
#include <Wire.h>
#include <SSD1306.h>
#include <DHTesp.h>

SSD1306             display(0x3c,4, 5, GEOMETRY_128_32);

String user_html = "";

char*               ssid_pfix = (char*)"IoTThermostat";
unsigned long       lastPublishMillis = - pubInterval;
unsigned long       lastClicked = 0;
int                 clicked = 0;
const int           pulseA = 12;
const int           pulseB = 13;
const int           pushSW = 2;
volatile int        lastEncoded = 0;
volatile long       encoderValue= 70;


#define             DHTPIN 14
DHTesp              dht;
#define             interval 2000
unsigned long       lastDHTReadMillis = 0;
float               humidity = 0;
float               temperature = 0;
float               tgtT;
int                 inputPin = A0;

IRAM_ATTR void handleRotary(){
    int MSB = digitalRead(pulseA);
    int LSB = digitalRead(pulseB);

    int encoded = (MSB << 1) | LSB;
    int sum = ( lastEncoded << 2) | encoded;
    if( sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011 ) encoderValue++;
    if( sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000 ) encoderValue--;
    lastEncoded = encoded;
    if ( encoderValue > 255 ){
      encoderValue = 255;
    }else if( encoderValue < 0 ){
      encoderValue = 0;
    }
  lastPublishMillis = millis() - pubInterval + 200; 
}

void gettemperature() {
    unsigned long currentMillis = millis();
    
   // if(currentMillis - lastDHTReadMillis >= interval) {
    if(currentMillis - lastDHTReadMillis >= interval) {
        //lastDHTReadMillis = currentMillis;

        humidity = dht.getHumidity();              // Read humidity (percent)
        temperature = dht.getTemperature();        // Read temperature as Fahrenheit
    }
}

void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");

    char dht_buffer[10];
    char dht_buffer2[10];
// USER CODE EXAMPLE : command handling
    gettemperature();
    display.setColor(BLACK);
    display.fillRect(80, 11, 100, 33);
    display.setColor(WHITE);
    sprintf(dht_buffer, "%2.1f", temperature);
    display.drawString(80, 11, dht_buffer);
    data["temperature"] = dht_buffer;
    tgtT = map(encoderValue, 0, 255, 10, 50);
    sprintf(dht_buffer2, "%2.1f", tgtT);
    data["target"] = dht_buffer2;
    display.drawString(80, 22, dht_buffer2);

    display.display();
// USER CODE EXAMPLE : command handling

    serializeJson(root, msgBuffer);
    client.publish(publishTopic, msgBuffer);
}

void handleUserCommand(JsonDocument* root) {
    JsonObject d = (*root)["d"];

    
// USER CODE EXAMPLE : status/change update
// code if any of device status changes to notify the change

    if(d.containsKey("target")) {
        tgtT = d["target"];
        encoderValue = map(tgtT, 10, 50, 0, 255);
        lastPublishMillis = - pubInterval;
    }

// USER CODE EXAMPLE
} 




void message(char* topic, byte* payload, unsigned int payloadLength) {
    byte2buff(msgBuffer, payload, payloadLength);
    StaticJsonDocument<512> root;
    DeserializationError error = deserializeJson(root, String(msgBuffer));
  
    if (error) {
        Serial.println("handleCommand: payload parse FAILED");
        return;
    }

    handleIOTCommand(topic, &root);
    if (!strncmp(updateTopic, topic, cmdBaseLen)) {
// USER CODE EXAMPLE : meta data update
// If any meta data updated on the Internet, it can be stored to local variable to use for the logic
// in cfg["meta"]["XXXXX"], XXXXX should match to one in the user_html

// USER CODE EXAMPLE
    } else if (!strncmp(commandTopic, topic, cmdBaseLen)) {            // strcmp return 0 if both string matches
        handleUserCommand(&root);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(pulseA,INPUT_PULLUP);
    pinMode(pulseB,INPUT_PULLUP);
    attachInterrupt(pulseA,handleRotary,CHANGE);
    attachInterrupt(pulseB,handleRotary,CHANGE);
    dht.setup(DHTPIN, DHTesp::DHT22);
    display.init(); 
    display.flipScreenVertically();
    display.drawString(35, 0, "IoT Thermostat");
    display.drawString(20, 11, "Current: ");
    display.drawString(20, 22, "Target: ");
    display.display();
    initDevice();

    // If not configured it'll be configured and rebooted in the initDevice(),
    // If configured, initDevice will set the proper setting to cfg variable

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    // main setup
    Serial.printf("\nIP address : "); Serial.println(WiFi.localIP());
// USER CODE EXAMPLE : meta data to local variable
    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? atoi((const char*)meta["pubInterval"]) : 0;
    lastPublishMillis = - pubInterval;
// USER CODE EXAMPLE
    
    set_iot_server();
    client.setCallback(message);
    iot_connect();
}

void loop() {
    if (!client.connected()) {
        iot_connect();
    }
// USER CODE EXAMPLE : main loop
//     you can put any main code here, for example, 
//     the continous data acquistion and local intelligence can be placed here
// USER CODE EXAMPLE
    client.loop();
    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }
}