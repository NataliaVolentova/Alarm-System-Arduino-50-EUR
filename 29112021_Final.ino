/**************************************************************************
  Alarmanlage
 **************************************************************************/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MFRC522.h>

//Variablen
int state;
unsigned long previousMillis = 0;            // Zeit wann das Display das letzte Mal invertiert wurde
bool displayState = LOW;                     // Letzte mal Display
long interval = 10000;                       // Interval für Display-Invertierung und Buzzer ein/aus
const unsigned long triggerInterval = 5000; // Zeit die Runterläuft bis Alarm aktiviert ist
unsigned long millisAlarmStarted = 0;       // Zeit wann der Alarm aktiviert wurde
unsigned long secondsLeft = 0;

//Defines
#define SCREEN_WIDTH 128    // OLED Pixel Breite
#define SCREEN_HEIGHT 64    // OLED Pixel Höhe
#define OLED_RESET -1       // Kein Reset Pin am Display es wird deshalb -1 lt. Libary verwendet
#define SCREEN_ADDRESS 0x3C // Adresse über I2C Scanner Example ermittelt

//Instanzen
MFRC522 rfid(10, 9);
MFRC522::MIFARE_Key key;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup()
{
  state = 0; //Beim Einschalten sollte immer der Initial Step verwendet werden
  Serial.begin(9600);
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
  pinMode(8, INPUT); //Pin 8 wird für den PIR verwendet, ist also ein Eingang
  pinMode(7, OUTPUT);
  pinMode (6,OUTPUT);//LASER Modul

  //Display initialisieren
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  display.display();
  // Display Löschen
  display.clearDisplay();
  display_aus(); //Auf Display zeigen, dass die Alarmanlage ausgeschaltet ist
}

void loop()
{
  //State Machine realisiert mit einer Switch Case Anweisung
  bildschirmschoner();          //Damit sich die LEDs nicht in den Bildschirm einbrennen wird der immer wieder mal invertiert, beim Alarm passiert das schnell...
  int reading = analogRead(A0); // Den Signalausgang des Temperatursensors lesen

  switch (state)
  {
    case 0:
      //init Schritt
      alarm(false);
        display_aus(); //Auf Display zeigen, dass die Alarmanlage ausgeschaltet ist
      digitalWrite(6, LOW);
      if (checkCard())
      {
        //Wird die korrekte NFC Karte auf das Lesegerät gehalten Alarmanlage vorbereiten zum Scharfstellen
        state = 1;
        millisAlarmStarted = millis();
      }
      if (reading > 300)
      {
        state = 10;
      }
      break;
    case 1:
      if (display_countdown())
      {
        state = 2;
        display_aktiv();
        digitalWrite(6, HIGH);//Laser aktivieren
        delay(50);
      }
      if (reading > 300)
      {
        state = 10;
      }
      else if (checkCard())
      {
        state = 0;  
      }
      break;
    //Alarmanlage getriggert
    case 2:
      digitalWrite(6, HIGH);//Laser aktivieren
      if (reading > 300)
      {
        state = 10;
      }
      Serial.println(analogRead(A1));
      if ((analogRead(A1)<=850) || digitalRead(8))//10bit 1024 - 5V .... 2,5V AI 
      {
              state = 3;
              display_einbruch();
      }
      if (checkCard())//Wird die Karte aufs Lesegerät gehalten ausschalten
      {
        state = 0;  
      }
      break;
    case 3:
      alarm(true);
      if (checkCard())
      {
        display_aus();
        state = 0;
      }
      break;
    //Feueralarm
    case 10:
      display_feuer();
      alarm(true);
      if (reading < 250)
      {
        state = 0;
      }
      Serial.println(reading);
      break;
    default:
      Serial.println("Unknown Error");
      break;
  }
}

//Display Ansteuerung
void display_aus(void)
{
  interval = 10000;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(3, 3);
  display.setTextSize(6);
  display.println(F("Aus"));
  display.display();
}

void display_feuer(void)
{
  interval = 300;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(1, 1);
  display.setTextSize(3);
  display.println(F("Feuer"));
  display.setTextSize(4);
  display.println(F(" 122"));
  display.display();
}

bool display_countdown(void)
{
  unsigned long currentMillis = millis();
  secondsLeft = triggerInterval-(currentMillis - millisAlarmStarted);
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println(F("Aktiv in"));
  display.println(secondsLeft);
  display.println(F("mSec."));
  display.display();
  if ((currentMillis - millisAlarmStarted) >= triggerInterval)
    {
      return true;
    }
  else
    {
      return false;
    }
}

void display_aktiv()
{
  interval = 10000;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(1, 0);
  display.setTextSize(3);
  display.println(F("Aktiv"));
  display.display();
}

void display_einbruch(void)
{
  interval = 1200;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(1, 0);
  display.setTextSize(2);
  display.println(F("Einbruch!"));
  display.println(F("Protected"));
  display.println(F("Mode"));
  display.println(F("aktiviert"));
  display.display();
}

//NFC Karte lesen
bool checkCard()
{
  byte myUID[4] = {205, 78, 83, 132}; //UID eines spezifischen Ausweises
  // Nichts mehr zu tun, keine Karte vorhanden in den loop zurückspringen
  if (!rfid.PICC_IsNewCardPresent())
    return false;

  // Bei Fehler vom Auslesen ebenfalls zurückspringen
  if (!rfid.PICC_ReadCardSerial())
    return false;


  if (rfid.uid.uidByte[0] == myUID[0] ||
      rfid.uid.uidByte[1] == myUID[1] ||
      rfid.uid.uidByte[2] == myUID[2] ||
      rfid.uid.uidByte[3] == myUID[3])
  {

    // Stoppen der NFC Funktionen lt. Beispiel
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    //Die UID wurde erkannt True zurückgeben
    return true;
  }
  else
  {
    // Stoppen der NFC Funktionen lt. Beispiel
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return false;
  }
}

//Sonstige Funktionen
void alarm(bool bAktiv)
{
  if (bAktiv)
  {
    tone(7, 1000);
  }
  else
  {
    noTone(7);
  }
}

void bildschirmschoner()
{
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval)
  {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    // if the LED is off turn it on and vice-versa:
    if (displayState == LOW)
    {
      display.invertDisplay(true);
      displayState = HIGH;
    }
    else
    {
      display.invertDisplay(false);
      displayState = LOW;
    }
    display.display();
  }
}
