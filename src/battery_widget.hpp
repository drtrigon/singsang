#ifndef SINGSANG_BATTERY_WIDGET_HPP
#define SINGSANG_BATTERY_WIDGET_HPP

#include "base_widget.hpp"

namespace singsang
{
class CBatteryWidget : public CBaseWidget
{
public:
    CBatteryWidget() : CBaseWidget(270, 0, 40, 40) {}

    void update()
    {
        String newIconPath{m_currentIconPath};

        const bool isCharging = (M5.Axp.GetVinVoltage() > 3.F);
        if (isCharging)
        {
            newIconPath = "/icon-battery-charging.png";
        }
        else
        {
            const float batVoltage    = M5.Axp.GetBatVoltage();
            const float batPercentage = (batVoltage < 3.2) ? 0 : (batVoltage - 3.2) * 100;

            if (batPercentage > 80)
            {
                newIconPath = "/icon-battery-4.png";
            }
            else if (batPercentage > 60)
            {
                newIconPath = "/icon-battery-3.png";
            }
            else if (batPercentage > 40)
            {
                newIconPath = "/icon-battery-2.png";
            }
            else if (batPercentage > 20)
            {
                newIconPath = "/icon-battery-1.png";
            }
            else
            {
                newIconPath = "/icon-battery.png";
            }
        }

        if (!m_currentIconPath.equals(newIconPath))
        {
            m_currentIconPath = newIconPath;
            draw(true);
        }
    }

    void draw(const bool updateOnly)
    {
        M5.Lcd.drawPngFile(SPIFFS, m_currentIconPath.c_str(), m_positionX,
                           m_positionY, m_sizeX, m_sizeY);
    }

private:
    String m_currentIconPath{"/icon-battery.png"};
};

}  // namespace singsang

#endif  // SINGSANG_BATTERY_WIDGET_HPP