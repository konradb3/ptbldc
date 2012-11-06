#include "usb.h"		  

extern uint8_t USB_Rx_Buffer[VIRTUAL_COM_PORT_DATA_SIZE];
extern uint16_t USB_Rx_Cnt;

extern uint8_t  USART_Rx_Buffer [USART_RX_DATA_SIZE]; 
extern uint32_t USART_Rx_ptr_in;

void USB_Config(void){					  
//	!!!! NO CONFIGURABLE USB PULLUP in 1110B
//	GPIO_InitTypeDef GPIO_InitStructure;
//	// IO Clocks Enable
//	RCC_APB2PeriphClockCmd(USB_CONNECT_APB, ENABLE);
//	/*	USB_CONNECT_PORT Push-Pull 10MHz Outputs:	*\
//		USB_CONNECT_PIN	USB_PullUp					*/
//	GPIO_InitStructure.GPIO_Pin = USB_CONNECT_PIN;
//	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
//	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
//	GPIO_Init(USB_CONNECT_PORT, &GPIO_InitStructure);

	// USB Init
	Set_USBClock();
	USB_Interrupts_Config();			
	USB_Init();

//	// Enable USBDP PullUp
//	USB_Cable_Config(ENABLE);
}

void USB_SendNBytes(uint8_t* buf, uint16_t cnt){
	uint16_t i;
	for(i = 0; i < cnt; i++){
		USART_Rx_Buffer[USART_Rx_ptr_in] = buf[i];
		USART_Rx_ptr_in++;
		/* To avoid buffer overflow */
		if(USART_Rx_ptr_in == USART_RX_DATA_SIZE)
			USART_Rx_ptr_in = 0;
	}
}

uint8_t USB_ReadOneByte(void){
	uint8_t ret = 0;
	uint32_t i;
	if(USB_Rx_Cnt > 0){
		ret = USB_Rx_Buffer[0];
		for(i=1; i < USB_Rx_Cnt; i++)
			USB_Rx_Buffer[i-1] = USB_Rx_Buffer[i];
		USB_Rx_Cnt --;
	}
	return ret;
}

uint8_t USB_RxBufNotEmpty(void){
	return (USB_Rx_Cnt > 0) ? 1 : 0;
}
