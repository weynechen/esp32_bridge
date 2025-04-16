idf_component_register(
    SRCS 
        "src/network_module.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        "common"
        "nvs_flash"
        "esp_wifi"
        "lwip"
        "esp_event"
) 

# 添加编译选项，禁用异常支持
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-exceptions) 