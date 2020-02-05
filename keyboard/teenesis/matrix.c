/*
Copyright 2012 Jun Wako <wakojun@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * scan matrix
 */
#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <util/delay.h>
#include "print.h"
#include "debug.h"
#include "util.h"
#include "matrix.h"
#ifdef DEBUG_MATRIX_SCAN_RATE
#include  "timer.h"
#endif

#ifndef DEBOUNCE
#define DEBOUNCE	7
#endif
static uint8_t debouncing = DEBOUNCE;

/* matrix state(1:on, 0:off) */
static matrix_row_t matrix[MATRIX_ROWS];
static matrix_row_t matrix_debouncing[MATRIX_ROWS];

static matrix_row_t read_cols(void);
static void init_cols(void);
static void unselect_rows(void);
static void select_row(uint8_t row);

#ifdef DEBUG_MATRIX_SCAN_RATE
uint32_t matrix_timer;
uint32_t matrix_scan_count;
#endif

//                           Y7               B3     B2     B1
static uint8_t led_staus = 1<<6|1<<5|1<<4 | 1<<3 | 1<<2 | 1<<1;

inline
uint8_t matrix_rows(void)
{
    return MATRIX_ROWS;
}

inline
uint8_t matrix_cols(void)
{
    return MATRIX_COLS;
}

void matrix_init(void)
{
    debug_enable = true;
    debug_matrix = true;
    debug_mouse = true;
    // initialize row and col
    unselect_rows();
    init_cols();

    // initialize matrix state: all keys off
    for (uint8_t i=0; i < MATRIX_ROWS; i++) {
        matrix[i] = 0;
        matrix_debouncing[i] = 0;
    }

#ifdef DEBUG_MATRIX_SCAN_RATE
    matrix_timer = timer_read32();
    matrix_scan_count = 0;
#endif
}

uint8_t matrix_scan(void)
{
#ifdef DEBUG_MATRIX_SCAN_RATE
    matrix_scan_count++;

    uint32_t timer_now = timer_read32();
    if (TIMER_DIFF_32(timer_now, matrix_timer)>1000) {
        print("matrix scan frequency: ");
        pdec(matrix_scan_count);
        print("\n");

        matrix_timer = timer_now;
        matrix_scan_count = 0;
    }
#endif

    for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
        select_row(i);
        _delay_us(30);  // without this wait read unstable value.
        matrix_row_t cols = read_cols();
        if (matrix_debouncing[i] != cols) {
            matrix_debouncing[i] = cols;
            if (debouncing) {
                debug("bounce!: "); debug_hex(debouncing); debug("\n");
            }
            debouncing = DEBOUNCE;
        }
        unselect_rows();
    }

    if (debouncing) {
        if (--debouncing) {
            _delay_ms(1);
        } else {
            for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
                matrix[i] = matrix_debouncing[i];
            }
        }
    }

    return 1;
}

bool matrix_is_modified(void)
{
    if (debouncing) return false;
    return true;
}

inline
bool matrix_is_on(uint8_t row, uint8_t col)
{
    return (matrix[row] & ((matrix_row_t)1<<col));
}

inline
matrix_row_t matrix_get_row(uint8_t row)
{
    return matrix[row];
}

void matrix_print(void)
{
    print("\nr/c 0123456789ABCDEF\n");
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        phex(row); print(": ");
        pbin_reverse16(matrix_get_row(row));
        print("\n");
    }
}

uint8_t matrix_key_count(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
        count += bitpop16(matrix[i]);
    }
    return count;
}

/* Column pin configuration
 * col: 0   1   2   3   4   5   6
 * pin: B0  B1  B2  B3  B4  B5  B6
 */
static void  init_cols(void)
{
    // Input with pull-up(DDR:0, PORT:1)
#ifdef PRO238_ENABLE
    DDRB  &= ~(led_staus);
    PORTB |=  (led_staus);


#else
    DDRB  &= ~(1<<6 | 1<<5 | 1<<4 | 1<<3 | 1<<2 | 1<<1 | 1<<0);
    PORTB |=  (1<<6 | 1<<5 | 1<<4 | 1<<3 | 1<<2 | 1<<1 | 1<<0);
#endif
}

static matrix_row_t read_cols(void)
{
    return
        (~PINB) & 0b01111111;

/*
    return
        (PIND&(1<<0) ? 0 : (1<<0)) |
        (PIND&(1<<1) ? 0 : (1<<1)) |
        (PIND&(1<<2) ? 0 : (1<<2)) |
        (PIND&(1<<3) ? 0 : (1<<3)) |
        (PIND&(1<<4) ? 0 : (1<<4)) |

        (PINF&(1<<0) ? 0 : (1<<5)) |
        (PINF&(1<<1) ? 0 : (1<<6)) |
        (PINF&(1<<4) ? 0 : (1<<7)) |
        (PINF&(1<<5) ? 0 : (1<<8)) |
        (PINF&(1<<6) ? 0 : (1<<9)) |

        (PINF&(1<<7) ? 0 : (1<<10)) |
        (PIND&(1<<5) ? 0 : (1<<11)) |

        (PINC&(1<<6) ? 0 : (1<<12)) |
        (PINC&(1<<7) ? 0 : (1<<13)) ;
*/
}

/* Row pin configuration
 * row: 0   1   2   3   4   5   6   7   8   9   10  11  12  13  14
 * pin: F0  F1  F4  F5  F6  F7  D0  D1  D2  D3  D4  D5  D6  C6  C7
 * pin: D7  E6  F4  F5  F6  F7  D0  D1  D2  D3  D4  D5  D6  C6  C7
 */
static void unselect_rows(void)
{
    // Hi-Z(DDR:0, PORT:0) to unselect
#ifdef PRO238_ENABLE
#endif

#ifdef ADVPRO_ENABLE
    DDRD  &= ~(1<<7 | 1<<6 | 1<<5 | 1<<4 | 1<<3 | 1<<2 | 1<<1 | 1<<0);
    PORTD &= ~(1<<7 | 1<<6 | 1<<5 | 1<<4 | 1<<3 | 1<<2 | 1<<1 | 1<<0);
    DDRF  &= ~(1<<7 | 1<<6 | 1<<5 | 1<<4);
    PORTF &= ~(1<<7 | 1<<6 | 1<<5 | 1<<4);
    DDRC  &= ~(1<<7 | 1<<6);
    PORTC &= ~(1<<7 | 1<<6);
    DDRE  &= ~(1<<6);
    PORTE &= ~(1<<6);
#endif

#ifdef TEENSY_ENABLE
    DDRD  &= ~(1<<6 | 1<<5 | 1<<4 | 1<<3 | 1<<2 | 1<<1 | 1<<0);
    PORTD &= ~(1<<6 | 1<<5 | 1<<4 | 1<<3 | 1<<2 | 1<<1 | 1<<0);
    DDRF  &= ~(1<<7 | 1<<6 | 1<<5 | 1<<4 | 1<<1 | 1<<0);
    PORTF &= ~(1<<7 | 1<<6 | 1<<5 | 1<<4 | 1<<1 | 1<<0);
    DDRC  &= ~(1<<7 | 1<<6);
    PORTC &= ~(1<<7 | 1<<6);
#endif
}

static void select_row(uint8_t row)
{
    // Output low(DDR:1, PORT:0) to select
    switch (row) {
#ifdef ADVPRO_ENABLE
        case 0:
            DDRD  |= (1<<7);
            PORTD &= ~(1<<7);
            break;
        case 1:
            DDRE  |= (1<<6);
            PORTE &= ~(1<<6);
            break;
#else
        case 0:
            DDRF  |= (1<<0);
            PORTF &= ~(1<<0);
            break;
        case 1:
            DDRF  |= (1<<1);
            PORTF &= ~(1<<1);
            break;
#endif
        case 2:
            DDRF  |= (1<<4);
            PORTF &= ~(1<<4);
            break;
        case 3:
            DDRF  |= (1<<5);
            PORTF &= ~(1<<5);
            break;
        case 4:
            DDRF  |= (1<<6);
            PORTF &= ~(1<<6);
            break;
        case 5:
            DDRF  |= (1<<7);
            PORTF &= ~(1<<7);
            break;
        case 6:
            DDRD  |= (1<<0);
            PORTD &= ~(1<<0);
            break;
        case 7:
            DDRD  |= (1<<1);
            PORTD &= ~(1<<1);
            break;
        case 8:
            DDRD  |= (1<<2);
            PORTD &= ~(1<<2);
            break;
        case 9:
            DDRD  |= (1<<3);
            PORTD &= ~(1<<3);
            break;
        case 10:
            DDRD  |= (1<<4);
            PORTD &= ~(1<<4);
            break;
        case 11:
            DDRD  |= (1<<5);
            PORTD &= ~(1<<5);
            break;
#ifdef FKEYS_ENABLE
        case 12:
            DDRD  |= (1<<6);
            PORTD &= ~(1<<6);
            break;
        case 13:
            DDRC  |= (1<<6);
            PORTC &= ~(1<<6);
            break;
        case 14:
            DDRC  |= (1<<7);
            PORTC &= ~(1<<7);
            break;
#endif
    }
}

