#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

#include "config.h"

WiFiClient espClient;
PubSubClient client(espClient);

bool RelayON = false;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(SSID);
  WiFi.begin(SSID, PASSWORD);
  WiFi.mode(WIFI_STA);   // to disable the access point mode
  WiFi.hostname(RELAY_NAME);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
    //ESP.restart();
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(RELAY_NAME);

  // No authentication by default
  // ArduinoOTA.setPASSWORD("admin");

  // PASSWORD can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPASSWORDHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  char subscribe_topic[80];
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(RELAY_NAME)) {
      Serial.println("connected");
      sprintf(subscribe_topic, "ha/%s/cmd",RELAY_NAME);
       client.subscribe(subscribe_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

#define MAX_MSG_LEN 10
void mqtt_callback(char *topic, byte *payload, unsigned int length) {
  // copy payload to a static string
  static char message[MAX_MSG_LEN+1];
  if (length > MAX_MSG_LEN) {
    length = MAX_MSG_LEN;
  }
  strncpy(message, (char *)payload, length);
  message[length] = '\0';
  
  Serial.printf("topic %s, message received: %s\n", topic, message);

  // decode message
  if (strcmp(message, "off") == 0) {
    RelayON = false;
  } else if (strcmp(message, "on") == 0) {
    RelayON = true;
  }
}


void setup(void)
{
  // start serial port
  Serial.begin(115200);
  // built-in LED for reporting the stats of the controller.
  // solid on  -- cannot access WIFI or cannot connect to the MQTT server
  // slow blinking  -- off state
  // fast blinking  -- on state
  pinMode(LED_BUILTIN, OUTPUT);    
  setup_wifi(); 
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(mqtt_callback);
  pinMode(RELAY_OUT_PIN, OUTPUT); 
  digitalWrite(RELAY_OUT_PIN,!PIN_LOGIC_WHEN_ON);  // turn relay initially off.

}


void loop(void)
{ 
  static int hist_cnt = 0;
  static bool built_in_LED_on = false;

  ArduinoOTA.handle();

  digitalWrite(LED_BUILTIN,built_in_LED_on);
  hist_cnt++;
  if(RelayON)
  {
    // fast blinking (10 Hz) when relay is on
    built_in_LED_on = !built_in_LED_on;  
    hist_cnt = 0;
  }
  else
  {
    // slow blinking (1Hz) when relay is off
    if(hist_cnt > 10) 
    {
      hist_cnt = 0;
      built_in_LED_on = !built_in_LED_on;  
    }
  }
  if (!client.connected())
    reconnect();

  client.loop();
  delay(100);

  if(RelayON)
    digitalWrite(RELAY_OUT_PIN,PIN_LOGIC_WHEN_ON);
  else
    digitalWrite(RELAY_OUT_PIN,!PIN_LOGIC_WHEN_ON);     
 
}
