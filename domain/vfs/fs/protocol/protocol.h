#ifndef OWL_VFS_FS_PROTOCOL
#define OWL_VFS_FS_PROTOCOL

#include <fuse3/fuse.h>
#include <cstdint>
#include <cstring>

#pragma pack(push, 1)

// =================================== Базовые определения ===================================

/// @brief Идентификаторы операций файловой системы
typedef enum fs_op_id_t {
    FS_OP_GETATTR       = 0x01,
    FS_OP_READDIR       = 0x02,
    FS_OP_OPEN          = 0x03,
    FS_OP_READ          = 0x04,
    FS_OP_WRITE         = 0x05,
    FS_OP_CREATE        = 0x06,
    FS_OP_MKDIR         = 0x07,
    FS_OP_UNLINK        = 0x08,
    FS_OP_RMDIR         = 0x09,
    FS_OP_RENAME        = 0x0A,
    FS_OP_TRUNCATE      = 0x0B,
    FS_OP_UTIMENS       = 0x0C,
    FS_OP_GETXATTR      = 0x0D,
    FS_OP_SETXATTR      = 0x0E,
    FS_OP_LISTXATTR     = 0x0F,
    FS_OP_RELEASE       = 0x10,
    FS_OP_FSYNC         = 0x11,
    FS_OP_ACCESS        = 0x12,
    FS_OP_STATFS        = 0x13,
    FS_OP_FALLOCATE     = 0x14,
    FS_OP_COPY_FILE_RANGE = 0x15,
    FS_OP_LSEEK         = 0x16,
} fs_op_id;

/// @brief Флаги файловых операций
typedef union fs_op_flags_t {
    struct fs_op_flags_bits {
        uint32_t async               : 1;  // Асинхронная операция
        uint32_t direct_io           : 1;  // Прямой ввод-вывод
        uint32_t keep_cache          : 1;  // Кэширование
        uint32_t nonseekable         : 1;  // Не поддерживает seek
        uint32_t atomic_o_trunc      : 1;  // Атомарное truncate при открытии
        uint32_t noflush             : 1;  // Не сбрасывать кэш
        uint32_t reserved            : 26;
    } flags;
    uint32_t bits;
} fs_op_flags;

/// @brief Статус выполнения операции
typedef union fs_op_status_t {
    struct fs_op_status_bits {
        uint32_t success             : 1;  // Операция успешна
        uint32_t io_error            : 1;  // Ошибка ввода-вывода
        uint32_t no_entry            : 1;  // Файл/директория не найден
        uint32_t no_perm             : 1;  // Нет прав доступа
        uint32_t bad_fd              : 1;  // Неверный файловый дескриптор
        uint32_t not_dir             : 1;  // Не директория
        uint32_t is_dir              : 1;  // Является директорией
        uint32_t invalid_arg         : 1;  // Неверный аргумент
        uint32_t out_of_space        : 1;  // Недостаточно места
        uint32_t would_block         : 1;  // Операция блокировала бы
        uint32_t interrupted         : 1;  // Операция прервана
        uint32_t not_supported       : 1;  // Не поддерживается
        uint32_t reserved            : 20;
    } flags;
    uint32_t bits;
} fs_op_status;

// =================================== Операция GETATTR ===================================

#define FS_GETATTR_PACKET   0x9001

/// @brief Запрос получения атрибутов файла/директории
typedef struct fs_getattr_req_t {
    uint64_t            op_id;          // Идентификатор операции (FS_OP_GETATTR)
    fs_op_flags         flags;          // Флаги операции
    uint32_t            path_len;       // Длина пути
    char                path[1];        // Путь к файлу (variable length)
} fs_getattr_req;

/// @brief Ответ с атрибутами файла
typedef union fs_getattr_resp_flags_t {
    struct fs_getattr_resp_flags_bits {
        uint32_t is_file               : 1;  // Является обычным файлом
        uint32_t is_dir                : 1;  // Является директорией
        uint32_t is_symlink            : 1;  // Является симлинком
        uint32_t is_block_dev          : 1;  // Является блочным устройством
        uint32_t is_char_dev           : 1;  // Является символьным устройством
        uint32_t is_fifo               : 1;  // Является FIFO
        uint32_t is_socket             : 1;  // Является сокетом
        uint32_t read_owner            : 1;  // Право чтения для владельца
        uint32_t write_owner           : 1;  // Право записи для владельца
        uint32_t exec_owner            : 1;  // Право выполнения для владельца
        uint32_t read_group            : 1;  // Право чтения для группы
        uint32_t write_group           : 1;  // Право записи для группы
        uint32_t exec_group            : 1;  // Право выполнения для группы
        uint32_t read_other            : 1;  // Право чтения для остальных
        uint32_t write_other           : 1;  // Право записи для остальных
        uint32_t exec_other            : 1;  // Право выполнения для остальных
        uint32_t set_uid               : 1;  // Set-UID бит
        uint32_t set_gid               : 1;  // Set-GID бит
        uint32_t sticky                : 1;  // Sticky бит
        uint32_t has_xattr             : 1;  // Имеет расширенные атрибуты
        uint32_t reserved              : 12;
    } flags;
    uint32_t bits;
} fs_getattr_resp_flags;

/// @brief Ответ на запрос атрибутов
typedef struct fs_getattr_resp_t {
    uint64_t            op_id;          // Идентификатор операции
    fs_op_status        status;         // Статус выполнения
    fs_getattr_resp_flags file_flags;   // Флаги файла
    uint64_t            inode;          // Номер inode
    uint64_t            size;           // Размер файла в байтах
    uint64_t            blocks;         // Количество блоков
    uint32_t            block_size;     // Размер блока
    uint32_t            uid;            // ID владельца
    uint32_t            gid;            // ID группы
    uint64_t            atime;          // Время последнего доступа (сек)
    uint32_t            atime_nsec;     // Время последнего доступа (нсек)
    uint64_t            mtime;          // Время последней модификации (сек)
    uint32_t            mtime_nsec;     // Время последней модификации (нсек)
    uint64_t            ctime;          // Время создания/изменения (сек)
    uint32_t            ctime_nsec;     // Время создания/изменения (нсек)
    uint32_t            nlink;          // Количество жестких ссылок
    uint32_t            rdev;           // Номер устройства (для спец. файлов)
} fs_getattr_resp;

// =================================== Операция READDIR ===================================

#define FS_READDIR_PACKET   0x9002

/// @brief Флаги для элементов директории
typedef union fs_dirent_flags_t {
    struct fs_dirent_flags_bits {
        uint32_t is_file               : 1;
        uint32_t is_dir                : 1;
        uint32_t is_symlink            : 1;
        uint32_t is_block_dev          : 1;
        uint32_t is_char_dev           : 1;
        uint32_t is_fifo               : 1;
        uint32_t is_socket             : 1;
        uint32_t reserved              : 25;
    } flags;
    uint32_t bits;
} fs_dirent_flags;

/// @brief Элемент директории
typedef struct fs_dirent_t {
    uint64_t            inode;          // Номер inode
    uint64_t            offset;         // Смещение для следующего чтения
    fs_dirent_flags     flags;          // Флаги элемента
    uint32_t            name_len;       // Длина имени
    char                name[1];        // Имя элемента (variable length)
} fs_dirent;

/// @brief Запрос на чтение директории
typedef struct fs_readdir_req_t {
    uint64_t            op_id;          // Идентификатор операции
    fs_op_flags         flags;          // Флаги операции
    uint32_t            path_len;       // Длина пути
    uint64_t            fh;             // Файловый дескриптор
    uint64_t            offset;         // Смещение для чтения
    char                path[1];        // Путь к директории
} fs_readdir_req;

/// @brief Ответ с содержимым директории
typedef struct fs_readdir_resp_t {
    uint64_t            op_id;          // Идентификатор операции
    fs_op_status        status;         // Статус выполнения
    uint32_t            dirent_count;   // Количество элементов
    uint32_t            total_size;     // Общий размер данных
    fs_dirent           dirents[1];     // Массив элементов (variable length)
} fs_readdir_resp;

// =================================== Операция READ ===================================

#define FS_READ_PACKET      0x9004

/// @brief Запрос на чтение файла
typedef struct fs_read_req_t {
    uint64_t            op_id;          // Идентификатор операции
    fs_op_flags         flags;          // Флаги операции
    uint32_t            path_len;       // Длина пути
    uint64_t            fh;             // Файловый дескриптор
    uint64_t            offset;         // Смещение для чтения
    uint32_t            size;           // Количество байт для чтения
    char                path[1];        // Путь к файлу
} fs_read_req;

/// @brief Ответ с данными файла
typedef struct fs_read_resp_t {
    uint64_t            op_id;          // Идентификатор операции
    fs_op_status        status;         // Статус выполнения
    uint32_t            data_len;       // Длина прочитанных данных
    uint32_t            reserved;       // Выравнивание
    uint8_t             data[1];        // Данные файла (variable length)
} fs_read_resp;

// =================================== Операция WRITE ===================================

#define FS_WRITE_PACKET     0x9005

/// @brief Запрос на запись в файл
typedef struct fs_write_req_t {
    uint64_t            op_id;          // Идентификатор операции
    fs_op_flags         flags;          // Флаги операции
    uint32_t            path_len;       // Длина пути
    uint64_t            fh;             // Файловый дескриптор
    uint64_t            offset;         // Смещение для записи
    uint32_t            data_len;       // Длина данных
    char                path_and_data[1]; // Путь и данные (variable length)
} fs_write_req;

/// @brief Ответ на запись
typedef struct fs_write_resp_t {
    uint64_t            op_id;          // Идентификатор операции
    fs_op_status        status;         // Статус выполнения
    uint32_t            bytes_written;  // Количество записанных байт
    uint64_t            new_size;       // Новый размер файла
} fs_write_resp;

// =================================== Операция CREATE ===================================

#define FS_CREATE_PACKET    0x9006

/// @brief Флаги создания файла
typedef union fs_create_flags_t {
    struct fs_create_flags_bits {
        uint32_t exclusive             : 1;  // O_EXCL - создавать только если не существует
        uint32_t truncate              : 1;  // O_TRUNC - обрезать при открытии
        uint32_t append                : 1;  // O_APPEND - запись в конец
        uint32_t direct                : 1;  // O_DIRECT - прямой ввод-вывод
        uint32_t sync                  : 1;  // O_SYNC - синхронная запись
        uint32_t nofollow              : 1;  // O_NOFOLLOW - не следовать симлинкам
        uint32_t nonblock              : 1;  // O_NONBLOCK - неблокирующий режим
        uint32_t largefile             : 1;  // O_LARGEFILE - поддержка больших файлов
        uint32_t reserved              : 24;
    } flags;
    uint32_t bits;
} fs_create_flags;

/// @brief Запрос на создание файла
typedef struct fs_create_req_t {
    uint64_t            op_id;          // Идентификатор операции
    fs_op_flags         flags;          // Флаги операции
    fs_create_flags     create_flags;   // Флаги создания
    uint32_t            path_len;       // Длина пути
    uint32_t            mode;           // Режим доступа (permissions)
    char                path[1];        // Путь к создаваемому файлу
} fs_create_req;

/// @brief Ответ на создание файла
typedef struct fs_create_resp_t {
    uint64_t            op_id;          // Идентификатор операции
    fs_op_status        status;         // Статус выполнения
    uint64_t            fh;             // Файловый дескриптор
    uint64_t            inode;          // Номер inode созданного файла
} fs_create_resp;

// =================================== Операция MKDIR ===================================

#define FS_MKDIR_PACKET     0x9007

/// @brief Запрос на создание директории
typedef struct fs_mkdir_req_t {
    uint64_t            op_id;          // Идентификатор операции
    fs_op_flags         flags;          // Флаги операции
    uint32_t            path_len;       // Длина пути
    uint32_t            mode;           // Режим доступа
    char                path[1];        // Путь к создаваемой директории
} fs_mkdir_req;

/// @brief Ответ на создание директории
typedef struct fs_mkdir_resp_t {
    uint64_t            op_id;          // Идентификатор операции
    fs_op_status        status;         // Статус выполнения
    uint64_t            inode;          // Номер inode созданной директории
} fs_mkdir_resp;

// =================================== Операция UNLINK ===================================

#define FS_UNLINK_PACKET    0x9008

/// @brief Запрос на удаление файла
typedef struct fs_unlink_req_t {
    uint64_t            op_id;          // Идентификатор операции
    fs_op_flags         flags;          // Флаги операции
    uint32_t            path_len;       // Длина пути
    char                path[1];        // Путь к удаляемому файлу
} fs_unlink_req;

/// @brief Ответ на удаление файла
typedef struct fs_unlink_resp_t {
    uint64_t            op_id;          // Идентификатор операции
    fs_op_status        status;         // Статус выполнения
    uint32_t            files_removed;  // Количество удаленных файлов
} fs_unlink_resp;

// =================================== Операция GETXATTR ===================================

#define FS_GETXATTR_PACKET  0x900D

/// @brief Запрос на получение расширенного атрибута
typedef struct fs_getxattr_req_t {
    uint64_t            op_id;          // Идентификатор операции
    fs_op_flags         flags;          // Флаги операции
    uint32_t            path_len;       // Длина пути
    uint32_t            name_len;       // Длина имени атрибута
    char                path_and_name[1]; // Путь и имя атрибута (variable length)
} fs_getxattr_req;

/// @brief Ответ с значением атрибута
typedef struct fs_getxattr_resp_t {
    uint64_t            op_id;          // Идентификатор операции
    fs_op_status        status;         // Статус выполнения
    uint32_t            value_len;      // Длина значения атрибута
    uint32_t            reserved;       // Выравнивание
    char                value[1];       // Значение атрибута (variable length)
} fs_getxattr_resp;

// =================================== Базовый заголовок протокола ===================================

/// @brief Заголовок протокола FUSE операций
typedef struct fs_protocol_header_t {
    uint16_t            magic;          // Магическое число (0xF5F5)
    uint16_t            version;        // Версия протокола (1)
    uint32_t            total_length;   // Общая длина пакета
    uint64_t            timestamp;      // Временная метка (наносекунды)
    uint32_t            sequence;       // Последовательный номер
    uint32_t            operation;      // Идентификатор операции
    uint8_t             reserved[16];   // Зарезервировано
} fs_protocol_header;

// =================================== Утилитарные функции ===================================

/// @brief Кодирование запроса getattr
static inline void fs_getattr_encode(const fs_getattr_req* req, 
                                    void* buffer, 
                                    size_t buffer_size) {
    if (buffer_size < sizeof(fs_getattr_req) + req->path_len) {
        return;
    }
    
    fs_protocol_header* header = (fs_protocol_header*)buffer;
    header->magic = 0xF5F5;
    header->version = 1;
    header->operation = FS_OP_GETATTR;
    header->timestamp = 0; // Заполняется вызывающей стороной
    header->sequence = 0;  // Заполняется вызывающей стороной
    header->total_length = sizeof(fs_protocol_header) + 
                          sizeof(fs_getattr_req) + 
                          req->path_len - 1; // -1 потому что path[1] уже включен
    
    fs_getattr_req* req_out = (fs_getattr_req*)(header + 1);
    memcpy(req_out, req, sizeof(fs_getattr_req));
    memcpy(req_out->path, req->path, req->path_len);
}

/// @brief Декодирование запроса getattr
static inline void fs_getattr_decode(const void* buffer, 
                                    fs_getattr_req* out_req) {
    const fs_protocol_header* header = (const fs_protocol_header*)buffer;
    const fs_getattr_req* req_in = (const fs_getattr_req*)(header + 1);
    
    out_req->op_id = req_in->op_id;
    out_req->flags = req_in->flags;
    out_req->path_len = req_in->path_len;
    
    // Копируем путь (вызывающая сторона должна выделить достаточно памяти)
    memcpy(out_req->path, req_in->path, req_in->path_len);
}

/// @brief Кодирование ответа getattr
static inline void fs_getattr_resp_encode(const fs_getattr_resp* resp,
                                         void* buffer,
                                         size_t buffer_size) {
    fs_protocol_header* header = (fs_protocol_header*)buffer;
    header->magic = 0xF5F5;
    header->version = 1;
    header->operation = FS_OP_GETATTR;
    header->total_length = sizeof(fs_protocol_header) + sizeof(fs_getattr_resp);
    
    fs_getattr_resp* resp_out = (fs_getattr_resp*)(header + 1);
    memcpy(resp_out, resp, sizeof(fs_getattr_resp));
}

// =================================== Вспомогательные макросы ===================================

/// @brief Получение типа операции из заголовка
#define FS_GET_OPERATION(header_ptr) ((header_ptr)->operation)

/// @brief Проверка магического числа
#define FS_CHECK_MAGIC(header_ptr) ((header_ptr)->magic == 0xF5F5)

/// @brief Получение указателя на данные операции
#define FS_GET_OPERATION_DATA(header_ptr, type) \
    ((type*)((uint8_t*)(header_ptr) + sizeof(fs_protocol_header)))

/// @brief Расчет размера пакета для операции с переменными данными
#define FS_CALC_PACKET_SIZE(fixed_size, var_len) \
    (sizeof(fs_protocol_header) + (fixed_size) + (var_len))

#pragma pack(pop)

#endif // OWL_VFS_FS_PROTOCOL