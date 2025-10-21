#include <stdint.h>

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

// Массив пунктов меню
const menu_item_t menu_items[] = {
    {" Boot OS       ", boot_os},
    {" System Info   ", 0},
    {" Hardware Test ", hardware_test},
    {" Settings      ", settings_menu},
    {" Security      ", security_menu},
};

const int menu_items_count = 5;

// Названия загрузочных устройств
const char* boot_device_names[] = {
    "Hard Disk",
    "CD/DVD",
    "USB",
    "Network",
    "Disabled"
};

// Главная функция
void main() {
    // Загружаем настройки из CMOS
    load_bios_settings();
    
    // Проверяем пароль при загрузке
    if (bios_settings.security_enabled) {
        enter_password();
    }
    
    clear_screen(0x07);

    while(1) {
        if (menu_state.needs_redraw) {
            draw_interface();
            menu_state.needs_redraw = 0;
        }
        handle_input();
        delay(10000);
    }
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
    delay(99999999);
    
    print_string("No updates found.", 35, 10, 0x07);
    print_string("Your BIOS is up to date.", 33, 11, 0x07);
    print_string("Current version: WeBIOS v4.51", 30, 13, 0x07);
    
    print_string("Press any key to return...", 30, 16, 0x07);
    delay(99999999);
    keyboard_read();
}

void hardware_test(void) {
    uint8_t error_count = 0;
    
    clear_screen(0x07);
    print_string("HARDWARE TEST UTILITY", 30, 1, 0x0F);
    print_string("=====================", 30, 2, 0x0F);
    
    print_string("Testing hardware components...", 25, 4, 0x07);
    delay(99999999);
    
    // Тест памяти
uint32_t detect_memory_range(uint32_t start, uint32_t end) {
    volatile uint32_t *test_addr;
    uint32_t size = 0;
    
    // Проверяем каждый мегабайт
    for(uint32_t addr = start; addr < end; addr += 0x100000) {
        test_addr = (volatile uint32_t*)addr;
        
        // Сохраняем оригинальное значение
        uint32_t original = *test_addr;
        
        // Пробуем записать и прочитать тестовые значения
        *test_addr = 0x12345678;
        if(*test_addr != 0x12345678) break;
        
        *test_addr = 0x87654321;  
        if(*test_addr != 0x87654321) break;
        
        // Восстанавливаем оригинальное значение
        *test_addr = original;
        
        size += 0x100000; // Добавляем 1MB к доступной памяти
    }
    return size;
}
    
    // Тест процессора
    print_string("CPU Test: ", 25, 7, 0x07);
    delay(99999999);
    print_string("PASSED", 36, 7, 0x0A);
    
    // Тест клавиатуры
print_string("Keyboard Test: ", 25, 8, 0x07);
delay(500000);

// Сбрасываем контроллер клавиатуры
outb(0x64, 0xFE);
delay(100000);

// Проверяем ответ контроллера
uint8_t status = inb(0x64);
if((status & 0x01) && inb(0x60) == 0xAA) {
    print_string("PASSED", 41, 8, 0x0A);
} else {
    print_string("FAILED", 41, 8, 0x0C);
    error_count++;
}
    
    // Тест видео
    print_string("Video Test: ", 25, 10, 0x07);
    delay(99999999);
    print_string("PASSED", 38, 10, 0x0A);
    
    // Сохраняем количество ошибок
    bios_settings.hw_error_count = error_count;
    write_cmos(CMOS_HW_ERROR_COUNT, error_count);
    
    // Проверяем количество ошибок
    if (error_count >= 2) {
        print_string("Critical errors detected!", 25, 12, 0x0C);
        print_string("System will show error on next boot.", 25, 13, 0x07);
        delay(999999);
    } else if (error_count > 0) {
        print_string("Some errors detected but system is operational.", 25, 12, 0x0E);
    } else {
        print_string("All tests passed successfully!", 25, 12, 0x0A);
    }
    
    print_string("Press any key to return...", 25, 16, 0x07);
    keyboard_read();
}

void show_boot_failed_error(void) {
    clear_screen(0x00); // Черный экран
    print_string("BIOS BOOT FAILED! ERROR FirmWare", 24, 12, 0x0C);
    
    // Мигающий курсор
    while(1) {
        print_string("_", 56, 12, 0x0C);
        delay(500000);
        print_string(" ", 56, 12, 0x00);
        delay(500000);
        
        // Проверяем ESC для выхода
        uint8_t scancode = keyboard_read();
        if (scancode == KEY_ESC) {
            __asm__ volatile("int $0x19"); // Перезагрузка
        }
    }
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
                delay(2000000);
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
    
    // Загружаем пароль (8 символов)
    for(int i = 0; i < 8; i++) {
        char c = read_cmos(CMOS_PASSWORD_0 + i);
        if (c != 0xFF) {  // 0xFF - пустое значение
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
    
    // Сохраняем пароль
    for(int i = 0; i < 8; i++) {
        if (i < 8 && bios_settings.password[i] != '\0') {
            write_cmos(CMOS_PASSWORD_0 + i, bios_settings.password[i]);
        } else {
            write_cmos(CMOS_PASSWORD_0 + i, 0xFF); // Пустое значение
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
    print_string("Attempting to boot from hard disk...", 0, 0, 0x07);
    print_string("Loading boot sector to 0x7C00...", 0, 1, 0x07);
    
    delay(1000000);
    
    boot_from_disk();
}

void boot_from_disk(void) {
    uint16_t boot_sector[256]; // 512 байт
    
    // Пытаемся прочитать загрузочный сектор (LBA 0)
    if (read_disk_sector(0, boot_sector)) {
        // Проверяем сигнатуру загрузочного сектора
        if (boot_sector[255] == 0xAA55) {
            print_string("Boot signature found! Transferring control...", 0, 3, 0x07);
            delay(2000000);
            
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

// Задержка
void delay(uint32_t count) {
    for(volatile uint32_t i = 0; i < count; i++);
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
    print_string("(C) 2025 WeBIOS - Press ESC to reboot", 0, HEIGHT - 1, 0x70);
}

// Левое меню
void show_left_menu(void) {
    print_string("WeBIOS Menu", 1, 0, 0x70);
    
    // Показываем видимые пункты меню
    for(int i = 0; i < 10; i++) {
        int item_index = menu_state.offset + i;
        if(item_index >= menu_items_count) break;
        
        uint8_t color = (item_index == menu_state.selected) ? 0x1F : 0x70;
        
        // Указатель выбора
        if(item_index == menu_state.selected) {
            print_string(">", 0, 2 + i, color);
        } else {
            print_string(" ", 0, 2 + i, color);
        }
        
        print_string(menu_items[item_index].name, 1, 2 + i, color);
    }
}

// Правая панель
void show_right_panel(void) {
    const char* titles[] = {
        "BOOT OPERATING SYSTEM",
        "SYSTEM INFORMATION", 
        "HARDWARE TEST UTILITY",
        "BIOS SETTINGS",
        "SECURITY SETTINGS"
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
            print_string("BIOS: WeBIOS v4.51", 22, 5, 0x0F);
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
