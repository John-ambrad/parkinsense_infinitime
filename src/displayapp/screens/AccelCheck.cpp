#include "displayapp/screens/AccelCheck.h"

#include <lvgl/lvgl.h>
#include "displayapp/DisplayApp.h"

using namespace Pinetime::Applications::Screens;

namespace {
  uint32_t Square(int16_t value) {
    int32_t wide = static_cast<int32_t>(value) * value;
    return static_cast<uint32_t>(wide);
  }

  uint16_t VectorLength(int16_t x, int16_t y, int16_t z) {
    uint32_t sum = Square(x) + Square(y) + Square(z);
    uint32_t root = 0;
    uint32_t bit = 1u << 16;
    while (bit != 0) {
      uint32_t trial = root + bit;
      if (static_cast<uint64_t>(trial) * trial <= sum) {
        root = trial;
      }
      bit >>= 1;
    }
    return static_cast<uint16_t>(root);
  }
}

AccelCheck::AccelCheck(Controllers::MotionController& motionController) : motionController {motionController} {
  motionController.SetRecordingRequested(true);

  lv_obj_t* title = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(title, "Accel check");
  lv_obj_align(title, nullptr, LV_ALIGN_IN_TOP_MID, 0, 8);

  lv_obj_t* scale = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(scale, "1024 counts is 1 g");
  lv_obj_align(scale, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

  liveLabel = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(liveLabel, "Live\nX 0  Y 0  Z 0\nlen 0");
  lv_label_set_align(liveLabel, LV_LABEL_ALIGN_CENTER);
  lv_obj_align(liveLabel, scale, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);

  fifoLabel = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(fifoLabel, "FIFO\nwaiting");
  lv_label_set_align(fifoLabel, LV_LABEL_ALIGN_CENTER);
  lv_obj_align(fifoLabel, liveLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);

  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
}

AccelCheck::~AccelCheck() {
  motionController.SetRecordingRequested(false);
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
}

void AccelCheck::Refresh() {
  int16_t liveX = motionController.X();
  int16_t liveY = motionController.Y();
  int16_t liveZ = motionController.Z();
  lv_label_set_text_fmt(liveLabel,
                        "Live\nX %d  Y %d  Z %d\nlen %u",
                        liveX,
                        liveY,
                        liveZ,
                        VectorLength(liveX, liveY, liveZ));
  lv_obj_align(liveLabel, nullptr, LV_ALIGN_CENTER, 0, -28);

  uint8_t count = motionController.LastBurstCount();
  if (count == 0) {
    lv_label_set_text_static(fifoLabel, "FIFO\nwaiting");
  } else {
    int16_t fifoX = motionController.LastBurstX();
    int16_t fifoY = motionController.LastBurstY();
    int16_t fifoZ = motionController.LastBurstZ();
    lv_label_set_text_fmt(fifoLabel,
                          "FIFO n=%u\nX %d  Y %d  Z %d\nlen %u",
                          count,
                          fifoX,
                          fifoY,
                          fifoZ,
                          VectorLength(fifoX, fifoY, fifoZ));
  }
  lv_obj_align(fifoLabel, liveLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);
}
