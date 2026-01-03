idf_component_register(
    SRCS "mqtt.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES "emqxsl-ca.crt"
    REQUIRES Common
)
