# lunalamp-esp32
ESP32 ESP-IDF based HomeKit integrated RGBWW lamp

basically it is an esp-idf homekit-sdk lightbulb example with the lightbulb hardware handling implemented

there are lots of solutions for RGBW lamp colorimetry out there, but not much for RGBWW, so i wrote my own crude algorithm for finding the ratio of warm white & cold white for a given linear-space RGB value

criteria for the algorithm i tried to meet: 
* RGB 1,1,1 -> RGBWwWc 0,0,0,0,1
* RGB ~1,0.5,0.1 (2700K white) -> RGBWwWc 0,0,0,1,0
* smooth transition everywhere else

since i'm not trying to really color-match anything here, i (kind of) calculate the distance between the RGB point and the WW&WC rays starting at 0,0,0. and this two distances i use as a mix ratio. after that i subtract as much mixed white as possible, everything else goes to the RGB channels, applying per-channel intensity coefficients

the actual warm-white value i took from the Home iPhone app - third of the top row of the color temperature swatch, my warm-white led is rated at 2700K, and cold-white at 6500K

build&flash&monitor: **idf.py flash monitor**

for other stuff check the esp-idf documentation
