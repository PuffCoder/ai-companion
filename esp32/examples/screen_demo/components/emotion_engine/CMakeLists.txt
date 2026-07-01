idf_component_register(
    SRCS "emotion_engine.c"
    INCLUDE_DIRS "."
    REQUIRES display_driver freertos log
)
