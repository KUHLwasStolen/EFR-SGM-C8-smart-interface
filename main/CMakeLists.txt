idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES  "website/index.html"
                                    "website/styles.css"
                                    "website/updater.js"
                                    "../YOUR_CONFIGURATION/YOUR_AP_SSID.txt"
                                    "../YOUR_CONFIGURATION/YOUR_AP_PASSWORD.txt"
                                    "../YOUR_CONFIGURATION/YOUR_TIMEZONE.txt"
                    EMBED_FILES "website/favicon.svg")