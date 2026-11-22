#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"

#include "gatts_table_creat_demo.h"

/* -------------------------------------------------------
 * CONFIGURACION
 * ------------------------------------------------------- */

#define GATTS_TABLE_TAG       "HEART_RATE"
#define PROFILE_NUM           1
#define PROFILE_APP_IDX       0
#define ESP_APP_ID            0x55
#define SVC_INST_ID           0

#define SAMPLE_DEVICE_NAME    "ESP32_HEART_RATE"

#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))

/* -------------------------------------------------------
 * UUID DEL SERVICIO HEART RATE
 * ------------------------------------------------------- */

static const uint16_t HEART_RATE_SERVICE_UUID = 0x180D;
static const uint16_t HEART_RATE_MEAS_UUID = 0x2A37;
static const uint16_t BODY_SENSOR_LOCATION_UUID = 0x2A38;
static const uint16_t HEART_RATE_CONTROL_POINT_UUID = 0x2A39;

/* UUID internos de GATT */

static const uint16_t primary_service_uuid =
    ESP_GATT_UUID_PRI_SERVICE;

static const uint16_t character_declaration_uuid =
    ESP_GATT_UUID_CHAR_DECLARE;

static const uint16_t character_client_config_uuid =
    ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

/* -------------------------------------------------------
 * VALORES DE LAS CARACTERISTICAS
 * ------------------------------------------------------- */

/* 0x00 = formato UINT8
 * 72 = frecuencia cardiaca de demostracion
 */
static const uint8_t heart_rate_measurement[2] = {
    0x00, 72
};

/* 0x01 = Chest (pecho) */
static const uint8_t body_sensor_location = 0x01;

/* Valor inicial del Control Point */
static const uint8_t control_point_value = 0x00;

/* CCCD: notificaciones desactivadas inicialmente */
static const uint8_t heart_rate_ccc[2] = {
    0x00, 0x00
};

/* Propiedades de las caracteristicas */

static const uint8_t char_prop_read_notify =
    ESP_GATT_CHAR_PROP_BIT_READ |
    ESP_GATT_CHAR_PROP_BIT_NOTIFY;

static const uint8_t char_prop_read =
    ESP_GATT_CHAR_PROP_BIT_READ;

static const uint8_t char_prop_write =
    ESP_GATT_CHAR_PROP_BIT_WRITE;

/* -------------------------------------------------------
 * TABLA DE ATRIBUTOS GATT
 * ------------------------------------------------------- */

static uint16_t heart_rate_handle_table[HRS_IDX_NB];

static const esp_gatts_attr_db_t gatt_db[HRS_IDX_NB] = {

    /* Servicio Heart Rate 0x180D */

    [IDX_SVC] =
    {{ESP_GATT_AUTO_RSP},
     {ESP_UUID_LEN_16,
      (uint8_t *)&primary_service_uuid,
      ESP_GATT_PERM_READ,
      sizeof(uint16_t),
      sizeof(HEART_RATE_SERVICE_UUID),
      (uint8_t *)&HEART_RATE_SERVICE_UUID}},

    /* Declaracion Heart Rate Measurement */

    [IDX_HR_MEAS_CHAR] =
    {{ESP_GATT_AUTO_RSP},
     {ESP_UUID_LEN_16,
      (uint8_t *)&character_declaration_uuid,
      ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE,
      CHAR_DECLARATION_SIZE,
      (uint8_t *)&char_prop_read_notify}},

    /* Valor Heart Rate Measurement 0x2A37 */

    [IDX_HR_MEAS_VAL] =
    {{ESP_GATT_AUTO_RSP},
     {ESP_UUID_LEN_16,
      (uint8_t *)&HEART_RATE_MEAS_UUID,
      ESP_GATT_PERM_READ,
      sizeof(heart_rate_measurement),
      sizeof(heart_rate_measurement),
      (uint8_t *)heart_rate_measurement}},

    /* Descriptor CCCD 0x2902 */

    [IDX_HR_MEAS_CFG] =
    {{ESP_GATT_AUTO_RSP},
     {ESP_UUID_LEN_16,
      (uint8_t *)&character_client_config_uuid,
      ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      sizeof(uint16_t),
      sizeof(heart_rate_ccc),
      (uint8_t *)heart_rate_ccc}},

    /* Declaracion Body Sensor Location */

    [IDX_BSL_CHAR] =
    {{ESP_GATT_AUTO_RSP},
     {ESP_UUID_LEN_16,
      (uint8_t *)&character_declaration_uuid,
      ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE,
      CHAR_DECLARATION_SIZE,
      (uint8_t *)&char_prop_read}},

    /* Valor Body Sensor Location 0x2A38 */

    [IDX_BSL_VAL] =
    {{ESP_GATT_AUTO_RSP},
     {ESP_UUID_LEN_16,
      (uint8_t *)&BODY_SENSOR_LOCATION_UUID,
      ESP_GATT_PERM_READ,
      sizeof(uint8_t),
      sizeof(body_sensor_location),
      (uint8_t *)&body_sensor_location}},

    /* Declaracion Heart Rate Control Point */

    [IDX_HRCP_CHAR] =
    {{ESP_GATT_AUTO_RSP},
     {ESP_UUID_LEN_16,
      (uint8_t *)&character_declaration_uuid,
      ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE,
      CHAR_DECLARATION_SIZE,
      (uint8_t *)&char_prop_write}},

    /* Valor Heart Rate Control Point 0x2A39 */

    [IDX_HRCP_VAL] =
    {{ESP_GATT_AUTO_RSP},
     {ESP_UUID_LEN_16,
      (uint8_t *)&HEART_RATE_CONTROL_POINT_UUID,
      ESP_GATT_PERM_WRITE,
      sizeof(uint8_t),
      sizeof(control_point_value),
      (uint8_t *)&control_point_value}}
};

/* -------------------------------------------------------
 * PUBLICIDAD BLUETOOTH BLE
 * ------------------------------------------------------- */

/*
 * Se anuncia:
 * - Flags
 * - Potencia TX
 * - Servicio 0x180D
 * - Nombre ESP32_HEART_RATE
 */

static uint8_t raw_adv_data[] = {

    /* Flags */
    0x02,
    ESP_BLE_AD_TYPE_FLAG,
    0x06,

    /* TX Power */
    0x02,
    ESP_BLE_AD_TYPE_TX_PWR,
    0xEB,

    /* UUID del servicio Heart Rate: 0x180D */
    0x03,
    ESP_BLE_AD_TYPE_16SRV_CMPL,
    0x0D,
    0x18,

    /* Nombre completo: ESP32_HEART_RATE */
    0x0A,
    ESP_BLE_AD_TYPE_NAME_CMPL,
    'E', 'S', 'T', 'E','F', 'A', 'N', 'I', 'A'
};

/* Parametros de publicidad */

static esp_ble_adv_params_t adv_params = {

    .adv_int_min = ESP_BLE_GAP_ADV_ITVL_MS(20),
    .adv_int_max = ESP_BLE_GAP_ADV_ITVL_MS(40),

    .adv_type = ADV_TYPE_IND,

    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,

    .channel_map = ADV_CHNL_ALL,

    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY
};

/* -------------------------------------------------------
 * CALLBACKS
 * ------------------------------------------------------- */

static void gatts_event_handler(
    esp_gatts_cb_event_t event,
    esp_gatt_if_t gatts_if,
    esp_ble_gatts_cb_param_t *param
);

static void gap_event_handler(
    esp_gap_ble_cb_event_t event,
    esp_ble_gap_cb_param_t *param
);

/* -------------------------------------------------------
 * CALLBACK GAP
 * ------------------------------------------------------- */

static void gap_event_handler(
    esp_gap_ble_cb_event_t event,
    esp_ble_gap_cb_param_t *param
)
{
    switch (event) {

        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:

            ESP_LOGI(
                GATTS_TABLE_TAG,
                "Datos de publicidad configurados"
            );

            esp_ble_gap_start_advertising(&adv_params);

            break;

        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:

            if (param->adv_start_cmpl.status ==
                ESP_BT_STATUS_SUCCESS) {

                ESP_LOGI(
                    GATTS_TABLE_TAG,
                    "Publicidad BLE iniciada correctamente"
                );

            } else {

                ESP_LOGE(
                    GATTS_TABLE_TAG,
                    "Error al iniciar publicidad BLE"
                );
            }

            break;

        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:

            ESP_LOGI(
                GATTS_TABLE_TAG,
                "Publicidad BLE detenida"
            );

            break;

        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:

            ESP_LOGI(
                GATTS_TABLE_TAG,
                "Parametros de conexion actualizados"
            );

            break;

        default:
            break;
    }
}

/* -------------------------------------------------------
 * CALLBACK GATTS
 * ------------------------------------------------------- */

static void gatts_event_handler(
    esp_gatts_cb_event_t event,
    esp_gatt_if_t gatts_if,
    esp_ble_gatts_cb_param_t *param
)
{
    switch (event) {

        /* Registro de la aplicacion */

        case ESP_GATTS_REG_EVT: {

            ESP_LOGI(
                GATTS_TABLE_TAG,
                "Aplicacion GATT registrada"
            );

            esp_ble_gap_set_device_name(SAMPLE_DEVICE_NAME);

            esp_err_t ret =
                esp_ble_gap_config_adv_data_raw(
                    raw_adv_data,
                    sizeof(raw_adv_data)
                );

            if (ret != ESP_OK) {

                ESP_LOGE(
                    GATTS_TABLE_TAG,
                    "Error configurando publicidad: %s",
                    esp_err_to_name(ret)
                );
            }

            ret = esp_ble_gatts_create_attr_tab(
                gatt_db,
                gatts_if,
                HRS_IDX_NB,
                SVC_INST_ID
            );

            if (ret != ESP_OK) {

                ESP_LOGE(
                    GATTS_TABLE_TAG,
                    "Error creando tabla GATT: %s",
                    esp_err_to_name(ret)
                );
            }

            break;
        }

        /* Tabla de atributos creada */

        case ESP_GATTS_CREAT_ATTR_TAB_EVT: {

            if (param->add_attr_tab.status != ESP_GATT_OK) {

                ESP_LOGE(
                    GATTS_TABLE_TAG,
                    "Error al crear tabla de atributos"
                );

                break;
            }

            if (param->add_attr_tab.num_handle != HRS_IDX_NB) {

                ESP_LOGE(
                    GATTS_TABLE_TAG,
                    "Numero de atributos incorrecto"
                );

                break;
            }

            memcpy(
                heart_rate_handle_table,
                param->add_attr_tab.handles,
                sizeof(heart_rate_handle_table)
            );

            ESP_LOGI(
                GATTS_TABLE_TAG,
                "Tabla GATT creada correctamente"
            );

            esp_ble_gatts_start_service(
                heart_rate_handle_table[IDX_SVC]
            );

            break;
        }

        /* Servicio iniciado */

        case ESP_GATTS_START_EVT:

            ESP_LOGI(
                GATTS_TABLE_TAG,
                "Servicio Heart Rate iniciado"
            );

            break;

        /* Lectura de caracteristicas */

        case ESP_GATTS_READ_EVT:

            ESP_LOGI(
                GATTS_TABLE_TAG,
                "Solicitud de lectura GATT, handle=%d",
                param->read.handle
            );

            break;

        /* Escritura de caracteristicas */

        case ESP_GATTS_WRITE_EVT: {

            ESP_LOGI(
                GATTS_TABLE_TAG,
                "Escritura GATT, handle=%d, longitud=%d",
                param->write.handle,
                param->write.len
            );

            ESP_LOG_BUFFER_HEX(
                GATTS_TABLE_TAG,
                param->write.value,
                param->write.len
            );

            if (!param->write.is_prep) {

                /* Comprobar si se ha escrito el descriptor CCCD */

                if (
                    param->write.handle ==
                    heart_rate_handle_table[IDX_HR_MEAS_CFG]
                    &&
                    param->write.len == 2
                ) {

                    uint16_t ccc_value =
                        param->write.value[0] |
                        (param->write.value[1] << 8);

                    if (ccc_value == 0x0001) {

                        ESP_LOGI(
                            GATTS_TABLE_TAG,
                            "Notificaciones Heart Rate activadas"
                        );

                        uint8_t notify_data[2] = {
                            0x00, 72
                        };

                        esp_err_t ret =
                            esp_ble_gatts_send_indicate(
                                gatts_if,
                                param->write.conn_id,
                                heart_rate_handle_table[IDX_HR_MEAS_VAL],
                                sizeof(notify_data),
                                notify_data,
                                false
                            );

                        if (ret != ESP_OK) {

                            ESP_LOGE(
                                GATTS_TABLE_TAG,
                                "Error enviando notificacion: %s",
                                esp_err_to_name(ret)
                            );
                        }

                    } else if (ccc_value == 0x0000) {

                        ESP_LOGI(
                            GATTS_TABLE_TAG,
                            "Notificaciones desactivadas"
                        );

                    } else {

                        ESP_LOGW(
                            GATTS_TABLE_TAG,
                            "Valor CCCD no reconocido: 0x%04X",
                            ccc_value
                        );
                    }
                }

            } else {

                ESP_LOGW(
                    GATTS_TABLE_TAG,
                    "Escritura preparada no implementada"
                );
            }

            break;
        }

        /* Conexion BLE */

        case ESP_GATTS_CONNECT_EVT: {

            ESP_LOGI(
                GATTS_TABLE_TAG,
                "Cliente BLE conectado"
            );

            ESP_LOG_BUFFER_HEX(
                GATTS_TABLE_TAG,
                param->connect.remote_bda,
                6
            );

            break;
        }

        /* Desconexion BLE */

        case ESP_GATTS_DISCONNECT_EVT:

            ESP_LOGI(
                GATTS_TABLE_TAG,
                "Cliente BLE desconectado"
            );

            ESP_LOGI(
                GATTS_TABLE_TAG,
                "Reiniciando publicidad BLE"
            );

            esp_ble_gap_start_advertising(&adv_params);

            break;

        case ESP_GATTS_MTU_EVT:

            ESP_LOGI(
                GATTS_TABLE_TAG,
                "MTU recibido: %d",
                param->mtu.mtu
            );

            break;

        default:
            break;
    }
}

/* -------------------------------------------------------
 * FUNCION PRINCIPAL
 * ------------------------------------------------------- */

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(
        GATTS_TABLE_TAG,
        "Iniciando servidor BLE Heart Rate"
    );

    /* Inicializar NVS */

    ret = nvs_flash_init();

    if (
        ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND
    ) {

        ESP_ERROR_CHECK(nvs_flash_erase());

        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    /* Liberar memoria Bluetooth clasica */

    ESP_ERROR_CHECK(
        esp_bt_controller_mem_release(
            ESP_BT_MODE_CLASSIC_BT
        )
    );

    /* Inicializar controlador Bluetooth */

    esp_bt_controller_config_t bt_cfg =
        BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ret = esp_bt_controller_init(&bt_cfg);

    ESP_ERROR_CHECK(ret);

    /* Activar modo BLE */

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);

    ESP_ERROR_CHECK(ret);

    /* Inicializar Bluedroid */

    esp_bluedroid_config_t cfg =
        BT_BLUEDROID_INIT_CONFIG_DEFAULT();

    ret = esp_bluedroid_init_with_cfg(&cfg);

    ESP_ERROR_CHECK(ret);

    /* Activar Bluedroid */

    ret = esp_bluedroid_enable();

    ESP_ERROR_CHECK(ret);

    /* Registrar callback GATT */

    ret = esp_ble_gatts_register_callback(
        gatts_event_handler
    );

    ESP_ERROR_CHECK(ret);

    /* Registrar callback GAP */

    ret = esp_ble_gap_register_callback(
        gap_event_handler
    );

    ESP_ERROR_CHECK(ret);

    /* Registrar aplicacion */

    ret = esp_ble_gatts_app_register(ESP_APP_ID);

    ESP_ERROR_CHECK(ret);

    /* Configurar MTU */

    ret = esp_ble_gatt_set_local_mtu(500);

    if (ret != ESP_OK) {

        ESP_LOGE(
            GATTS_TABLE_TAG,
            "Error configurando MTU: %s",
            esp_err_to_name(ret)
        );
    }

    ESP_LOGI(
        GATTS_TABLE_TAG,
        "Inicializacion BLE completada"
    );
}