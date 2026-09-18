/*!
 * @file  DFRobot_MAX98357A.cpp
 * @brief  Define the infrastructure DFRobot_MAX98357A class
 * @details  Configure a classic Bluetooth, pair with Bluetooth devices, receive Bluetooth audio, 
 * @n        Process simple audio signal, and pass it into the amplifier using I2S communication
 * @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license  The MIT License (MIT)
 * @author  [qsjhyy](yihuan.huang@dfrobot.com)
 * @version  V1.0
 * @date  2022-01-21
 * @url  https://github.com/DFRobot/DFRobot_MAX98357A
 */
#include "DFRobot_MAX98357A.h"
#include <freertos/semphr.h>

// Recent cores release unused Bluetooth memory before setup(). Declare Classic
// usage before app_main(), just as the core's BluetoothSerial library does.
#if __has_include("esp32-hal-alloc-bt-classic-mem.h")
#include "esp32-hal-alloc-bt-classic-mem.h"
#endif

// IDF 5's channel driver must never be mixed with the legacy I2S driver.
namespace {
bool initializationOK(const char *stage, esp_err_t error)
{
  if (error == ESP_OK) return true;
  Serial.printf("[MAX98357A] %s failed: %s (0x%x)\n",
                stage, esp_err_to_name(error), static_cast<unsigned int>(error));
  return false;
}
#if ESP_IDF_VERSION_MAJOR >= 5
  i2s_chan_handle_t txChannel = nullptr;
  SemaphoreHandle_t i2sMutex = nullptr;
#endif

esp_err_t writeAudio(const void *data, size_t size, size_t *written, TickType_t timeout)
{
#if ESP_IDF_VERSION_MAJOR >= 5
  *written = 0;
  if (!i2sMutex || xSemaphoreTake(i2sMutex, timeout) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }
  // The old driver takes RTOS ticks; the channel API takes milliseconds.
  esp_err_t err = txChannel
      ? i2s_channel_write(txChannel, data, size, written, timeout * portTICK_PERIOD_MS)
      : ESP_ERR_INVALID_STATE;
  xSemaphoreGive(i2sMutex);
  return err;
#else
  return i2s_write(I2S_NUM_0, data, size, written, timeout);
#endif
}

esp_err_t setAudioSampleRate(uint32_t rate)
{
#if ESP_IDF_VERSION_MAJOR >= 5
  if (!rate || !i2sMutex) return ESP_ERR_INVALID_ARG;
  if (xSemaphoreTake(i2sMutex, pdMS_TO_TICKS(1000)) != pdTRUE) return ESP_ERR_TIMEOUT;
  esp_err_t err = txChannel ? i2s_channel_disable(txChannel) : ESP_ERR_INVALID_STATE;
  if (err == ESP_OK) {
    i2s_std_clk_config_t clock = I2S_STD_CLK_DEFAULT_CONFIG(rate);
    err = i2s_channel_reconfig_std_clock(txChannel, &clock);
    // Restore the running state even when reconfiguration fails.
    esp_err_t enableError = i2s_channel_enable(txChannel);
    if (err == ESP_OK) err = enableError;
  }
  xSemaphoreGive(i2sMutex);
  return err;
#else
  return i2s_set_sample_rates(I2S_NUM_0, rate);
#endif
}

void releaseAudio()
{
#if ESP_IDF_VERSION_MAJOR >= 5
  if (!i2sMutex) return;
  xSemaphoreTake(i2sMutex, portMAX_DELAY);
  if (txChannel) {
    i2s_channel_disable(txChannel);
    i2s_del_channel(txChannel);
    txChannel = nullptr;
  }
  // Keep the mutex alive: a callback may already be waiting for it.
  xSemaphoreGive(i2sMutex);
#else
  i2s_driver_uninstall(I2S_NUM_0);
#endif
}
} // namespace

uint8_t DFRobot_MAX98357A::remoteAddress[6];   // Address of the connected remote Bluetooth device

float _volume = 1.0;   // Change the coefficient of audio signal volume
int32_t _sampleRate = 44100;   // I2S communication frequency
bool _avrcConnected = false;   // AVRC connection status
bool _filterFlag = false;   // Filter enabling flag

String _metadata = "";   // metadata
uint8_t _metaFlag = 0;   // metadata refresh flag
uint8_t _voiceSource = MAX98357A_VOICE_FROM_BT;   // The audio source, used to correct left and right audio

Biquad _filterLLP[NUMBER_OF_FILTER];   // Left channel low-pass filter
Biquad _filterRLP[NUMBER_OF_FILTER];   // Right channel low-pass filter
Biquad _filterLHP[NUMBER_OF_FILTER];   // Left channel high-pass filter
Biquad _filterRHP[NUMBER_OF_FILTER];   // Right channel high-pass filter

char fileName[100];
uint8_t SDAmplifierMark = SD_AMPLIFIER_STOP;   // SD card play flag
TaskHandle_t xPlayWAV = NULL;   // SD card play Task
String _musicList[100];   // SD card music list
uint8_t musicCount = 0;   // SD card music count

/**
 * @struct sWavParse_t
 * @brief The struct for parsing audio information in WAV format
 */
typedef struct
{
    char                  riffType[4];
    unsigned int          riffSize;
    char                  waveType[4];
    char                  formatType[4];
    unsigned int          formatSize;
    uint16_t              compressionCode;
    uint16_t              numChannels;
    uint32_t              sampleRate;
    unsigned int          bytesPerSecond;
    unsigned short        blockAlign;
    uint16_t              bitsPerSample;
    char                  dataType1[1];
    char                  dataType2[3];
    unsigned int          dataSize;
    char                  data[800];
}sWavParse_t;

/**
 * @struct sWavInfo_t
 * @brief All the information of the audio in WAV format
 */
typedef struct
{
    sWavParse_t header;
    FILE *fp;
}sWavInfo_t;

/*************************** Init ******************************/

DFRobot_MAX98357A::DFRobot_MAX98357A()
{
}

bool DFRobot_MAX98357A::begin(const char *btName, int bclk, int lrclk, int din)
{
  // Initialize I2S
  if (!initI2S(bclk, lrclk, din)){
    DBG("Initialize I2S failed !");
    return false;
  }

  // Initialize bluetooth
  if (!initBluetooth(btName)){
    DBG("Initialize bluetooth failed !");
    releaseAudio();
    return false;
  }

  // Initialize the filter
  setFilter(_filterLLP, bq_type_lowpass, 20000.0);
  setFilter(_filterRLP, bq_type_lowpass, 20000.0);
  setFilter(_filterLHP, bq_type_highpass, 2.0);
  setFilter(_filterRHP, bq_type_highpass, 2.0);

  return true;
}

void DFRobot_MAX98357A::end(void)
{
  ESP_ERROR_CHECK(esp_avrc_ct_deinit());   // destroy AVRCP
  ESP_ERROR_CHECK(esp_a2d_sink_deinit());   // destroy A2DP
  ESP_ERROR_CHECK(esp_bluedroid_disable());   // stop & destroy bluetooth
  ESP_ERROR_CHECK(esp_bluedroid_deinit());
  btStop();
  releaseAudio();   // stop & destroy the selected I2S driver
}

bool DFRobot_MAX98357A::initI2S(int _bclk, int _lrclk, int _din)
{
#if ESP_IDF_VERSION_MAJOR >= 5
  if (!i2sMutex) i2sMutex = xSemaphoreCreateMutex();
  if (!i2sMutex) return initializationOK("I2S mutex allocation", ESP_ERR_NO_MEM);
  xSemaphoreTake(i2sMutex, portMAX_DELAY);
  if (txChannel) {
    xSemaphoreGive(i2sMutex);
    return initializationOK("I2S channel already active", ESP_ERR_INVALID_STATE);
  }

  i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  channel.dma_desc_num = 4;
  channel.dma_frame_num = 400;
  channel.auto_clear = true;
  i2s_std_config_t config = {};
  config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(static_cast<uint32_t>(_sampleRate));
  config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  config.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  config.gpio_cfg.bclk = static_cast<gpio_num_t>(_bclk);
  config.gpio_cfg.ws = static_cast<gpio_num_t>(_lrclk);
  config.gpio_cfg.dout = static_cast<gpio_num_t>(_din);
  config.gpio_cfg.din = I2S_GPIO_UNUSED;

  const char *stage = "i2s_new_channel";
  esp_err_t err = i2s_new_channel(&channel, &txChannel, nullptr);
  if (err == ESP_OK) {
    stage = "i2s_channel_init_std_mode";
    err = i2s_channel_init_std_mode(txChannel, &config);
  }
  if (err == ESP_OK) {
    stage = "i2s_channel_enable";
    err = i2s_channel_enable(txChannel);
  }
  if (err != ESP_OK && txChannel) {
    i2s_del_channel(txChannel);
    txChannel = nullptr;
  }
  xSemaphoreGive(i2sMutex);
  return initializationOK(stage, err);
#else
  const i2s_config_t i2s_config = {
    .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),   // The main controller can transmit data but not receive.
    .sample_rate = _sampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,   // 16 bits per sample
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,   // 2-channels
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,   // I2S communication I2S Philips standard, data launch at second BCK
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,   // Interrupt level 1
    .dma_buf_count = 4,   // number of buffers, 128 max.
    .dma_buf_len = 400,   // size of each buffer, AVRC communication may be affected if the value is too high.
    .use_apll = false,   // For the application of a high precision clock, select the APLL_CLK clock source in the frequency range of 16 to 128 MHz. It's not the case here, so select false.
    .tx_desc_auto_clear = true
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = _bclk,   // Serial clock (SCK), aka bit clock (BCK)
    .ws_io_num = _lrclk,   // Word select (WS), i.e. command (channel) select, used to switch between left and right channel data
    .data_out_num = _din,   // Serial data signal (SD), used to transmit audio data in two's complement format
    .data_in_num = I2S_PIN_NO_CHANGE   // Not used
  };

  if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL)){
    DBG("Install and start I2S driver failed !");
    return false;
  }
  if (i2s_set_pin(I2S_NUM_0, &pin_config)){
    DBG("Set I2S pin number failed !");
    i2s_driver_uninstall(I2S_NUM_0);
    return false;
  }

  return true;
#endif
}

bool DFRobot_MAX98357A::initBluetooth(const char * _btName)
{
  // Initialize bluedroid
  if (!btStarted() && !btStart()){
    Serial.println("[MAX98357A] Bluetooth controller start failed; check Bluetooth memory reservation and board/core selection.");
    return false;
  }
  esp_bluedroid_status_t bt_state = esp_bluedroid_get_status();
  if (bt_state == ESP_BLUEDROID_STATUS_UNINITIALIZED){
    if (!initializationOK("esp_bluedroid_init", esp_bluedroid_init())) {
      DBG("Initialize bluedroid failed !");
      return false;
    }
  }
  if (bt_state != ESP_BLUEDROID_STATUS_ENABLED){
    if (!initializationOK("esp_bluedroid_enable", esp_bluedroid_enable())) {
      DBG("Enable bluedroid failed !");
      return false;
    }
  }
  if (!initializationOK("esp_bt_dev_set_device_name", esp_bt_dev_set_device_name(_btName))){
    DBG("Set device name failed !");
    return false;
  }

  // Initialize AVRCP
  if (!initializationOK("esp_avrc_ct_init", esp_avrc_ct_init())){
    DBG("Initialize the bluetooth AVRCP controller module failed !");
    return false;
  }
  if (!initializationOK("esp_avrc_ct_register_callback", esp_avrc_ct_register_callback(avrcCallback))){
    DBG("Register application callbacks to AVRCP module failed !");
    return false;
  }

  // Initialize A2DP
  if (!initializationOK("esp_a2d_sink_init", esp_a2d_sink_init())){
    DBG("Initialize the bluetooth A2DP sink module failed !");
    return false;
  }
  if (!initializationOK("esp_a2d_register_callback", esp_a2d_register_callback(a2dpCallback))){
    DBG("Register application callbacks to A2DP module failed !");
    return false;
  }
  if (!initializationOK("esp_a2d_sink_register_data_callback", esp_a2d_sink_register_data_callback(audioDataProcessCallback))){
    DBG("Register A2DP sink data output function failed !");
    return false;
  }

  // Set discoverability and connectability mode for legacy bluetooth.
  if (!initializationOK("esp_bt_gap_set_scan_mode", esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE))){
    DBG("Set discoverability and connectability mode for legacy bluetooth failed !");
    return false;
  }
  _voiceSource = MAX98357A_VOICE_FROM_BT;

  return true;
}

bool DFRobot_MAX98357A::initSDCard(uint8_t csPin)
{
  if(!SD.begin(csPin)){
    DBG("Card Mount Failed");
    return false;
  }

  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE){
    DBG("No SD card attached");
    return false;
  }

  DBG("SD Card Type: ");
  if(cardType == CARD_MMC){
    DBG("MMC");
  } else if(cardType == CARD_SD){
    DBG("SDSC");
  } else if(cardType == CARD_SDHC){
    DBG("SDHC");
  } else {
    DBG("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  DBG("SD Card Size: ");
  DBG(cardSize);
  // Serial.printf("SD Card Size: %lluMB\n", cardSize);

  _voiceSource = MAX98357A_VOICE_FROM_SD;

  SDAmplifierMark = SD_AMPLIFIER_STOP;
  xTaskCreate(&playWAV, "playWAV", 2048, NULL, 5, &xPlayWAV);

  return true;
}

/*************************** Function ******************************/

void DFRobot_MAX98357A::reverseLeftRightChannels(void)
{
  _voiceSource = (_voiceSource ? MAX98357A_VOICE_FROM_SD : MAX98357A_VOICE_FROM_BT);
}

void DFRobot_MAX98357A::listDir(fs::FS &fs, const char * dirName)
{
  DBG(dirName);
  DBG("|");

  File root = fs.open(dirName);
  if(!root){
    DBG("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    DBG("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while(file){
    // DBG(file.name());
    if(file.isDirectory()){
      listDir(fs, file.path());
    } else {
      DBG(file.path());
      if(strstr(file.name(), ".wav")){
        _musicList[musicCount] = file.path();
        musicCount++;
      }
    }
    file = root.openNextFile();
  }
}

void DFRobot_MAX98357A::scanSDMusic(String * musicList)
{
  musicCount = 0;   // Discard original list
  listDir(SD, "/");
  uint8_t i = 0;
  while(i < musicCount){
    musicList[i] = _musicList[i];
    i++;
  }

  // Set playing music by default
  char SDName[80]="/sd";
  strcat(SDName, musicList[0].c_str());
  strcpy(fileName, SDName);
}

void DFRobot_MAX98357A::playSDMusic(const char *musicName)
{
  SDPlayerControl(SD_AMPLIFIER_STOP);
  char SDName[80]="/sd";   // The default SD card mount point in SD.h
  strcat(SDName, musicName);   // It need to be an absolute path.
  strcpy(fileName, SDName);
  SDPlayerControl(SD_AMPLIFIER_PLAY);
}

void DFRobot_MAX98357A::SDPlayerControl(uint8_t CMD)
{
  SDAmplifierMark = CMD;
  delay(10);   // Wait music playback to stop
}

String DFRobot_MAX98357A::getMetadata(uint8_t type)
{
  _metadata = "";
  if(_avrcConnected){
    esp_avrc_ct_send_metadata_cmd(type, type);   // Request metadata from remote Bluetooth device via AVRC command
    for(uint8_t i=0; i<20; i++){   // Wait response
      if(0 != _metaFlag){
        break;
      }
      delay(100);
    }
    _metaFlag = 0;
  }
  return _metadata;
}

uint8_t * DFRobot_MAX98357A::getRemoteAddress(void)
{
  if(!_avrcConnected){   // Bluetooth AVRC is not connected
    return NULL;
  }
  return remoteAddress;
}

void DFRobot_MAX98357A::setVolume(float vol)
{
  vol /= 5.0;   // vol range is 0-9
  _volume = constrain(vol, 0.0, 2.0);
}

void DFRobot_MAX98357A::openFilter(int type, float fc)
{
  if(bq_type_lowpass == type){   // Set low-pass filter
    setFilter(_filterLLP, type, fc);
    setFilter(_filterRLP, type, fc);
  }else{   // Set high-pass filter
    setFilter(_filterLHP, type, fc);
    setFilter(_filterRHP, type, fc);
  }
  _filterFlag = true;
}

void DFRobot_MAX98357A::closeFilter(void)
{
  _filterFlag = false;
}

void DFRobot_MAX98357A::setFilter(Biquad * _filter, int _type, float _fc)
{
  _fc = (constrain(_fc, 2.0, 20000.0)) / (float)_sampleRate;   // Ratio of filter threshold to sampling frequency, range: 0.0-0.5
  float Q;
  for(int i = 0; i<NUMBER_OF_FILTER; i++){
    Q = 1 / (2 * cos( PI / (NUMBER_OF_FILTER * 4) + i * PI / (NUMBER_OF_FILTER * 2) ));
    DBG("\n-------- Q ");
    DBG(Q);
    DBG("++++++++ _fc ");
    DBG(_fc);
    DBG("++++++++ _type ");
    DBG(_type);
    _filter[i].setBiquad(_type, _fc, Q, 0);
  }
}

int16_t DFRobot_MAX98357A::filterToWork(Biquad * filterHP, Biquad * filterLP, float rawData)
{
  for(int i = 0; i<NUMBER_OF_FILTER; i++){
    rawData = filterLP[i].process(rawData);
  }

  for(int i = 0; i<NUMBER_OF_FILTER; i++){
    rawData = filterHP[i].process(rawData);
  }

  return (int16_t)(constrain(rawData, -32767, 32767));
}

/*************************** Function ******************************/

void DFRobot_MAX98357A::a2dpCallback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t*param)
{
  esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *)(param);

  switch (event) {
    /*!<
     * Audio codec is configured, only used for A2DP SINK
     *
     * @brief ESP_A2D_AUDIO_CFG_EVT
     *
     * struct a2d_audio_cfg_param {
     *     esp_bd_addr_t remote_bda;              /*!< remote bluetooth device address
     *     esp_a2d_mcc_t mcc;                     /*!< A2DP media codec capability information
     * } audio_cfg;                               /*!< media codec configuration information
     */
    case ESP_A2D_AUDIO_CFG_EVT:
    /*!<
     * Connection state changed event
     *
     * @brief  ESP_A2D_CONNECTION_STATE_EVT
     *
     * struct a2d_conn_stat_param {
     *     esp_a2d_connection_state_t state;      /*!< one of values from esp_a2d_connection_state_t
     *     esp_bd_addr_t remote_bda;              /*!< remote bluetooth device address
     *     esp_a2d_disc_rsn_t disc_rsn;           /*!< reason of disconnection for "DISCONNECTED"
     * } conn_stat;                               /*!< A2DP connection status
     */
    case ESP_A2D_CONNECTION_STATE_EVT:
    /*!< audio stream transmission state changed event */
    case ESP_A2D_AUDIO_STATE_EVT:
    /*!< acknowledge event in response to media control commands */
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
    /*!< indicate a2dp init&deinit complete */
    case ESP_A2D_PROF_STATE_EVT:
      break;
    default:
      // "a2dp invalid cb event: %d", event
      break;
  }
}

void DFRobot_MAX98357A::avrcCallback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param)
{
  esp_avrc_ct_cb_param_t *rc = (esp_avrc_ct_cb_param_t *)(param);

  switch (event) {
    /*!< metadata response event */
    case ESP_AVRC_CT_METADATA_RSP_EVT: {
        char *attr_text = (char *) malloc (rc->meta_rsp.attr_length + 1);
        memcpy(attr_text, rc->meta_rsp.attr_text, rc->meta_rsp.attr_length);
        attr_text[rc->meta_rsp.attr_length] = 0;
        _metadata = String(attr_text);
        _metaFlag = rc->meta_rsp.attr_id;
        DBG("_metadata");
        DBG(_metadata);
        DBG(rc->meta_rsp.attr_id);

        free(attr_text);
        break;
      }
    /*!< connection state changed event */
    case ESP_AVRC_CT_CONNECTION_STATE_EVT:{
        /*!< connection established */
        _avrcConnected = rc->conn_stat.connected;
        if(_avrcConnected){
          uint8_t * p = rc->conn_stat.remote_bda;
          for(uint8_t i=0; i<6; i++){
            remoteAddress[i] = *(p + i);
            DBG(remoteAddress[i], HEX);
          }
        /*!< disconnecting remote device */
        }else{
          DBG(sizeof(remoteAddress));
          memset(remoteAddress, 0, 6);
        }
        break;
      }
    /*!< passthrough response event */
    case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
    /*!< notification event */
    case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
    /*!< feature of remote device indication event */
    case ESP_AVRC_CT_REMOTE_FEATURES_EVT:
    /*!< supported notification events capability of peer device */
    case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT:
    /*!< play status response event */
    case ESP_AVRC_CT_PLAY_STATUS_RSP_EVT:
    /*!< set absolute volume response event */
    case ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT:
      break;
    default:
      // "unhandled AVRC event: %d", event
      break;
  }
}

void DFRobot_MAX98357A::audioDataProcessCallback(const uint8_t *data, uint32_t len)
{
  int16_t* data16 = (int16_t*)data;   // Convert to 16-bit sample data
  int16_t processedData[2];   // Store the processed audio data
  int count = len / 4;   // The number of audio data to be processed in int16_t[2]
  size_t i2s_bytes_write = 0;   // i2s_write() the variable storing the number of data to be written

  if(!_filterFlag){   // Change sample data only according to volume multiplier
    for(int i=0; i<count; i++){
      processedData[0+_voiceSource] = (int16_t)((*data16) * _volume);   // Change audio data volume of left channel
      DBG(processedData[0], HEX);
      data16++;

      processedData[1-_voiceSource] = (int16_t)((*data16) * _volume);   // Change audio data volume of right channel
      DBG(processedData[1], HEX);
      data16++;

      writeAudio(processedData, 4, &i2s_bytes_write, 20);   // Transfer audio data to the amplifier via I2S
    }
  }else{   // Filtering with a simple digital filter
    for(int i=0; i<count; i++){
      processedData[0+_voiceSource] = filterToWork(_filterLHP, _filterLLP, ((*data16) * _volume));   // Change audio data volume of left channel, and perform filtering operation
      data16++;

      processedData[1-_voiceSource] = filterToWork(_filterRHP, _filterRLP, ((*data16) * _volume));   // Change audio data volume of right channel, and perform filtering operation
      data16++;

      writeAudio(processedData, 4, &i2s_bytes_write, 100);   // Transfer audio data to the amplifier via I2S
    }
  }
}

void DFRobot_MAX98357A::playWAV(void *arg)
{
  while(1){
    while(SD_AMPLIFIER_STOP == SDAmplifierMark){
      vTaskDelay(100);
    }

    sWavInfo_t * wav = (sWavInfo_t *)calloc(1, sizeof(sWavInfo_t));
    if(wav == NULL){
      DBG("Unable to allocate WAV struct.");
      SDAmplifierMark = SD_AMPLIFIER_STOP;
      continue;
    }

    wav->fp = fopen(fileName, "rb");
    if(wav->fp == NULL){
      DBG("Unable to open wav file.");
      DBG(fileName);
      SDAmplifierMark = SD_AMPLIFIER_STOP;
      continue;
    }
    if(fread(&(wav->header.riffType), 1, 4, wav->fp) != 4){
      DBG("couldn't read RIFF_ID.");
      SDAmplifierMark = SD_AMPLIFIER_STOP;
      continue;  /* bad error "couldn't read RIFF_ID" */
    }
    if(strncmp("RIFF", wav->header.riffType, 4)){
      DBG("RIFF descriptor not found.") ;
      SDAmplifierMark = SD_AMPLIFIER_STOP;
      continue;
    }
    fread(&(wav->header.riffSize), 4, 1, wav->fp);
    if(fread(&wav->header.waveType, 1, 4, wav->fp) != 4){
      DBG("couldn't read format");
      SDAmplifierMark = SD_AMPLIFIER_STOP;
      continue;  /* bad error "couldn't read format" */
    }
    if(strncmp("WAVE", wav->header.waveType, 4)){
      DBG("WAVE chunk ID not found.") ;
      SDAmplifierMark = SD_AMPLIFIER_STOP;
      continue;
    }
    if(fread(&(wav->header.formatType), 1, 4, wav->fp) != 4){
      DBG("couldn't read format_ID");
      SDAmplifierMark = SD_AMPLIFIER_STOP;
      continue;  /* bad error "couldn't read format_ID" */
    }
    if(strncmp("fmt", wav->header.formatType, 3)){
      DBG("fmt chunk format not found.");
      SDAmplifierMark = SD_AMPLIFIER_STOP;
      continue;
    }
    fread(&(wav->header.formatSize), 4, 1, wav->fp);
    fread(&(wav->header.compressionCode), 2, 1, wav->fp);
    fread(&(wav->header.numChannels), 2, 1, wav->fp);
    fread(&(wav->header.sampleRate), 4, 1, wav->fp);
    fread(&(wav->header.bytesPerSecond), 4, 1, wav->fp);
    fread(&(wav->header.blockAlign), 2, 1, wav->fp);
    fread(&(wav->header.bitsPerSample), 2, 1, wav->fp);
    while(1){
      if(fread(&wav->header.dataType1, 1, 1, wav->fp) != 1){
        DBG("Unable to read data chunk ID.");
        free(wav);
        break;
      }
      if(strncmp("d", wav->header.dataType1, 1) == 0){
        fread(&wav->header.dataType2, 3, 1, wav->fp);
        if(strncmp("ata", wav->header.dataType2, 3) == 0){
          fread(&(wav->header.dataSize),4,1,wav->fp);
          break;
        }
      }
    }

    // Set I2S sampling rate from the WAV header.
    if (setAudioSampleRate(wav->header.sampleRate) != ESP_OK) {
      DBG("Set I2S sample rate failed !");
      fclose(wav->fp);
      free(wav);
      SDAmplifierMark = SD_AMPLIFIER_STOP;
      continue;
    }

    while(fread(&wav->header.data, 1 , 800 , wav->fp)){
      audioDataProcessCallback((uint8_t *)&wav->header.data, 800);   // Send the parsed audio data to the amplifier broadcast function
      if(SD_AMPLIFIER_STOP == SDAmplifierMark){
        break;
      }
      while(SD_AMPLIFIER_PAUSE == SDAmplifierMark){
        vTaskDelay(100);
      }
    }

    fclose(wav->fp);
    free(wav);
    SDAmplifierMark = SD_AMPLIFIER_STOP;
    vTaskDelay(100);
  }
  vTaskDelete(xPlayWAV);
}
