#pragma once
#include <drivers/Bma421_C/bma4_defs.h>

namespace Pinetime {
  namespace Drivers {
    class TwiMaster;

    class Bma421 {
    public:
      enum class DeviceTypes : uint8_t { Unknown, BMA421, BMA425 };

      struct Values {
        uint32_t steps;
        int16_t x;
        int16_t y;
        int16_t z;
      };

      // One acceleration sample in watch axes. 1024 counts is 1 g at the ±2 g range.
      struct Sample {
        int16_t x;
        int16_t y;
        int16_t z;
      };

      Bma421(TwiMaster& twiMaster, uint8_t twiAddress);
      Bma421(const Bma421&) = delete;
      Bma421& operator=(const Bma421&) = delete;
      Bma421(Bma421&&) = delete;
      Bma421& operator=(Bma421&&) = delete;

      /// The chip freezes the TWI bus after the softreset operation. Softreset is separated from the
      /// Init() method to allow the caller to uninit and then reinit the TWI device after the softreset.
      void SoftReset();
      void Init();
      Values Process();

      // Copies waiting FIFO frames into samples once the watermark is reached.
      // Returns 0 before that, or when samples is null or capacity is 0.
      // A return equal to capacity means more frames may still be waiting.
      size_t DrainFifo(Sample* samples, size_t capacity);

      void ResetStepCounter();

      void Read(uint8_t registerAddress, uint8_t* buffer, size_t size);
      void Write(uint8_t registerAddress, const uint8_t* data, size_t size);

      bool IsOk() const;
      DeviceTypes DeviceType() const;

    private:
      void Reset();
      Sample ToWatchAxes(int16_t rawX, int16_t rawY, int16_t rawZ) const;

      TwiMaster& twiMaster;
      uint8_t deviceAddress = 0x18;
      struct bma4_dev bma;
      struct bma4_accel_config accel_conf; // Store the device configuration for later reference.
      bool isOk = false;
      bool isResetOk = false;
      DeviceTypes deviceType = DeviceTypes::Unknown;
    };
  }
}