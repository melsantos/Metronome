/* 
Modified code for use of shift registers on the lower and upper nibble of PORTD
*/

void transmit_data_lower_nibble(unsigned char data) {
	int i;
	for (i = 0; i < 8 ; ++i) {
		
		// Sets SRCLR to 1 allowing data to be set
		// Also clears SRCLK in preparation of sending data
		PORTD = 0x08;
		
		// set SER = next bit of data to be sent.
		PORTD |= ((data >> i) & 0x01);
		
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		PORTD |= 0x02;
	}
	
	// set RCLK = 1. Rising edge copies data from “Shift” register to “Storage” register
	PORTD |= 0x04;
	
	// clears all lines in preparation of a new transmission
	PORTD = 0x00;
	
}

void transmit_data_upper_nibble(unsigned char data) {
	int i;
	for (i = 0; i < 8 ; ++i) {
		
		// Sets of upper nibble nibble to SRCLR to 1 allowing data to be set
		// Also clears SRCLK in preparation of sending data
		PORTD = 0x80;
		
		// set SER = next bit of data to be sent.
		if(i < 4) {
			PORTD |= ((data << (4 - i)) & 0x10);
		} else {
			PORTD |= ((data >> (i % 4)) & 0x10);
		}
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		PORTD |= 0x20;
	}
	
	// set RCLK = 1. Rising edge copies data from “Shift” register to “Storage” register
	PORTD |= 0x40;
	
	// clears all lines in preparation of a new transmission
	PORTD = 0x00;
	
}
