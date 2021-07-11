#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <WiFiAP.h>
#include <EEPROM.h>
#include <ArduinoJson.h>



#define SPI_FLASH_SEC_SIZE 1024

#define DEFAULT_SSID ".@ SPU WiFi"
#define DEFAULT_PASS ""
#define LED_BUILTIN  2

String myApSsid = "Device-1";
String myApPass = "12345678";
String myId = "1";
String apSsid = DEFAULT_SSID;
String apPass = DEFAULT_PASS;
uint32_t last_time = 0;
uint32_t wait = 15000;

WebServer server(80);

void setup() {
  // h/w pin
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW); // turn on led

  // communication
  Serial.begin(115200);
  Serial.println();

  eepromRead();

  
  // try, sta mode
  Serial.println();
  Serial.println("STA_MODE_ON");
  Serial.print("Connecting to ");
  Serial.println(apSsid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(apSsid.c_str(), apPass.c_str());
  char temp[10];
  uint64_t id = ESP.getEfuseMac();
  sprintf(temp,"%04X",(uint16_t)(id >> 32));
  myApSsid = myApSsid + "-" + String(temp);
  sprintf(temp, "%08X",(uint32_t)id);
  myApSsid = myApSsid + String(temp);
  Serial.println("myApSsid: "+ myApSsid);
  
  } 
    

void loop() {
      
   //handleLed();
   server.handleClient();
   if (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    
     if(millis() - last_time > wait){
    last_time = millis();
    Serial.println();
    Serial.println("STA_MODE_OFF");
    Serial.println("WiFi disconnected");
    Serial.println("AP_MODE_ON");
    
    
    // try, ap mode
    WiFi.softAP(myApSsid.c_str(), myApPass.c_str());
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
  
    // web service
    server.on("/", handleRoot);
    //server.on("/inline", []() {server.send(200, "text/plain", "this works as well");});
    server.on("/apSetup", HTTP_GET, handleApSetupGet);
    server.on("/apData", HTTP_GET, handleApDataGet);
    server.on("/apData", HTTP_POST, handleApDataPost);
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("HTTP server started");
    
     
    // turn led off
    digitalWrite(LED_BUILTIN, HIGH); // turn off led
   
  }
  
  if(WiFi.status() == WL_CONNECTED)
  {
  Serial.println();
  Serial.println("AP_MODE_OFF");
  Serial.println("STA_MODE_ON");
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.stop();
 
  }
 }
   
      //SetAP();
 
}


void handleApDataPost(){
  if(server.args() !=1){
    server.send(400, "text/plain", "argument error");
  }
  else { //args =1
    String jsonString = server.arg(0);
    StaticJsonDocument<100> doc;
    DeserializationError err = deserializeJson(doc, jsonString);
    if(err){
      server.send(500,"text/plain","server error");
    }
    else { // no error
      String _apSsid = doc["ssid"].as<String>();
      String _apPass = doc["pass"].as<String>();
      server.send(200, "text/plain","ok");

       //protect write,duplicate data to eeprom
    if (_apSsid != apSsid || _apPass != apPass){
      apSsid = _apSsid;
      apPass = _apPass;
      eepromWrite();
    }
    }


   
  }
}
void eepromWrite(){
  // "@$"[n]<ssid>[m]<pass>
  char c;
  int addr = 0;
  unsigned char  s,i;
  
  EEPROM.begin(SPI_FLASH_SEC_SIZE);

  //write "@$"
  c = '@'; EEPROM.put(addr, c); addr ++;
  c = '$'; EEPROM.put(addr, c); addr ++;

  //ssid
  s = (unsigned char)apSsid.length(); EEPROM.put(addr, s); addr ++;
  for(i = 0;i < s;i++){
    c = apSsid[i]; EEPROM.put(addr, c); addr ++;
  }

  //pass
  s = (unsigned char)apPass.length(); EEPROM.put(addr, s); addr ++;
  for(i = 0;i < s;i++){
    c = apPass[i]; EEPROM.put(addr, c); addr ++;
  }

  EEPROM.end();
}

void eepromRead(){
  // "@$"[n]<ssid>[m]<pass>
  char c;
  int addr = 0;
  unsigned char  s,i;
  
  EEPROM.begin(SPI_FLASH_SEC_SIZE);

  //read and cheack "@$"
  char header[3] = {' ',' ','\0'};
  EEPROM.get(addr, header[0]);addr++;
  EEPROM.get(addr, header[1]);addr++;

  if(strcmp(header, "@$") != 0){
    eepromWrite();
    //return;
  }else{
    //ssid
    EEPROM.get(addr, s); addr++;
    apSsid = "";
    for(i = 0; i < s;i ++){
      EEPROM.get(addr, c); apSsid = apSsid + c; addr ++;
    }

    //pass
     EEPROM.get(addr, s); addr++;
    apPass = "";
    for(i = 0; i < s;i ++){
      EEPROM.get(addr, c); apPass = apPass  + c; addr ++;
    }
  }
}
  
void handleApDataGet() {
  digitalWrite(LED_BUILTIN, LOW);
  String str = "{\"ssid\":\"" + apSsid + "\", \"pass\":\"" + apPass + "\"}";
  server.send(200, "text/json", str);
  digitalWrite(LED_BUILTIN, HIGH);
}

void handleApSetupGet() {
  digitalWrite(LED_BUILTIN, LOW);
  String str = "<html><head> <title>AP Setup</title> <style>body{font-family: monospace; font-size: 28px;}input{width: 100%; border: 1px solid blue; border-radius: 5px; font: inherit; font-weight: bold; color: blue; outline: none; display: block;}label{font-weight: bold;}button{width: 31%; font: inherit; border: 1px solid blue; border-radius: 10px; outline: none;}#button-group{text-align: center;}</style></head><body> <center> <h1>AP Setup</h1> </center> <div > <label>SSID:</label> <input type='input' id='ssid' placeholder='<ssid>'><br><label>PASS:</label> <input type='input' id='pass' placeholder='<pass>'><br></div><div id='button-group'> <button id='submit'>Submit</button> <button id='clear'>Clear</button> <button id='reload'>Reload</button> </div><script type='text/javascript'> document.getElementById('clear').onclick=function (){document.getElementById('ssid').value=''; document.getElementById('pass').value='';}; document.getElementById('reload').onclick=function(){console.log('reload button is clicked...'); var xmlHttp=new XMLHttpRequest(); xmlHttp.onreadystatechange=function (){if (xmlHttp.readyState==XMLHttpRequest.DONE){if (xmlHttp.status==200){console.log(xmlHttp.responseText); res=JSON.parse(xmlHttp.responseText); document.getElementById('ssid').value=res.ssid; document.getElementById('pass').value=res.pass;}else{console.log('fail');}}else{}}; xmlHttp.open('GET', '/apData'); xmlHttp.send();}; document.getElementById('submit').onclick=function(){console.log('submit button is clicked...'); var xmlHttp=new XMLHttpRequest(); xmlHttp.onreadystatechange=function (){if (xmlHttp.readyState==XMLHttpRequest.DONE){if (xmlHttp.status==200){alert('Success....');}else{alert('Fail....');}}else{}}; var data=JSON.stringify({ssid: document.getElementById('ssid').value, pass: document.getElementById('pass').value}); xmlHttp.open('POST', '/apData'); xmlHttp.send(data);}; document.getElementById('reload').click(); </script></body></html>";
  server.send(200, "text/html", str);
  
  digitalWrite(LED_BUILTIN, HIGH);
}

void handleLed() {
  uint32_t cur = millis();
  static uint32_t next = cur + 1000;
  if (cur >= next) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    next = cur + 1000;
  }
}

void handleRoot() {
  digitalWrite(LED_BUILTIN, LOW);
  server.send(200, "text/html", "<H1>Hello, my id is "+myId+"</H1>");
  digitalWrite(LED_BUILTIN, HIGH);
}

void handleNotFound() {
  digitalWrite(LED_BUILTIN, LOW);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(LED_BUILTIN, HIGH);
}
