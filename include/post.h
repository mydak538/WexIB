#ifndef POST_H
#define POST_H

#include <stdint.h>

// Результаты POST
#define POST_SUCCESS 0
#define POST_CPU_FAIL 1
#define POST_MEMORY_FAIL 2
#define POST_VIDEO_FAIL 3
#define POST_KEYBOARD_FAIL 4
#define POST_DISK_FAIL 5
#define POST_CMOS_FAIL 6

// Порты оборудования
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDE_STATUS 0x1F7
#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71
#define SPEAKER_PORT 0x61
#define PIT_CHANNEL2 0x42
#define PIT_COMMAND 0x43

// Функции POST
uint8_t run_post(void);
void post_cpu_test(void);
void post_memory_test(void);
void post_video_test(void);
void post_keyboard_test(void);
void post_disk_test(void);
void post_cmos_test(void);
void show_post_error(uint8_t error_code);

// Вспомогательные функции
void beep(uint32_t frequency, uint32_t duration);
void post_delay(uint32_t count);
uint8_t inb(uint16_t port);
void outb(uint16_t port, uint8_t value);

#endif
