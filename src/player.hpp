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
#include "color_wheel_widget.hpp"

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
	void appendSDDirectory(File dir, const bool list_dir);
    void populateMusicFileList();
    void updateGui();
    void vibrate();
    void loadConfiguration(const char *filename);
    void saveConfiguration(const char *filename);
    void rec_record(const char *filepath);
    void rec_play(const char *filepath);
    void writeWavHeader(File wavFile, uint32_t NumSamples);

    Audio* m_audio = new Audio();

    CRGB leds[10];

    int                 m_currentVolume{8};
    int                 m_activeSongIdx[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};  // set all items to 0 - one item per playlist
    unsigned int        m_activeSongIdxIdx{11};     // defaults to "/" (all) playlist
    unsigned int        m_turnOffAfterInactiveForMilliSec{5 * 60 * 1000};
    unsigned int        m_lastActivityTimestamp{0};
    std::vector<String> m_songFiles{};
    std::vector<String> m_songDirs{};
    unsigned int        m_songFilesSize{0};
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

    unsigned int ui_PageNumber{0};  // 0=player (default), 1=playlists/hörbert, 2=record, 3=color wheel, 4=display off, (5=settings e.g. speaker off, auto off, etc., ?=file/directory browser or include into hörbert mode)

    const uint32_t m_colors[12] = {TFT_MAROON, TFT_PURPLE, TFT_LIGHTGREY, TFT_BLUE, TFT_GREEN, TFT_YELLOW, TFT_ORANGE, TFT_PINK, TFT_CYAN, TFT_RED, TFT_MAGENTA, TFT_WHITE};

    CColorWheelWidget c_ColorWheelWidget;
};

}  // namespace singsang

#endif  // SINGSANG_PLAYER_HPP