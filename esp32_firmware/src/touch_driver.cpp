#include "touch_driver.h"

TouchDriver::TouchDriver(int sda, int scl, int rst, int interrupt) 
    : _sda(sda), _scl(scl), _rst(rst), _int(interrupt) {
    _width = 172;
    _height = 320;
    _rotation = 0;
}

bool TouchDriver::begin(int width, int height, int rotation) {
    _width = width;
    _height = height;
    _rotation = rotation;

    // Reset touch controller
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, LOW);
    delay(150);
    digitalWrite(_rst, HIGH);
    delay(200);

    // Initialize I2C Wire
    Wire.begin(_sda, _scl, 400000); // 400kHz I2C speed

    // Test communication: read ID register or just ping the chip
    Wire.beginTransmission(AXS5106L_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.println("AXS5106L touch controller not found at I2C address 0x63!");
        return false;
    }
    Serial.println("AXS5106L touch controller detected successfully!");
    return true;
}

bool TouchDriver::readI2C(uint8_t reg, uint8_t *data, uint32_t len) {
    Wire.beginTransmission(AXS5106L_I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) {
        return false;
    }

    uint32_t readBytes = Wire.requestFrom((uint8_t)AXS5106L_I2C_ADDR, (uint8_t)len);
    if (readBytes != len) {
        return false;
    }

    Wire.readBytes(data, len);
    return true;
}

bool TouchDriver::readTouch(int &x, int &y) {
    uint8_t data[8] = {0};
    
    // Read 6 bytes of touch info from register 0x01
    // Format: [status, touch_num, x_high, x_low, y_high, y_low]
    if (!readI2C(AXS5106L_REG_DATA, data, 6)) {
        return false;
    }

    uint8_t touch_num = data[1];
    if (touch_num == 0 || touch_num > 5) {
        return false; // No touches detected
    }

    // Extract raw coordinates
    uint16_t rx = ((uint16_t)(data[2] & 0x0F)) << 8 | data[3];
    uint16_t ry = ((uint16_t)(data[4] & 0x0F)) << 8 | data[5];

    // Apply rotation mappings (matching Waveshare's specifications)
    switch (_rotation) {
        case 1: // Landscape
            y = rx;
            x = ry;
            break;
        case 2: // Inverted Portrait
            x = rx;
            y = _height - 1 - ry;
            break;
        case 3: // Inverted Landscape
            y = _height - 1 - rx;
            x = _width - 1 - ry;
            break;
        case 0: // Portrait (Default)
        default:
            // Dalle Waveshare 1.47 : l'axe X du tactile est inverse par rapport a
            // l'affichage natif (rx=0 a droite) -> on retourne le X. Y aligne 1:1.
            x = _width - 1 - rx;
            y = ry;
            break;
    }

    return true;
}
