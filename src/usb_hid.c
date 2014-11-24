/* ReportID IN = 0x03, ReportID OUT = 0x04 */

#include "usb_hid.h"

uint8_t USB_HID_OUT_BUF[HID_OUT_BUFFER_SIZE+1];	/* PC->STM32 */
uint8_t USB_HID_IN_BUF[HID_IN_BUFFER_SIZE+1];	/* STM32->PC */

uint16_t USB_HID_RecData_Len;
uint8_t USB_HID_RecData_Ready;


void USB_HID_Init(void)
{
	bDeviceState=UNCONNECTED;
	USB_HID_RecData_Ready=0;
	USB_HID_RecData_Len=0;
	Set_System();
	USB_Interrupts_Config();
	Set_USBClock();
	USB_Init();
}


DEVICE_STATE USB_HID_GetStatus(void)
{
	return(bDeviceState);
}

uint16_t USBD_HID_RecReport(void)
{
	uint16_t ret_wert=0;

	if(USB_HID_RecData_Ready==1) {
		USB_HID_RecData_Ready=0;
		ret_wert=USB_HID_RecData_Len;
	}

	return(ret_wert);
}

/* len: bytes to send (1...HID_IN_BUFFER_SIZE) */
ErrorStatus USB_HID_SendData(uint8_t report_id, uint8_t *ptr, uint8_t len)
{
	uint8_t n;

	if(bDeviceState!=CONFIGURED)
		return(ERROR);

	/* Report ID */
	USB_HID_IN_BUF[0]=report_id;

	for(n=0;n<=HID_IN_BUFFER_SIZE;n++) {
		if(n<=len) {
			/* after Report ID */
			USB_HID_IN_BUF[n+1]=*ptr;
			ptr++;
		} else {
			USB_HID_IN_BUF[n]=0x00;
		}
	}

	if (HID_SendData() != (HID_IN_BUFFER_SIZE+1))
		return(ERROR);

	return(SUCCESS);
}


USB_HID_RXSTATUS_t USB_HID_ReceiveData(uint8_t *ptr)
{
	uint16_t check, n;

	if(bDeviceState!=CONFIGURED)
		return(RX_USB_ERR);

	check=USBD_HID_RecReport();
	if(check==0) {
		ptr[0]=0x00;
		return(RX_EMPTY);
	}

	/* USB_HID_OUT_BUF[0] is Report ID */
	for(n=0;n<HID_OUT_BUFFER_SIZE+1;n++) {
		if(n<check)
			ptr[n]=USB_HID_OUT_BUF[n+1];
		else
			ptr[n]=0x00;
	}

	return(RX_READY);
}
