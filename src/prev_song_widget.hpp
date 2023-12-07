#ifndef SINGSANG_PREV_SONG_WIDGET_HPP
#define SINGSANG_PREV_SONG_WIDGET_HPP

#include "base_widget.hpp"

namespace singsang
{
class CPrevSongWidget : public CBaseWidget
{
public:
    CPrevSongWidget() : CBaseWidget(10, 200, 40, 40) {}

    void update() {}

    void draw(const bool f_updateOnly)
    {
        M5.Lcd.drawPngFile(SPIFFS, "/player-track-prev.png", m_positionX,
                           m_positionY, m_sizeX, m_sizeY);
    }
};

}  // namespace singsang

#endif  // SINGSANG_PREV_SONG_WIDGET_HPP