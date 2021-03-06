/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2011 Stephen Caudle <scaudle@doceme.com>
 * Copyright (C) 2012 Daniel Serpell <daniel.serpell@gmail.com>
 * Copyright (C) 2015 Piotr Esden-Tempski <piotr@esden.net>
 * Copyright (C) 2015 Chuck McManis <cmcmanis@mcmanis.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this librar(float)y.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <ctype.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/systick.h>
#include "clock.h"
#include "sdram.h"
#include "lcd.h"
#include <math.h>
#define PI 3.14159265

/* utility functions */
void uart_putc(char c);
int _write(int fd, char *ptr, int len);

static void gpio_setup(void)
{
	/* Setup GPIO pin GPIO13 on GPIO port G for LED. */
	rcc_periph_clock_enable(RCC_GPIOG);
	gpio_mode_setup(GPIOG, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO13);

	/* Setup GPIO A pins for the USART1 function */
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_USART1);
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9 | GPIO10);
	gpio_set_af(GPIOA, GPIO_AF7, GPIO9 | GPIO10);

	usart_set_baudrate(USART1, 115200);
	usart_set_databits(USART1, 8);
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_mode(USART1, USART_MODE_TX_RX);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
	usart_enable(USART1);
}

/* Maximum number of iterations for the escape-time calculation */
#define max_iter 32
uint16_t lcd_colors[] = {
	0x0,
	0x1f00,
	0x00f8,
	0xe007,
	0xff07,
	0x1ff8,
	0xe0ff,
	0xffff,
	0xc339,
	0x1f00 >> 1,
	0x00f8 >> 1,
	0xe007 >> 1,
	0xff07 >> 1,
	0x1ff8 >> 1,
	0xe0ff >> 1,
	0xffff >> 1,
	0xc339 >> 1,
	0x1f00 << 1,
	0x00f8 << 1,
	0x6007 << 1,
	0x6f07 << 1,
	0x1ff8 << 1,
	0x60ff << 1,
	0x6fff << 1,
	0x4339 << 1,
	0x1f00 ^ 0x6ac9,
	0x00f8 ^ 0x6ac9,
	0xe007 ^ 0x6ac9,
	0xff07 ^ 0x6ac9,
	0x1ff8 ^ 0x6ac9,
	0xe0ff ^ 0x6ac9,
	0xffff ^ 0x6ac9,
	0xc339 ^ 0x6ac9,
	0,
	0,
	0,
	0,
	0
};

void clear_screen(){
        int x, y;
        for (x = -120; x < 120; x++) {
                for (y = -160; y < 160; y++) {
                        lcd_draw_pixel(x+120, y+160, lcd_colors[0]);
                        // 0 negro,1 azul, 2 rojo, 3 verde 5 morado 6 amarillo, 7 blanco
                }
        }
}

void drawline(int px1, int py1, int px2, int py2){
        //240*320
        if (px1!=px2){
                float m = (py2-py1)/(px2-px1);
                int b=py1-m*px1;
                //chy = (px2-px1+1)/2;
                //chx = (py2-py1+1)/2;
                for(int x=px1;x<=px2; x++){
                        //y=mx+b
                        float y=m*x+b;
                        lcd_draw_pixel(x,(int) y, lcd_colors[7]);
                        if(y-(int)y!=0){
                                lcd_draw_pixel(x,((int) y)+1, lcd_colors[7]);
                        
                        }
                }
        }else{
                for(int i=py1; i<=py2; i++){
                        lcd_draw_pixel(px1, i, lcd_colors[7]);
                        
                        }
        }
        
}

void drawcircle2(int cx, int cy, int r){
        int x, y;
        for (x = -120; x < 120; x++) {
                for (y = -160; y < 160; y++) {
                        if(((x - cx) * (x - cx) + (y - cy) * (y - cy)) <= r * r){
                                lcd_draw_pixel(x+120, y+160, lcd_colors[7]);
                                // 0 negro,1 azul, 2 rojo, 3 verde 5 morado 6 amarillo, 7 blanco
                        }
                }
        }
}

void drawcircle(int cx, int cy, int r){
        //        ((x1 - start_X) * (x1 - start_X) + (y1 - start_Y) * (y1 - start_Y)) <= r * r        

        int x,y;
        double k=1/r;
        double a=k*180/PI;
        int i=(int)(360/a);
        
        for(int n=0;n<=i;n++){
                double tx=(double)r*cos(k*(n+1));
                double ty=(double)r*sin(k*(n+1));
                //                if(tx-(int)tx>=0.5){
                        x=cx+((int)tx)+1;
                        // }else{
                        // x=cx+(int)tx;
                        // }
                        //   if(ty-(int)ty>=0.5){
                        y=cy+((int)tx)+1;
                        // }else{
                        // y=cy+(int)ty;
                        // }
                
                lcd_draw_pixel(x,y,lcd_colors[7]);
                }
}

int main(void)
{
        /* Clock setup */
	clock_setup();
	/* USART and GPIO setup */
	gpio_setup();
	/* Enable the SDRAM attached to the board */
	sdram_init();
	/* Enable the LCD attached to the board */
	lcd_init();        
	printf("System initialized.\n");
        
        while(1){
                clear_screen();        
		/* Blink the LED (PG13) on the board with each fractal drawn. */
		gpio_toggle(GPIOG, GPIO13);		/* LED on/off */
                //lcd_draw_pixel(50,50,lcd_colors[7]);
                //drawline(100,0,100,319);              /* 7-White color*/
                drawcircle2(0,0,100);
                lcd_show_frame();
        }

	return 0;
}

/*
 * uart_putc
 *
 * This pushes a character into the transmit buffer for
 * the channel and turns on TX interrupts (which will fire
 * because initially the register will be empty.) If the
 * ISR sends out the last character it turns off the transmit
 * interrupt flag, so that it won't keep firing on an empty
 * transmit buffer.
 */
void uart_putc(char c) {

	while ((USART_SR(USART1) & USART_SR_TXE) == 0);
	USART_DR(USART1) = c;
}

/*
 * Called by libc stdio functions
 */
int
_write(int fd, char *ptr, int len)
{
	int i = 0;

	/*
	 * Write "len" of char from "ptr" to file id "fd"
	 * Return number of char written.
	 */
	if (fd > 2) {
		return -1;  /* STDOUT, STDIN, STDERR */
	}
	while (*ptr && (i < len)) {
		uart_putc(*ptr);
		if (*ptr == '\n') {
			uart_putc('\r');
		}
		i++;
		ptr++;
	}
	return i;
}

