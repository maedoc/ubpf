idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES ubpf_esp32
                       EMBED_FILES "filters/producer.o" "filters/consumer.o" "filters/init.o")
