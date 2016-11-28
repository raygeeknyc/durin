/*
 By Raymond Blum <raymond@insanegiantrobots.com>
 Protected by Creative Commons license 0.1

 For more information, please see
 <http://creativecommons.org/publicdomain/zero/1.0/>
*/

/****
 * Make sure that your PIR sensor's delay and sensitivity settings are correct
 * and that you've set any jumper to "H" - retriggering mode.
 * 
 * Determine your servo positions first with a calibration sketch and set the appropriate values
 * below in SERVO_HALT_POSITION and SERVO_HALT_POSITION
 * */

// A string that the device sends when it starts up, useful to verify successful flashes
#define VERSION_ID "V86"
#define APPLICATION_NAME "durin"

// The range of the hand servo's motion  -  SG90 microservos
#define SERVO_HALT_POSITION 160
#define SERVO_GO_POSITION 70

// The range of the hand servo's motion  - Solar microservos
//SERVO_HALT_POSITION 20
// SERVO_GO_POSITION 100

// Step the hand servo this much at a time to avoid jumping too quickly
#define SERVO_STEP 2
const int _SERVO_STEP_DOWN = (SERVO_HALT_POSITION > SERVO_GO_POSITION)?(-1*SERVO_STEP):(SERVO_STEP);
const int _SERVO_STEP_UP = -1*_SERVO_STEP_DOWN;


// The minimum change in sensor readings to react to
#define LIGHT_CHANGE_THRESHOLD 200

// Wait at least this long between reported events

#define PIR_MINIMUM_DELAY_SECS 60
#define LIGHT_MINIMUM_DELAY_SECS 30
const long PIR_MINIMUM_DELAY_MS = PIR_MINIMUM_DELAY_SECS * 1000;
const long LIGHT_MINIMUM_DELAY_MS = LIGHT_MINIMUM_DELAY_SECS * 1000;

// Wait this long after setting LEDs on or off before recording new light levels
#define SET_LIGHT_PROPAGATE_DELAY_MS 10

// After this long without motion, reset the alert indicators
#define MOTION_ALERT_EXPIRATION_MINS 2
const long MOTION_ALERT_EXPIRATION_MS = MOTION_ALERT_EXPIRATION_MINS * 60 * 1000;

// Report in every 30 minutes
#define PULSE_MAXIMUM_REPORT_INTERVAL_MINS 30
const long PULSE_MAXIMUM_REPORT_INTERVAL_MS = PULSE_MAXIMUM_REPORT_INTERVAL_MINS * 60 * 1000;

// Ignore motion and light change events for this long after the LEDs states are switched
#define LIGHT_SET_BLACKOUT_PERIOD_SECS 3
const long LIGHT_SET_BLACKOUT_PERIOD_MS = LIGHT_SET_BLACKOUT_PERIOD_SECS * 1000;

// text commands that can be sent via cloud remote procedure calls
#define DEBUG_MESSAGE "#debug"
#define HELP_MESSAGE "#help"
#define HALT_MESSAGE "#halt"
#define GO_MESSAGE "#go"
#define QUIET_MESSAGE "#quiet"
#define ALERTS_MESSAGE "#alert"
#define STATUS_MESSAGE "#status?"

// Wait this long between servo steps for the motor to catch up
#define SERVO_STEP_DELAY_MS 15

// Hardware configuration
const int HAND_PIN = A5;  // Must be a PWM capable pin
const int LED_PIN = D7;   // The built-in LED
const int HALT_LED_PIN = D6;
const int GO_LED_PIN = D5;
const int PIR_PIN = D4;
const int BUZZER_PIN = D3;  // Cannot share a timer with the HAND_PIN
const int CDS_PIN = A1;

const int RED_BUTTON_PIN = D2;
const int GREEN_BUTTON_PIN = D1;

Servo hand;
int handPos;

int redPressed, greenPressed, lightLevel, prevLightLevel, silence, pirWasOn, isAlerting;

// The timestamps of the most recent events
unsigned long int motionAt, lightAt, pulseAt, pirAt, now, lightSetAt;

// Event counts per pulse
int motionCount, lightChangeCount;

// Return the median of 3 sensor readings
int getLightLevel() {
    int min = 32000, max = -1, total = 0;
    int reading;
    for (int i=0; i<3; i++) {
        reading = analogRead(CDS_PIN);
        total += reading;
        if (reading > max) {
            max = reading;
        }
        if (reading < min) {
            min = reading;
        }
    }
    return (total - min) - max;
}

void setup()
{
    Particle.subscribe("reply", getSms);
    handPos = SERVO_HALT_POSITION;
    hand.attach(HAND_PIN);
    handDown();
    pinMode(LED_PIN, OUTPUT);
    pinMode(GO_LED_PIN, OUTPUT);
    pinMode(HALT_LED_PIN, OUTPUT);
    pinMode(GREEN_BUTTON_PIN, INPUT_PULLUP);
    pinMode(RED_BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(CDS_PIN, INPUT);
    pinMode(PIR_PIN, INPUT_PULLUP);

    digitalWrite(LED_PIN, LOW);
    digitalWrite(HALT_LED_PIN, LOW);
    digitalWrite(GO_LED_PIN, LOW);

    redPressed = false;
    greenPressed = false;
    lightLevel = prevLightLevel = getLightLevel();
    tone(BUZZER_PIN, 1000);
    digitalWrite(HALT_LED_PIN, HIGH);
    digitalWrite(GO_LED_PIN, HIGH);
    delay(1000);
    digitalWrite(HALT_LED_PIN, LOW);
    digitalWrite(GO_LED_PIN, LOW);
    noTone(BUZZER_PIN);
    lightAt = pulseAt = motionAt = millis();
    silence = false;
    lightChangeCount = 0;
    motionCount = 0;
    pirWasOn = false;
    lightSetAt = 0;

    Particle.publish("device_ready",  String(APPLICATION_NAME)+VERSION_ID);
}

/** Only if we have just toggled on from false, return true
 * */
int getPir() {
    unsigned long now = millis();
    int pir = digitalRead(PIR_PIN);
    if (now < (lightSetAt + LIGHT_SET_BLACKOUT_PERIOD_MS)) {
        pirWasOn = pir;
        return false;
    }
    if (!pir) {
        pirWasOn = false;
        return false;
    }
    if (pirWasOn) {
        return false;
    }
    pirWasOn = true;
    return true;
}

void getSms(const char *event, const char *data) {
    if (!strcmp(data, HALT_MESSAGE)) {
        signalHalt();
    } else if (!strcmp(data, GO_MESSAGE)) {
        signalGo();
    } else if (!strcmp(data, QUIET_MESSAGE)) {
        disableAlerts();
    } else if (!strcmp(data, ALERTS_MESSAGE)) {
        enableAlerts();
    } else if (!strcmp(data, STATUS_MESSAGE)) {
        sendPulse(true);
    } else if (!strcmp(data, DEBUG_MESSAGE)) {
        sendDebug();
    } else if (!strcmp(data, HELP_MESSAGE)) {
        sendHelp();
    }
}

// Gradually move the hand to the down "go" position
void handDown() {
    if (abs(handPos - SERVO_GO_POSITION) <= SERVO_STEP) {
        return;
    }
    for ( handPos = SERVO_HALT_POSITION; ((handPos >= min(SERVO_GO_POSITION, SERVO_HALT_POSITION)) && (handPos <= max(SERVO_GO_POSITION, SERVO_HALT_POSITION))); handPos += _SERVO_STEP_DOWN) {
        hand.write(handPos);
        delay(SERVO_STEP_DELAY_MS);
    }
}

// Gradually move the hand to the up "halt" position
void handUp() {
    if (abs(handPos - SERVO_HALT_POSITION) <= SERVO_STEP) {
        return;
    }
    for (handPos = SERVO_GO_POSITION; ((handPos >= min(SERVO_GO_POSITION, SERVO_HALT_POSITION)) && (handPos <= max(SERVO_GO_POSITION, SERVO_HALT_POSITION))); handPos += _SERVO_STEP_UP) {
        hand.write(handPos);
        delay(SERVO_STEP_DELAY_MS);
    }
}

void recordSetLight() {
    delay(SET_LIGHT_PROPAGATE_DELAY_MS);  // Wait for LED state changes to take effect
    lightLevel = prevLightLevel = getLightLevel();  // We just changed the light levels so update this to prevent a false positive
    lightSetAt = millis();
}

void showQuiet(bool requested) {
    Particle.publish("set_quiet", (requested)?"requested":"auto");
    digitalWrite(HALT_LED_PIN, LOW);
    digitalWrite(GO_LED_PIN, LOW);
    recordSetLight();
    noTone(BUZZER_PIN);
    handDown();
    isAlerting = false;    
}

void disableAlerts() {
    showQuiet(true);
    silence = true;
}

void enableAlerts() {
    silence = false;
}

void signalHalt() {
    if (!silence) {
        digitalWrite(HALT_LED_PIN, HIGH);
        digitalWrite(GO_LED_PIN, LOW);
        recordSetLight();
        handUp();
        tone(BUZZER_PIN, 100, 500);
        isAlerting = true;
        motionAt = millis();

    }
}

void signalGo() {
    if (!silence) {
        digitalWrite(HALT_LED_PIN, LOW);
        digitalWrite(GO_LED_PIN, HIGH);
        recordSetLight();
        handDown();
        tone(BUZZER_PIN, 400, 200);
        isAlerting = false;
    }
}

void sendPulse(bool sendMessage) {
    int mins = (millis() - pulseAt) / 1000 / 60;
    Particle.publish((sendMessage?"pulse_notify":"pulse"), "{appl:"+String(APPLICATION_NAME)+",version:"+VERSION_ID+String(",mins:")+String(mins)+",light:"+String(lightLevel)+",silenced:"+String((silence)?"true":"false")
        +",alerting:"+(isAlerting?"true":"false")+",light_changes:"+String(lightChangeCount)+",motions:"+String(motionCount)+",mins_since_motion:"+String((millis()-motionAt)/1000/60)+"}");
}

void sendDebug() {
    unsigned long since = (now - motionAt);
    unsigned long nextAt = motionAt + MOTION_ALERT_EXPIRATION_MS;
    Particle.publish("debug", String("{alerting:")+(isAlerting?"true":"false")+",interval:"+String(MOTION_ALERT_EXPIRATION_MS)+",now:"+String(now)+",since:"+String(since)+",next:"+String(nextAt)+"}");
}

void sendHelp() {
    Particle.publish("usage", "{#go,#halt,#quiet,#alert,#status?,#help,#debug}");
}

void loop() {
    now = millis();

    if ((now > (motionAt + MOTION_ALERT_EXPIRATION_MS)) && isAlerting) {
        showQuiet(false);
    }
    
    if ( (now > (lightSetAt + LIGHT_SET_BLACKOUT_PERIOD_MS)) && (abs((lightLevel = getLightLevel()) - prevLightLevel) > LIGHT_CHANGE_THRESHOLD) ) {
        if (now > (lightAt + LIGHT_MINIMUM_DELAY_MS)) {
            lightAt = now;
            Particle.publish("light_change", String(lightLevel));
            lightChangeCount += 1;
            prevLightLevel = lightLevel;
        }
    }
    if (now > (pulseAt + PULSE_MAXIMUM_REPORT_INTERVAL_MS)) {
            sendPulse(false);
            pulseAt = now;
            lightChangeCount = 0;
            motionCount = 0;
    }
    if (getPir() == HIGH) {
        if (now > (motionAt + PIR_MINIMUM_DELAY_MS)) {
            motionAt = now;
            Particle.publish("motion_detected", String(digitalRead(PIR_PIN)));
            motionCount += 1;
            signalHalt();
        }
      }
    if (digitalRead(GREEN_BUTTON_PIN) == LOW) {
        if (!greenPressed) {
            greenPressed = true;
            Particle.publish("user_button", "green");
        }
        digitalWrite(LED_PIN, HIGH);
    } else {
        greenPressed = false;
    }
    if (digitalRead(RED_BUTTON_PIN) == LOW) {
        if (!redPressed) {
            redPressed = true;
            Particle.publish("user_button", "red");
        }
        digitalWrite(LED_PIN, HIGH);
    } else {
        redPressed = false;
    }
    if (!redPressed && !greenPressed) {
        digitalWrite(LED_PIN, LOW);
    }
}
