#include "player.hpp"

namespace singsang
{
CPlayer::CPlayer() {}

void CPlayer::begin()
{
    initializeHardware();
    initializeGui();

    // initialize Player (should be put into own method)
    loadConfiguration("");
    --m_activeSongIdx;  // we start using startNextSong() in loop()
}

void CPlayer::loop()
{
    m_audio.loop();

    handleTouchEvents();

    updateGui();

    handleInactivityTimeout();

    // skip non-playable files
    if (m_isRunning && (!m_audio.isRunning())) {  // not playing e.g. due to not supported file format (hacky work-a-round)
// repeat could be done here by a simple --m_activeSongIdx
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
}

void CPlayer::initializeHardware()
{
    M5.begin(true, true, true, false, kMBusModeOutput, false);  // SpeakerEnable = false

    M5.Axp.SetLed(false);
    M5.Axp.SetLcdVoltage(1800);  // dimmed, nominal value is 2800
    //M5.Axp.SetSpkEnable(true);
    updateSpeaker();

    if (!SPIFFS.begin()) {  // Start SPIFFS, return 1 on success.
        //M5.Lcd.setTextSize(2);  //Set the font size to 2.
        //M5.Lcd.println("SPIFFS Failed to Start.");
        Serial.println("SPIFFS Failed to Start.");
    }

    WiFi.mode(WIFI_OFF);

#ifdef M5GO_BOTTOM
    FastLED.addLeds<NEOPIXEL, 25>(leds, 10);  // GRB ordering is assumed, DATA_PIN=25, NUM_LEDS=10
    //FastLED.setBrightness(64);
    FastLED.setBrightness(1);
/*    fill_solid(leds, 10, CRGB::Blue);
    FastLED.show();
    delay(400);
    fill_solid(leds, 10, CRGB::Red);
    FastLED.show();
    delay(400);
    fill_solid(leds, 10, CRGB::Green);
    FastLED.show();
    delay(300);*/
	fill_rainbow(leds, 10, 0, 20);
    FastLED.show();
/*    delay(5000);
    fill_gradient_RGB(leds, 0, CRGB::Blue, 9, CRGB::Green);
    FastLED.show();
    delay(5000);*/
#endif

    delay(100);

    // internal speaker (default)
    m_audio.setPinout(12, 0, 2);
#ifdef RCA_MODULE
    // RCA Module 13.2 (can be used at same time)
    m_audio.setPinout(19, 0, 2);  // I2S_BCLK, I2S_LRC, I2S_DOUT
#endif
    m_audio.setVolume(m_currentVolume);

    populateMusicFileList();

    vibrate();
}

void CPlayer::initializeGui()
{
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextFont(2);

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

void CPlayer::appendSDDirectory(File dir)
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
				appendSDDirectory(entry);
            }
			else
            {
                const bool entryIsFile = (entry.size() > 0);
                if (entryIsFile)
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
    File musicDir = SD.open("/");

    appendSDDirectory(musicDir);

    Serial.print("MusicFileList length: ");
    Serial.println(m_songFiles.size());
}

void CPlayer::handleInactivityTimeout()
{
    if (m_audio.isRunning())
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
            m_audio.stopSong();
            M5.Axp.DeepSleep(0U);  // power off
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
		return;
    }

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

    if (touchPoint.y >= 250)  // "capacitive" Buttons A, B, C below display
    {
		// actually you can set-up any number of buttons, e.g. 5 or more)
        if ((0 <= touchPoint.x) && (touchPoint.x <= 100))         // BtnA
        {
            Serial.println("M5.BtnA");
//            recordTEST();
        }
        else if ((105 <= touchPoint.x) && (touchPoint.x <= 205))  // BtnB
        {
            Serial.println("M5.BtnB");
        }
        else if ((210 <= touchPoint.x) && (touchPoint.x <= 310))  // BtnC
        {
            Serial.println("M5.BtnC");
        }
    }
}

void CPlayer::startNextSong()
{
    if (m_songFiles.size() == 0)
    {
        return;
    }

    m_activeSongIdx = (m_activeSongIdx + 1) % m_songFiles.size();

    if (m_audio.isRunning())
    {
        m_audio.stopSong();
    }

    m_audio.connecttoSD(m_songFiles[m_activeSongIdx].c_str());

    saveConfiguration("");
}

void CPlayer::startPrevSong()
{
    if (m_songFiles.size() == 0)
    {
        return;
    }

    m_activeSongIdx = (m_activeSongIdx - 1) % m_songFiles.size();

    if (m_audio.isRunning())
    {
        m_audio.stopSong();
    }

    m_audio.connecttoSD(m_songFiles[m_activeSongIdx].c_str());

    saveConfiguration("");
}

void CPlayer::pauseSong()
{
    m_audio.pauseResume();

    m_isRunning = m_audio.isRunning();
}

void CPlayer::setPosSong(float f_relPos)
{
    if (!m_audio.isRunning())
    {
		return;
	}

    m_audio.pauseResume();

    m_audio.setAudioPlayPosition(f_relPos * m_audio.getAudioFileDuration());

    m_audio.pauseResume();
}

void CPlayer::updateGui()
{
    m_batteryWidget.update();

    int audioProgressPercentage = 0;
    if (m_audio.isRunning() && m_audio.getAudioFileDuration() > 0)
    {
        audioProgressPercentage = 100. * m_audio.getAudioCurrentTime() /
                                  m_audio.getAudioFileDuration();
    }
    m_progressWidget.update(audioProgressPercentage);

    m_fileSelectionWidget.update(m_songFiles.size(), m_activeSongIdx);

    m_volumeWidget.update(m_currentVolume);

    m_sleepTimerWidget.update(m_sleepMode);

    m_pauseSongWidget.update(m_audio.isRunning());
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
    m_audio.setVolume(m_currentVolume);
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
        m_audio.forceMono(true);
    } else {
        // internal speaker disabled, stereo (for external mode)
        M5.Axp.SetSpkEnable(false);
        m_audio.forceMono(false);
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
    File file = SPIFFS.open("/status", FILE_READ);

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/v6/assistant to compute the capacity.
    StaticJsonDocument<512> doc;

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if (error)
        Serial.println(F("Failed to read file, using default configuration"));

    // Copy values from the JsonDocument to the Config
    if ((doc["sd_cardSize"] == SD.cardSize()) &&
        (doc["sd_totalBytes"] == SD.totalBytes()) &&
        (doc["sd_usedBytes"] == SD.usedBytes()) &&
        (doc["m_songFiles.size"] == m_songFiles.size()))
	{
        m_activeSongIdx = doc["m_activeSongIdx"] | m_activeSongIdx;
        // we do not restore volume as environmental conditions might have changed
		// instead we use always a safe low default to protect user
        //m_currentVolume = doc["m_currentVolume"] | m_currentVolume;
    }

    // Close the file (Curiously, File's destructor doesn't close the file)
    file.close();
}

void CPlayer::saveConfiguration(const char *filename)
{
    // Delete existing file, otherwise the configuration is appended to the file
    SPIFFS.remove("/status");

    // Open file for writing
    File file = SPIFFS.open("/status", FILE_WRITE);
    if (!file) {
        Serial.println(F("Failed to create file"));
        return;
    }

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/assistant to compute the capacity.
    StaticJsonDocument<256> doc;

    // Set the values in the document
    doc["sd_cardSize"] = SD.cardSize();
    doc["sd_totalBytes"] = SD.totalBytes();
    doc["sd_usedBytes"] = SD.usedBytes();
    doc["m_activeSongIdx"] = m_activeSongIdx;
    doc["m_currentVolume"] = m_currentVolume;
	doc["m_songFiles.size"] = m_songFiles.size();
    //doc["..."] = m_audio.getAudioCurrentTime();  // (store current playing position)

    // Serialize JSON to file
    if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to file"));
    }

    // Close the file
    file.close();
}

void CPlayer::recordTEST() {  // see https://github.com/m5stack/M5Core2/blob/master/examples/Basics/record/record.ino
    uint8_t microphonedata0[DATA_SIZE * 1];  // DATA_SIZE = 1024
    int data_offset = 0;

    SD.remove("/___aaa000.wav");
	File file = SD.open("/___aaa000.wav", FILE_WRITE);
    vibrate();
    M5.Spk.InitI2SSpeakOrMic(MODE_MIC);  // ISSUE: seem to mess-up ESP32-audioI2S settings! look at source to figure out how to enable mic w/o issues
    size_t byte_read;
    while (1) {
        i2s_read(Speak_I2S_NUMBER, (char *)(microphonedata0),
                 DATA_SIZE, &byte_read, (100 / portTICK_RATE_MS));
        file.write(microphonedata0, byte_read);
        data_offset += byte_read;
        if (M5.Touch.ispressed() != true) break;
//        if ((data_offset >= (1024 * 1024 * 100)) || (M5.Touch.ispressed() != true)) break;
    }
    file.close();
//    size_t bytes_written;
    M5.Spk.InitI2SSpeakOrMic(MODE_SPK);
//    i2s_write(Speak_I2S_NUMBER, microphonedata0, data_offset, &bytes_written,
//              portMAX_DELAY);
    m_audio.connecttoSD("/___aaa000.wav");
}

}  // namespace singsang