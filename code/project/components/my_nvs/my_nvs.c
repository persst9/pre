#include "my_nvs.h"
static const char* TAG = "my_nvs";

/** 从nvs中读取字符值
 * @param namespace NVS命名空间
 * @param key 要读取的键值
 * @param value 读到的值
 * @param maxlen 外部存储数组的最大值
 * @return 读取到的字节数
*/
size_t read_nvs_str(const char* namespace,const char* key,char* value,int maxlen)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret_val = ESP_FAIL;
    size_t required_size = 0;
    ESP_ERROR_CHECK(nvs_open(namespace, NVS_READWRITE, &nvs_handle));
    ret_val = nvs_get_str(nvs_handle, key, NULL, &required_size);
    if(ret_val == ESP_OK && required_size <= maxlen)
    {
        nvs_get_str(nvs_handle,key,value,&required_size);
    }
    else
        required_size = 0;
    nvs_close(nvs_handle);
    return required_size;
}

/** 写入值到NVS中（字符数据）
 * @param namespace NVS命名空间
 * @param key NVS键值
 * @param value 需要写入的值
 * @return ESP_OK or ESP_FAIL
*/
esp_err_t write_nvs_str(const char* namespace,const char* key,const char* value)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    ESP_ERROR_CHECK(nvs_open(namespace, NVS_READWRITE, &nvs_handle));
    
    ret = nvs_set_str(nvs_handle, key, value);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return ret;
}
esp_err_t write_nvs_u8(const char* namespace,const char* key,uint8_t value)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    ESP_ERROR_CHECK(nvs_open(namespace, NVS_READWRITE, &nvs_handle));
    
    ret = nvs_set_u8(nvs_handle, key, value);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return ret;
}
uint8_t read_nvs_u8(const char* namespace,const char* key)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    uint8_t value = 0;
    ESP_ERROR_CHECK(nvs_open(namespace, NVS_READWRITE, &nvs_handle));
    ret = nvs_get_u8(nvs_handle, key, &value);
    nvs_close(nvs_handle);
    return value;
}
esp_err_t write_nvs_u16(const char* namespace,const char* key,uint16_t value)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    ESP_ERROR_CHECK(nvs_open(namespace, NVS_READWRITE, &nvs_handle));
    
    ret = nvs_set_u16(nvs_handle, key, value);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return ret;
}
uint16_t read_nvs_u16(const char* namespace,const char* key)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    uint16_t value = 0;
    ESP_ERROR_CHECK(nvs_open(namespace, NVS_READWRITE, &nvs_handle));
    ret = nvs_get_u16(nvs_handle, key, &value);
    nvs_close(nvs_handle);
    return value;
}
/** 从nvs中读取字节数据（二进制）
 * @param namespace NVS命名空间
 * @param key 要读取的键值
 * @param value 读到的值
 * @param maxlen 外部存储数组的最大值
 * @return 读取到的字节数
*/
size_t read_nvs_blob(const char* namespace,const char* key,uint8_t *value,int maxlen)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret_val = ESP_FAIL;
    size_t required_size = 0;
    ESP_ERROR_CHECK(nvs_open(namespace, NVS_READWRITE, &nvs_handle));
    ret_val = nvs_get_blob(nvs_handle, key, NULL, &required_size);
    if(ret_val == ESP_OK && required_size <= maxlen)
    {
        nvs_get_blob(nvs_handle,key,value,&required_size);
    }
    else
        required_size = 0;
    nvs_close(nvs_handle);
    return required_size;
}

/** 擦除nvs区中某个键
 * @param namespace NVS命名空间
 * @param key 要读取的键值
 * @return 错误值
*/
esp_err_t erase_nvs_key(const char* namespace,const char* key)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret_val = ESP_FAIL;
    ESP_ERROR_CHECK(nvs_open(namespace, NVS_READWRITE, &nvs_handle));
    ret_val = nvs_erase_key(nvs_handle,key);
    ret_val = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return ret_val;
}
esp_err_t erase_nvs_id(const uint8_t id)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret_val = ESP_FAIL;
    char str[15];
    snprintf(str, sizeof(str), "%s%u", NVS_NODEID_KEY,id);
    ESP_ERROR_CHECK(nvs_open(NVS_NODEID_NAMESPACE, NVS_READWRITE, &nvs_handle));
    ret_val = nvs_erase_key(nvs_handle,str);
    ret_val = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return ret_val;
}
/** 写入值到NVS中(字节数据)
 * @param namespace NVS命名空间
 * @param key NVS键值
 * @param value 需要写入的值
 * @return ESP_OK or ESP_FAIL
*/
esp_err_t write_nvs_blob(const char* namespace,const char* key,uint8_t* value,size_t len)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    ESP_ERROR_CHECK(nvs_open(namespace, NVS_READWRITE, &nvs_handle));
    ret = nvs_set_blob(nvs_handle, key, value,len);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t writeNodesId(const uint8_t id,const char* value)
{
    char str[15];
    snprintf(str, sizeof(str), "%s%u", NVS_NODEID_KEY, id);
    return write_nvs_str(NVS_NODEID_NAMESPACE,str,value);
}

size_t readNodesId(const uint8_t id,char* value,int maxlen)
{
    char str[15];
    snprintf(str, sizeof(str), "%s%u", NVS_NODEID_KEY, id);
    return read_nvs_str(NVS_NODEID_NAMESPACE,str,value,maxlen);
}

esp_err_t write_Data_Threshold(const char* key,uint16_t value)
{
    esp_err_t ret_val;
    char str[16];
    snprintf(str,16,"%u",value);
    ret_val = write_nvs_u16(NODE_DATA_NAMESPACE,key,str);
    return ret_val;
}

uint16_t read_Data_Threshold(const char* key)
{
    uint16_t value = 0;
    read_nvs_u16(NODE_DATA_NAMESPACE,key);
    if(value == 0) return 0;
    return value;//返回阈值
}

esp_err_t erase_nvs_namespace(const char* namespace_name)
{
    // 1. 检查入参合法性
    if (namespace_name == NULL || strlen(namespace_name) == 0) {
        ESP_LOGE(TAG, "Invalid namespace name");
        return ESP_ERR_INVALID_ARG;
    }

    // 2. 打开NVS分区（默认分区名称为 "nvs"，如果自定义了分区表需修改）
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s', err=0x%x", namespace_name, err);
        return err;
    }

    // 3. 擦除当前命名空间的所有数据
    err = nvs_erase_all(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase NVS namespace '%s', err=0x%x", namespace_name, err);
        nvs_close(nvs_handle); // 即使擦除失败，也要关闭句柄
        return err;
    }

    // 4. 提交擦除操作（必须提交，否则擦除不会生效）
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit erase for '%s', err=0x%x", namespace_name, err);
        nvs_close(nvs_handle);
        return err;
    }

    // 5. 关闭NVS句柄，释放资源
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Successfully erased all data in NVS namespace: %s", namespace_name);

    return ESP_OK;
}

