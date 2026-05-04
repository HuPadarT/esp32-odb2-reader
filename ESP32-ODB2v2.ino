//C++
// MVVM design pattern, SOLID, Clean Code
#include <Wire.h>
#include <U8g2lib.h>
#include <BluetoothSerial.h>

const int BTN_NEXT = 5;  // bal oldali v. felső gomb (Lapozás)
const int BTN_YES = 18;  // Középső gomb (Igen)
const int BTN_NO = 23;   // jobb oldali v. alsó gomb (Mégse)

const int buttonPins[] = {BTN_NEXT, BTN_YES, BTN_NO};

// I2C Szoftveres meghajtás (SCK=19, SDA=22)
U8G2_SH1106_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, /* clock=*/ 19, /* data=*/ 22, /* reset=*/ U8X8_PIN_NONE);
BluetoothSerial SerialBT;

// az ELM327 OBD2 MAC címe
uint8_t obdAddress[6] = {0x00, 0x10, 0xCC, 0x4F, 0x36, 0x03};

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
int dtcCount = 0; // Teszteléshez 3-ra állítva, hogy látszódjon a lista
int currentDtcIndex = 0;
String obdBuffer = ""; // buffer a beérkező adatokhoz

// Gomb pergésmentesítés
bool buttonStates[] = {HIGH, HIGH, HIGH};
bool lastButtonStates[] = {HIGH, HIGH, HIGH};
unsigned long lastDebounceTimes[] = {0, 0, 0};
unsigned long debounceDelay = 50;

// OBD2 Lekérdező (Polling) időzítő
unsigned long lastPollTime = 0;
unsigned long pollInterval = 2000; // 2 másodpercenként kérdez

float batteryTemp = 0.0;
bool hasData = false;
bool isParsingBatTemp = false;

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
	SerialBT.begin("Lexus_Display", true); // az eszköz (ESP32) saját neve
	// Biztonsági azonosítás az ELM327 felé ---
	SerialBT.setPin("1234");
	// A kapcsolódás az ELM327-hez - MAC ADDR: 00:10:CC:4F:36:03
	// MAC alapú, azonnali csatlakozás a név alapú helyett
	isConnected = SerialBT.connect(obdAddress);
	
	if (isConnected)
	{ // 3 üzenet küldése az ODB2-nek - ELM327 incializálása
		//SerialBT.println("ATZ"); // reseteli az ELM327-et
		//delay(1000);
		// Gyorsított "Warm Start" inicializálás a lassú ATZ helyett
		SerialBT.println("AT WS");
		delay(300);
		SerialBT.println("ATE0"); // kikapcsolja a visszhangot - hogy ne ismételje vissza a kapott utasítást
		delay(500);
		SerialBT.println("ATL0"); // /r/n helyett /r az új sor
		delay(500);
		SerialBT.println("AT H1"); // hívás azonosító - kitől jön vissza az üzenet
		delay(200);
	}
	
	updateDisplay();
}

void loop()
{
	handleBluetoothData();
	readButtons();
	pollObdData(); // adatok lekérdezése az ELM 327-től
}

// Aktív OBD2 lekérdező logika
void pollObdData()
{
	if (isConnected)
	{
		if ((millis() - lastPollTime) > pollInterval)
		{
			lastPollTime = millis();
			
			switch (currentMenuState)
			{
				case 1:
					SerialBT.println("21 98");
					break;
					
				case 2:
					SerialBT.println("2101");
					break;
			}
		}
	}
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
	switch (buttonPin)
	{
		case BTN_NEXT:
			switch (currentMenuState)
			{
				// A 4 főképernyő közötti lapozás (Fall-through logika)
				case 0:
				case 1:
				case 2:
				case 3:
					currentMenuState = (currentMenuState + 1) % 4;
					updateDisplay();
					if (currentMenuState == 3 && isConnected)
					{
						SerialBT.println("03"); // DTC lekérdezés belépéskor
					}
					break;
					
				// Lapozás a konkrét hibakódok között
				case 5:
					if (dtcCount > 0)
					{
						currentDtcIndex = (currentDtcIndex + 1) % dtcCount;
						updateDisplay();
					}
					break;
			}
			break;
			
		case BTN_YES:
			switch (currentMenuState)
			{
				// Belépés a DTC listába
				case 3:
					if (dtcCount > 0)
					{
						currentMenuState = 5;
						currentDtcIndex = 0;
						updateDisplay();
					}
					break;
					
				// Összes DTC törlésének megerősítése
				case 4:
					if (isConnected)
					{
						SerialBT.println("04");
					}
					dtcCount = 0;
					currentMenuState = 3;
					updateDisplay();
					break;
					
				// Egyedi törlés kiválasztása
				case 5:
					currentMenuState = 6;
					updateDisplay();
					break;
					
				// Egyedi DTC törlésének megerősítése
				case 6:
					for (int i = currentDtcIndex; i < dtcCount - 1; i++)
					{
						dtcList[i] = dtcList[i + 1];
					}
					dtcCount--;
					if (dtcCount == 0)
					{
						currentMenuState = 3;
					}
					else
					{
						if (currentDtcIndex >= dtcCount)
						{
							currentDtcIndex = 0;
						}
						currentMenuState = 5;
					}
					updateDisplay();
					break;
			}
			break;
			
		case BTN_NO:
			switch (currentMenuState)
			{
				// Visszaugrás a Hibrid/TPMS képernyőről a Főképernyőre
				case 1:
				case 2:
					currentMenuState = 0;
					updateDisplay();
					break;
					
				// Összes törlése megerősítő kérdés megnyitása
				case 3:
					if (dtcCount > 0)
					{
						currentMenuState = 4;
						updateDisplay();
					}
					break;
					
				// Közös visszalépés a 3-as menübe (Fall-through logika)
				case 4:
				case 5:
					currentMenuState = 3;
					if (isConnected)
					{
						SerialBT.println("03"); // Frissítés visszalépéskor
					}
					updateDisplay();
					break;
					
				// Mégsem töröljük az egyedit
				case 6:
					currentMenuState = 5;
					updateDisplay();
					break;
			}
			break;
	}
}

void handleBluetoothData()
{
	while (SerialBT.available())
	{
		// Beolvasunk egyetlen karaktert
		char c = SerialBT.read();
		
		// Ha elértük a válasz végét jelző promptot
		if (c == '>')
		{
			// Diagnosztikai kiírás (Lássuk a teljes, vágatlan csomagot!)
			Serial.println("--- TELJES OBD CSOMAG ---");
			Serial.println(obdBuffer);
			
			// Szóközök és sortörések kipucolása
			String cleanInput = obdBuffer;
			cleanInput.replace(" ", "");
			cleanInput.replace("\r", "");
			cleanInput.replace("\n", "");
			
			// --- 1. HIBRID AKKU (7EA21) KERESÉSE ---
			int batPos = cleanInput.indexOf("7EA21");
			
			if (batPos != -1 && cleanInput.length() >= batPos + 9)
			{
				String hexVal = cleanInput.substring(batPos + 7, batPos + 9);
				long decimalVal = strtol(hexVal.c_str(), NULL, 16);
				batteryTemp = (int)decimalVal - 40;
				hasData = true;
				updateDisplay();
			}
			
			// --- 2. HIBAKÓDOK (43) KERESÉSE ---
			int dtcPos = cleanInput.indexOf("43");
			
			if (dtcPos != -1 && cleanInput.length() >= dtcPos + 6)
			{
				dtcCount = 0;
				for (int i = dtcPos + 2; i < cleanInput.length() - 3; i += 4)
				{
					String hexCode = cleanInput.substring(i, i + 4);
					if (hexCode != "0000" && hexCode.length() == 4)
					{
						char firstChar = hexCode.charAt(0);
						String prefix = "";
						
						if (firstChar == '0') { prefix = "P0"; }
						else if (firstChar == '1') { prefix = "P1"; }
						else if (firstChar == '2') { prefix = "P2"; }
						else if (firstChar == '3') { prefix = "P3"; }
						else if (firstChar == '4') { prefix = "C0"; }
						else if (firstChar == '5') { prefix = "C1"; }
						else if (firstChar == '6') { prefix = "C2"; }
						else if (firstChar == '7') { prefix = "C3"; }
						else if (firstChar == '8') { prefix = "B0"; }
						else if (firstChar == '9') { prefix = "B1"; }
						else if (firstChar == 'A') { prefix = "B2"; }
						else if (firstChar == 'B') { prefix = "B3"; }
						else if (firstChar == 'C') { prefix = "U0"; }
						else if (firstChar == 'D') { prefix = "U1"; }
						else if (firstChar == 'E') { prefix = "U2"; }
						else if (firstChar == 'F') { prefix = "U3"; }
						
						if (prefix != "")
						{
							String finalDtc = prefix + hexCode.substring(1, 4);
							
							if (dtcCount < 9)
							{
								dtcList[dtcCount] = finalDtc;
								dtcCount++;
							}
						}
					}
				}
				updateDisplay();
			}
			
			// Feldolgozás végén KÖTELEZŐ kiüríteni a puffert a következő kérdéshez!
			obdBuffer = "";
		}
		else
		{
			// Ha a karakter nem '>', akkor gyűjtjük tovább a dobozba
			obdBuffer += c;
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
	// Optimalizáció: Változók létrehozása a cikluson kívül
	String tempStr = "";
	String frontLeft = "";
	String frontRight = "";
	String rearLeft = "";
	String rearRight = "";
	String dtcStr = "";
	String countStr = "";
	String codeStr = "";
	String askStr = "";
	
	// 1. Adat-előkészítő állapotgép (A stringek feltöltése)
	switch (currentMenuState)
	{
		case 1:
			if (hasData)
			{
				tempStr = "Bat Temp: " + String(batteryTemp, 0) + " C";
			}
			break;
			
		case 2:
			frontLeft = "BE: " + String(tpmsFL, 2);
			frontRight = "JE: " + String(tpmsFR, 2);
			rearLeft = "BH: " + String(tpmsRL, 2);
			rearRight = "JH: " + String(tpmsRR, 2);
			break;
			
		case 3:
			dtcStr = "DTCs: " + String(dtcCount) + " Found";
			break;
			
		case 5:
			countStr = "Code " + String(currentDtcIndex + 1) + "/" + String(dtcCount);
			codeStr = dtcList[currentDtcIndex] + " (YES:Clr)";
			break;
			
		case 6:
			askStr = "Clear " + dtcList[currentDtcIndex] + "?";
			break;
	}

	// 2. Képernyő rajzoló állapotgép (U8G2 ciklus)
	u8g2.firstPage();
	
	do
	{
		switch (currentMenuState)
		{
			case 0:
				u8g2.drawStr(0, 15, "Lexus NX300h");
				
				if (isConnected)
				{
					u8g2.drawStr(0, 35, "Status: Connected");
				}
				else
				{
					u8g2.drawStr(0, 35, "Status: Disconnected");
				}
				break;
				
			case 1:
				if (hasData)
				{
					u8g2.drawStr(0, 15, tempStr.c_str());
				}
				else
				{
					u8g2.drawStr(0, 15, "Bat Temp: -- C");
				}
				
				u8g2.drawStr(0, 35, "Data Live...");
				break;
				
			case 2:
				u8g2.drawStr(0, 15, "Guminyomas (bar):");
				u8g2.drawStr(0, 35, frontLeft.c_str());
				u8g2.drawStr(64, 35, frontRight.c_str());
				u8g2.drawStr(0, 55, rearLeft.c_str());
				u8g2.drawStr(64, 55, rearRight.c_str());
				break;
				
			case 3:
				u8g2.drawStr(0, 15, dtcStr.c_str());
				
				if (dtcCount > 0)
				{
					u8g2.drawStr(0, 35, "YES:View  NO:Clr");
				}
				else
				{
					u8g2.drawStr(0, 35, "System Clean");
				}
				break;
				
			case 4:
				u8g2.drawStr(0, 15, "Clear ALL DTCs?");
				u8g2.drawStr(0, 35, "YES:Clr  NO:Back");
				break;
				
			case 5:
				u8g2.drawStr(0, 15, countStr.c_str());
				u8g2.drawStr(0, 35, codeStr.c_str());
				break;
				
			case 6:
				u8g2.drawStr(0, 15, askStr.c_str());
				u8g2.drawStr(0, 35, "YES:Clr  NO:Back");
				break;
		}
	}
	while (u8g2.nextPage());
}