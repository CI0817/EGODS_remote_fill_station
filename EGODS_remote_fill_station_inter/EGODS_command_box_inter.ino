/*
filename      : EGODS_command_box.ino
Written By    : Chhay Irseng
Created Date  : 13/12/2023
Description   : Let a microncontroller (Arduino Uno R4 Minima) constantly receives 
                pressure data readings wireless (using LoRA technology) from another microcontroller 
                and sends control signals to the other micrcontroller to actuate three separate valves.
*/

// Import libraries
#include <SPI.h>
#include <LoRa.h>

// Define microcontroller pins
  /* Switches */
const int FILL_SW = 4;
const int DUMP_SW = 5;
const int CHECK_SW = 6;

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
  /* Local and destination addresses */
byte local_address = 0xCC;
byte destination_address = 0xBB;

  /* Code to be sent */
byte opcode = 0;
String eight_bit_opcode = "";
/* opcode:
  (0) 0b00000000  : fill valve OFF, dump valve OFF, check valve OFF (off state)
  (1) 0b00000001  : fill valve OFF, dump valve OFF, check valve ON  (don't care state)
  (2) 0b00000010  : fill valve OFF, dump valve ON,  check valve OFF (dumping state)
  (3) 0b00000011  : fill valve OFF, dump valve ON,  check valve ON  (forbidden state - bad practice?)
  (4) 0b00000100  : fill valve ON,  dump valve OFF, check valve OFF (fill-ready state)
  (5) 0b00000101  : fill valve ON,  dump valve OFF, check valve ON  (filling state)
  (6) 0b00000110  : fill valve ON,  dump valve ON,  check valve OFF (forbidden state - draining the supply tank)
  (7) 0b00000111  : fill valve ON,  dump valve ON,  check valve ON  (forbidden state - draining the supply tank + bad practice)
*/

  /* Pressure transducer data to be received */
String fill_pres_str = "";
String dump_pres_str = "";
String check_pres_str = "";

  /* Set up and loop function success checker */
byte set_up_success = 0;
byte loop_fail_once = 0;

  /* Time intervals and last sent */
long last_send_time = 0;        // last send time
int interval = 1000;          // interval between sends

void setup() {
  // Set up Serial baud rate
  Serial.begin(9600); while (!Serial); // wait for Serial to set up properly

  // Set up switch pins
  pinMode(FILL_SW, INPUT);
  pinMode(DUMP_SW, INPUT);
  pinMode(CHECK_SW, INPUT);

  // Set up LoRA module pins
  LoRa.setPins(LORA_CS, LORA_RST, LORA_G0);

  // Set up LoRA operating frequency
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("SET_UP_ERROR  : LORA module failed to set up operating frequency.");
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
    return;
  }
  if (!set_up_success) {
    Serial.println("LOOP_ERROR    : the set_up function failed to complete properly, loop function cannot proceed.");
    loop_fail_once = 1;
    return;
  }
  // If everything ran properly, proceed to run the loop...
  if (millis() - last_send_time > interval) {
    Serial.println("-------- TRANSMIT --------");
    // Get the code by reading the switches
    opcode = opcode_generator();
    eight_bit_code_format(opcode);
    Serial.println("code: " + eight_bit_opcode);
    // Send the code to the actuator box
    sendMessage(opcode);
    // Update the last send time and interval
    last_send_time = millis();
    interval = random(1000) + 500;
    Serial.println("-------- RECEIVE ---------");
    // Put the module back in receiving mode
    LoRa.receive();
    // Print the data onto the Serial monitor
    Serial.println("fill_pres: " + fill_pres_str);
    Serial.println("dump_pres: " + dump_pres_str);
    Serial.println("check_pres: " + check_pres_str);
    Serial.println("--------------------------------------------------");
  }
}

/* This function is used to read the input signals of the three switches, 
    shift their bits accordingly, and combine together to form one of the operating states */
byte opcode_generator() {
  // Read the input signals from the three switches and shift their bits accordingly
  byte fill_sw_state = digitalRead(FILL_SW); fill_sw_state = fill_sw_state << 2;
  byte dump_sw_state = digitalRead(DUMP_SW); dump_sw_state = dump_sw_state << 1;
  byte check_sw_state = digitalRead(CHECK_SW);
  // Combine all the switches input signal to form the operating state
  return (fill_sw_state + dump_sw_state + check_sw_state);
}

/* This function is used to send the operating state as a code to the actuator box */
void sendMessage (byte code) {
  // Start transmitting the packet
  LoRa.beginPacket();
  // Include the destination and local address as the headers
  LoRa.write(destination_address);
  LoRa.write(local_address);
  // Transmit the message
  LoRa.write(code);
  // End the transmission
  LoRa.endPacket();
}

/* This functin is used to receive data from the actuator box and store them appropriately */
void onReceive(int packet_size) {
  // Check if there is any received packet
  if (packet_size) {
    // Read the destination and local address and compare them to make sure it is intended for us
    byte recipient_address = LoRa.read();
    byte sender_address = LoRa.read();
    if ((recipient_address != local_address) || (sender_address != destination_address)) {
      Serial.println("ON RECEIVE ERROR  : packet was not meant to be received.");
      return;
    }
    char curr_char = "";    // Hold the current character read from the packet
    byte separator = 0;     // Indicate which data is going to be read (e.g., the fill pressure or the dump pressure or the check pressure)
    fill_pres_str = ""; dump_pres_str = ""; check_pres_str = "";    // Clear the string variable to be written into
    // Check if there is still contents in the packet received
    while (LoRa.available()) {
      // Read the current character from the packet
      curr_char = (char)LoRa.read();
      // Conditional statements to indicate which data is to be read
      if (separator == 0) {
        if (curr_char == '|') {
          separator = 1;
        } else {
          fill_pres_str += curr_char;
        }
      } else if (separator == 1) {
        if (curr_char == '|') {
          separator = 2;
        } else {
          dump_pres_str += curr_char;
        }
      } else if (separator == 2) {
        check_pres_str += curr_char;
      }
    }
  }
}

/* This function is used to create an eight-bit format for printing onto the serial monitor. 
    It is for displaying the operating state neatly */
void eight_bit_code_format(byte code) {
  // Clear the variable to be written into
  eight_bit_opcode = "";
  // Add one character at a time into string variable using a "for" loop
  for (int i=7; i >-1; i--) {
    eight_bit_opcode += bitRead(code,i);
  }
}

/* ------------------------------------------------------------------------------------ */
// // empty sketch
// void setup() {
//   // put your setup code here, to run once:

// }

// void loop() {
//   // put your main code here, to run repeatedly:

// }
