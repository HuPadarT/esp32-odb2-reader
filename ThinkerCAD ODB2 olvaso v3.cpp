//C++
// MVVM design pattern, SOLID, Clean Code
// TPMS teszteléshez a soros monitorba írd be: 2101 73 72 75 73 és enter
// Akkumulátor hőfok teszteléshez: 41 05 32)
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int BTN_NEXT = 2;
const int BTN_YES = 3;
const int BTN_NO = 4;

const int buttonPins[] = {BTN_NEXT, BTN_YES, BTN_NO};

// --- MODEL RÉTEG ---
// 0: Főképernyő, 1: Adatok (Akku), 2: Guminyomás (TPMS), 3: DTC Összegző
// 4: Összes törlése megerősítés, 5: DTC Lista nézet, 6: Egyedi törlés megerősítés
int currentMenuState = 0;

// Szimulált hibakód lista
String dtcList[5] = {"P0A80", "P3000", "C1259", "", ""};
int dtcCount = 3;
int currentDtcIndex = 0;

// Gombok debounce változói
bool buttonStates[] = {HIGH, HIGH, HIGH};
bool lastButtonStates[] = {HIGH, HIGH, HIGH};
unsigned long lastDebounceTimes[] = {0, 0, 0};
unsigned long debounceDelay = 50;

// Akkumulátor és TPMS adatok
float batteryTemp = 0.0;
bool hasBatteryData = false;

float tpmsFL = 0.0;
float tpmsFR = 0.0;
float tpmsRL = 0.0;
float tpmsRR = 0.0;
bool hasTpmsData = false;

LiquidCrystal_I2C lcd(0x20, 16, 2);

void setup()
{
	Serial.begin(9600);
	
	lcd.init();
	
	lcd.backlight();
	
	for (int i = 0; i < 3; i++)
	{
		pinMode(buttonPins[i], INPUT_PULLUP);
	}
	
	updateDisplay();
}

void loop()
{
	handleSerial();
	
	readButtons();
}

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

// vezérlés
void processButtonPress(int buttonPin)
{
	if (buttonPin == BTN_NEXT)
	{
		// 4 főmenü közötti lapozás
		if (currentMenuState == 0 || currentMenuState == 1 || currentMenuState == 2 || currentMenuState == 3)
		{
			currentMenuState = (currentMenuState + 1) % 4;
			
			updateDisplay();
		}
		else if (currentMenuState == 5)
		{
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
			if (dtcCount > 0)
			{
				currentMenuState = 5;
				currentDtcIndex = 0;
				
				updateDisplay();
			}
		}
		else if (currentMenuState == 4)
		{
			lcd.clear();
			
			lcd.print("Clearing ALL...");
			
			delay(1500);
			
			dtcCount = 0;
			currentMenuState = 3;
			
			updateDisplay();
		}
		else if (currentMenuState == 5)
		{
			currentMenuState = 6;
			
			updateDisplay();
		}
		else if (currentMenuState == 6)
		{
			lcd.clear();
			
			lcd.print("Code Cleared!");
			
			delay(1000);
			
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
		if (currentMenuState == 1 || currentMenuState == 2)
		{
			// Visszalépés a főképernyőre az adatnézetekből
			currentMenuState = 0;
			
			updateDisplay();
		}
		else if (currentMenuState == 3)
		{
			if (dtcCount > 0)
			{
				currentMenuState = 4;
				
				updateDisplay();
			}
		}
		else if (currentMenuState == 4)
		{
			currentMenuState = 3;
			
			updateDisplay();
		}
		else if (currentMenuState == 5)
		{
			currentMenuState = 3;
			
			updateDisplay();
		}
		else if (currentMenuState == 6)
		{
			currentMenuState = 5;
			
			updateDisplay();
		}
	}
}

// Model frissítése
void handleSerial()
{
	if (Serial.available() > 0)
	{
		String incoming = Serial.readStringUntil('\n');
		
		incoming.trim();
		
		// Akkumulátor hőfok szimuláció (Példa bemenet: 41 05 32)
		if (incoming.startsWith("41 05"))
		{
			String hexVal = incoming.substring(6, 8);
			
			long decimalVal = strtol(hexVal.c_str(), NULL, 16);
			
			batteryTemp = decimalVal - 40;
			
			hasBatteryData = true;
			
			updateDisplay();
		}
		// TPMS szimuláció (Példa bemenet: 2101 73 72 75 73)
		else if (incoming.startsWith("2101"))
		{
			// Feltételezzük a fix pozíciókat: 5-6, 8-9, 11-12, 14-15
			String hexFL = incoming.substring(5, 7);
			String hexFR = incoming.substring(8, 10);
			String hexRL = incoming.substring(11, 13);
			String hexRR = incoming.substring(14, 16);
			
			// A HEX értéket megszorozzuk 0.02-vel, hogy kb 2.0 - 2.5 Bar közötti értéket kapjunk
			tpmsFL = strtol(hexFL.c_str(), NULL, 16) * 0.02;
			tpmsFR = strtol(hexFR.c_str(), NULL, 16) * 0.02;
			tpmsRL = strtol(hexRL.c_str(), NULL, 16) * 0.02;
			tpmsRR = strtol(hexRR.c_str(), NULL, 16) * 0.02;
			
			hasTpmsData = true;
			
			updateDisplay();
		}
	}
}

// A megjelenítés (View)
void updateDisplay()
{
	lcd.clear();
	
	if (currentMenuState == 0)
	{
		lcd.setCursor(0, 0);
		
		lcd.print("Lexus NX300h");
		
		lcd.setCursor(0, 1);
		
		lcd.print("System Ready");
	}
	else if (currentMenuState == 1)
	{
		lcd.setCursor(0, 0);
		
		if (hasBatteryData)
		{
			lcd.print("Bat Temp: " + String(batteryTemp, 0) + " C");
		}
		else if (!hasBatteryData)
		{
			lcd.print("Bat Temp: -- C");
		}
		
		lcd.setCursor(0, 1);
		
		lcd.print("Data Live...");
	}
	else if (currentMenuState == 2)
	{
		lcd.setCursor(0, 0);
		
		// Első kerekek (Front)
		if (hasTpmsData)
		{
			lcd.print("F:" + String(tpmsFL, 2) + " " + String(tpmsFR, 2) + " Bar");
		}
		else if (!hasTpmsData)
		{
			lcd.print("F:--.-- --.-- Bar");
		}
		
		lcd.setCursor(0, 1);
		
		// Hátsó kerekek (Rear)
		if (hasTpmsData)
		{
			lcd.print("R:" + String(tpmsRL, 2) + " " + String(tpmsRR, 2) + " Bar");
		}
		else if (!hasTpmsData)
		{
			lcd.print("R:--.-- --.-- Bar");
		}
	}
	else if (currentMenuState == 3)
	{
		lcd.setCursor(0, 0);
		
		lcd.print("DTCs: " + String(dtcCount) + " Found");
		
		lcd.setCursor(0, 1);
		
		if (dtcCount > 0)
		{
			lcd.print("YES:View NO:Clr");
		}
		else if (dtcCount == 0)
		{
			lcd.print("System Clean");
		}
	}
	else if (currentMenuState == 4)
	{
		lcd.setCursor(0, 0);
		
		lcd.print("Clear ALL DTCs?");
		
		lcd.setCursor(0, 1);
		
		lcd.print("YES:Clr  NO:Back");
	}
	else if (currentMenuState == 5)
	{
		lcd.setCursor(0, 0);
		
		lcd.print("Code " + String(currentDtcIndex + 1) + "/" + String(dtcCount));
		
		lcd.setCursor(0, 1);
		
		lcd.print(dtcList[currentDtcIndex] + " (YES:Clr)");
	}
	else if (currentMenuState == 6)
	{
		lcd.setCursor(0, 0);
		
		lcd.print("Clear " + dtcList[currentDtcIndex] + "?");
		
		lcd.setCursor(0, 1);
		
		lcd.print("YES:Clr  NO:Back");
	}
}