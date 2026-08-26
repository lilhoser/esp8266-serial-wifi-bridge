#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define LINE_CAPACITY 1024
#define IO_CHUNK 256
#define EXEC_TIMEOUT_MS 60000UL
#define FRAME_RETRIES 10
#define FRAME_TIMEOUT_MS 5000UL
#define FRAME_MAGIC_0 'W'
#define FRAME_MAGIC_1 '9'
#define FRAME_MAGIC_2 'F'
#define FRAME_MAGIC_3 '2'
#define FRAME_DATA 'D'
#define FRAME_ACK 'A'
#define FRAME_NAK 'N'
#define FRAME_FIN 'F'
#define FRAME_CONFIRM 'C'
#define CANCEL_BYTE 0x18

static HANDLE serial_port = INVALID_HANDLE_VALUE;
static DWORD active_baud = 115200;
static int reflection_mode = -1;
static int pending_valid = 0;
static BYTE pending_value = 0;

static int baud_supported(DWORD baud)
{
    switch (baud) {
    case 300:
    case 1200:
    case 2400:
    case 4800:
    case 9600:
    case 19200:
    case 38400:
    case 57600:
    case 115200:
        return 1;
    default:
        return 0;
    }
}

static DWORD crc32_update(DWORD crc, const BYTE *data, DWORD length)
{
    DWORD i;
    while (length-- != 0) {
        crc ^= *data++;
        for (i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320UL : 0);
    }
    return crc;
}

static void put_u16(BYTE *p, WORD value)
{
    p[0] = (BYTE)(value & 0xFF);
    p[1] = (BYTE)((value >> 8) & 0xFF);
}

static WORD get_u16(const BYTE *p)
{
    return (WORD)((WORD)p[0] | ((WORD)p[1] << 8));
}

static void put_u32(BYTE *p, DWORD value)
{
    p[0] = (BYTE)(value & 0xFF);
    p[1] = (BYTE)((value >> 8) & 0xFF);
    p[2] = (BYTE)((value >> 16) & 0xFF);
    p[3] = (BYTE)((value >> 24) & 0xFF);
}

static DWORD get_u32(const BYTE *p)
{
    return (DWORD)p[0] | ((DWORD)p[1] << 8) |
           ((DWORD)p[2] << 16) | ((DWORD)p[3] << 24);
}

static int serial_read_byte(BYTE *value)
{
    DWORD received = 0;
    if (pending_valid) {
        *value = pending_value;
        pending_valid = 0;
        return 1;
    }
    if (!ReadFile(serial_port, value, 1, &received, NULL)) return -1;
    return received == 1 ? 1 : 0;
}

static int consume_reflection(const BYTE *expected, DWORD length)
{
    DWORD matched = 0;
    int exact = 1;
    DWORD allowance = 1000UL +
        (DWORD)(((unsigned long long)length * 10000ULL) / active_baud);
    DWORD deadline = GetTickCount() + allowance;
    while (matched < length &&
           (LONG)(deadline - GetTickCount()) > 0) {
        BYTE value;
        int result = serial_read_byte(&value);
        if (result < 0) return 0;
        if (result == 0) {
            Sleep(2);
            continue;
        }
        if (value != expected[matched]) {
            if (reflection_mode < 0) {
                pending_value = value;
                pending_valid = 1;
                reflection_mode = 0;
                return 1;
            }
            exact = 0;
        }
        ++matched;
    }
    if (matched == length) {
        reflection_mode = 1;
        (void)exact;
        return 1;
    }
    if (reflection_mode < 0) {
        reflection_mode = 0;
        return 1;
    }
    /* Reflection is only local display noise.  A damaged/missing reflected
       copy does not mean the peer failed to receive the transmitted bytes. */
    return 1;
}

static int serial_write_all(const void *buffer, DWORD length)
{
    const BYTE *data = (const BYTE *)buffer;
    const BYTE *start = data;
    DWORD total = length;
    while (length != 0) {
        DWORD written = 0;
        if (!WriteFile(serial_port, data, length, &written, NULL) || written == 0)
            return 0;
        data += written;
        length -= written;
    }
    if (reflection_mode == 0) return 1;
    if (!consume_reflection(start, total)) return 0;
    return 1;
}

static int send_line(const char *format, ...)
{
    char buffer[LINE_CAPACITY + 4];
    int length;
    va_list args;
    va_start(args, format);
    length = _vsnprintf(buffer, LINE_CAPACITY, format, args);
    va_end(args);
    if (length < 0 || length > LINE_CAPACITY) return 0;
    buffer[length++] = '\r';
    buffer[length++] = '\n';
    return serial_write_all(buffer, (DWORD)length);
}

static int read_line_timeout(char *buffer, DWORD capacity, DWORD timeout_ms)
{
    DWORD used = 0;
    DWORD deadline = GetTickCount() + timeout_ms;
    for (;;) {
        BYTE value;
        DWORD received = 0;
        int result = serial_read_byte(&value);
        if (result < 0) return 0;
        if (result == 0) {
            if (timeout_ms != 0 &&
                (LONG)(deadline - GetTickCount()) <= 0) return 0;
            Sleep(5);
            continue;
        }
        if (value == '\n') continue;
        if (value == '\r') {
            buffer[used] = '\0';
            return 1;
        }
        if (used + 1 < capacity) buffer[used++] = (char)value;
    }
}

static int read_line(char *buffer, DWORD capacity)
{
    return read_line_timeout(buffer, capacity, 0);
}

static int read_bytes_timeout(BYTE *buffer, DWORD length, DWORD timeout_ms)
{
    DWORD have = 0;
    DWORD deadline = GetTickCount() + timeout_ms;
    while (have < length && (LONG)(deadline - GetTickCount()) > 0) {
        int result = serial_read_byte(buffer + have);
        if (result < 0) return 0;
        if (result == 0) {
            Sleep(1);
            continue;
        }
        ++have;
    }
    return have == length;
}

static int send_frame_once(BYTE type, DWORD sequence,
                           const BYTE *payload, WORD length)
{
    BYTE wire[15 + IO_CHUNK];
    DWORD crc;
    if (length > IO_CHUNK) return 0;
    wire[0] = FRAME_MAGIC_0;
    wire[1] = FRAME_MAGIC_1;
    wire[2] = FRAME_MAGIC_2;
    wire[3] = FRAME_MAGIC_3;
    wire[4] = type;
    put_u32(wire + 5, sequence);
    put_u16(wire + 9, length);
    crc = crc32_update(0xFFFFFFFFUL, wire + 4, 7);
    if (length != 0) crc = crc32_update(crc, payload, length);
    crc ^= 0xFFFFFFFFUL;
    put_u32(wire + 11, crc);
    if (length != 0) memcpy(wire + 15, payload, length);
    return serial_write_all(wire, 15UL + length);
}

/* Returns 1 for a valid frame, -1 for timeout/I/O, and -2 for bad framing. */
static int read_frame(BYTE *type, DWORD *sequence, BYTE *payload,
                      WORD *length, DWORD timeout_ms)
{
    static const BYTE magic[4] = {
        FRAME_MAGIC_0, FRAME_MAGIC_1, FRAME_MAGIC_2, FRAME_MAGIC_3
    };
    BYTE header[11];
    DWORD deadline = GetTickCount() + timeout_ms;
    int matched = 0;
    while ((LONG)(deadline - GetTickCount()) > 0) {
        BYTE value;
        int result = serial_read_byte(&value);
        if (result < 0) return -1;
        if (result == 0) {
            Sleep(1);
            continue;
        }
        if (value == CANCEL_BYTE) return -3;
        if (value == magic[matched]) {
            ++matched;
            if (matched == 4) break;
        } else {
            matched = value == magic[0] ? 1 : 0;
        }
    }
    if (matched != 4) return -1;
    if (!read_bytes_timeout(header, sizeof(header), FRAME_TIMEOUT_MS)) return -1;
    *type = header[0];
    *sequence = get_u32(header + 1);
    *length = get_u16(header + 5);
    if (*length > IO_CHUNK) return -2;
    if (*length != 0 &&
        !read_bytes_timeout(payload, *length, FRAME_TIMEOUT_MS)) return -1;
    {
        DWORD expected = get_u32(header + 7);
        DWORD actual = crc32_update(0xFFFFFFFFUL, header, 7);
        if (*length != 0) actual = crc32_update(actual, payload, *length);
        actual ^= 0xFFFFFFFFUL;
        if (actual != expected) return -2;
    }
    return 1;
}

static int send_ack(BYTE type, DWORD sequence)
{
    return send_frame_once(type, sequence, NULL, 0);
}

static int send_frame_reliable(BYTE type, DWORD sequence,
                               const BYTE *payload, WORD length)
{
    int attempt;
    for (attempt = 0; attempt < FRAME_RETRIES; ++attempt) {
        BYTE reply_type, reply_payload[IO_CHUNK];
        DWORD reply_sequence;
        WORD reply_length;
        int result;
        if (!send_frame_once(type, sequence, payload, length)) return 0;
        result = read_frame(&reply_type, &reply_sequence, reply_payload,
                            &reply_length, FRAME_TIMEOUT_MS);
        if (result == -3) return 0;
        if (result == 1 && reply_sequence == sequence &&
            reply_type == FRAME_ACK) return 1;
        if (result == 1 && reply_sequence == sequence &&
            reply_type == FRAME_NAK) continue;
    }
    return 0;
}

static int receive_frames_to_file(HANDLE file, DWORD total_size,
                                  DWORD expected_crc, DWORD *actual_crc_out)
{
    BYTE type, payload[IO_CHUNK];
    DWORD sequence, expected_sequence = 0;
    DWORD received_total = 0;
    DWORD crc = 0xFFFFFFFFUL;
    WORD length;
    int failures = 0;
    while (received_total < total_size) {
        int result = read_frame(&type, &sequence, payload, &length,
                                FRAME_TIMEOUT_MS);
        if (result == -3) return 0;
        if (result != 1) {
            send_ack(FRAME_NAK, expected_sequence);
            if (++failures >= FRAME_RETRIES) return 0;
            continue;
        }
        if (type == FRAME_DATA && sequence == expected_sequence &&
            length != 0 && length <= total_size - received_total) {
            DWORD written = 0;
            if (!WriteFile(file, payload, length, &written, NULL) ||
                written != length) return 0;
            crc = crc32_update(crc, payload, length);
            received_total += length;
            ++expected_sequence;
            failures = 0;
            if (!send_ack(FRAME_ACK, sequence)) return 0;
        } else if (type == FRAME_DATA && expected_sequence != 0 &&
                   sequence == expected_sequence - 1) {
            if (!send_ack(FRAME_ACK, sequence)) return 0;
        } else {
            send_ack(FRAME_NAK, expected_sequence);
        }
    }
    crc ^= 0xFFFFFFFFUL;
    *actual_crc_out = crc;
    for (failures = 0; failures < FRAME_RETRIES; ++failures) {
        int result = read_frame(&type, &sequence, payload, &length,
                                FRAME_TIMEOUT_MS);
        if (result == -3) return 0;
        if (result != 1) continue;
        if (type == FRAME_FIN && sequence == expected_sequence && length == 8 &&
            get_u32(payload) == total_size && get_u32(payload + 4) == crc &&
            crc == expected_crc) {
            DWORD finish_deadline;
            if (!send_ack(FRAME_ACK, sequence)) return 0;
            finish_deadline = GetTickCount() + 2000UL;
            while ((LONG)(finish_deadline - GetTickCount()) > 0) {
                int finish = read_frame(&type, &sequence, payload, &length, 250UL);
                if (finish == 1 && type == FRAME_CONFIRM &&
                    sequence == expected_sequence) return 1;
                if (finish == 1 && type == FRAME_FIN &&
                    sequence == expected_sequence)
                    send_ack(FRAME_ACK, sequence);
            }
            return 1;
        }
        send_ack(FRAME_NAK, expected_sequence);
    }
    return 0;
}

static int file_crc_and_size(HANDLE file, DWORD *size_out, DWORD *crc_out)
{
    BYTE buffer[IO_CHUNK];
    DWORD crc = 0xFFFFFFFFUL;
    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) return 0;
    if (SetFilePointer(file, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER &&
        GetLastError() != NO_ERROR) return 0;
    for (;;) {
        DWORD received = 0;
        if (!ReadFile(file, buffer, sizeof(buffer), &received, NULL)) return 0;
        if (received == 0) break;
        crc = crc32_update(crc, buffer, received);
    }
    if (SetFilePointer(file, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER &&
        GetLastError() != NO_ERROR) return 0;
    *size_out = size;
    *crc_out = crc ^ 0xFFFFFFFFUL;
    return 1;
}

static int wait_ready(void)
{
    char line[64];
    return read_line_timeout(line, sizeof(line), 15000UL) &&
           strcmp(line, "READY") == 0;
}

static int cancel_requested(void)
{
    BYTE value;
    int result = serial_read_byte(&value);
    if (result <= 0) return 0;
    return value == CANCEL_BYTE;
}

static int is_cancel_line(const char *line)
{
    const BYTE *p = (const BYTE *)line;
    if (*p == '\0') return 0;
    while (*p == CANCEL_BYTE) ++p;
    return *p == '\0';
}

static int send_file_handle(HANDLE file, const char *kind, DWORD result_code)
{
    BYTE buffer[IO_CHUNK];
    BYTE finish[8];
    DWORD size, crc, sequence = 0;
    if (!file_crc_and_size(file, &size, &crc)) return send_line("ERR\tFILE_READ\t%lu", GetLastError());
    if (!send_line("%s\t%lu\t%08lX\t%lu", kind, size, crc, result_code)) return 0;
    if (!wait_ready()) return 0;
    for (;;) {
        DWORD received = 0;
        if (!ReadFile(file, buffer, sizeof(buffer), &received, NULL)) return 0;
        if (received == 0) break;
        if (!send_frame_reliable(FRAME_DATA, sequence, buffer,
                                 (WORD)received)) return 0;
        ++sequence;
    }
    put_u32(finish, size);
    put_u32(finish + 4, crc);
    if (!send_frame_reliable(FRAME_FIN, sequence, finish, sizeof(finish)))
        return 0;
    return send_frame_once(FRAME_CONFIRM, sequence, NULL, 0);
}

static void handle_get(const char *path)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        send_line("ERR\tGET\t%lu", GetLastError());
        return;
    }
    send_file_handle(file, "FILE", 0);
    CloseHandle(file);
}

static void handle_put(char *arguments)
{
    char *size_text = arguments;
    char *crc_text = strchr(size_text, '\t');
    char *path;
    DWORD size, expected_crc, actual_crc = 0;
    HANDLE file;
    if (crc_text == NULL) { send_line("ERR\tPUT_SYNTAX"); return; }
    *crc_text++ = '\0';
    path = strchr(crc_text, '\t');
    if (path == NULL) { send_line("ERR\tPUT_SYNTAX"); return; }
    *path++ = '\0';
    size = strtoul(size_text, NULL, 10);
    expected_crc = strtoul(crc_text, NULL, 16);
    file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        send_line("ERR\tPUT_OPEN\t%lu", GetLastError());
        return;
    }
    if (!send_line("READY") ||
        !receive_frames_to_file(file, size, expected_crc, &actual_crc)) {
        CloseHandle(file);
        DeleteFileA(path);
        send_line("ERR\tPUT_IO\t%lu", GetLastError());
        return;
    }
    CloseHandle(file);
    if (actual_crc != expected_crc) {
        DeleteFileA(path);
        send_line("ERR\tPUT_CRC\t%08lX", actual_crc);
        return;
    }
    send_line("STORED\t%lu\t%08lX", size, actual_crc);
}

static void handle_exec(const char *command)
{
    char temp_dir[MAX_PATH], temp_file[MAX_PATH];
    char command_line[LINE_CAPACITY + 32];
    SECURITY_ATTRIBUTES security;
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    HANDLE output, input;
    DWORD wait_result, exit_code = 0;

    if (GetTempPathA(sizeof(temp_dir), temp_dir) == 0 ||
        GetTempFileNameA(temp_dir, "W98", 0, temp_file) == 0) {
        send_line("ERR\tTEMP\t%lu", GetLastError());
        return;
    }
    security.nLength = sizeof(security);
    security.lpSecurityDescriptor = NULL;
    security.bInheritHandle = TRUE;
    output = CreateFileA(temp_file, GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                         CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (output == INVALID_HANDLE_VALUE) {
        DeleteFileA(temp_file);
        send_line("ERR\tTEMP_OPEN\t%lu", GetLastError());
        return;
    }
    input = CreateFileA("NUL", GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                        OPEN_EXISTING, 0, NULL);
    if (input == INVALID_HANDLE_VALUE) {
        CloseHandle(output);
        DeleteFileA(temp_file);
        send_line("ERR\tNUL_OPEN\t%lu", GetLastError());
        return;
    }
    _snprintf(command_line, sizeof(command_line) - 1, "COMMAND.COM /C %s", command);
    command_line[sizeof(command_line) - 1] = '\0';
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdInput = input;
    startup.hStdOutput = output;
    startup.hStdError = output;
    ZeroMemory(&process, sizeof(process));
    if (!CreateProcessA(NULL, command_line, NULL, NULL, TRUE, 0, NULL, NULL,
                        &startup, &process)) {
        DWORD error = GetLastError();
        CloseHandle(input);
        CloseHandle(output);
        DeleteFileA(temp_file);
        send_line("ERR\tEXEC_START\t%lu", error);
        return;
    }
    CloseHandle(input);
    wait_result = WaitForSingleObject(process.hProcess, EXEC_TIMEOUT_MS);
    if (wait_result != WAIT_OBJECT_0) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandle(output);
        DeleteFileA(temp_file);
        send_line("ERR\tEXEC_TIMEOUT");
        return;
    }
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    FlushFileBuffers(output);
    send_file_handle(output, "OUTPUT", exit_code);
    CloseHandle(output);
    DeleteFileA(temp_file);
}

static int open_serial(void)
{
    DCB dcb;
    COMMTIMEOUTS timeouts;
    serial_port = CreateFileA("COM1", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                              OPEN_EXISTING, 0, NULL);
    if (serial_port == INVALID_HANDLE_VALUE) return 0;
    ZeroMemory(&dcb, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(serial_port, &dcb)) return 0;
    dcb.BaudRate = active_baud;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = TRUE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fAbortOnError = FALSE;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    if (!SetCommState(serial_port, &dcb)) return 0;
    ZeroMemory(&timeouts, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutConstant = 5000;
    timeouts.WriteTotalTimeoutMultiplier = 2;
    if (!SetCommTimeouts(serial_port, &timeouts)) return 0;
    SetupComm(serial_port, 8192, 8192);
    EscapeCommFunction(serial_port, CLRDTR);
    EscapeCommFunction(serial_port, CLRRTS);
    PurgeComm(serial_port, PURGE_RXABORT | PURGE_RXCLEAR |
                            PURGE_TXABORT | PURGE_TXCLEAR);
    return 1;
}

int main(int argc, char **argv)
{
    char line[LINE_CAPACITY];
    char ready[64];
    if (argc == 2 && strcmp(argv[1], "--selftest") == 0) {
        static const BYTE test[] = "123456789";
        DWORD crc = crc32_update(0xFFFFFFFFUL, test, 9) ^ 0xFFFFFFFFUL;
        printf("CRC32=%08lX\n", crc);
        return crc == 0xCBF43926UL ? 0 : 1;
    }
    if (argc > 2) {
        puts("Usage: W98AGENT [baud]");
        return 2;
    }
    if (argc == 2) {
        char *end = NULL;
        active_baud = strtoul(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' ||
            !baud_supported(active_baud)) {
            puts("Unsupported baud. Use 300, 1200, 2400, 4800, 9600,");
            puts("19200, 38400, 57600, or 115200.");
            return 2;
        }
    }
    printf("W98AGENT V9 - COM1 %lu-8-N-1\n", active_baud);
    puts("Close this window to stop the agent.");
    if (!open_serial()) {
        printf("Cannot open/configure COM1 (Windows error %lu).\n", GetLastError());
        return 1;
    }
    sprintf(ready, "READY\tW98SER/2\t%lu", active_baud);
    send_line(ready);
    while (read_line(line, sizeof(line))) {
        if (is_cancel_line(line)) {
            /* Accepted only for compatibility with an earlier V5 client. */
        } else if (strcmp(line, "HELLO") == 0 || strcmp(line, "PING") == 0) {
            send_line(ready);
        } else if (strncmp(line, "SYNC\t", 5) == 0 && line[5] != '\0') {
            send_line("SYNCED\t%s", line + 5);
        } else if (strncmp(line, "EXEC\t", 5) == 0) {
            handle_exec(line + 5);
        } else if (strncmp(line, "GET\t", 4) == 0) {
            handle_get(line + 4);
        } else if (strncmp(line, "PUT\t", 4) == 0) {
            handle_put(line + 4);
        } else if (strcmp(line, "BYE") == 0) {
            send_line("BYE");
            break;
        } else if (line[0] != '\0') {
            send_line("ERR\tUNKNOWN_COMMAND");
        }
    }
    CloseHandle(serial_port);
    return 0;
}
