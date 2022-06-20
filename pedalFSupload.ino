#include <Wire.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <FS.h>
#include <WebSocketsServer.h>



#ifndef STASSID
#define STASSID "wifi_network_id"
#define STAPSK  "your_password"
#endif

const char* mdnsName = "esp8266";

byte a; //temporary to hold each byte of data whilst being written to the eeprom chip 
int pinA =0;
int pinB =15;  
int pinC =2;
int pinD =13;
int pinUp = 12;
int pinDown = 14;
int counter=0;
unsigned long bytecounter=0;// used to store which effect is selected
unsigned long timesince = 0; //count time in millis since last effect select switch change
unsigned long debounce =1000; // change to make switch more or less sensitive


int BCD[4]; //an array with four positions, one for each pin of FV1 effect select pins



File fsUploadFile;              // a File object to temporarily store the received file
File newrom;

String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)
void handleFileUpload();     // upload a new file to the SPIFFS

WebSocketsServer webSocket(81);
ESP8266WebServer server(80);



void setup() {
//Serial for debugging
Serial.begin(115200);
SPIFFS.remove("effect.hex");
//Start wifi and web server
MDNS.begin(mdnsName);
MDNS.addService("http", "tcp", 80);
WiFi.mode(WIFI_AP_STA);
WiFi.begin(STASSID, STAPSK);


// Start the SPI Flash Files System

SPIFFS.begin();    
Wire.begin();

//Handle user web requests and which html page to send to browser.                      
  server.on("/upload", HTTP_GET, []() {             
    if (!handleFileRead("/upload.html"))                
      server.send(404, "text/plain", "404: Not Found"); 
  });

  server.on("/upload", HTTP_POST,                       // if the client posts to the upload page
    [](){ server.send(200); },                          // Send status 200 (OK) to tell the client we are ready to receive
    handleFileUpload                                    // Receive and save the file
  );

  server.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

// Actually start the server
server.begin();                           
  
Serial.println("HTTP server started");
  
//Start Websocket service
startWebSocket();
Serial.println("Websocket service started");

//Set output pins and toggle switch pins
pinMode(pinA, OUTPUT); // 
pinMode(pinB, OUTPUT); //
pinMode(pinC, OUTPUT); //
pinMode(pinD, OUTPUT);// 
pinMode(pinUp, INPUT_PULLUP);
pinMode(pinDown, INPUT_PULLUP);
//set pedal to select program 1 at startup

  digitalWrite(pinA, LOW); 
  
  digitalWrite(pinB, LOW); 
  
  digitalWrite(pinC, LOW); 
  
  digitalWrite(pinD, LOW); 
}

void loop() {

server.handleClient();
webSocket.loop();  
  
//capture change to counter 
if (digitalRead(pinUp) == LOW) {
if((millis()- timesince) > debounce){
 counter++;
 delay(500);
  }
}
if (digitalRead(pinDown) == LOW) {
if((millis()- timesince) > debounce){
 counter--;
  delay(500);
  }
}
//reset counter if reach 15 (effect 16)
if(counter >15){
  counter = 0;
  }
//loop counter back to effect 16 if reach 0 (effect 1)
if(counter <0){
  counter = 15;
  }
/* Convert counter, a two byte (16bit number) to an array of 4bits. A bit for each effect selection pin. 0-15 will only use the first byte to store data, 
   so use lowbyte to access this byte. Loop through bit 0 -  3 and read them into array
*/
 for (int n = 0; n<=3; n++){
  BCD[n]=bitRead(lowByte(counter),n);
  }
Serial.println("Current effect: ");
Serial.println(counter);

//write to each output according to the array
  digitalWrite(pinA, BCD[0]); 
  digitalWrite(pinB, BCD[1]); 
  digitalWrite(pinC, BCD[2]); 
  digitalWrite(pinD, BCD[3]);  
}




//Addition functions 
String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  else if (filename.endsWith(".hex")) return "application/octet-stream";
  return "text/plain";
}


bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "control.html";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  
  if (SPIFFS.exists(path)) {                                      
    File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

void handleFileUpload(){ // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print("handleFileUpload Name: "); Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w+");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) {                                    // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      Serial.print(filename);
      //writeprom
    }
if(SPIFFS.exists("/effect.hex")){
Serial.println("Found HEX");
File newrom = SPIFFS.open("/effect.hex","r+"); 
//write to eeprom one bye at a time 
for (unsigned long bytecounter  = 0; bytecounter < 4096; bytecounter++)
{
//seek allows to 'seek' to byte number set by bytecounter
if(newrom.seek(bytecounter)){
  ;//go to byte number x, at start bytecounter 0: goto start of file

a=newrom.peek();//peek accesses the next byte in the file from current position. At start next byte is the first byte
Serial.println("Writing new effects");  

/* write address of byte (0 for first 1 for second etc...) tells eeprom where to store recieved bytes. At start bytecounter = 0
 *  as bytecounter is a int stored by the esp8266 as 2 bytes of information we tell the eemprom the address of the MSB (1st byte) then that the second byte (LSB) should be 256 buts
 *  ahead
 */
Wire.beginTransmission(0x50);
Wire.write((int)highByte(bytecounter));
  Wire.write((int)lowByte(bytecounter));
//eeprom has recieved the two bytes that count the position of the byte to be written in the file. The next byte will be the actual data from the file
Wire.write(a); //write the byte accessed by peek
Wire.endTransmission();

//give sometime to for the eeprom to process/write the byte. Loop back for every byte to write (bytes 0-4095 for 32kbit 32*1024)
delay(10);
}
else
{Serial.print("fail");
}}
    Serial.println("Finished flashing HEX");
    SPIFFS.remove("effect.hex");

    

      server.sendHeader("Location","/success.html");      // Redirect the client to the success page
      server.send(303); 
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}



void startWebSocket() { // Start a WebSocket server
  webSocket.begin();                          // start the websocket server
  webSocket.onEvent(webSocketEvent);          // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("WebSocket server started.");
  
}



void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) { // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {              // if a new websocket connection is established
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
       
      }
      break;
    case WStype_TEXT:                     // if new text data is received
      
      if (payload[0] == '#') {            // we get RGB data
        uint32_t recieve = (uint32_t) strtol((const char *) &payload[1], NULL, 10 );   // decode rgb data
                            
counter = recieve;
      break;
  }
}
}
