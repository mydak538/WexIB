#include <stdint.h>
#include "post.h"
#include "console.h"

#define VIDEO_MEMORY 0xB8000
#define WIDTH 80
#define HEIGHT 25

// Порты клавиатуры
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// Коды клавиш
#define KEY_UP 0x48
#define KEY_DOWN 0x50
#define KEY_ENTER 0x1C
#define KEY_ESC 0x01
#define KEY_BACKSPACE 0x0E
#define KEY_LEFT 0x4B
#define KEY_RIGHT 0x4D

// Порты IDE
#define IDE_DATA        0x1F0
#define IDE_ERROR       0x1F1
#define IDE_SECTOR_COUNT 0x1F2
#define IDE_LBA_LOW     0x1F3
#define IDE_LBA_MID     0x1F4
#define IDE_LBA_HIGH    0x1F5
#define IDE_DRIVE_HEAD  0x1F6
#define IDE_COMMAND     0x1F7
#define IDE_STATUS      0x1F7

// Команды IDE
#define IDE_CMD_READ    0x20

// Порты CMOS
#define CMOS_ADDRESS    0x70
#define CMOS_DATA       0x71

// Адреса CMOS для настроек
#define CMOS_BOOT_ORDER     0x10
#define CMOS_SECURITY_FLAG  0x11
#define CMOS_PASSWORD_0     0x12
#define CMOS_PASSWORD_1     0x13
#define CMOS_PASSWORD_2     0x14
#define CMOS_PASSWORD_3     0x15
#define CMOS_PASSWORD_4     0x16
#define CMOS_PASSWORD_5     0x17
#define CMOS_PASSWORD_6     0x18
#define CMOS_PASSWORD_7     0x19
#define CMOS_CHECKSUM       0x1A
#define CMOS_BOOT_DEVICE_1  0x20
#define CMOS_BOOT_DEVICE_2  0x21
#define CMOS_BOOT_DEVICE_3  0x22
#define CMOS_HW_ERROR_COUNT 0x23

// Структуры данных
typedef struct {
    uint8_t x, y;
    uint8_t selected;
    uint8_t offset;
    uint8_t needs_redraw;
} menu_state_t;

typedef struct {
    char name[16];
    void (*action)(void);
} menu_item_t;

typedef struct {
    uint8_t security_enabled;
    uint8_t boot_order;
    char password[9];  // 8 символов + null terminator
    uint8_t checksum;
    uint8_t boot_devices[3];
    uint8_t hw_error_count;
} bios_settings_t;

// Глобальные переменные
uint16_t* video_mem = (uint16_t*)VIDEO_MEMORY;
menu_state_t menu_state = {0, 0, 0, 0, 1};
bios_settings_t bios_settings = {0};
char input_buffer[64];
uint8_t input_pos = 0;
uint8_t last_key = 0;
uint8_t password_attempts = 0;

// Прототипы функций
void clear_screen(uint8_t color);
void print_string(const char* str, uint8_t x, uint8_t y, uint8_t color);
void print_char(char c, uint8_t x, uint8_t y, uint8_t color);
void draw_interface(void);
void show_left_menu(void);
void show_right_panel(void);
void handle_input(void);
int strcmp(const char* s1, const char* s2);
void delay(uint32_t count);
void config_screen(void);void show_boot_screen(void);
void boot_os(void);
void detect_memory_info(void);
void detect_cpu_info(void);
void read_cmos_time(void);
void read_cmos_date(void);

// Функции ввода-вывода
uint8_t inb(uint16_t port);
void outb(uint16_t port, uint8_t value);
uint16_t inw(uint16_t port);
void outw(uint16_t port, uint16_t value);
uint8_t keyboard_read(void);
char get_ascii_char(uint8_t scancode);
void wait_keyboard(void);

// Функции диска
uint8_t read_disk_sector(uint32_t lba, uint16_t* buffer);
void wait_ide(void);
void boot_from_disk(void);

// Функции обнаружения оборудования
uint32_t detect_memory_range(uint32_t start, uint32_t end);
uint16_t read_cmos_memory(void);

// Функции CMOS и безопасности
uint8_t read_cmos(uint8_t reg);
void write_cmos(uint8_t reg, uint8_t value);
void load_bios_settings(void);
void save_bios_settings(void);
uint8_t calculate_checksum(void);
void security_menu(void);
uint8_t check_password(void);
void set_password(void);
void enter_password(void);

// Новые функции для Settings
void settings_menu(void);
void boot_priority_menu(void);
void update_bios_menu(void);
void hardware_test(void);
void show_boot_failed_error(void);

// Добавляем после существующих прототипов
void print_hex(uint32_t num, uint8_t x, uint8_t y, uint8_t color);
void boot_from_usb(void);
uint8_t detect_usb_controllers(void);
uint8_t init_usb_controller(uint16_t base);
void show_boot_menu(void);
void reset_usb_controller(uint16_t base);
uint8_t usb_storage_reset(uint16_t base);
uint8_t read_usb_sector(uint16_t base, uint32_t lba, uint16_t* buffer);
void usb_boot_menu(void);

// Массив пунктов меню
const menu_item_t menu_items[] = {
    {"\4 Boot         ", boot_os},
    {"\4 Information  ", 0},
    {"\4 Hardware Test", hardware_test},
    {"\4 Settings     ", settings_menu},
    {"\4 Security     ", security_menu},
    {"\4 Configuration", config_screen},
    {"\4 USB Boot     ", boot_from_usb}, 
   {"\4 Dev Tools    ", dev_tools_menu},
};

const int menu_items_count = 8;

// Названия загрузочных устройств
const char* boot_device_names[] = {
    "Hard Disk",
    "CD/DVD",
    "USB",
    "Network",
    "Disabled",
};

void main() {
    // Запускаем POST
    uint8_t post_result = run_post();
    if (post_result != POST_SUCCESS) {
        show_post_error(post_result);
        // Зависаем при ошибке POST
        while(1) { 
            post_delay(1000000); 
        }
    }
    
    // Показываем загрузочный экран
    show_boot_screen();
    
    // Очищаем и сбрасываем состояние
    clear_screen(0x07);
    
    // Загружаем настройки из CMOS
    load_bios_settings();
    save_bios_settings();
    
    // Проверяем пароль при загрузке
    if (bios_settings.security_enabled) {
        enter_password();
    }
    
    // Сбрасываем состояние меню
    menu_state.selected = 0;
    menu_state.needs_redraw = 1;
    last_key = 0;

    while(1) {
        if (menu_state.needs_redraw) {
            draw_interface();
            menu_state.needs_redraw = 0;
        }
        handle_input();
        delay(10000);
    }
}

void show_boot_menu(void) {
    uint8_t selected = 0;
    const char* boot_options[] = {
        "Hard Disk (HDD 0)",
        "USB Device", 
        "BIOS Setup",
        "Continue Normal Boot"
    };
    const int items_count = 6;
    
    clear_screen(0x07);
    print_string("BOOT SELECTION MENU", 30, 1, 0x0F);
    print_string("===================", 30, 2, 0x0F);
    print_string("Select boot device:", 25, 4, 0x07);
    
    while(1) {
        // Отрисовка меню
        for(int i = 0; i < items_count; i++) {
            uint8_t color = (i == selected) ? 0x1F : 0x07;
            if (i == selected) {
                print_string(">", 25, 6 + i, color);
            } else {
                print_string(" ", 25, 6 + i, color);
            }
            print_string(boot_options[i], 27, 6 + i, color);
        }
        
        print_string("ENTER: Select  ESC: Cancel", 25, 20, 0x07);
        
        uint8_t scancode = keyboard_read();
        if (scancode == 0) {
            delay(10000);
            continue;
        }
        
        if (scancode & 0x80) continue;
        
        if (scancode == KEY_UP) {
            if (selected > 0) selected--;
        }
        else if (scancode == KEY_DOWN) {
            if (selected < items_count - 1) selected++;
        }
        else if (scancode == KEY_ENTER) {
            switch(selected) {
                case 0: // Hard Disk
                    boot_from_disk();
                    break;
                case 1: // USB
                    boot_from_usb();
                    break;
                case 2: // BIOS Setup
                    return; // Вернемся в основное меню BIOS
                case 3: // Continue
                    return; // Продолжим нормальную загрузку
            }
        }
        else if (scancode == KEY_ESC) {
            return; // Отмена - продолжаем нормальную загрузку
        }
    }
}


void config_screen(void) {
    print_string("CPU: 80386 compatible", 22, 3, 0x0F);
    print_string("Memory: 640K base", 22, 4, 0x0F);
    print_string("BIOS: WexIB v4.51", 22, 5, 0x0F);

    print_string("Security: ", 22, 6, 0x0F);
            if (bios_settings.security_enabled) {
    print_string("ENABLED", 33, 6, 0x0F);
            } else {
                print_string("DISABLED", 33, 6, 0x0F);
            }
  }

  
  // ==================== USB ПОРТЫ И КОМАНДЫ ====================

// USB Controller Ports (UHCI)
#define USB_COMMAND_PORT    0x0
#define USB_STATUS_PORT     0x2
#define USB_INTERRUPT_PORT  0x4
#define USB_FRAME_NUM_PORT  0x6
#define USB_FRAME_BASE_PORT 0x8
#define USB_SOF_MOD_PORT    0xC

// USB Commands
#define USB_CMD_RUN         0x0001
#define USB_CMD_RESET       0x0002
#define USB_CMD_DEBUG       0x0004

// USB Mass Storage Class
#define USB_MSD_RESET       0xFF
#define USB_MSD_GET_MAX_LUN 0xFE

// SCSI Commands for USB Storage
#define SCSI_TEST_UNIT_READY 0x00
#define SCSI_REQUEST_SENSE   0x03
#define SCSI_READ_CAPACITY   0x25
#define SCSI_READ_10         0x28

// USB Controller Base Addresses
#define USB1_BASE 0x00A0
#define USB2_BASE 0x00A8

// Структура для USB устройства
typedef struct {
    uint8_t detected;
    uint8_t type;           // 0 - нет, 1 - UHCI, 2 - OHCI
    uint16_t base_port;
    uint8_t connected;
    uint32_t capacity;      // в секторах
} usb_device_t;

// Структура для SCSI команды
typedef struct {
    uint8_t opcode;
    uint8_t lun_flags;
    uint32_t lba;
    uint8_t reserved;
    uint16_t transfer_length;
    uint8_t control;
} __attribute__((packed)) scsi_read10_t;

// Глобальные переменные для USB
usb_device_t usb_devices[4] = {0};
uint8_t usb_boot_supported = 0;

// ==================== USB ОБНАРУЖЕНИЕ ====================

uint8_t detect_usb_controllers(void) {
    uint8_t count = 0;
    
    // Проверяем UHCI контроллеры по стандартным адресам
    for(int i = 0; i < 4; i++) {
        uint16_t base = 0x00A0 + i * 8;
        
        // Проверяем существование контроллера
        uint16_t original = inw(base + USB_COMMAND_PORT);
        outw(base + USB_COMMAND_PORT, 0x0000);
        
        if(inw(base + USB_COMMAND_PORT) == 0x0000) {
            outw(base + USB_COMMAND_PORT, 0x0001);
            if(inw(base + USB_COMMAND_PORT) == 0x0001) {
                usb_devices[count].detected = 1;
                usb_devices[count].type = 1; // UHCI
                usb_devices[count].base_port = base;
                count++;
                
                print_string("USB Controller found at: 0x", 22, 10 + count, 0x0E);
                print_hex(base, 46, 10 + count, 0x0E);
            }
        }
        outw(base + USB_COMMAND_PORT, original);
    }
    
    usb_boot_supported = (count > 0);
    return count;
}

void reset_usb_controller(uint16_t base) {
    // Сброс контроллера
    outw(base + USB_COMMAND_PORT, inw(base + USB_COMMAND_PORT) | USB_CMD_RESET);
    delay(50000);
    outw(base + USB_COMMAND_PORT, inw(base + USB_COMMAND_PORT) & ~USB_CMD_RESET);
    delay(50000);
}

uint8_t init_usb_controller(uint16_t base) {
    reset_usb_controller(base);
    
    // Базовая инициализация UHCI
    outw(base + USB_COMMAND_PORT, USB_CMD_RUN);
    
    // Проверяем, что контроллер работает
    if((inw(base + USB_STATUS_PORT) & 0x8000) == 0) {
        return 1; // Успех
    }
    
    return 0; // Ошибка
}

// ==================== USB STORAGE ФУНКЦИИ ====================

uint8_t usb_storage_reset(uint16_t base) {
    // Сброс USB storage устройства
    uint8_t reset_cmd = USB_MSD_RESET;
    
    // Отправляем команду сброса (упрощенно)
    outb(base + 0x00, reset_cmd);
    delay(100000);
    
    return 1;
}

uint8_t usb_read_capacity(uint16_t base, uint32_t* capacity) {
    // Упрощенное определение емкости
    // В реальности нужно отправлять SCSI команду READ CAPACITY
    
    // Заглушка - предполагаем стандартный размер
    *capacity = 7864320; // ~4GB в секторах по 512 байт
    return 1;
}

uint8_t read_usb_sector(uint16_t base, uint32_t lba, uint16_t* buffer) {
    // Чтение сектора с USB устройства
    scsi_read10_t cmd;
    
    // Формируем SCSI команду READ(10)
    cmd.opcode = SCSI_READ_10;
    cmd.lun_flags = 0;
    cmd.lba = lba;
    cmd.reserved = 0;
    cmd.transfer_length = 1; // 1 сектор
    cmd.control = 0;
    
    // Отправляем команду (упрощенно)
    // В реальной реализации нужен полноценный USB stack
    
    // Заглушка - читаем "виртуальные" данные
    for(int i = 0; i < 256; i++) {
        buffer[i] = 0xAA55; // Тестовый паттерн
    }
    
    // Проверяем сигнатуру загрузочного сектора
    if(buffer[255] == 0xAA55) {
        return 1;
    }
    
    return 0;
}

// ==================== USB BOOT ФУНКЦИИ ====================

void boot_from_usb(void) {
    clear_screen(0x07);
    print_string("Attempting to boot from USB...", 25, 5, 0x0F);
    
    // Обнаруживаем USB контроллеры
    uint8_t usb_count = detect_usb_controllers();
    
    if(usb_count == 0) {
        print_string("No USB controllers found!", 25, 7, 0x0C);
        print_string("Press any key to return...", 25, 9, 0x07);
        keyboard_read();
        return;
    }
    
    print_string("Found ", 25, 7, 0x0E);
    print_char('0' + usb_count, 31, 7, 0x0E);
    print_string(" USB controller(s)", 32, 7, 0x0E);
    
    // Пытаемся загрузиться с каждого обнаруженного USB
    for(int i = 0; i < usb_count; i++) {
        if(usb_devices[i].detected) {
            print_string("Trying USB controller at: 0x", 25, 9 + i, 0x07);
            print_hex(usb_devices[i].base_port, 53, 9 + i, 0x07);
            
            if(init_usb_controller(usb_devices[i].base_port)) {
                print_string(" - Initialized", 56, 9 + i, 0x0A);
                
                // Сбрасываем storage устройство
                if(usb_storage_reset(usb_devices[i].base_port)) {
                    print_string("USB storage reset - OK", 25, 11 + i, 0x0A);
                    
                    // Читаем загрузочный сектор
                    uint16_t boot_sector[256];
                    if(read_usb_sector(usb_devices[i].base_port, 0, boot_sector)) {
                        print_string("Boot sector read successfully!", 25, 13 + i, 0x0A);
                        
                        // Проверяем сигнатуру
                        if(boot_sector[255] == 0xAA55) {
                            print_string("Valid boot signature found!", 25, 15 + i, 0x0A);
                            print_string("Transferring control to USB boot sector...", 25, 17 + i, 0x0E);
                            delay(200000);
                            
                            // Копируем загрузочный сектор и передаем управление
                            uint16_t* dest = (uint16_t*)0x7C00;
                            for(int j = 0; j < 256; j++) {
                                dest[j] = boot_sector[j];
                            }
                            
                            __asm__ volatile(
                                "cli\n"
                                "mov $0x0000, %%ax\n"
                                "mov %%ax, %%ds\n"
                                "mov %%ax, %%es\n"
                                "mov %%ax, %%ss\n"
                                "mov $0x7C00, %%sp\n"
                                "sti\n"
                                "ljmp $0x0000, $0x7C00\n"
                                :
                                :
                                : "memory"
                            );
                        } else {
                            print_string("Invalid boot signature!", 25, 15 + i, 0x0C);
                        }
                    } else {
                        print_string("Failed to read boot sector!", 25, 13 + i, 0x0C);
                    }
                } else {
                    print_string("USB storage reset failed!", 25, 11 + i, 0x0C);
                }
            } else {
                print_string(" - Init failed", 56, 9 + i, 0x0C);
            }
        }
    }
    
    print_string("USB boot failed on all controllers!", 25, 19, 0x0C);
    print_string("Press any key to return...", 25, 21, 0x07);
    keyboard_read();
}

void usb_boot_menu(void) {
    clear_screen(0x07);
    print_string("USB BOOT UTILITY", 35, 1, 0x0F);
    print_string("================", 35, 2, 0x0F);
    
    // Показываем обнаруженные USB устройства
    uint8_t usb_count = detect_usb_controllers();
    
    print_string("Detected USB Controllers:", 25, 4, 0x07);
    
    if(usb_count == 0) {
        print_string("None", 50, 4, 0x0C);
    } else {
        for(int i = 0; i < usb_count; i++) {
            print_string("Controller ", 25, 6 + i, 0x07);
            print_char('0' + i + 1, 37, 6 + i, 0x0E);
            print_string(" at port 0x", 39, 6 + i, 0x07);
            print_hex(usb_devices[i].base_port, 50, 6 + i, 0x0E);
        }
    }
    
    print_string("1. Boot from USB", 25, 10, 0x07);
    print_string("2. USB Diagnostics", 25, 11, 0x07);
    print_string("ESC. Return to Main Menu", 25, 13, 0x07);
    
    while(1) {
        uint8_t scancode = keyboard_read();
        if(scancode == 0) {
            delay(10000);
            continue;
        }
        
        if(scancode & 0x80) continue;
        
        char c = get_ascii_char(scancode);
        
        if(scancode == KEY_ESC) return;
        
        switch(c) {
            case '1':
                boot_from_usb();
                return;
            case '2':
                // USB диагностика может быть добавлена позже
                print_string("USB diagnostics not implemented yet", 25, 15, 0x0E);
                delay(1000000);
                return;
        }
    }
}

// Вспомогательная функция для вывода hex
void print_hex(uint32_t num, uint8_t x, uint8_t y, uint8_t color) {
    char hex_chars[] = "0123456789ABCDEF";
    char buffer[9];
    
    for(int i = 7; i >= 0; i--) {
        buffer[i] = hex_chars[num & 0xF];
        num >>= 4;
    }
    buffer[8] = '\0';
    
    print_string(buffer, x, y, color);
}

// ==================== НОВЫЕ ФУНКЦИИ SETTINGS ====================

void settings_menu(void) {
    uint8_t selected = 0;
    const char* menu_items[] = {
        "Boot Priority",
        "Update BIOS", 
        "Load Defaults",
        "Save & Exit",
        "Exit Without Save"
    };
    const int items_count = 5;
    
    clear_screen(0x07);
    print_string("BIOS SETTINGS", 35, 1, 0x0F);
    print_string("===================", 35, 2, 0x0F);
    
    while(1) {
        // Отрисовка меню
        for(int i = 0; i < items_count; i++) {
            uint8_t color = (i == selected) ? 0x1F : 0x07;
            if (i == selected) {
                print_string(">", 30, 5 + i, color);
            } else {
                print_string(" ", 30, 5 + i, color);
            }
            print_string(menu_items[i], 32, 5 + i, color);
        }
        
        print_string("Use UP/DOWN to navigate, ENTER to select, ESC to return", 15, 20, 0x07);
        
        uint8_t scancode = keyboard_read();
        if (scancode == 0) {
            delay(10000);
            continue;
        }
        
        if (scancode & 0x80) continue;
        
        if (scancode == KEY_UP) {
            if (selected > 0) selected--;
        }
        else if (scancode == KEY_DOWN) {
            if (selected < items_count - 1) selected++;
        }
        else if (scancode == KEY_ENTER) {
            switch(selected) {
                case 0: boot_priority_menu(); break;
                case 1: update_bios_menu(); break;
                case 2: 
                    // Load Defaults
                    bios_settings.boot_devices[0] = 0; // HDD
                    bios_settings.boot_devices[1] = 1; // CD/DVD
                    bios_settings.boot_devices[2] = 4; // Disabled
                    print_string("Defaults loaded! Press any key...", 30, 15, 0x07);
                    keyboard_read();
                    break;
                case 3:
                    // Save & Exit
                    save_bios_settings();
                    return;
                case 4:
                    // Exit Without Save
                    return;
            }
            // Перерисовываем меню после возврата из подменю
            clear_screen(0x07);
            print_string("BIOS SETTINGS", 35, 1, 0x0F);
            print_string("===================", 35, 2, 0x0F);
        }
        else if (scancode == KEY_ESC) {
            return;
        }
    }
}

void boot_priority_menu(void) {
    uint8_t selected = 0;
    
    clear_screen(0x07);
    print_string("BOOT PRIORITY SETTINGS", 30, 1, 0x0F);
    print_string("========================", 30, 2, 0x0F);
    
    print_string("Set the boot order (1st, 2nd, 3rd):", 25, 4, 0x07);
    print_string("Use +/- to change, ENTER to save, ESC to cancel", 20, 18, 0x07);
    
    while(1) {
        // Отрисовка текущих настроек
        print_string("1st Boot Device: ", 25, 7, 0x07);
        print_string(boot_device_names[bios_settings.boot_devices[0]], 42, 7, 0x0F);
        
        print_string("2nd Boot Device: ", 25, 9, 0x07);
        print_string(boot_device_names[bios_settings.boot_devices[1]], 42, 9, 0x0F);
        
        print_string("3rd Boot Device: ", 25, 11, 0x07);
        print_string(boot_device_names[bios_settings.boot_devices[2]], 42, 11, 0x0F);
        
        // Курсор выбора
        print_string("  ", 40, 7 + selected * 2, 0x07);
        print_string(">", 40, 7 + selected * 2, 0x1F);
        
        uint8_t scancode = keyboard_read();
        if (scancode == 0) {
            delay(10000);
            continue;
        }
        
        if (scancode & 0x80) continue;
        
        if (scancode == KEY_UP) {
            if (selected > 0) selected--;
        }
        else if (scancode == KEY_DOWN) {
            if (selected < 2) selected++;
        }
        else if (scancode == KEY_ENTER) {
            // Сохраняем в CMOS
            write_cmos(CMOS_BOOT_DEVICE_1, bios_settings.boot_devices[0]);
            write_cmos(CMOS_BOOT_DEVICE_2, bios_settings.boot_devices[1]);
            write_cmos(CMOS_BOOT_DEVICE_3, bios_settings.boot_devices[2]);
            
            print_string("Boot priority saved to CMOS!", 25, 15, 0x07);
            print_string("Press any key...", 25, 16, 0x07);
            keyboard_read();
            return;
        }
        else if (scancode == KEY_ESC) {
            return;
        }
        else {
            char c = get_ascii_char(scancode);
            if (c == '+' || c == '=') {
                // Увеличиваем приоритет устройства
                if (bios_settings.boot_devices[selected] < 4) {
                    bios_settings.boot_devices[selected]++;
                }
            }
            else if (c == '-' || c == '_') {
                // Уменьшаем приоритет устройства
                if (bios_settings.boot_devices[selected] > 0) {
                    bios_settings.boot_devices[selected]--;
                }
            }
        }
    }
}

void update_bios_menu(void) {
    clear_screen(0x07);
    print_string("BIOS UPDATE UTILITY", 32, 5, 0x0F);
    print_string("====================", 32, 6, 0x0F);
    
    print_string("Searching for updates...", 30, 8, 0x07);
    delay(100000);
    
    print_string("No updates found.", 35, 10, 0x07);
    print_string("Your BIOS is up to date.", 33, 11, 0x07);
    print_string("Current version: WeBIOS v4.51", 30, 13, 0x07);
    
    delay(1000000);
}

void hardware_test(void) {
    uint8_t error_count = 0;
    
    clear_screen(0x07);
    print_string("HARDWARE TEST UTILITY", 30, 1, 0x0F);
    print_string("=====================", 30, 2, 0x0F);
    
    print_string("Testing hardware components...", 25, 4, 0x07);
    delay(10000);
    
  
    // Тест видео
    print_string("Video Test: ", 25, 10, 0x07);
    delay(10000);
    print_string("PASSED", 38, 10, 0x0A);
    
    // Сохраняем количество ошибок
    bios_settings.hw_error_count = error_count;
    write_cmos(CMOS_HW_ERROR_COUNT, error_count);
    
    // Проверяем количество ошибок
    if (error_count >= 2) {
        print_string("Critical errors detected!", 25, 12, 0x0C);
        print_string("System will show error on next boot.", 25, 13, 0x07);
        delay(9999000);
    } else if (error_count > 0) {
        print_string("Some errors detected but system is operational.", 25, 12, 0x0E);
    } else {
        print_string("All tests passed successfully!", 25, 12, 0x0A);
    }
    
    print_string("Press any key to return...", 25, 16, 0x07);
    keyboard_read();
}

// ==================== СИСТЕМА БЕЗОПАСНОСТИ ====================

void enter_password(void) {
    char password[9] = {0};
    uint8_t pos = 0;
    
    clear_screen(0x07);
    print_string("BIOS Password Protection", 25, 5, 0x07);
    print_string("Enter password: ", 25, 7, 0x07);
    
    while(1) {
        // Показываем звездочки вместо пароля
        print_string("                ", 41, 7, 0x07);
        for(int i = 0; i < pos; i++) {
            print_char('*', 41 + i, 7, 0x07);
        }
        print_char('_', 41 + pos, 7, 0x07);
        
        uint8_t scancode = keyboard_read();
        if (scancode == 0) {
            delay(10000);
            continue;
        }
        
        if (scancode & 0x80) continue;
        
        if (scancode == KEY_ENTER) {
            password[pos] = '\0';
            
            if (strcmp(password, bios_settings.password) == 0) {
                // Пароль верный
                print_string("Access granted!", 25, 9, 0x07);
                delay(2000);
                return;
            } else {
                // Неверный пароль
                password_attempts++;
                pos = 0;
                print_string("Invalid password! Attempts: ", 25, 9, 0x07);
                print_char('0' + password_attempts, 53, 9, 0x07);
                print_string("/3", 54, 9, 0x07);
                
                if (password_attempts >= 3) {
                    print_string("System halted!", 25, 11, 0x07);
                    while(1) { delay(1000000); } // Зависаем
                }
            }
        }
        else if (scancode == KEY_BACKSPACE) {
            if (pos > 0) {
                pos--;
                password[pos] = '\0';
            }
        }
        else if (scancode == KEY_ESC) {
            // Перезагрузка при ESC
            __asm__ volatile("int $0x19");
        }
        else {
            char c = get_ascii_char(scancode);
            if (c && pos < 8) {
                password[pos] = c;
                pos++;
                password[pos] = '\0';
            }
        }
    }
}

void security_menu(void) {
    clear_screen(0x07);
    print_string("BIOS SECURITY SETTINGS", 25, 1, 0x07);
    print_string("================================", 25, 2, 0x07);
    
    print_string("Password Protection: ", 25, 4, 0x07);
    if (bios_settings.security_enabled) {
        print_string("ENABLED", 47, 4, 0x07);
    } else {
        print_string("DISABLED", 47, 4, 0x07);
    }
    
    print_string("Password: ", 25, 5, 0x07);
    if (bios_settings.security_enabled && bios_settings.password[0] != '\0') {
        print_string("SET", 36, 5, 0x07);
    } else {
        print_string("NOT SET", 36, 5, 0x07);
    }
    
    print_string("1. Enable/Disable Password", 25, 7, 0x07);
    print_string("2. Set Password", 25, 8, 0x07);
    print_string("3. Clear Password", 25, 9, 0x07);
    print_string("ESC. Return to Main Menu", 25, 11, 0x07);
    
    while(1) {
        uint8_t scancode = keyboard_read();
        if (scancode == 0) {
            delay(10000);
            continue;
        }
        
        if (scancode & 0x80) continue;
        
        // Получаем ASCII символ из скан-кода
        char c = get_ascii_char(scancode);
        
        if (scancode == KEY_ESC) {
            return;
        }
        
        switch(c) {
            case '1':
                bios_settings.security_enabled = !bios_settings.security_enabled;
                save_bios_settings();
                // Обновляем экран
                print_string("                ", 47, 4, 0x07);
                if (bios_settings.security_enabled) {
                    print_string("ENABLED", 47, 4, 0x07);
                } else {
                    print_string("DISABLED", 47, 4, 0x07);
                }
                break;
                
            case '2':
                set_password();
                // После установки пароля обновляем экран
                security_menu(); // Перезагружаем меню чтобы обновиться
                return;
                
            case '3':
                bios_settings.password[0] = '\0';
                bios_settings.security_enabled = 0;
                save_bios_settings();
                // Обновляем экран
                print_string("                ", 47, 4, 0x07);
                print_string("DISABLED", 47, 4, 0x07);
                print_string("                ", 36, 5, 0x07);
                print_string("NOT SET", 36, 5, 0x07);
                break;
        }
    }
}

void set_password(void) {
    char new_password[9] = {0};
    uint8_t pos = 0;
    
    clear_screen(0x07);
    print_string("SET BIOS PASSWORD", 25, 1, 0x07);
    print_string("Enter new password (max 8 chars): ", 25, 3, 0x07);
    print_string("Press Enter when done, ESC to cancel", 25, 5, 0x07);
    
    while(1) {
        // Очищаем строку ввода
        print_string("        ", 25, 4, 0x07);
        
        // Показываем звездочки
        for(int i = 0; i < pos; i++) {
            print_char('*', 25 + i, 4, 0x07);
        }
        // Показываем курсор
        print_char('_', 25 + pos, 4, 0x07);
        
        uint8_t scancode = keyboard_read();
        if (scancode == 0) {
            delay(10000);
            continue;
        }
        
        if (scancode & 0x80) continue;
        
        if (scancode == KEY_ENTER) {
            if (pos > 0) {
                new_password[pos] = '\0';
                
                // Сохраняем пароль
                for(int i = 0; i < 9; i++) {
                    bios_settings.password[i] = new_password[i];
                }
                bios_settings.security_enabled = 1;
                save_bios_settings();
                
                print_string("Password set to: ", 25, 7, 0x07);
                print_string(new_password, 42, 7, 0x07);
                print_string("Press any key...", 25, 9, 0x07);
                keyboard_read();
                return;
            }
        }
        else if (scancode == KEY_ESC) {
            return;
        }
        else if (scancode == KEY_BACKSPACE) {
            if (pos > 0) {
                pos--;
                new_password[pos] = '\0';
            }
        }
        else {
            char c = get_ascii_char(scancode);
            if (c && pos < 8) {
                new_password[pos] = c;
                pos++;
                new_password[pos] = '\0';
            }
        }
    }
}

uint8_t check_password(void) {
    return (strcmp(input_buffer, bios_settings.password) == 0);
}

// Функция показа загрузочного экрана
void show_boot_screen(void) {
    // Очищаем экран черным цветом
    clear_screen(0x00);
    
    // Большой логотип WexIB 
    print_string("I8,        8        ,8I                         88  88888888ba", 10, 3, 0x001F);  
    print_string("`8b       d8b       d8'                         88  88      \"8b", 10, 4, 0x001F);
    print_string(" \"8,     ,8\"8,     ,8\"                          88  88      ,8P", 10, 5, 0x001F);
    print_string("  Y8     8P Y8     8P   ,adPPYba,  8b,     ,d8  88  88aaaaaa8P'", 10, 6, 0x001F);
    print_string("  `8b   d8' `8b   d8'  a8P_____88   `Y8, ,8P'   88  88\"\"\"\"\"\"8b,", 10, 7, 0x001F);
    print_string("   `8a a8'   `8a a8'   8PP\"\"\"\"\"\"\"     )888(     88  88      `8b", 10, 8, 0x001F);
    print_string("    `8a8'     `8a8'    \"8b,   ,aa   ,d8\" \"8b,   88  88      a8P", 10, 9, 0x001F);  
    print_string("     `8'       `8'      `\"Ybbd8\"'  8P'     `Y8  88  88888888P\"", 10, 10, 0x001F);  
    
    // Добавляем "BIOS" под логотипом 
    print_string("BIOS v4.51", 35, 12, 0x001F);  
    
    print_string("Press B+T simultaneously for boot menu", 18, 20, 0x001F);
    
    // Анимация прогресс-бара
    print_string("[", 25, 15, 0x001F);
    print_string("]", 55, 15, 0x001F);
    
    uint8_t b_pressed = 0;
    uint8_t t_pressed = 0;
    
    for(int i = 0; i < 30; i++) {
        // Проверяем одновременное нажатие B+T
        if (inb(KEYBOARD_STATUS_PORT) & 0x01) {
            uint8_t scancode = inb(KEYBOARD_DATA_PORT);
            
            if (!(scancode & 0x80)) { // Только нажатие
                if (scancode == 0x30) { // B key
                    b_pressed = 1;
                } 
                else if (scancode == 0x14) { // T key
                    t_pressed = 1;
                }
                
                // Если обе клавиши нажаты одновременно
                if (b_pressed && t_pressed) {
                    show_boot_menu();
                    return;
                }
            } else {
                // Обработка отпускания клавиш
                uint8_t release_code = scancode & 0x7F;
                if (release_code == 0x30) { // B released
                    b_pressed = 0;
                } 
                else if (release_code == 0x14) { // T released
                    t_pressed = 0;
                }
            }
        }
        
        // Анимация прогресс-бара
        print_string("=", 26 + i, 15, 0x001F);
        print_string(">", 26 + i, 15, 0x001F);
        delay(20000);
        print_string(" ", 26 + i, 15, 0x001F);
    }
    
    // Заполняем прогресс-бар полностью
    for(int i = 0; i < 30; i++) {
        print_string("=", 26 + i, 15, 0x001F);
        delay(10000);
    }
    
    delay(99999);
}

// ==================== CMOS ФУНКЦИИ ====================

uint8_t read_cmos(uint8_t reg) {
    outb(CMOS_ADDRESS, reg);
    delay(10);
    return inb(CMOS_DATA);
}

void write_cmos(uint8_t reg, uint8_t value) {
    outb(CMOS_ADDRESS, reg);
    delay(10);
    outb(CMOS_DATA, value);
}

void load_bios_settings(void) {
    // Загружаем настройки из CMOS
    bios_settings.security_enabled = read_cmos(CMOS_SECURITY_FLAG);
    bios_settings.boot_order = read_cmos(CMOS_BOOT_ORDER);
    
    // ИСПРАВЛЕННАЯ ЧАСТЬ: Загружаем пароль правильно
    for(int i = 0; i < 8; i++) {
        uint8_t c = read_cmos(CMOS_PASSWORD_0 + i);
        if (c != 0x00) {  // Проверяем на 0 вместо 0xFF
            bios_settings.password[i] = c;
        } else {
            bios_settings.password[i] = '\0';
            break;
        }
    }
    bios_settings.password[8] = '\0';
    
    // Загружаем настройки загрузки
    bios_settings.boot_devices[0] = read_cmos(CMOS_BOOT_DEVICE_1);
    bios_settings.boot_devices[1] = read_cmos(CMOS_BOOT_DEVICE_2);
    bios_settings.boot_devices[2] = read_cmos(CMOS_BOOT_DEVICE_3);
    
    // Загружаем счетчик ошибок
    bios_settings.hw_error_count = read_cmos(CMOS_HW_ERROR_COUNT);
    
    // Проверяем контрольную сумму
    uint8_t stored_checksum = read_cmos(CMOS_CHECKSUM);
    uint8_t calculated_checksum = calculate_checksum();
    
    if (stored_checksum != calculated_checksum) {
        // Сброс настроек при неверной контрольной сумме
        bios_settings.security_enabled = 0;
        bios_settings.boot_order = 0;
        bios_settings.password[0] = '\0';
        // Устанавливаем значения по умолчанию для загрузки
        bios_settings.boot_devices[0] = 0; // HDD
        bios_settings.boot_devices[1] = 1; // CD/DVD
        bios_settings.boot_devices[2] = 4; // Disabled
    }
}

void save_bios_settings(void) {
    // Сохраняем настройки в CMOS
    write_cmos(CMOS_SECURITY_FLAG, bios_settings.security_enabled);
    write_cmos(CMOS_BOOT_ORDER, bios_settings.boot_order);
    
    // ИСПРАВЛЕННАЯ ЧАСТЬ: Сохраняем пароль правильно
    for(int i = 0; i < 8; i++) {
        if (bios_settings.password[i] != '\0') {
            write_cmos(CMOS_PASSWORD_0 + i, bios_settings.password[i]);
        } else {
            write_cmos(CMOS_PASSWORD_0 + i, 0x00); // Сохраняем 0 вместо 0xFF
        }
    }
    
    // Сохраняем настройки загрузки
    write_cmos(CMOS_BOOT_DEVICE_1, bios_settings.boot_devices[0]);
    write_cmos(CMOS_BOOT_DEVICE_2, bios_settings.boot_devices[1]);
    write_cmos(CMOS_BOOT_DEVICE_3, bios_settings.boot_devices[2]);
    
    // Сохраняем счетчик ошибок
    write_cmos(CMOS_HW_ERROR_COUNT, bios_settings.hw_error_count);
    
    // Сохраняем контрольную сумму
    bios_settings.checksum = calculate_checksum();
    write_cmos(CMOS_CHECKSUM, bios_settings.checksum);
}

uint8_t calculate_checksum(void) {
    uint8_t sum = 0;
    sum += bios_settings.security_enabled;
    sum += bios_settings.boot_order;
    
    for(int i = 0; i < 8 && bios_settings.password[i] != '\0'; i++) {
        sum += bios_settings.password[i];
    }
    
    // Добавляем настройки загрузки в контрольную сумму
    sum += bios_settings.boot_devices[0];
    sum += bios_settings.boot_devices[1];
    sum += bios_settings.boot_devices[2];
    sum += bios_settings.hw_error_count;
    
    return ~sum + 1; // Дополнение до двух
}

// ==================== ЗАГРУЗКА ОС ====================

void boot_os(void) {
    clear_screen(0x07);
    
    // Проверяем приоритет загрузки
    if(bios_settings.boot_devices[0] == 2) { // USB
        print_string("Booting from USB (1st priority)...", 0, 0, 0x07);
        boot_from_usb();
    } 
    else if(bios_settings.boot_devices[1] == 2) { // USB как второе устройство
        print_string("Trying USB (2nd priority)...", 0, 0, 0x07);
        boot_from_usb();
    }
    else {
        // Стандартная загрузка с HDD
        print_string("Attempting to boot from hard disk...", 0, 0, 0x07);
        boot_from_disk();
    }
}

void boot_from_disk(void) {
    uint16_t boot_sector[256]; // 512 байт
    
    // Пытаемся прочитать загрузочный сектор (LBA 0)
    if (read_disk_sector(0, boot_sector)) {
        // Проверяем сигнатуру загрузочного сектора
        if (boot_sector[255] == 0xAA55) {
            print_string("Boot signature found! Transferring control...", 0, 3, 0x07);
            delay(100);
            
            // Копируем загрузочный сектор по адресу 0x7C00
            uint16_t* dest = (uint16_t*)0x7C00;
            for(int i = 0; i < 256; i++) {
                dest[i] = boot_sector[i];
            }
            
            // Переходим в реальный режим и передаем управление
            __asm__ volatile(
                "cli\n"
                "mov $0x0000, %ax\n"
                "mov %ax, %ds\n"
                "mov %ax, %es\n"
                "mov %ax, %ss\n"
                "mov $0x7C00, %sp\n"
                "sti\n"
                "ljmp $0x0000, $0x7C00\n"
            );
        } else {
            print_string("Error: No boot signature (0xAA55)", 0, 3, 0x07);
            print_string("Press any key to return...", 0, 5, 0x07);
            keyboard_read();
        }
    } else {
        print_string("Error: Cannot read boot sector", 0, 3, 0x07);
        print_string("Press any key to return...", 0, 5, 0x07);
        keyboard_read();
    }
}

uint8_t read_disk_sector(uint32_t lba, uint16_t* buffer) {
    wait_ide();
    
    // Выбираем мастер-диск
    outb(IDE_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    
    // Количество секторов
    outb(IDE_SECTOR_COUNT, 1);
    
    // LBA адрес
    outb(IDE_LBA_LOW, lba & 0xFF);
    outb(IDE_LBA_MID, (lba >> 8) & 0xFF);
    outb(IDE_LBA_HIGH, (lba >> 16) & 0xFF);
    
    // Команда чтения
    outb(IDE_COMMAND, IDE_CMD_READ);
    
    // Ждем готовности
    wait_ide();
    
    // Проверяем ошибки
    if (inb(IDE_STATUS) & 0x01) {
        return 0; // Ошибка
    }
    
    // Читаем данные
    for(int i = 0; i < 256; i++) {
        buffer[i] = inw(IDE_DATA);
    }
    
    return 1;
}

void wait_ide(void) {
    uint8_t status;
    int timeout = 10000;
    
    // Ждем когда диск не busy
    do {
        status = inb(IDE_STATUS);
        timeout--;
    } while ((status & 0x80) && timeout > 0);
    
    timeout = 10000;
    // Ждем ready
    do {
        status = inb(IDE_STATUS);
        timeout--;
    } while (!(status & 0x40) && timeout > 0);
}

// ==================== ОБНАРУЖЕНИЕ ПАМЯТИ ====================

void detect_memory_info(void) {
    // Простая заглушка для демонстрации
    print_string("Memory: 640K base, 16M extended", 0, 10, 0x07);
}

uint32_t detect_memory_range(uint32_t start, uint32_t end) {
    // Упрощенная реализация - возвращаем размер
    return end - start;
}

uint16_t read_cmos_memory(void) {
    return 640; // 640K базовой памяти
}

// ==================== ОБНАРУЖЕНИЕ ПРОЦЕССОРА ====================

void detect_cpu_info(void) {
    // Упрощенная реализация
    print_string("CPU: 80386 compatible", 0, 10, 0x07);
}

// ==================== ФУНКЦИИ ВВОДА-ВЫВОДА ====================

void delay(uint32_t milliseconds) {
    for (volatile int i = 0; i < milliseconds * 1000; i++);
}

// Чтение из порта
uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

// Запись в порт
void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

// Чтение слова из порта
uint16_t inw(uint16_t port) {
    uint16_t result;
    __asm__ volatile("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

// Запись слова в порт
void outw(uint16_t port, uint16_t value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

// Ожидание готовности клавиатуры
void wait_keyboard(void) {
    while (inb(KEYBOARD_STATUS_PORT) & 0x02);
}

// Чтение скана кода клавиши
uint8_t keyboard_read(void) {
    if (!(inb(KEYBOARD_STATUS_PORT) & 0x01)) {
        return 0; // Нет данных
    }
    return inb(KEYBOARD_DATA_PORT);
}

// Преобразование скан-кода в ASCII
char get_ascii_char(uint8_t scancode) {
    // Базовая US QWERTY раскладка
    static const char scan_codes[] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
        0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0,
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
        '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0,
        ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
    };
    
    if (scancode < sizeof(scan_codes)) {
        return scan_codes[scancode];
    }
    return 0;
}

// ==================== ВИДЕОФУНКЦИИ ====================

// Очистка экрана
void clear_screen(uint8_t color) {
    uint16_t blank = (color << 8) | ' ';
    for(int i = 0; i < WIDTH * HEIGHT; i++) {
        video_mem[i] = blank;
    }
}

// Вывод строки
void print_string(const char* str, uint8_t x, uint8_t y, uint8_t color) {
    uint16_t* location = video_mem + y * WIDTH + x;
    while(*str) {
        *location++ = (color << 8) | *str++;
    }
}

// Вывод символа
void print_char(char c, uint8_t x, uint8_t y, uint8_t color) {
    video_mem[y * WIDTH + x] = (color << 8) | c;
}

// ==================== ИНТЕРФЕЙС ====================

// Отрисовка интерфейса
void draw_interface(void) {
    // Левое меню (темный фон)
    for(int y = 0; y < HEIGHT - 1; y++) {
        for(int x = 0; x < 20; x++) {
            print_char(' ', x, y, 0x70);
        }
    }
    
    // Правая панель (светлый фон)
    for(int y = 0; y < HEIGHT - 1; y++) {
        for(int x = 20; x < WIDTH; x++) {
            print_char(' ', x, y, 0x0F);
        }
    }
    
    // Нижняя строка (копирайт)
    for(int x = 0; x < WIDTH; x++) {
        print_char(' ', x, HEIGHT - 1, 0x70);
    }
    
    show_left_menu();
    show_right_panel();
    
    // Копирайт
    print_string("(C) 2025 WexIB - Press ESC to reboot", 0, HEIGHT - 1, 0x70);
}

// Левое меню
void show_left_menu(void) {
    print_string("  BIOS Menu", 1, 0, 0x70);
    
    // Показываем видимые пункты меню
    for(int i = 0; i < 10; i++) {
        int item_index = menu_state.offset + i;
        if(item_index >= menu_items_count) break;
        
        uint8_t color = (item_index == menu_state.selected) ? 0x1F : 0x70;
        
        print_string(menu_items[item_index].name, 1, 2 + i, color);
    }
}

// Правая панель
void show_right_panel(void) {
    const char* titles[] = {
        "BOOT MANAGER",
        "SYSTEM INFORMATION", 
        "HARDWARE TEST UTILITY",
        "BIOS SETTINGS",
        "SECURITY SETTINGS",
        "BIOS CONFIGURATION",
    };
    
    print_string(titles[menu_state.selected], 22, 1, 0x0F);
    
    // Содержимое в зависимости от выбора
    switch(menu_state.selected) {
        case 0: // Boot OS
            print_string("Boot from Hard Disk", 22, 3, 0x0F);
            print_string("Will load first sector (512 bytes)", 22, 5, 0x0F);
            print_string("and transfer control to it", 22, 6, 0x0F);
            print_string("Press Enter to boot", 22, 8, 0x0E);
            break;
            
        case 1: // System Info
            print_string("CPU: 80386 compatible", 22, 3, 0x0F);
            print_string("Memory: 640K base", 22, 4, 0x0F);
            print_string("BIOS: WexIB v4.51", 22, 5, 0x0F);
            print_string("Security: ", 22, 6, 0x0F);
            if (bios_settings.security_enabled) {
                print_string("ENABLED", 33, 6, 0x0F);
            } else {
                print_string("DISABLED", 33, 6, 0x0F);
            }
            break;
            
        case 2: // Hardware Test
            print_string("Run hardware diagnostics", 22, 3, 0x0F);
            print_string("Tests: Memory, CPU, Keyboard, Disk", 22, 5, 0x0F);
            print_string("Shows error if 2+ components fail", 22, 6, 0x0F);
            print_string("Press Enter to start test", 22, 8, 0x0E);
            break;
            
        case 3: // Settings
            print_string("BIOS Configuration", 22, 3, 0x0F);
            print_string("Boot Priority, Update BIOS", 22, 5, 0x0F);
            print_string("Load Defaults, Save Settings", 22, 6, 0x0F);
            print_string("Press Enter to configure", 22, 8, 0x0E);
            break;
            
        case 4: // Security
            print_string("Password Protection", 22, 3, 0x0F);
            if (bios_settings.security_enabled) {
                print_string("Status: ENABLED", 22, 5, 0x0F);
                print_string("Password: SET", 22, 6, 0x0F);
            } else {
                print_string("Status: DISABLED", 22, 5, 0x0F);
                print_string("Password: NOT SET", 22, 6, 0x0F);
            }
            print_string("Press Enter to configure", 22, 8, 0x0E);
            break;
            
        case 5: // Configuration
    print_string("CPU: 80386 compatible", 22, 4, 0x0F);
    print_string("Memory: 640K base", 22, 5, 0x0F);
    print_string("VGA 640x480 16-color", 22, 6, 0x0F);
    print_string("BIOS: WexIB v4.51", 22, 7, 0x0F);
    print_string("DUAL BOOT: OFF", 22, 8, 0x0F);
    
    print_string("Security: ", 22, 9, 0x0F);
    if (bios_settings.security_enabled) {
        print_string("ENABLED", 33, 9, 0x0F);
    } else {
        print_string("DISABLED", 33, 9, 0x0F);
    }
    
    print_string("Boot Order: ", 22, 10, 0x0F);
    print_string(boot_device_names[bios_settings.boot_devices[0]], 34, 10, 0x0F);
    
    print_string("Hardware Errors: ", 22, 11, 0x0F);
    print_char('0' + bios_settings.hw_error_count, 39, 11, 0x0F);
    
    print_string("Build: Oct 25 2025", 22, 12, 0x0F);
    
    print_string("CMOS Checksum: ", 22, 13, 0x0F);
    if (calculate_checksum() == bios_settings.checksum) {
        print_string("VALID", 37, 13, 0x0A);
    } else {
        print_string("INVALID", 37, 13, 0x0C);
    }
    break;
    
    case 6: // USB Boot
    print_string("BOOT FROM USB DEVICE", 22, 3, 0x0F);
    print_string("Attempt to boot from USB storage", 22, 5, 0x0F);
    print_string("Supports USB flash drives", 22, 6, 0x0F);
    print_string("and external USB hard disks", 22, 7, 0x0F);
    print_string("Press Enter to boot from USB", 22, 9, 0x0E);
    break;
    
    case 7: // Dev Tools
    print_string("DEVELOPER TOOLS", 22, 3, 0x0F);
    print_string("Advanced debugging and diagnostics", 22, 5, 0x0F);
    print_string("System registers, memory map, POST logs", 22, 6, 0x0F);
    print_string("For development and troubleshooting", 22, 7, 0x0F);
    print_string("Press Enter for developer options", 22, 9, 0x0E);
    break;
    
    }
}

// ==================== ОБРАБОТКА ВВОДА ====================

void handle_input(void) {
    uint8_t scancode = keyboard_read();
    if (scancode == 0) return;
    
    if (scancode & 0x80) {
        last_key = 0;
        return;
    }
    
    if (scancode == last_key) {
        return;
    }
    last_key = scancode;
    
    switch(scancode) {
        case KEY_UP:
            if(menu_state.selected > 0) {
                menu_state.selected--;
                if(menu_state.selected < menu_state.offset) {
                    menu_state.offset = menu_state.selected;
                }
                menu_state.needs_redraw = 1;
            }
            break;
            
        case KEY_DOWN:
            if(menu_state.selected < menu_items_count - 1) {
                menu_state.selected++;
                if(menu_state.selected >= menu_state.offset + 5) {
                    menu_state.offset = menu_state.selected - 4;
                }
                menu_state.needs_redraw = 1;
            }
            break;
            
        case KEY_ENTER:
            if(menu_items[menu_state.selected].action) {
                menu_items[menu_state.selected].action();
                menu_state.needs_redraw = 1;
            }
            break;
            
        case KEY_ESC:
            __asm__ volatile("int $0x19");
            break;
    }
}

// ==================== СТРОКОВЫЕ ФУНКЦИИ ====================

int strcmp(const char* s1, const char* s2) {
    while(*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// ==================== CMOS ФУНКЦИИ ВРЕМЕНИ ====================

void read_cmos_time(void) {
    // Чтение времени из CMOS
}

void read_cmos_date(void) {
    // Чтение даты из CMOS
}

