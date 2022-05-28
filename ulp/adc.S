/* ULP Example: using ADC in deep sleep

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.

   This file contains assembly code which runs on the ULP.

   ULP wakes up to run this code at a certain period, determined by the values
   in SENS_ULP_CP_SLEEP_CYCx_REG registers. On each wake up, the program
   measures input voltage on the given ADC channel 'adc_oversampling_factor'
   times. Measurements are accumulated and average value is calculated.
   Average value is compared to the two thresholds: 'low_thr' and 'high_thr'.
   If the value is less than 'low_thr' or more than 'high_thr', ULP wakes up
   the chip from deep sleep.
*/

/* ULP assembly files are passed through C preprocessor first, so include directives
   and C macros may be used in these files 
 */
#include "sdkconfig.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"
#include "soc/sens_reg.h"

	/* ADC1 channel 6, GPIO34 */
	.set adc_channel, 6

	.set bullshit_value, 66

	/* Configure the number of ADC samples to average on each measurement.
	   For convenience, make it a power of 2. */
	.set adc_oversampling_factor_log, 2
	.set adc_oversampling_factor, (1 << adc_oversampling_factor_log)

	/* Define variables, which go into .bss section (zero-initialized data) */
	.bss

	// RTC IO number used to sample the input signal.
	.global io_number
io_number:
	.long 0

	// Active bit
	.global active
active:
	.long 0

	// Previous pin state
	.global previous
previous:
	.long 0

	// Duration of the exercise
	.global duration
duration:
	.long 0

	// Step count of one exercise
	.global steps
steps:
	.long 0

	// Cumulative load of the excercise
	.global load
load:
	.long 0

	// Counter for the timeout (end of the exercise)
	.global timeout_count
timeout_count:
	.long 0

	// Limit for the timeout counter
	.global timeout_max
timeout_max:
	.long 0

	// Store last ADC measurement for debugging
	.global last_result
last_result:
	.long 0

	// Bullshit variable for bullshit debugging
	.global bullshit
bullshit:
	.long 0

	/* Code goes into .text section */
	.text
	.global entry
entry:

	move r0, bullshit
	move r1, bullshit_value
	st r1, r0, 0

read_io:
	/* Load io_number */
	move r3, io_number
	ld r3, r3, 0

#if CONFIG_IDF_TARGET_ESP32S2
    /* ESP32S2 powers down RTC periph when entering deep sleep and thus by association SENS_SAR_IO_MUX_CONF_REG */
	WRITE_RTC_FIELD(SENS_SAR_IO_MUX_CONF_REG, SENS_IOMUX_CLK_GATE_EN, 1)
#endif

	/* Lower 16 IOs and higher need to be handled separately,
	 * because r0-r3 registers are 16 bit wide. Check which IO this is.
	 */
	move r0, r3
	jumpr read_io_high, 16, ge

	/* Read the value of lower 16 RTC IOs into R0 */
read_io_low:
	READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S, 16)
	rsh r0, r0, r3
	jump check_pin

	/* Read the value of RTC IOs 16-17, into R0 */
read_io_high:
	READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 16, 2)
	sub r3, r3, 16
	rsh r0, r0, r3

check_pin:
	move r1, previous
	ld r2, r1, 0
	st r0, r1, 0

	and r0, r0, 1
	jumpr check_active1, 1, eq

check_edge:
	and r2, r2, 1
	jump check_active2, eq
	jump idle

check_active1:
	move r3, active
	ld r3, r3, 0
	and r3, r3, 1
	jump exit, eq

idle:
	move r0, timeout_count
	move r1, timeout_max
	ld r0, r0, 0
	ld r1, r1, 0
	sub r0, r1, r0
	jump timeout, eq

	move r0, timeout_count
	move r2, duration
	ld r1, r0, 0
	ld r3, r2, 0
	add r1, r1, 1
	add r3, r3, 1
	st r1, r0, 0
	st r3, r2, 0
	jump exit

timeout:
	move r0, 0
	move r1, active
	st r0, r1, 0
	jump wake_up

check_active2:
	move r1, active
	ld r1, r1, 0
	and r0, r1, 1
	jumpr do_stuff, 1, eq

activate:
	move r0, 1
	move r1, active
	st r0, r1, 0
	move r0, 0
	move r1, duration
	st r0, r1, 0
	move r1, steps
	st r0, r1, 0
	move r1, load
	st r0, r1, 0

do_stuff:
	move r0, 0
	move r1, timeout_count
	st r0, r1, 0

	move r0, duration
	ld r1, r0, 0
	add r1, r1, 1
	st r1, r0, 0
	move r0, steps
	ld r1, r0, 0
	add r1, r1, 1
	st r1, r0, 0

measure:
	adc r0, 0, adc_channel + 1
	move r3, last_result
	st r0, r3, 0
	move r3, load
	ld r2, r3, 0
	add r0, r0, r2
	rsh r0, r0, 1
	st r0, r3, 0
//	jump wake_up
	jump exit

	/* value within range, end the program */
	.global exit
exit:
	halt

	.global wake_up
wake_up:
	/* Check if the system can be woken up */
	READ_RTC_FIELD(RTC_CNTL_LOW_POWER_ST_REG, RTC_CNTL_RDY_FOR_WAKEUP)
	and r0, r0, 1
	jump exit, eq

	/* Wake up the SoC, end program */
	wake
	WRITE_RTC_FIELD(RTC_CNTL_STATE0_REG, RTC_CNTL_ULP_CP_SLP_TIMER_EN, 0)
	halt