
I recommend using this library instead of my own: https://github.com/DavidAntliff/esp32-ds18b20, it does everything this library does, plus more!

----

# Not so simple library for multiple DS18B20 on ESP32

## Resources

### DS18B20 datasheet

https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf

### 1-Wire communication through software

https://www.maximintegrated.com/en/app-notes/index.mvp/id/126

### 1-Wire search algorithm

https://www.maximintegrated.com/en/app-notes/index.mvp/id/187

### CPP implementation

https://github.com/PaulStoffregen/OneWire
https://github.com/milesburton/Arduino-Temperature-Control-Library

## Setup

From the root directory of your ESP-IDF project

```
git clone git@github.com:brettanomyces/ds18b20.git
echo "COMPONENT_ADD_INCLUDEDIRS := ../components/ds18b20" > main/components.mk
```

## Usage

```
// main/main.c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ds18b20.h"

uint8_t pin = 27;
uint8_t addr1[] = {0x28, 0xff, 0xa5, 0xa0, 0x68, 0x14, 0x04, 0x36 };
uint8_t addr2[] = {0x28, 0xff, 0xb7, 0xa0, 0x68, 0x14, 0x04, 0xc9 };

void mainTask(void * pvParameters){
  ow_init(pin);

  uint8_t count, result, i;

  while (1) {
    // display addr of all connected devices
    count = 0;
    result = ow_first();
    while (result)
    {
      printf("addr%d: ", ++count);

      // print addr bytes in hex
      for (i = 0; i < 8; i++) {
        printf("0x%02x ", ROM_NO[i]);
      }
      printf("\n");

      result = ow_next();
    }

    // get temp by addr
    printf("temp1: %0.3f\n", ds_get_temp(addr1));
    printf("temp2: %0.3f\n", ds_get_temp(addr2));

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void app_main()
{
  xTaskCreatePinnedToCore(&mainTask, "mainTask", 2048, NULL, 5, NULL, 0);
}
```
