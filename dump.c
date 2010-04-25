#include <stdint.h>

#include <arduino/sleep.h>
#include <arduino/serial.h>
#include <arduino/timer2.h>
#include <arduino/adc.h>

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
	output.str = str;
	output.printing = 1;

	serial_interrupt_dre_enable();

	while (output.printing) {
		sleep_enable();
		sleep_cpu();
		sleep_disable();
	}

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
static volatile uint16_t value;
static char buf[16] __attribute__ ((section (".noinit")));

timer2_interrupt_a()
{
	time++;
}

adc_interrupt()
{
	uint8_t high;
	uint8_t low;

	low  = adc_data_low();
	high = adc_data_high();

	value = (high << 8) | low;
}

int
main()
{
	serial_baud_9600();
	serial_mode_8e1();
	serial_tx();

	sleep_mode_idle();

	/* setup timer2 to trigger interrupt a
	 * once every millisecond */
	timer2_mode_ctc();
	timer2_compare_a_set(124);
	timer2_clock_d128();
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
		char *p;

		p = sprint_uint16_b10(buf, value);
		*p++ = '\n';
		*p = '\0';
		serial_puts(buf);
	}

	return 0;
}
