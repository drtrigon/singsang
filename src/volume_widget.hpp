#ifndef SINGSANG_VOLUME_WIDGET_HPP
#define SINGSANG_VOLUME_WIDGET_HPP

#include "base_widget.hpp"

#define sizeYMax      4  // height of bar, multiple of 2
#define volumeIdxMax 10  // number of bars, <= 20 = (height=80)/(sizeYMax=4)

namespace singsang
{
class CVolumeWidget : public CBaseWidget
{
public:
    CVolumeWidget() : CBaseWidget(10, 0, 40, 80) {}

    void update(const int f_newAudioVolume)
    {
        if (f_newAudioVolume != m_audioVolume)
        {
            m_audioVolume = f_newAudioVolume;
            draw(true);
        }
    }

    void setMode(unsigned int f_newOutputMode)
    {
        if (f_newOutputMode != m_outputMode)
        {
            m_outputMode = f_newOutputMode;
            draw(false);
        }
    }

    int getButton(TouchPoint_t f_point)
    {
/*        if (!isTouched(f_point))
        {
            return ...;
        }*/
        return int(float(f_point.y - m_positionY) / m_sizeY * 2);
    }

    void draw(const bool updateOnly)
    {
        //const uint16_t color = M5.Lcd.color565(100, 100, 100);
        uint16_t color = M5.Lcd.color565(100, 100, 100);
        if (m_outputMode == 1) {
            color = M5.Lcd.color565(100, 100, 0);
        }

        for (int volumeIdx = 0; volumeIdx < volumeIdxMax; volumeIdx++)
        {
            const int pointX = m_positionX;
            const int pointY = m_positionY + m_sizeY - (m_sizeY/volumeIdxMax) * (volumeIdx + 1);
            const int sizeX  = (m_sizeX/volumeIdxMax) * (volumeIdx + 1);// + 12;
            const int sizeY  = sizeYMax;

            if (!updateOnly)
            {
                M5.Lcd.fillRoundRect(pointX, pointY, sizeX, sizeY, sizeYMax/2, color);
            }

            const int barIsActive = (m_audioVolume > 2 * volumeIdx);
            const int pickColor   = barIsActive ? color : TFT_BLACK;
            M5.Lcd.fillRoundRect(pointX+1, pointY+1, sizeX-2, sizeY-2, (sizeYMax/2)-1, pickColor);
        }
    }

private:
    int m_audioVolume;
	unsigned int m_outputMode;
};

}  // namespace singsang

#endif  // SINGSANG_VOLUME_WIDGET_HPP