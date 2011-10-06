/*
 * This file is part of blipduino.
 *
 * blipduino is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * blipduino is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with blipduino.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>

#include <arduino/pins.h>
#include <arduino/sleep.h>
#include <arduino/serial.h>
#include <arduino/timer2.h>
#include <arduino/adc.h>

#define BOUND_LOW 250
#define BOUND_HIGH 350

static volatile uint8_t new_value;

static volatile struct {
	char *str;
	uint8_t printing;
} output;

serial_interrupt_dre()
{
	char *str = output.str;
	uint8_t c = *str;

	if (c == '\0') {
		serial_interrupt_dre_disable();
		output.printing = 0;
	} else {
		serial_write(c);
		output.str = str + 1;
	}
}

static void
serial_puts(char *str)
{
again:
	cli();
	if (output.printing) {
		sleep_enable();
		sei();
		sleep_cpu();
		sleep_disable();
		goto again;
	}
	sei();

	output.str = str;
	output.printing = 1;

	serial_interrupt_dre_enable();
}

static char *
sprint_uint16_b10(char *p, uint16_t n)
{
	if (n >= 10000)
		*p++ = '0' + (n / 10000);

	if (n >= 1000)
		*p++ = '0' + (n / 1000) % 10;

	if (n >= 100)
		*p++ = '0' + (n / 100) % 10;

	if (n >= 10)
		*p++ = '0' + (n / 10) % 10;

	*p++ = '0' + n % 10;

	return p;
}

static volatile uint16_t time;
static char buf[16] __attribute__ ((section (".noinit")));

timer2_interrupt_a()
{
	time++;
}

adc_interrupt()
{
	new_value = 1;
}

int
main(void)
{
	uint8_t state = 0;

	serial_baud_9600();
	serial_mode_8e1();
	serial_transmitter_enable();

	sleep_mode_idle();

	pin13_mode_output();
	pin13_low();

	/* setup timer2 to trigger interrupt a
	 * once every millisecond
	 * 128 * (124 + 1) / 16MHz = 1ms */
	timer2_mode_ctc();
	timer2_clock_d128();
	timer2_compare_a_set(124);
	timer2_interrupt_a_enable();

	adc_reference_internal_5v();
	adc_pin_select(5);
	adc_clock_d128();
	adc_trigger_freerunning();
	adc_trigger_enable();
	adc_interrupt_enable();
	adc_enable();

	sei();
	while (1) {
		uint16_t value;

		cli();
		if (!new_value) {
			sleep_enable();
			sei();
			sleep_cpu();
			sleep_disable();
			continue;
		}
		sei();

		value = adc_data();
		new_value = 0;

		if (state && value < BOUND_LOW) {
			uint16_t now = time;
			char *p;

			timer2_clock_reset();
			time = 0;

			pin13_low();

			p = sprint_uint16_b10(buf, now);
			*p++ = '\n';
			*p = '\0';
			serial_puts(buf);

			state = 0;
			continue;
		}

		if (value > BOUND_HIGH) {
			pin13_high();
			state = 1;
		}
	}
}
