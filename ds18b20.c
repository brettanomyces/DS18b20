/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"

// timings for standard speed
#define DELAY_US_A 6
#define DELAY_US_B 64
#define DELAY_US_C 60
#define DELAY_US_D 10
#define DELAY_US_E 9
#define DELAY_US_F 55
#define DELAY_US_G 0
#define DELAY_US_H 450
#define DELAY_US_I 70
#define DELAY_US_J 410

// ROM commands
#define SEARCH_ROM 0xF0
#define READ_ROM 0x33
#define MATCH_ROM 0x55
#define SKIP_ROM 0xCC
#define ALARM_SEARCH 0xEC

// DS18B20 function commands
#define CONVERT 0x44
#define WRITE_SCRATCHPAD 0x4E
#define READ_SCRATCHPAD 0xBE
#define COPY_SCRATCHPAD 0x48
#define RECALL 0xB8
#define READ_POWER_SUPPLY 0xB4

uint8_t ROM_NO[8];
uint8_t OW_GPIO;
uint8_t last_discrepancy;
uint8_t last_family_discrepancy;
uint8_t last_device_flag;
uint8_t crc8;

static uint8_t dscrc_table[] = {
        0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
      157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
       35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
      190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
       70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
      219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
      101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
      248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
      140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
       17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
      175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
       50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
      202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
       87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
      233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
      116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53
};

uint8_t docrc8(uint8_t value)
{
   crc8 = dscrc_table[crc8 ^ value];
   return crc8;
}

// send a 1-wire write bit. provide 10us recovery time
void ow_write_bit(uint8_t bit)
{
  gpio_set_direction(OW_GPIO, GPIO_MODE_OUTPUT);

  if (bit) {
    // write '1' bit
    gpio_set_level(OW_GPIO, 0);  // drives dq low
    ets_delay_us(DELAY_US_A);

    gpio_set_level(OW_GPIO, 1);  // releases the bus
    ets_delay_us(DELAY_US_B);  // complete the time slot and 10us recovery
  } else {
    // write '0' bit
    gpio_set_level(OW_GPIO, 0);  // drives dq low
    ets_delay_us(DELAY_US_C);
    
    gpio_set_level(OW_GPIO, 1);  // releases the bus
    ets_delay_us(DELAY_US_D);
  }
}

// read a bit from the 1-wire bus and return it. provide a 10us recovery time
uint8_t ow_read_bit(void)
{
  uint8_t result;

  gpio_set_direction(OW_GPIO, GPIO_MODE_OUTPUT);

  gpio_set_level(OW_GPIO, 0);  // drives dq low
  ets_delay_us(DELAY_US_A);

  gpio_set_level(OW_GPIO, 1);  // releases the bus
  ets_delay_us(DELAY_US_E);

  gpio_set_direction(OW_GPIO, GPIO_MODE_INPUT);
  result = gpio_get_level(OW_GPIO) & 0x01;  // sample the bit value from the slave

  ets_delay_us(DELAY_US_F);  // complete the time slot and 10us recovery

  return result;
}

// wire 1-wire data byte
void ow_write_byte(uint8_t byte)
{
  uint8_t i;

  // loop to write each bit in byte, ls-bit first
  for (i = 0; i < 8; i++) {
    ow_write_bit(byte & 0x01);

    // shift the data byte for the next bit
    byte >>= 1;
  }
}

// read 1-wire data byte and return it
uint8_t ow_read_byte(void)
{
  uint8_t i;
  uint8_t byte = 0;

  for (i = 0; i < 8; i++) {
    // shift the result to get it ready for the next bit
    byte >>= 1;

    // if the result is one, then set MS bit
    if (ow_read_bit()) {
      byte |= 0x80;
    }
  }
  return byte;
}

// sends reset pulse
uint8_t ow_touch_reset(void)
{
  uint8_t result;

  gpio_set_direction(OW_GPIO, GPIO_MODE_OUTPUT);

  ets_delay_us(DELAY_US_G);
  gpio_set_level(OW_GPIO,0);  // drives dq low

  ets_delay_us(DELAY_US_H);
  gpio_set_level(OW_GPIO,1);  // releases the bus

  gpio_set_direction(OW_GPIO, GPIO_MODE_INPUT);
  ets_delay_us(DELAY_US_I);

  result = gpio_get_level(OW_GPIO) ^ 0x01;  // sample for presence pulse from slave

  ets_delay_us(DELAY_US_J);  // complete the reset sequence recovery

  return result;  // return sample presence pulse result
}

// write a 1-wire data byte and return the sampled result
uint8_t ow_touch_byte(uint8_t byte)
{
  uint8_t i;
  uint8_t result = 0;

  for (i = 0; i < 8; i++) {
    // shift the result to get it ready for the next bit
    result >>= 1;

    // if sending a '1' then read a bit, else write a '0'
    if (byte & 0x01) {
      if (ow_read_bit()) {
        result |= 0x80;
      }
    } else {
      ow_write_bit(0);
    }

    // shift the data byte for the next bit
    byte >>= 1;
  }
  return result;
}

// do a rom select
void ow_select(uint8_t rom[8])
{
  uint8_t i;

  ow_write_byte(MATCH_ROM);

  for (i = 0; i < 8; i++) {
    ow_write_byte(rom[i]);
  }
}

// do a rom skip
void ow_skip(void)
{
  ow_write_byte(SKIP_ROM);
}

uint8_t ow_search() 
{
   int id_bit_number;
   int last_zero, rom_byte_number, search_result;
   int id_bit, cmp_id_bit;
   unsigned char rom_byte_mask, search_direction;

   // initialize for search
   id_bit_number = 1;
   last_zero = 0;
   rom_byte_number = 0;
   rom_byte_mask = 1;
   search_result = 0;
   crc8 = 0;

   // if the last call was not the last one
   if (!last_device_flag)
   {
      // 1-Wire reset
      if (!ow_touch_reset())
      {
         // reset the search
         last_discrepancy = 0;
         last_device_flag = 0;
         last_family_discrepancy = 0;
         return 0;
      }

      // issue the search command 
      ow_write_byte(SEARCH_ROM);  

      // loop to do the search
      do
      {
         // read a bit and its complement
         id_bit = ow_read_bit();
         cmp_id_bit = ow_read_bit();

         // check for no devices on 1-wire
         if ((id_bit == 1) && (cmp_id_bit == 1))
            break;
         else
         {
            // all devices coupled have 0 or 1
            if (id_bit != cmp_id_bit)
               search_direction = id_bit;  // bit write value for search
            else
            {
               // if this discrepancy if before the Last Discrepancy
               // on a previous next then pick the same as last time
               if (id_bit_number < last_discrepancy)
                  search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
               else
                  // if equal to last pick 1, if not then pick 0
                  search_direction = (id_bit_number == last_discrepancy);

               // if 0 was picked then record its position in LastZero
               if (search_direction == 0)
               {
                  last_zero = id_bit_number;

                  // check for Last discrepancy in family
                  if (last_zero < 9)
                     last_family_discrepancy = last_zero;
               }
            }

            // set or clear the bit in the ROM byte rom_byte_number
            // with mask rom_byte_mask
            if (search_direction == 1)
              ROM_NO[rom_byte_number] |= rom_byte_mask;
            else
              ROM_NO[rom_byte_number] &= ~rom_byte_mask;

            // serial number search direction write bit
            ow_write_bit(search_direction);

            // increment the byte counter id_bit_number
            // and shift the mask rom_byte_mask
            id_bit_number++;
            rom_byte_mask <<= 1;

            // if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
            if (rom_byte_mask == 0)
            {
                docrc8(ROM_NO[rom_byte_number]);  // accumulate the CRC
                rom_byte_number++;
                rom_byte_mask = 1;
            }
         }
      }
      while(rom_byte_number < 8);  // loop until through all ROM bytes 0-7

      // if the search was successful then
      if (!((id_bit_number < 65) || (crc8 != 0)))
      {
         // search successful so set last_discrepancy,last_device_flag,search_result
         last_discrepancy = last_zero;

         // check for last device
         if (last_discrepancy == 0)
            last_device_flag = 1;
         
         search_result = 1;
      }
   }

   // if no device found then reset counters so next 'search' will be like a first
   if (!search_result || !ROM_NO[0])
   {
      last_discrepancy = 0;
      last_device_flag = 0;
      last_family_discrepancy = 0;
      search_result = 0;
   }

   return search_result;
}

uint8_t ow_first()
{
  // reset the search state
  last_discrepancy = 0;
  last_device_flag = 0;
  last_family_discrepancy = 0;

  return ow_search();
}

uint8_t ow_next()
{
  // leave the search state alone
  return ow_search();
}

float ds_get_temp(uint8_t addr[])
{
  uint8_t t1, t2;
  float temp;

  ow_touch_reset();

  // convert
  ow_select(addr);
  ow_write_byte(CONVERT);
  vTaskDelay( 750 / portTICK_RATE_MS);  // wait for conversion
  ow_touch_reset();

  // read
  ow_select(addr);
  ow_write_byte(READ_SCRATCHPAD);
  t1 = ow_read_byte();
  t2 = ow_read_byte();
  ow_touch_reset();

  temp = (float) (t1 + (t2 * 256)) / 16;
  return temp;
}

void ow_init(uint8_t pin) 
{
  OW_GPIO = pin;
  gpio_pad_select_gpio(OW_GPIO);
}
