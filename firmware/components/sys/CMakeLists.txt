file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../../main
                       REQUIRES i2c lepton nvs_flash video)

