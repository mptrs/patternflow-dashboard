// Patternflow Dashboard - optional accelerometer (LIS3DH) for automatic rotation.
//
// Wiring: 3V3, GND, SDA -> GPIO43 (TX pad), SCL -> GPIO44 (RX pad): the only
// free header pins on the Patternflow board (the Audio edition's microphone
// uses the same two, so this edition and a microphone don't go together).
//
// Nothing here may break a panel without the sensor:
//   - all I2C traffic runs in a low-priority task on core 0, never in a frame
//   - no sensor: auto-rotation simply stays off and K2 works as before; the
//     task looks for one every 10 seconds, so it can be plugged in later
//   - the chip is identified (WHO_AM_I = 0x33) before anything is trusted,
//     and readings that cannot be gravity are ignored
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <math.h>

namespace DashAccel {

constexpr int SDA_PIN = 43;
constexpr int SCL_PIN = 44;
constexpr uint8_t WHO_AM_I = 0x0F, CTRL_REG1 = 0x20, CTRL_REG4 = 0x23, OUT_X_L = 0x28;

// Settings (NVS namespace "dashboard")
inline bool autoRotate = true;     // follow the sensor
inline bool flipPatterns = true;   // turn Patternflow's own patterns 180 degrees when upside down
inline uint8_t uprightAxis = 0;    // axis (0 x, 1 y) that points down when the panel hangs upright (portrait)
inline int8_t uprightSign = 1;     // and its sign
inline int8_t landscapeSign = 1;   // sign on the other axis that means "landscape" (else landscape upside down)

// Written by the sensor task, read by the loop task
inline volatile bool present = false;
inline volatile uint8_t address = 0;
inline volatile float gx = 0, gy = 0, gz = 0;  // in g
inline volatile int stable = -1;               // orientation held for ~1 s, -1 = unknown / lying flat
inline volatile uint32_t stableSerial = 0;     // bumps each time `stable` changes

inline void load() {
  Preferences prefs;
  if (!prefs.begin("dashboard", true)) return;
  autoRotate = prefs.getBool("autoRot", true);
  flipPatterns = prefs.getBool("flipPat", true);
  uprightAxis = prefs.getUChar("upAxis", 0) & 1;
  uprightSign = prefs.getChar("upSign", 1) < 0 ? -1 : 1;
  landscapeSign = prefs.getChar("landSign", 1) < 0 ? -1 : 1;
  prefs.end();
}

inline void save() {
  Preferences prefs;
  if (!prefs.begin("dashboard", false)) return;
  prefs.putBool("autoRot", autoRotate);
  prefs.putBool("flipPat", flipPatterns);
  prefs.putUChar("upAxis", uprightAxis);
  prefs.putChar("upSign", uprightSign);
  prefs.putChar("landSign", landscapeSign);
  prefs.end();
}

// Orientation from gravity: 0 landscape, 1 portrait, 2/3 the same upside down,
// -1 if the panel lies flat or the reading is not plausible.
inline int orientationOf(float x, float y, float z) {
  const float g = sqrtf(x * x + y * y + z * z);
  if (g < 0.5f || g > 1.5f) return -1;  // not gravity: no data, or the panel is being moved
  if (fabsf(z) > fmaxf(fabsf(x), fabsf(y))) return -1;  // lying flat
  const float a = uprightAxis ? y : x, b = uprightAxis ? x : y;
  if (fabsf(a) >= fabsf(b)) return (a > 0) == (uprightSign > 0) ? 1 : 3;
  return (b > 0) == (landscapeSign > 0) ? 0 : 2;
}

// "This is upright": remember which way gravity points right now.
inline bool calibrateUpright() {
  if (!present) return false;
  const float x = gx, y = gy;
  if (fmaxf(fabsf(x), fabsf(y)) < 0.5f) return false;  // flat or moving
  uprightAxis = fabsf(y) > fabsf(x) ? 1 : 0;
  uprightSign = (uprightAxis ? y : x) > 0 ? 1 : -1;
  save();
  return true;
}

// ---------------------------------------------------------------- I2C
inline bool writeReg(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

inline bool readRegs(uint8_t addr, uint8_t reg, uint8_t* out, size_t n) {
  Wire.beginTransmission(addr);
  Wire.write(n > 1 ? reg | 0x80 : reg);  // 0x80: auto-increment
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, (int)n) != (int)n) return false;
  for (size_t i = 0; i < n; i++) out[i] = Wire.read();
  return true;
}

inline bool probe() {
  for (uint8_t addr : {(uint8_t)0x18, (uint8_t)0x19}) {
    uint8_t id = 0;
    if (!readRegs(addr, WHO_AM_I, &id, 1) || id != 0x33) continue;
    // 50 Hz, all axes, normal mode; block data update, +-2 g, high resolution
    if (!writeReg(addr, CTRL_REG1, 0x47) || !writeReg(addr, CTRL_REG4, 0x88)) continue;
    address = addr;
    return true;
  }
  return false;
}

inline bool readGravity() {
  uint8_t raw[6];
  if (!readRegs(address, OUT_X_L, raw, 6)) return false;
  // 12-bit left-justified, 1 mg per digit at +-2 g in high-resolution mode
  gx = (int16_t)(raw[0] | raw[1] << 8) / 16 / 1000.0f;
  gy = (int16_t)(raw[2] | raw[3] << 8) / 16 / 1000.0f;
  gz = (int16_t)(raw[4] | raw[5] << 8) / 16 / 1000.0f;
  return true;
}

inline void sensorTask(void*) {
  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  Wire.setTimeOut(20);
  int candidate = -1, same = 0, failures = 0;
  for (;;) {
    if (!present) {
      if (probe()) {
        present = true;
        failures = 0;
        Serial.printf("[DASH] accelerometer found at 0x%02X\n", address);
      } else {
        vTaskDelay(pdMS_TO_TICKS(10000));  // look again in a while: it may be plugged in later
        continue;
      }
    }
    if (!readGravity()) {
      if (++failures >= 5) {  // unplugged
        present = false;
        stable = -1;
        Serial.println("[DASH] accelerometer lost");
      }
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }
    failures = 0;
    const int o = orientationOf(gx, gy, gz);
    if (o == candidate) {
      same++;
    } else {
      candidate = o;
      same = 1;
    }
    if (o >= 0 && same == 5 && stable != o) {  // held for ~1 s
      stable = o;
      stableSerial = stableSerial + 1;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

inline void begin() {
  load();
  xTaskCreatePinnedToCore(sensorTask, "dash_accel", 4096, nullptr, 1, nullptr, 0);
}

}  // namespace DashAccel
