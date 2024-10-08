#include <ESP8266WiFi.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Wire.h>

// Configurare senzor DHT
#define DHTPIN 4 // Pinul pentru DHT
#define DHTTYPE DHT22 // Tipul senzorului DHT
DHT dht(DHTPIN, DHTTYPE);

// Setări pentru OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Butoane
#define BUTTON_UP 0       // Buton creștere
#define BUTTON_DOWN 5     // Buton scădere
#define BUTTON_SELECT 13  // Buton selectare
#define BUTTON_MENU 2     // Buton schimbare meniu (D4)

// Variabile pentru meniu
bool inMenu = false;
int selectedMenu = 0; // 0 = meniul pentru temperatură, 1 = meniul pentru ceas

// Variabile pentru temperatura setată
float setTemperature = 21.0;
float minTemp = 19.0;
bool inSetTempMode = false;  // Modul de setare a temperaturii
float morningTemp = 21.0;
float afternoonTemp = 23.0;
float eveningTemp = 19.0;

float* currentTempSetting = nullptr;

float hysteresis = 0.2;  // Histerezis ajustabil (între 0.1 și 0.5)
bool relayState = false;  // Variabilă pentru starea curentă a releului


// Configurare Wi-Fi
const char* ssid = "wifi"; // SSID-ul rețelei Wi-Fi
const char* password = "password"; // Parola rețelei Wi-Fi
const char* serverIP = "192.168.0.150"; // IP-ul Arduino-ului

// Variabile pentru ora curentă
int currentHour = 0;
int currentMinute = 0;
int currentSecond = 0;
bool inSetTimeMode = false;
bool settingHours = true; // Inițial setăm orele

// Interval de actualizare
unsigned long previousMillis = 0;
const long interval = 1000; // 1 secundă

unsigned long previousDHTMillis = 0;
const long dhtInterval = 2000; // 2 secunde

unsigned long previousRelayMillis = 0;
const long relayInterval = 5000; // 5 secunde

// Variabile pentru gestionarea butoanelor cu millis()
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 200;  // 200 ms pentru debouncing
unsigned long lastPressTimeUp = 0;
unsigned long lastPressTimeDown = 0;
unsigned long pressInterval = 500;  // 500 ms pentru apăsare lungă

// Variabile pentru actualizarea timpului folosind millis()
unsigned long previousSecondMillis = 0;

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Inițializează I2C folosind pinii specificați
  Wire.begin(14, 12); // SDA = GPIO 14, SCL = GPIO 12
  
  // Inițializează OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }

  // Setează contrastul (luminozitatea) OLED-ului
  //display.ssd1306_command(SSD1306_DISPLAYOFF);
  //display.ssd1306_command(SSD1306_SETCONTRAST);
  //display.ssd1306_command(5);  // Setează o valoare de contrast mai mică (exemplu: 50)
  //display.clearDisplay();


  // Configurare pini butoane
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_MENU, INPUT_PULLUP);

  // Conectare la Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Conectat la rețea!");

  // Afișăm datele senzorului la început
  DisplayDataRead();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Actualizare timp curent
  if (currentMillis - previousSecondMillis >= 1000) { // Verificăm dacă a trecut o secundă
    previousSecondMillis = currentMillis;
    currentSecond++;
    if (currentSecond >= 60) {
      currentSecond = 0;
      currentMinute++;
      if (currentMinute >= 60) {
        currentMinute = 0;
        currentHour++;
        if (currentHour >= 24) {
          currentHour = 0;
        }
      }
    }
  }

  // Afișăm datele dacă nu se apasă niciun buton și nu suntem în niciun meniu
  if (!inMenu && !inSetTempMode && !inSetTimeMode) {
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      DisplayDataRead();
    }
  }

  // Verificăm butonul de meniu pentru a intra în meniuri
  if (digitalRead(BUTTON_MENU) == LOW && (currentMillis - lastDebounceTime) > debounceDelay) {
    lastDebounceTime = currentMillis;
    inMenu = !inMenu; // Comutăm între meniuri și afișare normală

    if (inMenu) {
      displayMainMenu();
    } else {
      DisplayDataRead(); // Revenim la afișarea senzorului când ieșim din meniu
    }
  }

  // Gestionăm meniul principal dacă suntem în el
  if (inMenu) {
    handleMainMenu();
  }

  // Gestionăm setarea temperaturii sau orei, în funcție de selecția din meniu
  if (inSetTempMode) {
    SetareTemp();
  } else if (inSetTimeMode) {
    setTime();
  }
    // Apelăm RelaySend() doar la fiecare 5 secunde
  if (currentMillis - previousRelayMillis >= relayInterval) {
    previousRelayMillis = currentMillis;
    RelaySend();
  }
}

// Afișare date temperatură și umiditate
void DisplayDataRead() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(40, 0);
  display.println(F("Welcome"));

  display.setTextSize(2);
  display.setCursor(0, 16);
  display.print(F("T:"));
  display.print(temperature);
  display.print(F(" C"));

  display.setCursor(0, 40);
  display.print(F("H:"));
  display.print(humidity);
  display.print(F(" %"));

  // Afișăm ora curentă
  display.setTextSize(1);
  display.setCursor(0, 56); // Setează poziția mai jos pe ecran pentru afișarea orei
  display.print("Ora: ");
  if (currentHour < 10) display.print("0"); // Afișăm ora cu două cifre
  display.print(currentHour);
  display.print(":");
  if (currentMinute < 10) display.print("0"); // Afișăm minutele cu două cifre
  display.print(currentMinute);
  display.print(":");
  if (currentSecond < 10) display.print("0"); // Afișăm secundele cu două cifre
  display.print(currentSecond);

  display.display();  // Actualizăm ecranul
}

// Afișare meniu principal cu selecție
void displayMainMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  if (selectedMenu == 0) display.print(" > ");
  display.println("Set Morning Temp");

  display.setCursor(0, 16);
  if (selectedMenu == 1) display.print(" > ");
  display.println("Set Afternoon Temp");

  display.setCursor(0, 32);
  if (selectedMenu == 2) display.print(" > ");
  display.println("Set Evening Temp");

  display.setCursor(0, 48);
  if (selectedMenu == 3) display.print(" > ");
  display.println("Set Clock");

  display.display();
}


// Gestionarea meniului principal
void handleMainMenu() {
  unsigned long currentMillis = millis();

  if (digitalRead(BUTTON_UP) == LOW && currentMillis - lastPressTimeUp > debounceDelay) {
    lastPressTimeUp = currentMillis;
    selectedMenu = (selectedMenu + 1) % 4; // Acum avem 4 opțiuni
    displayMainMenu();
  }

  if (digitalRead(BUTTON_SELECT) == LOW && currentMillis - lastDebounceTime > debounceDelay) {
    lastDebounceTime = currentMillis;
    inMenu = false; // Ieșim din meniul principal

    if (selectedMenu == 0) {
      inSetTempMode = true; // Modul de setare pentru dimineață
      currentTempSetting = &morningTemp;
    } else if (selectedMenu == 1) {
      inSetTempMode = true; // Modul de setare pentru după-amiază
      currentTempSetting = &afternoonTemp;
    } else if (selectedMenu == 2) {
      inSetTempMode = true; // Modul de setare pentru seară
      currentTempSetting = &eveningTemp;
    } else if (selectedMenu == 3) {
      inSetTimeMode = true; // Setarea orei
    }
  }
}


// Funcție pentru setarea temperaturii cu butoanele UP și DOWN
void SetareTemp() {
  unsigned long currentMillis = millis();

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Set Temp:");

  display.setCursor(0, 40);
  display.print(*currentTempSetting);
  display.print(" *C");
  display.display();

  if (digitalRead(BUTTON_UP) == LOW && currentMillis - lastPressTimeUp > pressInterval) {
    lastPressTimeUp = currentMillis;
    *currentTempSetting += 0.5;
  }

  if (digitalRead(BUTTON_DOWN) == LOW && currentMillis - lastPressTimeDown > pressInterval) {
    lastPressTimeDown = currentMillis;
    if (*currentTempSetting > minTemp) {
      *currentTempSetting -= 0.5;
    }
  }

  if (digitalRead(BUTTON_SELECT) == LOW && currentMillis - lastDebounceTime > debounceDelay) {
    lastDebounceTime = currentMillis;
    inSetTempMode = false; // Ieșim din modul de setare a temperaturii
  }
}


// Funcție pentru setarea orei
void setTime() {
  unsigned long currentMillis = millis();

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Set Time:");

  // Afișăm ora curentă pentru setare
  display.setCursor(0, 40);
  if (settingHours) {
    display.print("Hour: ");
    display.print(currentHour);
  } else {
    display.print("Minute: ");
    display.print(currentMinute);
  }
  display.display();

  // Buton UP pentru a crește ora/minutul
  if (digitalRead(BUTTON_UP) == LOW && currentMillis - lastPressTimeUp > pressInterval) {
    lastPressTimeUp = currentMillis;
    if (settingHours) {
      currentHour = (currentHour + 1) % 24; // Crește ora (0-23)
    } else {
      currentMinute = (currentMinute + 1) % 60; // Crește minutul (0-59)
    }
  }

  // Buton DOWN pentru a scădea ora/minutul
  if (digitalRead(BUTTON_DOWN) == LOW && currentMillis - lastPressTimeDown > pressInterval) {
    lastPressTimeDown = currentMillis;
    if (settingHours) {
      currentHour = (currentHour + 23) % 24; // Scade ora (0-23)
    } else {
      currentMinute = (currentMinute + 59) % 60; // Scade minutul (0-59)
    }
  }

  // Buton SELECT pentru a schimba între setarea orei și minutei
  if (digitalRead(BUTTON_SELECT) == LOW && currentMillis - lastDebounceTime > debounceDelay) {
    lastDebounceTime = currentMillis;
    settingHours = !settingHours; // Comutăm între setarea orei și minutei
  }

  // Buton MENU pentru a salva ora și a ieși din modul de setare
  if (digitalRead(BUTTON_MENU) == LOW && currentMillis - lastDebounceTime > debounceDelay) {
    lastDebounceTime = currentMillis;
    inSetTimeMode = false; // Ieșim din modul de setare a timpului
  }
}

// Funcție care trimite comanda către releu pe baza temperaturii citite și aplică histerezis
void RelaySend() {
    float temperature = dht.readTemperature();
    float targetTemperature;

    // Selectăm temperatura țintă în funcție de ora curentă
    if (currentHour >= 6 && currentHour < 12) {
        targetTemperature = morningTemp;
    } else if (currentHour >= 12 && currentHour < 18) {
        targetTemperature = afternoonTemp;
    } else {
        targetTemperature = eveningTemp;
    }

    // Verificăm dacă citirea temperaturii este validă
    if (isnan(temperature)) {
        Serial.println("Eroare citire DHT");
        return;
    }

    // Aplicația histerezisului
    if (temperature < (targetTemperature - hysteresis)) {
        if (!relayState) {
            sendRelayCommand(1); // Pornim releul dacă temperatura scade sub prag minus histerezis
            relayState = true;
            Serial.println("Releu ON");
        }
    } else if (temperature > (targetTemperature + hysteresis)) {
        if (relayState) {
            sendRelayCommand(0); // Oprim releul dacă temperatura depășește prag plus histerezis
            relayState = false;
            Serial.println("Releu OFF");
        }
    }

    // Afișăm starea releului și temperatura curentă pentru debugging
    Serial.print("Temperatura actuală: ");
    Serial.println(temperature);
    Serial.print("Releu stare: ");
    Serial.println(relayState ? "ON" : "OFF");
}



void sendRelayCommand(int state) {
  WiFiClient client;
  float temperature = dht.readTemperature();
  
  // Timeout pentru conexiune
  unsigned long startAttemptTime = millis();

  if (client.connect(serverIP, 81)) { // Conectare la server
    String command = "releu=" + String(state); // Formează comanda releului
    client.print(command); // Trimiterea comenzii
    client.stop(); // Închide conexiunea
    Serial.print("Comandă trimisă: ");
    Serial.println(command);
  } else if (millis() - startAttemptTime > 1000) {  // Timeout de 1 secundă
    Serial.println("Conexiune eșuată la server.");
    client.stop(); // Închide conexiunea dacă eșuează
  }
}