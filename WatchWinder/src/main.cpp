#include <Arduino.h>
#include <WiFi.h>
#include <EEPROM.h>
#define EEPROM_SIZE 230

const int M1 = 13; //Stepper Driver Pin 1
const int M2 = 12; //Stepper Driver Pin 2
const int M3 = 14; //Stepper Driver Pin 3
const int M4 = 27; //Stepper Driver Pin 4
const int steps_per_revolution = 510;
const long timeout_seconds = 3000;
const char *ssid = "Mom, use this one";
const char *password = "allcapstwowords";

struct configObject {
  int  stepperSpeed;
  int  oscillationsInterval;
  int  intervalPause;
};
const int eeAddress  = 10;

WiFiServer server(80);
bool status = false;
bool wave = true;
int stepper_speed = 50;
int oscillations_per_interval = 20;
int interval_pause = 10;
int step_counter = 0;
int stepperPhase = 3;
int oscillation_counter = 0;
bool direction = true;
bool interval_timer_reset = false;
String config_string;
String status_string;
String status_message = "Stopped";
String header;

unsigned long current_time = millis();
unsigned long previous_time = 0;
unsigned long previous_interval_time = 0;


void load_config()
{
  configObject config;
  EEPROM.get(eeAddress, config);
  stepper_speed = config.stepperSpeed;
  oscillations_per_interval = config.oscillationsInterval;
  interval_pause = config.intervalPause;
}

void save_config()
{
  configObject customVar = {
    stepper_speed,
    oscillations_per_interval,
    interval_pause
  };
  EEPROM.put(eeAddress, customVar);
  EEPROM.commit();
}
void startWifi(){
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void startHotspot(){
   WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  server.begin();
}
void gpioPins(){
  pinMode(M1, OUTPUT);
  pinMode(M2, OUTPUT);
  pinMode(M3, OUTPUT);
  pinMode(M4, OUTPUT);
}

void setup()
{
  Serial.begin( 9600);
  startWifi();
  server.begin();
  EEPROM.begin(EEPROM_SIZE);
  load_config();
  gpioPins();
}

void wavePhase(int i){
stepperPhase = 3;
switch (i)
  {
  case 0:
    digitalWrite(M1, HIGH);
    digitalWrite(M2, LOW);
    digitalWrite(M3, LOW);
    digitalWrite(M4, LOW);
    break;
  case 1:
    digitalWrite(M1, LOW);
    digitalWrite(M2, HIGH);
    digitalWrite(M3, LOW);
    digitalWrite(M4, LOW);
    break;
  case 2:
    digitalWrite(M1, LOW);
    digitalWrite(M2, LOW);
    digitalWrite(M3, HIGH);
    digitalWrite(M4, LOW);
    break;
  case 3:
    digitalWrite(M1, LOW);
    digitalWrite(M2, LOW);
    digitalWrite(M3, LOW);
    digitalWrite(M4, HIGH);
    break;
  } 
}
void dulePhase(int i){
  stepperPhase = 7;
switch (i)
  {
  case 0:
    digitalWrite(M1, LOW);
    digitalWrite(M2, LOW);
    digitalWrite(M3, LOW);
    digitalWrite(M4, HIGH);
    break;
  case 1:
    digitalWrite(M1, LOW);
    digitalWrite(M2, LOW);
    digitalWrite(M3, HIGH);
    digitalWrite(M4, HIGH);
    break;
  case 2:
    digitalWrite(M1, LOW);
    digitalWrite(M2, LOW);
    digitalWrite(M3, HIGH);
    digitalWrite(M4, LOW);
    break;
  case 3:
    digitalWrite(M1, LOW);
    digitalWrite(M2, HIGH);
    digitalWrite(M3, HIGH);
    digitalWrite(M4, LOW);
    break;
  case 4:
    digitalWrite(M1, LOW);
    digitalWrite(M2, HIGH);
    digitalWrite(M3, LOW);
    digitalWrite(M4, LOW);
    break;
  case 5:
    digitalWrite(M1, HIGH);
    digitalWrite(M2, HIGH);
    digitalWrite(M3, LOW);
    digitalWrite(M4, LOW);
    break;
  case 6:
    digitalWrite(M1, HIGH);
    digitalWrite(M2, LOW);
    digitalWrite(M3, LOW);
    digitalWrite(M4, LOW);
    break;
  case 7:
    digitalWrite(M1, HIGH);
    digitalWrite(M2, LOW);
    digitalWrite(M3, LOW);
    digitalWrite(M4, HIGH);
    break;
  }
}

void pulse_stepper(int i, bool dir)
{
  if(wave){
    wavePhase(i);
  } else {
    dulePhase(i);
  };
  
  int ramp = steps_per_revolution - i * 10;
  if (ramp > 0)
  {
    delayMicroseconds(ramp);
  }
  delayMicroseconds(750 + 50 * (100 - stepper_speed));
}

void move_stepper()
{
  if (direction == true)
  {
    for (int i = 0; i <= stepperPhase; i++)
    {
      pulse_stepper(i, true);
    }
  }
  else
  {
    for (int i = stepperPhase; i >= 0; i--)
    {
      pulse_stepper(i, true);
    }
  }
}

void stop_stepper()
{
  digitalWrite(M1, LOW);
  digitalWrite(M2, LOW);
  digitalWrite(M3, LOW);
  digitalWrite(M4, LOW);
}

void setPhase(){
  wave = !wave;
}

void winding_function()
{
  if (status)
  {
    if (step_counter == 0)
    {
      direction = true;
    }
    if (step_counter == steps_per_revolution)
    {
      oscillation_counter++;
      direction = false;
    }
    if (oscillation_counter >= oscillations_per_interval && step_counter == 0)
    {
      if (!interval_timer_reset)
      {
        previous_interval_time = millis();
        interval_timer_reset = true;
        stop_stepper();
      }
      else
      {
        int minutes = interval_pause - (millis() - previous_interval_time) / 60000;
        status_message = "Next interval in " + String(minutes) + " minutes";
      }
      if (millis() - previous_interval_time >= interval_pause * 60000 || millis() - previous_interval_time < 0)
      {
        interval_timer_reset = false;
        oscillation_counter = 0;
        status_message = "Running";
      }
    }
    else
    {
      status_message = "Running";
      move_stepper();
      if (direction)
      {
        step_counter++;
      }
      else
      {
        step_counter--;
      }
    }
  }
  else
  {
    stop_stepper();
  }
}

void webconfig_function()
{
  WiFiClient client = server.available();
  if (client)
  {
    current_time = millis();
    previous_time = current_time;
    String currentLine = "";
    while (client.connected() && current_time - previous_time <= timeout_seconds)
    {
      current_time = millis();
      if (client.available())
      {
        char c = client.read();
        header += c;
        if (c == '\n')
        {
          if (currentLine.length() == 0)
          {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            if (header.indexOf("/wwStatusChange/") >= 0)
            {
              status_string = header.substring(0, header.indexOf("HTTP"));
              status_string = status_string.substring(status_string.lastIndexOf("wwStatusChange") + 15);
              if (status_string.indexOf("start") >= 0)
              {
                status = true;
                status_message = "Running";
              }
              if (status_string.indexOf("stop") >= 0)
              {
                status = false;
                status_message = "Stopped";
                oscillation_counter = 0;
              }
            }
            if (header.indexOf("/config/") >= 0)
            {
              int old_stepper_speed = stepper_speed;
              int old_oscillations_per_interval = oscillations_per_interval;
              int old_interval_pause = interval_pause;
              config_string = header.substring(0, header.indexOf("HTTP"));
              config_string = config_string.substring(config_string.lastIndexOf("/config/") + 8);
              stepper_speed = config_string.substring(0, config_string.indexOf("/")).toInt();
              config_string = config_string.substring(config_string.indexOf("/") + 1);
              oscillations_per_interval = config_string.substring(0, config_string.indexOf("/")).toInt();
              config_string = config_string.substring(config_string.indexOf("/") + 1);
              interval_pause = config_string.toInt();
              if (old_stepper_speed != stepper_speed || old_oscillations_per_interval != oscillations_per_interval || old_interval_pause != interval_pause)
              {
                save_config();
              }
            }
            if (header.indexOf("/wave") >= 0) {setPhase();}
            client.println("<!DOCTYPE html><html lang=\"en\">");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" charset=\"utf-8\">");
            client.println("<title>Watch Winder Configuration</title>");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center; background-color: #252b31; color:#f6fafb;}");
            client.println(".button { background-color: #5e6668; border: none; color: #c1c8c7; padding: 16px 40px;text-decoration: none; font-size: 30px; margin: 20px; cursor: pointer;}");
            client.println(".button:hover {background-color: #9fc2d4; color: #f6fafb;}");
            client.println(".slidecontainer {width: 100%;}");
            client.println(".slider {width: 100%; background: #5e6668; outline: none;}");
            client.println(".slider:hover {background: #c1c8c7;}");
            client.println(".slider::-moz-range-thumb {width: 25px; height: 25px; background: #9fc2d4;cursor: pointer;}</style></head>");
            //Content
            client.println("<body><h1>Watch Winder Configuration</h1>");
            client.println("<h2>Status: " + status_message + "</h2>");
            //Stepper Speed Control
            client.println("<p>Stepper Speed: <span id=\"speedSliderOutput\"></span> %</p>");
            client.println("<div class=\"slidecontainer\"><input type=\"range\" min=\"10\" max=\"100\" value=\"" + (String)stepper_speed + "\" class=\"slider\" id=\"speedSlider\" step=\"10\"></div>");
            //oscillations Control
            client.println("<p>Oscillations per Interval: <span id=\"oscillationsSliderOutput\"></span></p>");
            client.println("<div class=\"slidecontainer\"><input type=\"range\" min=\"1\" max=\"500\" value=\"" + (String)oscillations_per_interval + "\" class=\"slider\" id=\"oscillationsSlider\" step=\"1\"></div>");
            //Interval Control
            client.println("<p>Pause between Intervals: <span id=\"intervalSliderOutput\"></span> min</p>");
            client.println("<div class=\"slidecontainer\"><input type=\"range\" min=\"0\" max=\"1000\" value=\"" + (String)interval_pause + "\" class=\"slider\" id=\"intervalSlider\" step=\"1\"></div>");
            //Buttons
            client.println("<form style=\"display: inline\" action=\"null\" method=\"get\" id=\"saveForm\"><button class=\"button\">Save</button></form>");
            client.println("<p><a href=\"/resetSettings\"><button class=\"button\">resetSettings</button></a></p>");
            if(wave){
            client.println("<p><a href=\"/wave\"><button class=\"button\">wave</button></a></p>");
            } else{
              client.println("<p><a href=\"/wave\"><button class=\"button\">dule</button></a></p>");
            }
            if (status)
            {
              client.println("<form style=\"display: inline\" action=\"wwStatusChange/stop\" method=\"get\" id=\"stopButton\"><button class=\"button\">Stop</button></form>");
            }
            else
            {
              client.println("<form style=\"display: inline\" action=\"wwStatusChange/start\" method=\"get\" id=\"startButton\"><button class=\"button\">Start</button></form>");
            }
            //Script
            client.println("<script>");
            //Speed Slider
            client.println("var slider1 = document.getElementById(\"speedSlider\");");
            client.println("var output1 = document.getElementById(\"speedSliderOutput\");");
            client.println("output1.innerHTML = slider1.value;");
            client.println("slider1.oninput = function() {");
            client.println("output1.innerHTML = this.value;");
            client.println("document.getElementById(\"saveForm\").action = \"config/\"+document.getElementById(\"speedSlider\").value + \"/\" + document.getElementById(\"oscillationsSlider\").value + \"/\" + document.getElementById(\"intervalSlider\").value+\"/\";}");
            //oscillation Slider
            client.println("var slider2 = document.getElementById(\"oscillationsSlider\");");
            client.println("var output2 = document.getElementById(\"oscillationsSliderOutput\");");
            client.println("output2.innerHTML = slider2.value;");
            client.println("slider2.oninput = function() {");
            client.println("output2.innerHTML = this.value;");
            client.println("document.getElementById(\"saveForm\").action = \"config/\"+document.getElementById(\"speedSlider\").value + \"/\" + document.getElementById(\"oscillationsSlider\").value + \"/\" + document.getElementById(\"intervalSlider\").value+\"/\";}");
            //Interval Slider
            client.println("var slider3 = document.getElementById(\"intervalSlider\");");
            client.println("var output3 = document.getElementById(\"intervalSliderOutput\");");
            client.println("output3.innerHTML = slider3.value;");
            client.println("slider3.oninput = function() {");
            client.println("output3.innerHTML = this.value;");
            client.println("document.getElementById(\"saveForm\").action = \"config/\"+document.getElementById(\"speedSlider\").value + \"/\" + document.getElementById(\"oscillationsSlider\").value + \"/\" + document.getElementById(\"intervalSlider\").value+\"/\";}");
            client.println("</script>");
            client.println("</body></html>");
            // HTTP response ends with blank line
            client.println();
            break;
          }
          else
          {
            currentLine = "";
          }
        }
        else if (c != '\r')
        {
          currentLine += c;
        }
      }
    }
    header = "";
    client.stop();
  }
}



















void loop()
{
  webconfig_function();
  winding_function();
}
