idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES  "website/index.html"
                                    "website/styles.css"
                                    "website/updater.js"
                                    "website/settings/settings.html"
                                    "website/settings/settings.js"
                    EMBED_FILES "website/favicon.svg")