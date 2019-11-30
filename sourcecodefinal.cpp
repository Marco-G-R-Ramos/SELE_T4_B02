#include <Arduino.h>
#define MASTER_ADDR 15 //1111
#define SLAVE1_ADDR 13 //1101
#define SLAVE2_ADDR 14 //1110
#define LED_PIN   13  // pino para LED's
#define REandDE   2   // pino para RE and DE do master transceiver
#define ADDR3_PIN 11  // pino para adress
#define ADDR2_PIN 10  // pino para adress
#define ADDR1_PIN 9   // pino para adress
#define ADDR0_PIN 8   // pino para adress
#define BUT1_PIN  7   // pino para botão
#define BUT2_PIN  6   // pino para botão
#define FOSC 1843200 // Clock Speed do atmega328
#define BAUD 9600    // baud rate escolhida
#define MYUBRR FOSC/16/BAUD - 1 

void asynch9_init(unsigned int ubrr) {// put your code here, to setup asynchronous serial communication using 9 bits:
  UBRR0H = (unsigned char)(ubrr>>8); //Set baud rate 
  UBRR0L = (unsigned char)ubrr;
  UCSR0A |= (1<<MPCM0);//Enable Multi processor communication mode -> começam todos com ele ligado
  UCSR0B = (1<<RXEN0)|(1<<TXEN0)|(1<<UCSZ02); //Enable receiving; enable receiving; set Zn2=1 (for 9 bit data)
  UCSR0C |= (1<<UCSZ01)|(1<<UCSZ00);//set Zn1=1 and Zn2=1 (for 9 bit data, in conjunction with Zn2=1)
}

void send_addr(uint8_t addr) { // put your code here, to send an address:
  while (!( UCSR0A & (1<<UDRE0))); //exits loop when register IS empty --> ready to write data
  UCSR0B |= (1<<TXB80); //9th bit = 1 --> meaning the data is an address
  UDR0 = addr; // writes the address itself in the data register
}

void send_data(uint8_t data) { // put your code here, to send a data byte:
  while (!( UCSR0A & (1<<UDRE0))); //while not register empty. again, it waits for empty register; for data currently there to be read
  UCSR0B &= ~(1<<TXB80); //9th bit = 0 
  UDR0 = data; // writes the data onto the data register
}

int get_data(void) { // put your code here, to receive a data byte using multi processor communication mode:
  unsigned char status, resh, resl;
  while (!(UCSR0A & (1<<RXC0))); // RXC0 is "Receive Complete" - is high when there is data to be read (is 0 while there is no data) -> the while loop runs while there is no data
  status = UCSR0A;   // stores UCSR0A register's values
  resh = UCSR0B;     // Register UCSR0B
  resl = UDR0;       // stores received data (read only after reading status)
  if (status & (((1<<FE0)|(1<<DOR0)) |(1<<UPE0)) )   //FE - Frame error; DOR - Data OverRun; UPE - Parity error
    return -1; // If any one of the errors occur, returns -1
  resh = ((resh >> 1) & 0x01); // "Filter the 9th bit, then return" - this gives the value of RXB8 - the received 9th bit)
  return ((resh << 8) | resl); //returns data + RXB8<<8 -> it's the data byte + the RXB8 as the MSB (9th bit)
}

void setup(){ // put your setup code here, to run once
  asynch9_init(MYUBRR);//runs the USART initiallizing functions
  pinMode(LED_PIN, OUTPUT); //internal led pin (13)
  pinMode(REandDE, OUTPUT); //REandDE enable pin para o Master Transciever
  pinMode(ADDR3_PIN, INPUT_PULLUP); //Address pins -> default as 1 (pullup)
  pinMode(ADDR2_PIN, INPUT_PULLUP); //Address pins -> default as 1 (pullup)
  pinMode(ADDR1_PIN, INPUT_PULLUP); //Address pins -> default as 1 (pullup)
  pinMode(ADDR0_PIN, INPUT_PULLUP); //Address pins -> default as 1 (pullup)
  pinMode(BUT1_PIN, INPUT_PULLUP); //Button pins -> default as 1   
  pinMode(BUT2_PIN, INPUT_PULLUP); //Button pins -> default as 1 
}

//initializes variables for holding the values of the buttons, address pins and data_bytes
int but1,but2,ad0,ad1,ad2,ad3,address,d_ata,ninth;
uint8_t b_yte;

void loop() { // put your main code here, to run repeatedly:
  but1=digitalRead(BUT1_PIN); //Ler os estados dos botões (pullup)
  but2=digitalRead(BUT2_PIN);
  ad0=digitalRead(ADDR0_PIN); //Ler os pinos de endereço
  ad1=digitalRead(ADDR1_PIN);
  ad2=digitalRead(ADDR2_PIN);
  ad3=digitalRead(ADDR3_PIN);
  address = (ad0<<0)|(ad1<<1)|(ad2<<2)|(ad3<<3);//definir uma variável com o valor do endereço
  
  ///////// Código dividido em 2 partes, uma para o MASTER e outra para os SLAVES 1 e 2.
  ///////// O código dos slaves é igual, a sua diferença é no address.

  if (address == MASTER_ADDR)
  {   ///////////  MASTER (no pins grounded)     ///////////
    digitalWrite(REandDE,HIGH); // ficará sempre desta maneira pois este é MASTER
    if (but1==0)
    {  //If we press button 1 (7), we send the address for SLAVE1 (13) and then send data (1 / 0x01) for turning on its LED
      send_addr(SLAVE1_ADDR);
      send_data(1); // ordem para ligar o LED
    }
    if (but2==0)
    {  //If we press button 2 (6), we send the address for SLAVE2 (14) and then send data (1 / 0x01) for turning on its LED
      send_addr(SLAVE2_ADDR);
      send_data(1); // ordem para ligar o LED
    }
  }//////////////////end of MASTER's code//////////////////

  else 
  {   ///////////      SLAVE     ////////// 
    digitalWrite(REandDE,LOW); // ficará sempre desta maneira pois este é o SLAVE
    d_ata = get_data(); // receives the 9 bits of the frame
    ninth = d_ata>>8; // the ninth bit (1 if address frame and 0 if data frame)
    b_yte = d_ata;    //gets rid of the ninth bit (b_yte initialized as uint8_t)
    if (ninth == 1)   //o nono bit é 1, logo é um address
    {//então podemos ler o byte e verificar se estamos a ser endereçados
      if (b_yte == address)
      { //O address enviado é o deste slave! --> desligamos o bit MPCM para aceitarmos dados que não são um endereço!
        UCSR0A &= ~(1<<MPCM0);//Disable Multi processor communication mode
      }
      else 
      {//O address enviado não é o deste slave --> voltamos a ligar MPCM
        UCSR0A |= (1<<MPCM0);//Enable Multi processor communication mode
      }
    }

    else 
    {//O nono bit é 0, logo estamos a receber dados (E o MPCM deve ter de estar a 0 no slave que queremos, para isto acontecer)
      if (b_yte == 1)
      {
        digitalWrite(LED_PIN,HIGH); delay(10); digitalWrite(LED_PIN,LOW);
      }
    }
  }
}    //////////////////end of SLAVES's code//////////////////