#include "STM8L052C6.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "display.h"
#include "motor.h"

#define UART_BUFFER_SIZE 32

#define MP1 (1 << 2)

uint16_t count = 0;
bool motorDir = false;

uint8_t uartBufferPos = 0;
char uartBuffer[UART_BUFFER_SIZE];
volatile uint32_t tim2_millis = 0;

uint32_t millis() {
  return tim2_millis;
}

void delay_ms(uint32_t ms) {
    uint32_t start = millis();
    while ((millis() - start) < ms);
}

void initTIM2() {
    // 16000 ticks
    TIM2_ARRH = 0x3E;
    TIM2_ARRL = 0x80;
    //TIM2_PSCR = 0b010;

    TIM2_IER |= 0x1; // Update interrupt
    TIM2_CR1 = 0x1; // enable timer
}

void initUSART() {
    SYSCFG_RMPCR1 |= 0x10; // TX: PA2, RX: PA3
    PA_DDR |= MP1;
    PA_CR1 |= MP1;

    USART1_CR2 = USART_CR2_TEN | USART_CR2_REN | USART_CR2_RIEN; // Transmit, receive, interrrupt enable
    USART1_CR3 &= ~(USART_CR3_STOP1 | USART_CR3_STOP2); // 1 stop bit
    USART1_BRR1 = 0x11; USART1_BRR2 = 0x6; // 57600 baud (use 8 B for 115200)
}

int uartWrite(const char *str) {
	char i;
	for(i = 0; i < strlen(str); i++) {
		while(!(USART1_SR & USART_SR_TXE));
		USART1_DR = str[i];
	}
	return(i); // Bytes sent
}

int putchar(int data) {
    USART1_DR = data;
    while (!(USART1_SR & USART_SR_TC));
    return 0;
}

char uartRead() {
    if(USART1_SR & USART_SR_RXNE) {
        return USART1_DR;
    } else {
        return '\0';
    }
}

void clearUartBuffer() {
    uartBufferPos = 0;
    memset(uartBuffer,0,UART_BUFFER_SIZE);
}

void initClock() {
    CLK_CKDIVR = 0; // Set the frequency to 16 MHz
    CLK_PCKENR1 = 0xFF; // Enable peripherals
    CLK_PCKENR2 = 0xFF;
}

int main() {
    initClock();
    initTIM2();
    enableInterrupts()
    initUSART();
    initDisplay();
    initMotor();

    struct DisplayData display = {0};
    display.timegrid = 1;
    display.sun = 1;
    display.moon = 1;
    display.window = 1;
    display.vacation = 1;
    display.degrees = 1;
    display.percent = 1;
    display.battery = 1;
    display.automatic = 1;
    display.manual = 1;
    display.colon = 1;
    display.point1 = 1;
    display.point2 = 1;
    display.point3 = 1;
    display.weekdays = 1;
    display.bargraph = 0x1;

    display.num1segments = charSegments('T');
    display.num2segments = charSegments('E');
    display.num3segments = charSegments('S');
    display.num4segments = charSegments('T');

    setDisplay(&display);

    printf("Thermostat: Startup complete\n");

    while(true) {
        uint16_t adc, t;

        delay_ms(1000);

        if(display.bargraph == ((uint32_t)1 << 23))
            display.bargraph = 1;
        else
            display.bargraph = (display.bargraph << 1);

        if(display.weekdays == (1 << 6))
            display.weekdays = 1;
        else
            display.weekdays = (display.weekdays << 1);
        setDisplay(&display);

        if (uartBuffer[0] == 'b') {
            motorDir = false;
            memmove(uartBuffer, uartBuffer+1, strlen(uartBuffer));
            count = atoi(uartBuffer);
        } else if (uartBuffer[0] == 'f') {
            motorDir = true;
            memmove(uartBuffer, uartBuffer+1, strlen(uartBuffer));
            count = atoi(uartBuffer);
        }
        clearUartBuffer();

        adc = readADC();
        t = tim2_millis/100;
        printf("ADC value: %d, Time: %u\n", readADC(), (uint16_t)(millis()/100));

        if (count > 0) {
            printf("Running %s for %d.%d sec\n", (motorDir? "forward" : "backward"), count/10, count%10);
            setMotor(motorDir, !motorDir);
            count--;
        } else {
            setMotor(0, 0);
        }
    }
}

#define UART_RECV_ISR 28
#define TIM2_OVF_ISR 19

void uart_isr() __interrupt(UART_RECV_ISR) {
    uint8_t i;
    uartBufferPos %= UART_BUFFER_SIZE-1;
    for(i = uartBufferPos; i < UART_BUFFER_SIZE; i++) {
        uartBuffer[i] = uartRead();
    }
    uartBufferPos++;
}

void tim2_isr() __interrupt(TIM2_OVF_ISR) {
    TIM2_SR1 = 0;
    tim2_millis++;
}