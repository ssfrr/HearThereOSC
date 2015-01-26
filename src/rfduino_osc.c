/*
 * Copyright 2014 Spencer Russell
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lo/lo.h"
#include "ble.h"

#define OSC_HOST "localhost"
#define OSC_PORT "10001"
#define OSC_PATH "/orientation"

#define CONFIGURATION_HANDLE 0x000f
#define RFDUINO_MAC "E2:BB:09:A5:B9:58"

#define HANDLE_STATUS(expr) handleStatus((expr), __FILE__, __LINE__)

static void handleStatus(BLE_Status status, const char* fileName, int lineNum);
static void bleDataReceived(int16_t handle, uint8_t *data,
        size_t dataLen, void *param);

int main(int argc, char **argv)
{
    BLE_Status status;
    BLE_Connection *conn = NULL;
    lo_address addr = lo_address_new(OSC_HOST, OSC_PORT);

    printf("Connecting...\n");
    status = BLE_Connect(&conn, RFDUINO_MAC);
    HANDLE_STATUS(status);

    printf("Configuring...\n");
    status = BLE_WriteUint16Request(conn, CONFIGURATION_HANDLE, 0x0001, NULL, NULL);
    HANDLE_STATUS(status);

    printf("Registering Notification Callback...\n");
    status = BLE_RegisterNotificationCallback(conn, BLE_ANY_HANDLE,
            bleDataReceived, (void *)&addr);
    HANDLE_STATUS(status);

    printf("Starting handler loop...\n");
    while(BLE_SUCCESS == status)
    {
        status = BLE_HandleEvents(conn, BLE_NO_TIMEOUT);
        HANDLE_STATUS(status);
    }

    status = BLE_Disconnect(conn);
    HANDLE_STATUS(status);
}

static void bleDataReceived(int16_t handle, uint8_t *data,
        size_t dataLen, void *param)
{
    int i;
    lo_address addr = *(lo_address *)param;

    if(dataLen != 9)
    {
        printf("got packet of %zu bytes! expected 9\n", dataLen);
        return;
    }

    for(i = 0; i < 9; ++i)
    {
        uint8_t seq = data[0];
        int16_t w = (data[1] << 8) + data[2];
        int16_t x = (data[3] << 8) + data[4];
        int16_t y = (data[5] << 8) + data[6];
        int16_t z = (data[7] << 8) + data[8];
        if (lo_send(addr, OSC_PATH, "ffff", w, x, y, z) == -1)
        {
            printf("OSC error %d: %s\n", lo_address_errno(addr),
                   lo_address_errstr(addr));
        }
    }
}

static void handleStatus(BLE_Status status, const char* fileName, int lineNum)
{
    if(BLE_SUCCESS == status)
    {
        return;
    }
    printf("BLE Error (%s:%d): %s\n", fileName, lineNum,
            BLE_GetErrorText(status));
    exit(status);
}
