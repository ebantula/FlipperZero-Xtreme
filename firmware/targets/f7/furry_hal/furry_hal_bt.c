#include <furry_hal_bt.h>

#include <ble/ble.h>
#include <interface/patterns/ble_thread/shci/shci.h>
#include <stm32wbxx.h>

#include <furry_hal_version.h>
#include <furry_hal_bt_hid.h>
#include <furry_hal_bt_serial.h>
#include "battery_service.h"

#include <furry.h>

#define TAG "FurryHalBt"

#define FURRY_HAL_BT_DEFAULT_MAC_ADDR \
    { 0x6c, 0x7a, 0xd8, 0xac, 0x57, 0x72 }

/* Time, in ms, to wait for mode transition before crashing */
#define C2_MODE_SWITCH_TIMEOUT 10000

FurryMutex* furry_hal_bt_core2_mtx = NULL;
static FurryHalBtStack furry_hal_bt_stack = FurryHalBtStackUnknown;

typedef void (*FurryHalBtProfileStart)(void);
typedef void (*FurryHalBtProfileStop)(void);

typedef struct {
    FurryHalBtProfileStart start;
    FurryHalBtProfileStart stop;
    GapConfig config;
    uint16_t appearance_char;
    uint16_t advertise_service_uuid;
} FurryHalBtProfileConfig;

FurryHalBtProfileConfig profile_config[FurryHalBtProfileNumber] = {
    [FurryHalBtProfileSerial] =
        {
            .start = furry_hal_bt_serial_start,
            .stop = furry_hal_bt_serial_stop,
            .config =
                {
                    .adv_service_uuid = 0x3080,
                    .appearance_char = 0x8600,
                    .bonding_mode = true,
                    .pairing_method = GapPairingPinCodeShow,
                    .mac_address = FURRY_HAL_BT_DEFAULT_MAC_ADDR,
                    .conn_param =
                        {
                            .conn_int_min = 0x18, // 30 ms
                            .conn_int_max = 0x24, // 45 ms
                            .slave_latency = 0,
                            .supervisor_timeout = 0,
                        },
                },
        },
    [FurryHalBtProfileHidKeyboard] =
        {
            .start = furry_hal_bt_hid_start,
            .stop = furry_hal_bt_hid_stop,
            .config =
                {
                    .adv_service_uuid = HUMAN_INTERFACE_DEVICE_SERVICE_UUID,
                    .appearance_char = GAP_APPEARANCE_KEYBOARD,
                    .bonding_mode = true,
                    .pairing_method = GapPairingPinCodeVerifyYesNo,
                    .mac_address = FURRY_HAL_BT_DEFAULT_MAC_ADDR,
                    .conn_param =
                        {
                            .conn_int_min = 0x18, // 30 ms
                            .conn_int_max = 0x24, // 45 ms
                            .slave_latency = 0,
                            .supervisor_timeout = 0,
                        },
                },
        },
};
FurryHalBtProfileConfig* current_profile = NULL;

void furry_hal_bt_init() {
    if(!furry_hal_bt_core2_mtx) {
        furry_hal_bt_core2_mtx = furry_mutex_alloc(FurryMutexTypeNormal);
        furry_assert(furry_hal_bt_core2_mtx);
    }

    // Explicitly tell that we are in charge of CLK48 domain
    furry_check(LL_HSEM_1StepLock(HSEM, CFG_HW_CLK48_CONFIG_SEMID) == 0);

    // Start Core2
    bl_igloo_init();
}

void furry_hal_bt_lock_core2() {
    furry_assert(furry_hal_bt_core2_mtx);
    furry_check(furry_mutex_acquire(furry_hal_bt_core2_mtx, FurryWaitForever) == FurryStatusOk);
}

void furry_hal_bt_unlock_core2() {
    furry_assert(furry_hal_bt_core2_mtx);
    furry_check(furry_mutex_release(furry_hal_bt_core2_mtx) == FurryStatusOk);
}

static bool furry_hal_bt_radio_stack_is_supported(const BlIglooC2Info* info) {
    bool supported = false;
    if(info->StackType == INFO_STACK_TYPE_BLE_LIGHT) {
        if(info->VersionMajor >= FURRY_HAL_BT_STACK_VERSION_MAJOR &&
           info->VersionMinor >= FURRY_HAL_BT_STACK_VERSION_MINOR) {
            furry_hal_bt_stack = FurryHalBtStackLight;
            supported = true;
        }
    } else if(info->StackType == INFO_STACK_TYPE_BLE_FULL) {
        if(info->VersionMajor >= FURRY_HAL_BT_STACK_VERSION_MAJOR &&
           info->VersionMinor >= FURRY_HAL_BT_STACK_VERSION_MINOR) {
            furry_hal_bt_stack = FurryHalBtStackFull;
            supported = true;
        }
    } else {
        furry_hal_bt_stack = FurryHalBtStackUnknown;
    }
    return supported;
}

bool furry_hal_bt_start_radio_stack() {
    bool res = false;
    furry_assert(furry_hal_bt_core2_mtx);

    furry_mutex_acquire(furry_hal_bt_core2_mtx, FurryWaitForever);

    // Explicitly tell that we are in charge of CLK48 domain
    furry_check(LL_HSEM_1StepLock(HSEM, CFG_HW_CLK48_CONFIG_SEMID) == 0);

    do {
        // Wait until C2 is started or timeout
        if(!bl_igloo_wait_for_c2_start(FURRY_HAL_BT_C2_START_TIMEOUT)) {
            FURRY_LOG_E(TAG, "Core2 start failed");
            bl_igloo_thread_stop();
            break;
        }

        // If C2 is running, start radio stack fw
        if(!furry_hal_bt_ensure_c2_mode(BlIglooC2ModeStack)) {
            break;
        }

        // Check whether we support radio stack
        const BlIglooC2Info* c2_info = bl_igloo_get_c2_info();
        if(!furry_hal_bt_radio_stack_is_supported(c2_info)) {
            FURRY_LOG_E(TAG, "Unsupported radio stack");
            // Don't stop SHCI for crypto enclave support
            break;
        }
        // Starting radio stack
        if(!bl_igloo_start()) {
            FURRY_LOG_E(TAG, "Failed to start radio stack");
            bl_igloo_thread_stop();
            ble_app_thread_stop();
            break;
        }
        res = true;
    } while(false);
    furry_mutex_release(furry_hal_bt_core2_mtx);

    return res;
}

FurryHalBtStack furry_hal_bt_get_radio_stack() {
    return furry_hal_bt_stack;
}

bool furry_hal_bt_is_ble_gatt_gap_supported() {
    if(furry_hal_bt_stack == FurryHalBtStackLight || furry_hal_bt_stack == FurryHalBtStackFull) {
        return true;
    } else {
        return false;
    }
}

bool furry_hal_bt_is_testing_supported() {
    if(furry_hal_bt_stack == FurryHalBtStackFull) {
        return true;
    } else {
        return false;
    }
}

bool furry_hal_bt_start_app(FurryHalBtProfile profile, GapEventCallback event_cb, void* context) {
    furry_assert(event_cb);
    furry_assert(profile < FurryHalBtProfileNumber);
    bool ret = false;

    do {
        if(!bl_igloo_is_radio_stack_ready()) {
            FURRY_LOG_E(TAG, "Can't start BLE App - radio stack did not start");
            break;
        }
        if(!furry_hal_bt_is_ble_gatt_gap_supported()) {
            FURRY_LOG_E(TAG, "Can't start Ble App - unsupported radio stack");
            break;
        }
        GapConfig* config = &profile_config[profile].config;
        // Configure GAP
        if(profile == FurryHalBtProfileSerial) {
            // Set mac address
            memcpy(
                config->mac_address, furry_hal_version_get_ble_mac(), sizeof(config->mac_address));
            // Set advertise name
            strlcpy(
                config->adv_name,
                furry_hal_version_get_ble_local_device_name_ptr(),
                FURRY_HAL_VERSION_DEVICE_NAME_LENGTH);

            config->adv_service_uuid |= furry_hal_version_get_hw_color();
        } else if(profile == FurryHalBtProfileHidKeyboard) {
            // Change MAC address for HID profile
            uint8_t default_mac[GAP_MAC_ADDR_SIZE] = FURRY_HAL_BT_DEFAULT_MAC_ADDR;
            if(memcmp(config->mac_address, default_mac, 6) == 0) {
                config->mac_address[2]++;
            }
            // Change name Flipper -> Control
            if(strnlen(config->adv_name, FURRY_HAL_VERSION_DEVICE_NAME_LENGTH) < 2 ||
               strnlen(config->adv_name + 1, FURRY_HAL_VERSION_DEVICE_NAME_LENGTH) < 1) {
                snprintf(
                    config->adv_name,
                    FURRY_HAL_VERSION_DEVICE_NAME_LENGTH,
                    "%cControl %s",
                    *furry_hal_version_get_ble_local_device_name_ptr(),
                    furry_hal_version_get_ble_local_device_name_ptr() + 1);
            }
        }
        if(!gap_init(config, event_cb, context)) {
            gap_thread_stop();
            FURRY_LOG_E(TAG, "Failed to init GAP");
            break;
        }
        // Start selected profile services
        if(furry_hal_bt_is_ble_gatt_gap_supported()) {
            profile_config[profile].start();
        }
        ret = true;
    } while(false);
    current_profile = &profile_config[profile];

    return ret;
}

void furry_hal_bt_reinit() {
    FURRY_LOG_I(TAG, "Disconnect and stop advertising");
    furry_hal_bt_stop_advertising();

    FURRY_LOG_I(TAG, "Stop current profile services");
    current_profile->stop();

    // Magic happens here
    hci_reset();

    FURRY_LOG_I(TAG, "Stop BLE related RTOS threads");
    ble_app_thread_stop();
    gap_thread_stop();

    FURRY_LOG_I(TAG, "Reset SHCI");
    furry_check(bl_igloo_reinit_c2());

    furry_delay_ms(100);
    bl_igloo_thread_stop();

    FURRY_LOG_I(TAG, "Start BT initialization");
    furry_hal_bt_init();

    furry_hal_bt_start_radio_stack();
}

bool furry_hal_bt_change_app(FurryHalBtProfile profile, GapEventCallback event_cb, void* context) {
    furry_assert(event_cb);
    furry_assert(profile < FurryHalBtProfileNumber);
    bool ret = true;

    furry_hal_bt_reinit();

    ret = furry_hal_bt_start_app(profile, event_cb, context);
    if(ret) {
        current_profile = &profile_config[profile];
    }
    return ret;
}

bool furry_hal_bt_is_active() {
    return gap_get_state() > GapStateIdle;
}

bool furry_hal_bt_is_connected() {
    return gap_get_state() == GapStateConnected;
}

void furry_hal_bt_start_advertising() {
    if(gap_get_state() == GapStateIdle) {
        gap_start_advertising();
    }
}

void furry_hal_bt_stop_advertising() {
    if(furry_hal_bt_is_active()) {
        gap_stop_advertising();
        while(furry_hal_bt_is_active()) {
            furry_delay_tick(1);
        }
    }
}

void furry_hal_bt_update_battery_level(uint8_t battery_level) {
    if(battery_svc_is_started()) {
        battery_svc_update_level(battery_level);
    }
}

void furry_hal_bt_update_power_state() {
    if(battery_svc_is_started()) {
        battery_svc_update_power_state();
    }
}

void furry_hal_bt_get_key_storage_buff(uint8_t** key_buff_addr, uint16_t* key_buff_size) {
    ble_app_get_key_storage_buff(key_buff_addr, key_buff_size);
}

void furry_hal_bt_set_key_storage_change_callback(
    BlIglooKeyStorageChangedCallback callback,
    void* context) {
    furry_assert(callback);
    bl_igloo_set_key_storage_changed_callback(callback, context);
}

void furry_hal_bt_nvm_sram_sem_acquire() {
    while(LL_HSEM_1StepLock(HSEM, CFG_HW_BLE_NVM_SRAM_SEMID)) {
        furry_thread_yield();
    }
}

void furry_hal_bt_nvm_sram_sem_release() {
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_BLE_NVM_SRAM_SEMID, 0);
}

bool furry_hal_bt_clear_white_list() {
    furry_hal_bt_nvm_sram_sem_acquire();
    tBleStatus status = aci_gap_clear_security_db();
    if(status) {
        FURRY_LOG_E(TAG, "Clear while list failed with status %d", status);
    }
    furry_hal_bt_nvm_sram_sem_release();
    return status != BLE_STATUS_SUCCESS;
}

void furry_hal_bt_dump_state(FurryString* buffer) {
    if(furry_hal_bt_is_alive()) {
        uint8_t HCI_Version;
        uint16_t HCI_Revision;
        uint8_t LMP_PAL_Version;
        uint16_t Manufacturer_Name;
        uint16_t LMP_PAL_Subversion;

        tBleStatus ret = hci_read_local_version_information(
            &HCI_Version, &HCI_Revision, &LMP_PAL_Version, &Manufacturer_Name, &LMP_PAL_Subversion);

        furry_string_cat_printf(
            buffer,
            "Ret: %d, HCI_Version: %d, HCI_Revision: %d, LMP_PAL_Version: %d, Manufacturer_Name: %d, LMP_PAL_Subversion: %d",
            ret,
            HCI_Version,
            HCI_Revision,
            LMP_PAL_Version,
            Manufacturer_Name,
            LMP_PAL_Subversion);
    } else {
        furry_string_cat_printf(buffer, "BLE not ready");
    }
}

bool furry_hal_bt_is_alive() {
    return bl_igloo_is_alive();
}

void furry_hal_bt_start_tone_tx(uint8_t channel, uint8_t power) {
    aci_hal_set_tx_power_level(0, power);
    aci_hal_tone_start(channel, 0);
}

void furry_hal_bt_stop_tone_tx() {
    aci_hal_tone_stop();
}

void furry_hal_bt_start_packet_tx(uint8_t channel, uint8_t pattern, uint8_t datarate) {
    hci_le_enhanced_transmitter_test(channel, 0x25, pattern, datarate);
}

void furry_hal_bt_start_packet_rx(uint8_t channel, uint8_t datarate) {
    hci_le_enhanced_receiver_test(channel, datarate, 0);
}

uint16_t furry_hal_bt_stop_packet_test() {
    uint16_t num_of_packets = 0;
    hci_le_test_end(&num_of_packets);
    return num_of_packets;
}

void furry_hal_bt_start_rx(uint8_t channel) {
    aci_hal_rx_start(channel);
}

float furry_hal_bt_get_rssi() {
    float val;
    uint8_t rssi_raw[3];

    if(aci_hal_read_raw_rssi(rssi_raw) != BLE_STATUS_SUCCESS) {
        return 0.0f;
    }

    // Some ST magic with rssi
    uint8_t agc = rssi_raw[2] & 0xFF;
    int rssi = (((int)rssi_raw[1] << 8) & 0xFF00) + (rssi_raw[0] & 0xFF);
    if(rssi == 0 || agc > 11) {
        val = -127.0;
    } else {
        val = agc * 6.0f - 127.0f;
        while(rssi > 30) {
            val += 6.0;
            rssi >>= 1;
        }
        val += (float)((417 * rssi + 18080) >> 10);
    }
    return val;
}

/** fill the RSSI of the remote host of the bt connection and returns the last 
 *  time the RSSI was updated
 * 
*/
uint32_t furry_hal_bt_get_conn_rssi(uint8_t* rssi) {
    int8_t ret_rssi = 0;
    uint32_t since = gap_get_remote_conn_rssi(&ret_rssi);

    if(ret_rssi == 127 || since == 0) return 0;

    *rssi = (uint8_t)abs(ret_rssi);

    return since;
}

uint32_t furry_hal_bt_get_transmitted_packets() {
    uint32_t packets = 0;
    aci_hal_le_tx_test_packet_number(&packets);
    return packets;
}

void furry_hal_bt_stop_rx() {
    aci_hal_rx_stop();
}

bool furry_hal_bt_ensure_c2_mode(BlIglooC2Mode mode) {
    BlIglooCommandResult fw_start_res = bl_igloo_force_c2_mode(mode);
    if(fw_start_res == BlIglooCommandResultOK) {
        return true;
    } else if(fw_start_res == BlIglooCommandResultRestartPending) {
        // Do nothing and wait for system reset
        furry_delay_ms(C2_MODE_SWITCH_TIMEOUT);
        furry_crash("Waiting for FUS->radio stack transition");
        return true;
    }

    FURRY_LOG_E(TAG, "Failed to switch C2 mode: %d", fw_start_res);
    return false;
}

void furry_hal_bt_set_profile_adv_name(
    FurryHalBtProfile profile,
    const char name[FURRY_HAL_BT_ADV_NAME_LENGTH]) {
    furry_assert(profile < FurryHalBtProfileNumber);
    furry_assert(name);

    if(strlen(name) == 0) {
        memset(
            &(profile_config[profile].config.adv_name[1]),
            0,
            strlen(&(profile_config[profile].config.adv_name[1])));
    } else {
        profile_config[profile].config.adv_name[0] = AD_TYPE_COMPLETE_LOCAL_NAME;
        memcpy(&(profile_config[profile].config.adv_name[1]), name, FURRY_HAL_BT_ADV_NAME_LENGTH);
    }
}

const char* furry_hal_bt_get_profile_adv_name(FurryHalBtProfile profile) {
    furry_assert(profile < FurryHalBtProfileNumber);
    return &(profile_config[profile].config.adv_name[1]);
}

void furry_hal_bt_set_profile_mac_addr(
    FurryHalBtProfile profile,
    const uint8_t mac_addr[GAP_MAC_ADDR_SIZE]) {
    furry_assert(profile < FurryHalBtProfileNumber);
    furry_assert(mac_addr);

    memcpy(profile_config[profile].config.mac_address, mac_addr, GAP_MAC_ADDR_SIZE);
}

const uint8_t* furry_hal_bt_get_profile_mac_addr(FurryHalBtProfile profile) {
    furry_assert(profile < FurryHalBtProfileNumber);
    return profile_config[profile].config.mac_address;
}

void furry_hal_bt_set_profile_pairing_method(FurryHalBtProfile profile, GapPairing pairing_method) {
    furry_assert(profile < FurryHalBtProfileNumber);
    profile_config[profile].config.pairing_method = pairing_method;
}

GapPairing furry_hal_bt_get_profile_pairing_method(FurryHalBtProfile profile) {
    furry_assert(profile < FurryHalBtProfileNumber);
    return profile_config[profile].config.pairing_method;
}