#ifndef _MY_NVS_H_
#define _MY_NVS_H_

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#define NVS_NODEID_NAMESPACE  "NodeId"      //namespace最长15字节
#define NVS_NODEID_NUM        "NodeNum"      //namespace最长15字节
#define NVS_NODEID_KEY       "nodeId"      //key最长15字节
#define NODE_DATA_NAMESPACE  "Data_TH"      //阈值
#define NODE_TEMP_KEY       "temp"      //阈值
#define NODE_HUMI_KEY       "humi"      //阈值
#define NODE_LIGHT_KEY       "light"      //阈值
#define NODE_SOIL_KEY       "soil"      //阈值 
size_t read_nvs_str(const char* namespace,const char* key,char* value,int maxlen);
esp_err_t write_nvs_str(const char* namespace,const char* key,const char* value);
size_t read_nvs_blob(const char* namespace,const char* key,uint8_t *value,int maxlen);
esp_err_t erase_nvs_key(const char* namespace,const char* key);
esp_err_t write_nvs_blob(const char* namespace,const char* key,uint8_t* value,size_t len);


size_t readNodesId(uint8_t nodeId,char* value,int maxlen);
esp_err_t writeNodesId(uint8_t nodeId,const char* value);
esp_err_t write_Data_Threshold(const char* key,uint16_t value);
esp_err_t write_nvs_u8(const char* namespace,const char* key,uint8_t value);
uint16_t read_Data_Threshold(const char* key);
uint8_t read_nvs_u8(const char* namespace,const char* key);
esp_err_t erase_nvs_id(const uint8_t id);
esp_err_t erase_nvs_namespace(const char* namespace_name);
#endif // _MY_NVS_H_
