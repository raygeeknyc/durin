/*
 By Raymond Blum <raymond@insanegiantrobots.com>
 Protected by Creative Commons license 0.1

 For more information, please see
 <http://creativecommons.org/publicdomain/zero/1.0/>
*/

// A string that the device sends when it starts up, useful to verify successful flashes
#define VERSION_ID "V8"

// The minumum change in sensor readings to react to
#define LIGHT_CHANGE_THRESHOLD 70
#define PIR_THRESHOLD 255

// Wait at least this long between reported events
#define PIR_MINIMUM_DELAY_MS 20000
#define LIGHT_MINIMUM_DELAY_MS 30000

// Report in every 30 minutes
#define PULSE_MAXIMUM_REPORT_INTERVAL_MS 1800000

// text commands that can be sent via cloud remote procedure calls
#define HALT_MESSAGE "#halt"
#define GO_MESSAGE "#go"
#define QUIET_MESSAGE "#quiet"
#define ALERTS_MESSAGE "#alert"

// The range of the hand servo's motion
#define SERVO_HALT_POSITION 20
#define SERVO_GO_POSITION 120

// Hardware configuration
const int HAND_PIN = A4;
const int LED_PIN = D7;
const int HALT_LED_PIN = D6;
const int GO_LED_PIN = D5;
const int GREEN_BUTTON_PIN = D3;
const int RED_BUTTON_PIN = D4;
const int PIR_PIN = A0;
const int CDS_PIN = A1;
const int BUZZER_PIN = D1;

Servo hand;

int redPressed, greenPressed, lightLevel, prevLightLevel, silence;

// The timestamps of the most recent events
unsigned long int motionAt, lightAt, pulseAt;

int getLightLevel() {
    return analogRead(CDS_PIN);
}

void setup()
{
    hand.attach(HAND_PIN);
    pinMode(LED_PIN, OUTPUT);
    pinMode(GO_LED_PIN, OUTPUT);
    pinMode(HALT_LED_PIN, OUTPUT);
    pinMode(GREEN_BUTTON_PIN, INPUT_PULLUP);
    pinMode(RED_BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(CDS_PIN, INPUT);
    pinMode(PIR_PIN, INPUT);

    digitalWrite(LED_PIN, LOW);
    digitalWrite(HALT_LED_PIN, LOW);
    digitalWrite(GO_LED_PIN, LOW);

    redPressed = false;
    greenPressed = false;
    motionAt = 0;
    lightLevel = prevLightLevel = 0;
    tone(BUZZER_PIN, 1000);
    digitalWrite(HALT_LED_PIN, HIGH);
    digitalWrite(GO_LED_PIN, HIGH);
    delay(1000);
    digitalWrite(HALT_LED_PIN, LOW);
    digitalWrite(GO_LED_PIN, LOW);
    noTone(BUZZER_PIN);
    lightAt = pulseAt = motionAt = millis();
    silence = false;

    Particle.publish("device_ready", VERSION_ID);
    Particle.subscribe("reply", getSms);
}

int getPir() {
    int pir = 0;
    for (int i=0; i<10; i++) {
        pir = max(pir, analogRead(PIR_PIN));
    }
    return pir;
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
    }
}

void disableAlerts() {
    digitalWrite(HALT_LED_PIN, LOW);
    digitalWrite(GO_LED_PIN, LOW);
    noTone(BUZZER_PIN);
    hand.write(SERVO_GO_POSITION);
    silence = true;
}

void enableAlerts() {
    silence = false;
}

void signalHalt() {
    if (!silence) {
        digitalWrite(HALT_LED_PIN, HIGH);
        digitalWrite(GO_LED_PIN, LOW);
        tone(BUZZER_PIN, 750, 500);
        hand.write(SERVO_HALT_POSITION);
    }
}

void signalGo() {
    if (!silence) {
        digitalWrite(HALT_LED_PIN, LOW);
        digitalWrite(GO_LED_PIN, HIGH);
        tone(BUZZER_PIN, 1000, 200);
        hand.write(SERVO_GO_POSITION);
    }
}

void loop() {
    unsigned long int now = millis();
    int pir;
    
    if (abs((lightLevel = getLightLevel()) - prevLightLevel) > LIGHT_CHANGE_THRESHOLD) {
        if (now > (lightAt + LIGHT_MINIMUM_DELAY_MS)) {
            lightAt = now;
            Particle.publish("light_change", String(lightLevel));
            prevLightLevel = lightLevel;
        }
    }
    if (now > (pulseAt + PULSE_MAXIMUM_REPORT_INTERVAL_MS)) {
            pulseAt = now;
            Particle.publish("pulse", "light: "+String(lightLevel)+" silenced: "+((silence)?"true":"false"));
    }
    if ((pir = getPir()) > PIR_THRESHOLD) {
        if (now > (motionAt + PIR_MINIMUM_DELAY_MS)) {
            motionAt = now;
            Particle.publish("motion_detected", String(pir));
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
