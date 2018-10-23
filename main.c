#include<io.h>
#include<util/delay.h>
#include<stdio.h>
 
void USART_Init(unsigned int baud){
    //Assign upper part of baud number (bits 8 to 11)
    UBRRH = (unsigned char)(baud>>8);
 
    //Assign remaining baud number
    UBRRL = (unsigned char)baud;
 
    //Enable receiver and transmitter
    UCSRB = (1<<RXEN)|(1<<TXEN);
 
    //Initialize to 8 data bits 1 stop bit
    UCSRC = (1<<URSEL)|(0<<USBS)|(3<<UCSZ0);
}
 
int isRecordOn(){
    uint8_t temp = PINA & 0x01;
 
    if(temp == 0x01){
        return 1;
    }
 
    else {
        return 0;
    }
 
    return -1;
}
 
int isPlayOn(void){
    uint8_t temp = PINA & 0x02;
 
    if(temp == 0x02){
        PORTB = 2;
        return 1;
    }
 
    else{
        PORTB = 0;
        return 0;
    }
 
    return -1;
}
 
int isModifyOn(){
    uint8_t temp = PINA & 0x04;
 
    if(temp == 0x04){
        PORTB = 4;
        return 1;
    }
 
    else{
        PORTB = 0;
        return 0;
    }
 
    return -1;
}
 
unsigned int USART_Receive(void){
    //Wait for available data
    while(!(UCSRA & (1<<RXC))){}
 
    //Make data available in buffer
    return UDR;
}
 
unsigned int USART_Transmit(unsigned int data){
    //Wait for transmitter
    while(!(UCSRA & (1<<UDRE))){}
 
    //Write data to USART buffer
    UDR=data;
}
 
int main(void){
    UBRRH=0x00;
    UBRRL=0x07;
    USART_Init(UBRRL);
    DDRA = 0x00; //Make all ports input
    DDRB=0xFF;   //Make all ports output
    PORTB =0x00;
 
   // while(1){
       
   // }
    while(1){
     if(isRecordOn()){
        PORTB = USART_Receive();
     };
      //isPlayOn();
      //isModifyOn();
    }
}
