# idf_component_register(
#     SRCS 
#     "TS.c"
#     "dht11.c"  
#     INCLUDE_DIRS "."
#     REQUIRES esp_adc
#     driver
#    # spi_flash   # 可选，根据驱动实际需求添加
# )
set(src_dirs
            DHT11
            BH1750
            IIC
            LORA
            WIFI
            MQTT
            TS
            WARE)

set(include_dirs
            DHT11
            BH1750
            IIC
            LORA
            WIFI
            MQTT
            TS
            WARE)

set(requires
            driver
            esp_adc
            nvs_flash
            mqtt
            esp_wifi)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)