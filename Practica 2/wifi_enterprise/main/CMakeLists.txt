# TTLS/PAP needs only the server CA, no client certificate or private key.
idf_component_register(SRCS "wifi_enterprise_main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_wifi wpa_supplicant nvs_flash
                    EMBED_TXTFILES ca.pem)
