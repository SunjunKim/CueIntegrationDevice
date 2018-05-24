/**
 * Temporal Pointing LED Strip
 * Experiment driver
 * Written by Sunjun Kim, Apr 14 2017
 */

import processing.serial.*;
import controlP5.*;
import arb.soundcipher.*;
import java.util.Collections;
import java.util.Random;

Serial sp;

int numRepeat = 5;
int restTime = 5;
int marginStart = 0;  // margin test start
int marginEnd = 250;    // margin test end
int marginStep = 50;  // margin test step  ==> for (t = start ; t <= end ; t += step)

int tD = 2000;  // temporal distance
int tW = 100;   // temporal width
int tDrnd = 300;    // stdev of temporal distance  ==> temporal distance in experiment = distNormal(tD, rnd^2);
int spdRnd = 50;
int speed1 = 150;  // test speed #1 (LED/s)
int speed2 = 300;  // test speed #2 (LED/s)

int feedbackType = 3;
int aim = 210;

StringList buffer = new StringList();
SoundCipher sc = new SoundCipher(this);
int lf = 10; 

ArrayList<UnitExperiment> conditions = new ArrayList<UnitExperiment>();

ControlP5 cp5;
Textfield tfName;
String pName = "";

Textlabel tlMessage, tlProgress;
PrintWriter logRaw, logResult, logTiming;

Bang startBtn, beginBtn;

boolean started = false;
int conditionNo = 0;
int repeatCount = 0;

void setup()
{
  size(500, 300);

  String[] serials = Serial.list();
  String portName = serials[serials.length - 1];
  sp = new Serial(this, portName, 115200);
  sp.bufferUntil(lf);


  for (int margin=marginStart; margin<=marginEnd; margin+=marginStep)
  {
    //public UnitExperiment(int d, int w, int margin, int s, int fbType)
    conditions.add(new UnitExperiment(tD, tW, margin, speed1, feedbackType));
    conditions.add(new UnitExperiment(tD, tW, margin, speed2, feedbackType));
  }
  
  long seed = System.nanoTime();
 
  for(int i=0;i<conditions.size();i++)
  {
    println(conditions.get(i));
  }
  
  Collections.shuffle(conditions, new Random(seed));

  PFont font = createFont("arial", 20);
  textFont(font);  

  cp5 = new ControlP5(this);

  tfName = cp5.addTextfield("Participant:")
    .setPosition(20, 20)
    .setSize(200, 40)
    .setFocus(true)
    .setFont(font)
    .setColor(color(255, 255, 255))
    ;

  startBtn = cp5.addBang("run").setPosition(240, 20).setSize(80, 40);
  beginBtn = cp5.addBang("Begin").setPosition(100, 100).setSize(80, 40);
  //nextBtn = cp5.addBang("Next").setPosition(200, 100).setSize(80, 40);

  startBtn.getCaptionLabel().align(ControlP5.CENTER, ControlP5.CENTER);
  beginBtn.getCaptionLabel().align(ControlP5.CENTER, ControlP5.CENTER);

  beginBtn.hide();

  tlProgress = new Textlabel(cp5, "", 100, 160, 400, 300);
  tlMessage = new Textlabel(cp5, "", 100, 200, 400, 40);

  tlProgress.setFont(font).setColor(color(255, 0, 255));
  tlMessage.setFont(font).setColor(color(127));

  playStartSound();
}

void draw()
{
  background(0);
  text(pName, 165, 84);

  color(255);
  text("Test: "+(conditionNo+1)+"/"+conditions.size(), 350, 50);

  if (started)
  {
    tlProgress.setText("Progress: "+(repeatCount)+"/"+numRepeat);
    tlProgress.draw(this);
  }

  tlMessage.draw(this);
}

// pressed "run" button
public void run()
{
  if (pName.equals(""))
  {
    pName = tfName.getText();
    startBtn.hide();
    tfName.lock();
        
    logResult = createWriter(pName+"/result.csv");
    logTiming = createWriter(pName+"/timing.csv");
    logRaw = createWriter(pName+"/rawLog.csv");

    started = true;
    conditionNo = 0;
    setExperiment(conditionNo);

    beginBtn.show();
  }
}

void keyPressed()
{
  if(keyCode == ' ' && beginBtn.isVisible())
  {
    Begin();
  }
  if(key == 'r')
  {
    UnitExperiment ue = conditions.get(conditionNo);
    ue.setCondition(sp);
  } 
}

// pressed "begin" button
public void Begin()
{
  playStartSound();
  beginBtn.hide();
  started = true;
}

// executed at the begin of each condition
public void setExperiment(int expNo)
{
  if (expNo >= conditions.size())
  {
    // end of experiment!
    beginBtn.hide();
    tlMessage.setText("END!! Call the experimenter.");
    playFinaleSound();
    
    logResult.flush();
    logResult.close();
    
    logRaw.flush();
    logRaw.close();
    
    logTiming.flush();
    logTiming.close();
    return;
  }

  started = false;
  beginBtn.show();

  UnitExperiment ue = conditions.get(expNo);
  tlMessage.setText(ue.toString().replace('\t','\n'));
  repeatCount = 0;
  
  logRaw.println(ue.toLogString());
  logResult.print(ue.toLogString());
  logTiming.print(ue.toLogString());    
  logRaw.flush();
  logResult.flush();  
  logTiming.flush();
  
  ue.setCondition(sp);
}

void serialEvent(Serial myPort) 
{

  try {
    while (myPort != null && myPort.available() > 0) {
      String myString = myPort.readString();
      if (myString != null) {
        processCommand(trim(myString));
      }
    }
  }
  catch(RuntimeException e) {
    e.printStackTrace();
  }
}

void processCommand(String command)
{
  String[] tokens = split(command, '\t');
  if (tokens.length != 2)
    return;

  buffer.append(command); 
  if (getType(command).equals("R"))
  {
    processBuffer();
  }
}

// excuted after one trial.
void processBuffer()
{
  int T1 = findItemInBuffer("T1");    // target appear time
  int T0 = findItemInBuffer("T0");    // target disappear time
  int A1 = findItemInBuffer("A1");    // target entered the aim zone
  int A0 = findItemInBuffer("A0");    // target escaped the aim zone
  int B1 = findItemInBuffer("B1");    // button pressed

  if (T1 == -1 || T0 == -1 || A1 == -1 || A0 == -1 || B1 == -1)
    return;

  boolean success = (B1 >= A1 && B1 <= A0);

  if (started)
  {
    // write to the log file
    for(int i=0;i<buffer.size();i++)
    {
      logRaw.println(buffer.get(i).replace('\t',','));
    } 
    logResult.print(","+(success?1:0));
    logTiming.print(","+(B1-A1));    

    repeatCount++;
    if (repeatCount == numRepeat)
    {
      started = false;
      logResult.println();
      logTiming.println();
      playEndSound();
      delay(restTime*1000);
      playRestartSound();
      beginBtn.show();
      setExperiment(++conditionNo);
    }
    
    logRaw.flush();
    logResult.flush();
    logTiming.flush();
  }

  buffer.clear();
}

// Utility functions ===================================================

int findItemInBuffer(String commandType)
{
  int n = buffer.size();
  for (int i=0; i<n; i++)
  {
    String command = buffer.get(i);
    if (getType(command).equals(commandType))
      return getTimeStamp(command);
  }

  return -1;
}

int getTimeStamp(String command)
{
  String[] tokens = split(command, '\t');
  if (tokens.length != 2)
    return -1;

  return int(tokens[0]);
}

String getType(String command)
{
  String[] tokens = split(command, '\t');
  if (tokens.length != 2)
    return "";

  return tokens[1];
}


void playStartSound()
{
  sc.playNote(70, 50, 2.0);
}

void playEndSound()
{
  float[] pitches = {60, 65};
  float[] dynamics = {100, 100};
  float[] durations = {0.3, 0.3};

  sc.playPhrase(pitches, dynamics, durations);
}

void playRestartSound()
{
  float[] pitches = {65, 60};
  float[] dynamics = {100, 100};
  float[] durations = {0.3, 0.3};

  sc.playPhrase(pitches, dynamics, durations);
}


void playFinaleSound()
{
  float[] pitches = {60, 70, 65, 70, 65, 70, 65};
  float[] dynamics = {0, 100, 50, 100, 50, 100, 50};
  float[] durations = {2, 0.3, 0.6, 0.3, 0.6, 0.3, 0.6};

  sc.playPhrase(pitches, dynamics, durations);
}



class UnitExperiment
{
  public int tDistance; // unit: ms
  public int tWidth; // unit: ms
  public int tMargin; // unit: ms
  public int speed; // unit: led/sec
  public int feedback;

  public UnitExperiment(int d, int w, int margin, int s, int fbType)
  {    
    tDistance = d;
    tWidth = w;    
    tMargin = margin;
    speed = s;
    feedback = fbType;
  }

  public void setCondition(Serial port)
  {
    port.write("a"+aim+"\n");
    port.write("t"+rnd+"\n");
    port.write("d"+tDistance+"\n");
    port.write("w"+tWidth+"\n");
    port.write("m"+tMargin+"\n");
    port.write("s"+speed+"\n");
    port.write("p"+feedback+"\n");
  }

  public String toString()
  {
    String ret = "";

    ret += "Dist:"+tDistance;
    ret += "\tWidth:"+tWidth;
    ret += "\tMargin:"+tMargin;
    ret += "\tSpeed:"+speed;
    return ret;
  }
  
  public String toLogString()
  {
     String ret = "";

    ret += "d"+tDistance+",w"+tWidth+",m"+tMargin+",s"+speed+",p"+feedback;
    return ret;
  }
}