#include <avr/io.h>
#include <util/delay.h>
#include "timer.h"
#include "spi.h"

uint16_t yValue;

// Pin configuration for the ATmega328P
#define DIN_PIN PB3  // Digital Pin 11, MOSI (Master Out Slave In)
#define CS_PIN PB2   // Digital Pin 10, SS (Slave Select)
#define CLK_PIN PB1  // Digital Pin 9, SCK (Serial Clock)
#define BUZZER_PIN PB4  // Assuming the buzzer is connected to PB4


// Game variables
volatile int basketPosition = 3;  // Initial basket position
volatile int fruitPositionX = 0;  // Initial fruit position X (randomized later)
volatile int fruitPositionY = 0;  // Initial fruit position Y
volatile unsigned long timerOverflowCount = 0;  // Timer overflow counter

unsigned int ADCVal;
unsigned int scoreCounter  = 0; //score counter
unsigned int gOnes = 0;   //ones place 7seg led
unsigned int gTens = 0;   //tens place 7seg led
unsigned int gHundreds = 0;   //hundreds place 7seg led

    int displayCounter = 0;
    int missedCounter = 0;
    bool missed =0;


//fruit variables
unsigned char randomnumber;
unsigned int  fallingRate = 100;
unsigned int fallingFruitIncrementor = 0;
unsigned int catchHolder;
unsigned int basketRowSeven = 7;
unsigned int basketRowEight = 8;
bool caught = 0;
unsigned int basketColumnOne;
unsigned int basketColumnTwo;
unsigned int basketColumnThree;
unsigned int fallingFruitRow;
unsigned int fallingFruitColumn;
unsigned int loseCounter=0;


unsigned int currentNoteIndex = 0;
unsigned int noteDurationCounter = 0;

struct note {
    uint16_t icr1Frequency; // Frequency value to set ICR1 register
    uint16_t duration;      // duration (ticks note remains active for)
};

struct note song[] = {
    {620, 150},   //incermenting tones
    {590, 100},  
    {550, 100},  
    {520, 350}, 

    {0,200}, //no sound
      
    {520, 150}, // same sound going down in tone 
    {55, 100},  
    {590, 100}, 
    {620, 350},

    {0,200}, //no sound

};






unsigned char seg7[] = {
    0b11111100, // 0
    0b00100100, // 1
    0b10111010, // 2
    0b10101110, // 3
    0b01100110, // 4
    0b11001110, // 5
    0b11011110, // 6
    0b10100100, // 7
    0b11111110, // 8
    0b11101110, // 9
};
unsigned char seg7BVal[] = {
    0b00000001, // 0
    0b00000000, // 1
    0b00000000, // 2
    0b00000000, // 3
    0b00000001, // 4
    0b00000001, // 5
    0b00000001, // 6
    0b00000000, // 7
    0b00000001, // 8
    0b00000001, // 9
};

unsigned char MatrixColumn[]{
    0b10000000,   //column 0
    0b01000000,   //column 1
    0b00100000,   //column 2
    0b00010000,   //column 3
    0b00001000,   //column 4
    0b00000100,   //column 5
    0b00000010,   //column 6
    0b00000001,   //column 7

};


unsigned char generateRandomNumber() {
    // Static seed to retain its value between function calls
    static unsigned char seed = 123;  // Initial seed value as unsigned char

    // Parameters for the LCG using unsigned char
    const unsigned char a = 17;   // Multiplier
    const unsigned char c = 43;   // Increment
    const unsigned char m = 254;  // Modulus (2^8)

    // Update the seed and calculate the next random number using 8-bit arithmetic
    seed = (a * seed + c) % m;

    // Adjust result to be in the range 0-7
    return static_cast<unsigned char>(seed % 8);
}

struct B {
    bool operator()(int pad) { return PINB & (1 << pad); } 
    void operator()(int pad, bool value) { 
        PORTB = (value) ? PORTB | (1 << pad) : PORTB & ~(1 << pad);
    }
};

struct C {
    bool operator()(int pad) { return PINC & (1 << pad); } 
    void operator()(int pad, bool value) {
        PORTC = (value) ? PORTC | (1 << pad) : PORTC & ~(1 << pad);
    }
};

struct D {
    bool operator()(int pad) { return PIND & (1 << pad); } 
    void operator()(int pad, bool value) { 
        PORTD = (value) ? PORTD | (1 << pad) : PORTD & ~(1 << pad);
    }
};

template <typename Group>
bool get(int pad)
{
    return Group()(pad);
}

template <typename Group>
void set(int pad, bool value)
{
    Group()(pad, value);
}

void matrixLED(unsigned char commandMatrix, unsigned char dataMatrix){
    set<B>(2,0);       // Select the MAX7219 by pulling CS low
    SPI_SEND(commandMatrix);  // Send the address (commandMatrix)
    SPI_SEND(dataMatrix);     // Send the data (dataMatrix)
    set<B>(2,1);        // Deselct Matrix by pulling CS high
}
void clearMatrix() {
    matrixLED(1, 0x00);matrixLED(2, 0x00);matrixLED(3, 0x00);matrixLED(4, 0x00); //turning top 4 row Leds off
    matrixLED(5, 0x00);matrixLED(6, 0x00);matrixLED(7, 0x00);matrixLED(8, 0x00); //turning bottom 4 row Leds off
}

void startScreen(){
    clearMatrix();
    matrixLED(2, MatrixColumn[2]^MatrixColumn[5]);
    matrixLED(5, MatrixColumn[2]^MatrixColumn[3]^MatrixColumn[4]^MatrixColumn[5]);
    
}

void missedScreen(){
    clearMatrix();
    matrixLED(2,MatrixColumn[1]^MatrixColumn[6]);
    matrixLED(3,MatrixColumn[2]^MatrixColumn[5]);
    matrixLED(4,MatrixColumn[3]^MatrixColumn[4]);
    matrixLED(5,MatrixColumn[3]^MatrixColumn[4]);
    matrixLED(6,MatrixColumn[2]^MatrixColumn[5]);
    matrixLED(7,MatrixColumn[1]^MatrixColumn[6]);

}
void loseScreen(){
    clearMatrix();
    matrixLED(7, MatrixColumn[0]^MatrixColumn[7]);
    matrixLED(6, MatrixColumn[1]^MatrixColumn[6]);
    matrixLED(2, MatrixColumn[2]^MatrixColumn[5]);
    matrixLED(5, MatrixColumn[2]^MatrixColumn[3]^MatrixColumn[4]^MatrixColumn[5]);

}
enum segStates{ onesPlace, tensPlace, hundredsPlace} segState;
void TickFuc_segStates(){ // state behavior

    switch (segState)
    {


    case onesPlace:
        segState = tensPlace;
        break;


    case tensPlace:
        segState = hundredsPlace;
        break;


    case hundredsPlace:
        segState = onesPlace;
        break;


    } // end state behavior


    switch (segState)
    { // state actions


    case onesPlace:
        set<C>(0,0);
        set<C>(1,1);
        set<C>(2,1);
        gOnes = scoreCounter % 10;
        PORTD = seg7[gOnes];
        PORTB = seg7BVal[gOnes];
        break;


    case tensPlace:
      
      set<C>(0,1);
      set<C>(1,0);
      set<C>(2,1);
      gTens = (scoreCounter / 10) % 10;
      PORTD = seg7[gTens];
      PORTB = seg7BVal[gTens];

      
      break;


    case hundredsPlace:
      set<C>(0,1);
      set<C>(1,1);
      set<C>(2,0);        
      gHundreds = (scoreCounter / 100) % 10;
      PORTD = seg7[gHundreds];
      PORTB = seg7BVal[gHundreds];

      break;
    }


} // end tick

void winScreen(){
    clearMatrix();
    matrixLED(4, MatrixColumn[1]^MatrixColumn[6]);
    matrixLED(2, MatrixColumn[2]^MatrixColumn[5]);
    matrixLED(5, MatrixColumn[2]^MatrixColumn[3]^MatrixColumn[4]^MatrixColumn[5]);
     set<B>(BUZZER_PIN, 1);  // Turn off buzzer

}
void setupMatrix() {
    set<B>(2,0); // drag CS (SS) line low
    SPI_SEND(0x09);SPI_SEND(0x00);  //Decode mode (set to off)
    set<B>(2,1); // drag CS (SS) line high
   
    set<B>(2,0);SPI_SEND(0x0A);SPI_SEND(0x02); set<B>(2,1); //intensity 
    set<B>(2,0);SPI_SEND(0x0B);SPI_SEND(0x07); set<B>(2,1); //Scan limit(active rows) set tp all rows active
    set<B>(2,0);SPI_SEND(0x0C);SPI_SEND(0x01); set<B>(2,1); //shutdown set to normal operation (LED on)
    set<B>(2,0);SPI_SEND(0x0F);SPI_SEND(0x00); set<B>(2,1); //Display test(all LEDs on) set to off 
}


enum basketStates {choose_C, first_C,second_C, third_C,
fourth_C, fifth_C,sixth_C, seventh_C, eight__C} basketState;

void tickFunCBasket(){
ADCVal = ADC%1024;
 switch(basketState){
    case choose_C:
     set<B>(BUZZER_PIN, 0);  // Turn off buzzer
    if (ADCVal <= 70){
        basketState = eight__C;
    }
    else if(ADCVal <= 254) {
        basketState = seventh_C; 
    }
    else if(ADCVal <= 340) {
        basketState = sixth_C;
    }
    else if(ADCVal <= 450) {
        basketState = fifth_C;
    }
    else if(ADCVal <= 635) {
        basketState = fourth_C;
   }
    else if(ADCVal <= 762) {
        basketState = third_C;    
    }
    else if (ADCVal <= 998) {
        basketState = second_C;
    }
    else if (ADCVal<=1023){
        basketState = first_C;      
    }
    break;
 }

 switch(basketState){
    case eight__C:
     set<B>(BUZZER_PIN, 0);  // Turn off buzzer
    matrixLED(7,(MatrixColumn[7] ^ MatrixColumn[5]));   // top right of basket &  top left of basket
    matrixLED(8,MatrixColumn[6]);   //center of basket
    basketColumnOne = 5; //left
    basketColumnTwo = 6; //middle
    basketColumnThree = 7; // right
    basketState = choose_C;
    break;

   case seventh_C:
    set<B>(BUZZER_PIN, 0);  // Turn off buzzer
    matrixLED(7,(MatrixColumn[7] ^ MatrixColumn[5]));   // top right of basket &  top left of basket
    matrixLED(8,MatrixColumn[6]);   //center of basket
    basketColumnOne = 5; //left
    basketColumnTwo = 6; //middle
    basketColumnThree = 7; // right
    basketState = choose_C;

    break;
   
   case sixth_C:
    set<B>(BUZZER_PIN, 0);  // Turn off buzzer
    matrixLED(7,(MatrixColumn[6] ^ MatrixColumn[4]));   // top right of basket &  top left of basket
    matrixLED(8,MatrixColumn[5]);   //center of basket
    basketColumnOne = 4; //left
    basketColumnTwo = 5; //middle
    basketColumnThree = 6; // right
    basketState = choose_C;
    break;

    case fifth_C:
     set<B>(BUZZER_PIN, 0);  // Turn off buzzer
    matrixLED(7,(MatrixColumn[5] ^ MatrixColumn[3]));   // top right of basket &  top left of basket
    matrixLED(8,MatrixColumn[4]);   //center of basket
    basketColumnOne = 3; //left
    basketColumnTwo = 4; //middle
    basketColumnThree = 5; // right
    basketState = choose_C;
    break;

    case fourth_C:
    set<B>(BUZZER_PIN, 0);  // Turn off buzzer
    matrixLED(7,(MatrixColumn[4] ^ MatrixColumn[2]));   // top right of basket &  top left of basket
    matrixLED(8,MatrixColumn[3]);   //center of basket
    basketColumnOne = 2; //left
    basketColumnTwo = 3; //middle
    basketColumnThree = 4; // right
    basketState = choose_C;
    break;

    case third_C:
    set<B>(BUZZER_PIN, 0);  // Turn off buzzer
    matrixLED(7,(MatrixColumn[3] ^ MatrixColumn[1]));   // top right of basket &  top left of basket   // top left of basket
    matrixLED(8,MatrixColumn[2]);   //center of basket
    basketColumnOne = 1; //left
    basketColumnTwo = 2; //middle
    basketColumnThree = 3; // right
    basketState = choose_C;
    break;

    case second_C:
     set<B>(BUZZER_PIN, 0);  // Turn off buzzer
    matrixLED(7,(MatrixColumn[2] ^ MatrixColumn[0]));   // top right of basket &  top left of basket
    matrixLED(8,MatrixColumn[1]);   //center of basket
    basketColumnOne = 0; //left
    basketColumnTwo = 1; //middle
    basketColumnThree = 2; // right
    basketState = choose_C;
    break;

    case first_C:
     set<B>(BUZZER_PIN, 0);  // Turn off buzzer
    matrixLED(7,(MatrixColumn[2] ^ MatrixColumn[0]));   // top right of basket &  top left of basket
    matrixLED(8,MatrixColumn[1]);   //center of basket
    basketColumnOne = 0; //left
    basketColumnTwo = 1; //middle
    basketColumnThree = 2; // right
    basketState = choose_C;
    break;


}
} ;
   
enum NoteStates { Initial_State, Play_Note, Wait_Note, Next_Note } Note_State;

void TickFunc_PlaySong() {
    // State transitions
    switch (Note_State) {
        case Initial_State:
            currentNoteIndex = 0; 
            noteDurationCounter = 0;
            Note_State = Play_Note;
            break;
        
        case Play_Note:
            ICR1 = song[currentNoteIndex].icr1Frequency;  // current note frequency
            OCR1A = ICR1 / 4;  //duty cycle 25%
            noteDurationCounter = 0;
            Note_State = Wait_Note;
            break;
        
        case Wait_Note:
            if (noteDurationCounter >= song[currentNoteIndex].duration) {
                Note_State = Next_Note;
            } else {
                noteDurationCounter++;
            }
            break;
        
        case Next_Note:
            currentNoteIndex++;
            if (currentNoteIndex >= sizeof(song) / sizeof(song[0])) { ///finished with array, size(song[0]) returns 4, size9song) returns 4* #of elements in array
                currentNoteIndex = 0; // Loop back to start
            }
            Note_State = Play_Note;
            break;
        
        default:
            Note_State = Initial_State;
            break;
    }
    set<B>(BUZZER_PIN, 0);  // Turn off buzzer
};


enum fallingStates{chooseColumnState, rowOne, rowTwo,
rowThree,RowFour, rowFive, rowSix,rowSeven, rowEight} fallingState;
void tickFunc_FallingFruit(){
    tickFunCBasket();

    switch (fallingState){     //transitions


        case chooseColumnState:
         set<B>(BUZZER_PIN, 0);  // Turn off buzzer
        fallingState = rowOne; 
        break;

        case rowOne:
         set<B>(BUZZER_PIN, 0);  // Turn off buzzer
        if (fallingFruitIncrementor>fallingRate){         
            fallingFruitIncrementor = 0;
            fallingState = rowTwo;  
            }
        else{
            fallingFruitIncrementor++;
        }
         break;


        case rowTwo:
         set<B>(BUZZER_PIN, 0);  // Turn off buzzer
            if (fallingFruitIncrementor>fallingRate){         
            fallingFruitIncrementor = 0;
            fallingState = rowThree;  
            }
        else{
            fallingFruitIncrementor++;
        }
        break;       

        case rowThree:
         set<B>(BUZZER_PIN, 0);  // Turn off buzzer
            if (fallingFruitIncrementor>fallingRate){         
            fallingFruitIncrementor = 0;
            fallingState = RowFour;  
            }
        else{
            fallingFruitIncrementor++;
        }
        break;     
        case RowFour:
         set<B>(BUZZER_PIN, 0);  // Turn off buzzer
            if (fallingFruitIncrementor>fallingRate){         
            fallingFruitIncrementor = 0;
            fallingState = rowFive;  
            }
        else{
            fallingFruitIncrementor++;
        }
        break;    

        case rowFive:
         set<B>(BUZZER_PIN, 0);  // Turn off buzzer
            if (fallingFruitIncrementor>fallingRate){         
            fallingFruitIncrementor = 0;
            fallingState = rowSix;  
            }
        else{
            fallingFruitIncrementor++;
        }
        break;     
        case rowSix:
         set<B>(BUZZER_PIN, 0);  // Turn off buzzer
            if (fallingFruitIncrementor>fallingRate){         
            fallingFruitIncrementor = 0;
            fallingState = rowSeven;  
            }
        else{
            fallingFruitIncrementor++;
        }
        break;     
        case rowSeven:
         set<B>(BUZZER_PIN, 0);  // Turn off buzzer
            if (fallingFruitIncrementor>fallingRate){         
            fallingFruitIncrementor = 0;
            fallingState = rowEight;  
            }
        else{
            fallingFruitIncrementor++;
        }
        break;     
        case rowEight:
         set<B>(BUZZER_PIN, 0);  // Turn off buzzer
            if (fallingFruitIncrementor>fallingRate){         
            fallingFruitIncrementor = 0;
            fallingState = chooseColumnState;  
            }
        else{
            fallingFruitIncrementor++;
        }
        break;     
        
    }

        switch (fallingState){ 
        case chooseColumnState:
         set<B>(BUZZER_PIN, 0);  // Turn off buzzer
        matrixLED(8,0x00);
        if(caught){
            scoreCounter++;
            caught = 0;
        }
        else{
            caught = 0;
            missed = 1;
            missedCounter++;

        }

        randomnumber = generateRandomNumber();
        break;
        case rowOne:
        missed =0;
        matrixLED(1,MatrixColumn[randomnumber]);
        break;
        case rowTwo:
        matrixLED(1,0x00);
        matrixLED(2,MatrixColumn[randomnumber]);
        break;
        case rowThree:
        matrixLED(2,0x00);
        matrixLED(3,MatrixColumn[randomnumber]);
        break;
        case RowFour:
        matrixLED(3,0x00);
        matrixLED(4,MatrixColumn[randomnumber]);
        break;
        case rowFive:
        matrixLED(4,0x00);
        matrixLED(5,MatrixColumn[randomnumber]);
        break;
        case rowSix:
        matrixLED(5,0x00);
        matrixLED(6,MatrixColumn[randomnumber]);
        break;
        case rowSeven:
        matrixLED(6,0x00);
        matrixLED(7,MatrixColumn[randomnumber]);
        if ((randomnumber == basketColumnOne || randomnumber == basketColumnThree) ){
            caught = 1;
             
        }
        break;
        case rowEight:
        matrixLED(7,0x00);
        matrixLED(8,MatrixColumn[randomnumber]);
        if ((randomnumber == basketColumnTwo) ){
            caught = 1;
        }

        break;
     }
};

enum playStates{idlePLay, pressPlay, activleyPlaying, pressReset, loseState, winState, missedState} playState;
void tickFuncPlayStates(){
    ADCVal = ADC;
    switch (playState)
    {
    case idlePLay:
    set<B>(BUZZER_PIN, 0);  // Turn off buzzer
   missedCounter = 0;
    scoreCounter=0;
    if(ADCVal >= 650) {
        playState = activleyPlaying;    
    }

         break;
    case activleyPlaying:
        clearMatrix();

        // if(PINC&0x04){
        //     playState = pressReset;
        // } 
        // else{
        //     playState = activleyPlaying;
        // }
         if(PINC^0x40){
            playState = pressReset;
        } 
        else{
            playState = activleyPlaying;
        }
        if(scoreCounter<24){
            playState = activleyPlaying;
        }
        if(scoreCounter >= 24) {
        
        playState = winState;
        }  

        if (missed == 1){
            playState = missedState;
        }
        if (missedCounter >=3){
            playState = loseState;
        }
        break;
    case pressReset:
        set<B>(BUZZER_PIN, 0);  // Turn off buzzer
        if(PINC^0x40){
            playState = pressReset;
        } 
        else{
            playState = idlePLay;
        }
        
        break;

        case winState: 
        
        if (displayCounter< 300)
        {
            winScreen();
            displayCounter++;
        }
        else if (displayCounter < 600 ){
            displayCounter ++; 
            clearMatrix();
        }
        else{
            displayCounter = 0;
        }
        TickFunc_PlaySong();
       if(ADCVal <= 150) {
        playState = idlePLay;    
        }
        
    break;
        break;
        case missedState:
         set<B>(BUZZER_PIN, 0);  // Turn off buzzer
        if (displayCounter < 1000){
            missedScreen();
            displayCounter++;
        }
        else{
            playState = activleyPlaying;
            displayCounter =0;
        }
break;

case loseState:
 set<B>(BUZZER_PIN, 0);  // Turn off buzzer
 if(ADCVal <= 150) {
        playState = idlePLay;    
    }
    break;

    }

switch (playState){
    
    
    case idlePLay:
        startScreen();
        break;
    case pressPlay:
        break;
    case activleyPlaying:
        tickFunc_FallingFruit();
        case winState:

        break;
    case pressReset:
        break;
        case loseState:
        loseScreen();
        break;
}};



int main() {
   

    DDRC = 0x07;  // Configure PORTC as output for 7-segment control
    DDRD = 0xFF;  // Configure PORTD as output for 7-segment display
    DDRB = 0xFF;
    //playState = idlePLay;
//setting up matrix
    SPI_INIT();
    setupMatrix();
    clearMatrix();

    //anolog stick
    ADMUX = 0b01000100;
    ADCSRA = 0b11100111;
    ADCSRB = 0b00000000;
    TCCR1A = 0x82;  // 0b10000010 in hexadecimal for Fast PWM mode
    TCCR1B = 0x1A;  // 0b00011010 in hexadecimal for Fast PWM mode

    TimerSet(1);  
    TimerOn();
    while (1) {

       tickFuncPlayStates();
       TickFuc_segStates();  // Update 7-segment display with new score
        while (!TimerFlag);
        TimerFlag = false;
        set<B>(BUZZER_PIN, 0);  // Turn off buzzer
    }


    return 0;
}
