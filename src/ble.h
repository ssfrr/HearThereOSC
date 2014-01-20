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

#include "stdint.h"

/**
 * DEFINES AND TYPES
 **/

#define BLE_NO_TIMEOUT -1
#define BLE_ANY_HANDLE -1

// Don't forget to add a human-readable status string in ble.c!
// if the response is BLE_ESYSTEM then there was an error in an internal system
// call and the application can check errno for details
typedef enum
{
    BLE_SUCCESS,
    BLE_EINVAL,
    BLE_ESYSTEM,
    BLE_EHCI_OPEN,
    BLE_EL2CAP_OPEN,
    BLE_EL2CAP_BIND,
    BLE_ECONNECT,
    BLE_ECALLBACKS_FULL,
} BLE_Status;

typedef void (*NotificationCallback)(int16_t, uint8_t *, size_t, void *);
typedef void (*WriteRequestCallback)(BLE_Status status);
typedef struct BLE_Connection BLE_Connection;



/**
 * PUBLIC FUNCTION DECLARATIONS
 **/

BLE_Status BLE_Connect(BLE_Connection **conn, const char *devMAC);
// handle can be used to limit which handles should trigger the callback. It
// can be BLE_ANY_HANDLE to accept from any handle
BLE_Status BLE_Disconnect(BLE_Connection *conn);
BLE_Status BLE_RegisterNotificationCallback(BLE_Connection *conn, int handle,
        NotificationCallback cb, void *param);
// if a callback is given then then the function returns as soon as the command
// goes out and the callback is called from the BLE_HandleEvents context when
// the response is received. If the callback is NULL the function blocks until
// the response is received.
BLE_Status BLE_WriteUint16Request(BLE_Connection *conn, int handle,
        uint16_t value, WriteRequestCallback cb, void *param);
BLE_Status BLE_HandleEvents(BLE_Connection *conn, int timeoutMs);
const char *BLE_GetErrorText(BLE_Status status);
