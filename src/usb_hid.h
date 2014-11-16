
#ifndef __USB_HID_H
#define __USB_HID_H

#include "stm32f10x.h"
#include "usb_core.h"
#include "usb_desc.h"
#include "usb_init.h"
#include "usb_pwr.h"
#include "hw_config.h"

#define   HID_IN_BUFFER_SIZE   16  //  (1...64) STM32->PC
#define   HID_OUT_BUFFER_SIZE  16  //  (1...64) PC->STM32

#define   HID_IN_INTERVAL     1  // (1...255) STM32->PC
#define   HID_OUT_INTERVAL    1  // (1...255) PC->STM32


typedef enum {
  RX_USB_ERR =0,
  RX_EMPTY,
  RX_READY
}USB_HID_RXSTATUS_t; 


void USB_HID_Init(void);
DEVICE_STATE USB_HID_GetStatus(void);
ErrorStatus USB_HID_SendData(uint8_t *ptr, uint8_t len);
USB_HID_RXSTATUS_t USB_HID_ReceiveData(uint8_t *ptr);

#endif // __USB_HID_H
