# esp8266Switch
Alternate firmware for EcoPlug embedded wifi switch
www.thegreatgeekery.com

1) Perform initial configuration through captive portal
2) Go to the device_name.local and set configuration parameters
3) MQTT topics is /device_name/switch

Example openhab device:
Switch TestPlug "Test Eco Plug" (all) {mqtt=">[broker:/ecoplug/switch:command:on:ON],>[broker:/ecoplug/switch:command:off:OFF]
