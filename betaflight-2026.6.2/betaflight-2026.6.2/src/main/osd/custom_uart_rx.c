#include "custom_uart_rx.h"
#include "io/serial.h"
#include "drivers/serial.h"
#include "rx/rx.h"          // ← для rcData[]
#include <string.h>
#include <stdbool.h>

#define RX_BUF_SIZE 128
#define MODE_BUF_SIZE 16

static serialPort_t *rxPort = NULL;
static uint8_t rxBuf[RX_BUF_SIZE];
static uint16_t rxPos = 0;
static bool inMsg = false;

static char lastMode[MODE_BUF_SIZE] = "---";

static void parseMessage(char *msg)
{
    (void)msg;

    const char *p = strstr(msg, "MODE:");
    if (!p) {
        strncpy(lastMode, "---", sizeof(lastMode) - 1);
        lastMode[sizeof(lastMode) - 1] = '\0';
        return;
    }

    p += 5;

    size_t i = 0;
    while (i < MODE_BUF_SIZE - 1) {
        char c = *p;
        if (c == '\0' || c == ';' || c == '!' || c == ',' || c == '\r' || c == '\n') {
            break;
        }
        if (c < 32) { p++; continue; }
        if (c >= 'a' && c <= 'z') { c -= 32; }
        lastMode[i++] = c;
        p++;
    }
    lastMode[i] = '\0';

    if (lastMode[0] == '\0') {
        strncpy(lastMode, "---", sizeof(lastMode) - 1);
        lastMode[sizeof(lastMode) - 1] = '\0';
    }
}

static void customUartRxCallback(uint16_t c, void *ctx)
{
    (void)ctx;

    if (!inMsg) {
        if (c == '0') {
            inMsg = true;
            rxPos = 0;
            if (rxPos < RX_BUF_SIZE) {
                rxBuf[rxPos++] = (uint8_t)c;
            }
        }
        return;
    }

    if (rxPos >= RX_BUF_SIZE - 1) {
        inMsg = false;
        rxPos = 0;
        return;
    }

    rxBuf[rxPos++] = (uint8_t)c;

    if (c == '!') {
        if (rxPos < RX_BUF_SIZE) {
            rxBuf[rxPos] = '\0';
            parseMessage((char *)rxBuf);
        }
        inMsg = false;
        rxPos = 0;
    }
}

// --- Отправка команд по тумблеру ---

static bool vzvodPrevState = false;

void customUartPollSwitch(void)
{
    // AUX6 = канал 10 = индекс 9 в массиве rcData
    // rcData[] хранит значения 1000..2000
    uint16_t chValue = (uint16_t)rcData[9];
    bool currentState = (chValue > 1500);

    if (currentState && !vzvodPrevState) {
        // Включение
        serialWriteBuf(rxPort, (const uint8_t *)"0VZVOD_EN!", 11);
    } else if (!currentState && vzvodPrevState) {
        // Выключение
        serialWriteBuf(rxPort, (const uint8_t *)"0VZVOD_DIS!", 11);
    }


    vzvodPrevState = currentState;
}

const char *customUartRxGetMode(void)
{
    return lastMode;
}

void customUartRxInit(void)
{
    serialPortIdentifier_e portId = SERIAL_PORT_UART5;

    rxPort = openSerialPort(
        portId,
        FUNCTION_NONE,
        customUartRxCallback,
        NULL,
        115200,
        MODE_RXTX,
        SERIAL_NOT_INVERTED
    );

    if (!rxPort) {
        return;
    }
}
