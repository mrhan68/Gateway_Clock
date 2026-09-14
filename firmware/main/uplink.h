#ifndef UPLINK_H
#define UPLINK_H

#include "esp_err.h"

#ifdef __cplusplus
extern C {
#endif

/**
 * @brief Khoi tao module Uplink Telemetry (FSD §4.6, Journey Test JRN-04)
 * Tao task dinh ky gui telemetry ve he thong va duy tri heartbeat toi Supervisor.
 */
void uplink_init(void);

#ifdef __cplusplus
}
#endif

#endif // UPLINK_H
