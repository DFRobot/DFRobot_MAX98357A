/*!
 * @file  SDmusic.ino
 * @brief  This demo allows you to play the audio in the SD card (currently only support the audio in .wav format)
 * @details  Connect the module with Dupont wires or other cables as per the comments of init function below, and insert the SD card
 * @n  If there are music files in the correct format in the SD card, 
 * @n  (only support English for music file and path name currently and try not to use spaces, only support .wav for the audio format currently)
 * @n  you can set volume, set filter, scan, play, pause and switch songs through the config function in setup
 * @n   and adjust the config through the serial command when the program is running as per the code comment in the loop function.
 * @note  The serial operation can be directly changed to other operations such as button or knob in practice so as to realize real Bluetooth speaker.
 * @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license  The MIT License (MIT)
 * @author  [qsjhyy](yihuan.huang@dfrobot.com)
 * @version  V1.0
 * @date  2022-02-16
 * @url  https://github.com/DFRobot/DFRobot_MAX98357A
 */
#include <DFRobot_MAX98357A.h>

DFRobot_MAX98357A amplifier;   // instantiate an object to control the amplifier

String musicList[100];   // SD card music list

/**************************************************************
                      Setup And Loop                          
**************************************************************/

void setup(void)
{
  Serial.begin(115200);

  /**
   * @brief Initialize I2S
   * @param _bclk - I2S communication pin number, serial clock (SCK), aka bit clock (BCK)
   * @param _lrclk - I2S communication pin number, word select (WS), i.e. command (channel) select, used to switch between left and right channel data
   * @param _din - I2S communication pin number, serial data signal (SD), used to transmit audio data in two's complement format
   * @return true on success, false on error
   */
  while ( !amplifier.initI2S(/*_bclk=*/GPIO_NUM_25, /*_lrclk=*/GPIO_NUM_26, /*_din=*/GPIO_NUM_27) ){
    Serial.println("Initialize I2S failed !");
    delay(3000);
  }
  /**
   * @fn initSDCard
   * @brief Initialize SD card
   * @param csPin cs pin number for spi communication of SD card module
   * @return true on success, false on error
   * @note Pin connection of SD card module
   * @n  SD Card |  ESP32
   * @n    +5V   |   VCC(5V)
   * @n    GND   |   GND
   * @n    SS    |   csPin (default to be IO5, can be customized through init function)
   * @n    SCK   |   SCK(IO18)
   * @n    MISO  |   MISO(IO19)
   * @n    MOSI  |   MOSI(IO23)
   * @n  Search MicroSD card reader module at www.dfrobot.com
   */
  while (!amplifier.initSDCard(/*csPin=*/GPIO_NUM_5)){
    Serial.println("Initialize SD card failed !");
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
   * @brief Scan the music files in WAV format in the SD card
   * @param musicList - The music files in .wav format scanned from the SD card. Type is character string array.
   * @return None
   * @note Only support English for music file and path name currently and try not to use spaces, only support .wav for the audio format currently
   */
  amplifier.scanSDMusic(musicList);
  /**
   * Print the list of the scanned music files that can be played
   */
  printMusicList();

  /**
   * @brief Set volume
   * @param vol - Set volume, the range can be set to 0-9
   * @note 5 for the original volume of audio data, no increase or decrease
   */
  amplifier.setVolume(5);

  /**
   * @brief Close the audio filter
   * @note The high-pass filter and the low-pass filter will be closed at the same time because they work simultaneously.
   * @n    When you set the range outstripping the value people's ears can distinguish, it is considered to close the corresponding filter; 
   * @n    The initial values of the filters are bq_type_highpass(2) and bq_type_lowpass(20000) 
   * @n    The filter is considered to be off at this time.
   */
  amplifier.closeFilter();

  /**
   * @brief Open audio filter
   * @param type - bq_type_highpass: open high-pass filtering; bq_type_lowpass: open low-pass filtering
   * @param fc - Threshold of filtering, range: 2-20000
   * @note For example, setting high-pass filter mode and the threshold of 500 indicates to filter out the audio signal below 500; high-pass filter and low-pass filter will work simultaneously.
close the audio filter
   */
  amplifier.openFilter(bq_type_highpass, 500);

  /**
   * @brief SD card music playback control interface
   * @param CMD - Playback control command: 
   * @n SD_AMPLIFIER_PLAY: Start to play music, which can be played from the position where you paused before
   * @n   If no music file is selected through playSDMusic(), the first one in the list will be played by default.
   * @n   Playback error may occur if music files are not scanned from SD card in the correct format (only support English for path name of music files and WAV for their format currently)
   * @n SD_AMPLIFIER_PAUSE: Pause playback, keep the playback position of the current music file
   * @n SD_AMPLIFIER_STOP: Stop playback, end the current music playback
   * @return None
   */
  amplifier.SDPlayerControl(SD_AMPLIFIER_PLAY);
  delay(5000);   // Play for 5 seconds first, clearly separated from the next operation of skipping the song

  /**
   * @brief Play music files in the SD card
   * @param Filename - music file name, only support the music files in .wav format currently
   * @note Music file name must be an absolute path like /musicDir/music.wav
   * @return None
   */
  if(musicList[1].length()){
    Serial.println("Changing Music...\n");
    amplifier.playSDMusic(musicList[1].c_str());
  }else{
    Serial.println("The currently selected music file is incorrect!\n");
  }

}

void loop()
{
  /**
   * @brief The parsing and implementation of the serial command is to call the config function above when the program is running using the serial command
   * @note  The amplifier can be configured by entering the corresponding command in the serial port,  format is: cmd-value (command - set value, some commands that don't require assignment can be omitted)
   * @n  Currently available commands:
   * @n    Start playback: e.g. start-
   * @n    Pause playback: e.g. pause-
   * @n    Stop playback: e.g. stop-
   * @n    Print music list: e.g. musicList-
   * @n    Change songs according to the music list: e.g. changeMusic-1
   * @n    Set and open high-pass filter: e.g. hp-500
   * @n    Set and open low-pass filter: e.g. lp-15000
   * @n    Close filter: e.g. closeFilter-
   * @n    Set volume: e.g. vol-5.0
   * @n    Other commands will print this description
   * @n  For the detailed meaning of the commands, please refer to the comments of the corresponding functions.
   */
  parseSerialCommand();
  delay(500);
}

/**************************************************************
              Print the list of the scanned music files that can be played                  
**************************************************************/

void printMusicList(void)
{
  uint8_t i = 0;

  if(musicList[i].length()){
    Serial.println("\nMusic List: ");
  }else{
    Serial.println("The SD card audio file scan is empty, please check whether there are audio files in the SD card that meet the format!");
  }

  while(musicList[i].length()){
    Serial.print("\t");
    Serial.print(i);
    Serial.print("  -  ");
    Serial.println(musicList[i]);
    i++;
  }
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
   * cmd : indicate the command type
   * value : indicate the set value corresponding to the command type, some commands can be empty
   * For example: (1) set high-pass filter, filter the audio data below 500: hp-500
   *      (2) close filter: closeFilter-
   */
  if(Serial.available()){   // Detect whether there is an available serial command
    cmd = Serial.readStringUntil('-');   // Read the specified terminator character string, used to cut and identify the serial command.  The same comment won't repeat later.

    if(cmd.equals("hp")){   // Determine if it’s the command type for setting high-pass filter
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
      value =Serial.parseFloat();

      amplifier.openFilter(bq_type_lowpass, value);

    }else if(cmd.equals("closeFilter")){   // Determine if it's the command type for closing filter
      Serial.println("Closing filter...\n");

      /**
       * @brief Close the audio filter
       */
      amplifier.closeFilter();

    }else if(cmd.equals("vol")){   // Determine if it's the command type for setting volume
      Serial.println("Setting volume...\n");
      value =Serial.parseFloat();

      /**
       * @brief Set volume
       * @param vol - Set volume, the range can be set to 0-9
       * @note 5 for the original volume of audio data, no increase or decrease
       */
      amplifier.setVolume(value);

    }else if(cmd.equals("start")){   // Determine if it's the command type for starting playback
      Serial.println("starting amplifier...\n");

      /**
       * @brief SD card music playback control interface
       * @param CMD - Playback control command: 
       * @n SD_AMPLIFIER_PLAY: Start to play music, which can be played from the position where you paused before
       * @n   If no music file is selected through playSDMusic(), the first one in the list will be played by default.
       * @n   Playback error may occur if music files are not scanned from SD card in the correct format (only support English for path name of music files and WAV for their format currently)
       * @n SD_AMPLIFIER_PAUSE: pause playback, keep the playback position of the current music file
       * @n SD_AMPLIFIER_STOP: stop playback, end the current music playback
       * @return None
       */
      amplifier.SDPlayerControl(SD_AMPLIFIER_PLAY);

    }else if(cmd.equals("pause")){   // Determine if it's the command type for pausing playback
      Serial.println("Pause amplifier...\n");

      // The same as above
      amplifier.SDPlayerControl(SD_AMPLIFIER_PAUSE);

    }else if(cmd.equals("stop")){   // Determine if it's the command type for stopping playback
      Serial.println("Stopping amplifier...\n");

      // The same as above
      amplifier.SDPlayerControl(SD_AMPLIFIER_STOP);

    }else if(cmd.equals("musicList")){   // Determine if it's the command type for printing the list of the music files that can be played currently
      Serial.println("Scanning music list...\n");

      /**
       * @brief Scan the music files in WAV format in the SD card
       * @param musicList - The music files in .wav format scanned from the SD card. Type is character string array
       * @return None
       * @note Only support English for music file and path name currently and try to avoid spaces, only support .wav for the audio format currently
       */
      amplifier.scanSDMusic(musicList);
      /**
       * Print the list of the scanned music files that can be played
       */
      printMusicList();

    }else if(cmd.equals("changeMusic")){   // Determine if it's the command type for changing songs according to the music list
      cmd = musicList[Serial.parseInt()];

      /**
       * @brief Play music files in the SD card
       * @param Filename - music file name, only support the music files in .wav format currently
       * @note Music file name must be an absolute path like /musicDir/music.wav
       * @return None
       */
      if(cmd.length()){
        Serial.println("Changing Music...\n");
        amplifier.playSDMusic(cmd.c_str());
      }else{
        Serial.println("The currently selected music file is incorrect!\n");
      }

    }else{   // Unknown command type
      Serial.println("Help : \n \
      Currently available commands (format: cmd-value):\n \
        Start playback: e.g. start-\n \
        Pause playback: e.g. pause-\n \
        Stop playback: e.g. stop-\n \
        Print music list: e.g. musicList-\n \
        Change songs according to the music list: e.g. changeMusic-1\n \
        Set and open high-pass filter: e.g. hp-500\n \
        Set and open low-pass filter: e.g. lp-15000\n \
        Close filter: e.g. closeFilter-\n \
        Set volume: e.g. vol-5.0\n \
      For the detailed meaning, please refer to the code comments of this demo.\n");   //
    }
    while(Serial.read() >= 0);   // Clear the remaining data in the serial port
  }
}
