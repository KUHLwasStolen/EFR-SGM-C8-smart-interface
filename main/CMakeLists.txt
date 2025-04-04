idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES  "website/index.html"
                                    "website/styles.css"
                                    "website/updater.js"
                                    "../YOUR_AP_CREDENTIALS/YOUR_AP_SSID.txt"
                                    "../YOUR_AP_CREDENTIALS/YOUR_AP_PASSWORD.txt"
                    EMBED_FILES "website/favicon.png")