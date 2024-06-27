# Embed the server root certificate into the final binary
#
# (If this was a component, we would set COMPONENT_EMBED_TXTFILES here.)
idf_component_register(SRCS "stack.c" "http_client_handler.c" "wifi_connect.c" "spi_transmit.c" "time_sync.c" "main.c"
                    INCLUDE_DIRS "include"
                    EMBED_TXTFILES server_root_cert.pem local_server_cert.pem)
