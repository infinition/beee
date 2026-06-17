#include "imu_driver.h"

// Registres QMI8658
#define QMI_WHO_AM_I   0x00 // doit valoir 0x05
#define QMI_CTRL1      0x02
#define QMI_CTRL2      0x03 // config accelerometre (FS + ODR)
#define QMI_CTRL3      0x04 // config gyroscope (non utilise)
#define QMI_CTRL5      0x06 // filtres passe-bas
#define QMI_CTRL7      0x08 // activation des capteurs
#define QMI_AX_L       0x35 // debut des donnees accel (AX_L..AZ_H)

bool ImuDriver::writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

bool ImuDriver::readRegs(uint8_t reg, uint8_t *buf, uint8_t len) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false; // repeated start
    uint8_t n = Wire.requestFrom(_addr, len);
    if (n != len) return false;
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

bool ImuDriver::probe(uint8_t addr) {
    _addr = addr;
    uint8_t id = 0;
    if (!readRegs(QMI_WHO_AM_I, &id, 1)) return false;
    return id == 0x05;
}

bool ImuDriver::begin() {
    // Le bus Wire est deja initialise par le tactile (meme SDA/SCL). On sonde les
    // deux adresses possibles du QMI8658.
    if (!probe(0x6B) && !probe(0x6A)) {
        _present = false;
        Serial.println("IMU QMI8658 non detecte (rotation auto desactivee).");
        return false;
    }
    Serial.printf("IMU QMI8658 detecte a l'adresse 0x%02X\n", _addr);

    writeReg(QMI_CTRL7, 0x00);  // desactive tout le temps de la config
    writeReg(QMI_CTRL1, 0x40);  // ADDR auto-increment, interface serie standard
    writeReg(QMI_CTRL2, 0x04);  // accel: +/-2g, ODR ~250Hz
    writeReg(QMI_CTRL5, 0x00);  // pas de filtre LPF
    writeReg(QMI_CTRL7, 0x01);  // active uniquement l'accelerometre
    delay(10);

    _present = true;
    return true;
}

bool ImuDriver::readAccel(float &ax, float &ay, float &az) {
    if (!_present) return false;
    uint8_t b[6];
    if (!readRegs(QMI_AX_L, b, 6)) return false;

    int16_t rx = (int16_t)((b[1] << 8) | b[0]);
    int16_t ry = (int16_t)((b[3] << 8) | b[2]);
    int16_t rz = (int16_t)((b[5] << 8) | b[4]);

    const float scale = 16384.0f; // LSB/g pour +/-2g
    ax = rx / scale;
    ay = ry / scale;
    az = rz / scale;
    return true;
}
