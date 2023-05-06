#include <furry_hal_bt_serial.h>
#include "dev_info_service.h"
#include "battery_service.h"
#include "serial_service.h"

#include <furry.h>

void furry_hal_bt_serial_start() {
    // Start device info
    if(!dev_info_svc_is_started()) {
        dev_info_svc_start();
    }
    // Start battery service
    if(!battery_svc_is_started()) {
        battery_svc_start();
    }
    // Start Serial service
    if(!serial_svc_is_started()) {
        serial_svc_start();
    }
}

void furry_hal_bt_serial_set_event_callback(
    uint16_t buff_size,
    FurryHalBtSerialCallback callback,
    void* context) {
    serial_svc_set_callbacks(buff_size, callback, context);
}

void furry_hal_bt_serial_notify_buffer_is_empty() {
    serial_svc_notify_buffer_is_empty();
}

void furry_hal_bt_serial_set_rpc_status(FurryHalBtSerialRpcStatus status) {
    SerialServiceRpcStatus st;
    if(status == FurryHalBtSerialRpcStatusActive) {
        st = SerialServiceRpcStatusActive;
    } else {
        st = SerialServiceRpcStatusNotActive;
    }
    serial_svc_set_rpc_status(st);
}

bool furry_hal_bt_serial_tx(uint8_t* data, uint16_t size) {
    if(size > FURRY_HAL_BT_SERIAL_PACKET_SIZE_MAX) {
        return false;
    }
    return serial_svc_update_tx(data, size);
}

void furry_hal_bt_serial_stop() {
    // Stop all services
    if(dev_info_svc_is_started()) {
        dev_info_svc_stop();
    }
    // Start battery service
    if(battery_svc_is_started()) {
        battery_svc_stop();
    }
    // Start Serial service
    if(serial_svc_is_started()) {
        serial_svc_stop();
    }
}