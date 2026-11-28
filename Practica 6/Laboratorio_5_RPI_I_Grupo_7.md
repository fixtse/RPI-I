# Laboratorio de RPI I

Grupo: 7

Integrantes:

- Amirhosein Mozaffari

- Estefania Ruiz

- Sergio Romero Plasencia

Fecha: 29/11/2026

Número de laboratorio: 6

## Desarrollo de ejercicios

### Ejercicio 1

Modifica los parámetros de escaneo para que este se realice con menor frecuencia. Añade estos parámetros a través de menuconfig. ¿Qué diferencia hay entre el intervalo de escaneo y la ventana de escaneo? Muestra cómo cambia el comportamiento del escaneo al modificar ambos parámetros.

Se sustituye del ejemplo original:

	.scan_interval = ESP_BLE_GAP_SCAN_ITVL_MS(50),
	.scan_window = ESP_BLE_GAP_SCAN_WIN_MS(30),

Por:

	.scan_interval = ESP_BLE_GAP_SCAN_ITVL_MS(CONFIG_BLE_SCAN_INTERVAL_MS),
	.scan_window = ESP_BLE_GAP_SCAN_WIN_MS(CONFIG_BLE_SCAN_WINDOW_MS),

Se añade en el Kconfig.projbuild

Después en el código del cliente.c

```c
static esp_ble_scan_params_t ble_scan_params = {

.scan_type = BLE_SCAN_TYPE_ACTIVE,

.own_addr_type = BLE_ADDR_TYPE_PUBLIC,

.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,

.scan_interval = ESP_BLE_GAP_SCAN_ITVL_MS(

CONFIG_BLE_SCAN_INTERVAL_MS

),
.scan_window = ESP_BLE_GAP_SCAN_WIN_MS(

CONFIG_BLE_SCAN_WINDOW_MS
),
.scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE
};
```

</div>

<div align="center">

![img1](img/img1.png)

*Parámetros para cambio de intervalo de escaneo y ventana de escaneo*

</div>


</div>

<div align="center">

![img2](img/img2.png)

*Menuconfig para cambio de parámetros*

</div>

¿Qué diferencia hay entre el intervalo de escaneo y la ventana de escaneo?

La diferencia radica en que el intervalo marca cada cuánto tiempo empieza un ciclo de búsqueda, mientras que la ventana define cuánto tiempo pasa la radio realmente encendida escuchando dentro de ese ciclo:
Intervalo de escaneo (scan_interval): Es el periodo total que transcurre desde que comienza un escaneo en un canal de anuncio (37, 38 o 39) hasta que arranca el siguiente ciclo en el siguiente canal. Determina la frecuencia con la que el cliente inicia la búsqueda.
Ventana de escaneo (scan_window): Es el tiempo efectivo dentro de cada intervalo durante el cual el receptor Bluetooth permanece activo capturando paquetes de advertising. 

### Tabla de pruebas y valores

| Configuración | Intervalo (`scan_interval`) | Ventana (`scan_window`) | Ciclo de trabajo (Duty Cycle) | Comportamiento observado |
| :--- | :--- | :--- | :--- | :--- |
| **Original (Rápido)** | $50\text{ ms}$ (`80`) | $30\text{ ms}$ (`48`) | $60\%$ | **Detección casi inmediata** (decenas de milisegundos). Alto consumo energético porque la radio pasa más de la mitad del tiempo escuchando. |
| **Baja frecuencia / Ventana corta** | $1000\text{ ms}$ (`1600`) | $50\text{ ms}$ (`80`) | $5\%$ | **Detección lenta / errática.** El cliente tarda varios segundos en encontrar al ESP32-C3 porque pasa $950\text{ ms}$ "dormido" por cada segundo y puede perder paquetes de anuncio. Consumo muy bajo. |
| **Baja frecuencia / Ventana amplia** | $1000\text{ ms}$ (`1600`) | $500\text{ ms}$ (`800`) | $50\%$ | **Detección moderada.** Aunque la frecuencia de inicio es baja (1 vez por segundo), al mantener la ventana abierta medio segundo tiene alta probabilidad de capturar el anuncio del servidor en el primer o segundo intento. |
| **Escaneo Continuo** | $1000\text{ ms}$ (`1600`) | $1000\text{ ms}$ (`1600`) | $100\%$ | **Sin pausas.** La radio escucha el $100\%$ del tiempo, cambiando de canal cada $1000\text{ ms}$. Máximo consumo de batería y máxima probabilidad de detección. |

#### Prueba 1

<div align="center">

![img3](img/img3.png)

*Prueba con valores default*
</div>

<div align="center">

![img4](img/img4.png)

*Prueba con valores default*
</div>



#### Prueba 2

</div>

<div align="center">

![img5](img/img5.png)

*Respuesta a menor frecuencia*

</div>

</div>

<div align="center">

![img6](img/img6.png)

*Respuesta a menor frecuencia*

</div>


En esta prueba el ESP32 pasa menos tiempo escuchando: aproximadamente el 15 %.

#### Prueba 3

</div>

<div align="center">

![img7](img/img7.png)

*Baja frecuencia / Ventana amplia*

</div>

</div>

<div align="center">

![img8](img/img8.png)

*Baja frecuencia / Ventana amplia*

</div>

#### Prueba 4

</div>

<div align="center">

![img9](img/img9.png)

*Escaneo Continuo*

</div>


</div>

<div align="center">

![img10](img/img10.png)

*Escaneo Continuo*

</div>



### Ejercicio 2

¿Por qué llega un momento en el que el escaneo se detiene? Analiza el código y modifícalo para que el proceso de escaneo se realice indefinidamente. Comprueba que el tiempo durante el cual el dispositivo permanece en fase de escaneo corresponde al valor seleccionado.

El escaneo se detiene por dos razones

1.- En la función esp_gap_cb(), dentro del evento ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT, define los siguiente:

```c
case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
    // The unit of duration is seconds.
    // If duration is set to 0, scanning will continue indefinitely
    // until esp_ble_gap_stop_scanning is explicitly called.
    uint32_t duration = 30;
    esp_ble_gap_start_scanning(duration);
    break;
}
```
Al pasar duration = 30, la pila BLE de ESP-IDF inicia un temporizador interno de 30 segundos. Si durante ese tiempo no encuentra el servidor, el escaneo se detiene automáticamente y dispara el evento ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT.

2.- Por detención explícita al encontrar el dispositivo:
Dentro de ESP_GAP_BLE_SCAN_RESULT_EVT, cuando el cliente detecta un anuncio cuyo nombre coincide con "ESP_GATTESTEFY", ejecuta explícitamente esp_ble_gap_stop_scanning(); justo antes de iniciar la conexión GATT (esp_ble_gattc_enh_open).


#### Modicar el codigo 

##### Cambio de la duración a 0

```c
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)

{

uint8_t *adv_name = NULL;

uint8_t adv_name_len = 0;

switch (event) {

case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {

uint32_t duration = 0;

ESP_LOGI(GATTC_TAG, "Iniciando escaneo indefinido");

esp_ble_gap_start_scanning(duration);

break;

}
```

##### Marca de tiempo para comprobar la duración real

El escaneo es indefinido hasta encontrar el dispositivo 


<div align="center">

![img11](img/img11.png)

*Tiempo total de escaneo*

</div>

### Ejercicio 3

Muestra también por pantalla el valor del RSSI de cada dispositivo BLE detectado.

Dentro del case ESP_GAP_SEARCH_INQ_RES_EVT; se coloca el siguiente código para visualizar lo solicitado en el ejercicio:

```c
ESP_LOGI(GATTC_TAG, "Scan result, device "ESP_BD_ADDR_STR", name len %u", ESP_BD_ADDR_HEX(scan_result->scan_rst.bda), adv_name_len);

ESP_LOG_BUFFER_CHAR(GATTC_TAG, adv_name, adv_name_len);

// Mostrar RSSI de cada dispositivo BLE detectado
ESP_LOGI(GATTC_TAG, "RSSI: %d dBm", scan_result->scan_rst.rssi);
```

Este evento se ejecuta cada vez que se detecta un dispositivo BLE. Por tanto, al colocar ahí la instrucción ESP_LOGI(), se mostrará el RSSI de cada dispositivo detectado, no solamente el del servidor GATT al que nos conectamos.

<div align="center">

![img12](img/img12.png)

*RSSI parámetro añadido al escaneo*

</div>

### Ejercicio 4

Añade a menuconfig un campo que permita configurar la dirección Bluetooth de un dispositivo conocido. Modifica el código para que únicamente se muestre la información del dispositivo cuya dirección se haya configurado.

Se crea la opción requerida por el ejercicio en Kconfig 


<div align="center">

![img13](img/img13.png)

*Campo para ingresar la MAC*

</div>

<div align="center">

![img14](img/img14.png)

*Mac del dispositivo servidor*

</div>


Código que se cambio en la busqueda
```c

case 

ESP_GAP_SEARCH_INQ_RES_EVT: {

    // Evitar procesar nuevos resultados si ya encontramos el dispositivo
    if (dispositivo_encontrado) {
        break;
    }

    // Obtener la MAC configurada en menuconfig
    unsigned int mac[6];

    if (sscanf(CONFIG_EXAMPLE_DEVICE_BLE_ADDRESS,
               "%x:%x:%x:%x:%x:%x",
               &mac[0], &mac[1], &mac[2],
               &mac[3], &mac[4], &mac[5]) != 6) {
        ESP_LOGE(GATTC_TAG, "Direccion MAC no valida");
        break;
    }

    // Comparar las direcciones MAC
    bool match = true;

    for (int i = 0; i < 6; i++) {
        if (scan_result->scan_rst.bda[i] != mac[i]) {
            match = false;
            break;
        }
    }

    // Si no coincide, ignorar el dispositivo
    if (!match) {
        break;
    }

    // Dispositivo encontrado
    dispositivo_encontrado = true;

    ESP_LOGI(GATTC_TAG, "DISPOSITIVO ENCONTRADO");

    ESP_LOGI(GATTC_TAG, "Direccion MAC: "ESP_BD_ADDR_STR,
             ESP_BD_ADDR_HEX(scan_result->scan_rst.bda));

    // Mostrar RSSI
    ESP_LOGI(GATTC_TAG, "RSSI: %d dBm",
             scan_result->scan_rst.rssi);

    // Obtener nombre del dispositivo
    adv_name = esp_ble_resolve_adv_data_by_type(
        scan_result->scan_rst.ble_adv,
        scan_result->scan_rst.adv_data_len +
        scan_result->scan_rst.scan_rsp_len,
        ESP_BLE_AD_TYPE_NAME_CMPL,
        &adv_name_len);

    if (adv_name != NULL) {
        ESP_LOGI(GATTC_TAG, "Nombre del dispositivo:");
        ESP_LOG_BUFFER_CHAR(GATTC_TAG, adv_name, adv_name_len);
    }

#if CONFIG_EXAMPLE_DUMP_ADV_DATA_AND_SCAN_RESP

    if (scan_result->scan_rst.adv_data_len > 0) {
        ESP_LOGI(GATTC_TAG, "adv data:");
        ESP_LOG_BUFFER_HEX(GATTC_TAG,
                           &scan_result->scan_rst.ble_adv[0],
                           scan_result->scan_rst.adv_data_len);
    }

    if (scan_result->scan_rst.scan_rsp_len > 0) {
        ESP_LOGI(GATTC_TAG, "scan resp:");
        ESP_LOG_BUFFER_HEX(GATTC_TAG,
                           &scan_result->scan_rst.ble_adv[scan_result->scan_rst.adv_data_len],
                           scan_result->scan_rst.scan_rsp_len);
    }

#endif

    // Detener el escaneo al encontrar el dispositivo
    ESP_LOGI(GATTC_TAG, "Dispositivo localizado. Deteniendo escaneo...");

    esp_ble_gap_stop_scanning();

    break;
}

```

<div align="center">

![img15](img/img15.png)

*Características del dispositivo con la mac seleccionada*

</div>

### Ejercicio 5
Modifica el código para que, en función del valor de RSSI obtenido para el dispositivo de interés, el cliente muestre por pantalla un indicador proporcional a la distancia aproximada a dicho dispositivo (por ejemplo, un número concreto de puntos según la distancia). Para ello, establece un valor mínimo y otro máximo de RSSI y aplica la siguiente fórmula: (RSSI - RSSI_MIN) / (RSSI_MAX - RSSI_MIN). Prueba a alejar físicamente los dispositivos entre sí.

Valores mínimo y máximo

```c

#define RSSI_MIN -100
#define RSSI_MAX -40
#define MAX_PUNTOS 10
```
| RSSI (dBm) | Indicador       |
|-----------|-----------------|
| -40 dBm   | **********      |
| -55 dBm   | *******         |
| -70 dBm   | ****            |
| -85 dBm   | **              |
| -100 dBm  |                 |

Aplicación de la fórmula en el código

```c
// Limitar RSSI al rango definido y aplicar la formula del ejercicio.
            int rssi_limitado = rssi;
            if (rssi_limitado < RSSI_MIN) rssi_limitado = RSSI_MIN;
            if (rssi_limitado > RSSI_MAX) rssi_limitado = RSSI_MAX;

            float proporcion = (float)(rssi_limitado - RSSI_MIN) /
                               (float)(RSSI_MAX - RSSI_MIN);
            int puntos = (int)(proporcion * MAX_PUNTOS + 0.5f);

            char indicador[MAX_PUNTOS + 1];
            for (int i = 0; i < MAX_PUNTOS; i++) {
                indicador[i] = (i < puntos) ? '*' : ' ';
            }
            indicador[MAX_PUNTOS] = '\0';

```
Indicador visual

```c

char indicador[MAX_PUNTOS + 1];

for (int i = 0; i < MAX_PUNTOS; i++) {
    indicador[i] = (i < puntos) ? '*' : ' ';
}

indicador[MAX_PUNTOS] = '\0';
```


<div align="center">

![img16](img/img16.png)

*Características del dispositivo con la mac seleccionada*

</div>

### Ejercicio 6
Conecta el cliente GATT al servidor modificado del último ejercicio de la práctica anterior para recibir notificaciones y mostrarlas por consola.

Ciclo completo de conexión y envío de notificaciones desde el servidor BLE (Heart Rate) hacia un cliente.
En la imagen de abajo se observa la conexión del cliente (31293 ms): Un cliente BLE se conecta al servidor e imprime su dirección MAC: 24:0a:c4:ea:50:92.
* Negociación de MTU (31753 ms): Se establece un tamaño máximo de paquete (MTU) de 500 bytes.
* Activación de notificaciones (31813 ms): El cliente realiza una escritura GATT de 2 bytes (01 00, equivalente a 0x0001 en formato little-endian) sobre el descriptor handle=43 (CCCD), lo que habilita el envío de notificaciones en el servicio de frecuencia 	cardíaca (Notificaciones Heart Rate activadas).
* Transmisión periódica de datos (32683 ms – 48683 ms): A partir de ese momento, el servidor envía una notificación cada segundo (APP: Sending data...), lo que confirma que la comunicación y suscripción con el cliente funcionan correctamente.


<div align="center">

![img17](img/img17.png)

*Conexón del cliente y servidor*
</div>

En la imagen a continuación se puede observar:

* Detección del servidor objetivo (6901 ms):** Tras 6 segundos de escaneo, el cliente localiza el dispositivo con dirección MAC `60:55:f9:bc:53:ba` y nombre `ESTEFANIA`, registrando una intensidad de señal (RSSI) de `-60 dBm` (`7/10` puntos de proximidad).
* Detención del escaneo y conexión GATT (6911–7061 ms):** El cliente detiene el escaneo e inicia la conexión (`Connected, conn_id 0`) con el servidor remoto.
* Descubrimiento del servicio y negociación de MTU (7511–7541 ms):** El cliente explora la tabla GATT, encuentra el servicio primario de frecuencia cardíaca (`UUID16: 0x180d`), registra las notificaciones y negocia un tamaño máximo de paquete (MTU) de `500` bytes.
* Activación y recepción de notificaciones (7601–10461 ms):** Tras habilitar el descriptor CCCD (`0x0001`), el cliente recibe periódicamente cada segundo las tramas de 2 bytes enviadas por el servidor (`00 48`, `00 ff`, `00 3f`) e interpreta en consola las mediciones de ritmo cardíaco correspondientes (`72 BPM`, `255 BPM`, `63 BPM`).

</div>

<div align="center">

![img18](img/img18.png)

*Recepción continua de notificaciones*
</div>

En la última imagen se puede visualizar

* Recepción periódica de notificaciones (6821–14821 ms):** El cliente recibe tramas de 2 bytes cada segundo desde el servidor (`00 b8`, `00 84`, `00 0a`, `00 72`, `00 a2`, `00 2a`, `00 4c`, `00 ae`), decodificando y mostrando por consola las mediciones correspondientes en pulsaciones por minuto (`184 BPM`, `132 BPM`, `10 BPM`, `114 BPM`, `162 BPM`, `42 BPM`, `76 BPM` y `174 BPM`).
* Desconexión por tiempo de espera (21711 ms):** Se produce una pérdida de enlace con el servidor `60:55:f9:bc:53:ba` por *Connection Timeout* (`reason 0x08`, habitual cuando se apaga o reinicia el servidor).
* Reinicio automático del escaneo (21711–21721 ms):** Al ejecutarse el evento `ESP_GATTC_DISCONNECT_EVT`, el cliente restablece su estado y vuelve a iniciar el escaneo BLE (`Scanning start successfully`) para buscar y reconectarse nuevamente al servidor.

</div>

<div align="center">

![img19](img/img19.png)

*Recuperación automática ante una desconexión*

</div>