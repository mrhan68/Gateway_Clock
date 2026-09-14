#ifndef SERIAL_CLI_H
#define SERIAL_CLI_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo và bắt đầu task Serial Console CLI trên UART0 (115200 baud)
 */
void serial_cli_init(void);

#ifdef __cplusplus
}
#endif

#endif // SERIAL_CLI_H
