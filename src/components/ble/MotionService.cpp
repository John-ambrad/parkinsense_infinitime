#include "components/ble/MotionService.h"
#include "components/motion/MotionController.h"
#include "components/ble/NimbleController.h"
#include <host/ble_att.h>
#include <nrf_log.h>
#include <algorithm>

using namespace Pinetime::Controllers;

namespace {
  // 0003yyxx-78fc-48fe-8e23-433b3a1942d0
  constexpr ble_uuid128_t CharUuid(uint8_t x, uint8_t y) {
    return ble_uuid128_t {.u = {.type = BLE_UUID_TYPE_128},
                          .value = {0xd0, 0x42, 0x19, 0x3a, 0x3b, 0x43, 0x23, 0x8e, 0xfe, 0x48, 0xfc, 0x78, x, y, 0x03, 0x00}};
  }

  // 00030000-78fc-48fe-8e23-433b3a1942d0
  constexpr ble_uuid128_t BaseUuid() {
    return CharUuid(0x00, 0x00);
  }

  constexpr ble_uuid128_t motionServiceUuid {BaseUuid()};
  constexpr ble_uuid128_t stepCountCharUuid {CharUuid(0x01, 0x00)};
  constexpr ble_uuid128_t motionValuesCharUuid {CharUuid(0x02, 0x00)};
  constexpr ble_uuid128_t burstCharUuid {CharUuid(0x03, 0x00)};

  int MotionServiceCallback(uint16_t /*conn_handle*/, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg) {
    auto* motionService = static_cast<MotionService*>(arg);
    return motionService->OnStepCountRequested(attr_handle, ctxt);
  }
}

// TODO Refactoring - remove dependency to SystemTask
MotionService::MotionService(NimbleController& nimble, Controllers::MotionController& motionController)
  : nimble {nimble},
    motionController {motionController},
    characteristicDefinition {{.uuid = &stepCountCharUuid.u,
                               .access_cb = MotionServiceCallback,
                               .arg = this,
                               .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                               .val_handle = &stepCountHandle},
                              {.uuid = &motionValuesCharUuid.u,
                               .access_cb = MotionServiceCallback,
                               .arg = this,
                               .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                               .val_handle = &motionValuesHandle},
                              {.uuid = &burstCharUuid.u,
                               .access_cb = MotionServiceCallback,
                               .arg = this,
                               .flags = BLE_GATT_CHR_F_NOTIFY,
                               .val_handle = &burstHandle},
                              {0}},
    serviceDefinition {
      {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &motionServiceUuid.u, .characteristics = characteristicDefinition},
      {0},
    } {
  // TODO refactor to prevent this loop dependency (service depends on controller and controller depends on service)
  motionController.SetService(this);
}

void MotionService::Init() {
  int res = 0;
  res = ble_gatts_count_cfg(serviceDefinition);
  ASSERT(res == 0);

  res = ble_gatts_add_svcs(serviceDefinition);
  ASSERT(res == 0);
}

int MotionService::OnStepCountRequested(uint16_t attributeHandle, ble_gatt_access_ctxt* context) {
  if (attributeHandle == stepCountHandle) {
    NRF_LOG_INFO("Motion-stepcount : handle = %d", stepCountHandle);
    uint32_t buffer = motionController.NbSteps();

    int res = os_mbuf_append(context->om, &buffer, sizeof(buffer));
    return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  if (attributeHandle == motionValuesHandle) {
    int16_t buffer[3] = {motionController.X(), motionController.Y(), motionController.Z()};

    int res = os_mbuf_append(context->om, buffer, 3 * sizeof(int16_t));
    return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  return 0;
}

void MotionService::OnNewStepCountValue(uint32_t stepCount) {
  if (!stepCountNotificationEnabled) {
    return;
  }

  uint32_t buffer = stepCount;
  auto* om = ble_hs_mbuf_from_flat(&buffer, sizeof(buffer));

  uint16_t connectionHandle = nimble.connHandle();

  if (connectionHandle == 0 || connectionHandle == BLE_HS_CONN_HANDLE_NONE) {
    return;
  }

  ble_gattc_notify_custom(connectionHandle, stepCountHandle, om);
}

void MotionService::OnNewMotionValues(int16_t x, int16_t y, int16_t z) {
  if (!motionValuesNotificationEnabled) {
    return;
  }

  int16_t buffer[3] = {x, y, z};
  auto* om = ble_hs_mbuf_from_flat(buffer, 3 * sizeof(int16_t));

  uint16_t connectionHandle = nimble.connHandle();

  if (connectionHandle == 0 || connectionHandle == BLE_HS_CONN_HANDLE_NONE) {
    return;
  }

  ble_gattc_notify_custom(connectionHandle, motionValuesHandle, om);
}

void MotionService::OnBurst(const Pinetime::Drivers::Bma421::Sample* samples, size_t count, uint32_t firstSampleMs) {
  if (!burstNotificationEnabled || samples == nullptr || count == 0 || count > burstSampleCapacity) {
    return;
  }

  uint16_t connectionHandle = nimble.connHandle();
  if (connectionHandle == 0 || connectionHandle == BLE_HS_CONN_HANDLE_NONE) {
    return;
  }

  // ATT notify uses 1 opcode byte and 2 handle bytes in front of the payload.
  const size_t attPayloadLimit = ble_att_mtu(connectionHandle) - 3;
  constexpr size_t headerLength = 1 + sizeof(firstSampleMs);
  if (attPayloadLimit <= headerLength) {
    return;
  }
  const size_t samplesPerNotify = std::min(burstSampleCapacity, (attPayloadLimit - headerLength) / 6);

  size_t start = 0;
  while (start < count) {
    const size_t chunkCount = std::min(samplesPerNotify, count - start);
    const uint32_t chunkFirstSampleMs = firstSampleMs + static_cast<uint32_t>(start) * burstSampleIntervalMs;
    SendBurstChunk(connectionHandle, samples + start, chunkCount, chunkFirstSampleMs);
    start += chunkCount;
  }
}

void MotionService::SendBurstChunk(uint16_t connectionHandle,
                                   const Pinetime::Drivers::Bma421::Sample* samples,
                                   size_t count,
                                   uint32_t firstSampleMs) {
  const uint16_t payloadLength = static_cast<uint16_t>(1 + sizeof(firstSampleMs) + count * 6);

  uint8_t buffer[1 + sizeof(firstSampleMs) + burstSampleCapacity * 6];
  buffer[0] = static_cast<uint8_t>(count);
  buffer[1] = static_cast<uint8_t>(firstSampleMs);
  buffer[2] = static_cast<uint8_t>(firstSampleMs >> 8);
  buffer[3] = static_cast<uint8_t>(firstSampleMs >> 16);
  buffer[4] = static_cast<uint8_t>(firstSampleMs >> 24);

  size_t offset = 5;
  for (size_t i = 0; i < count; i++) {
    uint16_t x = static_cast<uint16_t>(samples[i].x);
    uint16_t y = static_cast<uint16_t>(samples[i].y);
    uint16_t z = static_cast<uint16_t>(samples[i].z);
    buffer[offset++] = static_cast<uint8_t>(x);
    buffer[offset++] = static_cast<uint8_t>(x >> 8);
    buffer[offset++] = static_cast<uint8_t>(y);
    buffer[offset++] = static_cast<uint8_t>(y >> 8);
    buffer[offset++] = static_cast<uint8_t>(z);
    buffer[offset++] = static_cast<uint8_t>(z >> 8);
  }

  auto* om = ble_hs_mbuf_from_flat(buffer, payloadLength);
  if (om == nullptr) {
    return;
  }
  ble_gattc_notify_custom(connectionHandle, burstHandle, om);
}

void MotionService::SubscribeNotification(uint16_t attributeHandle) {
  if (attributeHandle == stepCountHandle) {
    stepCountNotificationEnabled = true;
  } else if (attributeHandle == motionValuesHandle) {
    motionValuesNotificationEnabled = true;
  } else if (attributeHandle == burstHandle) {
    burstNotificationEnabled = true;
  }
}

void MotionService::UnsubscribeNotification(uint16_t attributeHandle) {
  if (attributeHandle == stepCountHandle) {
    stepCountNotificationEnabled = false;
  } else if (attributeHandle == motionValuesHandle) {
    motionValuesNotificationEnabled = false;
  } else if (attributeHandle == burstHandle) {
    burstNotificationEnabled = false;
  }
}
