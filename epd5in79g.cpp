/**
 *  @filename   :   epd2in66g.cpp
 *  @brief      :   Implements for e-paper library
 *  @author     :   Waveshare
 *
 *  Copyright (C) Waveshare     2022/08/17
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include "epd5in79g.h"

Epd::~Epd() {
};

Epd::Epd() {
    reset_pin = RST_PIN;
    dc_pin = DC_PIN;
    cs_pin = CS_PIN;
    busy_pin = BUSY_PIN;
    WIDTH = EPD_WIDTH;
    HEIGHT = EPD_HEIGHT;
};

int Epd::Init() {
    /* this calls the peripheral hardware interface, see epdif */
    if (IfInit() != 0) {
        return -1;
    }
    Reset();

    SendCommand(0xA2);	//********************
    SendData(0x01);

    SendCommand(0x00);	//0x00
    SendData(0x03);	
    SendData(0x29);

    SendCommand(0xA2);	//********************
    SendData(0x02);	

    SendCommand(0x00);	//0x00
    SendData(0x07);	
    SendData(0x29);

    SendCommand(0xA2);	//********************
    SendData(0x00);	

    SendCommand(0x50);	//
    SendData(0x97);	

    SendCommand(0x61); //0x61	
    SendData(0x01);	
    SendData(0x8c);	
    SendData(0x01);	
    SendData(0x10); 	

    SendCommand(0x06);	//0x06
    SendData(0x38);	
    SendData(0x38);
    SendData(0x38);
    SendData(0x00);	//////////////////////////////////////////

    SendCommand(0xE9);	//0xE0
    SendData(0x01);

    SendCommand(0xE0);	//0xE0
    SendData(0x01);

    SendCommand(0x04);
    ReadBusyH();  //while(1);	
    return 0;
}

/**
 *  @brief: basic function for sending commands
 */
void Epd::SendCommand(unsigned char command) {
    DigitalWrite(dc_pin, LOW);
    SpiTransfer(command);
}

/**
 *  @brief: basic function for sending data
 */
void Epd::SendData(unsigned char data) {
    DigitalWrite(dc_pin, HIGH);
    SpiTransfer(data);
}

/**
 *  @brief: Wait until the busy_pin goes LOW
 */
void Epd::ReadBusyH(void) {
    Serial.print("e-Paper busy H\r\n ");
    while(DigitalRead(busy_pin) == LOW) {      //LOW: busy, HIGH: idle
        DelayMs(5);
    } 
    Serial.print("e-Paper busy release H\r\n ");    
}

void Epd::ReadBusyL(void) {
    Serial.print("e-Paper busy L\r\n ");
    while(DigitalRead(busy_pin) == HIGH) {      //LOW: idle, HIGH: busy
        DelayMs(5);
    }      
    Serial.print("e-Paper busy release L\r\n ");
}

/**
 *  @brief: module reset.
 *          often used to awaken the module in deep sleep,
 *          see Epd::Sleep();
 */
void Epd::Reset(void) {
    DigitalWrite(reset_pin, HIGH);
    DelayMs(20);    
    DigitalWrite(reset_pin, LOW);                //module reset    
    DelayMs(2);
    DigitalWrite(reset_pin, HIGH);
    DelayMs(20);     
}

/******************************************************************************
function :	Turn On Display
parameter:
******************************************************************************/
void Epd::TurnOnDisplay(void)
{
    SendCommand(0xA2); 
    SendData(0x00);

    SendCommand(0x12); 
    SendData(0x00);
    ReadBusyH();
}

/******************************************************************************
function :	Clear screen
parameter:
******************************************************************************/
void Epd::Clear(UBYTE color)
{
    // rightmost part
    UWORD Width, Height;
    Width = (WIDTH % 8 == 0)? (WIDTH / 8 ): (WIDTH / 8 + 1); // 2 bits per pixel -> 4 pixels per byte, and WIDTH/2 pixels per IC. thus /8 -> bytes per IC
    Height = HEIGHT;


    SendCommand(0xA2);	//********************
    SendData(0x01);
    SendCommand(0x10);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            SendData((color<<6) | (color<<4) | (color<<2) | color); // set 4 pixels with 4x2bits
        }
    }

   // leftmost part

    SendCommand(0xA2);	//********************
    SendData(0x02);
    SendCommand(0x10);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            SendData((color<<6) | (color<<4) | (color<<2) | color);
        }
    }

    TurnOnDisplay();
}
//https://www.waveshare.com/wiki/5.79inch_e-Paper_Module_(G)_Manual#Overview

/******************************************************************************
function :	Sends the image buffer in RAM to e-Paper and displays
parameter:
******************************************************************************/
void Epd::Display(UBYTE *Image)
{
    UWORD Width, Height, Width1; //WIDTH=792, HEIGHT=272. 396 columns per IC
    Width1 = (WIDTH % 4 == 0)? (WIDTH / 4 ): (WIDTH / 4 + 1); //792/4 = 198 bytes per IC, since one byte is four pixels
    Width =(WIDTH % 8 == 0)?(WIDTH / 8 ):(WIDTH / 8 + 1); // Width=792/8=99 bytes per IC, because each IC has 396 columns
    Height = HEIGHT; // Well, it is what it is

    SendCommand(0xA2);	//********************
    SendData(0x01); // Chipselect two?
    SendCommand(0x10);
    for (UWORD j = 0; j < Height; j++) { // loop over whole height. We can safely do Height/2 it seems
        for (UWORD i = 0; i < Width; i++) {
            SendData(pgm_read_byte(&Image[j * Width1 + i + Width])); //j * 198 + i + 99
        }

        for (UWORD i = 0; i < Width; i++) {
            SendData(pgm_read_byte(&Image[(Height - j - 1) * Width1 + i + Width])); // (272-j-1) * 198 + i + 99
        }
    }

    // We can relatively easily split this in two. First serve rightmost part of image, then leftmost (remove +Width)


    SendCommand(0xA2);  //********************
    SendData(0x02); // Chipselect 1
    SendCommand(0x10);
    
    for (UWORD j = 0; j < Height/2; j++) { // loop up/down until half the height

        // do it in halves because thats how the master slave system works?
        for (UWORD i = 0; i < Width; i++) {
            SendData(pgm_read_byte(&Image[i + j * Width1])); // i + (j*198) send data for each col in that bar height
        }

        for (UWORD i = 0; i < Width; i++) {
            SendData(pgm_read_byte(&Image[i + (Height - j - 1) * Width1])); // i + (272-j-1) * 198 from the top of the screen, do the same. work towards the middle
        } 
    }
    
    TurnOnDisplay();
}

// Display on a single Chip/half
void Epd::DisplayH(UBYTE cs, UBYTE *Image)
{
    UWORD Width, Height, Width1; //WIDTH=792, HEIGHT=272. 396 columns per IC
    Width1 = (WIDTH % 4 == 0)? (WIDTH / 4 ): (WIDTH / 4 + 1); //792/4 = 198 bytes per IC, since one byte is four pixels
    Width =(WIDTH % 8 == 0)?(WIDTH / 8 ):(WIDTH / 8 + 1); // Width=792/8=99 bytes per IC, because each IC has 396 columns
    Height = HEIGHT; // Well, it is what it is

    // We can relatively easily split this in two. First serve rightmost part of image, then leftmost (remove +Width)


    SendCommand(0xA2);  //********************
    SendData(cs); // Chipselect 1
    SendCommand(0x10);
    
    for (UWORD j = 0; j < Height/2; j++) { // loop up/down until half the height

        // do it in halves because thats how the master slave system works?
        // problem: whole image already goes into this top loop, leaving no data for the one below. (which right now is just the same bytes, so that works)
        for (UWORD i = 0; i < Width; i++) {
            SendData(Image[i + j * Width]); // i + (j*198) send data for each col in that bar height
        }
  
        for (UWORD i = 0; i < Width; i++) {
            SendData(Image[i + (Height - j - 1) * Width]); // i + (272-j-1) * 198 from the top of the screen, do the same. work towards the middle
        } 
    }
    
//    TurnOnDisplay();
}

void Epd::Display_part(const UBYTE *Image, UWORD xstart, UWORD ystart, UWORD image_width, UWORD image_height)
{
    UWORD Width, Width1, Width2, Height, i, j, xend, yend;
    Width = (WIDTH % 8 == 0)? (WIDTH / 8 ): (WIDTH / 8 + 1);
    Height = HEIGHT;
    xend = xstart + image_width;
    yend = ystart + image_height;

    if(xstart>395)
    {
        xstart = xstart - 396 ;
        xend = xstart + image_width;
        SendCommand(0xA2);
        SendData(0x01);
        SendCommand(0x10);
        for (UWORD j = 0; j < Height/2; j++) {
            for (UWORD i = 0; i < Width; i++) {
                if(j >= ystart && i < xend/4 && i >= xstart/4)
                    SendData(pgm_read_byte(&Image[(j - ystart) * image_width/4 + i - xstart/4]));
                else
                    SendData(0x55);
            }
            for (UWORD i = 0; i < Width; i++) {
                if((Height - j) < yend && i < xend/4 && i >= xstart/4)
                    SendData(pgm_read_byte(&Image[(yend - (Height - j)) * image_width/4 + i- xstart/4]));
                else
                    SendData(0x55);
            } 
        }

        SendCommand(0xA2);
        SendData(0x02);
        SendCommand(0x10);
        for (UWORD j = 0; j < Height; j++) {
            for (UWORD i = 0; i < Width; i++) {
                    SendData(0x55);
            }
        }
    }
    else if(xend<396)
    {
        xend = xstart + image_width;
        SendCommand(0xA2);
        SendData(0x01);
        SendCommand(0x10);
        for (UWORD j = 0; j < Height; j++) {
            for (UWORD i = 0; i < Width; i++) {
                    SendData(0x55);
            }
        }

        SendCommand(0xA2);
        SendData(0x02);
        SendCommand(0x10);
        for (UWORD j = 0; j < Height/2; j++) {
            for (UWORD i = 0; i < Width; i++) {
                if(j >= ystart && i < xend/4 && i >= xstart/4)
                    SendData(pgm_read_byte(&Image[(j - ystart) * image_width/4 + i - xstart/4]));
                else
                    SendData(0x55);
            }
            for (UWORD i = 0; i < Width; i++) {
                if((Height - j) < yend && i < xend/4 && i >= xstart/4)
                    SendData(pgm_read_byte(&Image[(yend - (Height - j)) * image_width/4 + i- xstart/4]));
                else
                    SendData(0x55);
            } 
        }

    }
    else
    {
        xend = xstart + image_width;
        Width1 = ((395 - xstart) % 4 == 0)?((395 - xstart) / 4 ):((395 - xstart) / 4 + 1);
        Width2 = ((xend - 396) % 4 == 0)?((xend - 396) / 4 ):((xend - 396) / 4 + 1);
        SendCommand(0xA2);
        SendData(0x01);
        SendCommand(0x10);
        for (UWORD j = 0; j < Height/2; j++) {
            for (UWORD i = 0; i < Width; i++) {
                if(j >= ystart && i < Width2)
                    SendData(pgm_read_byte(&Image[(j - ystart) * image_width/4 + i + Width1]));
                else
                    SendData(0x55);
            }
            for (UWORD i = 0; i < Width; i++) {
                if((Height - j) < yend && i < Width2)
                    SendData(pgm_read_byte(&Image[(yend - (Height - j)) * image_width/4 + i + Width1]));
                else
                    SendData(0x55);
            } 
        }

        SendCommand(0xA2);
        SendData(0x02);
        SendCommand(0x10);
        for (UWORD j = 0; j < Height/2; j++) {
            for (UWORD i = 0; i < Width; i++) {
                if(j >= ystart && i >= xstart/4)
                    SendData(pgm_read_byte(&Image[(j - ystart) * image_width/4 + i - xstart/4]));
                else
                    SendData(0x55);
            }
            for (UWORD i = 0; i < Width; i++) {
                if((Height - j) < yend && i >= xstart/4)
                    SendData(pgm_read_byte(&Image[(yend - (Height - j)) * image_width/4 + i- xstart/4]));
                else
                    SendData(0x55);
            } 
        }
    }
    TurnOnDisplay();
}

/******************************************************************************
function :	Enter sleep mode
parameter:
******************************************************************************/
void Epd::Sleep(void)
{
    // SendCommand(0x02); // POWER_OFF
    // SendData(0X00);
    // ReadBusyH();
    
    SendCommand(0x07); // DEEP_SLEEP
    SendData(0XA5);
}

/* END OF FILE */
