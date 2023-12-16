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
const int FILL_LED = 4;
const int DUMP_LED = 7;
const int CHECK_LED = 8;

  /* LoRA comms */
const int LORA_G0 = 3;
const int LORA_SCK = 13;
const int LORA_MISO = 12;
const int LORA_MOSI = 11;
const int LORA_CS = 10;
const int LORA_RST = 2;

// Declare and initialise constants
const long LORA_FREQ = 915E6;     // LoRA's transceiver module operaating frequency

// Declare and initialise global variables
byte set_up_success = 0;
byte loop_fail_once = 0;
byte encoded_message = 0;
byte received_message = 0;
String eight_bit_encoded_message = "";
/* message encode/decode:
  (0) 0b00000000  : fill valve OFF, dump valve OFF, check valve OFF (off state)
  (1) 0b00000001  : fill valve OFF, dump valve OFF, check valve ON  (don't care state)
  (2) 0b00000010  : fill valve OFF, dump valve ON,  check valve OFF (dumping state)
  (3) 0b00000011  : fill valve OFF, dump valve ON,  check valve ON  (forbidden state - bad practice?)
  (4) 0b00000100  : fill valve ON,  dump valve OFF, check valve OFF (fill-ready state)
  (5) 0b00000101  : fill valve ON,  dump valve OFF, check valve ON  (filling state)
  (6) 0b00000110  : fill valve ON,  dump valve ON,  check valve OFF (forbidden state - draining the supply tank)
  (7) 0b00000111  : fill valve ON,  dump valve ON,  check valve ON  (forbidden state - draining the supply tank + bad practice)
*/
byte fill_sw_state = 0; byte dump_sw_state = 0; byte check_sw_state = 0;

void setup() {
  // Set up serial baud rate
  Serial.begin(9600); while (!Serial); // wait for Serial to set up properly

  // Set up valve actuator pins
  pinMode(FILL_LED, OUTPUT);
  pinMode(DUMP_LED, OUTPUT);
  pinMode(CHECK_LED, OUTPUT);

  // Set up LoRA module pins
  LoRa.setPins(LORA_CS, LORA_RST, LORA_G0);

  // Set up LoRA operating frequency
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("SET_UP_ERROR    : LORA module failed to set up operating frequency.");
    return;
  }

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
  int packet_size = LoRa.parsePacket();
  // Serial.println("Packet size: " + String(packet_size));
  if (packet_size) {
    encoded_message = LoRa.read();
    eight_bit_message_format(encoded_message);
    Serial.println("Received command: " + eight_bit_encoded_message);
    LED_actuation(encoded_message);
    Serial.println("Current LED state: " + state_determination(encoded_message));
    Serial.println("---------------------------------------------");
  }
  // delay(500);
}

void LED_actuation(byte message) {
  if (message==1 | message==3 | message==6 | message==7) {
    Serial.println("DONT_CARE/FORBIDDEN : received message belongs to a don't care or forbidden state.");
  } else { received_message = message; };
  byte received_fill_sw_state = (received_message >> 2) & 0b1; Serial.println("\treceived_fill_sw_state: " + String(received_fill_sw_state));
  byte received_dump_sw_state = (received_message >> 1) & 0b1; Serial.println("\treceived_dump_sw_state: " + String(received_dump_sw_state));
  byte received_check_sw_state = received_message & 0b1; Serial.println("\treceived_check_sw_state: " + String(received_check_sw_state));
  eight_bit_message_format(received_message);
  Serial.println("received message: " + eight_bit_encoded_message);
  digitalWrite(FILL_LED, received_fill_sw_state);
  digitalWrite(DUMP_LED, received_dump_sw_state);
  digitalWrite(CHECK_LED, received_check_sw_state);
}

String state_determination(byte state_encode) {
  if (state_encode==0) {
    return "off";
  } else if (state_encode==1) {
    return "don't care";
  } else if (state_encode==2) {
    return "dumping";
  } else if (state_encode==4) {
    return "fill-ready";
  } else if (state_encode==5) {
    return "filling";
  } else if ((state_encode==3) | (state_encode==6) | (state_encode==7)) {
    return "forbidden";
  }
  return "unknown";
}

void eight_bit_message_format(byte message) {
  eight_bit_encoded_message = "";
  for (int i=7; i >-1; i--) {
    eight_bit_encoded_message += bitRead(message,i);
  }
}

/* ----------------------------------------------------------------------------------- */
// // empty sketch
// void setup() {

// }

// void loop() {

// }

