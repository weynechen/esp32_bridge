idf_component_register(
    SRCS 
        "src/pmu.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        "device"
        "common"
) 

# 添加编译选项，禁用异常支持
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-exceptions) 