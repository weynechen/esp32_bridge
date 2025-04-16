idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "." 
                 "../components/common/include"
                 "../components/device/include"
                 "../components/network/include"
                 "../components/battery/include"
                 "../components/pmu/include"
                #  "../components/protocol/include"
    REQUIRES 
        device 
        common
        network
        battery
        pmu
        esp_event
        esp_adc
) 

# 添加编译选项，禁用异常支持
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-exceptions) 