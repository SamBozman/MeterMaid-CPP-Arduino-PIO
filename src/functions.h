#pragma once
#include "declarations.h"

//*******************************************************************************
void check_status() //TODO Eventually get rid of this function
{
  static ulong checkstatus_timeout = 0;
#define HEARTBEAT_INTERVAL 10000L
  // Print hearbeat every HEARTBEAT_INTERVAL (10) seconds.
  if ((millis() > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    static int num = 1;

    if (WiFi.status() == WL_CONNECTED)
      Serial.print("^"); // ^ means connected to WiFi
    else
      Serial.print("x"); // x means not connected to WiFi

    if (num == 80)
    {
      Serial.println();
      num = 1;
    }
    else if (num++ % 10 == 0)
    {
      Serial.print(" ");
    }
    checkstatus_timeout = millis() + HEARTBEAT_INTERVAL;
  }
}
//*********************************************************
void checkPmiDue()
{

  const long int secondsInaDay = 86400; // Number of seconds in a day
  //const long int minDate = 1264695154;
  const int PMI_days = 30 * PMI_Months;
  unsigned long int unixLastPmi;
  //int int_HourMeter = atoi(HourMeter);
  //int int_PMI_Hrs = atoi(PMI_Hrs);

  // check for PMI due by months
  char *ptr_strTime = Last_PMI;
  struct tm tm;
  time_t ts;
  memset(&tm, 0, sizeof(struct tm));
  strptime(ptr_strTime, "%Y-%m-%d", &tm);
  ts = mktime(&tm);
  unixLastPmi = (int)ts;
  int Over = 0;
  int daysSinceLastPMI = (CurrentTime - unixLastPmi) / secondsInaDay;
  cout << "Days since last pmi = " << daysSinceLastPMI << endl;
  if (daysSinceLastPMI >= PMI_days)
  {
    Over = daysSinceLastPMI - PMI_days;
    cout << "PMI DUE by Months" << endl;
    StaticJsonDocument<256> jsonObj;
    jsonObj["UnitID"] = UnitID;
    jsonObj["Note"] = "Days overdue:";
    jsonObj["Last_PMI"] = Last_PMI;
    jsonObj["HourMeter"] = HourMeter;
    jsonObj["Overdue_by"] = Over;
    jsonObj["Triggered_by"] = "Date";
    char buffer[256];
    serializeJson(jsonObj, buffer);
    mqttClient.publish("PMI_due", buffer);
  }

  // check for PMI due by Hourmeter
  cout << "HourMeter = " << HourMeter << endl;
  cout << "PMI_Hrs = " << PMI_Hrs << endl;

  if (HourMeter >= PMI_Hrs)
  {
    Over = HourMeter - PMI_Hrs;
    cout << "PMI DUE by Hourmeter" << endl;
    StaticJsonDocument<256> jsonObj;
    jsonObj["UnitID"] = UnitID;
    jsonObj["Note"] = "Hours overdue:";
    jsonObj["Last_PMI"] = Last_PMI;
    jsonObj["HourMeter"] = HourMeter;
    jsonObj["Overdue_by"] = Over;
    jsonObj["Triggered_by"] = "HourMeter";
    char buffer[256];
    serializeJson(jsonObj, buffer);
    mqttClient.publish("PMI_due", buffer);
  }
}
//*********************************************************
void updateConfig(byte *payload, unsigned int length)
{
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error)
  {
    Serial.print(F("getConfig download failed!"));
    Serial.println(error.c_str());
    return;
  }
  Serial.println(F("Retrieved Unit config file!"));
  JsonObject arr0 = doc[0];
  //serializeJsonPretty(doc, Serial);
  Serial.println();

  // Extract the downloaded values
  const char *UID = arr0["UnitID"];
  int P_Mths = arr0["PMI_Months"];
  int P_Hrs = arr0["PMI_Hrs"];
  int U_Hrs = arr0["Preset_Hrs"];
  const char *P_UH = arr0["Last_PMI"];

  boolean cmp_flag = false; //set control flag
  //Compare downloaded values to stored values
  if (!(U_Hrs <= -1))
    cmp_flag = true;
  if (!strcmp(UID, UnitID) == 0)
    cmp_flag = true;
  if (!(P_Mths == PMI_Months))
    cmp_flag = true;
  if (!(P_Hrs == PMI_Hrs))
    cmp_flag = true;
  if (!strcmp(P_UH, Last_PMI) == 0)
    cmp_flag = true;

  if (cmp_flag)
  { //If anything has changed then...
    if (!(U_Hrs <= -1))
    { //only change HourMeter if download is anthing but -1
      //itoa(U_Hrs, HourMeter, 10);// int to array of base type
      HourMeter = U_Hrs;
    }
    strcpy(UnitID, UID);
    //itoa(P_Mths, PMI_Months, 10);
    PMI_Months = P_Mths;
    //itoa(P_Hrs, PMI_Hrs, 10);
    PMI_Hrs = P_Hrs;

    strcpy(Last_PMI, P_UH);
    Serial.println(F("Config Updated and confirmConfig puplished!"));
    mqttClient.publish("confirmConfig", UnitID); //Confirm Update back to DB
    saveConfig();                                // Save changes to flash
  }
  else
  {
    Serial.println(F("Config was not changed!"));
  }
  checkPmiDue();
  Serial.println();
}
//*********************************************************
void dataInCallback(char *topic, byte *payload, unsigned int length)
{
  // getConfig
  if (strcmp(topic, ClientID) == 0)
    updateConfig(payload, length);
  else // get timestamp
      if (strcmp(topic, ClientID_t) == 0)
    getTime(payload, length);
}
//*********************************************************
void getTime(byte *payload, unsigned int length)
{
  cout << "Topic = getTime " << endl;
  if (length == (13))
  {
    char time[length - 2];
    for (int i = 0; i < length - 3; i++)
    {
      time[i] = (char)payload[i];
    }
    time[length - 3] = '\0';
    CurrentTime = strtol(time, NULL, 10);
    cout << "Current UnixTimestamp is: " << CurrentTime << endl;
  }
}
//*********************************************************
void readConfig()
{
  if (SPIFFS.begin(true))
  {
    Serial.println("Opened SPIFFS file system");
    if (SPIFFS.exists("/config.json"))
    {
      Serial.println(F("reading config file"));
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println(F("opened config file"));

        StaticJsonDocument<256> json;

        DeserializationError error = deserializeJson(json, configFile);
        if (error)
        {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.c_str());
          configFile.close();
          json.clear();
          SPIFFS.end();
          return;
        }
        Serial.println(F("deserializeJson() worked!! "));
        serializeJsonPretty(json, Serial);
        Serial.println();

        strcpy(UnitID, json["UnitID"]);
        HourMeter = json["HourMeter"];
        PMI_Months = json["PMI_Months"];
        PMI_Hrs = json["PMI_Hrs"];
        strcpy(Last_PMI, json["Last_PMI"]);
        strcpy(Last_MQTT, json["Last_MQTT"]);
        configFile.close();
        json.clear();
      }
      else
      {
        Serial.println(F("Could not read config file"));
      }
    }
    else
    {
      Serial.println(F("Config file does not exist"));
    }
  }
  else
  {
    Serial.println(F("failed to mount FS"));
  }

  SPIFFS.end();
}
//*********************************************************
bool mqttConnect()
{
  if (mqttClient.connect(ClientID))
  {
    Serial.println(F("mqttClient connected"));
    mqttClient.subscribe(ClientID);
    Serial.println(F("Subscribed to ClientID"));
    mqttClient.subscribe(ClientID_t);
    Serial.println(F("Subscribed to ClientID_t"));
    return true;
  }
  else
  {
    Serial.println(F("MqttConnect Failed!"));
    Serial.print(F("RC error = :"));
    Serial.println(F("failed, rc="));
    Serial.print("MqttClient State = :");
    Serial.println(mqttClient.state());
    return false;
  }
}
//*********************************************************
void saveConfig()
{
  Serial.println(F("saving config"));

  if (SPIFFS.begin())
  {
    Serial.println(F("mounted file system"));
    if (SPIFFS.exists("/config.json"))
    {
      Serial.println("removing config.json file");
      SPIFFS.remove("/config.json");
    }

    File configFile = SPIFFS.open("/config.json", "w");

    if (configFile)
    {
      Serial.println("writing config file");
      StaticJsonDocument<256> json;

      json["UnitID"] = UnitID;
      json["HourMeter"] = HourMeter;
      json["PMI_Months"] = PMI_Months;
      json["PMI_Hrs"] = PMI_Hrs;
      json["Last_PMI"] = Last_PMI;
      json["Last_MQTT"] = Last_MQTT;

      serializeJson(json, configFile);
      serializeJsonPretty(json, Serial);

      Serial.println();
      json.clear();
      configFile.close();
      Serial.println(F("Config was saved to SPIFFS!"));
      SPIFFS.end();
    }
  }
  else
  {
    Serial.println(F("failed to mount FS"));
  }
}
//*********************************************************
void addToHM()
{
  // Adds whole number of division to HM. Remainer ignored
  HourMeter += RTC_totTime / 3600;
  // Yeilds the remainder of divishion. Whole number ignored
  RTC_totTime = RTC_totTime % 3600;
  saveConfig();
}
//*********************************************************
void goToSleep()
{
  // If you saved every hour, 24 hours a day, every day you would take 11 years
  // before you reached the 100,000 save limit of the SPIFFS system
  stopTime = millis();
  runTime += (stopTime - startTime) / 1000; //# of seconds unit was running
  RTC_totTime += runTime;

  if (RTC_totTime >= 3600) // 1 hour
  {
    addToHM();
  }

  Serial.println("Going to sleep now");
  pinMode(RUN_SENSOR, INPUT_PULLUP);
  esp_deep_sleep_start();
}
//*******************************************************************************
void configWiFi()
{
  unsigned long startedAt = millis();
  ESP_WiFiManager ESP_wifiManager("ConfigOnSwitch");

  ESP_wifiManager.setMinimumSignalQuality(8);
  // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5

  Router_SSID = ESP_wifiManager.WiFi_SSID();
  Router_Pass = ESP_wifiManager.WiFi_Pass();
  Serial.println("Stored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);

  if (Router_SSID == "")
  {
    openAP();
  }

#define WIFI_CONNECT_TIMEOUT 30000L
#define WHILE_LOOP_DELAY 200L
#define WHILE_LOOP_STEPS (WIFI_CONNECT_TIMEOUT / (3 * WHILE_LOOP_DELAY))

  startedAt = millis();

  while ((WiFi.status() != WL_CONNECTED) &&
         (millis() - startedAt < WIFI_CONNECT_TIMEOUT))
  {
    WiFi.mode(WIFI_STA);
    WiFi.persistent(true);
    // We start by connecting to a WiFi network

    Serial.print("Attempting to connect to ");
    Serial.println(Router_SSID);

    WiFi.begin(Router_SSID.c_str(), Router_Pass.c_str());

    int i = 0;
    while ((!WiFi.status() || WiFi.status() >= WL_DISCONNECTED) &&
           i++ < WHILE_LOOP_STEPS)
    {
      delay(WHILE_LOOP_DELAY);
    }
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("connected. Local IP: ");
    Serial.println(WiFi.localIP());
  }
  else
    Serial.println(ESP_wifiManager.getStatus(WiFi.status()));
}
//*********************************************************
void openAP()
{
  Serial.println("\nConfiguration portal requested.");
  digitalWrite(AP_LED, HIGH); // blue led on for configuration mode

  ESP_WiFiManager ESP_wifiManager; // Local intialization.

  // Check if there is stored WiFi router/password credentials.
  // If not found, device will remain in configuration mode until switched off
  // via webserver.
  Serial.print("Opening configuration portal. ");
  Router_SSID = ESP_wifiManager.WiFi_SSID(); //TODO always keep timeout??
  if (Router_SSID != "")
  {
    ESP_wifiManager.setConfigPortalTimeout(
        60);
    Serial.println("Got stored Credentials. Timeout 60s");
  }
  else
    Serial.println("No stored Credentials. No timeout");

  // blocking loop awaiting configuration
  if (!ESP_wifiManager.startConfigPortal((const char *)ClientID, apPwd))
  {
    Serial.println("Not connected to WiFi but continuing anyway.");
  }
  else
  {
    // if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }

  digitalWrite(AP_LED, LOW); // Configuration LED (blue) off
}
