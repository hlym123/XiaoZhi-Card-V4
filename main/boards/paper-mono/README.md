# Paper Mono

WiFi 版 M5Stack PaperMono，基于 [PaperMono 参考资料](../../../m5stack_dev/product/others/PaperMono/develop) 实现。

## 硬件规格（参考 m5stack_paper_mono/README.md）

- **MCU**: ESP32-S3
- **PSRAM**: 8MB
- **Flash**: 16MB
- **Display**: 480×800 SPI EPD GDEM0397T81P

### 引脚

| 功能 | 引脚 |
|------|------|
| **I2C1** | SCL G48, SDA G47 |
| **PMIC M5PM1** | I2C1 @0x6E |
| **IO Expander M5IOE1** | I2C1 @0x6F |
| **EPD** | CS G16, CLK G15, MOSI G14, DC G17, BUSY G18 |
| **EPD PWR_EN** | M5IOE1_G3 (高有效) |
| **EPD RST** | M5IOE1_G5 |
| **EPD 背光** | M5PM1_G3 (PWM) |
| **Touch FT5X06** | I2C1, PWR_EN M5IOE1_G13, INT G4, RST M5IOE1_G6 |
| **按键** | KEY1 G2, KEY2 G3 |
| **Audio** | PDM MIC LMD4737T261, CLK G45, DAT G46（暂用 dummy_audio_codec） |

### 与 XiaoZhi-Card 的区别

- **仅 WiFi**：无 4G/ML307，非 DualNetworkBoard
- **不同显示屏**：GDEM0397T81P 480×800（非 GDEY027T91 176×264）
- **不同电源架构**：M5PM1 + M5IOE1（无 BQ27220、AW32001）
- **音频**：暂用 dummy_audio_codec

## 构建

```bash
idf.py set-target esp32s3
idf.py menuconfig   # Board Type 选择 "Paper Mono (WiFi only)"
idf.py build
```

## 参考资料

- `~/m5stack_dev/product/others/PaperMono/develop/src/components/m5stack_paper_mono/README.md`
- `~/m5stack_dev/product/others/PaperMono/README.md`
