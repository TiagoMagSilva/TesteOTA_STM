#include "stm32ota.h"
String bin_file_name;
const String STM32_CHIPNAME[9] = {
  "Unknown Chip",
  "STM32F03xx4/6",
  "STM32F030x8/05x",
  "STM32F030xC",
  "STM32F103x4/6",
  "STM32F103x8/B",
  "STM32F103xC/D/E",
  "STM32F105/107",
  "STM32F411xx"
};

char rx_buffer[64];
int bytes_buffer;

stm32ota::stm32ota(int _NRST, int _BOOT0, int _LED) {
  SerialPC.println("Configurando BOOT RS LED");
  this->pin_BOOT0 = _BOOT0;
  this->pin_LED = _LED;
  this->pin_NRST = _NRST;
  pinMode(_BOOT0, OUTPUT);
  pinMode(_NRST, OUTPUT);
  pinMode(_LED, OUTPUT);
  digitalWrite(_NRST, HIGH);
}

String stm32ota::conect() {

  String stringtmp;
  int rdtmp;
  SerialPC.println("Connecting");
  delay(100);
  digitalWrite(pin_BOOT0, HIGH);
  delay(100);
  digitalWrite(pin_NRST, LOW);
  digitalWrite(pin_LED, LOW);
  delay(50);
  digitalWrite(pin_NRST, HIGH);
  delay(500);
  for (int i = 0; i < 3; i++) {
    digitalWrite(pin_LED, !digitalRead(pin_LED));
    delay(100);
  }

  SerialESP.write(STM32INIT);
  delay(10);
  if (SerialESP.baudRate() > 0)
    ;
  rdtmp = SerialESP.read();
  if (rdtmp == STM32ACK) {
    stringtmp = STM32_CHIPNAME[GetId()];
  } else if (rdtmp == STM32NACK) {
    SerialESP.write(STM32INIT);
    delay(10);
    if (SerialESP.available() > 0)
      ;
    rdtmp = SerialESP.read();
    if (rdtmp == STM32ACK) {
      stringtmp = STM32_CHIPNAME[GetId()];
    }
  } else
    stringtmp = "ERROR";
  return stringtmp;
}

//------------------------------------------------------------------------------
unsigned char stm32ota::GetId() {  //
  int getid = 0;
  unsigned char sbuf[5];
  SerialPC.println("Getting ID");
  SendCommand(STM32ID);
  while (!SerialESP.available())
    ;
  sbuf[0] = SerialESP.read();
  if (sbuf[0] == STM32ACK) {
    SerialESP.readBytesUntil(STM32ACK, sbuf, 4);
    getid = sbuf[1];
    getid = (getid << 8) + sbuf[2];
    SerialPC.print("Device ID: ");
    SerialPC.println(getid,HEX);
    if (getid == 0x444)
      return 1;
    if (getid == 0x440)
      return 2;
    if (getid == 0x442)
      return 3;
    if (getid == 0x412)
      return 4;
    if (getid == 0x410)
      return 5;
    if (getid == 0x414)
      return 6;
    if (getid == 0x418)
      return 7;
    if (getid == 0x431)
      return 8;
  } else return 0;

  return 0;
}
//------------------------------------------------------------------------------
void stm32ota::SendCommand(unsigned char commd) {  //
  SerialESP.write(commd);
  SerialESP.write(~commd);
}
//----------------------------------------------------------------------------------
unsigned char stm32ota::Erase() {   //
  SendCommand(STM32ERASE);
  while (!SerialESP.available())
    ;
  if (SerialESP.read() == STM32ACK) {
    SerialESP.write(0xFF);
    SerialESP.write(0x00);
  } else return STM32ERR;
  while (!SerialESP.available())
    ;
  return SerialESP.read();
}
//------------------------------------------------------------------------------
unsigned char stm32ota::Erasen() {  //
  SendCommand(STM32ERASEN);
  while (!SerialESP.available())
    ;
  if (SerialESP.read() == STM32ACK) {
    SerialESP.write(0xFF);
    SerialESP.write(0xFF);
    SerialESP.write(0x00);
  } else return STM32ERR;
  while (!SerialESP.available())
    ;
  return SerialESP.read();
}
//------------------------------------------------------------------------------
boolean stm32ota::EraseChip() {  //
  boolean aux;
  if (Erase() == STM32ACK)
    aux = true;
  else if (Erasen() == STM32ACK)
    aux = true;
  else
    aux = false;
  return aux;
}
//------------------------------------------------------------------------------
boolean stm32ota::Flash(String bin_file_name) { 
  File fsUploadFile;
  boolean flashwr;
  int lastbuf = 0;
  uint8_t cflag;
  int bini = 0;
  uint8_t binread[256];

  fsUploadFile = SPIFFS.open(bin_file_name, "r");
  if (fsUploadFile) {
    bini = fsUploadFile.size() / 256;
    lastbuf = fsUploadFile.size() % 256;
    for (int i = 0; i < bini; i++) {
      fsUploadFile.read(binread, 256);
      SendCommand(STM32WR);
      while (!SerialESP.available())
        ;
      cflag = SerialESP.read();
      if (cflag == STM32ACK)
        if (Address(STM32STADDR + (256 * i)) == STM32ACK) {
          if (SendData(binread, 255) == STM32ACK)
            flashwr = true;
          else flashwr = false;
        }
    }
    fsUploadFile.read(binread, lastbuf);
    SendCommand(STM32WR);
    while (!SerialESP.available())
      ;
    cflag = SerialESP.read();
    if (cflag == STM32ACK)
      if (Address(STM32STADDR + (256 * bini)) == STM32ACK) {
        if (SendData(binread, lastbuf) == STM32ACK)
          flashwr = true;
        else flashwr = false;
      }
    fsUploadFile.close();
  }
  return flashwr;
}

//------------------------------------------------------------------------------
boolean stm32ota::downloadFile(String File_Url, String File_Name = "") {
  int StepProgress = 0;
  int beforeStep = 0;
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  if (File_Name == "") {
    File_Name = File_Url.substring(File_Url.lastIndexOf("/"), File_Url.length());
  } else {
    File_Name = String("/") + File_Name;
  }

  bin_file_name = File_Name;
  http.begin(client, File_Url);  // abre o link
  int httpCode = http.GET();     //verifica se o link e valido
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {  //arquivo OK
      int len = http.getSize();
      int paysize = len;
      uint8_t buff[128] = { 0 };                      // Cria o buffer para receber os dados
      WiFiClient* stream = http.getStreamPtr();       // Recebe os dados
      File configFile = SPIFFS.open(File_Name, "w");  //cria o arquivo na spifss
      if (!configFile) {
        return false;
      }
      int Step = paysize / 10;
      while (http.connected() && (len > 0 || len == -1)) {
        size_t size = stream->available();
        if (size) {
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          configFile.write(buff, c);
          if (len > 0) {
            len -= c;
          }
        }
      }
      configFile.close();  //finaliza o arquivo
      http.end();
      return true;
    } else {
      return false;
    }
  } else {
    return false;
  }
  return true;
}
//------------------------------------------------------------------------------
unsigned char stm32ota::Address(unsigned long addr) {

  unsigned char sendaddr[4];
  unsigned char addcheck = 0;
  sendaddr[0] = addr >> 24;
  sendaddr[1] = (addr >> 16) & 0xFF;
  sendaddr[2] = (addr >> 8) & 0xFF;
  sendaddr[3] = addr & 0xFF;
  for (int i = 0; i <= 3; i++) {
    SerialESP.write(sendaddr[i]);
    addcheck ^= sendaddr[i];
  }
  SerialESP.write(addcheck);
  while (!SerialESP.available())
    ;
  return SerialESP.read();
}
//------------------------------------------------------------------------------
unsigned char stm32ota::SendData(unsigned char* data, unsigned char wrlen) {
  SerialESP.write(wrlen);
  for (int i = 0; i <= wrlen; i++) {
    SerialESP.write(data[i]);
  }
  SerialESP.write(getChecksum(data, wrlen));
  while (!SerialESP.available())
    ;
  return SerialESP.read();
}
//------------------------------------------------------------------------------
unsigned char stm32ota::getChecksum(unsigned char* data, unsigned char datalen) {
  unsigned char lendata = datalen;
  for (int i = 0; i <= datalen; i++)
    lendata ^= data[i];
  return lendata;
}

//----------------------------------------------------------------------------------
void stm32ota::deletfiles(String bin_file) {
  File dir = SPIFFS.open("/");
  if (SPIFFS.exists(bin_file)) {
    SPIFFS.remove(bin_file);
  }
}
//----------------------------------------------------------------------------------
String stm32ota::binfilename() {
  String aux = bin_file_name;
  return aux;
}
//----------------------------------------------------------------------------------
void stm32ota::RunMode() {  //Tested  Change to runmode
  digitalWrite(pin_BOOT0, LOW);
  delay(100);
  digitalWrite(pin_NRST, LOW);
  digitalWrite(pin_LED, LOW);
  delay(50);
  digitalWrite(pin_NRST, HIGH);
  delay(200);
  for (int i = 0; i < 3; i++) {
    digitalWrite(pin_LED, !digitalRead(pin_LED));
    delay(100);
  }
}
//----------------------------------------------------------------------------------
void stm32ota::ConfigPins(int _NRST, int _BOOT0, int _LED){
  SerialPC.println("Configurando BOOT RS LED");
  this->pin_BOOT0 = _BOOT0;
  this->pin_LED = _LED;
  this->pin_NRST = _NRST;
  pinMode(_BOOT0, OUTPUT);
  pinMode(_NRST, OUTPUT);
  pinMode(_LED, OUTPUT);
  digitalWrite(_NRST, HIGH);
}
//----------------------------------------------------------------------------------
char stm32ota::chipVersion() {  // Tested
  unsigned char vsbuf[14];
  SendCommand(STM32GET);
  while (!SerialESP.available())
    ;
  vsbuf[0] = SerialESP.read();
  if (vsbuf[0] != STM32ACK)
    return STM32ERR;
  else {
    SerialESP.readBytesUntil(STM32ACK, vsbuf, 14);
    return vsbuf[1];
  }
}
//----------------------------------------------------------------------------------
String stm32ota::otaUpdate(String File_Url) 
{
  SerialPC.println("Updating");
  String aux = "";
  if (WiFi.waitForConnectResult() == WL_CONNECTED) 
  {

    if (downloadFile(File_Url, "stm32.bin")) 
    {
      //printfiles();
      String aux = conect();
      if (aux != "ERROR") 
      {
        EraseChip();
        Flash("/stm32.bin");
        RunMode();
        deletfiles("/stm32.bin");
      } 
      else 
      {
        return "Unknown Chip";
      }
    } 
    else 
    {
      return "Download Fail";
    }
  } 
  else 
  {
    return "WiFi not conected";
  }
  return "Update OK";
}

//----------------------------------------------------------------------------------

String stm32ota::stm32Read(unsigned long addr, unsigned char n_bytes) {
  unsigned char rdbuf[15] = { '\0' };
  unsigned char sendaddr[4];
  unsigned char addcheck = 0;
  if (n_bytes > 15) {
    n_bytes = 15;
  }
  if (conect() != "ERROR") {
    SendCommand(STM32RD);
    while (!SerialESP.available())
      ;
    if (SerialESP.read() == STM32ACK) {
      sendaddr[0] = addr >> 24;
      sendaddr[1] = (addr >> 16) & 0xFF;
      sendaddr[2] = (addr >> 8) & 0xFF;
      sendaddr[3] = addr & 0xFF;
      for (int i = 0; i <= 3; i++) {
        SerialESP.write(sendaddr[i]);
        addcheck ^= sendaddr[i];
      }
      SerialESP.write(addcheck);
    } else return "ERROR";
    while (!SerialESP.available())
      ;
    if (SerialESP.read() == STM32ACK) {
      SerialESP.write(n_bytes);
      addcheck = n_bytes ^ STM32XOR;
      SerialESP.write(addcheck);
    } else return "ERROR";
    delay(100);
    while (!SerialESP.available())
      ;
    bytes_buffer = SerialESP.available();
    SerialESP.readBytes(rx_buffer, bytes_buffer);
    String myNewString(rx_buffer);
    RunMode();
    return myNewString;
  }
  return "ERROR";
}
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
