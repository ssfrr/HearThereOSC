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

struct BLE_Connection
{
    int dummy;
};

#define ATT_OP_WRITE_REQ 0x12

// TODO: these are probably in a header file
#define ATT_CID 4
#define BDADDR_LE_PUBLIC       0x01
#define BDADDR_LE_RANDOM       0x02

// TODO: this is in l2cap.h, but including that gives a bunch of other errors
struct sockaddr_l2 {
  sa_family_t    l2_family;
  unsigned short l2_psm;
  bdaddr_t       l2_bdaddr;
  unsigned short l2_cid;
  uint8_t        l2_bdaddr_type;
};

/**
 * MODULE DATA
 **/

static const char *StatusStrings[] = {
    "Success",
    "Invalid argument given",
};

/**
 * PRIVATE FUNCTION DECLARATIONS
 **/

/**
 * PUBLIC FUNCTION DEFINITIONS
 **/

BLE_Status BLE_Connect(BLE_Connection **conn, const char *devMAC)
{
    return BLE_SUCCESS;
}

// handle can be used to limit which handles should trigger the callback. It
// can be BLE_ANY_HANDLE to accept from any handle
BLE_Status BLE_RegisterNotificationCallback(BLE_Connection *conn, int handle,
        NotificationCallback cb, void *param)
{
    return BLE_SUCCESS;
}

BLE_Status BLE_Disconnect(BLE_Connection *conn)
{
    return BLE_SUCCESS;
}

const char *BLE_GetErrorText(BLE_Status status)
{
    return StatusStrings[status];
}

// if a callback is given then then the function returns as soon as the command
// goes out and the callback is called from the BLE_HandleEvents context when
// the response is received. If the callback is NULL the function blocks until
// the response is received.
BLE_Status BLE_WriteUint16Request(BLE_Connection *conn, int handle,
        uint16_t value, WriteRequestCallback cb)
{
    return BLE_SUCCESS;
}

BLE_Status BLE_HandleEvents(BLE_Connection *conn, int timeoutMs)
{
    return BLE_SUCCESS;
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
static int connect_l2cap_socket(int l2cap_sock, const char *dev_addr, uint8_t bdaddr_type)
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

static void enable_rfduino_notifications(int l2cap_socket)
{
    // format is opcode, handle_LSB, handle_MSB, data
    // handle for configuration is 0x000f
    // value is 0x0001 to enable notifications
    uint8_t cmdbuf[] = {ATT_OP_WRITE_REQ, 0x0f, 0x00, 0x01, 0x00};
    write(l2cap_socket, cmdbuf, sizeof(cmdbuf));
}


int ble_init(void)
{
    int hci_sock, l2cap_sock;
    int status;
    // TODO: get address by scanning
    char addr[19] = {"E6:4B:E0:99:63:8C"};

    hci_sock = get_hci_socket();
    if (hci_sock < 0) {
        perror("opening hci socket");
        goto no_cleanup;
    }

    l2cap_sock = get_l2cap_socket();
    if (l2cap_sock < 0) {
        perror("opening l2cap socket");
        goto cleanup_hci;
    }

    status = bind_l2cap_socket(l2cap_sock);
    printf("bind: ");
    if(status == -1)
    {
        printf("%s\n", strerror(errno));
        goto cleanup_l2cap;
    }
    printf("success\n");

    status = connect_l2cap_socket(l2cap_sock, addr, BDADDR_LE_RANDOM);
    printf("connect: ");
    if(status == -1)
    {
        printf("%s\n", strerror(errno));
        goto cleanup_l2cap;
    }
    printf("success\n");

    enable_rfduino_notifications(l2cap_sock);

    while(1)
    {
        process_l2cap_data(l2cap_sock);
    }

cleanup_l2cap:
    close(l2cap_sock);

cleanup_hci:
    /* we could use the normal close() here, but we use hci_close_dev for
     * symmetry, in case the bluetooth people add some cleanup code later */
    hci_close_dev(hci_sock);

no_cleanup:

    return 0;
}
