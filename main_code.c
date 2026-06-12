#include <xc.h>
#include <stdarg.h>
#include <stdio.h>
#include "peripherals.h"

// CONFIGURATION BITS
#pragma config PLLDIV = 1, CPUDIV = OSC1_PLL2, USBDIV = 1       
#pragma config FOSC = HS, FCMEN = OFF, IESO = OFF       
#pragma config PWRT = OFF, BOR = OFF, BORV = 3         
#pragma config VREGEN = OFF, WDT = OFF, WDTPS = 32768    
#pragma config CCP2MX = ON, PBADEN = OFF, LPT1OSC = OFF    
#pragma config MCLRE = ON, STVREN = ON, LVP = OFF        
#pragma config ICPRT = OFF, XINST = OFF      

// Global Variables
int small_cap = 0;
int medium_cap = 0;
int big_cap = 0;

int small_cap_diam = 20;
int medium_cap_diam = 25;
int big_cap_diam = 59;
int cap_threshhold = 2;

// Motor Variables
int prev_accel = 0;
int prev_decel = 0;
int motor_speed = 0;
int change = 0;

// Servo Ports
#define servo1 RB0 // Servo that controls centerer
#define servo2 RB1 // Servo that controls centerer
#define servo3 RB2
#define servo4 RB3
#define servo_cycle 3

// Button Ports
#define button1 RA1 // Accelerate button
#define button2 RA2 // Decelerate button
#define button3 RA3 // Button that shows either linear velocity or caps
#define button4 RA4 // TBD
#define button5 RA5 // Emergency Button

// Laser Ports
#define laser1 RB4 // Laser that measures diameter
#define laser2 RB5 // Laser that measures height difference
int linear_vel = 0;

// LED Ports
#define LED1 RC0 // RED LED
#define LED2 RC2 // YELLOW LED
#define LED3 RC6 // GREEN LIGHT

#define BUZZER_PIN RC7

// LCD Logic
int change2 = 0;
int prev_switch_state;

// NEW VARIABLES FOR NON-BLOCKING LASER TRACKING
int last_laser_state = 0;
int cap_timer_cs = 0;

void Init_Ports() {
    
    LCD_Init();
    Init_Timer0();

    ADCON1 = 0x0F; // Sets all pins to digital mode
    
    // TRISA = 0b00111110; // Sets RA1, RA2, RA3, RA4, RA5 as inputs
    // Or just set the whole port to input to be safe:
    TRISA = 0xFF; 

    // Set RB0-RB3 as outputs (Servos) and RB4-RB5 as inputs (Lasers)
    // TRISB bits: 7 6 5 4 3 2 1 0
    // Binary:    0 0 1 1 0 0 0 0  -> 0x30
    TRISB = 0x30; 
    
    TRISC = 0x00; // Sets all pins on Port C to outputs (LEDs)
}

void LCD_write(const char *format, ...) {
    
    /*
    This function uses the logic of the functions in the peripherals. But all
    it does is it supplies a simple write command to the user. This is helpful
    to visualize bugs, problems, etc.
    */
    
    char buffer[20];    // A small array to hold the text
    va_list args;       // List for the "..." arguments
    
    va_start(args, format);
    vsprintf(buffer, format, args); // Formats the variables into the buffer
    va_end(args);

    LCD_Clear();        // Wipes the screen
    LCD_XY(0, 0);       // Goes to the start
    LCD_Cadena(buffer); // Sends the final string to the LCD
    
}

void LCD_display() {
    // 'static' variables only initialize ONCE and remember their value
    static int prev_switch_state = -1; // -1 ensures it updates on the first run
    static int display_mode = 0;       // 0 = Caps, 1 = Speed
    
    int current_switch = button3;

    // Trigger update if:
    // 1. The switch was flipped (Physical change)
    // 2. Motor_Speed() set the 'change2' flag (Value change)
    if (current_switch != prev_switch_state || change2 == 1) {
        
        // If the switch moved, update the mode
        if (current_switch != prev_switch_state) {
            if (display_mode == 0) display_mode = 1;
            else display_mode = 0;
            
            prev_switch_state = current_switch;
        }

        // Now write the correct screen
        if (display_mode == 1) {
            LCD_write("Speed: %d mm/s", linear_vel);
        } else {
            update_LCD();
        }

        change2 = 0; // Reset the speed change flag
        __delay_ms(50); // Debounce protection
    }
}

void Motor_Speed () {
    
    /*
    Function that reads the motor button input and switches. 
    Motor Speed must be from -1 to 1. A speed of 0.5 indicates a
    duty cycle of 50%, meaning motor will be turned on at half of
    the time. Meaning the motor will spin at half speed.
    */
    
    int cur_acel = button1;
    int cur_decel = button2;
    change = 0;           // Logic to change LCD display

    // Detect any change in the Accelerate switch (Rising or Trailing Edge)
    if (cur_acel != prev_accel) { 
    
        if (motor_speed == 0) {
            motor_speed = 75;
            change = 1;
        } 
        else if (motor_speed == 75) {
            motor_speed = 87; // Next step (using 87 for int logic)
            change = 1;
        }
        else if (motor_speed == 87) {
            motor_speed = 100;
            change = 1;
        }
        
        prev_accel = cur_acel;
        __delay_ms(200);      
    
    } 
    // Detect any change in the Decelerate switch (Rising or Trailing Edge)
    else if (cur_decel != prev_decel) { 
    
        if (motor_speed == 100) {
            motor_speed = 87;
            change = 1;
        } 
        else if (motor_speed == 87) {
            motor_speed = 75;
            change = 1;
        }
        else if (motor_speed == 75) {
            motor_speed = 0;
            change = 1;
        }
        
        prev_decel = cur_decel; // Update state after edge is detected
        __delay_ms(200);        // Debounce protection
    
    }
    
    // Apply changes to PWM and Linear Velocity if an edge was triggered
    if (change == 1) {
        
        set_PWM_DutyCycle(motor_speed);
        
        if (motor_speed == 0) {
            linear_vel = 0;
        } else if (motor_speed == 75) {
            linear_vel = 84;
        } else if (motor_speed == 87) {
            linear_vel = 65;
        } else if (motor_speed == 100) {
            linear_vel = 75;
        }
        
        change2 = 1; // Signal LCD_display to refresh the screen
    }

}

void buzzer() {

    for (int i = 0; i < 500; i++) {
    
        BUZZER_PIN = 1;
        __delay_us(500);
        BUZZER_PIN = 0;
        __delay_us(500);
    
    }

}

void servo_angle (int angle, int servo_num) {

    /* Code that is the Arduino equivalent to servo.write. This code
    will bring the servo to the required angle. It does this by creating
    a 50 Hz signal that the servo uses. In theory, the pulse must last
    from 1 ms to 2 ms, but experimenting, we found that it's closer to
    0.5 ms to 2.5 ms. 
    */
    
    for (int i = 0; i < servo_cycle; i+= 1) {
    
        // Logic to choose and control servo independently //
        if (servo_num == 1) {
            servo1 = 1;
        } else if (servo_num == 2) {
            servo2 = 1;
        } else if (servo_num == 3) {
            servo3 = 1;
        } else if (servo_num == 4) {
            servo4 = 1;
        }
        
        /*
        Depending on the angle given, we must set a specific delay for the
        duty on cycle. We tried to do a more intelligent way instead of a huge
        switch condition, but after many tries, this way is just more effective.
        Since we cant put a variable in __delay_us(). 
        */
        
        switch(angle) {
            case 0:   __delay_us(500);  break;
            case 5:   __delay_us(555);  break;
            case 10:  __delay_us(611);  break;
            case 15:  __delay_us(666);  break;
            case 20:  __delay_us(722);  break;
            case 25:  __delay_us(777);  break;
            case 30:  __delay_us(833);  break;
            case 35:  __delay_us(888);  break;
            case 40:  __delay_us(944);  break;
            case 45:  __delay_us(1000); break;
            case 50:  __delay_us(1055); break;
            case 55:  __delay_us(1111); break;
            case 60:  __delay_us(1166); break;
            case 65:  __delay_us(1222); break;
            case 70:  __delay_us(1277); break;
            case 75:  __delay_us(1333); break;
            case 80:  __delay_us(1388); break;
            case 85:  __delay_us(1444); break;
            case 90:  __delay_us(1500); break;
            case 95:  __delay_us(1555); break;
            case 100: __delay_us(1611); break;
            case 105: __delay_us(1666); break;
            case 110: __delay_us(1722); break;
            case 115: __delay_us(1777); break;
            case 120: __delay_us(1833); break;
            case 125: __delay_us(1888); break;
            case 130: __delay_us(1944); break;
            case 135: __delay_us(2000); break;
            case 140: __delay_us(2055); break;
            case 145: __delay_us(2111); break;
            case 150: __delay_us(2166); break;
            case 155: __delay_us(2222); break;
            case 160: __delay_us(2277); break;
            case 165: __delay_us(2333); break;
            case 170: __delay_us(2388); break;
            case 175: __delay_us(2444); break;
            case 180: __delay_us(2500); break;
            default:  __delay_us(1500); break; 
        }
        
        // Turn off all servos
        servo1 = 0;
        servo2 = 0;
        servo3 = 0;
        servo4 = 0;
        
        __delay_ms(18); // Off duty cycle for the servos
    
    }

}

void servo_open() {

    /*
     This function is just the open and close sequency for the first servos,
     so that the bottle cap will always be centered.
    */
    
    for (int angle = 100; angle > 0; angle -= 20) {
        
        servo_angle(angle, 1);
        servo_angle(angle, 2);
        servo_angle(angle, 3);
        servo_angle(angle, 4);
        
        for (int delay = 0; delay < 500; delay += 25) {
        
            __delay_ms(25);
            Motor_Speed();
        
        }

    }

}

void servo_classify(int servo_num, int open_angle, int close_angle, int delay_open, int delay_close) {
    
    schedule_servo(servo_num, open_angle, delay_open);
    schedule_servo(servo_num, close_angle, delay_close);
    
}

void cap_classifier(int cap_diameter) {

    /*
     Open Servo Angles
    servo_angle(45, 1);
    servo_angle(135, 2);
    servo_angle(45, 3);
    servo_angle(135, 4);
    
    Closed Servo Angles
    servo_angle(135, 1);
    servo_angle(45, 2);
    servo_angle(135, 3);
    servo_angle(45, 4);
    */
    
    int valid_cap = 0;

    if (cap_diameter >= 20 && cap_diameter <= 23) {
        
        small_cap += 1;
        servo_classify(3, 135, 45 , 2100, 3400);
        valid_cap = 1;
        
    } 
    
    else if (cap_diameter >= 24 && cap_diameter <= 28) {
        
        medium_cap += 1;
        servo_classify(2, 45, 135, 10, 1500);
        valid_cap = 1;
        
    } 
    
    else if (cap_diameter >= 30 && cap_diameter <= 44) {
        
        medium_cap += 1;
        servo_classify(4, 45, 135, 2100, 3400);
        valid_cap = 1;
        
    }
    
    else if (cap_diameter >= 45 && cap_diameter <= 70) {
        
        big_cap += 1;
        servo_classify(1, 135, 45, 10, 1500);
        valid_cap = 1;
        
    }
    
    if (valid_cap == 1) {
        
        LED2 = 1;
        __delay_ms(100);
        LED2 = 0;
        
    }
    
}

void six_seven() {

    if (button4 == 1) {
        
        LCD_write("67! 67! 67! 67!");
    
        for (int i = 0; i < 5; i ++) {
        
            servo_angle(70, 1);
            servo_angle(160, 2);
            servo_angle(20, 3);
            servo_angle(110, 4);
            LED1 = 1;
            LED2 = 1;
            LED3 = 1;
            
            __delay_ms(200);
            
            servo_angle(20, 1);
            servo_angle(110, 2);
            servo_angle(70, 3);
            servo_angle(160, 4);
            LED1 = 0;
            LED2 = 0;
            LED3 = 0;
            
            __delay_ms(200);
        
        }
        
        servo_angle(45, 1);
        servo_angle(135, 2);
        servo_angle(45, 3);
        servo_angle(135, 4);
    
    }

}

void laser_sensors() {
    
    int cap_timer_cs = 0;
    int cap_diameter = 0;
    
    if (laser1 == 1) {
        while (laser1 == 1) {
            cap_timer_cs += 1; // Increments perfectly while cap passes
            __delay_ms(10);
        }
        
        cap_diameter = (cap_timer_cs * linear_vel)/100; // Correct calculation
        
        LCD_write("T:%dms D:%dmm", cap_timer_cs * 10, cap_diameter ); 
        __delay_ms(500); 
        change2 = 1;      
        
        cap_classifier(cap_diameter); // Works perfectly for the FIRST cap!
    }
    
    cap_diameter = 0;
    
}

void read_emergency() {

    if (button5 == 0) {
    
        motor_speed = 0;
        linear_vel = 0;
        set_PWM_DutyCycle(0);        
        LCD_write("EMGERGENCIA!");
        
        while (button5 == 0) {
        
            LED1 = 1;
            buzzer();
            buzzer(); 
            
            LED1 = 0;
            __delay_ms(1000); 
        
        }
        
        LED1 = 0;
        LED2 = 0;
        LED3 = 0;
        
        LCD_write("Banda Pausada");
        
    
    }
    
    /*
    while (button5 == 1) {
    
        motor_speed = 0;
        linear_vel = 0;
        set_PWM_DutyCycle(0);
        
        LED1 = 1;
        LED2 = 1;
        LED3 = 1;
        
        __delay_ms(1000);
    
    }
    */

}

void main() {
    
    Init_Ports();
    configuraPWM();
    
    LED1 = 1;
    buzzer(); 
    buzzer(); 
    LED1 = 0;
    
    LED2 = 1;
    buzzer(); 
    buzzer(); 
    __delay_ms(2000);
    LED2 = 0;
    
    LED3 = 1;
    buzzer(); 
    buzzer();
    buzzer(); 
    buzzer();
    LED3 = 0;
    
    while(1) {
        
        six_seven();
        process_servos();
        Motor_Speed();
        laser_sensors();
        LCD_display();
        read_emergency();
        
    }
    
}