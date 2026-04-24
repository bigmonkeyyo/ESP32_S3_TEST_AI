# DNESP32S3B 硬件引脚对照表（V1.0）

本文基于以下资料整理：
- `DNESP32S3B开发板 V1.0 硬件参考手册.pdf` 的 **1.2.2 节 / 表 1.2.2.1**（PDF 第 6~7 页）
- 本工程 BSP 宏定义：`components/BSP/LCD/lcd.h`、`components/BSP/IIC/iic.h`、`components/BSP/LED/led.h`、`components/BSP/XL9555/xl9555.h`

## 1. 本工程常用引脚速查（先看这个）

| 功能 | 引脚 |
| --- | --- |
| 板载蓝色 LED | `GPIO4`（`LED_GPIO_PIN`） |
| I2C0（触摸/扩展等） | `SDA=GPIO48`，`SCL=GPIO45` |
| I2C 扩展中断（XL9555 INT） | `GPIO3` |
| LCD 控制 | `CS=GPIO1`，`RS/DC=GPIO2`，`RD=GPIO41`，`WR=GPIO42` |
| LCD 数据线（8bit） | `D0=GPIO40`，`D1=GPIO39`，`D2=GPIO38`，`D3=GPIO12`，`D4=GPIO11`，`D5=GPIO10`，`D6=GPIO9`，`D7=GPIO46` |
| USB_Slave/JTAG | `GPIO19(USB_D-)`，`GPIO20(USB_D+)` |
| 音频 I2S | `LRCK=GPIO13`，`SDOUT=GPIO14`，`BCK=GPIO21`，`SDIN=GPIO47` |
| BOOT 键 | `GPIO0` |
| 串口0 | `RXD0`，`TXD0` |
| 手册标注勿用 | `GPIO35`、`GPIO36`、`GPIO37` |

## 2. 手册原表整理（表 1.2.2.1）

> 说明：
> - 手册本表从引脚标号 `4` 开始，`1~3` 未列在该 GPIO 资源表中。
> - `完全独立` 列中：`Y`=可做独立 IO，`N`=板上有外设连接。

| 引脚标号 | GPIO | 连接资源1 | 连接资源2 | 完全独立 | 连接关系 |
| --- | --- | --- | --- | --- | --- |
| 4 | IO4 | LED0 | OV_D0 | N | RGBLCD 的 DE 信号 / 摄像头 D0 信号 / LED 信号 |
| 5 | IO5 | - | - | Y | 该引脚完全独立 |
| 6 | IO6 | - | - | Y | 该引脚完全独立 |
| 7 | IO7 | SPI_SCK | - | N | SPI2 口 SCK |
| 8 | IO15 | SPI_MISO | - | N | SPI2 口 MISO |
| 9 | IO16 | SPI_MOSI | - | N | SPI2 口 MOSI |
| 10 | IO17 | TF_CS | - | N | 触摸 IC 的 CS 信号 |
| 11 | IO18 | - | - | Y | 该引脚完全独立 |
| 12 | IO8 | - | - | Y | 该引脚完全独立 |
| 13 | IO19 | USB_D- | - | N | USB D- 信号 |
| 14 | IO20 | USB_D+ | - | N | USB D+ 信号 |
| 15 | IO3 | IIC_INT | - | N | XL9555 的 INT 信号 |
| 16 | IO46 | LCD_D7 | - | N | LCD D7 |
| 17 | IO9 | LCD_D6 | - | N | LCD D6 |
| 18 | IO10 | LCD_D5 | - | N | LCD D5 |
| 19 | IO11 | LCD_D4 | - | N | LCD D4 |
| 20 | IO12 | LCD_D3 | - | N | LCD D3 |
| 21 | IO13 | IIS_LRCK | - | N | 音频 LRCK |
| 22 | IO14 | IIS_SDOUT | - | N | 音频 SDOUT |
| 23 | IO21 | IIS_BCK | - | N | 音频 BCK |
| 24 | IO47 | IIS_SDIN | - | N | MIC SDIN |
| 25 | IO48 | CTP_SDA | IIC_SDA | N | 触摸 IC SDA / IIC0 SDA |
| 26 | IO45 | CTP_SCL | IIC_SCL | N | 触摸 IC SCL / IIC0 SCL |
| 27 | IO0 | BOOT | - | N | BOOT 按键信号 |
| 28 | IO35 | - | - | Y | 勿用 |
| 29 | IO36 | - | - | Y | 勿用 |
| 30 | IO37 | - | - | Y | 勿用 |
| 31 | IO38 | LCD_D2 | - | N | LCD D2 |
| 32 | IO39 | LCD_D1 | - | N | LCD D1 |
| 33 | IO40 | LCD_D0 | - | N | LCD D0 |
| 34 | IO41 | IIC_SDA | - | N | IIC0 SDA |
| 35 | IO42 | IIC_SCL | - | N | IIC0 SCL |
| 36 | RXD0 | U0RXD | - | N | 串口 RX |
| 37 | TXD0 | U0TXD | - | N | 串口 TX |
| 38 | IO2 | LCD_RS | - | N | LCD RS |
| 39 | IO1 | LCD_CS | - | N | LCD CS |

## 3. 我补充的使用建议（结合本工程）

1. `GPIO4` 在手册里是复用位（`LED0/OV_D0/RGBLCD_DE`），本工程把它用于板载 LED，若后续启用摄像头或 RGBLCD 需重新检查复用冲突。
2. `GPIO45/48` 在手册中同时标注为触摸和 IIC0，本工程 `iic.h` 也是按 I2C0 用这两脚配置，默认不要再复用到别的外设。
3. `GPIO41/42` 在手册表中标为 `IIC0 SDA/SCL`，但本工程 `lcd.h` 里配置为 LCD `RD/WR`。如果后面新增功能要占用这两脚，建议先对照原理图核对板级连线后再改。
