#include <Adafruit_GPS.h>
#include <blynk.h>
#include "street.h" // custom street type def

// GPS defs
USARTSerial& mySerial = Serial1;
Adafruit_GPS GPS(&mySerial);
uint32_t timer = millis();

#define GPSECHO false // true if raw GPS data echos to console
#define debug true // true if parsed debug info echos to console

// Blynk defs
#define blynkMapPin V0
#define blynkblynkFixLEDPin V1
#define blynkMutePin V2
#define blynkSpeedPin V3
#define blynkblynkSpeedLEDPin V4

char blynkAuth[] = "7c613879f7fd454fb9c668b476f3ba11";

WidgetMap blynkMap(blynkMapPin);
WidgetLED blynkFixLED(blynkblynkFixLEDPin);
WidgetLED blynkSpeedLED(blynkblynkSpeedLEDPin);
int mapIndex = 0; // determines index for map
int muted = 0;

// other defs
#define speakerPin 2
int speedBuffer = 0; // mph allowed above speedLimit without alarm

// defined streets
street anderson;
street campus;
street research;
street circuit;

void setup() {
  initGPS();
  defineStreets();
  pinMode(speakerPin, OUTPUT);
  Blynk.begin(blynkAuth);
}

void loop() {
  readRawGPS();

  if (badGPSRead()) return;

  // if millis() or timer wraps around, reset it
  if (timer > millis())  timer = millis();

  // check speed limit every two seconds
  if (millis() - timer > 2000) {
    timer = millis(); // reset the timer

    if (!GPS.fix) {
      blynkFixLED.off();
      noTone(speakerPin);
      return; // don't have a fix so don't read or try to use data
    }

    if (debug) printDebugInfo();

    float mph = (int) GPS.speed * 1.150779; // convert knots to mph

    Blynk.run();
    blynkFixLED.on(); // alert Blynk that we have a fix
    // send current location to Blynk map widget
    blynkMap.location(mapIndex, GPS.latitudeDegrees, GPS.longitudeDegrees, (int) mph);
    mapIndex++;
    Blynk.virtualWrite(blynkSpeedPin, (int) mph); // send current speed to Blynk speedometer

    int speedLimit = 0;
    street streets[] = {anderson, campus, research, circuit};

    // check if current location is within a street's geofence,
    // get speed limit if yes
    for (int i = 0; i < (sizeof(streets)/sizeof(street)); i++) {
      if (pointOnStreet(GPS.longitudeDegrees, GPS.latitudeDegrees, streets[i])) {
        if (debug) Serial.println(streets[i].name);
        speedLimit = streets[i].speedLimit;
      }
    }

    if (!speedLimit) {
      // not currently on a geofenced street
      blynkSpeedLED.off();
      noTone(speakerPin);
      return;
    }

    float speedDiff = mph - (speedLimit + speedBuffer); // mph of excess speed

    // sound different frequency depending on how much speed limit is exceeded
    if (speedDiff >= 25) {
      alarm(2750);
    } else if (speedDiff >= 20) {
      alarm(2449);
    } else if (speedDiff >= 15) {
      alarm(2182);
    } else if (speedDiff >= 10) {
      alarm(2060);
    } else if (speedDiff >= 5) {
      alarm(1835);
    } else if (speedDiff > 0) {
      alarm(1635);
    } else {
      blynkSpeedLED.off(); // alert Blynk we are no longer speeding
      noTone(speakerPin);
    }
  }
}

// sound an alarm with a given frequency, only if not muted
void alarm(int frequency) {
  blynkSpeedLED.on(); // alert Blynk that we are speeding
  if (muted) {
    noTone(speakerPin);
  } else {
    tone(speakerPin, frequency);
  }
}

// geo-fence streets and cache them
void defineStreets() {
  // Campus Dr.
  campus.name = "CAMPUS";
  float campusX[] = {-78.934191,-78.919449,-78.916123,-78.919485,-78.933230};
  memcpy(campus.polyX, campusX, sizeof(campusX));
  float campusY[] = {36.000608,36.002312,36.000698,35.998426,35.998038};
  memcpy(campus.polyY, campusY, sizeof(campusY));
  campus.numPoints = sizeof(campusX)/sizeof(float);
  campus.speedLimit = 25;

  // Anderson Rd.
  anderson.name = "ANDERSON";
  float andersonX[] = {-78.933358,-78.934752,-78.935376,-78.933908,-78.930973,-78.929578,-78.928257};
  memcpy(anderson.polyX, andersonX, sizeof(andersonX));
  float andersonY[] = {35.997101,35.992530,35.984068,35.981841,35.983326,35.994133,35.997844};
  memcpy(anderson.polyY, andersonY, sizeof(andersonY));
  anderson.numPoints = sizeof(andersonX)/sizeof(float);
  anderson.speedLimit = 30;

  // Resarch Dr.
  research.name = "RESEARCH";
  float researchX[] = {-78.943496,-78.940014,-78.940808,-78.938769,-78.936495,-78.938697,-78.941566};
  memcpy(research.polyX, researchX, sizeof(researchX));
  float researchY[] = {36.008546,36.003802,36.002605,36.001890,36.003816,36.004765,36.008560};
  memcpy(research.polyY, researchY, sizeof(researchY));
  research.numPoints = sizeof(researchX)/sizeof(float);
  research.speedLimit = 5;

  // Circuit Dr.
  circuit.name = "CIRCUIT";
  float circuitX[] = {-78.941872,-78.944290,-78.947881,-78.946095,-78.943190,-78.940916};
  memcpy(circuit.polyX, circuitX, sizeof(circuitX));
  float circuitY[] = {36.006385,36.005802,36.002021,36.001422,36.004634,36.005451};
  memcpy(circuit.polyY, circuitY, sizeof(circuitY));
  circuit.numPoints = sizeof(circuitX)/sizeof(float);
  circuit.speedLimit = 20;
}

// determines if (x,y) is located in a given street s
// x == long, y == lat
// sourced from http://alienryderflex.com/polygon/
bool pointOnStreet(float x, float y, street s) {
  int i, j = s.numPoints - 1;
  bool  oddNodes = false;
  for (i=0; i < s.numPoints; i++) {
    if (s.polyY[i] < y && s.polyY[j] >= y ||  s.polyY[j] < y && s.polyY[i] >= y) {
      if (s.polyX[i] + (y - s.polyY[i]) / (s.polyY[j] - s.polyY[i]) * (s.polyX[j] - s.polyX[i]) < x) {
        oddNodes =! oddNodes;
      }
    }
    j = i;
  }
  return oddNodes;
}

void printDebugInfo() {
  Serial.println("===========================================");

  Serial.print("Location (degrees): ");
  Serial.print(GPS.latitudeDegrees, 10);
  Serial.print(", ");
  Serial.println(GPS.longitudeDegrees, 10);

  Serial.print("Speed (mph): ");
  Serial.println(GPS.speed * 1.150779); // convert from knots to mph
}

// sends commands to initialize GPS unit
void initGPS() {
  Serial.begin(115200); // best frequency for GPS
  GPS.begin(9600);
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA); // output type
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); // 1 Hz update rate
  GPS.sendCommand(PGCMD_ANTENNA); // provide antenna debug info
  useInterrupt(true);
  delay(1000);
}

// sets up interrupt
void useInterrupt(boolean v) {
  static HAL_InterruptCallback callback;
  static HAL_InterruptCallback previous;
  callback.handler = handleSysTick;
  HAL_Set_System_Interrupt_Handler(SysInterrupt_SysTick, &callback, &previous, nullptr);
}

// called once a millisecond by interrupt, looks for any new GPS data and stores it
void handleSysTick(void* data) {
  char c = GPS.read();
  if (GPSECHO && c) {
    Serial.write(c);
  }
}

void readRawGPS() {
  char c = GPS.read();
  if (GPSECHO && c) {
    Serial.print(c);
  }
}

bool badGPSRead() {
  // fail to parse a sentence; just wait for another
  return (GPS.newNMEAreceived() && !GPS.parse(GPS.lastNMEA()));
}

// sync mute status with Blynk
BLYNK_CONNECTED() {
  Blynk.syncVirtual(blynkMutePin);
}

// write mute command from Blynk app
BLYNK_WRITE(blynkMutePin) {
  muted = param.asInt();
}
