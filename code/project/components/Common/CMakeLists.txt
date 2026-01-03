# components/Common/CMakeLists.txt（最终正确版）
idf_component_register(
    SRCS ""
    INCLUDE_DIRS "."
    # 合并自定义组件 + 内置组件依赖，全部用 REQUIRES 声明（确保传递）
    REQUIRES 
        # 自定义组件
        WiFi IIC DHT11 BH1750 LORA CustomMQTT TS WARE
        # ESP-IDF 内置组件（关键：从 PRIV_REQUIRES 改为 REQUIRES）
        driver esp_adc esp_wifi esp-tls mqtt nvs_flash log freertos esp_event lwip
)