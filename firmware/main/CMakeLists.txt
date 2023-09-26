set(SOURCES main.c ctrl_task.c lep_task.c mon_task.c video_task.c)
idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS .
                    REQUIRES i2c lepton sys video)
