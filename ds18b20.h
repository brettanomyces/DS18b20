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
#ifndef DS18B20_H_  
#define DS18B20_H_

extern uint8_t ROM_NO[8];

void ow_write_bit(uint8_t bit);
uint8_t ow_read_bit(void);
void ow_write_byte(uint8_t data);
uint8_t ow_read_byte(void);
uint8_t ow_touch_reset(void);

uint8_t ow_search();
uint8_t ow_first();
uint8_t ow_next();

float ds_get_temp(uint8_t addr[]);
void ow_init(uint8_t pin);

#endif
