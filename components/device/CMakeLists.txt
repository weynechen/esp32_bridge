idf_component_register(
    SRCS 
        "device_manager.cpp"
        "uart_device.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        "common"
        "network"
        "driver"
) 

# 添加编译选项，禁用异常支持
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-exceptions) 