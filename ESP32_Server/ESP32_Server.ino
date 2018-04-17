#include <DNSServer.h>
#include <Update.h>
#include <SPI.h>
#include <WiFi.h>
#include <SD.h>

MD5Builder md5;
DNSServer dnsServer;
WiFiServer webServer(80);
HardwareSerial uartSerial(2);  // pins 16(RX) , 17(TX)
WiFiServer uartServer(24);      
WiFiClient uartClient;    


String firmwareFile = "/fwupdate.bin"; //update filename
String firmwareVer = "1.03"; 
int cprog = 0;

//------default settings if config.ini is missing------//
String AP_SSID = "PS4_WEB_AP";
String AP_PASS = "password";
IPAddress Server_IP(10,1,1,1);
IPAddress Subnet_Mask(255,255,255,0);
//-----------------------------------------------------//


String split(String str, String from, String to)
{
  String tmpstr = str;
  tmpstr.toLowerCase();
  from.toLowerCase();
  to.toLowerCase();
  int pos1 = tmpstr.indexOf(from);
  int pos2 = tmpstr.indexOf(to, pos1 + from.length());   
  String retval = str.substring(pos1 + from.length() , pos2);
  return retval;
}


bool instr(String str, String search)
{
int result = str.indexOf(search);
if (result == -1)
{
  return false;
}
return true;
}


void handleWebServer() {
 WiFiClient client = webServer.available(); 
    if (client) {
        while (client.connected()) {
            if (client.available() > 0) {  
        String cData = client.readStringUntil('\r');
        client.flush();
        String path;

        if (instr(cData,"GET "))
        {
           path = split(cData, "GET ", " HTTP");
        }
        else if (instr(cData,"POST "))
        {
           path = split(cData, "POST ", " HTTP");
        }

  String dataType = "text/plain";
  if (instr(path,"/document/"))
  {
    path.replace("/document/" + split(path,"/document/","/ps4/") + "/ps4/", "/");
  }
  if (path.endsWith("/")) {
    path += "index.htm";
  }
  if (path.endsWith(".html")) {
    path.replace(".html",".htm");
  }
  if (path.endsWith(".src")) {
    path = path.substring(0, path.lastIndexOf("."));
  } else if (path.endsWith(".htm")) {
    dataType = "text/html";
  } else if (path.endsWith(".css")) {
    dataType = "text/css";
  } else if (path.endsWith(".js")) {
    dataType = "application/javascript";
  } else if (path.endsWith(".png")) {
    dataType = "image/png";
  } else if (path.endsWith(".gif")) {
    dataType = "image/gif";
  } else if (path.endsWith(".jpg")) {
    dataType = "image/jpeg";
  } else if (path.endsWith(".ico")) {
    dataType = "image/x-icon";
  } 


  File dataFile = SD.open(path.c_str());
  if (dataFile.isDirectory()) {
    path += "/index.htm";
    dataType = "text/html";
    dataFile = SD.open(path.c_str());
  }

  if (!dataFile) {
        client.print("HTTP/1.1 404 Not Found\r\n");
        client.print("Content-type: text/plain\r\n");
        client.print("Connection: close\r\n");
    break;
  }
        int filesize = dataFile.size();
        client.print("HTTP/1.1 200 OK\r\n");
        client.print("Content-type: " + dataType + "\r\n");
        client.print("Content-Length: " + String(filesize) + "\r\n");
        client.print("Connection: close\r\n\r\n");
 
     while (dataFile.available()) {
       client.write(dataFile.read());
     }

        dataFile.close();
        break;
            }
        }
        delay(10);   
        client.stop();
    } 
}




void handleUart()
{
  if(!uartClient.connected())
  {
    uartClient = uartServer.available();
  }
  else
  {
    if(uartClient.available() > 0)
    {
      char data[1024];
      int cnt = 0;
      while(uartClient.available())
      {
        data[cnt] = uartClient.read();
        cnt++;
      }    
      uartClient.flush();
      String cBuffer = String(data);
      Serial.println(cBuffer);
      //process commands to send
      //uartSerial.print(cBuffer);
    }
    
    while (uartSerial.available() > 0) {
       String sData = uartSerial.readString();   
       Serial.print(sData);  
       uartClient.print(sData);  
    }
    }
}


void updateFw() {
  if (SD.exists(firmwareFile)) {
     File updateFile;
     Serial.println("Update file found");
     updateFile = SD.open(firmwareFile, FILE_READ);
 if (updateFile) {
    size_t updateSize = updateFile.size();
  if (updateSize > 0) {
    md5.begin();
    md5.addStream(updateFile,updateSize);
    md5.calculate();
    String md5Hash = md5.toString();
    Serial.println("Update file hash: " + md5Hash);
    updateFile.close();
    updateFile = SD.open(firmwareFile, FILE_READ);
   if (updateFile) {
      if(updateFile.isDirectory()){
         updateFile.close();
         return;
      }
        int md5BufSize = md5Hash.length() + 1;
        char md5Buf[md5BufSize];
        md5Hash.toCharArray(md5Buf, md5BufSize) ;
        Update.setMD5(md5Buf);
       Serial.println("Updating Firmware...");
      if (!Update.begin(updateSize)) {   
      Update.printError(Serial);
      updateFile.close();
      return;
      }
      Update.writeStream(updateFile);
      if (Update.end()) {
      Serial.println("Installed firmware hash: " + Update.md5String()); 
      Serial.println("Update complete");
      updateFile.close();
      SD.remove(firmwareFile); 
      ESP.restart();  
      }
      else {
        Serial.println("Update failed");
        Update.printError(Serial);
        updateFile.close();  
      }
    }
  }
   else {
    Serial.println("Error, file is invalid");
    updateFile.close(); 
    SD.remove(firmwareFile);
   }
  }
  }
  else {
  Serial.println("No update file found");
  }
}


void setup(void) {

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("Version: " + firmwareVer);

  if (SD.begin(SS)) {
  File iniFile;
  
  if (SD.exists("/config.ini")) {
  iniFile = SD.open("/config.ini", FILE_READ);
  if (iniFile) {
  String iniData;
    while (iniFile.available()) {
      char chnk = iniFile.read();
      iniData += chnk;
    }
   iniFile.close();

   if(instr(iniData,"SSID="))
   {
   AP_SSID = split(iniData,"SSID=","\r\n");
   AP_SSID.trim();
   }
   
   if(instr(iniData,"PASSWORD="))
   {
   AP_PASS = split(iniData,"PASSWORD=","\r\n");
   AP_PASS.trim();
   }
   
   if(instr(iniData,"WEBSERVER_IP="))
   {
    String strwIp = split(iniData,"WEBSERVER_IP=","\r\n");
    strwIp.trim();
    Server_IP.fromString(strwIp);
   }

   if(instr(iniData,"SUBNET_MASK="))
   {
    String strsIp = split(iniData,"SUBNET_MASK=","\r\n");
    strsIp.trim();
    Subnet_Mask.fromString(strsIp);
   }
   
    }
  }
  else
  {
    iniFile = SD.open("/config.ini", FILE_WRITE);
    if (iniFile) {
    iniFile.print("\r\nSSID=" + AP_SSID + "\r\nPASSWORD=" + AP_PASS + "\r\n\r\nWEBSERVER_IP=" + Server_IP.toString() + "\r\n\r\nSUBNET_MASK=" + Subnet_Mask.toString() + "\r\n");
    iniFile.close();
    }
  }


    Update.onProgress([](unsigned int progress, unsigned int total) {
      int progr = (progress / (total / 100));
      if (progr >= cprog) {
        cprog = progr + 10;
        Serial.println(String(progr) + "%");
      }
    });
    
  updateFw();
  
  Serial.println("SSID: " + AP_SSID);
  Serial.println("Password: " + AP_PASS);
  Serial.print("\n");
  Serial.println("WEB Server IP: " + Server_IP.toString());
  Serial.println("Subnet: " + Subnet_Mask.toString());
  Serial.println("DNS Server IP: " + Server_IP.toString());
  Serial.print("\n\n");


  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID.c_str(), AP_PASS.c_str());
  WiFi.softAPConfig(Server_IP, Server_IP, Subnet_Mask);
  Serial.println("WIFI AP started");

  dnsServer.setTTL(30);
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  dnsServer.start(53, "*", Server_IP);
  Serial.println("DNS server started");

  webServer.setTimeout(100);
  webServer.begin();
  Serial.println("HTTP server started");
  
  }
  else
  {
     Serial.println("No SD card found");
     Serial.println("Starting Wifi AP without webserver");
     WiFi.mode(WIFI_AP);
     WiFi.softAPConfig(Server_IP, Server_IP, Subnet_Mask);
     WiFi.softAP(AP_SSID.c_str(), AP_PASS.c_str());
     Serial.println("WIFI AP started");

     dnsServer.setTTL(30);
     dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
     dnsServer.start(53, "*", Server_IP);
     Serial.println("DNS server started");
  }

  uartSerial.begin(115200);
  uartServer.begin();
  Serial.println("UART server started");
  
}



void loop(void) {
  dnsServer.processNextRequest();
  handleWebServer();
  handleUart();
}

