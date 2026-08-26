#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile BOOL running = TRUE;

#define KEY_QUEUE_SIZE 512
#define ECHO_MAX_AGE_MS 250

static BOOL baud_supported(DWORD baud)
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
        return TRUE;
    default:
        return FALSE;
    }
}

typedef struct key_queue {
    BYTE value[KEY_QUEUE_SIZE];
    unsigned head;
    unsigned count;
} KEY_QUEUE;

static BOOL key_push(KEY_QUEUE *queue, BYTE value)
{
    unsigned tail;
    if (queue->count == KEY_QUEUE_SIZE)
        return FALSE;
    tail = (queue->head + queue->count) % KEY_QUEUE_SIZE;
    queue->value[tail] = value;
    ++queue->count;
    return TRUE;
}

static BYTE key_pop(KEY_QUEUE *queue)
{
    BYTE value = queue->value[queue->head];
    queue->head = (queue->head + 1) % KEY_QUEUE_SIZE;
    --queue->count;
    return value;
}

static BOOL WINAPI stop_handler(DWORD event)
{
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT ||
        event == CTRL_CLOSE_EVENT || event == CTRL_LOGOFF_EVENT ||
        event == CTRL_SHUTDOWN_EVENT) {
        running = FALSE;
        return TRUE;
    }
    return FALSE;
}

static void show_received(BYTE value, BOOL *after_cr)
{
    if (value == '\r') {
        fputs("\r\n", stdout);
        *after_cr = TRUE;
    } else if (value == '\n') {
        if (!*after_cr)
            fputs("\r\n", stdout);
        *after_cr = FALSE;
    } else {
        *after_cr = FALSE;
        if (value == 8 || value == 127)
            fputs("\b \b", stdout);
        else if (value == '\t' || (value >= 32 && value <= 126))
            putchar(value);
    }
    fflush(stdout);
}

static void show_local_key(BYTE value)
{
    if (value == 8 || value == 127)
        fputs("\b \b", stdout);
    else if (value == '\t' || (value >= 32 && value <= 126))
        putchar(value);
    fflush(stdout);
}

static BOOL feed_password_prompt(BYTE value, unsigned *matched)
{
    static const char marker[] = "PASSWORD (HIDDEN): ";
    if (value == (BYTE)marker[*matched]) {
        ++*matched;
        if (marker[*matched] == '\0') {
            *matched = 0;
            return TRUE;
        }
    } else {
        *matched = value == (BYTE)marker[0] ? 1 : 0;
    }
    return FALSE;
}

static void feed_connection_status(BYTE value, char *line,
                                   unsigned *line_length,
                                   BOOL *remote_connected)
{
    if (value == '\r' || value == '\n') {
        if (*line_length != 0) {
            line[*line_length] = '\0';
            if (strcmp(line, "REMOTE CONNECTED") == 0)
                *remote_connected = TRUE;
            else if (strcmp(line, "REMOTE DISCONNECTED") == 0)
                *remote_connected = FALSE;
            *line_length = 0;
        }
    } else if (value >= 32 && value <= 126) {
        if (*line_length < 31)
            line[(*line_length)++] = (char)value;
        else
            *line_length = 0;
    } else {
        *line_length = 0;
    }
}

int main(int argc, char **argv)
{
    HANDLE port;
    DCB saved_dcb, active_dcb;
    COMMTIMEOUTS saved_timeouts, active_timeouts;
    BOOL have_dcb = FALSE, have_timeouts = FALSE, after_cr = FALSE;
    KEY_QUEUE key_queue;
    BOOL reflection_pending = FALSE;
    BYTE expected_reflection = 0;
    DWORD reflection_tick = 0;
    DWORD error_code = 0;
    DWORD baud = 300;
    BOOL hide_local_input = FALSE;
    BOOL remote_connected = FALSE;
    unsigned password_prompt_match = 0;
    char status_line[32];
    unsigned status_line_length = 0;

    if (argc > 2) {
        puts("Usage: W98TERM [baud]");
        return 2;
    }
    if (argc == 2) {
        char *end = NULL;
        baud = strtoul(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' || !baud_supported(baud)) {
            puts("Unsupported baud. Use 300, 1200, 2400, 4800, 9600,");
            puts("19200, 38400, 57600, or 115200.");
            return 2;
        }
    }

    ZeroMemory(&key_queue, sizeof(key_queue));
    SetConsoleCtrlHandler(stop_handler, TRUE);
    port = CreateFileA("COM1", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, 0, NULL);
    if (port == INVALID_HANDLE_VALUE) {
        printf("W98TERM ERROR: cannot open COM1 (Windows error %lu).\r\n",
               GetLastError());
        puts("Close any program using COM1, then run W98TERM again.");
        return 1;
    }

    ZeroMemory(&saved_dcb, sizeof(saved_dcb));
    saved_dcb.DCBlength = sizeof(saved_dcb);
    if (GetCommState(port, &saved_dcb)) {
        have_dcb = TRUE;
        active_dcb = saved_dcb;
    } else {
        ZeroMemory(&active_dcb, sizeof(active_dcb));
        active_dcb.DCBlength = sizeof(active_dcb);
    }

    active_dcb.BaudRate = baud;
    active_dcb.fBinary = TRUE;
    active_dcb.fParity = FALSE;
    active_dcb.fOutxCtsFlow = FALSE;
    active_dcb.fOutxDsrFlow = FALSE;
    active_dcb.fDtrControl = DTR_CONTROL_DISABLE;
    active_dcb.fDsrSensitivity = FALSE;
    active_dcb.fTXContinueOnXoff = TRUE;
    active_dcb.fOutX = FALSE;
    active_dcb.fInX = FALSE;
    active_dcb.fErrorChar = FALSE;
    active_dcb.fNull = FALSE;
    active_dcb.fRtsControl = RTS_CONTROL_DISABLE;
    active_dcb.fAbortOnError = FALSE;
    active_dcb.ByteSize = 8;
    active_dcb.Parity = NOPARITY;
    active_dcb.StopBits = ONESTOPBIT;

    if (!SetCommState(port, &active_dcb)) {
        error_code = GetLastError();
        printf("W98TERM ERROR: cannot configure COM1 (Windows error %lu).\r\n",
               error_code);
        CloseHandle(port);
        return 1;
    }

    if (GetCommTimeouts(port, &saved_timeouts))
        have_timeouts = TRUE;
    ZeroMemory(&active_timeouts, sizeof(active_timeouts));
    active_timeouts.ReadIntervalTimeout = MAXDWORD;
    active_timeouts.WriteTotalTimeoutConstant = 1000;
    active_timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(port, &active_timeouts);
    SetupComm(port, 4096, 4096);
    EscapeCommFunction(port, CLRDTR);
    EscapeCommFunction(port, CLRRTS);
    PurgeComm(port, PURGE_RXABORT | PURGE_RXCLEAR |
                    PURGE_TXABORT | PURGE_TXCLEAR);

    printf("W98TERM V7 READY - COM1 %lu-8-N-1 - DTR/RTS OFF\r\n", baud);
    puts("LOCAL COMMAND DISPLAY ON - PASSWORD DISPLAY OFF");
    puts("PRESS ADAPTER RESET NOW.  PRESS ESC TO EXIT.");
    puts("");

    while (running) {
        BYTE incoming[64];
        DWORD received = 0, i;

        if (!ReadFile(port, incoming, sizeof(incoming), &received, NULL)) {
            error_code = GetLastError();
            printf("\r\nW98TERM ERROR: COM1 read failed (Windows error %lu).\r\n",
                   error_code);
            break;
        }
        for (i = 0; i < received; ++i) {
            if (reflection_pending && incoming[i] == expected_reflection &&
                (DWORD)(GetTickCount() - reflection_tick) <= ECHO_MAX_AGE_MS) {
                reflection_pending = FALSE;
            } else {
                show_received(incoming[i], &after_cr);
                feed_connection_status(incoming[i], status_line,
                                       &status_line_length,
                                       &remote_connected);
                if (feed_password_prompt(incoming[i],
                                         &password_prompt_match))
                    hide_local_input = TRUE;
            }
        }

        while (_kbhit()) {
            int key = _getch();
            BYTE outgoing;

            if (key == 0 || key == 0xE0) {
                if (_kbhit())
                    (void)_getch();
                continue;
            }
            if (key == 27) {
                running = FALSE;
                break;
            }
            if (key == 3) {
                running = FALSE;
                break;
            }
            outgoing = (key == '\r') ? '\r' : (BYTE)key;
            if (!key_push(&key_queue, outgoing)) {
                fputs("\r\nW98TERM ERROR: keyboard queue full.\r\n", stdout);
                running = FALSE;
                break;
            }
        }

        if (reflection_pending &&
            (DWORD)(GetTickCount() - reflection_tick) > ECHO_MAX_AGE_MS)
            reflection_pending = FALSE;

        if (running && !reflection_pending && key_queue.count) {
            BYTE outgoing = key_pop(&key_queue);
            DWORD written = 0;
            if (!WriteFile(port, &outgoing, 1, &written, NULL) || written != 1) {
                error_code = GetLastError();
                printf("\r\nW98TERM ERROR: COM1 write failed (Windows error %lu).\r\n",
                       error_code);
                running = FALSE;
            } else {
                expected_reflection = outgoing;
                reflection_tick = GetTickCount();
                reflection_pending = TRUE;
                if (outgoing == '\r') {
                    if (remote_connected && !hide_local_input) {
                        fputs("\r\n", stdout);
                        fflush(stdout);
                        after_cr = TRUE;
                    }
                    hide_local_input = FALSE;
                } else if (!hide_local_input)
                    show_local_key(outgoing);
            }
        }
        Sleep(5);
    }

    if (have_dcb)
        SetCommState(port, &saved_dcb);
    if (have_timeouts)
        SetCommTimeouts(port, &saved_timeouts);
    CloseHandle(port);
    SetConsoleCtrlHandler(stop_handler, FALSE);
    puts("\r\nW98TERM CLOSED - COM1 SETTINGS RESTORED.");
    return error_code ? 1 : 0;
}
