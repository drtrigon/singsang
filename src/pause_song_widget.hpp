#ifndef SINGSANG_PAUSE_SONG_WIDGET_HPP
#define SINGSANG_PAUSE_SONG_WIDGET_HPP

#include "base_widget.hpp"

namespace singsang
{
class CPauseSongWidget : public CBaseWidget
{
public:
    CPauseSongWidget() : CBaseWidget(270, 120, 40, 40) {}

    void update(const int f_newIsRunning)
    {
        if (f_newIsRunning != m_isRunning)
        {
            m_isRunning = f_newIsRunning;
            draw(true);
        }
    }

    void draw(const bool f_updateOnly)
    {
        if (m_isRunning)
        {
            M5.Lcd.drawPngFile(SPIFFS, "/player-pause.png", m_positionX,
                               m_positionY, m_sizeX, m_sizeY);
        }
		else
        {
            M5.Lcd.drawPngFile(SPIFFS, "/player-play.png", m_positionX,
                               m_positionY, m_sizeX, m_sizeY);
        }
    }

private:
    int m_isRunning;
};

}  // namespace singsang

#endif  // SINGSANG_PAUSE_SONG_WIDGET_HPP