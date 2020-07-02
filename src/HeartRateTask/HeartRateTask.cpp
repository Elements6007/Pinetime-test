#include <nrfx_log.h>
#include "HeartRateTask.h"
#include "../Components/HeartRate/HeartRateController.h"
using namespace Pinetime;


HeartRateTask::HeartRateTask(Drivers::Hrs3300 &heartRateSensor) :
        heartRateSensor{heartRateSensor},
        heartRateController{*this} {
  messageQueue = xQueueCreate(10, 1);
}

void HeartRateTask::Start() {
  if (pdPASS != xTaskCreate(HeartRateTask::Process, "Heartrate", 256, this, 0, &taskHandle))
    APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
}

void HeartRateTask::Process(void *instance) {
  auto *app = static_cast<HeartRateTask *>(instance);
  app->Work();
}

void HeartRateTask::Work() {
  while (true) {
    Messages msg;
    uint32_t delay;
    if (state == States::Running) {
      if (measurementStarted) delay = 50;
      else delay = 100;
    } else
      delay = portMAX_DELAY;

    if (xQueueReceive(messageQueue, &msg, delay)) {
      switch (msg) {
        case Messages::GoToSleep:
          StopMeasurement();
          state = States::Idle;
          break;
        case Messages::WakeUp:
          state = States::Running;
          break;
        case Messages::StartMeasurement:
          StartMeasurement();
          break;
        case Messages::StopMeasurement:
          StopMeasurement();
          break;
      }
    }

    if (measurementStarted) {
      auto hr = heartRateSensor.Process();
      if (hr != 255) {
        if (hr == 254) heartRateController.Update(Controllers::HeartRateController::States::NoTouch, 0);
        else if (hr == 253) heartRateController.Update(Controllers::HeartRateController::States::NotEnoughData, 0);
        else heartRateController.Update(Controllers::HeartRateController::States::Running, hr);
      }
    }
  }
}

void HeartRateTask::PushMessage(HeartRateTask::Messages msg) {
  BaseType_t xHigherPriorityTaskWoken;
  xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(messageQueue, &msg, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) {
    /* Actual macro used here is port specific. */
    // TODO : should I do something here?
  }
}

void HeartRateTask::StartMeasurement() {
  heartRateSensor.Init();
  measurementStarted = true;
}

void HeartRateTask::StopMeasurement() {
  heartRateSensor.Disable();
  measurementStarted = false;
}

