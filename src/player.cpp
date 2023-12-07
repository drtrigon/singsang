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

    if (m_isRunning) {
        if (!m_audio.isRunning()) {  // not playing e.g. due to not supported file format (hacky work-a-round)
		    startNextSong();
        }
    }
}

void CPlayer::initializeHardware()
{
    M5.begin(true, true, true, false, kMBusModeOutput, false);  // SpeakerEnable = false

    M5.Axp.SetLed(false);
    M5.Axp.SetLcdVoltage(1800);  // dimmed, nominal value is 2800
    M5.Axp.SetSpkEnable(true);

    if (!SPIFFS.begin()) {  // Start SPIFFS, return 1 on success.
        //M5.Lcd.setTextSize(2);  //Set the font size to 2.
        //M5.Lcd.println("SPIFFS Failed to Start.");
        Serial.println("SPIFFS Failed to Start.");
    }

    WiFi.mode(WIFI_OFF);
    delay(100);

    m_audio.setPinout(12, 0, 2);
    m_audio.setVolume(m_currentVolume);
#ifdef FORCE_MONO
    m_audio.forceMono(true);
	Serial.println("FORCE_MONO set");
#endif

    populateMusicFileList();

    vibrate();
}

void CPlayer::initializeGui()
{
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextFont(2);

    M5.Lcd.drawJpgFile(SPIFFS, "/logo.jpg", 60, 20, 200, 200);

    m_batteryWidget.draw(false);
    m_fileSelectionWidget.draw(false);
    m_nextSongWidget.draw(false);
    m_prevSongWidget.draw(false);
    m_pauseSongWidget.draw(false);
    m_progressWidget.draw(false);
    m_volumeWidget.draw(false);
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

    if (touchPoint.y >= 250)  // "capacitive" Buttons A, B, C below display
    {
        if ((0 <= touchPoint.x) && (touchPoint.x <= 100))         // BtnA
        {
            Serial.println("M5.BtnA");
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
        (doc["sd_usedBytes"] == SD.usedBytes()))
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
  //doc["..."] = m_audio.getAudioCurrentTime();  // (store current playing position)

  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  file.close();
}

}  // namespace singsang