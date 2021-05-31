#include <Servo.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <RFID.h>

// til oppsett av servo
Servo myServo;

// til oppsett av RFID
const int SS_PIN = 10;
const int RST_PIN = 9;
RFID rfid(SS_PIN, RST_PIN);

// til oppsett av neopixel
const int neoPixel = 3;
const int antallLed = 19;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(antallLed, neoPixel, NEO_GRB + NEO_KHZ800);

// til kjøring av programmet
// tilgangskontroll
bool gyldigKort = false;
// utregning av høydemeter
int etasjeNummer = 5;
int etasjeHoyde = 3;
int gjeldendeHoyde;
// tar vare på totalt antall hoydemeter gatt av etasjen
int totalHoyde = 0;
//Hvor lyset er til enhver tid - hvor lyset er på den av totale lengden av dioder (19)
int lysLevel = 0;
// array av gyldige kort
int cards [][5] = {{158,196,208,21,159}, {86,100,255,43,230}};

// til 'milestones'
//Nar lys skal lyse:
int lysLevelArray[antallLed];
//Nar luke skal apnes - :
int basecampArray[4];

// ulike nivåer med utfordring som kan brukes til boksen
String fjellNavn[2] = {"Galdhøpiggen", "Mount Everest"};
int fjellHoyde[2] = {2469, 8800};


// farger til LED-lys
int blaa[] = {100, 255, 255};

void setup() {
    Serial.begin(9600);
    
    byggArray(1);

    myServo.attach(8);
    myServo.write(0);

    strip.begin();
    strip.show();

    SPI.begin();
    rfid.init();
}

// oppretter arrayene som skal benyttes til å avgjøre når lys skal lyse og døren åpnes
void byggArray(int fjellIndeks) {
    String navn = fjellNavn[fjellIndeks];
    int hoyde = fjellHoyde[fjellIndeks];

    Serial.println("Starter program med fjellet " + navn + ".");

    int lysIntervalLengde = hoyde / antallLed;
    for (int i = 0; i < antallLed; i++) {
        lysLevelArray[i] = (i + 1) * lysIntervalLengde;
    }

    basecampArray[0] = lysLevelArray[3];
    basecampArray[1] = lysLevelArray[8];
    basecampArray[2] = lysLevelArray[13];
    basecampArray[3] = lysLevelArray[18];
}

void loop () {
    // looper i løkken fram til et kort er funnet
    while (!gyldigKort) {
        scanKort();
    }

    // oppdaterer neopixel basert på ny informasjon
    oppdaterLys();

    // oppdaterer motor basert på ny informasjon
    oppdaterMotor();

    // kort delay for å sikre at samme kort ikke blir scannet flere ganger
    delay(1000);
    
    gyldigKort = false;
}




void scanKort() {
    // sjekker om kortet er gyldig
    if (rfid.isCard()) {
        if (rfid.readCardSerial()) {
            Serial.println("");
            Serial.print(rfid.serNum[0]);
            Serial.print(" ");
            Serial.print(rfid.serNum[1]);
            Serial.print(" ");
            Serial.print(rfid.serNum[2]);
            Serial.print(" ");
            Serial.print(rfid.serNum[3]);
            Serial.print(" ");
            Serial.print(rfid.serNum[4]);
            Serial.println("");

            for (int x = 0; x < sizeof(cards); x++) {
                for(int i = 0; i < sizeof(rfid.serNum); i++ ){
                    if(rfid.serNum[i] != cards[x][i]) {
                        gyldigKort = false;
                        break;
                    } else {
                        gyldigKort = true;
                    }
                }
                if (gyldigKort) break;
            }
        }

        if (gyldigKort) {
            oppdaterHoyde();
            Serial.println("gyldig kort");
        }

    } else {
        gyldigKort = false;
    }
}

void oppdaterHoyde() {
    // regner ut høydemeter og legger det til totalen
    gjeldendeHoyde = (etasjeNummer - 1) * etasjeHoyde; 
    totalHoyde += gjeldendeHoyde;
}





//OppdaterLys og tilhorende metoder - handterer alt som har med lys a gjore:

void oppdaterLys() {
    lysPuls(blaa, lysLevel, antallLed, 25);
    lysLevel = sjekkLysLevel();
    tegnDiodeInterval(0, lysLevel, blaa);
}

// sjekker hvor mange lysintervaller som er passert, og beregner hvor mange lys som skal lyse
int sjekkLysLevel() {
    int teller = 0;
    for (int i = 0; i < sizeof(lysLevelArray) / sizeof(lysLevelArray[0]); i++) {
        if (totalHoyde > lysLevelArray[i]) {
            teller ++;
        }
    }
    return teller;
}

// farger diodene i det gitte intervallet, i den oppgitte fargen
void tegnDiodeInterval(int fra, int til, int farge[]) {
    for (int i = fra; i < til; i++) {
        tegnDiode(i, blaa);
    }
}

// tegner dioden med den gitte indeksen, i den gitte fargen
void tegnDiode(int indeks, int farge[]) {
    strip.setPixelColor(indeks, farge[0], farge[1], farge[2]);
    strip.show();
}

// nullstiller alle lysene i den gitte intervallet
void nullstillLys(int fra, int til) {
    for (int i = fra; i < til; i++) {
        strip.setPixelColor(i, 0, 0, 0);
        strip.show();
    }
}

// tegner en lyspuls som beveger seg opp og ned i det gitte intervallet
void lysPuls(int farge[], int fra, int til, int tempo) {
    for (int i = fra; i < til; i++) {
        nullstillLys(fra, til);
        tegnDiode(i, farge);
        delay(tempo);
    }

    for (int i = til - 2; i >= fra; i--) {
        nullstillLys(fra, til);
        tegnDiode(i, farge);
        delay(tempo);
    }
    nullstillLys(fra, til);
}


// motoroperasjoner
void oppdaterMotor() {
    if (sjekkPassering()) {
        aapneLuke();
        // gir brukeren 45 sekunder på å utføre ønsket handling med luken
        delay(45000);
        lukkLuke();
    }
}

// sjekker om den pågående iterasjonen av programmet har passert en av "basecampene"
bool sjekkPassering() {
    bool passert = false;
    for (int i = 0; i < sizeof(basecampArray) / sizeof(basecampArray[0]); i++) {
        int interval = basecampArray[i];
        if (totalHoyde >= interval && (totalHoyde - gjeldendeHoyde) < interval) {
            passert = true;
            Serial.println("Åpen");
        }
    }
    return passert;
}

// åpner døren
void aapneLuke() {
    myServo.write(90);
}

// lukker døren
void lukkLuke() {
    myServo.write(0);
}
