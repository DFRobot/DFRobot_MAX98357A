/*!
 * @file  configAmplifier.ino
 * @brief  Simply configure the Bluetooth amplifier
 * @details  After reading the bluetoothAmplifier.ino sample, you may succeed in playing music.
 * @n  This demo allows you to simply configure the player: set volume, set filter, get the address of the remote Bluetooth device
 * @n  You can configure it through the setup function, and can also adjust the config through the serial command when the program is running as per the code comment in the loop function.
 * @note  The serial operation can be directly changed to other operations such as button or knob in practice so as to realize a real Bluetooth speaker.
 * @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license  The MIT License (MIT)
 * @author  [qsjhyy](yihuan.huang@dfrobot.com)
 * @version  V1.0
 * @date  2022-01-27
 * @url  https://github.com/DFRobot/DFRobot_MAX98357A
 */
#include <DFRobot_MAX98357A.h>

DFRobot_MAX98357A amplifier;   // instantiate an object to control the amplifier

/**************************************************************
                      Setup And Loop                          
**************************************************************/

void setup(void)
{
  Serial.begin(115200);

  /**
   * @brief Init function
   * @param btName - The created Bluetooth device name
   * @param bclk - I2S communication pin number, serial clock (SCK), aka bit clock (BCK)
   * @param lrclk - I2S communication pin number, word select (WS), i.e. command (channel) select, used to switch between left and right channel data
   * @param din - I2S communication pin number, serial data signal (SD), used to transmit audio data in two's complement format
   * @return true on success, false on error
   */
  while( !amplifier.begin(/*btName=*/"bluetoothAmplifier", /*bclk=*/GPIO_NUM_25, /*lrclk=*/GPIO_NUM_26, /*din=*/GPIO_NUM_27) ){
    Serial.println("Initialize failed !");
    delay(3000);
  }
  Serial.println("Initialize succeed!");

  /**
   * @fn reverseLeftRightChannels
   * @brief Reverse left and right channels, When you find that the left
   * @n  and right channels play opposite, you can call this interface to adjust
   */
  // amplifier.reverseLeftRightChannels();

  /**
   * @brief Set volume
   * @param vol - Set volume, the range can be set to 0-9
   * @note 5 for the original volume of audio data, no increase or decrease
   */
  amplifier.setVolume(5);

  /**
   * @brief Close the audio filter
   * @note The high-pass filter and the low-pass filter will be closed at the same time because they work simultaneously, 
   * @n    When you set the range outstripping the value people's ears can distinguish, it is considered to close the corresponding filter; 
   * @n    The initial values of the filters are bq_type_highpass(2) and bq_type_lowpass(20000), 
   * @n    The filter is considered to be off at this time.
   */
  amplifier.closeFilter();

  /**
   * @brief Open audio filter
   * @param type - bq_type_highpass: open high-pass filtering; bq_type_lowpass: open low-pass filtering
   * @param fc - Threshold of filtering, range: 2-20000
   * @note For example, setting high-pass filter mode and the threshold of 500 indicates to filter out the audio signal below 500; high-pass filter and low-pass filter will work simultaneously.
   */
  amplifier.openFilter(bq_type_highpass, 500);

  /**
   * @brief Get address of remote Bluetooth device
   * @note The address will be obtained after the module is paired with the remote Bluetooth device and successfully communicates with it based on the Bluetooth AVRCP protocol.
   * @return Return the array pointer storing the address of the remote Bluetooth device
   * @n Return None when the module does not connect to the remote device or failed to communicate with it based on the Bluetooth AVRCP protocol.
   * @n AVRCP(Audio Video Remote Control Profile)
   */
  uint8_t * addr = amplifier.getRemoteAddress();
  /**
   * Print the obtained address of the remote Bluetooth device
   * Note: if the remote Bluetooth address is not obtained, it keeps waiting.
   * The function implementation follows this demo, you can change it as per your need.
   */
  printRemoteAddress(addr);

  /**
   * @brief Send passthrough command to AVRCP target.
   * @param[in]  tl : transaction label, 0 to 15, consecutive commands should use different values.
   * @param[in]  key_code : passthrough command code, e.g. 
   * @n            ESP_AVRC_PT_CMD_PLAY : start playback; 
   * @n            ESP_AVRC_PT_CMD_STOP : stop playback; 
   * @n            ESP_AVRC_PT_CMD_BACKWARD : play the previous one; 
   * @n            ESP_AVRC_PT_CMD_FORWARD : play the next one; etc.
   * @param[in]  key_state : passthrough command key state, ESP_AVRC_PT_CMD_STATE_PRESSED or ESP_AVRC_PT_CMD_STATE_RELEASED
   * @note This function may not be so practical, but you can try it if you are interested. For the details refer to: 
   * @n    https://github.com/espressif/esp-idf/blob/master/components/bt/host/bluedroid/api/include/api/esp_avrc_api.h
   */
  esp_avrc_ct_send_passthrough_cmd(/*tl=*/0, /*tl=key_code*/ESP_AVRC_PT_CMD_PLAY, /*tl=key_state*/ESP_AVRC_PT_CMD_STATE_PRESSED);

}

void loop()
{
  /**
   * @brief The parsing and implementation of the serial command is to call the config function above by the serial command when the program is running
   * @note  The amplifier can be configured by entering the corresponding command in the serial port,  format: cmd-value (command - set value, some commands that don't require assignment can be omitted)
   * @n  Currently available commands:
   * @n    Set and open high-pass filter: e.g. hp-500
   * @n    Set and open low-pass filter: e.g. lp-15000
   * @n    Close filter: e.g. closeFilter-
   * @n    Set volume: e.g. vol-5
   * @n    Get the address of the remote Bluetooth device: e.g. addr-
   * @n    Start playback: e.g. start-
   * @n    Stop playback: e.g. stop-
   * @n    Play last track: e.g. previous-
   * @n    Play next track: e.g. next-
   * @n    Other commands will print this description
   * @n  For the detailed meaning of the commands, please refer to the comments of the corresponding functions.
   */
  parseSerialCommand();
  delay(500);
}

/**************************************************************
                  Print the obtained remote Bluetooth address                      
**************************************************************/

void printRemoteAddress(uint8_t * _addr)
{
  while(NULL == _addr){   // When obtaining remote Bluetooth address failed, it will wait until it succeeds.
    Serial.println("Please connect the remote Bluetooth device!");
    delay(5000);
    _addr = amplifier.getRemoteAddress();   // Get the remote Bluetooth address again.
  }
  Serial.print("Remote bluetooth device address : ");
  for(uint8_t i=0; i<5; i++){
    Serial.print(*_addr, HEX);
    Serial.print("-");
    _addr++;
  }
  Serial.println(*_addr, HEX);
  Serial.println();
}

/**************************************************************
                    The parsing and implementation of the serial command                        
**************************************************************/

void parseSerialCommand(void)
{
  String cmd;   // Save the command type read in the serial port
  float value;   // Save the command value read in the serial port

  /**
   * Command format: cmd-value
   * cmd: command type
   * value: the set value corresponding to the command type, some commands can be empty
   * For example: (1) set high-pass filter, filter the audio data below 500: hp-500
   *      (2) close filter: closeFilter-
   */
  if(Serial.available()){   // Detect whether there is an available serial command
    cmd = Serial.readStringUntil('-');   // Read the specified termination character string, used to cut and identify the serial command. The same type won't repeat later.

    if(cmd.equals("hp")){   // Determine if it's the command type for setting high-pass filter
      Serial.println("Setting a High-Pass filter...\n");
      value =Serial.parseFloat();   // Parse character string and return floating point number
      /**
       * @brief Open audio filter
       * @param type - bq_type_highpass: open high-pass filtering; bq_type_lowpass: open low-pass filtering
       * @param fc - Threshold of filtering, range: 2-20000
       * @note For example, setting high-pass filter mode and the threshold of 500 indicates to filter out the audio signal below 500; high-pass filter and low-pass filter can work simultaneously.
       */
      amplifier.openFilter(bq_type_highpass, value);

    }else if(cmd.equals("lp")){   // Determine if it's the command type for setting low-pass filter
      Serial.println("Setting a Low-Pass filter...\n");
      value = Serial.parseFloat();
      amplifier.openFilter(bq_type_lowpass, value);

    }else if(cmd.equals("closeFilter")){   // Determine if it's the command type for closing filter
      Serial.println("Closing filter...\n");
      /**
       * @brief Close the audio filter
       */
      amplifier.closeFilter();

    }else if(cmd.equals("vol")){   // Determine if it's the command type for setting volume
      Serial.println("Setting volume...\n");
      value = Serial.parseFloat();
      /**
       * @brief Set volume
       * @param vol - Set volume, the range can be set to 0-9
       * @note 5 for the original volume of audio data, no increase or decrease
       */
      amplifier.setVolume(value);

    }else if(cmd.equals("addr")){   // Determine if it's the command type for obtaining remote Bluetooth address
      /**
       * @brief Get the remote Bluetooth address
       * @note The address will be obtained after the module is paired with the remote Bluetooth device and successfully communicates with it based on the Bluetooth AVRCP protocol.
       * @return Return the array pointer storing the address of the remote Bluetooth device
       * @n Return None when the module does not connect to the remote device or failed to communicate with it based on the Bluetooth AVRCP protocol.
       * @n AVRCP(Audio Video Remote Control Profile)
       */
      uint8_t * addr = amplifier.getRemoteAddress();
      /**
       * Print the obtained address of the remote Bluetooth device
       * Note: if the remote Bluetooth address is not obtained, it keeps waiting.
       * The function implementation follows this demo, you can change it as per your need.
       */
      printRemoteAddress(addr);

    }else if(cmd.equals("start")){   // Determine if it's the command type for starting playback
      Serial.println("starting amplifier...\n");
      /**
       * @brief Send passthrough command to AVRCP target.
       * @param[in]       tl : transaction label, 0 to 15, consecutive commands should use different values.
       * @param[in]       key_code : passthrough command code, e.g. ESP_AVRC_PT_CMD_PLAY, ESP_AVRC_PT_CMD_STOP, etc.
       * @param[in]       key_state : passthrough command key state, ESP_AVRC_PT_CMD_STATE_PRESSED or ESP_AVRC_PT_CMD_STATE_RELEASED
       * @note This function may not be so practical, but you can try it if you are interested. For the details refer to: 
       * @n    https://github.com/espressif/esp-idf/blob/master/components/bt/host/bluedroid/api/include/api/esp_avrc_api.h
       */
      esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_PLAY, ESP_AVRC_PT_CMD_STATE_PRESSED);

    }else if(cmd.equals("stop")){   // Determine if it's the command type for stopping playback
      Serial.println("Stopping amplifier...\n");
      // Same as the above.
      esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_STOP, ESP_AVRC_PT_CMD_STATE_PRESSED);

    }else if(cmd.equals("previous")){   // Determine if it's the command type for playing last track
      Serial.println("Playing previous...\n");
      // Same as the above.
      esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_BACKWARD, ESP_AVRC_PT_CMD_STATE_PRESSED);

    }else if(cmd.equals("next")){   // Determine if it's the command type for playing next track
      Serial.println("Playing next...\n");
      // Same as the above.
      esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_FORWARD, ESP_AVRC_PT_CMD_STATE_PRESSED);

    }else{   // Unknown command type
      Serial.println("Help : \n \
      Currently available commands (format: cmd-value):\n \
        Set and open high-pass filter: e.g. hp-500\n \
        Set and open low-pass filter: e.g. lp-15000\n \
        Close filter: e.g. closeFilter-\n \
        Set volume: e.g. vol-5.0\n \
        Get the address of the remote Bluetooth device: e.g. addr-\n \
        Start playback: e.g. start-\n \
        Stop playback: e.g. stop-\n \
        Play last track: e.g. previous-\n \
        Play next track: e.g. next-\n \
      For more details about commands, please refer to the code comments of this demo.\n");   // 
    }
    while(Serial.read() >= 0);   // Clear the remaining data in the serial port
  }
}
