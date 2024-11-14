#define PUSH_BUTTON_PIN_1 D1
#define PUSH_BUTTON_PIN_2 D2
#define PUSH_BUTTON_PIN_3 D3
#define PUSH_BUTTON_PIN_4 D4
#define PUSH_BUTTON_PIN_7 D7
#define PUSH_BUTTON_PIN_8 D8

void setup() {
  pinMode(PUSH_BUTTON_PIN_1, OUTPUT);
  pinMode(PUSH_BUTTON_PIN_2, INPUT_PULLUP);
  pinMode(PUSH_BUTTON_PIN_3, INPUT_PULLUP);
  pinMode(PUSH_BUTTON_PIN_4, INPUT_PULLUP);
  pinMode(PUSH_BUTTON_PIN_7, OUTPUT);
  pinMode(PUSH_BUTTON_PIN_8, OUTPUT);
  Serial.begin(115200);
}

void loop() {
  bool d1ButtonState = digitalRead(PUSH_BUTTON_PIN_1);
  bool d2ButtonState = digitalRead(PUSH_BUTTON_PIN_2);
  bool d3ButtonState = digitalRead(PUSH_BUTTON_PIN_3);
  bool d4ButtonState = digitalRead(PUSH_BUTTON_PIN_4);
  bool d7ButtonState = digitalRead(PUSH_BUTTON_PIN_7);
  bool d8ButtonState = digitalRead(PUSH_BUTTON_PIN_8);
digitalWrite(PUSH_BUTTON_PIN_8, HIGH);
digitalWrite(PUSH_BUTTON_PIN_7, HIGH);
  if (d1ButtonState == LOW) {
    Serial.println("Button 1 pressed. Manual lights on.");
  } else {
    Serial.println("Button 1 released. Manual lights off.");
  }

  if (d2ButtonState == LOW) {
    Serial.println("Button 2 pressed. Manual lights on.");
//    digitalWrite(PUSH_BUTTON_PIN_8, HIGH);
//    digitalWrite(PUSH_BUTTON_PIN_7, HIGH);
  } else {
    Serial.println("Button 2 released. Manual lights off.");
//    digitalWrite(PUSH_BUTTON_PIN_8, LOW);
//    digitalWrite(PUSH_BUTTON_PIN_7, HIGH);
  }

  if (d3ButtonState == LOW) {
    Serial.println("Button 3 pressed. Manual lights on.");
  } else {
    Serial.println("Button 3 released. Manual lights off.");
  }

  if (d4ButtonState == LOW) {
    Serial.println("Button 4 pressed. Manual lights on.");
  } else {
    Serial.println("Button 4 released. Manual lights off.");
  }

  Serial.print("d8ButtonState: ");
  Serial.println(d8ButtonState);
  

  delay(2000);
}
