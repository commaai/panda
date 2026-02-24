#!/usr/bin/env python3
"""
Test that important GPIO pins are initialized correctly after board init.

Verifies:
- CAN transceiver enable lines (output mode, active-low)
- CAN RX/TX alternate functions (FDCAN1, FDCAN2, FDCAN3)
- SOM reset / bootkick lines (tres, cuatro)
- LED pins (output or PWM alternate function)
- USB pins (alternate function)
- Clock source pins (alternate function)
"""
import os
import pytest
from ctypes import cdll, c_uint8

# GPIO mode constants (matching STM32 MODER values)
MODE_INPUT = 0
MODE_OUTPUT = 1
MODE_ALTERNATE = 2
MODE_ANALOG = 3

# Pull-up/pull-down constants
PULL_NONE = 0
PULL_UP = 1
PULL_DOWN = 2

# Output type
OUTPUT_TYPE_PUSH_PULL = 0
OUTPUT_TYPE_OPEN_DRAIN = 1

# GPIO bank indices
GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH = range(8)

# Alternate function values
AF_TIM1 = 0x01
AF_TIM3 = 0x02
AF_FDCAN3_2 = 0x02  # FDCAN3 on AF2
AF_I2C5 = 0x04
AF_SPI4 = 0x05
AF_FDCAN3_5 = 0x05  # FDCAN3 on AF5
AF_UART7 = 0x07
AF_FDCAN1 = 0x09
AF_FDCAN2 = 0x09
AF_OTG1_FS = 0x0A

libpanda_gpio_dir = os.path.dirname(os.path.abspath(__file__))
libpanda_gpio_fn = os.path.join(libpanda_gpio_dir, "libpanda_gpio.so")


@pytest.fixture(scope="module")
def lib():
    return cdll.LoadLibrary(libpanda_gpio_fn)


def pin_mode(lib, bank, pin):
    return lib.query_pin_mode(c_uint8(bank), c_uint8(pin))


def pin_af(lib, bank, pin):
    return lib.query_pin_af(c_uint8(bank), c_uint8(pin))


def pin_otype(lib, bank, pin):
    return lib.query_pin_otype(c_uint8(bank), c_uint8(pin))


def pin_pupd(lib, bank, pin):
    return lib.query_pin_pupd(c_uint8(bank), c_uint8(pin))


def pin_odr(lib, bank, pin):
    return lib.query_pin_odr(c_uint8(bank), c_uint8(pin))


# ==================== Common GPIO tests (all boards) ====================

class TestCommonGPIO:
    """Tests for GPIO initialized by common_init_gpio(), shared by all boards."""

    @pytest.fixture(autouse=True, params=["red", "tres", "cuatro"])
    def init_board(self, lib, request):
        self.board = request.param
        if self.board == "red":
            lib.test_init_red()
        elif self.board == "tres":
            lib.test_init_tres()
        else:
            lib.test_init_cuatro()
        self.lib = lib

    def test_usb_pins_alternate(self):
        """A11, A12: USB OTG alternate function."""
        assert pin_mode(self.lib, GPIOA, 11) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOA, 11) == AF_OTG1_FS
        assert pin_mode(self.lib, GPIOA, 12) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOA, 12) == AF_OTG1_FS

    def test_fdcan1_pins(self):
        """B8, B9: FDCAN1 RX/TX alternate function."""
        assert pin_mode(self.lib, GPIOB, 8) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOB, 8) == AF_FDCAN1
        assert pin_pupd(self.lib, GPIOB, 8) == PULL_NONE
        assert pin_mode(self.lib, GPIOB, 9) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOB, 9) == AF_FDCAN1
        assert pin_pupd(self.lib, GPIOB, 9) == PULL_NONE

    def test_fdcan2_default_pins(self):
        """B5, B6: FDCAN2 RX/TX alternate function (default mux position)."""
        assert pin_mode(self.lib, GPIOB, 5) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOB, 5) == AF_FDCAN2
        assert pin_pupd(self.lib, GPIOB, 5) == PULL_NONE
        assert pin_mode(self.lib, GPIOB, 6) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOB, 6) == AF_FDCAN2
        assert pin_pupd(self.lib, GPIOB, 6) == PULL_NONE

    def test_voltage_sense_analog(self):
        """F11: VOLT_S in analog mode."""
        assert pin_mode(self.lib, GPIOF, 11) == MODE_ANALOG
        assert pin_pupd(self.lib, GPIOF, 11) == PULL_NONE


# ==================== Red Panda GPIO tests ====================

class TestRedGPIO:

    @pytest.fixture(autouse=True)
    def init_board(self, lib):
        lib.test_init_red()
        self.lib = lib

    def test_can_transceiver_enables(self):
        """CAN transceiver enable pins should be output mode with no pull."""
        # G11: transceiver 1 enable
        assert pin_mode(self.lib, GPIOG, 11) == MODE_OUTPUT
        assert pin_pupd(self.lib, GPIOG, 11) == PULL_NONE
        # B3: transceiver 2 enable
        assert pin_mode(self.lib, GPIOB, 3) == MODE_OUTPUT
        assert pin_pupd(self.lib, GPIOB, 3) == PULL_NONE
        # D7: transceiver 3 enable
        assert pin_mode(self.lib, GPIOD, 7) == MODE_OUTPUT
        assert pin_pupd(self.lib, GPIOD, 7) == PULL_NONE
        # B4: transceiver 4 enable
        assert pin_mode(self.lib, GPIOB, 4) == MODE_OUTPUT
        assert pin_pupd(self.lib, GPIOB, 4) == PULL_NONE

    def test_5v_output_sense_analog(self):
        """B1: 5VOUT_S should be analog mode."""
        assert pin_mode(self.lib, GPIOB, 1) == MODE_ANALOG

    def test_usb_load_switch(self):
        """B14: USB load switch, open drain with pull-up, driven high.
        Note: Red panda does not call clock_source_init, so B14 stays as output."""
        assert pin_mode(self.lib, GPIOB, 14) == MODE_OUTPUT
        assert pin_otype(self.lib, GPIOB, 14) == OUTPUT_TYPE_OPEN_DRAIN
        assert pin_pupd(self.lib, GPIOB, 14) == PULL_UP
        assert pin_odr(self.lib, GPIOB, 14) == 1

    def test_fdcan3_pins(self):
        """G9, G10: FDCAN3 RX/TX alternate function."""
        assert pin_mode(self.lib, GPIOG, 9) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOG, 9) == AF_FDCAN3_2
        assert pin_mode(self.lib, GPIOG, 10) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOG, 10) == AF_FDCAN3_2


# ==================== Tres GPIO tests ====================

class TestTresGPIO:

    @pytest.fixture(autouse=True)
    def init_board(self, lib):
        lib.test_init_tres()
        self.lib = lib

    def test_som_gpio_input(self):
        """C2: SOM GPIO should be input with pull-down."""
        assert pin_mode(self.lib, GPIOC, 2) == MODE_INPUT
        assert pin_pupd(self.lib, GPIOC, 2) == PULL_DOWN

    def test_bootkick_lines(self):
        """A0: bootkick, C12: SOM reset - both output."""
        assert pin_mode(self.lib, GPIOA, 0) == MODE_OUTPUT
        assert pin_mode(self.lib, GPIOC, 12) == MODE_OUTPUT

    def test_bootkick_safe_state(self):
        """On init with BOOT_BOOTKICK: A0 driven low, C12 driven high (not in reset)."""
        assert pin_odr(self.lib, GPIOA, 0) == 0
        assert pin_odr(self.lib, GPIOC, 12) == 1

    def test_uart7_debug(self):
        """E7, E8: UART7 for SOM debugging."""
        assert pin_mode(self.lib, GPIOE, 7) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOE, 7) == AF_UART7
        assert pin_mode(self.lib, GPIOE, 8) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOE, 8) == AF_UART7

    def test_fan_pwm(self):
        """C8: Fan PWM alternate function (TIM3)."""
        assert pin_mode(self.lib, GPIOC, 8) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOC, 8) == AF_TIM3

    def test_ir_pwm(self):
        """C9: IR LED PWM alternate function (TIM3)."""
        assert pin_mode(self.lib, GPIOC, 9) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOC, 9) == AF_TIM3

    def test_i2c_siren_open_drain(self):
        """C10, C11: I2C5 for siren codec, open drain."""
        assert pin_mode(self.lib, GPIOC, 10) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOC, 10) == AF_I2C5
        assert pin_otype(self.lib, GPIOC, 10) == OUTPUT_TYPE_OPEN_DRAIN
        assert pin_mode(self.lib, GPIOC, 11) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOC, 11) == AF_I2C5
        assert pin_otype(self.lib, GPIOC, 11) == OUTPUT_TYPE_OPEN_DRAIN

    def test_clock_source_pins(self):
        """B14, B15: Clock source output alternate function (TIM1).
        Tres calls clock_source_init(false), so B14/B15 get AF."""
        assert pin_mode(self.lib, GPIOB, 14) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOB, 14) == AF_TIM1
        assert pin_mode(self.lib, GPIOB, 15) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOB, 15) == AF_TIM1

    def test_fdcan3_pins(self):
        """G9, G10: FDCAN3 RX/TX alternate function (from common_init_gpio)."""
        assert pin_mode(self.lib, GPIOG, 9) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOG, 9) == AF_FDCAN3_2
        assert pin_mode(self.lib, GPIOG, 10) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOG, 10) == AF_FDCAN3_2


# ==================== Cuatro GPIO tests ====================

class TestCuatroGPIO:

    @pytest.fixture(autouse=True)
    def init_board(self, lib):
        lib.test_init_cuatro()
        self.lib = lib

    def test_fan_enable_open_drain(self):
        """D3: FAN_EN should be open drain."""
        assert pin_otype(self.lib, GPIOD, 3) == OUTPUT_TYPE_OPEN_DRAIN

    def test_dc_in_open_drain(self):
        """C11: DC_IN_EN_N should be open drain."""
        assert pin_otype(self.lib, GPIOC, 11) == OUTPUT_TYPE_OPEN_DRAIN

    def test_power_readout_analog(self):
        """C5, A6: Power readout pins in analog mode."""
        assert pin_mode(self.lib, GPIOC, 5) == MODE_ANALOG
        assert pin_mode(self.lib, GPIOA, 6) == MODE_ANALOG

    def test_can_transceiver_enables(self):
        """Cuatro CAN transceiver enable pins (B7, D8)."""
        assert pin_mode(self.lib, GPIOB, 7) == MODE_OUTPUT
        assert pin_pupd(self.lib, GPIOB, 7) == PULL_NONE
        assert pin_mode(self.lib, GPIOD, 8) == MODE_OUTPUT
        assert pin_pupd(self.lib, GPIOD, 8) == PULL_NONE

    def test_fdcan3_cuatro_pins(self):
        """D12, D13: FDCAN3 on different pins than red/tres."""
        assert pin_mode(self.lib, GPIOD, 12) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOD, 12) == AF_FDCAN3_5
        assert pin_pupd(self.lib, GPIOD, 12) == PULL_NONE
        assert pin_mode(self.lib, GPIOD, 13) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOD, 13) == AF_FDCAN3_5
        assert pin_pupd(self.lib, GPIOD, 13) == PULL_NONE

    def test_som_gpio_input(self):
        """C2: SOM GPIO should be input with pull-down."""
        assert pin_mode(self.lib, GPIOC, 2) == MODE_INPUT
        assert pin_pupd(self.lib, GPIOC, 2) == PULL_DOWN

    def test_bootkick_lines(self):
        """A0: bootkick output."""
        assert pin_mode(self.lib, GPIOA, 0) == MODE_OUTPUT

    def test_bootkick_safe_state(self):
        """On init, bootkick is active (A0 low)."""
        assert pin_odr(self.lib, GPIOA, 0) == 0

    def test_clock_source_channel1(self):
        """Cuatro enables clock source channel 1 on A8."""
        assert pin_mode(self.lib, GPIOA, 8) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOA, 8) == AF_TIM1

    def test_clock_source_pins(self):
        """B14, B15: Clock source output alternate function (TIM1)."""
        assert pin_mode(self.lib, GPIOB, 14) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOB, 14) == AF_TIM1
        assert pin_mode(self.lib, GPIOB, 15) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOB, 15) == AF_TIM1

    def test_fan_pwm_open_drain(self):
        """C8: Fan PWM, open drain."""
        assert pin_mode(self.lib, GPIOC, 8) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOC, 8) == AF_TIM3
        assert pin_otype(self.lib, GPIOC, 8) == OUTPUT_TYPE_OPEN_DRAIN

    def test_amp_disabled(self):
        """B0: Amplifier should be disabled (low) on init."""
        assert pin_mode(self.lib, GPIOB, 0) == MODE_OUTPUT
        assert pin_odr(self.lib, GPIOB, 0) == 0

    def test_uart7_debug(self):
        """E7, E8: UART7 for SOM debugging."""
        assert pin_mode(self.lib, GPIOE, 7) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOE, 7) == AF_UART7
        assert pin_mode(self.lib, GPIOE, 8) == MODE_ALTERNATE
        assert pin_af(self.lib, GPIOE, 8) == AF_UART7


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
