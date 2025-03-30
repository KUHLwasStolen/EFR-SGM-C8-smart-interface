idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES  "website/index.html"
                                    "../YOUR_AP_CREDENTIALS/YOUR_AP_SSID.txt"
                                    "../YOUR_AP_CREDENTIALS/YOUR_AP_PASSWORD.txt")