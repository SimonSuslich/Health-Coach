/*
  File: 1.0_completed_version.ino
  Author: Simon Niklas Suslich
  Date: 2023-12-13
  Description: Denna projekt använder en vibrationssensor för att mäta av rörelse och en RTC modul för att läsa av fysiskt inaktiv tid. Informationen ges som output på en OLED skärm och på en piezo som skickar ut ljudsignaler. När den Inaktiva tiden överskrider ett angivet värde, till exempel 10 sekunder skickas ut en kort ljudsignal och på Oled skärmen står det att man har varit fysiskt inaktiv och hur länge man har varit det. För att ställa tillbaka den inaktiva tiden till 0 måste man antingen röra på sig (triggera vibrationssensorn) eller välja ett träningspass bland de inbyggda träningspassen som finns i programmet. För att välja ett träningspass ska man skrolla med en Joystick fram eller tillbaka genom listan med de olika träningspassen. Man kan trycka på Joystickens knapp för att välja träningsprogram. Det kommer skickas ut en ljudsignal varenda gång en övning börjar och när en övning slutar. För att visa enkelt och smidigt hur mycket tid man har på sig för att utföra en övning finns en NeoPixel LED Ring med, som gör en countdown. Man får en när ett träninspass är över

*/



// Inkludera olika bibliotek för Joysticken, LED ringen, OLED skärmen, RTC modulen och Wire

#include <ezButton.h>
#include <Adafruit_NeoPixel.h>
#include <U8glib.h>
#include <RtcDS3231.h>
#include <Wire.h>


//Variablar för LED ringen
#define LED_PIN 6
#define LED_COUNT 18

// Variablar för Joystickens pinnar
#define VRX_PIN A0  // Arduino pin connected to VRX pin
#define VRY_PIN A1  // Arduino pin connected to VRY pin
#define SW_PIN 2    // Arduino pin connected to SW  pin


//Definiera RTC object, object för joystick knappen, OLED object och LED Ring object
RtcDS3231<TwoWire> rtcModule(Wire);
ezButton button(SW_PIN);
U8GLIB_SSD1306_128X64 oled(U8G_I2C_OPT_NO_ACK);
Adafruit_NeoPixel ring(LED_COUNT, LED_PIN, NEO_RGBW + NEO_KHZ800);

//Variablar för att lagra informationen om värderna för Joysticken
int xValue = 0;  // To store value of the X axis
int yValue = 0;  // To store value of the Y axis
int bValue = 0;  // To store value of the button

//definiera pinnar för piezon och vibrationssensorn
int vib_pin = 5;
int piezo = 8;

//variablar för vibrationssensorn
int vibState; // 1 eller 0, dvs finns det vibrationer eller ej
int vibCount = 0; // hur många vibrationer som har skett

// class för övningar för olika träningspass
class Exercise {
public:
  String name; //namn på träningspass
  String exercises[3]; //namn på övningar
  int duration[3]; //hur lång tid övningarna varar
};

// Create an instance of the Exercise class
Exercise cardio;  
Exercise legs;
Exercise arms;

// Sammla alla träningspass i en array
Exercise allExercises[3];
// Skapa en array för att enkelt nå de olika träningspassen i menyn
String menu[3];  // Items in the menu

//Index för menu arrayen, dvs en specifik träningsprogram
int menuIndex = 0;

// Generell tid för vila, kan väljas här
int restTime = 5;

//class för att enklare definiera tid
class Time {
public:
  int seconds;
  int minutes;
  int hours;
};
// variablar för tid
byte hour;
byte minute;
byte second;

byte initialSecond; //Sekunden man har just nu
byte lastSecond; // Den sista registrerade sekunden, viktig för att itterera tiden
byte lastDelaySecond; // Samma fast för delay-timern

// skapa object av class Time
Time inactiveTime; // Hur länga man har varit inaktiv totalt
Time inactiveDelayTime; // hur länge man har varit inaktiv mellan notifikationerna


//Kalla på funktionen så att den fungerar i void setup, annars kan koden sluta fungera ibland.
void updateOled(String text, String type = "default");


/* I Void setup sätts igång Serial.begin
  Sätter värde på Exercise objekten
  lägger till Exercise objecten till AllExercises[]
  lägger till allExercises[].name till menu[]
  Wire.begin(), initialiserar kommunikation
  Initialiserar tid utifrån datorn i RTC modulen
  bestämmer en debounce time för knappen på Joysticken
  initialiserar LED ringen och ger en bas ljusstyrka
  Anger pinMode för piezon och vib_pin
  bestämmer font för oled skärmen
  Skriver "HEALTH COACH för att välkommna" på OLED
  Skriver menyn på OLED, börjar med menu[menuIndex], då menuIndex = 0
  Sätter inaktiv tip till 0
  sätter delay tiden till 0

*/
void setup() {

  Serial.begin(9600);

  // Assign values to the exercise arrays
  cardio.name = "Cardio exercise";
  cardio.exercises[0] = "Jumping Jacks";
  cardio.exercises[1] = "Jump Rope";
  cardio.exercises[2] = "Jogging on Place";
  cardio.duration[0] = 5;
  cardio.duration[1] = 5;
  cardio.duration[2] = 5;

  legs.name = "Legs exercise";
  legs.exercises[0] = "Squats";
  legs.exercises[1] = "Squat Jumps";
  legs.exercises[2] = "Lunges";
  legs.duration[0] = 5;
  legs.duration[1] = 5;
  legs.duration[2] = 5;

  arms.name = "Arms exercise";
  arms.exercises[0] = "Pushups";
  arms.exercises[1] = "Dips";
  arms.exercises[2] = "Pullups";
  arms.duration[0] = 5;
  arms.duration[1] = 5;
  arms.duration[2] = 5;

  allExercises[0] = cardio;
  allExercises[1] = legs;
  allExercises[2] = arms;

  menu[0] = allExercises[0].name;
  menu[1] = allExercises[1].name;
  menu[2] = allExercises[2].name;


  Wire.begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  rtcModule.SetDateTime(compiled);

  button.setDebounceTime(50);  // set debounce time to 50 millisecond

  ring.begin();
  ring.show();
  ring.setBrightness(25);

  pinMode(vib_pin, INPUT);
  pinMode(piezo, OUTPUT);

  oled.setFont(u8g_font_helvR10);
  updateOled("HEALTH COACH", "centered");
  delay(2000);
  updateOled(menu[menuIndex], "scroll");

  setInactiveTimeToZero();
  setInactiveDelayTime();
}

/*
  Updaterar tiden
  kollar eventuella vibrationer
  if sats nr1: om man har varit inaktiv i 10 sekunder och har rört sig mindre än 3 gånger och om delay tiden sedan den senaste notifikationen är mer än 10 sekunder, då
    spela ljudnotifikation för att få uppmärksamhet
    skriv på Oled att man har varit inaktiv och hur länge
  annars om man har rört sig minst 3 gånger efter att inaktiv time är större än 10 sekunder, då
    vibCount (antalet rörelser) tillbaka till noll
    inaktiv tid tillbaka till noll
    delay tid tillbaka till noll
  
  kallar på button loop för att få den att fungera
  Om använder button.isReleased() kommer koden köras i if sats nr2
    först kollar hur många övningar det finns i det valda träningspasset
    Sen loopar igenom alla övningar i träningspasset.
    Skriver ut på skärmen vilket övning man ska göra
    Kallar på piezon när övningen börjar
    Sätter igång ledCountDown(), dvs visar på LED ringen hur lång tid man ska göra övningen
    Genom en if sats avgör om den ska skriva "rest" eller "exercise completed"
    Spelar ljudsignal
    Skriver på skärmen när exercisen är slut

    sätter menuIndex tillbaka till 0
    skriver på skärmen menyn
    sätter inaktive tid på noll och delay tid på noll
  
  läser av analaoga värden på joystickens x och y värde

  if sats nr3:
    använder en if sats logik för att avgöra om man styr åt höger eller vänster med joysticken och beroende på värdet på menuIndex
    ökar eller sänker den, dvs skrollar igenom menyn. Uppdaterar vad som skrivs på OLED.
    


*/

void loop() {
  updateTime();
  checkVibrations(); 

  //If sats nr 1
  if (inactiveTime.seconds > 10 && vibCount < 3) {
    if (inactiveDelayTime.seconds > 10) {
      piezoAlert("high");
      updateOled("TOO INACTIVE !!!", "centered");
      delay(1500);
      updateOled("The last time you were active was:", "centered");
      delay(1000);
      updateTime();
      updateOled(String(inactiveTime.hours) + " h, " + String(inactiveTime.minutes) + " m, " + String(inactiveTime.seconds) + " s", "centered");
      delay(2000);
      updateOled(menu[menuIndex], "scroll");
      setInactiveDelayTime();
    }
  } else if (inactiveTime.seconds > 10 && vibCount >= 3) {
    vibCount = 0;
    setInactiveTimeToZero();
    setInactiveDelayTime();
  }

  button.loop();  // MUST call the loop() function first
  //if sats nr2
  if (button.isReleased()) {
    int lengthOfArray;
    for (auto i : allExercises[menuIndex].exercises) {
      lengthOfArray++;
    }

    for (int index = 0; index < lengthOfArray; index++) {
      updateOled(allExercises[menuIndex].exercises[index], "centered");
      piezoAlert("high");
      ledCountDown(allExercises[menuIndex].duration[index]);      
      if (index + 1 != lengthOfArray) {
        updateOled("Rest", "centered");
        piezoAlert("low");
        ledRecharge();
      } else {
        updateOled("Exercise completed", "centered");
        piezoAlert("low");
        updateOled("Great Job!", "centered");
        delay(1000);
      }
    }

    menuIndex = 0;
    updateOled(menu[menuIndex], "scroll");
    setInactiveTimeToZero();
    setInactiveDelayTime();
  }

  xValue = analogRead(VRX_PIN);
  yValue = analogRead(VRY_PIN);

  // if sats nr 3
  if (xValue > 900 && menuIndex < 2) {
    menuIndex++;
    updateOled(menu[menuIndex], "scroll");
    delay(500);
  } else if (xValue > 900 && menuIndex == 2) {
    menuIndex = 0;
    updateOled(menu[menuIndex], "scroll");
    delay(500);
  } else if (xValue < 100 && menuIndex > 0) {
    menuIndex--;
    updateOled(menu[menuIndex], "scroll");
    delay(500);
  } else if (xValue < 100 && menuIndex == 0) {
    menuIndex = 2;
    updateOled(menu[menuIndex], "scroll");
    delay(500);
  }
}

/*
  Funktionen sätter delay tiden till noll
  Kollar vilken tid som är nu
  lägger det värdet på lastDelaySecond

  lägger värde på sekunderna minuterna och timmarna till noll
*/

void setInactiveDelayTime() {
  RtcDateTime now = rtcModule.GetDateTime();
  initialSecond = now.Second();
  lastDelaySecond = initialSecond;

  inactiveDelayTime.seconds = 0;
  inactiveDelayTime.minutes = 0;
  inactiveDelayTime.hours = 0;
}

/*
  Funktionen sätter inaktiv tiden till noll
  Kollar vilken tid som är nu, tilldelar värdet till initialSecond
  lägger det värdet på lastSecond

  lägger värde på sekunderna minuterna och timmarna till noll
*/

void setInactiveTimeToZero() {
  RtcDateTime now = rtcModule.GetDateTime();
  initialSecond = now.Second();
  lastSecond = initialSecond;

  inactiveTime.seconds = 0;
  inactiveTime.minutes = 0;
  inactiveTime.hours = 0;
}

/*
  Funktionen använder sig av RTC modulen för att kolla tiden just nu
  (inactive time)
  om tiden som sparades i lastSecond variabeln, räknas det som en sekund

  Om det finns mer än 59 sekunder, räknas det in som en minut
  om det finns mer än 59 minuter, räknas det in som en timma

  Samma logik för delay tiden
*/

void updateTime() {

  RtcDateTime current = rtcModule.GetDateTime();

//inactive time
  if (current.Second() != lastSecond) {
    inactiveTime.seconds++;
    inactiveDelayTime.seconds++;
    lastSecond = current.Second();
  }

  if (inactiveTime.seconds > 59) {
    inactiveTime.seconds = 0;
    inactiveTime.minutes++;
  }

  if (inactiveTime.minutes > 59) {
    inactiveTime.minutes = 0;
    inactiveTime.hours++;
  }

//delay time
  if (current.Second() != lastSecond) {
    inactiveDelayTime.seconds++;
    lastSecond = current.Second();
  }

  if (inactiveDelayTime.seconds > 59) {
    inactiveDelayTime.seconds = 0;
    inactiveDelayTime.minutes++;
  }

  if (inactiveDelayTime.minutes > 59) {
    inactiveDelayTime.minutes = 0;
    inactiveDelayTime.hours++;
  }
}

/*
  Funktionen spelar en notifikation med piezon, tar emot en string som input
  Beroende på om inputen är "high" eller "low" kan piezon ge olika ljud
  Spelar ljudet i 1000 ms
*/
void piezoAlert(String type) {
  int pitch;

  if (type == "high") {
    pitch = 1028;
  } else if (type == "low") {
    pitch = 514;
  }

  tone(piezo, pitch);
  delay(1000);
  noTone(piezo);
}

/*
  Funktionen ändrar på texten som står på oled skärmen
  Funktionen tar emot minst en string som input, vilket är texten som ska visas på skärmen
  Den andra, optionella inputen, är eventuella sätt man kan modifiera på utseendet av texten
  Exempelvis om man skriver att type = "centered" kommer text meddelanded finnas i mitten
  Om man skriver type = "scroll" kommer textmeddelanded visas som om den är en del av en meny man kan skrolla i

  deklarera variabler för x och y värden på skärmen, för att göra det enklare med "centered"

  if sats, om type == "centered", då
    x värden balanceras utifrån hur långt meddelandet är, standardbredd på en karaktär = 4.125 (px)

  beroende på om type == "scroll" eller inte, kommer skärmen skriva textmeddelandet i övre vänstra hörnan och "previous" och "next" i de nedre hörnen

  om type != "scroll" bara visa text meddelanden. Om den är längre än 17 karaktärer lång kommer x värdet att ändras med hjäljp av en for loop för hela
  textmeddelandet skulle synas på skärmen.

  i samtliga fall används en do-while loop för att visa information på oled skärmen.
*/

void updateOled(String text, String type = "default") {
  int xPosition = 0;
  int yPosition = 11;
  if (type == "centered") {
    if (text.length() < 17) {
      xPosition = 64 - text.length() * 4.125;
    } else {
      xPosition = 0;
    }
    yPosition = 37;
  }

  if (type == "scroll") {
    oled.firstPage();
    do {
      oled.drawStr(xPosition, yPosition, text.c_str());
      oled.drawStr(0, 54, "Previous");
      oled.drawStr(100, 54, "Next");
    } while (oled.nextPage());
  } else {
    if (text.length() < 17) {
      oled.firstPage();
      do {
        oled.drawStr(xPosition, yPosition, text.c_str());
      } while (oled.nextPage());
    } else {
      oled.firstPage();
      do {
        oled.drawStr(0, yPosition, text.c_str());
      } while (oled.nextPage());
      delay(500);
      for (int i = 0; i < (text.length() - 17) * 5.25; i += 4) {
        oled.firstPage();
        do {
          oled.drawStr(-i, yPosition, text.c_str());
        } while (oled.nextPage());
      }
      delay(500);
    }
  }
}

/*
  funktionen tar emot en interger som input, 
  Inputen är en specifik tid som countdownen ska gå (basically en timer)

  delayTime är en siffra som innebär hur lång tid finns mellan varje gång LED ringen uppdateras¨
  Tiden är angiven i ms (*1000) och är dividerad per antal dioder (18)

  Sedan loopar programmet snabbt igenom alla dioder och tänder de nästan samtidigt (ser ut samtidigt för ett blott öga)

  Sist loopar programmet igenom de tända ljusen och släcker de en efter en, efter delay tiden som bestämdes i början av funktionen

*/

void ledCountDown(int time) {
  float delayTime = (time / 18.0) * 1000;

  for (int i = 0; i < ring.numPixels(); i++) {
    ring.setPixelColor(i, 255, 255, 255, 255);
    ring.show();
  }
  for (int i = 0; i < ring.numPixels(); i++) {
    delay(delayTime);
    ring.setPixelColor(i, 0, 0, 0, 0);
    ring.show();
  }
}

/*
  Funktionen följer samma logik som ledCountDown(time), men tar inte emot någon input.
  Anledningen varför det är så är för att det är enklare att ställa in en generell vilo tid (restTime)
  delayTime i det här fallet är *100, eftersom animationen där det är ett ljus som cirkulerar runt ringen ska spelas 10 gånger, och inte 1 gång

  en Loop inne i en loop. Loop 2 säger att ringen ska skina med en viss färg i en specifik diod under delaytiden och sen slockna
  Loop 1 säger att denna animationen ska spelas 10 ggr.
*/

void ledRecharge() {
  int delayTime = (restTime / 18.0) * 100;

  for (int j = 0; j < 10; j++) {
    for (int i = 0; i < ring.numPixels(); i++) {
      ring.setPixelColor(i, 255, 255, 255, 255);
      ring.show();
      delay(delayTime);
      ring.setPixelColor(i, 0, 0, 0, 0);
      ring.show();
    }
  }
}

/*
  Funktionen kollar om vibrationssensorn är triggered
  vibstate är en variable som kan vara 1 eller 0 och visar om det leds ström genom vibrationssenorn eller ej

  när vibstate == 1, dvs det vibrationssensorn är triggad, läggs till ett värde till vibCount, som räknar hur många gången användaren har rört sig.
*/

void checkVibrations() {
  vibState = digitalRead(vib_pin);

  if (vibState == 1) {
    vibCount++;
  }
}