#ifndef SINGSANG_COLORWHEEL_WIDGET_HPP
#define SINGSANG_COLORWHEEL_WIDGET_HPP

#include "base_widget.hpp"

#include <FastLED.h>

#define x_center    (320/2 - 1)
#define y_center    (240/2 - 1)
#define radius      90.0  // radius of color wheel

namespace singsang
{
class CColorWheelWidget : public CBaseWidget
{
public:
    CColorWheelWidget() : CBaseWidget(x_center - (210/2), y_center - (210/2), 210, 210) {}

    /*void update(void)
    {
        draw(true);
    }*/

    CHSV getPosition(TouchPoint_t f_point)
    {
/*        if (!isTouched(f_point))
        {
            return ...;
        }*/

        float x, y;

        // clear marker and draw color wheel
        draw(false);

        // calculate position marker/slider
        int16_t dx = f_point.x - x_center;                // cartesian vector from center of wheel
        int16_t dy = f_point.y - y_center;                //
        float a = atan2(dy, dx);                          // vector from cartesian to polar
	    if (a < 0) a +=  2 * PI;                          // (map [-PI, PI[ to [0, 2*PI[)
        //float r = min(sqrt(dx*dx + dy*dy), radius*1.);  //
        const float r = radius;                           //

        // draw marker/slider
        x = cos(a);                                       // position on wheel from polar angle
        y = sin(a);                                       // (parametrize wheel by angle)

        M5.Lcd.fillCircle(r * x + x_center, r * y + y_center, 10, M5.Lcd.color565(100, 100, 100));
        //M5.Lcd.drawLine(x_center, y_center, radius * x + x_center, radius * y + y_center, M5.Lcd.color565(100, 100, 100));

        return CHSV(static_cast<uint8_t>(255 * a / (2 * PI)), static_cast<uint8_t>(r * 255 / radius), 255);
    }

    void draw(const bool updateOnly)
    {
        if (updateOnly) {
            return;  // do nothing until next touch event
        }

        float x, y;

        // clear marker/slider
        for(uint8_t i = 6; i <= 15; ++i) {
// !!! TODO: partial clearing does not work properly
            M5.Lcd.drawCircle(x_center, y_center, radius - i, TFT_BLACK);
            M5.Lcd.drawCircle(x_center, y_center, radius + i, TFT_BLACK);
        }
        //M5.Lcd.fillCircle(x_center, y_center, radius + 15, TFT_BLACK);

        // draw color wheel
        CRGB rgb;
        for(uint16_t a = 0; a < 360; ++a) {
            hsv2rgb_rainbow(CHSV(static_cast<uint8_t>(255 * float(a) / 360), 255, 255), rgb);  //convert HSV to RGB

            x = cos(a * PI / 180);                        // position on wheel from polar angle
            y = sin(a * PI / 180);                        // (parametrize wheel by angle)

            M5.Lcd.fillCircle(radius * x + x_center, radius * y + y_center, 5, M5.Lcd.color565(rgb.r, rgb.g, rgb.b));
        }
    }
};

}  // namespace singsang

#endif  // SINGSANG_COLORWHEEL_WIDGET_HPP