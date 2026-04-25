//C++
// MVVM design pattern, SOLID, Clean Code
#include <Wire.h>
#include <U8g2lib.h>
#include <BluetoothSerial.h>

// --- KONSTANSOK ÉS PIN-EK (A te "Nested Routing" kiosztásod) ---
const int BTN_NEXT = 5;  // bal oldali v. felső gomb (Lapozás)
const int BTN_YES = 18;  // Középső gomb (Igen)
const int BTN_NO = 23;   // jobb oldali v. alsó gomb (Mégse)

const int buttonPins[] = {BTN_NEXT, BTN_YES, BTN_NO};

// I2C Szoftveres meghajtás (SCK=19, SDA=22)
U8G2_SH1106_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, /* clock=*/ 19, /* data=*/ 22, /* reset=*/ U8X8_PIN_NONE);
BluetoothSerial SerialBT;

// model
int currentMenuState = 0;
bool isConnected = false;

// TPMS Adatok
float tpmsFL = 0.00; // Bal Első
float tpmsFR = 0.00; // Jobb Első
float tpmsRL = 0.00; // Bal Hátsó
float tpmsRR = 0.00; // Jobb Hátsó

// DTC Adatok
String dtcList[9] = {"", ""};
int dtcCount = 3;
int currentDtcIndex = 0;

// Gomb pergésmentesítés
bool buttonStates[] = {HIGH, HIGH, HIGH};
bool lastButtonStates[] = {HIGH, HIGH, HIGH};
unsigned long lastDebounceTimes[] = {0, 0, 0};
unsigned long debounceDelay = 50;

float batteryTemp = 0.0;
bool hasData = false;

void setup()
{
  Serial.begin(115200);
  
  for (int i = 0; i < 3; i++)
  {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  
  u8g2.begin();
  
  u8g2.setFont(u8g2_font_ncenB08_tr);
  
  drawConnectingScreen();
  
  SerialBT.begin("Lexus_Display", true);
  
  // Biztonsági azonosítás az ELM327 felé ---
  SerialBT.setPin("1234");
  // A kapcsolódás az ELM327-hez
  isConnected = SerialBT.connect("OBDII");
  
  if (isConnected)
  { // 3 üzenet küldése az ODB2-nek - ELM327 incializálása
    SerialBT.println("ATZ"); // reseteli az ELM327-et
    delay(1000);
    SerialBT.println("ATE0"); // kikapcsolja a visszhangot - hogy ne ismételje vissza a kapott utasítást
    delay(500);
    SerialBT.println("ATL0"); // /r/n helyett /r az új sor
    delay(500);
  }
  
  updateDisplay();
}

void loop()
{
  handleBluetoothData();
  
  readButtons();
}

// vezérlés
void readButtons()
{
  for (int i = 0; i < 3; i++)
  {
    bool reading = digitalRead(buttonPins[i]);
    
    if (reading != lastButtonStates[i])
    {
      lastDebounceTimes[i] = millis();
    }
    
    if ((millis() - lastDebounceTimes[i]) > debounceDelay)
    {
      if (reading != buttonStates[i])
      {
        buttonStates[i] = reading;
        
        if (buttonStates[i] == LOW)
        {
          processButtonPress(buttonPins[i]);
        }
      }
    }
    
    lastButtonStates[i] = reading;
  }
}

void processButtonPress(int buttonPin)
{
  if (buttonPin == BTN_NEXT)
  {
    // A 4 főképernyő között lapozunk
    if (currentMenuState < 4)
    {
      currentMenuState = (currentMenuState + 1) % 4;
      
      updateDisplay();
    }
    else if (currentMenuState == 5)
    {
      // DTC Lista lapozása
      if (dtcCount > 0)
      {
        currentDtcIndex = (currentDtcIndex + 1) % dtcCount;
        
        updateDisplay();
      }
    }
  }
  else if (buttonPin == BTN_YES)
  {
    if (currentMenuState == 3)
    {
      // Belépés a DTC listába
      if (dtcCount > 0)
      {
        currentMenuState = 5;
        currentDtcIndex = 0;
        
        updateDisplay();
      }
    }
    else if (currentMenuState == 4)
    {
      // Összes DTC törlése
      if (isConnected)
      {
        SerialBT.println("04");
      }
      
      dtcCount = 0;
      currentMenuState = 3;
      
      updateDisplay();
    }
    else if (currentMenuState == 5)
    {
      // Egyedi törlés megerősítő képernyője
      currentMenuState = 6;
      
      updateDisplay();
    }
    else if (currentMenuState == 6)
    {
      // Egyedi DTC kivétele a listából
      for (int i = currentDtcIndex; i < dtcCount - 1; i++)
      {
        dtcList[i] = dtcList[i + 1];
      }
      
      dtcCount--;
      
      if (dtcCount == 0)
      {
        currentMenuState = 3;
      }
      else if (dtcCount > 0)
      {
        if (currentDtcIndex >= dtcCount)
        {
          currentDtcIndex = 0;
        }
        
        currentMenuState = 5;
      }
      
      updateDisplay();
    }
  }
  else if (buttonPin == BTN_NO)
  {
    if (currentMenuState == 3)
    {
      // Összes törlése megerősítő kérdés
      if (dtcCount > 0)
      {
        currentMenuState = 4;
        
        updateDisplay();
      }
    }
    else if (currentMenuState == 4)
    {
      // Mégsem töröljük az összeset
      currentMenuState = 3;
      
      updateDisplay();
    }
    else if (currentMenuState == 5)
    {
      // Vissza a DTC listából a DTC összegzőre
      currentMenuState = 3;
      
      updateDisplay();
    }
    else if (currentMenuState == 6)
    {
      // Mégsem töröljük az egyedit
      currentMenuState = 5;
      
      updateDisplay();
    }
    else if (currentMenuState == 1 || currentMenuState == 2)
    {
      // Visszaugrás a Hibrid/TPMS képernyőről a Főképernyőre
      currentMenuState = 0;
      
      updateDisplay();
    }
  }
}

void handleBluetoothData()
{
  if (SerialBT.available() > 0)
  {
    String incoming = SerialBT.readStringUntil('\n');
    
    incoming.trim();
    
    if (incoming.startsWith("41 05"))
    {
      String hexVal = incoming.substring(6, 8);
      
      long decimalVal = strtol(hexVal.c_str(), NULL, 16);
      
      batteryTemp = decimalVal - 40;
      
      hasData = true;
      
      updateDisplay();
    }
  }
}

// View - megjelenítés
void drawConnectingScreen()
{
  u8g2.firstPage();
  
  do
  {
    u8g2.drawStr(0, 15, "Lexus NX300h");
    u8g2.drawStr(0, 35, "Connecting OBDII...");
    u8g2.drawStr(0, 55, "Please Wait...");
  }
  while (u8g2.nextPage());
}

void updateDisplay()
{
  u8g2.firstPage();
  
  do
  {
    if (currentMenuState == 0)
    {
      u8g2.drawStr(0, 15, "Lexus NX300h");
      
      if (isConnected)
      {
        u8g2.drawStr(0, 35, "Status: Connected");
      }
      else if (!isConnected)
      {
        u8g2.drawStr(0, 35, "Status: Disconnected");
      }
    }
    else if (currentMenuState == 1)
    {
      if (hasData)
      {
        String tempStr = "Bat Temp: " + String(batteryTemp, 0) + " C";
        u8g2.drawStr(0, 15, tempStr.c_str());
      }
      else if (!hasData)
      {
        u8g2.drawStr(0, 15, "Bat Temp: -- C");
      }
      
      u8g2.drawStr(0, 35, "Data Live...");
    }
    else if (currentMenuState == 2)
    {
      // guminyomás (2 Oszlopos mátrix)
      u8g2.drawStr(0, 15, "Guminyomas (bar):");
      
      // Felső sor (Első kerekek)
      String frontLeft = "BE: " + String(tpmsFL, 2);
      String frontRight = "JE: " + String(tpmsFR, 2);
      u8g2.drawStr(0, 35, frontLeft.c_str());
      u8g2.drawStr(64, 35, frontRight.c_str()); // X koordináta 64-től indul (jobb oszlop)
      
      // Alsó sor (Hátsó kerekek)
      String rearLeft = "BH: " + String(tpmsRL, 2);
      String rearRight = "JH: " + String(tpmsRR, 2);
      u8g2.drawStr(0, 55, rearLeft.c_str());
      u8g2.drawStr(64, 55, rearRight.c_str());
    }
    else if (currentMenuState == 3)
    {
      String dtcStr = "DTCs: " + String(dtcCount) + " Found";
      u8g2.drawStr(0, 15, dtcStr.c_str());
      
      if (dtcCount > 0)
      {
        u8g2.drawStr(0, 35, "YES:View  NO:Clr");
      }
      else if (dtcCount == 0)
      {
        u8g2.drawStr(0, 35, "System Clean");
      }
    }
    else if (currentMenuState == 4)
    {
      u8g2.drawStr(0, 15, "Clear ALL DTCs?");
      u8g2.drawStr(0, 35, "YES:Clr  NO:Back");
    }
    else if (currentMenuState == 5)
    {
      String countStr = "Code " + String(currentDtcIndex + 1) + "/" + String(dtcCount);
      u8g2.drawStr(0, 15, countStr.c_str());
      
      String codeStr = dtcList[currentDtcIndex] + " (YES:Clr)";
      u8g2.drawStr(0, 35, codeStr.c_str());
    }
    else if (currentMenuState == 6)
    {
      String askStr = "Clear " + dtcList[currentDtcIndex] + "?";
      u8g2.drawStr(0, 15, askStr.c_str());
      
      u8g2.drawStr(0, 35, "YES:Clr  NO:Back");
    }
  }
  while (u8g2.nextPage());
}
