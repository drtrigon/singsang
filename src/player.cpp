#include "player.hpp"

namespace singsang
{
CPlayer::CPlayer() {}

void CPlayer::begin()
{
    initializeHardware();
    populateMusicFileList();
    initializeGui();

    // battery warning audio
    if (((M5.Axp.GetBatVoltage() - 3.2) * 100) < 20)
        m_audio->connecttoFS(SPIFFS, "audio-battery.mp3");

    // initialize Player (should be put into own method)
    loadConfiguration("/status");
    --m_activeSongIdx[m_activeSongIdxIdx];  // we start using startNextSong() in loop()
}

void CPlayer::loop()
{
    if (!SD.exists("/")) {  // SD card removed
        M5.Axp.PowerOff();  // power off alternatively use ESP.restart() after detection of SD card
    }

    m_audio->loop();

    handleTouchEvents();

    updateGui();

    handleInactivityTimeout();

    // skip non-playable files
    if (m_isRunning && (!m_audio->isRunning())) {  // not playing e.g. due to not supported file format (hacky work-a-round)
// repeat could be done here by a simple --m_activeSongIdx[m_activeSongIdxIdx]
        startNextSong();
    }

    // sleep fade-out timer (linear volume decrease; 1 step every 5 mins)
    if (m_sleepMode) {
        if ((millis() - m_sleepMode) >= (5 * 60 * 1000)) {
			decreaseVolume();
			m_sleepMode = millis();

            if (m_currentVolume <= 0) {
                pauseSong();      // will eventually trigger handleInactivityTimeout()
				m_sleepMode = 0;  // disable sleep mode
            }
        }
    }

    // store current song position (which memory? SPIFFS? may be in given intervals of x secs only, e.g. every 10 secs or so and m_audio->isRunning()...?)
}

void CPlayer::initializeHardware()
{
    M5.begin(true, true, true, false, kMBusModeOutput, false);  // SpeakerEnable = false

    M5.Axp.SetLed(false);
    M5.Axp.SetLcdVoltage(1800);  // dimmed, nominal value is 2800
	M5.Axp.SetCHGCurrent(AXP192::kCHG_1320mA);  // maximum, see https://github.com/m5stack/M5Core2/blob/master/src/AXP192.h#L23
    //M5.Axp.SetSpkEnable(true);
    updateSpeaker();

    if (!SPIFFS.begin()) {  // Start SPIFFS, return 1 on success.
        //M5.Lcd.setTextSize(2);  //Set the font size to 2.
        //M5.Lcd.println("SPIFFS Failed to Start.");
        Serial.println("SPIFFS Failed to Start.");
    }

    WiFi.mode(WIFI_OFF);

#ifdef M5GO_BOTTOM
    FastLED.addLeds<SK6812, 25, GRB>(leds, 10);  // GRB ordering is assumed, DATA_PIN=25, NUM_LEDS=10
	FastLED.clear(true);
    //FastLED.setBrightness(64);
    FastLED.setBrightness(8);  // min. 8, better: 16 to get all colors in a smooth behaviour for colorwheel
/*    fill_solid(leds, 10, CRGB::Blue);
    FastLED.show();
    delay(400);
    fill_solid(leds, 10, CRGB::Red);
    FastLED.show();
    delay(400);
    fill_solid(leds, 10, CRGB::Green);
    FastLED.show();
    delay(300);*/
	fill_rainbow(leds, 10, 0, 26);  // last parameter is deltahue and should be something like 255/10
    FastLED.show();
/*    delay(5000);
    fill_gradient_RGB(leds, 0, CRGB::Blue, 9, CRGB::Green);
    FastLED.show();
    delay(5000);*/
/*    for(uint16_t i = 0; i < 10; ++i) {
        Serial.printf("%i %i %i %i\n", i, leds[i].r, leds[i].g, leds[i].b);
    }*/
#endif

    delay(100);

    // internal speaker (default)
    m_audio->setPinout(12, 0, 2);
#ifdef RCA_MODULE
    // RCA Module 13.2 (can be used at same time)
    m_audio->setPinout(19, 0, 2);  // I2S_BCLK, I2S_LRC, I2S_DOUT
#endif
    // prevent clic-noise when setting volume after init of audio lib (work-a-round)
    m_audio->setVolume(0);
    m_audio->connecttoFS(SPIFFS, "audio-battery.mp3");  // any file available can be used
    for (uint8_t i = 0; i < 100; ++i)  // 50 iterations should be enough, take x2
        m_audio->loop();
    m_audio->stopSong();
    m_audio->setVolume(m_currentVolume);

    vibrate();
}

void CPlayer::initializeGui()
{
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextFont(2);
    M5.Axp.SetDCDC3(true);  // backlight on (actually hardware not software init)
  
    switch(ui_PageNumber) {
      case 1:
        // playlists/hörbert

        for (unsigned int i=0; i < m_songDirs.size(); ++i)
        {
            M5.Lcd.fillRect((i % 4) * 80, (i / 4) * 80, 80, 80, m_colors[i]);
        }
        M5.Lcd.fillRoundRect(2 * 80, 2 * 80, 80, 80, 10, m_colors[10]);  // "/rec"
        M5.Lcd.fillRoundRect(3 * 80, 2 * 80, 80, 80, 10, m_colors[11]);  // "/" (all)
		break;

      /*case 2:
        // record
		break;*/

      case 3:
        // color wheel

        c_ColorWheelWidget.draw(false);
		break;

      case 4:
        // display off / lock

        M5.Axp.SetDCDC3(false);  // backlight off
		break;

      default:  // case 0
        // player 

        if (SD.exists("/logo.jpg"))
        {
            M5.Lcd.drawJpgFile(SD, "/logo.jpg", 60, 20, 200, 200);
        }
        else
        {
            M5.Lcd.drawJpgFile(SPIFFS, "/logo.jpg", 60, 20, 200, 200);
        }

        m_batteryWidget.draw(false);
        m_fileSelectionWidget.draw(false);
        m_nextSongWidget.draw(false);
        m_prevSongWidget.draw(false);
        m_pauseSongWidget.draw(false);
        m_progressWidget.draw(false);
        m_volumeWidget.draw(false);
        m_sleepTimerWidget.draw(false);
    }
}

void CPlayer::appendSDDirectory(File dir, const bool list_dir = false)
{
    bool nextFileFound;
    do
    {
        nextFileFound = false;

        File entry = dir.openNextFile();

        if (entry)
        {
            nextFileFound = true;

            if (entry.isDirectory())
            {
                // add folder to playlist, if ...
                // (limit to 10 here to save memory, otherwise
                // could also be done later, e.g. in populateMusicFileList())
                if ((list_dir) &&                                // ... enabled by list_dir switch
				    (strncmp(entry.path(), "/rec" , 4) != 0) &&  // ... not "/rec"
					(m_songDirs.size() < 10))                    // ... size < 10 (9 items + 1 added = 10 total)
                {
                    m_songDirs.push_back(entry.path());
                }

				appendSDDirectory(entry);
            }
			else
            {
                if (entry.size() > 0)  // entryIsFile
                {
                    m_songFiles.push_back(entry.path());
                }
            }

            entry.close();
        }
    } while (nextFileFound);
}

void CPlayer::populateMusicFileList()
{
    SD.mkdir("/rec");  // disable to enable locking of SD by not creating this folder manually

    File musicDir = SD.open("/");

    appendSDDirectory(musicDir, true);
	m_songFilesSize = m_songFiles.size();  // count files in "/" (all) on SD

#ifdef DEBUG
    for(int i=0; i < m_songFiles.size(); ++i)
        Serial.println(m_songFiles[i]);
#endif

    Serial.print("MusicFileList length: ");
    Serial.println(m_songFiles.size());
}

void CPlayer::handleInactivityTimeout()
{
    if (m_audio->isRunning())
    {
        m_lastActivityTimestamp = millis();
    }
    else
    {
        const auto currentTimestamp = millis();
        const bool isTimeoutReached =
            (currentTimestamp >
             (m_lastActivityTimestamp + m_turnOffAfterInactiveForMilliSec));

        if (isTimeoutReached)
        {
            m_audio->stopSong();
            if (ui_PageNumber == 3)    // color wheel (night light mode)
            {
                M5.Axp.DeepSleep(0U);  // sleep (keeping RGB LEDs and other peripherie powered)
            }
            else
            {
                M5.Axp.PowerOff();     // power off
            }
        }
    }
}

void CPlayer::handleTouchEvents()
{
/*    if (m_sleepMode)
    {
        // consider locking the UI completely (except for reset) in order to be "baby/children-proof"
        return;
    }*/

    const auto touchPoint = M5.Touch.getPressPoint();

    //Serial.print(M5.BtnA.read());
    //Serial.print(M5.BtnB.read());
    //Serial.println(M5.BtnC.read());
	// not working, see also https://docs.m5stack.com/en/api/core2/button and https://github.com/m5stack/M5Core2/blob/master/examples/Basics/button/button.ino

    const bool isButtonPressed = (touchPoint.x > -1 && touchPoint.y > -1);
    if (!isButtonPressed)
    {
        m_lastRecordTouchTimestamp = 0;
        return;
    }

    switch(ui_PageNumber) {
      case 1:
      {
        // playlists/hörbert

        File musicDir;
        const unsigned int i = (touchPoint.y / 80) * 4 + (touchPoint.x / 80);
        if (i < m_songDirs.size())  // always (size <= 10), due to limit in appendSDDirectory()
        {
            musicDir = SD.open(m_songDirs[i].c_str());
        }
        else if (i == 10)  // "/rec"
        {
            musicDir = SD.open("/rec");
        }
        else if (i == 11)  // "/" (all)
        {
            musicDir = SD.open("/");
        }
        //else
        //{
        //    break;
        //}

        if (musicDir)  // very similar to 'populateMusicFileList()' thus try to combine
        {
			vibrate();

            m_activeSongIdxIdx = i;  // index is valid (in range and playlist/folder exists)

            M5.Lcd.fillScreen(m_colors[m_activeSongIdxIdx]);  // overwrites UI

            m_songFiles.clear();

            appendSDDirectory(musicDir);
			m_isRunning &= (m_songFiles.size() > 0);
            //m_activeSongIdx[m_activeSongIdxIdx] = -1;
            --m_activeSongIdx[m_activeSongIdxIdx];
			startNextSong();

            initializeGui();  // re-initialize the UI

            Serial.print("MusicFileList length: ");
            Serial.println(m_songFiles.size());
        }

        break;
      }
      /*case 2:
        // record
		break;*/

      case 3:
        // color wheel

        if (c_ColorWheelWidget.isTouched(touchPoint))  // if there is this widget only, this 'if-statement' could be removed
        {
            vibrate();

#ifdef M5GO_BOTTOM
            // see also https://github.com/m5stack/M5-ProductExampleCodes/blob/master/Base/M5GO_BOTTOM2/M5GO_BOTTOM2.ino
            // see also https://docs.m5stack.com/en/base/m5go_bottom2
            Serial.printf("heap: %i\n", ESP.getFreeHeap());
            Serial.printf("stack: %d\n", uxTaskGetStackHighWaterMark(NULL));
            // update RGB LEDs
// !!! TODO: does not work properly and interferes with init above - mostly when connected to USB...???
            fill_solid(leds, 10, c_ColorWheelWidget.getPosition(touchPoint));  // NUM_LEDS=10
            FastLED.show();
#endif
        }

		break;

      case 4:
        // display off / lock

		break;

      default:  // case 0
        // player 

        if (m_nextSongWidget.isTouched(touchPoint))
        {
            vibrate();
            startNextSong();
        }

        if (m_prevSongWidget.isTouched(touchPoint))
        {
            vibrate();
            startPrevSong();
        }

        if (m_pauseSongWidget.isTouched(touchPoint))
        {
            vibrate();
            pauseSong();
        }

        if (m_volumeWidget.isTouched(touchPoint))
        {
            vibrate();
            if (m_volumeWidget.getButton(touchPoint)) {
                if (m_currentVolume == 0) {
                    m_outputMode = (m_outputMode + 1) % 2;
                    updateSpeaker();
                }
                decreaseVolume();
            } else {
                increaseVolume();
            }
        }

        if (m_progressWidget.isTouched(touchPoint))
        {
            vibrate();
            setPosSong(m_progressWidget.getPosition(touchPoint));
        }

        if (m_sleepTimerWidget.isTouched(touchPoint))
        {
            vibrate();
            toggleSleepTimer();
        }
    }

    if (touchPoint.y >= 250)  // "capacitive" Buttons A, B, C below display
    {
		// actually you can set-up any number of buttons, e.g. 5 or more)
        if ((0 <= touchPoint.x) && (touchPoint.x <= 100))         // BtnA
        {
            Serial.println("M5.BtnA");
            if (!m_lastRecordTouchTimestamp)
            {
                vibrate();
                m_lastRecordTouchTimestamp = millis();
            }
            else if ((millis() - m_lastRecordTouchTimestamp) > (3 * 1000))  // nach 3s
            {
                m_lastRecordTouchTimestamp = 0;
//                delay(500);  // touch dead-time hack (stops playing)
                String rec_name;
                for(unsigned int i=0; i<10000; ++i)
                {
                    rec_name = String("/rec/singsang") + String(i) + String(".wav");
                    if (!SD.exists(rec_name))
                        break;
                }
                rec_record(rec_name.c_str());
                ++m_songFilesSize;  // total file number; playlist "/" (all)
                vibrate();
                m_songFiles.push_back(rec_name);
                m_activeSongIdx[m_activeSongIdxIdx] = m_songFiles.size() - 2;
                startNextSong();
            }
        }
        else if ((105 <= touchPoint.x) && (touchPoint.x <= 205))  // BtnB
        {
            Serial.println("M5.BtnB");
            vibrate();   // also touch dead-time of 150ms
//            delay(500);  // touch dead-time hack (stops playing)
        }
        else if ((210 <= touchPoint.x) && (touchPoint.x <= 310))  // BtnC
        {
            Serial.println("M5.BtnC");
            vibrate();   // also touch dead-time of 150ms
            ui_PageNumber = (ui_PageNumber + 1) % 5;
            if (ui_PageNumber == 2)  // skip page until implemented
                ++ui_PageNumber;     // "
            initializeGui();  // re-initialize the UI - hacky... also the switch cases...
            //delay(c_ColorWheelWidget.m_touchDeadTimeMilliSec);  // touch dead-time hack
//            delay(500);  // touch dead-time hack (stops playing)
        }
    }

//    Serial.printf("%i %i\n", touchPoint.x, touchPoint.y);
}

void CPlayer::startNextSong()
{
    if (m_audio->isRunning())
    {
        m_audio->stopSong();
    }

    if (m_songFiles.size() == 0)
    {
        return;
    }

    m_activeSongIdx[m_activeSongIdxIdx] = (m_activeSongIdx[m_activeSongIdxIdx] + 1) % m_songFiles.size();

    m_audio->connecttoSD(m_songFiles[m_activeSongIdx[m_activeSongIdxIdx]].c_str());

    saveConfiguration("/status");
}

void CPlayer::startPrevSong()
{
    if (m_audio->isRunning())
    {
        m_audio->stopSong();
    }

    if (m_songFiles.size() == 0)
    {
        return;
    }

    m_activeSongIdx[m_activeSongIdxIdx] = (m_activeSongIdx[m_activeSongIdxIdx] + m_songFiles.size() - 1) % m_songFiles.size();  // mod does not work for negative numbers

    m_audio->connecttoSD(m_songFiles[m_activeSongIdx[m_activeSongIdxIdx]].c_str());

    saveConfiguration("/status");
}

void CPlayer::pauseSong()
{
    if (m_songFiles.size() == 0)
    {
        return;
    }

    m_audio->pauseResume();

    m_isRunning = m_audio->isRunning();
}

void CPlayer::setPosSong(float f_relPos)
{
    if (!m_audio->isRunning())
    {
		return;
	}

    m_audio->pauseResume();

    m_audio->setAudioPlayPosition(f_relPos * m_audio->getAudioFileDuration());

    m_audio->pauseResume();
}

void CPlayer::updateGui()
{
    switch(ui_PageNumber) {
      case 1:
        // playlists/hörbert

// 320x240 unterteilen in 80er kacheln (ev. nur z.b. 76 von 2 bis 78 statt 0 bis 80 nahtlos aneinander)
//   -> 4x3 raster sind 12 einträge
// verwende playlist oder die ersten 12 verzeichnisse auf der sd karte
// jeder knopf eine andere farbe (siehe hörbert)

		break;

      /*case 2:
        // record

// oberfläche ähnlich wie player ohne icon, stattdessen record button (grosser roter punkt), und ohne sleep mode
// arbeitsverzeichnis ist nur /rec ordner
// nach abspielen von aktuellem titel wird gestoppt
// ausgabe auch via line out möglich? funktion analog zu add pin auch in M5 library?
// rec; file index++ dann aufnehmen, play; akteullen file index abspielen, prev; file index--, next; file index++ (existent?)
// when switching page to othe mode, restart esp, if playback was pause otherwise we are just skipping/blättern over the page (either make it the laste mode or store flag that tells us to got to page 3 color wheel)

// - need to re-install i2s driver, this is done in Audio class constructor, how to re-call? (to use method below it needs to be a pointer OR put i2s driver install into own member func) / Modify the source? Issue a feature request? / compare i2s driver from M5 'InitI2SSpeakOrMic' with the one in the constructor from Audio class (ESP32-audioI2S), any compromise possible? (look at source to figure out how to enable mic w/o issues)
// - alternatively as a hacky work-a-round just restart ESP to switch from record to replay mode again! may be use volatile mem instead of 'saveConfiguration'?
// - when using ESP.restart(); to store setting use Preferences library, non-volatile mem or SPIFFS ('saveConfiguration')
//     Preferences: https://community.m5stack.com/topic/1496/storing-settings-in-the-memory-of-the-core
//     non-volatile: https://www.esp32.com/viewtopic.php?t=4931
//     SPIFFS: 'saveConfiguration'
    /*delete m_audio; //must do this if it already has been allocated memory!
    m_audio = new Audio();
    initializeHardware();*//*
    saveConfiguration("");
    ESP.restart();

		break;*/

      case 3:
        // color wheel

        //c_ColorWheelWidget.update();
		break;

      case 4:
        // display off / lock

		break;

      default:  // case 0
        // player 

        m_batteryWidget.update();

        int audioProgressPercentage = 0;
        if (m_audio->isRunning() && m_audio->getAudioFileDuration() > 0)
        {
            audioProgressPercentage = 100. * m_audio->getAudioCurrentTime() /
                                      m_audio->getAudioFileDuration();
        }
        m_progressWidget.update(audioProgressPercentage);

        m_fileSelectionWidget.update(m_songFiles.size(), m_activeSongIdx[m_activeSongIdxIdx]);

        m_volumeWidget.update(m_currentVolume);

        m_sleepTimerWidget.update(m_sleepMode);

        m_pauseSongWidget.update(m_audio->isRunning());
    }
}

void CPlayer::updateVolume(int f_deltaVolume)
{
    constexpr int minVolume = 0;
    constexpr int maxVolume = 20;

    int newVolume = m_currentVolume + f_deltaVolume;

    if (newVolume < minVolume)
    {
        newVolume = minVolume;
    }
    if (newVolume > maxVolume)
    {
        newVolume = maxVolume;
    }

    m_currentVolume = newVolume;
    m_audio->setVolume(m_currentVolume);
}

void CPlayer::increaseVolume()
{
    updateVolume(+2);
}

void CPlayer::decreaseVolume()
{
    updateVolume(-2);
}

void CPlayer::toggleSleepTimer()
{
    if (m_sleepMode)
    {
        m_sleepMode = 0;  // disable sleep mode
    }
    else
    {
        m_sleepMode = millis();  // enable sleep mode
    }
}

void CPlayer::updateSpeaker()
{
    if (m_outputMode == 0) {
        // internal speaker enabled, mono
        M5.Axp.SetSpkEnable(true);
        m_audio->forceMono(true);
    } else {
        // internal speaker disabled, stereo (for external mode)
        M5.Axp.SetSpkEnable(false);
        m_audio->forceMono(false);
    }

    m_volumeWidget.setMode(m_outputMode);
}

void CPlayer::vibrate()
{
    M5.Axp.SetLDOEnable(3, true);
    delay(150);
    M5.Axp.SetLDOEnable(3, false);
}

void CPlayer::loadConfiguration(const char *filename)
{
    // Open file for reading
    File file = SPIFFS.open(filename, FILE_READ);

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/v6/assistant to compute the capacity.
    StaticJsonDocument<512> doc;

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    //DeserializationError error = deserializeMsgPack(doc, file);
    if (error)
        Serial.println(F("Failed to read file, using default configuration"));

    // Copy values from the JsonDocument to the Config
    if ((doc["sd_cardSize"] == SD.cardSize()) &&
        (doc["sd_totalBytes"] == SD.totalBytes()) &&
        (doc["sd_usedBytes"] == SD.usedBytes()) &&
        (doc["m_songFiles.size"] == m_songFilesSize))
	{
        JsonArray arr = doc["m_activeSongIdx"].as<JsonArray>();
        for (unsigned int i=0; i < 12; ++i) {
            m_activeSongIdx[i] = arr[i] | m_activeSongIdx[i];
        }
        // we do not restore volume as environmental conditions might have changed
		// instead we use always a safe low default to protect user
        //m_currentVolume = doc["m_currentVolume"] | m_currentVolume;
    }

    // Print values
    serializeJsonPretty(doc, Serial);
    Serial.println("");

    // Close the file (Curiously, File's destructor doesn't close the file)
    file.close();
}

void CPlayer::saveConfiguration(const char *filename)
{
    // Delete existing file, otherwise the configuration is appended to the file
    SPIFFS.remove("/status");

    // Open file for writing
    File file = SPIFFS.open(filename, FILE_WRITE);
    if (!file) {
        Serial.println(F("Failed to create file"));
        return;
    }

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/assistant to compute the capacity.
    StaticJsonDocument<512> doc;

    // Set the values in the document
    doc["sd_cardSize"]   = SD.cardSize();
    doc["sd_totalBytes"] = SD.totalBytes();
    doc["sd_usedBytes"]  = SD.usedBytes();
    JsonArray arr = doc.createNestedArray("m_activeSongIdx");
    for (unsigned int i=0; i < 12; ++i) {
        arr.add(m_activeSongIdx[i]);
    }
    doc["m_currentVolume"] = m_currentVolume;
	doc["m_songFiles.size"] = m_songFilesSize;
    //doc["..."] = m_audio->getAudioCurrentTime();  // (store current playing position)

    // Serialize JSON to file
    if (serializeJson(doc, file) == 0) {
    //if (serializeMsgPack(doc, file) == 0) {
        Serial.println(F("Failed to write to file"));
    }

#ifdef DEBUG
    // Print values
    serializeJsonPretty(doc, Serial);
    Serial.println("");
#endif

    // Close the file
    file.close();
}

void CPlayer::rec_record(const char *filepath) {  // see https://github.com/m5stack/M5Core2/blob/master/examples/Basics/record/record.ino
    M5.Lcd.fillCircle(280, 80, 10, TFT_DARKGREY);

    m_audio->stopSong();

    uint16_t microphonedata0[DATA_SIZE/2 * 1];  // DATA_SIZE = 1024 (must be multiple of 2 as data actually is 16 bit!)
    int data_offset = 0;
    size_t byte_read;

	File file = SD.open(filepath, FILE_WRITE);
    file.seek(44);  // before 44 is the header that gets written in the end

    M5.Spk.InitI2SSpeakOrMic(MODE_MIC);  // install mic i2s driver

    // supress clic-noise in recorded file, by skipping some data (1 buffer)
    i2s_read(Speak_I2S_NUMBER, (char *)(microphonedata0),
             DATA_SIZE, &byte_read, (100 / portTICK_RATE_MS));

    M5.Lcd.fillCircle(280, 80, 10, TFT_RED);

    while (1) {
        i2s_read(Speak_I2S_NUMBER, (char *)(microphonedata0),
                 DATA_SIZE, &byte_read, (100 / portTICK_RATE_MS));
        for (unsigned int i=0; i<(DATA_SIZE/2); ++i)  // apply gain
            microphonedata0[i] <<= 2;                 // gain x4 (2 bits)
        file.write((uint8_t*)microphonedata0, byte_read);  // may be use union instead of cast
        data_offset += byte_read;
        if (M5.Touch.ispressed() != true) break;  // may be also limit max. file size...?
    }

    M5.Lcd.fillCircle(280, 80, 10, TFT_DARKGREY);

	writeWavHeader(file, data_offset/2);  // wav header 16bit=2byte samples (compatibility with other software)

    file.close();

    // need to re-install ESP32-audioI2S i2s driver, this is done in Audio class constructor
    delete m_audio; // must do this if it already has been allocated memory!
    m_audio = new Audio();
// ALTERNATIVE: RESTART AND PLAY LAST SONG BY STORING ITS INDEX IN CONFIGURATION !!!
// ACHTUNG MACHT EV. PROBLEME - EINZELNE BEFEHLE HERAUSNEHMEN...?!
    initializeHardware();

    M5.Lcd.fillCircle(280, 80, 10, TFT_BLACK);
}

/*void CPlayer::rec_play(const char *filepath) {  // see https://github.com/m5stack/M5Core2/blob/master/examples/Basics/record/record.ino
    m_audio->stopSong();

    uint8_t microphonedata0[DATA_SIZE * 1];  // DATA_SIZE = 1024 (must be multiple of 2 as data actually is 16 bit!)
    int data_offset = 0;
    uint32_t subChunk2Size = 0;

    File file = SD.open(filepath, FILE_READ);

    file.seek(40);  // subChunk2Size: == NumSamples * NumChannels * BitsPerSample/8  i.e. number of byte in the data.
    file.read((uint8_t*)&subChunk2Size, 4);
    data_offset = subChunk2Size;

    file.seek(44);  // 44 byte header (ignored), followed by raw data (used)

	Serial.println(file.size());

    M5.Spk.InitI2SSpeakOrMic(MODE_SPK);  // install (low level) speaker i2s driver

    size_t bytes_written;
    while (data_offset > 0) {
        file.read(microphonedata0, DATA_SIZE);
        i2s_write(Speak_I2S_NUMBER, microphonedata0, DATA_SIZE, &bytes_written,
                  portMAX_DELAY);
        data_offset -= DATA_SIZE;
    }
    file.close();

    // need to re-install ESP32-audioI2S i2s driver, this is done in Audio class constructor
    delete m_audio; // must do this if it already has been allocated memory!
    m_audio = new Audio();
// ALTERNATIVE: RESTART AND ... (?)
// ACHTUNG MACHT EV. PROBLEME - EINZELNE BEFEHLE HERAUSNEHMEN...?!
    initializeHardware();
//    updateVolume(0);
}*/

void CPlayer::writeWavHeader(File wavFile, uint32_t NumSamples) {  // see https://stackoverflow.com/questions/66484763/how-to-convert-analog-input-readings-from-arduino-to-wav-from-sketch and http://soundfile.sapp.org/doc/WaveFormat/
    // alternative method to use enum; https://github.com/atomic14/esp32_sdcard_audio/blob/main/arduino-wav-sdcard/lib/wav_file/src/WAVFile.h

    /// The first 4 byte of a wav file should be the characters "RIFF" */
    uint8_t chunkID[4] = {'R', 'I', 'F', 'F'};
    /// 36 + SubChunk2Size
    uint32_t chunkSize = 36; // You Don't know this until you write your data but at a minimum it is 36 for an empty file
    /// "should be characters "WAVE"
    uint8_t format[4] = {'W', 'A', 'V', 'E'};
    /// " This should be the letters "fmt ", note the space character
    uint8_t subChunk1ID[4] = {'f', 'm', 't', ' '};
    ///: For PCM == 16
    uint32_t subChunk1Size = 16;
    ///: For PCM this is 1, other values indicate compression
    uint16_t audioFormat = 1;
    ///: Mono = 1, Stereo = 2, etc.
    uint16_t numChannels = 1;
    ///: Sample Rate of file
    uint32_t sampleRate = 44100;
    ///: SampleRate * NumChannels * BitsPerSample/8
    uint32_t byteRate = 44100 * 1 * 2;
    ///: The number of byte for one frame NumChannels * BitsPerSample/8
    uint16_t blockAlign = 1 * 2;
    ///: 8 bits = 8, 16 bits = 16
    uint16_t bitsPerSample = 16;    // i2s driver in 'InitI2SSpeakOrMic' uses I2S_BITS_PER_SAMPLE_16BIT "// Fixed 12-bit stereo MSB."
    ///: Contains the letters "data"
    uint8_t subChunk2ID[4] = {'d', 'a', 't', 'a'};
    ///: == NumSamples * NumChannels * BitsPerSample/8  i.e. number of byte in the data.
    uint32_t subChunk2Size = 0; // You Don't know this until you write your data

    subChunk2Size = NumSamples * numChannels * bitsPerSample/8;
    chunkSize = 36 + subChunk2Size;

    wavFile.seek(0);
    wavFile.write(chunkID,4);
    wavFile.write((uint8_t*)&chunkSize,4);
    wavFile.write(format,4);
    wavFile.write(subChunk1ID,4);
    wavFile.write((uint8_t*)&subChunk1Size,4);
    wavFile.write((uint8_t*)&audioFormat,2);
    wavFile.write((uint8_t*)&numChannels,2);
    wavFile.write((uint8_t*)&sampleRate,4);
    wavFile.write((uint8_t*)&byteRate,4);
    wavFile.write((uint8_t*)&blockAlign,2);
    wavFile.write((uint8_t*)&bitsPerSample,2);
    wavFile.write(subChunk2ID,4);
    wavFile.write((uint8_t*)&subChunk2Size,4);
}

}  // namespace singsang