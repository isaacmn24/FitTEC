// SENSORES
#include <OneWire.h>
#include <DallasTemperature.h>

hw_timer_t * sampleTimer = NULL;
portMUX_TYPE sampleTimerMux = portMUX_INITIALIZER_UNLOCKED;

#define USE_ARDUINO_INTERRUPTS true    // Set-up low-level interrupts for most acurate BPM math.
#include <PulseSensorPlayground.h>     // Includes the PulseSensorPlayground Library. 

// BLUETOOTH
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
float txValue = 0;
const int readPin = 25; // Use GPIO number. See ESP32 board pinouts
const int LED = 19; // Could be different depending on the dev board. I used the DOIT ESP32 dev board.

// SENSORES
int maxBPM = 180;
int maxTemp = 38;

// ALARMA
#define pinBuzzer 14

// BLUETOOTH
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pCharacteristic) {
    char txString[8]; // make sure this is big enuffz
    dtostrf(txValue, 1, 2, txString); // float_val, min_width, digits_after_decimal, char_buffer
    
      pCharacteristic->setValue(txString);
  }
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");

        for (int i = 0; i < rxValue.length(); i++) {
          Serial.print(rxValue[i]);
        }

        Serial.println();

        // Do stuff based on the command received from the app
        if (rxValue.find("A") != -1) { 
          Serial.println("Turning ON!");
          digitalWrite(LED, HIGH);
        }
        else if (rxValue.find("B") != -1) {
          Serial.println("Turning OFF!");
          digitalWrite(LED, LOW);
        }

        Serial.println();
        Serial.println("*********");
      }
    }
};

// SENSORES
//  Variables
const int PulseWire = 36;       // PulseSensor PURPLE WIRE connected to ANALOG PIN 0
int Threshold = 550;           // Determine which Signal to "count as a beat" and which to ignore.
                               // Use the "Gettting Started Project" to fine-tune Threshold Value beyond default setting.
                               // Otherwise leave the default "550" value. 
                               
PulseSensorPlayground pulseSensor;  // Creates an instance of the PulseSensorPlayground object called "pulseSensor"

void IRAM_ATTR onSampleTime() {
  portENTER_CRITICAL_ISR(&sampleTimerMux);
    PulseSensorPlayground::OurThis->onSampleTime();
  portEXIT_CRITICAL_ISR(&sampleTimerMux);
}

//Sensor de Tempertura
// GPIO where the DS18B20 is connected to
const int oneWireBus = 4;     

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);

  // Temperatura
  // Start the DS18B20 sensor
  sensors.begin();

  // Configure the PulseSensor object, by assigning our variables to it. 
  pulseSensor.analogInput(PulseWire);   
  pulseSensor.setThreshold(Threshold);   

  // Double-check the "pulseSensor" object was created and "began" seeing a signal. 
   if (pulseSensor.begin()) {
    Serial.println("We created a pulseSensor Object !");  //This prints one time at Arduino power-up,  or on Arduino reset.  
  }

  sampleTimer = timerBegin(0, 80, true);                
  timerAttachInterrupt(sampleTimer, &onSampleTime, true);  
  timerAlarmWrite(sampleTimer, 2000, true);      
  timerAlarmEnable(sampleTimer);

  // BLUETOOTH
  pinMode(LED, OUTPUT);

  // Create the BLE Device
  BLEDevice::init("ESP32 UART Test"); // Give it a name

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
                    );
                      
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCallbacks());

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  bool alarma = 0;
  int myBPM = 0;

  // SENSORES
  /////////////////////// temperatura
  sensors.requestTemperatures(); 
  float temperaturaC = sensors.getTempCByIndex(0);
  Serial.print(temperaturaC);
  Serial.println("ºC");
  ////////////////////

  if (pulseSensor.sawStartOfBeat()) {            // Constantly test to see if "a beat happened".
    myBPM = pulseSensor.getBeatsPerMinute();  // Calls function on our pulseSensor object that returns BPM as an "int".
                                                  // "myBPM" hold this BPM value now. 
    Serial.println("♥  A HeartBeat Happened ! "); // If test is "true", print a message "a heartbeat happened".
    Serial.print("BPM: ");                        // Print phrase "BPM: " 
    Serial.println(myBPM);                        // Print the value inside of myBPM. 
  }
  //float porcentajeBPM = float(myBPM)/float(maxBPM);
  float porcentajeBPM = 0.5;
  int rangoBPM;
  if(porcentajeBPM < 0.2) {
   rangoBPM = 1;
   alarma = 1;
  }
  else if(porcentajeBPM < 0.6) {
   rangoBPM = 1; 
  }
  else if(porcentajeBPM < 0.7) {
   rangoBPM = 2; 
  }
  else if(porcentajeBPM < 0.8) {
   rangoBPM = 5; 
  }
  else if(porcentajeBPM < 0.9) {
   rangoBPM = 4; 
  }
  else if(porcentajeBPM < 1) {
   rangoBPM = 5;
  }
  else {
   rangoBPM = 6; 
   alarma = 1;
  }
  if(temperaturaC > maxTemp & temperaturaC < 35) {
    alarma = 1;
  }

  // BLUETOOTH
  String txValue = "";
  if (deviceConnected) {
    // Fabricate some arbitrary junk for now...
    txValue = txValue+temperaturaC+","+myBPM+","+rangoBPM+","+alarma; // This could be an actual sensor reading!

    // Let's convert the value to a char array:
    char txString[12]; // make sure this is big enuffz
    txValue.toCharArray(txString, 12);
    
    pCharacteristic->setValue(txString);
    
    pCharacteristic->notify(); // Send the value to the app!
    Serial.print("*** Sent Value: ");
    Serial.print(txString);
    Serial.println(" ***");
  }
  if(alarma){
    digitalWrite(pinBuzzer, HIGH);
    alarma = 0;
  }
  delay(1000);
}
