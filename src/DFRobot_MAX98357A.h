/*!
 * @file  DFRobot_MAX98357A.h
 * @brief  Define infrastructure of DFRobot_MAX98357A class
 * @details  Configure a classic Bluetooth, pair with Bluetooth devices, receive Bluetooth audio, 
 * @n        Process simple audio signal, and pass it into the amplifier using I2S communication
 * @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license  The MIT License (MIT)
 * @author  [qsjhyy](yihuan.huang@dfrobot.com)
 * @version  V1.0
 * @date  2022-01-20
 * @url  https://github.com/DFRobot/DFRobot_MAX98357A
 */
#ifndef __DFRobot_AMPLIFIER_H__
#define __DFRobot_AMPLIFIER_H__

#include <Arduino.h>

#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_gap_bt_api.h>
#include <esp_a2dp_api.h>
#include "esp_avrc_api.h"

#include <driver/i2s.h>

#include "Biquad.h"   // Code from https://www.earlevel.com/main/2012/11/26/biquad-c-source-code/ . Thank you very much!

#include "SD.h"

// #define ENABLE_DBG   //!< Open this macro and you can see the details of the program
#ifdef ENABLE_DBG
  #define DBG(...) {Serial.print("[");Serial.print(__FUNCTION__); Serial.print("(): "); Serial.print(__LINE__); Serial.print(" ] "); Serial.println(__VA_ARGS__);}
#else
  #define DBG(...)
#endif

#define NUMBER_OF_FILTER   ((int)(3))   //!< The number of the cascaded filter

#define SD_AMPLIFIER_PLAY  ((uint8_t)1)   //!< Playback control of audio in SD card - start playback
#define SD_AMPLIFIER_PAUSE ((uint8_t)2)   //!< Playback control of audio in SD card - pause playback
#define SD_AMPLIFIER_STOP  ((uint8_t)3)   //!< Playback control of audio in SD card - stop playback

#define MAX98357A_VOICE_FROM_SD ((uint8_t)0)
#define MAX98357A_VOICE_FROM_BT ((uint8_t)1)

class DFRobot_MAX98357A
{
public:
  static uint8_t remoteAddress[6];   // Store the address of the remote Bluetooth device

public:

/************************ Init ********************************/

  /**
   * @fn DFRobot_MAX98357A
   * @brief Constructor
   * @return None
   */
  DFRobot_MAX98357A(void);

  /**
   * @fn begin
   * @brief Init function
   * @param btName - Name of the created Bluetooth device
   * @param bclk - I2S communication pin number, serial clock (SCK), aka bit clock (BCK)
   * @param lrclk - I2S communication pin number, word select (WS), i.e. command (channel) select, used to switch between left and right channel data
   * @param din - I2S communication pin number, serial data signal (SD), used to transmit audio data in two's complement format
   * @return true on success, false on error
   */
  bool begin(const char *btName="bluetoothAmplifier", 
             int bclk=GPIO_NUM_25, 
             int lrclk=GPIO_NUM_26, 
             int din=GPIO_NUM_27);

  /**
   * @fn initI2S
   * @brief Initialize I2S
   * @param _bclk - I2S communication pin number, serial clock (SCK), aka bit clock (BCK)
   * @param _lrclk - I2S communication pin number, word select (WS), i.e. command (soundtrack) select, used to switch the data of the left and right channels
   * @param _din - I2S communication pin number, serial data signal (SD), used to transmit audio data in two's complement format
   * @return true on success, false on error
   */
  bool initI2S(int _bclk, int _lrclk, int _din);

  /**
   * @fn initBluetooth
   * @brief Initialize bluetooth
   * @param _btName - Name of the created Bluetooth device
   * @return true on success, false on error
   */
  bool initBluetooth(const char * _btName);

  /**
   * @fn initSDCard
   * @brief Initialize SD card
   * @param csPin cs pin number for spi communication of SD card module
   * @return true on success, false on error
   */
  bool initSDCard(uint8_t csPin=GPIO_NUM_5);

/*************************** Function ******************************/

  /**
   * @fn scanSDMusic
   * @brief Scan the music files in WAV format in the SD card
   * @param musicList - The music files in WAV format scanned from the SD card. Type is character string array
   * @return None
   * @note Only support English for path name of music files and WAV for their format currently
   */
  void scanSDMusic(String * musicList);

  /**
   * @fn playSDMusic
   * @brief Play music files in the SD card
   * @param Filename - music file name, only support the music files in .wav format currently
   * @note Music file name must be an absolute path like /musicDir/music.wav
   * @return None
   * @note Only support English for path name of music files and WAV for their format currently
   */
  void playSDMusic(const char *Filename);

  /**
   * @fn SDPlayerControl
   * @brief SD card music playback control interface
   * @param CMD - Playback control command: 
   * @n SD_AMPLIFIER_PLAY: Start to play music, which can be played from the position where you paused before
   * @n   If no music file is selected through playSDMusic(), the first one in the list will be played by default.
   * @n   Playback error may occur if music files are not scanned from SD card in the correct format (only support English for path name of music files and WAV for their format currently)
   * @n SD_AMPLIFIER_PAUSE: Pause playback, keep the playback position of the current music file
   * @n SD_AMPLIFIER_STOP: Stop playback, stop the current music playback
   * @return None
   */
  void SDPlayerControl(uint8_t CMD);

  /**
   * @fn getMetadata
   * @brief Get "metadata" through AVRC command
   * @param type - The type of metadata to be obtained, and the parameters currently supported: 
   * @n     ESP_AVRC_MD_ATTR_TITLE   ESP_AVRC_MD_ATTR_ARTIST   ESP_AVRC_MD_ATTR_ALBUM
   * @return The corresponding type of "metadata"
   */
  String getMetadata(uint8_t type);

  /**
   * @fn getRemoteAddress
   * @brief Get the address of the remote Bluetooth device
   * @note The address will be obtained after the module is paired with the remote Bluetooth device and successfully communicates with it based on the Bluetooth AVRCP protocol.
   * @return Return the array pointer storing the address of the remote Bluetooth device
   * @n Return None when the module does not connect to the remote device or failed to communicate with it based on the Bluetooth AVRCP protocol.
   * @n AVRCP(Audio Video Remote Control Profile)
   */
  uint8_t * getRemoteAddress(void);

  /**
   * @fn setVolume
   * @brief Set volume
   * @param vol - Set volume, the range can be set to 0-9
   * @note 5 for the original volume of audio data, no increase or decrease
   * @return None
   */
  void setVolume(float vol);

  /**
   * @fn openFilter
   * @brief Open audio filter
   * @param type - bq_type_highpass: open high-pass filtering; bq_type_lowpass: open low-pass filtering
   * @param fc - Threshold of filtering, range: 2-20000
   * @note For example, setting high-pass filter mode and the threshold of 500 indicates to filter out the audio signal below 500; high-pass filter and low-pass filter will work simultaneously.
   * @return None
   */
  void openFilter(int type, float fc);

  /**
   * @fn closeFilter
   * @brief Close the audio filter
   * @return None
   */
  void closeFilter(void);

  /**
   * @fn reverseLeftRightChannels
   * @brief Reverse left and right channels, When you find that the left
   * @n  and right channels play opposite, you can call this interface to adjust
   * @return None
   */
  void reverseLeftRightChannels(void);

protected:

  /**
   * @fn end
   * @brief End communication, release resources
   * @return None
   */
  void end(void);

  /**
   * @fn listDir
   * @brief Traversal of SD card files
   * @param fs - SD card file stream pointer
   * @param dirName - Start traversal of SD card catalogue
   * @return None
   */
  void listDir(fs::FS &fs, const char * dirName);

  /**
   * @fn setFilter
   * @brief Set filter
   * @param _filter - The filter to be set
   * @param _type - bq_type_highpass: open high-pass filtering; bq_type_lowpass: open low-pass filtering
   * @param _fc - Threshold of filtering, range: 2-20000
   * @return None
   */
  void setFilter(Biquad * _filter, int _type, float _fc);

  /**
   * @fn filterToWork
   * @brief Make the filter work, process audio data
   * @param filterHP - The high-pass filter to be used
   * @param filterLP - The low-pass filter to be used
   * @param rawData - The raw audio data to be processed. float
   * @return The processed audio data int16_t
   */
  static int16_t filterToWork(Biquad * filterHP, Biquad * filterLP, float rawData);

/*************************** Function ******************************/

  /**
   * @fn audioDataProcessCallback
   * @brief esp_a2d_sink_register_data_callback() function, 
   * @n     Process the audio stream data of Bluetooth A2DP protocol communication
   * @param data - The audio data from the remote Bluetooth device
   * @param len - Byte length of audio data
   * @return None
   * @note Because of some factors like action scope, the function should be static. Therefore it is shared by multiple objects of the class.
   */
  static void audioDataProcessCallback(const uint8_t *data, uint32_t len);

  /**
   * @fn filterToWork
   * @brief esp_a2d_register_callback() function, used to process the event of Bluetooth A2DP protocol communication
   * @param event - Type of the triggered A2DP event
   * @param param - The parameter information corresponding to the event
   * @return None
   * @note Because of some factors like action scope, the function should be static. Therefore it is shared by multiple objects of the class.
   */
  static void a2dpCallback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);

  /**
   * @fn avrcCallback
   * @brief esp_avrc_ct_register_callback() function, used to process the event of Bluetooth AVRC protocol communication
   * @param event - Type of the triggered AVRC event
   * @param param - The parameter information corresponding to the event
   * @return None
   * @note Because of some factors like action scope, the function should be static. Therefore it is shared by multiple objects of the class.
   */
  static void avrcCallback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param);

  /**
   * @fn playWAV
   * @brief The parsing play function for audio files in WAV format
   * @param arg - Corresponding parameter information
   * @return None
   * @note Because of some factors like action scope, the function should be static. Therefore it is shared by multiple objects of the class.
   */
  static void playWAV(void *arg);

private:

};

#endif
