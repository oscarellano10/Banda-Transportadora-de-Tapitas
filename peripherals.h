/* 
 * File:   peripherals.h
 * Author: edgep
 *
 * Created on June 5, 2026, 1:23 AM
 */

#ifndef PERIPHERALS_H
#define	PERIPHERALS_H

#include <xc.h>
#include <stdbool.h>

// --- CONSTANTS ---
#define _XTAL_FREQ 4000000 // 4 MHz
#define LCD_PORT LATD
#define LCD_TRIS TRISD
#define RW_PIN LATDbits.LATD6
#define RW_TRIS TRISDbits.RD6
#define RS_PIN LATDbits.LATD5
#define RS_TRIS TRISDbits.RD5
#define E_PIN   LATDbits.LATD4
#define E_TRIS TRISDbits.RD4

// --- EXTERNAL VARIABLES ---
// Sharing these variables between main.c and peripherals.c
extern int small_cap;
extern int medium_cap;
extern int big_cap;
extern volatile uint32_t system_ms;

// --- FUNCTION PROTOTYPES ---
void LCD_Comando(unsigned char cmd);
void LCD_Init(void);
void LCD_Data(unsigned char data);
void LCD_XY(int x, int y);
void LCD_Cadena(const char *dato);
void update_LCD(void);
void LCD_Clear(void);
void configuraPWM(void);
void set_PWM_DutyCycle(int percent);

void Init_Timer0(void);           
void process_servos(void);        
void schedule_servo(int servo_id, int angle, uint32_t delay_ms);
void servo_angle(int angle, int servo_num);

#endif