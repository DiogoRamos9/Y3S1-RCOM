// Application layer protocol header.
// NOTE: This file must not be changed.

#ifndef _APPLICATION_LAYER_H_
#define _APPLICATION_LAYER_H_

#include <stdio.h>

// Application layer main function.
// Arguments:
//   serialPort: Serial port name (e.g., /dev/ttyS0).
//   role: Application role {"tx", "rx"}.
//   baudrate: Baudrate of the serial port.
//   nTries: Maximum number of frame retries.
//   timeout: Frame timeout.
//   filename: Name of the file to send / receive.

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename);


// Send a data packet.
int sendDtPckt(unsigned char* data, int dataSize);

// Read a control packet.
int readctrlpckt(unsigned char control, unsigned char* buf, size_t* file_size, char* filename);

// Send a control packet.
int sendCtrlPckt(int some, const char* fileName, long int length);





#endif // _APPLICATION_LAYER_H_
