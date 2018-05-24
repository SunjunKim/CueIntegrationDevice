
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
}


void loop() {
  // transfer serial messages to the strip driver.
  if (Serial.available() > 0)
  {
    Serial1.write(Serial.read());
  }

  // time keeping (unit: 100 us).
  unsigned long newMicro = micros();
  unsigned long timestamp = (newMicro - lastMicro) / 100UL;
  if(resetTime != 0 && timestamp >= resetTime)
  {
    lastMicro = newMicro;
    resetTime = 0;
    Serial.print(timestamp);
    Serial.println("\tR");
  }

  // button trigger and debounce
  if(digitalRead(buttonAttachPin) == LOW)
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

  if(digitalRead(buttonAttachPin) != buttonPinState)
  {
    buttonPinState = !buttonPinState;

    if(buttonPinState)  // Button pin Raising edge
    {
      Serial.print(timestamp);
      Serial.println("\tB0");
    }
    else                // Button pin Falling edge (button on)
    {
      Serial.print(timestamp);
      Serial.println("\tB1");
    }
  }

  if(digitalRead(targetPin) != targetPinState)
  {
    targetPinState = !targetPinState;
    
    if(targetPinState)  // Target pin Raising edge
    {
      Serial.print(timestamp);
      Serial.println("\tT1");
    }
    else                // Target pin Falling edge
    {
      Serial.print(timestamp);
      Serial.println("\tT0");         
    }
  }
  if(digitalRead(aimPin) != aimPinState)
  {
    aimPinState = !aimPinState;
    if(aimPinState)   // Aim pin Raising edge
    {
      Serial.print(timestamp);
      Serial.println("\tA1");
    }
    else              // Aim pin Falling edge
    {
      // set timer reset at T+300ms
      resetTime = timestamp + 3000UL;
      Serial.print(timestamp);
      Serial.println("\tA0");
    }
  }

  digitalWrite(buttonPin, !buttonState);
}
