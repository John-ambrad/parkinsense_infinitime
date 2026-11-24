#pragma once

#include <cstdint>
#include <lvgl/lvgl.h>
#include "displayapp/screens/Screen.h"
#include "displayapp/apps/Apps.h"
#include "displayapp/Controllers.h"
#include "components/motion/MotionController.h"
#include "Symbols.h"

namespace Pinetime {
  namespace Applications {
    namespace Screens {

      class AccelCheck : public Screen {
      public:
        explicit AccelCheck(Controllers::MotionController& motionController);
        ~AccelCheck() override;

        void Refresh() override;

      private:
        Controllers::MotionController& motionController;
        lv_obj_t* liveLabel;
        lv_obj_t* fifoLabel;
        lv_task_t* taskRefresh;
      };
    }

    template <>
    struct AppTraits<Apps::AccelCheck> {
      static constexpr Apps app = Apps::AccelCheck;
      static constexpr const char* icon = Screens::Symbols::tachometer;

      static Screens::Screen* Create(AppControllers& controllers) {
        return new Screens::AccelCheck(controllers.motionController);
      };

      static bool IsAvailable(Pinetime::Controllers::FS& /*filesystem*/) {
        return true;
      };
    };
  }
}
