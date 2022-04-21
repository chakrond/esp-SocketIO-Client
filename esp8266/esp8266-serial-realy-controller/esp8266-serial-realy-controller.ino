
// 1st Relay
// Close relay
byte SendData_Open_1[] = {0xA0, 0x01, 0x01, 0xA2}; // A00101A2
byte SendData_Close_1[] = {0xA0, 0x01, 0x00, 0xA1}; // A00100A1

// 2nd Relay
// Close relay
byte SendData_Open_2[] = {0xA0, 0x02, 0x01, 0xA3}; // A00201A3
byte SendData_Close_2[] = {0xA0, 0x02, 0x00, 0xA2}; // A00200A2

boolean isFirst = true;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

}

void loop() {

  // 1st Relay
  // Open relay
  if (isFirst) {
  Serial.println("Open Relay 1");
  Serial.write(SendData_Open_1, sizeof(SendData_Open_1));
  isFirst = false;
  }
  //  delay(3000);
  //  // Close relay
  //  Serial.println("Close Relay 1");
  //  Serial.write(SendData_Close_1, sizeof(SendData_Close_1));
  //  delay(3000);
  //
  //
  //  // Open relay
  //  Serial.println("Open Relay 2");
  //  Serial.write(SendData_Open_2, sizeof(SendData_Open_2));
  //  delay(3000);
  //  // 2nd Relay
  //  // Close relay
  //  Serial.println("Close Relay 2");
  //  Serial.write(SendData_Close_2, sizeof(SendData_Close_2));
  //  delay(3000);
}
