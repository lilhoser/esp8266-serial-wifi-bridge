#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROUNDS 20
#define RESPONSE_TIMEOUT_MS 10000UL

static const BYTE command[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-\r";
static const BYTE reply[] =
    "\r\nERROR - TYPE HELP\r\nSERIALWIFI> ";

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

static int read_response(HANDLE port, BYTE *buffer, DWORD capacity,
                         DWORD timeout_ms, DWORD *actual)
{
    DWORD started = GetTickCount();
    *actual = 0;
    while (*actual < capacity &&
           (DWORD)(GetTickCount() - started) < timeout_ms) {
        DWORD received = 0;
        if (!ReadFile(port, buffer + *actual, capacity - *actual,
                      &received, NULL)) return 0;
        *actual += received;
        if (received == 0) Sleep(2);
    }
    return 1;
}

static void log_bytes(FILE *log, const BYTE *bytes, DWORD length)
{
    DWORD i;
    for (i = 0; i < length; ++i) fprintf(log, " %02X", bytes[i]);
    fputc('\n', log);
}

int main(int argc, char **argv)
{
    HANDLE port;
    DCB saved_dcb, dcb;
    COMMTIMEOUTS saved_timeouts, timeouts;
    BOOL have_dcb = FALSE, have_timeouts = FALSE;
    FILE *log;
    BYTE expected[sizeof(command) - 1 + sizeof(reply) - 1];
    BYTE received[sizeof(expected)];
    DWORD expected_length = (DWORD)sizeof(expected);
    int round, passed = 0;
    DWORD baud = 300;

    if (argc > 2) {
        puts("Usage: LINEPASS [baud]");
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

    memcpy(expected, command, sizeof(command) - 1);
    memcpy(expected + sizeof(command) - 1, reply, sizeof(reply) - 1);

    log = fopen("C:\\LINEPASS.TXT", "wt");
    if (log == NULL) {
        puts("LINEPASS ERROR: cannot create C:\\LINEPASS.TXT");
        return 1;
    }
    port = CreateFileA("COM1", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, 0, NULL);
    if (port == INVALID_HANDLE_VALUE) {
        fprintf(log, "ERROR OPEN_COM1 %lu\n", GetLastError());
        fclose(log);
        puts("LINEPASS ERROR: cannot open COM1");
        return 1;
    }

    ZeroMemory(&saved_dcb, sizeof(saved_dcb));
    saved_dcb.DCBlength = sizeof(saved_dcb);
    if (GetCommState(port, &saved_dcb)) {
        have_dcb = TRUE;
        dcb = saved_dcb;
    } else {
        ZeroMemory(&dcb, sizeof(dcb));
        dcb.DCBlength = sizeof(dcb);
    }
    dcb.BaudRate = baud;
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
    if (!SetCommState(port, &dcb)) {
        fprintf(log, "ERROR SET_COM1 %lu\n", GetLastError());
        CloseHandle(port);
        fclose(log);
        puts("LINEPASS ERROR: cannot configure COM1");
        return 1;
    }
    if (GetCommTimeouts(port, &saved_timeouts)) have_timeouts = TRUE;
    ZeroMemory(&timeouts, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.WriteTotalTimeoutConstant = 3000;
    timeouts.WriteTotalTimeoutMultiplier = 50;
    SetCommTimeouts(port, &timeouts);
    SetupComm(port, 8192, 8192);
    EscapeCommFunction(port, CLRDTR);
    EscapeCommFunction(port, CLRRTS);
    PurgeComm(port, PURGE_RXABORT | PURGE_RXCLEAR |
                    PURGE_TXABORT | PURGE_TXCLEAR);

    fprintf(log, "LINEPASS V1\n");
    fprintf(log, "HOST COM1 %lu 8-N-1 DTR_OFF RTS_OFF FLOW_OFF\n", baud);
    fprintf(log, "ROUNDS %d COMMAND_BYTES %lu RESPONSE_BYTES %lu\n",
            ROUNDS, (DWORD)(sizeof(command) - 1), expected_length);
    fprintf(log, "EXPECTED");
    log_bytes(log, expected, expected_length);
    fflush(log);

    puts("LINEPASS V1 - echo-off firmware acceptance");
    printf("COM1 fixed at %lu-8-N-1; running 20 whole-line rounds.\n", baud);
    puts("The test expects the carrier reflection, then one firmware reply.");
    Sleep(1000);

    for (round = 1; round <= ROUNDS; ++round) {
        DWORD written = 0, actual = 0, errors = 0;
        COMSTAT status;
        DWORD mismatch = expected_length;
        DWORD i;

        PurgeComm(port, PURGE_RXABORT | PURGE_RXCLEAR |
                        PURGE_TXABORT | PURGE_TXCLEAR);
        ClearCommError(port, &errors, &status);
        if (!WriteFile(port, command, sizeof(command) - 1, &written, NULL) ||
            written != sizeof(command) - 1) {
            fprintf(log, "ROUND %d WRITE_FAIL WRITTEN %lu ERROR %lu\n",
                    round, written, GetLastError());
            printf("FAIL: round %d write failed\n", round);
            break;
        }
        if (!read_response(port, received, expected_length,
                           RESPONSE_TIMEOUT_MS, &actual)) {
            fprintf(log, "ROUND %d READ_FAIL ERROR %lu\n",
                    round, GetLastError());
            printf("FAIL: round %d read failed\n", round);
            break;
        }
        ZeroMemory(&status, sizeof(status));
        ClearCommError(port, &errors, &status);
        for (i = 0; i < actual && i < expected_length; ++i) {
            if (received[i] != expected[i]) {
                mismatch = i;
                break;
            }
        }
        if (actual != expected_length || mismatch != expected_length ||
            errors != 0) {
            fprintf(log,
                    "ROUND %d FAIL RECEIVED %lu MISMATCH %lu ERRORS %08lX DATA",
                    round, actual, mismatch, errors);
            log_bytes(log, received, actual);
            printf("FAIL: round %d, received %lu/%lu bytes",
                   round, actual, expected_length);
            if (mismatch != expected_length)
                printf(", mismatch at byte %lu", mismatch + 1);
            if (errors != 0) printf(", UART errors %08lX", errors);
            putchar('\n');
            break;
        }
        ++passed;
        fprintf(log, "ROUND %d PASS ERRORS %08lX\n", round, errors);
        fflush(log);
        printf("Round %d/%d passed\n", round, ROUNDS);
    }

    fprintf(log, "RESULT %s %d/%d\n",
            passed == ROUNDS ? "PASS" : "FAIL", passed, ROUNDS);
    fclose(log);
    if (have_dcb) SetCommState(port, &saved_dcb);
    if (have_timeouts) SetCommTimeouts(port, &saved_timeouts);
    CloseHandle(port);
    printf("%s: %d/%d rounds passed\n",
           passed == ROUNDS ? "PASS" : "FAIL", passed, ROUNDS);
    puts("Log saved to C:\\LINEPASS.TXT");
    puts("Press Enter to close.");
    (void)getchar();
    return passed == ROUNDS ? 0 : 1;
}
