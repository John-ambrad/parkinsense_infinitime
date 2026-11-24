#pragma once

#include <atomic>
#include <cstdint>

#include <FreeRTOS.h>

#include "drivers/Bma421.h"
#include "components/ble/MotionService.h"
#include "utility/CircularBuffer.h"

namespace Pinetime {
  namespace Controllers {
    class MotionController {
    public:
      enum class DeviceTypes {
        Unknown,
        BMA421,
        BMA425,
      };

      enum class Days : uint8_t {
        Today = 0,
        Yesterday,
      };

      static constexpr size_t stepHistorySize = 2; // Store this many day's step counter

      void AdvanceDay();

      void Update(int16_t x, int16_t y, int16_t z, uint32_t nbSteps);

      bool IsRecordingSubscribed() const {
        return service != nullptr && service->IsRecordingSubscribed();
      }

      // The accel screen sets this so the FIFO is drained with no phone connected.
      void SetRecordingRequested(bool requested) {
        recordingRequested = requested;
      }

      bool RecordingRequested() const {
        return recordingRequested;
      }

      uint8_t LastBurstCount() const {
        return lastBurstCount;
      }

      uint32_t LastBurstFirstMs() const {
        return lastBurstFirstMs;
      }

      int16_t LastBurstX() const {
        return lastBurstX;
      }

      int16_t LastBurstY() const {
        return lastBurstY;
      }

      int16_t LastBurstZ() const {
        return lastBurstZ;
      }

      // Keeps the newest sample for the accel screen, then forwards the batch.
      // Does not touch gesture history or the step count.
      void OnRecordedSamples(const Pinetime::Drivers::Bma421::Sample* samples, size_t count, uint32_t firstSampleMs) {
        if (samples != nullptr && count > 0) {
          const auto& newest = samples[count - 1];
          lastBurstCount = static_cast<uint8_t>(count > 255 ? 255 : count);
          lastBurstFirstMs = firstSampleMs;
          lastBurstX = newest.x;
          lastBurstY = newest.y;
          lastBurstZ = newest.z;
        }
        if (service != nullptr) {
          service->OnBurst(samples, count, firstSampleMs);
        }
      }

      int16_t X() const {
        return xHistory[0];
      }

      int16_t Y() const {
        return yHistory[0];
      }

      int16_t Z() const {
        return zHistory[0];
      }

      uint32_t NbSteps(Days day = Days::Today) const {
        return nbSteps[static_cast<std::underlying_type_t<Days>>(day)];
      }

      void ResetTrip() {
        currentTripSteps = 0;
      }

      uint32_t GetTripSteps() const {
        return currentTripSteps;
      }

      bool ShouldRaiseWake() const;
      bool ShouldLowerSleep() const;

      int32_t CurrentShakeSpeed() const {
        return accumulatedSpeed;
      }

      DeviceTypes DeviceType() const {
        return deviceType;
      }

      void Init(Pinetime::Drivers::Bma421::DeviceTypes types);

      void SetService(Pinetime::Controllers::MotionService* service) {
        this->service = service;
      }

      Pinetime::Controllers::MotionService* GetService() const {
        return service;
      }

    private:
      Utility::CircularBuffer<uint32_t, stepHistorySize> nbSteps = {0};
      uint32_t currentTripSteps = 0;

      void SetSteps(Days day, uint32_t steps) {
        nbSteps[static_cast<std::underlying_type_t<Days>>(day)] = steps;
      }

      TickType_t lastTime = 0;
      TickType_t time = 0;

      struct AccelStats {
        static constexpr uint8_t numHistory = 2;

        int16_t xMean = 0;
        int16_t yMean = 0;
        int16_t zMean = 0;
        int16_t prevXMean = 0;
        int16_t prevYMean = 0;
        int16_t prevZMean = 0;

        uint32_t xVariance = 0;
        uint32_t yVariance = 0;
        uint32_t zVariance = 0;
      };

      AccelStats GetAccelStats() const;

      AccelStats stats = {};

      static constexpr uint8_t histSize = 8;
      Utility::CircularBuffer<int16_t, histSize> xHistory = {};
      Utility::CircularBuffer<int16_t, histSize> yHistory = {};
      Utility::CircularBuffer<int16_t, histSize> zHistory = {};
      int32_t accumulatedSpeed = 0;

      DeviceTypes deviceType = DeviceTypes::Unknown;
      Pinetime::Controllers::MotionService* service = nullptr;

      std::atomic<bool> recordingRequested {false};
      std::atomic<uint8_t> lastBurstCount {0};
      std::atomic<uint32_t> lastBurstFirstMs {0};
      std::atomic<int16_t> lastBurstX {0};
      std::atomic<int16_t> lastBurstY {0};
      std::atomic<int16_t> lastBurstZ {0};
    };
  }
}
