#include <EEPROM.h>         // библиотека для работы с EEPROM
#include "U8glib.h"
#include "GyverEncoder.h"

U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE|U8G_I2C_OPT_DEV_0);    // I2C / TWI 

int DISTANCE = 32;          // расстояние (шаги) сколько сделает мотор за 1 команду (* 100)
unsigned int Full_DISTANCE = 0;      // реальное количество шагов (DISTANCE * 100)
int Delay1 = 30;            // пауза между шагами мотора
bool Direct = true;         // изменение направление вращения мотора
bool TypeMode = true;       // определяем тип управления мотором, false - постоянное вращение мотора по нажатой кнопке
                            // true - единичное нажатие запускает цикл вращений, количество шагов задает перенная (DISTANCE * 100)
bool Mode = false;

#define ButtonPin 14         // кнопка управления (А0)

#define MOTOR_A_SLEEP_PIN 16    // пин разрешения работы драйвера (sleep)
#define MOTOR_A_STEP_PIN 11     // пин управления шагами мотора, HIGH-LOW - 1 шаг мотора
#define MOTOR_A_DIR_PIN 10      // пин управления направлением мотора, вперед или назад.
int StepCounter = 0;            // счетчик шагов
bool M_Step = false;            // переменная, управляющая шагами мотора

#define CLK 2
#define DT 3
#define SW 8
int EncoderTurnDir = 0;         // переменная, в которой хранится направление вращения энкодера, "0" - нет вращения, "1" - в право, "-1" - в лево

byte SettingsEnter = false;     // вошли в нвстройки пункта или нет
int MenuItem = 0;               // 4 пункта меню: 0 - шаги, 1 - направление, 2 - пауза, 3 - режим

Encoder enc1(CLK, DT, SW);      // для работы c кнопкой

char Buffer[] = "Hello String";
unsigned long DispTime;

#define addr_DISTANCE       0
#define addr_Delay1         2
#define addr_Direct         6
#define addr_TypeMode       7


ISR (TIMER2_COMPA_vect)       // функция вызывается прерыванием по таймеру и Управление шаговым двигателем
{
    M_Step = !M_Step;
    if(TypeMode){
        // работа шагового двигателя определенное количество шагов
        if (StepCounter < Full_DISTANCE){
            StepCounter++;
            digitalWrite(MOTOR_A_STEP_PIN, M_Step);
        }else{
            TCCR2B = (TCCR2B & 0xF8);
            TCNT2 = 0; 
            StepCounter=0;
            digitalWrite(MOTOR_A_SLEEP_PIN, LOW);  // разрешение питания мотора HIGH - включить, LOW - отключить
        }
    }else{
        // работа шагового двигателя пока нажата кнопка
        digitalWrite(MOTOR_A_STEP_PIN, M_Step);
    }
}

//------------------------------------------------------ Работа с EEPROM -------------------------------------
void Restore_from_EEPROM(){                         // Функция восстановления настроек из EEPROM
    DISTANCE = readIntFromEEPROM(addr_DISTANCE);
    Delay1   = readIntFromEEPROM(addr_Delay1);
    Direct   = EEPROM.read(addr_Direct);
    TypeMode = EEPROM.read(addr_TypeMode);
}

void EEPROM_Save(){                                 // Функция сохранения настроек в EEPROM
    writeIntIntoEEPROM(addr_DISTANCE, DISTANCE);
    writeIntIntoEEPROM(addr_Delay1, Delay1);
    EEPROM.update(addr_Direct, Direct);
    EEPROM.update(addr_TypeMode, TypeMode);
}

void writeIntIntoEEPROM(int address, int number){   // Функция сохранения перенных типа int в EEPROM
    EEPROM.update(address, number >> 8);
    EEPROM.update(address + 1, number & 0xFF);
}

int readIntFromEEPROM(int address){                 // Функция восстановления перенных типа int из EEPROM
    byte byte1 = EEPROM.read(address);
    byte byte2 = EEPROM.read(address + 1);
    return (byte1 << 8) + byte2;
}

//------------------------------------------------------ Работа с Дисплеем -------------------------------------
void draw(void) {
    // graphic commands to redraw the complete screen should be placed here  
    u8g.setFont(u8g_font_04b_24);  

    u8g.drawStr(2, 7, "Steps:  x100");           
    u8g.drawStr(66, 7, "Delay:");          
    u8g.drawStr(2, 39, "Direct:");                                              
    u8g.drawStr(66, 39, "Type:");
    
    u8g.setFont(u8g_font_osr18);
    sprintf(Buffer,"%d", DISTANCE);
    u8g.drawStr(10, 30, Buffer);
    sprintf(Buffer,"%d", Delay1);
    u8g.drawStr(70, 30, Buffer);
    u8g.setFont(u8g_font_ncenR14);
    u8g.drawStr(6, 60, (Direct ? "Forw." : "Back"));
    u8g.drawStr(70, 60, (TypeMode ? "Click" : "Hold"));

    if(SettingsEnter){
        switch (MenuItem) {                 // перещелкивание меню - рамки
            case 0:                 
                u8g.drawFrame (0,0,64,32);
                break;
            case 1:                 
                u8g.drawFrame (0,32,64,32);
                break;
            case 2:                 
                u8g.drawFrame (63,0,64,32);
                break;
            case 3:                 
                u8g.drawFrame (63,32,64,32);
                break;
        }        
    }else{
        switch (MenuItem) {                 // перелистывание меню - треугольники
           case 0:                 
                u8g.drawTriangle(0,11, 0,21, 5,16);
                break;
            case 1:                 
                u8g.drawTriangle(0,43, 0,53, 5,48);
                break;
            case 2:                 
                u8g.drawTriangle(127,11, 127,21, 122,16);
                break;
            case 3:                 
                u8g.drawTriangle(127,43, 127,53, 122,48);
                break;
        }
    }
}

void DisplayUpdate(){
    u8g.firstPage();
    do {
        draw();
    } while( u8g.nextPage() );
}//----------------------------------------------------


//-------------------------------------------------------- Управление Меню ----------------------------------------
void Menu(){
    enc1.tick();
    if (enc1.isRight()) EncoderTurnDir = 1;         // если был поворот энкодера
    if (enc1.isLeft())  EncoderTurnDir = -1;        // то изменяем значение переменной
    if (enc1.isClick()){                            // если была нажата кнопка
        SettingsEnter = !SettingsEnter;             // вход / выход в/из настроек

        if (millis() - DispTime > 300){
            DispTime = millis(); 
            DisplayUpdate();                        // обновляем изображение на дисплее
        }
        
        EEPROM_Save();                              // сохраняем настройки в EEPROM
    }
    
    if(SettingsEnter){                              // Если зашли в настройки
        if(EncoderTurnDir>1){
            EncoderTurnDir=1;
        }else if(EncoderTurnDir<-1){
            EncoderTurnDir=-1;
        }
        switch (MenuItem) {                         // в зависимости от того какой пункт меню выбран, те настройки и меняем
            case 0:                 
                if (EncoderTurnDir != 0){           // изменение количества шагов
                    DISTANCE = DISTANCE + EncoderTurnDir;
                    EncoderTurnDir = 0;
                    if (DISTANCE < 1){
                        DISTANCE = 1;
                    }else if (DISTANCE > 327){
                        DISTANCE = 327;
                    }
                    Full_DISTANCE = DISTANCE * 100 * 2;
                    DisplayUpdate();                            // обновляем изображение на дисплее
                }
                break;
            case 1:                 
                if (EncoderTurnDir != 0){                     // изменение направления вращения вперед/назад
                    Direct = !Direct;
                    EncoderTurnDir = 0;
                    digitalWrite(MOTOR_A_DIR_PIN, Direct);    // направление вращения мотора
                    DisplayUpdate();                          // обновляем изображение на дисплее
                }
                break;
            case 2:                 
                if (EncoderTurnDir != 0){                     // настройка паузы между шагами двигателя
                    Delay1 = Delay1 + (1 * EncoderTurnDir);   // (при сильно низкой паузе мотор не будет успевать делать шаг)
                    if (Delay1 < 15){
                        Delay1 = 15;
                    }else if (Delay1 > 80){
                        Delay1 = 80;
                    }
                    TCNT2 = 0;                                //  Устанавливаем настройки таймера
                    OCR2A = Delay1;                           // 
                    EncoderTurnDir = 0;
                    DisplayUpdate();                          // обновляем изображение на дисплее
                }
                break;
            case 3:                 
                if (EncoderTurnDir != 0){                     // изменения типа подачи: по удержанию кнопки или по количеству шагов
                    TypeMode = !TypeMode;
                    EncoderTurnDir = 0;
                    DisplayUpdate();                          // обновляем изображение на дисплее
                }
                break;
        } 
    }else{
        if (EncoderTurnDir != 0){                             //переключение между пунктами меню
            if(EncoderTurnDir>1){
                EncoderTurnDir=1;
            }else if(EncoderTurnDir<-1){
                EncoderTurnDir=-1;
            }
            Serial.println(EncoderTurnDir);
            MenuItem = MenuItem + EncoderTurnDir;
            if (MenuItem>3){MenuItem=0;};
            if (MenuItem<0){MenuItem=3;};
            EncoderTurnDir = 0;
            DisplayUpdate(); 
        }
    }
}//----------------------------------------------------

void setup() {
    pinMode(MOTOR_A_DIR_PIN, OUTPUT);       // пин отвечает за направление LOW - в одну сторону, HIGH - в другую сторону
    pinMode(MOTOR_A_STEP_PIN, OUTPUT);      // с этого пина подаем тактовый сигнал, LOW-HIGH - один сигнал
    
    digitalWrite(MOTOR_A_DIR_PIN, HIGH);    // направление вращения мотора
    digitalWrite(MOTOR_A_STEP_PIN, LOW);    // пин тактов шагов
    pinMode(MOTOR_A_SLEEP_PIN, OUTPUT);    // с этого пина подаем тактовый сигнал, LOW-HIGH - один сигнал
    digitalWrite(MOTOR_A_SLEEP_PIN, LOW);  // разрешение питания мотора HIGH - включить, LOW - отключить
    
    pinMode(ButtonPin, INPUT);              // кнопка для управления
    pinMode(ButtonPin, INPUT_PULLUP);

    enc1.setType(TYPE1);

    Restore_from_EEPROM();
    Full_DISTANCE = DISTANCE * 100 * 2;
    digitalWrite(MOTOR_A_DIR_PIN, Direct);  // направление вращения мотора

    DisplayUpdate();                        // обновляем изображение на дисплее
    DispTime = millis();

    cli(); // запрещаем перывания
    TCCR2A = 0;                       // устанавливаем настройки таймеря для учета времени
    TCCR2A |= (1 << WGM21);
    //TCCR2B |= (1 << CS22) | (1 << CS21) | (0 << CS20);// Set CS22, CS21 and CS20 bits for 256 prescaler
    TCCR2B = (TCCR2B & 0xF8);
    TIMSK2 |=(1<<OCIE2A);             // прерывание по совпадению
    TCNT2 = 0;                        //
    OCR2A = Delay1;                   // 
    sei(); // разрешаем прерывания
}

void loop() {
    Menu();
              
    if (!digitalRead(ButtonPin)){                   // если кнопка нажата старта мотора
        digitalWrite(MOTOR_A_SLEEP_PIN, HIGH);      // разрешение питания мотора HIGH - включить, LOW - отключить
        TCCR2B |= (1 << CS22) | (1 << CS21) | (0 << CS20);// Set CS22, CS21 and CS20 bits for 256 prescaler
    }else{
        if(!TypeMode){
            TCCR2B = (TCCR2B & 0xF8);
            TCNT2 = 0; 
            StepCounter=0;
            digitalWrite(MOTOR_A_SLEEP_PIN, LOW);  // разрешение питания мотора HIGH - включить, LOW - отключить
        }
    }
}
