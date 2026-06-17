#ifndef IMU_DRIVER_H
#define IMU_DRIVER_H

#include <Arduino.h>
#include <Wire.h>

// Driver minimal pour l'IMU QMI8658 (6 axes) de la Waveshare ESP32-S3-Touch-LCD-1.47.
// On n'utilise que l'accelerometre, pour detecter si l'ecran est retourne (flip 180).
class ImuDriver {
public:
    bool begin();                                  // true si QMI8658 detecte + initialise
    bool readAccel(float &ax, float &ay, float &az); // valeurs en g
    bool present() const { return _present; }

private:
    bool _present = false;
    uint8_t _addr = 0x6B; // SA0=1 (Waveshare) ; 0x6A teste en repli

    bool writeReg(uint8_t reg, uint8_t val);
    bool readRegs(uint8_t reg, uint8_t *buf, uint8_t len);
    bool probe(uint8_t addr);
};

#endif // IMU_DRIVER_H
