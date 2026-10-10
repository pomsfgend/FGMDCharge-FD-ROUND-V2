idf_component_register(SRCS "hooks.c" PRIV_REQUIRES bootloader_support spi_flash)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u bootloader_after_init")
