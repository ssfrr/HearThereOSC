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

#include <stdlib.h>
#include <errno.h>
#include <bluetooth/bluetooth.h>
#include "ble.h"

/**
 * INTERNAL DEFINES AND TYPES
 **/

#define ATT_OP_WRITE_REQ 0x12

// TODO: these are probably in a header file
#define ATT_CID 4
#define BDADDR_LE_PUBLIC       0x01
#define BDADDR_LE_RANDOM       0x02

#define MAX_NOTIFICATION_CALLBACKS 32
// TODO: what should this actually be?
#define MAX_MSG_SIZE 255

// TODO: this is in l2cap.h, but including that gives a bunch of other errors
struct sockaddr_l2 {
  sa_family_t    l2_family;
  unsigned short l2_psm;
  bdaddr_t       l2_bdaddr;
  unsigned short l2_cid;
  uint8_t        l2_bdaddr_type;
};

struct BLE_Connection
{
    int hci_sock;
    int l2cap_sock;
};


/**
 * MODULE DATA
 **/

// note the dummy string used for BLE_ESYSTEM, as BLE_GetErrorText passes
// through the string corresponding to the errno
static const char *StatusStrings[] = {
    "Success",
    "Invalid Argument",
    "",
    "Could not open HCI socket",
    "Could not open L2CAP socket",
    "Could not bind to L2CAP socket",
    "Could not connect to BLE Peripheral matching MAC address",
    "Callback table full",
};

// TODO: callbacks should be stored per-connection, probably in a linked list
static NotificationCallback NotificationCallbacks[MAX_NOTIFICATION_CALLBACKS];
static void *NotificationCallbackParams[MAX_NOTIFICATION_CALLBACKS];
// initialized to zero because it's declared static
static int NumNotificationCallbacks = 0;

/**
 * PRIVATE FUNCTION DECLARATIONS
 **/
static int get_l2cap_socket(void);
static int get_hci_socket(void);
static int bind_l2cap_socket(int l2cap_sock);
static int connect_l2cap_socket(int l2cap_sock, const char *dev_addr,
        uint8_t bdaddr_type);
void addUint16(uint8_t *dest, uint16_t value);
uint16_t getUint16(uint8_t *src);

/**
 * PUBLIC FUNCTION DEFINITIONS
 **/

BLE_Status BLE_Connect(BLE_Connection **conn, const char *devMAC)
{
    int status;
    int l2cap_sock, hci_sock;

    if(NULL == conn)
    {
        return BLE_EINVAL;
    }

    hci_sock = get_hci_socket();
    if (hci_sock < 0) {
        return BLE_EHCI_OPEN;
    }

    l2cap_sock = get_l2cap_socket();

    if (l2cap_sock < 0) {
        hci_close_dev(hci_sock);
        return BLE_EL2CAP_OPEN;
    }

    status = bind_l2cap_socket(l2cap_sock);
    if(status == -1)
    {
        hci_close_dev(hci_sock);
        close(l2cap_sock);
        return BLE_EL2CAP_BIND;
    }

    // TODO: should BDADDR_LE_RANDOM come from func args?
    status = connect_l2cap_socket(l2cap_sock, devMAC, BDADDR_LE_RANDOM);
    if(status == -1)
    {
        hci_close_dev(hci_sock);
        close(l2cap_sock);
        return BLE_ECONNECT;
    }

    *conn = (BLE_Connection *)malloc(sizeof(BLE_Connection));
    if(NULL == *conn)
    {
        int tempErrno = errno;
        hci_close_dev(hci_sock);
        close(l2cap_sock);
        // restore errno so the user gets the original error.
        errno = tempErrno;
        return BLE_ESYSTEM;
    }

    (*conn)->hci_sock = hci_sock;
    (*conn)->l2cap_sock = l2cap_sock;

    return BLE_SUCCESS;
}

BLE_Status BLE_Disconnect(BLE_Connection *conn)
{
    hci_close_dev(conn->hci_sock);
    close(conn->l2cap_sock);
    free(conn);
    return BLE_SUCCESS;
}

// handle can be used to limit which handles should trigger the callback. It
// can be BLE_ANY_HANDLE to accept from any handle
BLE_Status BLE_RegisterNotificationCallback(BLE_Connection *conn, int handle,
        NotificationCallback cb, void *param)
{
    //TODO: store handle to be used in later filtering
    if(BLE_ANY_HANDLE != handle)
    {
        printf("Filtering by handle currently not supported");
        return BLE_EINVAL;
    }
    if(NumNotificationCallbacks == MAX_NOTIFICATION_CALLBACKS)
    {
        return BLE_ECALLBACKS_FULL;
    }
    NotificationCallbacks[NumNotificationCallbacks] = cb;
    NotificationCallbackParams[NumNotificationCallbacks] = param;
    ++NumNotificationCallbacks;
    return BLE_SUCCESS;
}

// if a callback is given then then the function returns as soon as the command
// goes out and the callback is called from the BLE_HandleEvents context when
// the response is received. If the callback is NULL the function blocks until
// the response is received.
BLE_Status BLE_WriteUint16Request(BLE_Connection *conn, int handle,
        uint16_t value, WriteRequestCallback cb, void *param)
{
    uint8_t buf[5];
    int read_bytes;
    if(NULL != cb || NULL != param)
    {
        printf("async attribute writing (with callback) not supported yet\n");
        return BLE_EINVAL;
    }
    buf[0] = ATT_OP_WRITE_REQ;
    addUint16(&buf[1], handle);
    addUint16(&buf[3], value);
    // TODO: what errors can this throw?
    write(conn->l2cap_sock, buf, sizeof(buf));
    // here we wait for the response
    // TODO: some error handling to verify we actually get the ACK we expect
    read_bytes = read(conn->l2cap_sock, buf, sizeof(buf));
    return BLE_SUCCESS;
}

BLE_Status BLE_HandleEvents(BLE_Connection *conn, int timeoutMs)
{
    int i;
    int read_bytes;
    uint8_t buf[MAX_MSG_SIZE];
    uint8_t opcode;
    uint16_t handle;
    // TODO: what errors can this throw?
    read_bytes = read(conn->l2cap_sock, buf, sizeof(buf));
    if(read_bytes == -1)
    {
        return BLE_ESYSTEM;
    }
    if(read_bytes < 3)
    {
        printf("Received short packet: %d bytes\n", read_bytes);
        // not a fatal error, just consider it like no message was received
        return BLE_SUCCESS;
    }
    opcode = buf[0];
    handle = getUint16(&buf[1]);

    for(i = 0; i < NumNotificationCallbacks; ++i)
    {
        if(NotificationCallbacks[i])
        {
            NotificationCallbacks[i](handle, &buf[3], read_bytes - 3,
                    NotificationCallbackParams[i]);
        }
    }
    return BLE_SUCCESS;
}

const char *BLE_GetErrorText(BLE_Status status)
{
    if(BLE_ESYSTEM == status)
    {
        return strerror(errno);
    }
    return StatusStrings[status];
}

/**
 * PRIVATE FUNCTION DEFINITIONS
 **/

static int get_hci_socket(void)
{
    int dev_id = hci_get_route(NULL);
    if (dev_id < 0) {
        perror("getting hci device ID");
        exit(1);
    }
    return hci_open_dev(dev_id);
}

static int get_l2cap_socket(void)
{

    int l2cap_sock = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    return l2cap_sock;
}

static int bind_l2cap_socket(int l2cap_sock)
{
    struct sockaddr_l2 sockAddr;

    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.l2_family = AF_BLUETOOTH;
    bacpy(&sockAddr.l2_bdaddr, BDADDR_ANY);
    sockAddr.l2_cid = htobs(ATT_CID);

    return bind(l2cap_sock, (struct sockaddr *)&sockAddr, sizeof(sockAddr));
}

// takes the address of the device to connect to (e.g. "E6:4B:E0:99:63:8C")
// and an address type which is either BDADDR_LE_RANDOM or BDADDR_LE_PUBLIC
static int connect_l2cap_socket(int l2cap_sock, const char *dev_addr,
        uint8_t bdaddr_type)
{
    struct sockaddr_l2 sockAddr;

    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.l2_family = AF_BLUETOOTH;
    str2ba(dev_addr, &sockAddr.l2_bdaddr);
    sockAddr.l2_bdaddr_type = bdaddr_type;
    sockAddr.l2_cid = htobs(ATT_CID);

    return connect(l2cap_sock, (struct sockaddr *)&sockAddr, sizeof(sockAddr));
}

static void process_l2cap_data(int l2cap_sock)
{
    uint8_t l2cap_data[255];
    int i;
    int read_bytes;
    printf("Waiting for l2cap data...\n");
    read_bytes = read(l2cap_sock, l2cap_data, sizeof(l2cap_data));
    printf("Read %d bytes: ", read_bytes);
    for(i = 0; i < read_bytes; i++) {
        printf("%02x ", l2cap_data[i]);
    }
    printf("\n");
}

void addUint16(uint8_t *dest, uint16_t value)
{
    dest[0] = value & 0xff;
    dest[1] = value >> 8;
}

uint16_t getUint16(uint8_t *src)
{
    return (src[1] << 8) | src[0];
}

