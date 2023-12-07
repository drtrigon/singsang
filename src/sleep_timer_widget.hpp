#ifndef SINGSANG_SLEEP_TIMER_WIDGET_HPP
#define SINGSANG_SLEEP_TIMER_WIDGET_HPP

#include "base_widget.hpp"

namespace singsang
{
class CSleepTimerWidget : public CBaseWidget
{
public:
    CSleepTimerWidget() : CBaseWidget(10, 120, 40, 40) {}

    void update(const int f_newSleepMode)
    {
        if (f_newSleepMode != m_sleepMode)
        {
            m_sleepMode = f_newSleepMode;
            draw(true);
        }
    }

    void draw(const bool f_updateOnly)
    {
        if (m_sleepMode)
        {
            M5.Lcd.drawPngFile(SPIFFS, "/bed-off.png", m_positionX,
                               m_positionY, m_sizeX, m_sizeY);
        }
        else
        {
            M5.Lcd.drawPngFile(SPIFFS, "/bed.png", m_positionX,
                               m_positionY, m_sizeX, m_sizeY);
        }
    }

private:
    int m_sleepMode;
};

}  // namespace singsang

#endif  // SINGSANG_SLEEP_TIMER_WIDGET_HPP