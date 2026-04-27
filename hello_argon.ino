void setup() {
    Serial.begin(9600);
    pinMode(D7, OUTPUT);
}

void loop() {
    Serial.println("Hello World from Argon");
    digitalWrite(D7, HIGH);
    delay(500);
    digitalWrite(D7, LOW);
    delay(500);
}
