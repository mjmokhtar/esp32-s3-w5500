# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

#idf_component_register(
#    SRCS main.c         # list the source files of this component
#    INCLUDE_DIRS        # optional, add here public include directories
#    PRIV_INCLUDE_DIRS   # optional, add here private include directories
#    REQUIRES            # optional, list the public requirements (component names)
#    PRIV_REQUIRES       # optional, list the private requirements
#)

idf_component_register(SRCS 
							main.c 
							rgb_led.c 
							ethernet_app.c 
							wifi_app.c 
							http_server.c 
							app_nvs.c 
							wifi_reset_button.c 
							sntp_time_sync.c
						INCLUDE_DIRS "."
						EMBED_FILES 
							webpage/app.css	
							webpage/app.js	
							webpage/index.html
							webpage/favicon.ico	
							webpage/app.js	
							webpage/jquery-3.3.1.min.js
						)