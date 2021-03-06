/* 
   barometer.hpp: Altitude estimation using barometer

   Adapted from

    https://github.com/multiwii/baseflight/blob/master/src/sensors.c

   This file is part of Hackflight.

   Hackflight is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Hackflight is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with EM7180.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.hpp"

namespace hf {
 
class Barometer {

    public:

        void    init(const BarometerConfig & _config, Board * _board);
        void    calibrate(void);
        void    update(void);
        int32_t getAltitude(void);
        int32_t getVelocity(uint32_t dTimeMicros);

    private:

        BarometerConfig config;

        Board * board;

        int32_t  alt;
        int32_t  history[BarometerConfig::HISTORY_SIZE];
        int      historyIdx;
        int32_t  groundPressure;
        int32_t  groundAltitude;
        int32_t  lastAlt;
        uint32_t pressureSum;

        static float paToCm(uint32_t pa);
};

/********************************************* CPP ********************************************************/

void Barometer::init(const BarometerConfig & _config, Board * _board)
{
    memcpy(&config, &_config, sizeof(BarometerConfig));

    board = _board;

    pressureSum = 0;
    historyIdx = 0;
    groundPressure = 0;
    groundAltitude = 0;
    alt = 0;
    lastAlt = 0;

    for (int k=0; k<BarometerConfig::HISTORY_SIZE; ++k) {
        history[k] = 0;
    }
}

void Barometer::calibrate(void)
{
    groundPressure -= groundPressure / 8;
    groundPressure += pressureSum / (BarometerConfig::HISTORY_SIZE - 1);
    groundAltitude = paToCm(groundPressure/8);
}

void Barometer::update()
{
    float pressure = board->extrasGetBaroPressure();

    uint8_t indexplus1 = (historyIdx + 1) % BarometerConfig::HISTORY_SIZE;
    history[historyIdx] = (int32_t)pressure;
    pressureSum += history[historyIdx];
    pressureSum -= history[indexplus1];
    historyIdx = indexplus1;
}

int32_t Barometer::getAltitude(void)
{
    int32_t alt_tmp = paToCm((float)pressureSum/(BarometerConfig::HISTORY_SIZE-1)); 
    alt_tmp -= groundAltitude;
    alt = lrintf((float)alt * config.noiseLpf + (float)alt_tmp * (1.0f - config.noiseLpf));

    return alt;
}

int32_t Barometer::getVelocity(uint32_t dTimeMicros)
{
    static int32_t lastAlt;
    int32_t vel = (alt - lastAlt) * 1000000.0f / dTimeMicros;
    lastAlt = alt;
    vel = Filter::constrainAbs(vel, config.velocityBound); 
    return Filter::deadband(vel, config.velocityDeadband);
}


// Pressure in Pascals to altitude in centimeters
float Barometer::paToCm(uint32_t pa)
{
    return (1.0f - powf(pa / 101325.0f, 0.190295f)) * 4433000.0f;
}

} // namespace hf
