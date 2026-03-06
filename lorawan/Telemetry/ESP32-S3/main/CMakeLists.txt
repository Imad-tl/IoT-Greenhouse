idf_component_register(SRCS "gateway_s3.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_driver_uart bt nvs_flash mbedtls)