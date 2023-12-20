/*
filename      : EGODS_actuator_box.ino
Written By    : Chhay Irseng
Created Date  : 16/12/2023
Description   : Let a microncontroller (Arduino Uno R4 Minima) constantly sends 
                pressure data readings wireless (using LoRA technology) to another microcontroller 
                and receives control signals from the other micrcontroller to actuate three separate valves.
*/

// Import libraries
#include <SPI.h>
#include <LoRa.h>

// Define microcontroller pins
  /* Switches */
const int FILL_LED = 8;
const int DUMP_LED = 5;
const int CHECK_LED = 4;

  /* LoRA comms */
const int LORA_G0 = 3;
const int LORA_SCK = 13;
const int LORA_MISO = 12;
const int LORA_MOSI = 11;
const int LORA_CS = 10;
const int LORA_RST = 2;

  /* Ultrasonic sensor */
const int ULTRA_TRIG = 6;
const int ULTRA_ECHO = 7;

// Declare and initialise constants
const long LORA_FREQ = 915E6;     // LoRA's transceiver module operaating frequency

// Declare and initialise global variables
  /* Local and destination addresses */
byte local_address = 0xBB;
byte destination_address = 0xCC;

  /* Code to be received */
byte code = 0;
byte received_code = 0;       // to counter forbidden states
String eight_bit_code = "";
/* code encode/decode:
  (0) 0b00000000  : fill valve OFF, dump valve OFF, check valve OFF (off state)
  (1) 0b00000001  : fill valve OFF, dump valve OFF, check valve ON  (don't care state)
  (2) 0b00000010  : fill valve OFF, dump valve ON,  check valve OFF (dumping state)
  (3) 0b00000011  : fill valve OFF, dump valve ON,  check valve ON  (forbidden state - bad practice?)
  (4) 0b00000100  : fill valve ON,  dump valve OFF, check valve OFF (fill-ready state)
  (5) 0b00000101  : fill valve ON,  dump valve OFF, check valve ON  (filling state)
  (6) 0b00000110  : fill valve ON,  dump valve ON,  check valve OFF (forbidden state - draining the supply tank)
  (7) 0b00000111  : fill valve ON,  dump valve ON,  check valve ON  (forbidden state - draining the supply tank + bad practice)
*/

  /* Data to be sent */
String data = "";
float distance = 0;

  /* Set up and loop function success checker */
byte set_up_success = 0;
byte loop_fail_once = 0;

  /* Time intervals adn last sent */
long last_send_time = 0;        // last send time
int interval = 1000;          // interval between sends

void setup() {
  // Set up serial baud rate
  Serial.begin(9600); while (!Serial); // wait for Serial to set up properly

  // Set up valve actuator pins
  pinMode(FILL_LED, OUTPUT);
  pinMode(DUMP_LED, OUTPUT);
  pinMode(CHECK_LED, OUTPUT);

  // Set up the ultrasonic pins
  pinMode(ULTRA_TRIG, OUTPUT);
  pinMode(ULTRA_ECHO, INPUT);

  // Set up LoRA module pins
  LoRa.setPins(LORA_CS, LORA_RST, LORA_G0);

  // Set up LoRA operating frequency
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("SET_UP_ERROR    : LORA module failed to set up operating frequency.");
    return;
  }

  // Set up call-back function for receiving packets
  LoRa.onReceive(onReceive);
  LoRa.receive();

  // Finish setting up everything
  set_up_success = 1;
}

void loop() {
  // Verify that the set up function ran properly and the loop has not failed
  if (loop_fail_once) {
    Serial.println("loop fail once...");
    return;
  }
  if (!set_up_success) {
    Serial.println("LOOP_ERROR      : the set_up function failed to complete properly, loop function cannot proceed.");
    loop_fail_once = 1;
    return;
  }
  // If everything ran properly, proceed to run the loop...
  if (millis() - last_send_time > interval) {
    // Get the data by reading from the ultrasonic sensor
    ultrasonic_reading();
    data = String(distance);
    Serial.println("data: " + data);
    // Send the data to the command box
    sendMessage(data);
    // Update the last send time and interval
    last_send_time = millis();
    interval = random(1000) + 500;
    // Put the module back in receiving mode
    LoRa.receive();
    LED_actuation(code);
    eight_bit_code_format(code);
    Serial.println("code: " + eight_bit_code);
  }
}

// Code inspired by: https://howtomechatronics.com/tutorials/arduino/ultrasonic-sensor-hc-sr04/
void ultrasonic_reading() { 
  // Clears the trigPin
  digitalWrite(ULTRA_TRIG, 0);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(ULTRA_TRIG, 1);
  delayMicroseconds(10);
  digitalWrite(ULTRA_TRIG, 0);
  // Reads the echoPin, returns the sound wave travel time in microseconds
  long duration = pulseIn(ULTRA_ECHO, 1);
  // Calculating the distance
  distance = duration * 0.034 / 2;
}

void sendMessage(String data) {
  LoRa.beginPacket();
  LoRa.write(destination_address);
  LoRa.write(local_address);
  LoRa.print(data);
  LoRa.endPacket();
}

void onReceive(int packet_size) {
  if (packet_size) {
    byte recipient_address = LoRa.read();
    byte sender_address = LoRa.read();
    if ((recipient_address != local_address) || (sender_address != destination_address)) {
      Serial.println("ON RECEIVE ERROR  : packet was not meant to be received.");
      return;
    }
    code = LoRa.read();
  }
}

void LED_actuation(byte code) {
  if ((code==1) || (code==3) || (code==6) || (code==7)) {
    Serial.println("DONT_CARE/FORBIDDEN : received message belongs to a don't care or forbidden state.");
  } else { received_code = code; };
  byte received_fill_sw_state = (received_code >> 2) & 0b1; // Serial.println("fill_sw_state: " + String(received_fill_sw_state));
  byte received_dump_sw_state = (received_code >> 1) & 0b1; // Serial.println("dump_sw_state: " + String(received_dump_sw_state));
  byte received_check_sw_state = received_code & 0b1; // Serial.println("check_sw_state: " + String(received_check_sw_state));
  digitalWrite(FILL_LED, received_fill_sw_state);
  digitalWrite(DUMP_LED, received_dump_sw_state);
  digitalWrite(CHECK_LED, received_check_sw_state);
}

void eight_bit_code_format(byte code) {
  eight_bit_code = "";
  for (int i=7; i >-1; i--) {
    eight_bit_code += bitRead(code,i);
  }
}

/* ----------------------------------------------------------------------------------- */
// // empty sketch
// void setup() {

// }

// void loop() {

// }

