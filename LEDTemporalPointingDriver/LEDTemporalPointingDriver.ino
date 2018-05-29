#include "CircularBuffer.h"

struct Button {
  unsigned long timestamp;
  int value;
};

const int targetPin = 3;
const int aimPin = 4;

const int buttonPin = 2;
const int buttonAttachPin = 6;

bool targetPinState = false;
bool aimPinState = false;
bool buttonPinState = false;

// button debouncer
unsigned long debounceTime = 0;
bool buttonState = false;

unsigned long resetTime = 0;

unsigned long lastMicro;

// implementing button delay
unsigned long latency = 50UL * 1000UL;
unsigned long lastSavedTime = 0;
CircularBuffer<Button, 1000> buffer;  // maximum 500 ms
// to use: buffer.push(Button{ts, value});   /  Button data = buffer.pop();

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial1.begin(115200);

  pinMode(targetPin, INPUT);
  pinMode(aimPin, INPUT);
  pinMode(buttonPin, OUTPUT);
  pinMode(buttonAttachPin, INPUT);
  
  targetPinState = digitalRead(targetPin);
  aimPinState = digitalRead(aimPin);

  // assign internal pull-up
  digitalWrite(buttonAttachPin, HIGH);

  lastSavedTime = micros();
}


void loop() {
  // transfer serial messages to the strip driver.
  if (Serial.available() > 0)
  {
    char c = Serial.read();
    if(c == 'l')  // latency command
    {
      latency = readNumber() * 1000;

      if(latency > 500UL * 1000UL)
      {
        Serial.println(F("ERROR: TOO MUCH LATENCY: limit to 500"));
        latency = 500UL * 1000UL;
      }        
    }
    
    Serial1.write(c);
  }

  // time keeping (unit: 100 us = 0.1 ms).
  unsigned long newMicro = micros();
  unsigned long timestamp = (newMicro - lastMicro) / 100UL;
  if(resetTime != 0 && timestamp >= resetTime)
  {
    lastMicro = newMicro;
    resetTime = 0;
    Serial.print(timestamp);
    Serial.println("\tR");
  }

  // add delay
  unsigned long elapsedTimeFromLastSave = newMicro - lastSavedTime;
  int rawButtonValue = digitalRead(buttonAttachPin);
  int buttonValue = rawButtonValue;
  
  if(elapsedTimeFromLastSave > 500)  // 0.5 ms gaps between each item on buffer
  {
    lastSavedTime = newMicro;
    buffer.push(Button{newMicro, digitalRead(buttonAttachPin)});   // add value to tail
  }
  
  while(buffer.remain() > 0) // clear the expired data (elapsed more than [latency]) from the queue
  {
    if(newMicro - buffer.peek().timestamp > latency)
    {
      buffer.pop();
    }
    else
      break;
  }
  if(buffer.remain() > 0)
    buttonValue = buffer.peek().value; // assign the delayed button value from the head of the queue.

  // button trigger and debounce
  if(buttonValue == LOW)
  {
    if(buttonState == false)
    {
      debounceTime = newMicro;
      buttonState = true;
    }
  }
  else
  {
    unsigned long dt = newMicro - debounceTime;

    // 300 ms debounce
    if(dt > 300*1000UL && buttonState == true)
    {
      buttonState = false;
    }
  }

  if(buttonValue != buttonPinState)
  {
    buttonPinState = !buttonPinState;

    if(buttonPinState)  // Button pin Raising edge (button off)
    {
      Serial.print(timestamp);
      Serial.println(F("\tB0"));
    }
    else                // Button pin Falling edge (button on)
    {
      Serial.print(timestamp);
      Serial.println(F("\tB1"));
    }
  }

  if(digitalRead(targetPin) != targetPinState)
  {
    targetPinState = !targetPinState;
    
    if(targetPinState)  // Target pin Raising edge
    {
      Serial.print(timestamp);
      Serial.println(F("\tT1"));
    }
    else                // Target pin Falling edge
    {
      Serial.print(timestamp);
      Serial.println(F("\tT0"));         
    }
  }
  if(digitalRead(aimPin) != aimPinState)
  {
    aimPinState = !aimPinState;
    if(aimPinState)   // Aim pin Raising edge
    {
      Serial.print(timestamp);
      Serial.println(F("\tA1"));
    }
    else              // Aim pin Falling edge
    {
      // set timer reset at T+300ms
      resetTime = timestamp + 3000UL;
      Serial.print(timestamp);
      Serial.println(F("\tA0"));
    }
  }

  digitalWrite(buttonPin, !buttonState);
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
