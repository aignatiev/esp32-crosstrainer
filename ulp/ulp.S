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
	.set load_adc_channel, 6

	/* ADC1 channel 7, GPIO35 */
	.set vbat_adc_channel, 7

	.set vbat_lo, (4000*10/57)*4096/(1100UL * 4 / 3)
	.set vbat_hi, (5500*10/57)*4096/(1100UL * 4 / 3)

	/* Configure the number of ADC samples to average on each measurement.
	   For convenience, make it a power of 3. */
	.set adc_oversampling_factor_log, 3
	.set adc_oversampling_factor, (1 << adc_oversampling_factor_log)

	/* Define variables, which go into .bss section (zero-initialized data) */
	.bss

	// Delay counter for measuring temperature & voltage
	.global meas_count
meas_count:
	.long 0

	// Delay counter max for measuring temperature & voltage
	.global meas_max
meas_max:
	.long 0

	// Latest temperature measurement stored here
	.global temperature
temperature:
	.long 0

	// Latest vbat measurement stored here
	.global vbat
vbat:
	.long 0

	// Set when battery is detected to be low (until the batteries are swapped and the device restarted)
	.global battery_low
battery_low:
	.long 0

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

	// Counter for one second
	.global int_to_second_count
int_to_second_count:
	.long 0

	// Limit for one second counter
	.global int_to_second_max
int_to_second_max:
	.long 0

	// Duration of the exercise (in seconds)
	.global duration
duration:
	.long 0

	// Revolution count of one exercise
	.global revs
revs:
	.long 0

	// Cumulative load (thousands) of the excercise
	.global load_hi
load_hi:
	.long 0

	// Cumulative load (ones) of the excercise
	.global load_lo
load_lo:
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

	/* Code goes into .text section */
	.text
	.global entry
entry:

	//WRITE_RTC_REG(RTC_IO_TOUCH_PAD2_REG, RTC_IO_TOUCH_PAD2_HOLD_S, 1, 0)
	//WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S + 12, 1, 1)	// set debug pin
	//WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S + 10, 1, 1)	// set debug pin

// Enable TSENS
	//WRITE_RTC_REG(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_SAR_S, 2, SENS_FORCE_XPD_SAR_PU)

meas_temp:
	//tsens r2, 1000
	//move r3, temperature
	//st r2, r3, 0

	//WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S + 10, 1, 1)	// clear debug pin

init_vbat_measure:
	/* do measurements using ADC */
	/* r0 will be used as accumulator */
	move r0, 0
	/* initialize the loop counter */
	stage_rst

vbat_measure:
	/* measure and add value to accumulator */
	adc r1, 0, vbat_adc_channel + 1
	add r0, r0, r1
	/* increment loop counter and check exit condition */
	stage_inc 1
	jumps vbat_measure, adc_oversampling_factor, lt

	/* divide accumulator by adc_oversampling_factor.
	   Since it is chosen as a power of two, use right shift */
	rsh r0, r0, adc_oversampling_factor_log

	move r3, vbat
	st r0, r3, 0	// store result

check_active0:
	move r3, active
	ld r1, r3, 0
	sub r1, r1, 1	// check if active
	jump read_io//, eq

check_battery:
	move r3, battery_low
	ld r1, r3, 0
	and r1, r1, 1	// check if battery already low
	jump battery_is_high, eq

battery_is_low:
	jumpr read_io, vbat_hi, lt
	move r2, 0
	st r2, r3, 0	// new batteries inserted
	jump read_io

battery_is_high:
	jumpr read_io, vbat_lo, gt
	move r2, 1
	st r2, r3, 0	// battery is running low
	move r2, 0
	move r3, revs
	st r2, r3, 0	// clear revolution counter
	jump wake_up

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
	ld r2, r1, 0  	// load previous state
	st r0, r1, 0  	// store new state

	sub r0, r2, r0
	jumpr check_active2, 1, eq	// Falling edge

check_active1:
	move r3, active
	ld r3, r3, 0
	and r3, r3, 1 	// check if active
	jump exit, eq

idle:
	move r0, timeout_count
	move r1, timeout_max
	ld r0, r0, 0
	ld r1, r1, 0
	sub r0, r1, r0 // check if timed out
	jump timeout, eq

	move r0, timeout_count
	ld r1, r0, 0
	add r1, r1, 1 	// increment timeout counter
	st r1, r0, 0

	move r1, int_to_second_count
	ld r0, r1, 0
	add r0, r0, 1
	st r0, r1, 0  	// increment int_to_second counter

	move r2, int_to_second_max
	ld r2, r2, 0
	sub r0, r0, r2	// check if we have a second

	jump exit, ov

	move r2, 0
	st r2, r1, 0	// clear int_to_second counter

	move r0, duration
	ld r1, r0, 0
	add r1, r1, 1 	// increment duration counter
	st r1, r0, 0
	jump exit

timeout:
	move r0, 0
	move r1, active
	st r0, r1, 0	// deactivate
	jump wake_up	// wake up main cpu

check_active2:
	move r1, active
	ld r1, r1, 0
	and r0, r1, 1	// check if active
	jumpr do_stuff, 1, eq

activate:
	move r0, 1
	move r1, active
	st r0, r1, 0	// activate
	move r0, 0
	move r1, int_to_second_count
	st r0, r1, 0	// clear second counter
	move r1, duration
	st r0, r1, 0	// clear duration
	move r1, revs
	st r0, r1, 0	// clear revolution count
	move r1, load_hi
	st r0, r1, 0	// clear load measure
	move r1, load_lo
	st r0, r1, 0	// clear load measure

do_stuff:
	move r0, 0
	move r1, timeout_count
	st r0, r1, 0	// clear timeout counter

	move r0, revs
	ld r1, r0, 0
	add r1, r1, 1
	st r1, r0, 0	// increment revolution count

	move r1, int_to_second_count
	ld r0, r1, 0
	add r0, r0, 1
	st r0, r1, 0	// increment int_to_second counter

	move r2, int_to_second_max
	ld r2, r2, 0
	sub r0, r0, r2	// check if we have a second

	jump init_load_measure, ov

	move r2, 0
	st r2, r1, 0	// clear int_to_second counter

	move r0, duration
	ld r1, r0, 0
	add r1, r1, 1
	st r1, r0, 0	// increment duration

init_load_measure:
	/* do measurements using ADC */
	/* r0 will be used as accumulator */
	move r0, 0
	/* initialize the loop counter */
	stage_rst

load_measure:
	/* measure and add value to accumulator */
	adc r1, 0, load_adc_channel + 1
	add r0, r0, r1
	/* increment loop counter and check exit condition */
	stage_inc 1
	jumps load_measure, adc_oversampling_factor, lt

	/* divide accumulator by adc_oversampling_factor.
	   Since it is chosen as a power of two, use right shift */
	rsh r0, r0, adc_oversampling_factor_log


	move r3, last_result
	st r0, r3, 0	// store last result

	move r3, load_lo
	ld r2, r3, 0
	add r0, r0, r2
	jumpr overflow, 1000, ge
	st r0, r3, 0	// store load
	jump exit

overflow:
	move r1, load_hi
	ld r2, r1, 0
	add r2, r2, 1
	st r2, r1, 0 	// increment load_hi by 1
	sub r0, r0, 1000
	st r0, r3, 0	// decrement load_lo by 1000
	jumpr overflow, 1000, ge

	.global exit
exit:
	//WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S + 12, 1, 1)	// clear debug pin
	//WRITE_RTC_REG(RTC_IO_TOUCH_PAD2_REG, RTC_IO_TOUCH_PAD2_HOLD_S, 1, 1)

	halt

	.global wake_up
wake_up:
	/* Check if the system can be woken up */
	READ_RTC_FIELD(RTC_CNTL_LOW_POWER_ST_REG, RTC_CNTL_RDY_FOR_WAKEUP)
	and r0, r0, 1
	jump exit, eq

	//WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S + 12, 1, 1)	// clear debug pin
	//WRITE_RTC_REG(RTC_IO_TOUCH_PAD2_REG, RTC_IO_TOUCH_PAD2_HOLD_S, 1, 1)

	/* Wake up the SoC, end program */
	wake
	WRITE_RTC_FIELD(RTC_CNTL_STATE0_REG, RTC_CNTL_ULP_CP_SLP_TIMER_EN, 0)
	halt