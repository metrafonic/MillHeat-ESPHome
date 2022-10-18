#include "esphome.h"

bool newData;
char receivedChars[16];


// mill kommandoer
char settPower[] = {0x00, 0x10, 0x20, 0x00, 0x44, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Powertoggle er pos 5
char settTemp[] = {0x00, 0x10, 0x22, 0x00, 0x46, 0x01, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00}; // temperatursetting er pos 7
char settFan[] = {0x00, 0x10, 0x27, 0x00, 0x48, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // fan er pos 5

class MyCustomClimate : public Component, public UARTDevice, public Climate {
public:
  MyCustomClimate(UARTComponent *parent) : UARTDevice(parent) {}
  void setup() override {

  }

  void loop() override {
      recvWithStartEndMarkers();
      
      
      if (newData == true) {
        newData = false;     
      if (receivedChars[4] == 0xC9 ) { // Filtrer ut unødig informasjon
          
          if (receivedChars[12] != 0 ) {
          this->target_temperature= receivedChars[12];
          }

          if (receivedChars[6] != 0 ) {
            this->current_temperature = (float)receivedChars[6]/10;
          }
          if (receivedChars[10] == 0x00 ) {
          this->mode= climate::CLIMATE_MODE_OFF;
          this->action= climate::CLIMATE_ACTION_OFF;
          } else if (receivedChars[10] == 0x01 ) {
          this->mode= climate::CLIMATE_MODE_HEAT ;
          }
          if (receivedChars[11] == 0x01 ) {
          this->action= climate::CLIMATE_ACTION_HEATING;
          } else {
          this->action= climate::CLIMATE_ACTION_IDLE;
          }
          if (receivedChars[14] == 0x00 ) {
          this->fan_mode= climate::CLIMATE_FAN_OFF;
          } else if (receivedChars[10] == 0x01 ) {
          this->fan_mode= climate::CLIMATE_FAN_ON ;
          }
          this->publish_state();
    }
  }
}

  void control(const ClimateCall &call) override {
    ESP_LOGD("custom", "Climate change requested");

    if (call.get_fan_mode().has_value()) {
      // User requested fan mode change
      ClimateFanMode fan_mode = *call.get_fan_mode();
      // Send fan mode to hardware
      if (fan_mode == CLIMATE_FAN_ON) {
        sendCmd(settFan, sizeof(settFan), 0x01);
      } else if (fan_mode == CLIMATE_FAN_OFF) {
        sendCmd(settFan, sizeof(settFan), 0x00);
      }

      this->fan_mode = fan_mode;
      this->publish_state();
    }


    if (call.get_mode().has_value()) {

        switch (call.get_mode().value()) {
                case CLIMATE_MODE_OFF:
                  sendCmd(settPower, sizeof(settPower), 0x00);
                    break;
                case CLIMATE_MODE_HEAT:
                  sendCmd(settPower, sizeof(settPower), 0x01);
                    break;
        }

      ClimateMode mode = *call.get_mode();

      this->mode = mode;
      this->publish_state();
        }

    if (call.get_target_temperature().has_value()) {
      // User requested target temperature change
      int temp = *call.get_target_temperature();
      sendCmd(settTemp, sizeof(settTemp), temp);
      // ...
      this->target_temperature = temp;
      this->publish_state();

    }
    }
  

  ClimateTraits traits() override {
    // The capabilities of the climate device
    auto traits = climate::ClimateTraits();
    traits.set_supports_current_temperature(true);
    traits.set_supports_two_point_target_temperature(false);
    traits.set_visual_min_temperature(5);
    traits.set_visual_max_temperature(30);
    traits.set_supports_action(true);
    traits.set_supported_fan_modes({
      climate::CLIMATE_FAN_OFF,
      climate::CLIMATE_FAN_ON,
      });
    traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_HEAT,
    });
    return traits;
  }

void recvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarker = 0x5A;
  char endMarker = 0x5B;
  char lineend = 0x0A;
  char rc;

  if (available() > 0) {
    rc = read();
    if (recvInProgress == true) {
      if ((rc != endMarker) && (rc != lineend)) {
        receivedChars[ndx] = (char) rc;
        // Heater_debug.publish(receivedChars[ndx]);
        ndx++;
      }
      else {
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }

    else if (rc == startMarker) {
      // Heater_debug.publish("Start mark");
      recvInProgress = true;
    }
  }
}



/*--- Funksjon for summering av kontrollbyte ---*/
unsigned char checksum(char *buf, int len) {

  unsigned char chk = 0;
  for ( ; len != 0; len--) {
    chk += *buf++;
  }
  return chk;
}
/* Seriedata ut til mill mikrokontroller ---*/
void sendCmd(char* arrayen, int len, int kommando) {
  ESP_LOGD("custom", "Sending serial command");
  if (arrayen[4] == 0x46) { // Temperatur
    arrayen[7] = kommando;
  }
  if (arrayen[4] == 0x44) { // Power av/på
    arrayen[5] = kommando;
    arrayen[len] = (byte)0x00;  // Padding..
  }
  if (arrayen[4] == 0x48) { // Fan
    arrayen[5] = kommando;
    arrayen[len] = (byte)0x00;  // Padding..
  }
  char crc = checksum(arrayen, len + 1);
  ESP_LOGD("custom", "writing start byte");
  write((byte)0x5A); // Startbyte
  for (int i = 0; i < len + 1; i++) { // Beskjed
    write((byte)arrayen[i]);
  }
  write((byte)crc); // Kontrollbyte
  write((byte)0x5B); // Stoppbyte
}
};
