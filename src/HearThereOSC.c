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

#define ORIENTATION_CCCD_HANDLE 0x000f
#define CALIBRATION_HANDLE 0x0011

#define RFDUINO_MAC "FB:53:83:F3:18:FD" // donatello
//#define RFDUINO_MAC "F0:AB:47:E7:43:62" // raphael

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

    sleep(1);
//    printf("Starting Calibration...\n");
//    status = BLE_WriteUint16Request(conn, CALIBRATION_HANDLE, 0x0003, NULL, NULL);
//    printf("Chilling Out...\n");
//    sleep(5);

    printf("Registering Notification Callback...\n");
    status = BLE_RegisterNotificationCallback(conn, BLE_ANY_HANDLE,
            bleDataReceived, (void *)&addr);
    HANDLE_STATUS(status);

    printf("Enabling Orientation Notifications...\n");
    status = BLE_WriteUint16Request(conn, ORIENTATION_CCCD_HANDLE, 0x0001, NULL, NULL);
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
    // initialize last_seq to -1 as a sentinel value meaning uninitialized
    static int last_seq = -1;
    int i;
    lo_address addr = *(lo_address *)param;

    if(dataLen != 17)
    {
        printf("got packet of %zu bytes! expected 17\n", dataLen);
        return;
    }

    uint8_t seq = data[0];
    float w = *(float *)(data + 1);
    float x = *(float *)(data + 5);
    float y = *(float *)(data + 9);
    float z = *(float *)(data + 13);
    if(last_seq != -1 && seq != (uint8_t)(last_seq + 1)) {
        printf("Unexpected sequence number %d. Lost %d packets.\n", seq, (uint8_t)(seq - last_seq));
    }
    last_seq = seq;
    if(lo_send(addr, OSC_PATH, "ffff", w, x, y, z) == -1) {
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
