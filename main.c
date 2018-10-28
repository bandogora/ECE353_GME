#include <util/delay.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>

void USART_Init(unsigned int baud){
    //Assign upper part of baud number (bits 8 to 11)
    UBRRH = (unsigned char)(baud>>8);

    //Assign remaining baud number
    UBRRL = (unsigned char)baud;

    //Enable receiver and transmitter
    UCSRB = (1<<RXEN)|(1<<TXEN);

    //Initialize to 8 data bits 1 stop bit
    UCSRC = (1<<URSEL)|(3<<UCSZ0);

}

ISR(TIMER1_COMPA_vect){
	PORTB=0x00;
}

unsigned int ADCRead (unsigned int volt) {
	/* ADMUX already set to reference voltage (AVCC) */
	ADMUX |= volt;

	/* Begin Analog to Digital conversion */
	ADCSRA |= 1 << ADSC;

	/* Wait until the conversion has been finished */
	while(ADCSRA & (1<<ADSC)) {  }

	return ADC;
}

void EEPROM_Write(unsigned int address, unsigned char data){
    //wait for completetion of previous write
    while(EECR & (1<<EEWE)){}

    //set up address and data registers
    EEAR = address;
    EEDR = data;
    //Write logical one to EEMWE
    EECR |= (1<<EEMWE);
    //start eeprom write
    EECR |= (1<<EEWE);
}

unsigned char EEPROM_Read(unsigned int address){
    //wait for completetion of previous write
    while(EECR & (1<<EEWE)){ }

    //set up address register
    EEAR = address;

    //start EEPROM read
    EECR |= (1<<EERE);

    //Return Data
    return EEDR;
}

int isRecordOn(){
    uint8_t temp = PINA & 0x01;
    if(temp == 0x01)
        return 1;
    else
        return 0;
    return -1;
}

int isPlayOn(void){
    uint8_t temp = PINA & 0x02;
    if(temp == 0x02)
        return 1;
    else
        return 0;
    return -1;
}

int isModifyOn(){
    uint8_t temp = PINA & 0x04;
    if(temp == 0x04)
        return 1;
    else
        return 0;
    return -1;
}

unsigned char USART_Receive(void){
    //Wait for available data
    while(!(UCSRA & (1<<RXC))){
        if(isRecordOn()!=1)
            return 0;
    }

    //Make data available in buffer
    return UDR;
}

void USART_Transmit(unsigned char data){
    //Wait for transmitter
    while(!(UCSRA & (1<<UDRE))){}

    //Write data to USART buffer
    UDR=data;
}

void USART_Flush(void){
    unsigned char dummy;
    while (UCSRA & (1<<RXC)) dummy = UDR;
}



int main(void){
    //set baud rate
    unsigned int baud = 0x07;
    USART_Init(baud);
    USART_Flush();

    DDRA = 0x00;            //Make all ports input
    DDRB = 0xFF;            //Make all ports output
    PORTB = 0x00;

    TCCR1B |= (1<<CS12);  //Prescaler = 256
	TCNT1 = 0;			//Timer set to zero
	TIMSK = (1<<OCIE1A); //Enable timer1 overflow interrupt(TOIE1)
	OCR1A = 12499;
	/* Set ADCSRA ( ADC sample rate ) to 32 prescale = 4MHz/32 = 125KHz */
	ADCSRA |= (1 << ADPS2) | (1 << ADPS0);

	/* Set ADC reference voltage to AVCC */
	ADMUX = (1 << REFS0);

	/* Select Pin A7 */
	ADMUX |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

	/* Enable the ADC */
	ADCSRA |= 1<<ADEN | 0<<ADSC;

	sei();//Enable global interrupts


    unsigned int writeAddress = 0;
    unsigned int readAddress = 0;
    unsigned char data[3];
    unsigned int noteCount = 0;
	unsigned int notePlay = 0;
	uint16_t time = 0;
	float modifier;

    
	while(1){
		if(isRecordOn()){
		USART_Init(0x07);
			writeAddress = 0;
			noteCount = 0;
			TCNT1 = 0;
			USART_Flush();
            notePlay = 0;
            time =0;
            //reset address at start of record
            //record until eeprom is full or is switched off
            while((writeAddress<1022U) && isRecordOn()){
                for(unsigned int i=0;i<3;i++){
                    //store each of the three MIDI bytes in EEPROM	
					data[i] = USART_Receive();
                    EEPROM_Write(writeAddress,data[i]);
					writeAddress++;
					if(i==2){
						EEPROM_Write(1023,noteCount++);
						time=TCNT1;
						EEPROM_Write(writeAddress++,(time));
                    	EEPROM_Write(writeAddress++,time>>8);
						TCNT1 = 0;
					}
					//at the end of the midi signal add the timing info to the next two bytes in eeprom
                    PORTB = data[1];
        		}
    		}
		}

        if(isPlayOn()){
		    USART_Init(0x07);
		    readAddress = 0;
			notePlay=0;
			noteCount=EEPROM_Read(1023);
			USART_Flush();
			time = 0;
		
            while (notePlay < noteCount && isPlayOn()){
			    //read each of the three MIDI bytes from the EEPROM
				for(unsigned int j=0;j<3;j++){
                	data[j] = EEPROM_Read(readAddress);
					USART_Transmit(EEPROM_Read(readAddress));
					readAddress++;
                }
				//creating a 16 bit time value from 2 eeprom bytes (the second note)
				time = EEPROM_Read(readAddress+5)|(EEPROM_Read(readAddress+6)<<8);
				if(isModifyOn()){
					modifier = ADCRead(PINA & (1 << PA6));
					modifier = 2 * (modifier / 1024);
					time *= modifier;
				}	
					//delay by found time
					TCNT1 = 0;
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
