/* Name: Samuel Burgess
 *		 Justin Forgue
 *		 Aric Pennington
 *		 Elias Phillips
 * Date: 10/29/2018
 * Course Number: ECE353
 * Course Name: Computer Systems Lab 1
 * Project: General MIDI Explorer 
 * Main Program that drives the MIDI explorer hardware. 
 */

#include <util/delay.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>

void USART_Init(unsigned int baud);
void port_Init();
unsigned int ADCRead (unsigned int volt);
void EEPROM_Write(unsigned int address, unsigned char data);
unsigned char EEPROM_Read(unsigned int address);
int isRecordOn();
int isPlayOn();
int isModifyOn();
unsigned char USART_Receive(void);
void USART_Transmit(unsigned char data);
void USART_Flush(void);

int main(void){
    /* Initialize all of the address values and the baud rate */
    unsigned int baud = 0x07;
    unsigned int writeAddress = 0;
    unsigned int readAddress = 0;
    unsigned char data[3];
    unsigned int noteCount = 0;
	unsigned int notePlay = 0;
	uint16_t time = 0;
	float modifier;
	
	/* Initialize all of the ports and baud rate */
	USART_Init(baud);
    USART_Flush();
	port_Init();

    /* Start up the device to run forever */
	while(1){
		
		/* Record mode set on */
		if(isRecordOn()){
			
			/* Reset all of the local values incase the
			 * user decides to re-record notes to be played 
			 */
			USART_Init(0x07);
			writeAddress = 0;
			noteCount = 0;
			TCNT1 = 0;
			USART_Flush();
            notePlay = 0;
            time = 0;
			
            /* Records either until record is switched off
			 * or that the eeprom is full 
			 */
            while((writeAddress<1022U) && isRecordOn()){
                
				/* Store each of the three MIDI bytes in EEPROM	*/
				for(unsigned int i=0;i<3;i++){
					data[i] = USART_Receive();
                    EEPROM_Write(writeAddress,data[i]);
					writeAddress++;
					
					/* At the third byte of data records the timing data into the eeprom */
					if(i==2){
						EEPROM_Write(1023,noteCount++);
						time=TCNT1;
						EEPROM_Write(writeAddress++,(time));
                    	EEPROM_Write(writeAddress++,time>>8);
						TCNT1 = 0;
					}
                    PORTB = data[1];
        		}
    		}
		}

		
		/* Play mode set on */
        if(isPlayOn()){
		    USART_Init(0x07);
		    readAddress = 0;
			notePlay=0;
			noteCount=EEPROM_Read(1023);
			USART_Flush();
			time = 0;
		
			/* Reads from the eeprom until either all recorded 
			 * notes are played or play is switched off
			 */
            while (notePlay < noteCount && isPlayOn()){
			    
				/* Reads each of the three MIDI bytes in EEPROM	*/
				for(unsigned int j=0;j<3;j++){
                	data[j] = EEPROM_Read(readAddress);
					USART_Transmit(EEPROM_Read(readAddress));
					readAddress++;
                }
				
				/* Adds the delay for the timing in playback 
				 * by searching for the time delay in the eeprom
				 */
				time = EEPROM_Read(readAddress+5)|(EEPROM_Read(readAddress+6)<<8);
				
				/* If the modify switch is set on then time delay
				 * will be modified by the voltage of the photocell
				 */
				if(isModifyOn()){
					modifier = ADCRead(PINA & (1 << PA7));
					modifier = 2 * (modifier / 1024);
					time *= modifier;
				}	
				
				/* Resets the clock */
				TCNT1 = 0;
				
				/* Delays until the clock reaches the 
				 * the time stored from the eeprom 
				 */
				while(TCNT1<time){}
				
				readAddress+=2;
				PORTB = data[1];
				notePlay++;
            }
			// Causes extra light flash
			_delay_ms(1000);
        }
		PORTB = 0;
    }
}

/*
 * Function: USART_Init
 * -------------------
 *
 *   Initialize the USART with the appropriate bits and baud rate
 *
 *   returns: returns nothing
 */
void USART_Init(unsigned int baud){
    /* Assign upper part of baud number (bits 8 to 11) */
    UBRRH = (unsigned char)(baud>>8);

    /* Assign remaining baud number */
    UBRRL = (unsigned char)baud;

    /* Enable receiver and transmitter */
    UCSRB = (1<<RXEN)|(1<<TXEN);

    /* Initialize to 8 data bits 1 stop bit */
    UCSRC = (1<<URSEL)|(3<<UCSZ0);

}

/*
 * Function: port_Init
 * -------------------
 *
 *   Initialize the data ports on the ATmega including the timer
 *   and the ADC registers
 *
 *   returns: returns nothing
 */
void port_Init(){
	/* Make all ports input */
    DDRA = 0x00;

	/* Make all ports output */
    DDRB = 0xFF;

	/* Set LED's off by default */
    PORTB = 0x00;

	/* Prescaler = 256 */
    TCCR1B |= (1<<CS12);  
	
	/* Timer set to zero */
	TCNT1 = 0;		
	
	/* Enable timer1 overflow interrupt(TOIE1) */
	TIMSK = (1<<OCIE1A); 
	OCR1A = 12499;

	/* Set ADCSRA ( ADC sample rate ) to 32 prescale = 4MHz/32 = 125KHz */
	ADCSRA |= (1 << ADPS2) | (1 << ADPS0);

	/* Set ADC reference voltage to AVCC */
	ADMUX = (1 << REFS0);

	/* Select Pin A7 */
	ADMUX |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

	/* Enable the ADC */
	ADCSRA |= 1<<ADEN | 0<<ADSC;

	/* Enable global interrupts */
	sei();
}

/*
 * Function: ISR
 * -------------------
 *
 *   Take the interupt signal and sets the LED lights off
 *
 *   returns: returns nothing
 */
ISR(TIMER1_COMPA_vect){
	PORTB=0x00;
}

/*
 * Function: ADCRead
 * -------------------
 *
 *   Finds the analog voltage from the digital voltage 
 *
 *   returns: analog voltage
 */
unsigned int ADCRead (unsigned int volt) {
	/* ADMUX already set to reference voltage (AVCC) */
	ADMUX |= volt;

	/* Begin Analog to Digital conversion */
	ADCSRA |= 1 << ADSC;

	/* Wait until the conversion has been finished */
	while(ADCSRA & (1<<ADSC)) {  }

	return ADC;
}

/*
 * Function: EEPROM_Write
 * -------------------
 *
 *   Writes one byte of data at a time to the eeprom at the given address.
 *   Prevents concurrent writing to the eeprom
 *
 *   returns: returns nothing
 */
void EEPROM_Write(unsigned int address, unsigned char data){
    /* wait for completetion of previous write */
    while(EECR & (1<<EEWE)){}

    /* set up address and data registers */
    EEAR = address;
    EEDR = data;
	
    /* Write logical one to EEMWE */
    EECR |= (1<<EEMWE);
	
    /* start eeprom write */
    EECR |= (1<<EEWE);
}

/*
 * Function: EEPROM_Read
 * -------------------
 *
 *   Reads one byte of data at a time to the eeprom at the given address.
 *   Prevents concurrent reading to the eeprom
 *
 *   returns: one byte of data from eeprom
 */
unsigned char EEPROM_Read(unsigned int address){
    /* wait for completetion of previous write */
    while(EECR & (1<<EEWE)){ }

    /* set up address register */
    EEAR = address;

    /* start EEPROM read */
    EECR |= (1<<EERE);

    /* Return Data */
    return EEDR;
}

/*
 * Function: isRecordOn
 * -------------------
 *
 *   Checks if the record switch has been switch on yet
 *
 *   returns: 1 if the switch is on and 0 if it is off
 */
int isRecordOn(){
    uint8_t temp = PINA & 0x01;
    if(temp == 0x01)
        return 1;
    else
        return 0;
    return -1;
}

/*
 * Function: isPlayOn
 * -------------------
 *
 *   Checks if the play switch has been switch on yet
 *
 *   returns: 1 if the switch is on and 0 if it is off
 */
int isPlayOn(void){
    uint8_t temp = PINA & 0x02;
    if(temp == 0x02)
        return 1;
    else
        return 0;
    return -1;
}

/*
 * Function: isModifyOn
 * -------------------
 *
 *   Checks if the modify switch has been switch on yet
 *
 *   returns: 1 if the switch is on and 0 if it is off
 */
int isModifyOn(){
    uint8_t temp = PINA & 0x04;
    if(temp == 0x04)
        return 1;
    else
        return 0;
    return -1;
}

/*
 * Function: USART_Receive
 * -------------------
 *
 *   Takes in MIDI data from the computer and stores it into the UDR 
 *
 *   returns: returns the data that was recieved 
 */
unsigned char USART_Receive(void){
    /* Wait for available data */
    while(!(UCSRA & (1<<RXC))){
        if(isRecordOn()!=1)
            return 0;
    }

    /* Make data available in buffer */
    return UDR;
}

/*
 * Function: USART_Transmit
 * -------------------
 *
 *   Sends MIDI data from the USART out to the computer and loads
 *   that into that UDR buffer
 *
 *   returns: returns nothing
 */
void USART_Transmit(unsigned char data){
    /* Wait for transmitter */
    while(!(UCSRA & (1<<UDRE))){}

    /* Write data to USART buffer */
    UDR = data;
}

/*
 * Function: USART_Flush
 * -------------------
 *
 *   Flushes anything that is in the USART so there is norm
 *   bad data being written or read
 *
 *   returns: returns nothing
 */
void USART_Flush(void){
    unsigned char dummy;
    while (UCSRA & (1<<RXC)) dummy = UDR;
}