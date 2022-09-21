# DFRobot_MAX98357A
* [English Version](./README.md)

这是一个I2S功放模块。体积小巧，使用简单。可快速的将ESP32主控做成蓝牙音箱、或者WIFI对讲机，
语音识别、网络播报设备等。也可以加上SD卡模块播放SD卡音乐。

![产品实物图](./resources/images/MAX98357A.png)


## 产品链接 (https://www.dfrobot.com.cn/goods-3573.html)
    SKU: DFR0954


## 目录

* [概述](#概述)
* [库安装](#库安装)
* [方法](#方法)
* [兼容性](#兼容性)
* [历史](#历史)
* [创作者](#创作者)


## 概述

* 可直插，可贴片。小巧方便。
* 板载I2S功放，3W功率音质好。


## 库安装

要使用这个库, 首先下载库文件, 将其粘贴到\Arduino\libraries目录中, 然后打开示例文件夹并在文件夹中运行演示。


## 方法

```C++

  /**
   * @fn begin
   * @brief Init function
   * @param btName - 创建的蓝牙设备名
   * @param bclk - I2S通信引脚号, 串行时钟SCK, 也叫位时钟（BCK）
   * @param lrclk - I2S通信引脚号, 帧时钟WS, 即命令（声道）选择，用于切换左右声道的数据
   * @param din - I2S通信引脚号, 串行数据信号SD, 用于传输二进制补码表示的音频数据
   * @return true on success, false on error
   */
  bool begin(const char *btName="bluetoothAmplifier", 
             int bclk=GPIO_NUM_25, 
             int lrclk=GPIO_NUM_26, 
             int din=GPIO_NUM_27);

  /**
   * @fn initI2S
   * @brief Initialize I2S
   * @param _bclk - I2S通信引脚号, 串行时钟SCK, 也叫位时钟（BCK）
   * @param _lrclk - I2S通信引脚号, 帧时钟WS, 即命令（声道）选择，用于切换左右声道的数据
   * @param _din - I2S通信引脚号, 串行数据信号SD, 用于传输二进制补码表示的音频数据
   * @return true on success, false on error
   */
  bool initI2S(int _bclk, int _lrclk, int _din);

  /**
   * @fn initBluetooth
   * @brief Initialize bluetooth
   * @param _btName - 创建的蓝牙设备名
   * @return true on success, false on error
   */
  bool initBluetooth(const char * _btName);

  /**
   * @fn initSDCard
   * @brief Initialize SD card
   * @param csPin SD卡模块的spi通信的cs引脚号
   * @return true on success, false on error
   */
  bool initSDCard(uint8_t csPin=GPIO_NUM_5);

/*************************** 功能函数 ******************************/

  /**
   * @fn scanSDMusic
   * @brief 扫描SD卡里面的WAV格式的音乐文件
   * @param musicList - SD卡里面扫描到的WAV格式的音乐文件, 类型是字符串数组
   * @return None
   * @note 音乐文件路径名字当前仅支持英文, 格式当前仅支持WAV格式的音乐文件
   */
  void scanSDMusic(String * musicList);

  /**
   * @fn playSDMusic
   * @brief 播放SD卡里面的音频文件
   * @param Filename - 音乐文件名, 当前仅支持 .wav 格式的音频文件
   * @note 音乐文件名需为绝对路径, 列如: /musicDir/music.wav
   * @return None
   * @note 音乐文件路径名字当前仅支持英文, 格式当前仅支持WAV格式的音乐文件
   */
  void playSDMusic(const char *Filename);

  /**
   * @fn SDPlayerControl
   * @brief SD卡音频播放控制接口
   * @param CMD - 播放控制命令: 
   * @n SD_AMPLIFIER_PLAY: 开始播放音乐, 可从之前暂停播放的位置继续播放
   * @n   若没有通过playSDMusic()选择播放的音乐文件, 会默认播放音乐列表第一首
   * @n   若SD卡没有扫描到正确格式的音乐文件也会播放失败(音乐文件路径名字当前仅支持英文, 格式当前仅支持WAV格式的音乐文件)
   * @n SD_AMPLIFIER_PAUSE: 暂停播放, 保留当前音乐文件的播放位置
   * @n SD_AMPLIFIER_STOP: 停止播放, 结束当前音乐的播放
   * @return None
   */
  void SDPlayerControl(uint8_t CMD);

  /**
   * @fn getMetadata
   * @brief 通过 AVRC 命令获取"诠释数据"(metadata)
   * @param type - 需要获取的元数据的类型, 目前支持的参数: 
   * @n     ESP_AVRC_MD_ATTR_TITLE   ESP_AVRC_MD_ATTR_ARTIST   ESP_AVRC_MD_ATTR_ALBUM
   * @return 相应类型的 "元数据"
   */
  String getMetadata(uint8_t type);

  /**
   * @fn getRemoteAddress
   * @brief 获取远程蓝牙设备地址
   * @note 地址将在本机与远程蓝牙设备配对连接后, 成功进行蓝牙AVRCP协议通信后获取
   * @return 返回存储远程蓝牙设备地址的数组指针
   * @n 当未连接远程设备, 或和远程设备进行蓝牙AVRCP协议通信不成功时, 返回None
   * @n AVRCP(Audio Video Remote Control Profile)
   */
  uint8_t * getRemoteAddress(void);

  /**
   * @fn setVolume
   * @brief 设置音量
   * @param vol - 设置音量, 可以设置范围 0 ~ 9
   * @note 5 即为音频数据原始音量, 无增减
   * @return None
   */
  void setVolume(float vol);

  /**
   * @fn openFilter
   * @brief 打开音频滤波器
   * @param type - bq_type_highpass: 开启高通滤波; bq_type_lowpass: 开启低通滤波
   * @param fc - 过滤波的阈值, 范围: 2~20000
   * @note 列如, 设置高通滤波模式, 阈值为500, 即为过滤掉音频信号中低于500的信号; 且高通滤波和低通滤波会同时工作
   * @return None
   */
  void openFilter(int type, float fc);

  /**
   * @fn closeFilter
   * @brief 关闭音频滤波器
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

```


## 兼容性

MCU                | Work Well    | Work Wrong   | Untested    | Remarks
------------------ | :----------: | :----------: | :---------: | :----:
FireBeetle-ESP32   |      √       |              |             |


## 历史

- 2022/02/07 - 1.0.0 版本
- 2022/09/21 - 1.0.1 版本


## 创作者

Written by qsjhyy(yihuan.huang@dfrobot.com), 2022. (Welcome to our [website](https://www.dfrobot.com/))

