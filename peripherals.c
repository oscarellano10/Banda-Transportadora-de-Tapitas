#include "peripherals.h"
#include <stdio.h>
#include <stdbool.h>

volatile uint32_t system_ms = 0;

void LCD_Comando(unsigned char cmd){
    LCD_PORT &= 0xf0;
    LCD_TRIS &= 0xf0;
    LCD_PORT = (unsigned char)(LCD_PORT | ((cmd >> 4) & 0x0f));
    RW_PIN = 0; RS_PIN = 0;     
    __delay_us(50); 
    E_PIN = 1; __delay_us(50); E_PIN = 0;     
    
    LCD_PORT &= 0xf0;
    LCD_PORT = (unsigned char)(LCD_PORT | (cmd & 0x0f));
    __delay_us(50); 
    E_PIN = 1; __delay_us(50); E_PIN = 0;     
    LCD_TRIS |= 0x0f;
    __delay_ms(2); 
}

void LCD_Init(void){
    LCD_PORT = (unsigned char)(LCD_PORT & 0xf0);
    LCD_TRIS &= 0xf0; 
    RW_TRIS = 0; RS_TRIS = 0; E_TRIS = 0;
    RW_PIN = 0; RS_PIN = 0; E_PIN = 0;
    __delay_ms(100); 

    LCD_Comando(0x30); __delay_ms(5);
    LCD_Comando(0x30); __delay_us(100);
    LCD_Comando(0x32); __delay_us(100);
    LCD_Comando(0x28); LCD_Comando(0x08);      
    LCD_Comando(0x0f); LCD_Comando(0x01);      
    LCD_Comando(0x06); LCD_Comando(0x0C);      
}

void LCD_Data(unsigned char data){
    __delay_us(100);
    LCD_PORT &= 0xf0;
    LCD_TRIS &= 0xf0;
    LCD_PORT = (unsigned char)(LCD_PORT | ((data >> 4) & 0x0f));
    RW_PIN = 0; RS_PIN = 1;     
    __delay_us(50); 
    E_PIN = 1; __delay_us(50); E_PIN = 0;     
    LCD_PORT &= 0xf0;
    LCD_PORT = (unsigned char)(LCD_PORT | (data & 0x0f));
    __delay_us(50); 
    E_PIN = 1; __delay_us(50); E_PIN = 0;     
    LCD_TRIS |= 0x0f;
}

void LCD_XY(int x, int y){
    if(x > 0) LCD_Comando((unsigned char)(0xC0 + y));
    else LCD_Comando((unsigned char)(0x80 + y));
}

void LCD_Cadena(const char *dato){
    while(*dato){
        LCD_Data((unsigned char)*dato);
        dato++;
    }
}

void update_LCD(void) {
    char buffer[20];
    sprintf(buffer, "C:%d M:%d G:%d", small_cap, medium_cap, big_cap);
    LCD_Comando(0x01); 
    __delay_ms(2);
    LCD_XY(0,0);
    LCD_Cadena("Tapitas"); 
    LCD_XY(1,0);
    LCD_Cadena(buffer);
}

void LCD_Clear(void) {
    LCD_Comando(0x01); // The command to clear the screen
    __delay_ms(2);     // This command takes longer than others, so wait!
}

void configuraPWM(void){
    PR2 = 0xFF;          
    TRISCbits.RC1 = 0;   
    T2CONbits.T2CKPS = 1; 
    CCP2CONbits.CCP2M = 0x0F; 
    TMR2 = 0;
    T2CONbits.TMR2ON = 1;
}

void set_PWM_DutyCycle(int percent) {
    if(percent > 100) percent = 100;
    if(percent < 0) percent = 0;
    unsigned int duty = (unsigned int)(percent * 10.23);
    CCPR2L = (unsigned char)(duty >> 2);
    CCP2CONbits.DC2B = (unsigned char)(duty & 0x03);
}

typedef struct {
    int pin_num;
    uint32_t deadline;
    int target_angle;
    bool is_pending;
} ServoTask;

ServoTask servo_queue[32];

void __interrupt() High_ISR(void) {
    if (INTCONbits.TMR0IF) {
        // Reload for 1ms tick at 4MHz
        // 65536 - 1000 = 64536 (0xFC18)
        TMR0H = 0xFC;
        TMR0L = 0x18;
        
        system_ms++;
        INTCONbits.TMR0IF = 0;
    }
}

void Init_Timer0(void) {
    T0CON = 0x88; // 16-bit, No prescaler (1:1)
    TMR0H = 0xFC;
    TMR0L = 0x18;
    INTCONbits.TMR0IE = 1;
    INTCONbits.GIE = 1;
    INTCONbits.PEIE = 1;
}

void schedule_servo(int servo_id, int angle, uint32_t delay_ms) {
    for(int i = 0; i < 32; i++) {
        if(!servo_queue[i].is_pending) { // Find the first free spot
            servo_queue[i].pin_num = servo_id;
            servo_queue[i].target_angle = angle;
            servo_queue[i].deadline = system_ms + delay_ms;
            servo_queue[i].is_pending = true;
            return; // Exit once scheduled
        }
    }
}

void process_servos(void) {
    for(int i = 0; i < 32; i++) {
        if(servo_queue[i].is_pending && system_ms >= servo_queue[i].deadline) {
            servo_angle(servo_queue[i].target_angle, servo_queue[i].pin_num);
            servo_queue[i].is_pending = false;
        }
    }
}