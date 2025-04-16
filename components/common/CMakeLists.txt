idf_component_register(
    SRCS 
        "event_system.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        "esp_common"
) 

# 添加编译选项，禁用异常支持
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-exceptions) 