#ifndef CONSTANTS_H
#define CONSTANTS_H

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */

#define FLAG 0x7E

#define SET_SIZE 5
#define SET_ADDRESS 0x03
#define SET_CONTROL 0x03

#define UA_SIZE 5
#define UA_ADDRESS_SENDER 0x03
#define UA_ADDRESS_RECEIVER 0x01
#define UA_CONTROL 0x07

#define INF_INIT_SIZE 4
#define INF_ADDRESS 0x03
#define INF_CONTROL0 0x00
#define INF_CONTROL1 0x40
#define INF_ESCAPE 0x7D
#define INF_XOR_FLAG 0x5E
#define INF_XOR_ESCAPE 0x5D

#define RR_SIZE 5
#define RR_ADDRESS 0x03
#define RR_CONTROL0 0x05
#define RR_CONTROL1 0x85
#define REJ_CONTROL0 0x01
#define REJ_CONTROL1 0x81

#define DISC_SIZE 5
#define DISC_ADDRESS_SENDER 0x03
#define DISC_ADDRESS_RECEIVER 0x01
#define DISC_CONTROL 0x0B

#define MAX_ALARMS 3

#endif
