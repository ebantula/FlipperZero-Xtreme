#include <stdio.h>
#include <furry.h>
#include <furry_hal.h>
#include <lp5562_reg.h>
#include "../minunit.h"

#define DATA_SIZE 4

static void furry_hal_i2c_int_setup() {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
}

static void furry_hal_i2c_int_teardown() {
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
}

MU_TEST(furry_hal_i2c_int_1b) {
    bool ret = false;
    uint8_t data_one = 0;

    // 1 byte: read, write, read
    ret = furry_hal_i2c_read_reg_8(
        &furry_hal_i2c_handle_power,
        LP5562_ADDRESS,
        LP5562_CHANNEL_BLUE_CURRENT_REGISTER,
        &data_one,
        LP5562_I2C_TIMEOUT);
    mu_assert(ret, "0 read_reg_8 failed");
    mu_assert(data_one != 0, "0 invalid data");
    ret = furry_hal_i2c_write_reg_8(
        &furry_hal_i2c_handle_power,
        LP5562_ADDRESS,
        LP5562_CHANNEL_BLUE_CURRENT_REGISTER,
        data_one,
        LP5562_I2C_TIMEOUT);
    mu_assert(ret, "1 write_reg_8 failed");
    ret = furry_hal_i2c_read_reg_8(
        &furry_hal_i2c_handle_power,
        LP5562_ADDRESS,
        LP5562_CHANNEL_BLUE_CURRENT_REGISTER,
        &data_one,
        LP5562_I2C_TIMEOUT);
    mu_assert(ret, "2 read_reg_8 failed");
    mu_assert(data_one != 0, "2 invalid data");
}

MU_TEST(furry_hal_i2c_int_3b) {
    bool ret = false;
    uint8_t data_many[DATA_SIZE] = {0};

    // 3 byte: read, write, read
    data_many[0] = LP5562_CHANNEL_BLUE_CURRENT_REGISTER;
    ret = furry_hal_i2c_tx(
        &furry_hal_i2c_handle_power, LP5562_ADDRESS, data_many, 1, LP5562_I2C_TIMEOUT);
    mu_assert(ret, "3 tx failed");
    ret = furry_hal_i2c_rx(
        &furry_hal_i2c_handle_power,
        LP5562_ADDRESS,
        data_many + 1,
        DATA_SIZE - 1,
        LP5562_I2C_TIMEOUT);
    mu_assert(ret, "4 rx failed");
    for(size_t i = 0; i < DATA_SIZE; i++) mu_assert(data_many[i] != 0, "4 invalid data_many");

    ret = furry_hal_i2c_tx(
        &furry_hal_i2c_handle_power, LP5562_ADDRESS, data_many, DATA_SIZE, LP5562_I2C_TIMEOUT);
    mu_assert(ret, "5 tx failed");

    ret = furry_hal_i2c_tx(
        &furry_hal_i2c_handle_power, LP5562_ADDRESS, data_many, 1, LP5562_I2C_TIMEOUT);
    mu_assert(ret, "6 tx failed");
    ret = furry_hal_i2c_rx(
        &furry_hal_i2c_handle_power,
        LP5562_ADDRESS,
        data_many + 1,
        DATA_SIZE - 1,
        LP5562_I2C_TIMEOUT);
    mu_assert(ret, "7 rx failed");
    for(size_t i = 0; i < DATA_SIZE; i++) mu_assert(data_many[i] != 0, "7 invalid data_many");
}

MU_TEST(furry_hal_i2c_int_1b_fail) {
    bool ret = false;
    uint8_t data_one = 0;

    // 1 byte: fail, read, fail, write, fail, read
    data_one = 0;
    ret = furry_hal_i2c_read_reg_8(
        &furry_hal_i2c_handle_power,
        LP5562_ADDRESS + 0x10,
        LP5562_CHANNEL_BLUE_CURRENT_REGISTER,
        &data_one,
        LP5562_I2C_TIMEOUT);
    mu_assert(!ret, "8 read_reg_8 failed");
    mu_assert(data_one == 0, "8 invalid data");
    ret = furry_hal_i2c_read_reg_8(
        &furry_hal_i2c_handle_power,
        LP5562_ADDRESS,
        LP5562_CHANNEL_BLUE_CURRENT_REGISTER,
        &data_one,
        LP5562_I2C_TIMEOUT);
    mu_assert(ret, "9 read_reg_8 failed");
    mu_assert(data_one != 0, "9 invalid data");
}

MU_TEST_SUITE(furry_hal_i2c_int_suite) {
    MU_SUITE_CONFIGURE(&furry_hal_i2c_int_setup, &furry_hal_i2c_int_teardown);
    MU_RUN_TEST(furry_hal_i2c_int_1b);
    MU_RUN_TEST(furry_hal_i2c_int_3b);
    MU_RUN_TEST(furry_hal_i2c_int_1b_fail);
}

int run_minunit_test_furry_hal() {
    MU_RUN_SUITE(furry_hal_i2c_int_suite);
    return MU_EXIT_CODE;
}