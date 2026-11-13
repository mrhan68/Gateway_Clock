#ifndef SECRETS_EXAMPLE_H
#define SECRETS_EXAMPLE_H

/**
 * Copy this file to `firmware/main/secrets.h` (which is gitignored)
 * and fill in your real MQTTS broker credentials before building.
 */
#ifndef SECRET_MQTT_BROKER_URI
#define SECRET_MQTT_BROKER_URI  "mqtts://your-emqx-broker.emqxsl.com:8883"
#endif

#ifndef SECRET_MQTT_USERNAME
#define SECRET_MQTT_USERNAME    "your_mqtt_username"
#endif

#ifndef SECRET_MQTT_PASSWORD
#define SECRET_MQTT_PASSWORD    "your_mqtt_password"
#endif

#endif // SECRETS_EXAMPLE_H
