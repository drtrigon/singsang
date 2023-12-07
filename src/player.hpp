#ifndef SINGSANG_PLAYER_HPP
#define SINGSANG_PLAYER_HPP

#include <Audio.h>
#include <M5Core2.h>
#include <SD.h>
#include <SPIFFS.h>
#include <WiFi.h>

#include <ArduinoJson.h>

#include <FastLED.h>

#include <memory>
#include <vector>

#include "battery_widget.hpp"
#include "file_selection_widget.hpp"
#include "next_song_widget.hpp"
#include "prev_song_widget.hpp"
#include "pause_song_widget.hpp"
#include "progress_widget.hpp"
#include "volume_widget.hpp"
#include "sleep_timer_widget.hpp"

#define RCA_MODULE
#define M5GO_BOTTOM

namespace singsang
{
class CPlayer
{
public:
    CPlayer();

    ~CPlayer() = default;

    void begin();

    void loop();

    void startNextSong();

    void startPrevSong();

    void pauseSong();

    void setPosSong(float f_relPos);

    void updateVolume(int f_deltaVolume);

    void increaseVolume();

    void decreaseVolume();

    void toggleSleepTimer();

    void updateSpeaker();

private:
    void handleInactivityTimeout();
    void handleTouchEvents();
    void initializeGui();
    void initializeHardware();
	void appendSDDirectory(File dir);
    void populateMusicFileList();
    void updateGui();
    void vibrate();
    void loadConfiguration(const char *filename);
    void saveConfiguration(const char *filename);
	void recordTEST();

    Audio m_audio{};

    CRGB leds[10];

    int                 m_currentVolume{8};
    int                 m_activeSongIdx{0};
    unsigned int        m_turnOffAfterInactiveForMilliSec{5 * 60 * 1000};
    unsigned int        m_lastActivityTimestamp{0};
    std::vector<String> m_songFiles{};
    bool                m_isRunning{true};
    unsigned int        m_sleepMode{0};
    unsigned int        m_outputMode{0};

    CBatteryWidget       m_batteryWidget;
    CFileSelectionWidget m_fileSelectionWidget;
    CNextSongWidget      m_nextSongWidget;
    CPrevSongWidget      m_prevSongWidget;
    CPauseSongWidget     m_pauseSongWidget;
    CProgressWidget      m_progressWidget;
    CVolumeWidget        m_volumeWidget;
    CSleepTimerWidget    m_sleepTimerWidget;
};

}  // namespace singsang

#endif  // SINGSANG_PLAYER_HPP