#ifndef SINGSANG_PAUSE_SONG_WIDGET_HPP
#define SINGSANG_PAUSE_SONG_WIDGET_HPP

#include "base_widget.hpp"

namespace singsang
{
class CPauseSongWidget : public CBaseWidget
{
public:
    CPauseSongWidget() : CBaseWidget(270, 120, 40, 40) {}

    void update() {}

    void draw(const bool f_updateOnly)
    {
        M5.Lcd.drawPngFile(SPIFFS, "/icon-caret-right.png", m_positionX,
                           m_positionY, m_sizeX, m_sizeY);
    }
};

}  // namespace singsang

#endif  // SINGSANG_PAUSE_SONG_WIDGET_HPP