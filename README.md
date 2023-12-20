# singsang

Music player for an [M5Stack Core2](https://m5stack.com/collections/all/products/m5stack-core2-esp32-iot-development-kit), which is based on an ESP32 Microcontroller.
The simple GUI is specifically designed for children, who are able to choose a song based on the displayed album art.

![M5Stack Core2 running singsang](/media/singsang.jpg?raw=true)


## Features
- [x] aac, flac, mp3, opus, vorbis (ogg/oga), wav playback from SD card with a simple GUI
- [x] Recording to wav file format from internal microphone (never remove SD card while recording)
- [x] Hörbert-style playlists based on directories (use first 10 directories)
- [x] Add battery status indicator, including voice output (Swiss German)
- [x] Resume last song played on power-on
- [x] Timeout for touchscreen clicks to prevent multiple activations
- [x] Turn itself off, if it does not play for 5 minutes
- [x] Sleep mode with fade-out
- [x] Display Album art, if "/logo.jpg" existent on SD, needs to be 200 x 200 px
- [x] External stereo output mode for RCA Module 13.2 (can be selected using volume widget)
- [x] Support for M5GO Bottom RGB LED's to be used as night light (or just for fun)
- [x] Reduce pops & clicks in beginning of playback and recordings
- [x] Always use safe low default volume
- [ ] Use RGB leds as volume indicator (for parents) or spectrum/FFT
- [ ] Support bluetooth output to headphones or speakers

### More Features
- support for recursive file listing
- start next song if nothing is playing (auto-skips non-supported formats)
- playback position selection on progress bar
- identify SD and apply last index to correct SD
- missing SD card triggers power off
- for more information check commit summaries ...

## Considerations
(Album art is only displayed properly if it is embedded in the Mp3 file as Jpeg of size 400 x 400.)


## Credit
This project uses the [M5Core2](https://github.com/m5stack/M5Core2) library to access peripherals and [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) for high quality audio replay.
The beautiful [Giraffe](https://publicdomainvectors.org/en/free-clipart/Cartoon-giraffe-image/49785.html) logo is taken from publicdomainvectors.org. The buttons are based on the [Tabler](https://github.com/tabler/tabler-icons) icon set. 
Text-to-speech synthesis using [ttsfree.com](https://ttsfree.com/text-to-speech/german).


