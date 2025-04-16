idf_component_register(
    SRCS 
        "src/battery_manager.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        "common"
        "device"
        "driver"
        "esp_adc"
) 

# 添加编译选项，禁用异常支持
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-exceptions) 