#include "player.hpp"

namespace singsang
{
CPlayer::CPlayer() {}

void CPlayer::begin()
{
    initializeHardware();
    initializeGui();

    // initialize Player (should be put into own method)
	// hacky as int has size of 4 - should be using struct or json
	// also store additional info to identify the SD and check whether its the same as last time
	// also consider (re)storing state of volume and playback position within current file
    File status = SPIFFS.open("/status", FILE_READ);
	if (status) {
		m_activeSongIdx = int(status.read());  // hacky as int has size 4, works up to 255 only!

        m_audio.connecttoSD(m_songFiles[m_activeSongIdx].c_str());
	}
    status.close();
    Serial.printf("SD Card Size: %lluMB\n", SD.cardSize() / (1024 * 1024));   // FUTURE: use to generate unique SD ID
    Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));  // FUTURE: use to generate unique SD ID
    Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));    // FUTURE: use to generate unique SD ID
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

    File status = SPIFFS.open("/status", FILE_WRITE);
    status.write(char(m_activeSongIdx));  // hacky as int has size 4, works up to 255 only!
    status.close();
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

    File status = SPIFFS.open("/status", FILE_WRITE);
    status.write(char(m_activeSongIdx));  // hacky as int has size 4, works up to 255 only!
    status.close();
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

}  // namespace singsang