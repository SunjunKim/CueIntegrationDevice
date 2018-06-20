#include <Adafruit_DotStar.h>
// Because conditional #includes don't work w/Arduino sketches...
#include <SPI.h>         // COMMENT OUT THIS LINE FOR GEMMA OR TRINKET
//#include <avr/power.h> // ENABLE THIS LINE FOR GEMMA OR TRINKET

#define NUMPIXELS 128
#define MAX_VARIANCE 2147483600

// Target pin: digital 4 => PD2
//                      76543210
#define TGTH PORTD |= 0B00001000
#define TGTL PORTD &=~0B00001000

// AIM pin: digital 3 => PD3
//                      76543210
#define AIMH PORTD |= 0B00010000
#define AIML PORTD &=~0B00010000

// Button pin read: Digital pin 2 ==> PD4
//                   76543210
#define BTNP PORTD&0B00000100;

const int buttonPin = 2;
const int targetPin = 3;  // PD2
const int aimPin = 4;     // PD3

uint32_t aimColor = 0x000010;
uint32_t aimSuccess = 0x0002001;
uint32_t aimFail = 0x200000;

uint32_t targetColor = 0x202000;
uint32_t stopColor = 0x002001;
uint32_t stopColorError = 0x200000;

bool positionFeedback = true;
bool positionGrab = false;

// 0 - - - - -> (timestamp: task running time)
// |--------------- distance ----------------| \
//                   |---masking---|            +- lengths will vary depend on moveSpeed
//                                 |--width--| /
// ================================#####|#####===========
//                   |   aimPosition ---ˆ
//                   ˆ-- Target appears here (masking), move with seed of 'moveSpeed'
// temporal distance and width
// time units: ms
unsigned long distance = 1000;    // set with 'd' command (unit: ms)
unsigned long width = 200;        // set with 'w' command (unit: ms)
unsigned long masking = 300;      // set with 'm' command (unit: ms)
unsigned int aimPosition = 70;   // set with 'a' command (unit: pixel)
unsigned long moveSpeed = 50;    // set with 's' command (unit: pixel/sec)

/////////////////////////////////////////////////////////////////////// INTERNAL VARIABLES BELOW
// Hardware SPI is a little faster, but must be wired to specific pins
// (Arduino Uno = pin 11 for data, 13 for clock, other boards are different).
Adafruit_DotStar strip = Adafruit_DotStar(NUMPIXELS, DOTSTAR_BRG);

// timekeeper unit: us
unsigned long lastTS;
int lastPosition = 0; // keeping the last lighted position (to prevent skipping)
bool reset = true;  // timer reset flag

int stopPos = 0;    // stopped (=target intercepped) position
bool catchedTarget = false;   // is true when a button press is made within the aim
bool processStop = false;     // is true when a button is pressed at somewhere.
bool stopPosPassed = false;   // is true when a target is passed beyond the stopPos
int randTime = 0;             // std. of randomness in temporal distance
int randSpeed = 0;            // std. of randomness in speed

// random offsets
long randomOffset = 0;
long randomSpeedOffset = 0;

void setup() {
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000L)
  clock_prescale_set(clock_div_1); // Enable 16 MHz on Trinket
#endif
  Serial.begin(115200);

  while (!Serial) ;
  Serial.println("Done 1");

  strip.begin(); // Initialize pins for output
  strip.show();  // Turn all LEDs off ASAP

  lastTS = micros();

  pinMode(targetPin, OUTPUT);
  pinMode(aimPin, OUTPUT);
  pinMode(buttonPin, INPUT);

  attachInterrupt(digitalPinToInterrupt(buttonPin), stopTarget, FALLING);

  initTest();
  randomSeed(analogRead(0));
}

void initTest()
{
  TGTL;
  AIML;
  reset = true;
}

int calculateAimWidth(unsigned long temporalWidth, unsigned long mspeed)
{
  unsigned long aw = mspeed * temporalWidth / 1000UL;
  return (int)aw;
}

void stopTarget()
{
  processStop = true;
}

void processSerial()
{
// Serial data read
  if (Serial.available() > 0)
  {
    // Read whatever serial events here
    char c = Serial.read();
    unsigned long readN = 0;
    bool read = false;
    switch (c)
    {
      case 't':
        readN = randTime = readNumber();
        read = true;
        break;
      case 'r':
        readN = randSpeed = readNumber();
        read = true;
        break;
      case 'd':
        readN = distance = readNumber();
        masking = 0;
        read = true;
        break;
      case 'w':
        readN = width = readNumber();
        read = true;
        break;
      case 's':
        readN = moveSpeed = readNumber();
        read = true;
        break;
      case 'a':
        readN = aimPosition = readNumber();
        read = true;
        break;
      case 'm':
        readN = masking = readNumber();
        read = true;
        break;
      case 'p':
        readN = readNumber();
        read = true;

        if     (readN == 2)       {
          positionFeedback = true;
          positionGrab = false;
        }
        else if (readN == 1)       {
          positionFeedback = true;
          positionGrab = true;
        }
        else                      {
          positionFeedback = false;
          positionGrab = false;
        }
    }
    if (read)
    {
      stopPos = 0;
      initTest();
      Serial.print(c);
      Serial.println(readN);
    }
  }
}

void loop() {
  processSerial();

  // put your main code here, to run repeatedly:
  unsigned long newTS = micros();
  // task running time keeper. (timestamp: us)
  unsigned long timestamp = (newTS - lastTS);
  
  unsigned long effectiveDistance = distance + randomOffset;
  unsigned long effectiveMoveSpeed = moveSpeed + randomSpeedOffset;

  // recalculate temporal width 
  double correctionRatio = ((double)moveSpeed/(double)effectiveMoveSpeed);
  if(randSpeed == 0)
    correctionRatio = 1.0;
    
  unsigned long effectiveWidth = correctionRatio * (double)width;
  
  // calculated visual width of the aim, given the speed and temporal width
  int aimWidth = calculateAimWidth(effectiveWidth, effectiveMoveSpeed);

  // total number of the pixels required fo the entire task (distance: ms)
  int runningPixels = effectiveMoveSpeed * effectiveDistance / 1000UL;
  // distance of target moved in pixel
  int elapsedPos = effectiveMoveSpeed * timestamp / 1000000UL;
  // end position (may be overrun beyond the NUMPIXELS depends on the moveSpeed)
  int endPos = aimPosition + aimWidth / 2;
  // start position (may be negative depends on the moveSpeed)
  int startPos = endPos - runningPixels;

  // current position of the target in strip
  int targetPosition = startPos + elapsedPos;

  unsigned long aimStartTime = effectiveDistance - effectiveWidth;
  unsigned long aimEndTime = effectiveDistance;

  // positions of the aim area
  int aimStart = endPos - aimWidth;
  int aimEnd = endPos;

  unsigned long maskingWidth = masking + width;
  //unsigned long effectiveMaskingWidth = correctionRatio * maskingWidth;
  unsigned long effectiveMaskingWidth = masking+effectiveWidth;

  // goes true if masking should be applied.
  bool doMasking = (timestamp <= ((effectiveDistance - effectiveMaskingWidth) * 1000UL) || timestamp >= effectiveDistance * 1000UL);
  bool isInAim = aimStartTime * 1000UL <= timestamp && timestamp < aimEndTime * 1000UL;
  //bool isInAim = aimStart <= targetPosition && targetPosition < aimEnd;
  bool isTargetAppear = !doMasking;

  isTargetAppear ? TGTH : TGTL;
  isInAim ?        AIMH : AIML;

  // initialize the entire strip
  for (int i = 0; i < NUMPIXELS; i++)
  {
    strip.setPixelColor(i, 0);
  }
  // coloring the aim point
  for (int i = aimStart; i < aimEnd; i++)
  {
    if (i >= 0 && i < NUMPIXELS)
    {
      strip.setPixelColor(i, aimColor);
    }
  }

  // coloring stopPos
  if (stopPos > 0 && !stopPosPassed && positionFeedback)
  {
    if (stopPos >= aimStart && stopPos < aimEnd)
      strip.setPixelColor(stopPos, stopColor);
    else
      strip.setPixelColor(stopPos, stopColorError);
  }

  if (stopPos > lastPosition && stopPos <= targetPosition)
    stopPosPassed = true;

  // coloring target
  // set skipped pixel color with slightly faded color
  if (reset)
  {
    reset = false;
  }
  else
  {
    for (int i = lastPosition + 1; i < targetPosition; i++)
    {
      if (!doMasking && i >= 0 && i < NUMPIXELS && i < endPos)
      {
        if (positionFeedback)
        {
          if (stopPos == 0 || stopPosPassed || i < stopPos || !positionGrab)
            strip.setPixelColor(i, targetColor);
        }
        else
        {
          strip.setPixelColor(i, targetColor);
        }
      }
    }
  }
  if (!doMasking && targetPosition >= 0 && targetPosition < NUMPIXELS && targetPosition < endPos)
  {
    if (positionFeedback)
    {
      if (stopPos == 0 || stopPosPassed || targetPosition < stopPos || !positionGrab)
        strip.setPixelColor(targetPosition, targetColor);
    }
    else
    {
      strip.setPixelColor(targetPosition, targetColor);
    }
  }

  if (processStop)
  {
    stopPos = targetPosition;
    if (elapsedPos < runningPixels / 3)
    {
      stopPos = targetPosition + runningPixels;
    }
    stopPosPassed = false;
    processStop = false;

    if ((effectiveDistance * 1000UL - timestamp) < effectiveWidth * 1000UL)
      catchedTarget = true;
  }

  if (!positionFeedback && catchedTarget)
  {
    strip.setPixelColor(aimStart - 3, aimSuccess);
    strip.setPixelColor(aimEnd + 2, aimSuccess);
  }

  strip.show();

  lastPosition = targetPosition;

  // timer reset
  if (timestamp >= effectiveDistance * 1000UL)
  {
    // Add randomness in temporal distance
    if (randTime == 0)
      randomOffset = 0;
    else
    {
      randomOffset = box_muller(randTime);
    }

    // Add randomness in speed
    if (randSpeed == 0)
    {
      randomSpeedOffset = 0;
    }
    else
    {
      randomSpeedOffset = box_muller(randSpeed);
    }

    lastTS = newTS;
    initTest();
    catchedTarget = false;
  }

  if (timestamp > 100 * 1000UL && timestamp < 150 * 1000UL)
  {
    stopPos = 0;
  }
}

uint32_t colorFader(uint32_t color, int divider)
{
  uint8_t R = color & 0xFF0000 / 0x010000;
  uint8_t G = color & 0x00FF00 / 0x000100;
  uint8_t B = color & 0x0000FF;

  R /= divider;
  G /= divider;
  B /= divider;

  return R * 0x010000 + G * 0x000100 + B;
}

unsigned long readNumber()
{
  String inString = "";
  for (int i = 0; i < 10; i++)
  {
    while (Serial.available() == 0);
    int inChar = Serial.read();
    if (isDigit(inChar))
    {
      inString += (char)inChar;
    }

    if (inChar == '\n')
    {
      int val = inString.toInt();
      return (unsigned long)val;
    }
  }

  // flush remain strings in serial buffer
  while (Serial.available() > 0)
  {
    Serial.read();
  }
  return 0UL;
}

// gaussian random, adopted from https://github.com/ivanseidel/Gaussian/blob/master/Gaussian.h
double GaussianRandom(double mean, double variance) {
  double R1, R2;
  R1 = ::random(MAX_VARIANCE) / (double)MAX_VARIANCE;
  R2 = ::random(MAX_VARIANCE) / (double)MAX_VARIANCE;
  return mean + variance * cos( 2 * M_PI * R1) * sqrt(-log(R2));
}


// adopted from https://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform
double box_muller(double sigma)
{
  double epsilon = 0.0000000000001;
  double two_pi = 2.0 * 3.14159265358979323846;

  double z0, z1;
  double u1, u2;
  do
  {
    u1 = rand() * (1.0 / RAND_MAX);
    u2 = rand() * (1.0 / RAND_MAX);
  }
  while (u1 <= epsilon);

  z0 = sqrt(-2.0 * log(u1)) * cos(two_pi * u2);
  z1 = sqrt(-2.0 * log(u1)) * sin(two_pi * u2);

  // prevent > 3sigma value
  if (z0 < -3)
    z0 = -3;
  if (z0 > 3)
    z0 = 3;

  return z0 * sigma;
}

