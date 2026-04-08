idf_component_register(SRCS "demod1090.c" "mode-s.c"  "mqttclient.c"  "usb_host_lib_main.c" "demod1090.c" "mode-s.c" "class_driver.c" "esp_libusb.c" "tuner_r82xx.c" "tuner_fc2580.c" "tuner_fc0013.c" "tuner_fc0012.c" "tuner_e4k.c" "ota_handler.c"  "librtlsdr.c" 
                    INCLUDE_DIRS "." "../components/esp32-wifi-bootstrap"
                    PRIV_REQUIRES usb esp_driver_gpio esp32-wifi-bootstrap mqtt app_update esp_https_ota nvs_flash
                                  EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem
                    )
set_source_files_properties(tuner_r82xx.c "class_driver.c"
    PROPERTIES COMPILE_FLAGS
    "-Wno-unused-function -Wno-unused-variable -Wno-incompatible-pointer-types"
)
