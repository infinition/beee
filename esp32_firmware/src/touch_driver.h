#ifndef TOUCH_DRIVER_H
#define TOUCH_DRIVER_H

#include <Arduino.h>
#include <Wire.h>

#define AXS5106L_I2C_ADDR 0x63
#define AXS5106L_REG_DATA 0x01

class TouchDriver {
public:
    TouchDriver(int sda, int scl, int rst, int interrupt);
    bool begin(int width, int height, int rotation);
    bool readTouch(int &x, int &y);
    void setRotation(int rotation) { _rotation = rotation; } // pour l'auto-rotation gyro

private:
    int _sda;
    int _scl;
    int _rst;
    int _int;
    int _width;
    int _height;
    int _rotation;
    
    bool readI2C(uint8_t reg, uint8_t *data, uint32_t len);
};

#endif // TOUCH_DRIVER_H
