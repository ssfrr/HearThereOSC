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
#include "lo/lo.h"
#include "ble.h"

#define OSC_HOST "localhost"
#define OSC_PORT "9383"
#define OSC_PATH "/ble"

#define CONFIGURATION_HANDLE 0x000f
#define RFDUINO_MAC "E6:4B:E0:99:63:8C"

#define HANDLE_STATUS(expr) handleStatus((expr), __FILE__, __LINE__)

static void handleStatus(BLE_Status status, const char* fileName, int lineNum);
static void bleDataReceived(int16_t handle, uint8_t *data,
        size_t dataLen, void *param);

int main(int argc, char **argv)
{
    BLE_Status status;
    BLE_Connection *conn = NULL;
    lo_address addr = lo_address_new(OSC_HOST, OSC_PORT);

    status = BLE_Connect(&conn, RFDUINO_MAC);
    HANDLE_STATUS(status);

    status = BLE_WriteUint16Request(conn, CONFIGURATION_HANDLE, 0x0001, NULL, NULL);
    HANDLE_STATUS(status);

    status = BLE_RegisterNotificationCallback(conn, BLE_ANY_HANDLE,
            bleDataReceived, (void *)&addr);
    HANDLE_STATUS(status);

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
    lo_address addr = *(lo_address *)param;
    // TODO: we're not handling endianness here
    int sensorData = *(int *)data;

    if (lo_send(addr, OSC_PATH, "i", sensorData) == -1)
    {
        printf("OSC error %d: %s\n", lo_address_errno(addr),
               lo_address_errstr(addr));
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
