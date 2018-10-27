#include <util/delay.h>
#include <stdio.h>
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

unsigned char EEPROM_Read(unsigned int address)
{
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


unsigned int readTCNT1(void){
    unsigned char sreg;
    unsigned int i;

    //Save global interrupt flag
    sreg = SREG;

    //Disable interuppts
    //_CLI();

    //Read TCNT1 into i
    i = TCNT1;

    //Restore global interuppt flag
    SREG = sreg;

    return i;
}

int main(void){
    //set baud rate
    unsigned int baud = 0x07;
    USART_Init(baud);
    USART_Flush();

    DDRA = 0x00;            //Make all ports input
    DDRB = 0xFF;            //Make all ports output
    PORTB = 0x00;
    TCCR1B |= (1 << CS11);  //Prescaler = 8
    TCNT1 = 0;              //Clear count
    TIMSK = (1 << TOIE1) ;  //Enable timer1 overflow interrupt(TOIE1)
    sei();                  //Enable global interrupts

    unsigned int writeAddress = 0;
    unsigned int readAddress = 0;
    unsigned char data[8];
    unsigned int noteCount = 0;

    while(1){

        if(isRecordOn()){
/*
        while((writeAdress<1022 && isRecordOn()){
            for(int i = 0; i < 6; i++){
                if(i==1){
                    if(writeAddress > 1){
                        time = TCNT1;
                        EEPROM_Write(writeAddress, (time>>8));
                        writeAddress++;
                        EEPROM_Write(writeAddress, time);
                        writeAddress++;
                    }
                TCTN1 = 0;
                PORTB=data;
                }
            EEPROM_Write(writeAddress, data);
            EEPROM_Write((1022), (address>>8));
            EEPROM_Write((1023), address);
 
            writeAddress++;
            }
        noteCount++;
        }
    */
            //reset address at start of record
            //record until eeprom is full or is switched off
            unsigned int time;
            while((writeAddress<1024U) && isRecordOn()){
                for(unsigned int i=0;i<6;i++){



                    //store each of the six MIDI bytes in EEPROM
                    data[i] = USART_Receive();
                    EEPROM_Write(writeAddress,data[i]);


                    //Store time data

                    if(i==1){
                        EEPROM_Write(writeAddress++,(time >> 8));
                        EEPROM_Write(writeAddress++,time);

                    }

                    if(i==5){
                        //data[7] = readTCNT1();
                        EEPROM_Write(writeAddress++,TCNT1);
                        time = TCNT1;
                        //TCNT1 = 0;
                    }


                    PORTB = data[1];
                }
                TCNT1 = 0;
            }

            PORTB = 0;
        }

        if(isPlayOn()){
            unsigned char startTime;
            unsigned char endTime;

            while (readAddress<1024U && isPlayOn()){
                for(unsigned int j=0;j<6;j++){
                    USART_Transmit(EEPROM_Read(readAddress));
                    readAddress++;
                }

                startTime = EEPROM_Read(readAddress++);
                endTime = EEPROM_Read(readAddress++);
                //printf("%s", startTime);
                if(startTime < endTime){
                    _delay_ms(endTime - startTime);
                }else{
                    _delay_ms(startTime - endTime);
                }
                PORTB = EEPROM_Read(readAddress + 1);
            }
            readAddress = 0;
        }
    }
}