
#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "keypad.h"
#include "ADC.h"
#include "avr-nokia5110-master/nokia5110.h" //#include "io.h" 
#include "avr-nokia5110-master/nokia5110.c"//#include "io.c"
#include "PWM.h"
#include "scheduler.h"
#include "timer.h"
#include "shift_registers.h"
#include "lookupTempo.h"
//#include "fft/ffft.h"

unsigned char updateDisplay = 0;

unsigned char defaultMenuFlag = 1;

unsigned char playTempoFlag = 0;
unsigned char setTempoFlag = 0;
unsigned char playNoteFlag = 0;
unsigned char tunerFlag = 0;

unsigned char menuFlagIter = 0;

//sets flags to 1 to enable a function

enum menuState{choose, releaseButton, waitForSelect, select, idle_menu};
int menu(int state) {

	char buttons = ~PINA & 0x0E;

	switch(state) {
		case choose:
			if(buttons == 0x04) {
				menuFlagIter = (menuFlagIter < 3) ? menuFlagIter+1 : 0;
				updateDisplay = 1;
				state = releaseButton;
			}
			else if(buttons == 0x08) {
				menuFlagIter = (menuFlagIter > 0) ? menuFlagIter-1 : 3;
				updateDisplay = 1;
				state = releaseButton;
			}
			else if(buttons == 0x02) {
				state = waitForSelect;
			}
			else {
				state = choose;
			}
			break;
			
		case releaseButton:
			if(buttons != 0x00) {
				state = releaseButton;
			} else {
				state = choose;
			}
			break;
			
		case waitForSelect:
			if(buttons != 0x00) {
				state = waitForSelect;
			} else {
				state = select;
			}
			break;
		
		case select:
			if(menuFlagIter == 0) {
				playTempoFlag = 1;
				defaultMenuFlag = 0;
			}
			else if(menuFlagIter == 1) {
				setTempoFlag = 1;
				defaultMenuFlag = 0;
			}
			else if(menuFlagIter == 2) {
				playNoteFlag = 1;
				defaultMenuFlag = 0;
			}
			else if(menuFlagIter == 3) {
				tunerFlag = 1;
				defaultMenuFlag = 0;
			} 
			else {
				//this should never occur
				//output error message
			}
			updateDisplay = 1;
			state = idle_menu;
			break;
		
		case idle_menu:
			if(defaultMenuFlag != 0) {
				state = choose;
			} 
			else {
				state = idle_menu;
			}
			break;
		
		default:
			state = idle_menu;
			break;
	}
	
	return state;
}

/******************************************************/

// Tempo min = 40; Tempo max = 180;

unsigned short playTempoCnt = 0;
unsigned short matrixCnt = 0;

unsigned char TEMPO = 40;
#define TEMPO_ADDR 0

unsigned char BEAT = 4;
#define BEAT_ADDR 1

unsigned char currentBeat = 1;

unsigned char pattern_one = 0;
unsigned char pattern_two = 0;
unsigned char pattern_three = 0;

unsigned char matrixIter = 0;
unsigned char playTempoMatrixIter = 0;

#define MAX_TEMPO_PATTERNS 3

enum playTempoStates{wait, tick, buttonHeldPT, idle_playTempo};
int playTempo(int state) {
	
	char button = ~PINA & 0x02;
	
	unsigned short TEMPO_DIV_8 = tempoToMs(TEMPO-40) / 8;
	
	switch(state) {
		case wait:
			if(button == 0x02) {
				state = buttonHeldPT;
			} else {
				if(playTempoCnt < (tempoToMs(TEMPO-40) - 2)) {
					playTempoCnt++;
					if(!(playTempoCnt % TEMPO_DIV_8)) {
						if(playTempoMatrixIter == 0) {
							pattern_one = 1;
							matrixIter++;
						}
					}
				} else {
					
					playTempoCnt = 0;
					state = tick;
					PWM_on();
					
					if(currentBeat >= BEAT) { 
						set_PWM(1500);
						currentBeat = 1;
					} else {
						set_PWM(800);
						currentBeat++;
					}
					
					if(playTempoMatrixIter == 0) {
						pattern_one = 1;
						matrixIter = (matrixIter <= 8) ? 8 : 0;
					} else if (playTempoMatrixIter == 1) {
						pattern_two = 1;
					} else if (playTempoMatrixIter == 2) {
						pattern_three = 1;
						if(BEAT != 0) {
							matrixIter = (currentBeat == 1) ? 1 : 2;
						} else {
							matrixIter = 2;
						}
					}
				}
			}
			break;
		
		case tick:
			if(button == 0x02) {
				state = buttonHeldPT;
			}
			else if(playTempoCnt < 2) { //adjust length of click
				playTempoCnt++;
				state = tick;
			}
			else {
				
				if(playTempoMatrixIter == 2) {
					pattern_three = 1;
					matrixIter = 0;
				}
				
				state = wait;
				playTempoCnt = 0;
				PWM_off();
			}
			break;
		
		case buttonHeldPT:
			if(button != 0x00) {
				state = buttonHeldPT;
			} 
			else {
				state = idle_playTempo;
				defaultMenuFlag = 1;
				playTempoFlag = 0;
				updateDisplay = 1;
			}
			break;
		
		case idle_playTempo:
			if(playTempoFlag != 0) {
				state = wait;
				matrixIter = 0;
				playTempoCnt = 0;
				currentBeat = 1;
			}
			else {
				state = idle_playTempo;
			}
			break;
			
			default:
				state = idle_playTempo;
				break;
	}
	return state;
}

enum playTempoHelperState{cycle, buttonHeld, flagIsDown}; 

int playTempoHelper(int state) {
	
	unsigned char button = ~PINA & 0x10;
	
	switch(state) {
		
		case cycle:
			if(playTempoFlag == 0) {
				state = flagIsDown;
			} else if(button == 0x10) {
				state = buttonHeld;
				playTempoMatrixIter = (playTempoMatrixIter < MAX_TEMPO_PATTERNS - 1) ? playTempoMatrixIter+1 : 0;
				
				if(matrixIter != 1) {
					pattern_two = 0;
				} else {
					pattern_two = 1;
				}
			}
			else {
				state = cycle;
			}
			break;
			
		case buttonHeld:
			if(playTempoFlag == 0) {
				state = flagIsDown;
			} 
			else if(button == 0x00) {
				state = cycle;
			} 
			else {
				state = buttonHeld;
			}
			break;
		
		case flagIsDown:
			if(playTempoFlag != 0) {
				state = cycle;
			} else {
				state = flagIsDown;
			}
			break;
		
	}
	
	return state;
	
}

/******************************************************/

const short BUTTON_WAIT = 800;

unsigned short buttonCnt = 0;

#define TEMPO_VAR 1
#define BEAT_UPPER_THRESH 6

unsigned char tempoVarArrIter = 0;

//idle state means SM is not active;
enum setTempoState{maintain, inc, dec, pressedButton, buttonHeldST, idle_setTempo};
int setTempo(int state) {
	
	char buttons = ~PINA & 0x1E;
	
	//state transitions
	switch(state) {
		case maintain:
			if(buttons == 0x02) {
				state = buttonHeldST;
			}
			else if (buttons == 0x08) {
				if(tempoVarArrIter == 0) {
					state = inc;
					TEMPO = (TEMPO < 180) ? TEMPO+1 : TEMPO;
					eeprom_write_byte((uint8_t*)TEMPO_ADDR, TEMPO);
				} else {
					BEAT = (BEAT  < BEAT_UPPER_THRESH) ? BEAT+1 : BEAT;
					eeprom_write_byte((uint8_t*)BEAT_ADDR, BEAT);
					state = pressedButton;
				}
				updateDisplay = 1;
			}
			else if(buttons == 0x04) {
				if(tempoVarArrIter == 0) {
					state = dec;
					TEMPO = (TEMPO > 40) ? TEMPO-1 : TEMPO;
					eeprom_write_byte((uint8_t*)TEMPO_ADDR, TEMPO);
					updateDisplay = 1;
				} else {
					BEAT = (BEAT > 0) ? BEAT-1 : BEAT;
					eeprom_write_byte((uint8_t*)BEAT_ADDR, BEAT);
					state = pressedButton;
				}
				updateDisplay = 1;
			} else if(buttons == 0x10) {
				tempoVarArrIter = (tempoVarArrIter < TEMPO_VAR) ? tempoVarArrIter+1 : 0;
				updateDisplay = 1;
				state = pressedButton;
			} else {
				state = maintain;
			}
			break;
		
		case inc:
			if (buttons == 0x08) {
				state = inc;
				buttonCnt++;
			}
			else {
				state = maintain;
				buttonCnt = 0;
			}
					break;
		
		case dec:
			if(buttons == 0x04) {
				state = dec;
				buttonCnt++;
			}
			else {
				state = maintain;
				buttonCnt = 0;
			}
			break;
		
		case pressedButton:
			if(buttons != 0x00) {
				state = pressedButton;
			} else {
				state = maintain;
			}
			break;
		
		case buttonHeldST:
			if(buttons != 0x00) {
				state = buttonHeldST;
			}
			else {
				state = idle_setTempo;
				setTempoFlag = 0;
				defaultMenuFlag = 1;
				updateDisplay = 1;
			}
			break;
		
		case idle_setTempo:
			if(setTempoFlag != 0) {
				state = maintain;
				if(TEMPO < 40 || TEMPO > 180) {
					TEMPO = 40;
				}
			}
			else {
				state = idle_setTempo;
			}
			break;
		
		default:
			state = idle_setTempo;
			break;
	}
	
	//state actions
	switch(state) {
		case maintain:
			break;
		
		case inc:
			if(buttonCnt >= BUTTON_WAIT) {
				if(TEMPO  <= 170) {
					TEMPO+=10;
					updateDisplay = 1;
					buttonCnt = 0;
					eeprom_write_byte((uint8_t*)TEMPO_ADDR, TEMPO);
				}
				else if(TEMPO > 170) {
					TEMPO = 180;
					updateDisplay = 1;
					buttonCnt = 0;
					eeprom_write_byte((uint8_t*)TEMPO_ADDR, TEMPO);
				} else  if (TEMPO == 180) {
					TEMPO = 180;
					buttonCnt = 0;
				}
			}
			break;
		
		case dec:
			if(buttonCnt >= BUTTON_WAIT) {
				if(TEMPO >= 50) {
					TEMPO-=10;
					updateDisplay = 1;
					buttonCnt = 0;
					eeprom_write_byte((uint8_t*)TEMPO_ADDR, TEMPO);
				}
				else if(TEMPO < 50) {
					TEMPO = 40;
					updateDisplay = 1;
					buttonCnt = 0;
					eeprom_write_byte((uint8_t*)TEMPO_ADDR, TEMPO);
				}
				else if(TEMPO == 40) {
					TEMPO = 40;
					buttonCnt = 0;
				}
			}
			break;
		
		case pressedButton:
			break;
		
		case buttonHeldST:
			break;
		
		case idle_setTempo:
			break;
		
		default:
			state = idle_setTempo;
			break;
	}
	
	return state;
	
}

/******************************************************/

#define NOTE_RANGE 7
#define OCTAVES 3

//3rd octave
#define C3	130.81	
#define D3	146.83	
#define E3	164.81	
#define F3	174.61	
#define G3	196.00
#define A3	220.00	
#define B3	246.94

//4th octave
#define C4	261.63
#define D4	293.66
#define E4	329.63
#define F4	349.23
#define G4	392.00
#define A4	440.00
#define B4	493.88

//5th octave
#define C5	523.25
#define D5	587.33
#define E5	659.25
#define F5	698.46
#define G5	783.99
#define A5	880.00
#define B5	987.77

#define noteArrayIter_ADDR 2
#define octaveIter_ADDR 3

char octaveArr[OCTAVES] = {3, 4, 5};
char octaveIter = 0;

double noteArrayFreq3[NOTE_RANGE] = {C3, D3, E3, F3, G3, A3, B3}; // for PWM
double noteArrayFreq4[NOTE_RANGE] = {C4, D4, E4, F4, G4, A4, B4}; // for PWM
double noteArrayFreq5[NOTE_RANGE] = {C5, D5, E5, F5, G5, A5, B5}; // for PWM
char noteArray[NOTE_RANGE] = {'C', 'D', 'E', 'F', 'G', 'A', 'B'}; // for display

unsigned char noteArrayIter = 0;


enum playNoteState{setOn, play, playHeld, buttonHeldForIncDec, buttonHeldForSilence, buttonHeldPN, idle_playNote};

int playNote(int state) {
	
	unsigned char buttons = ~PINA & 0x1E;
	
	switch(state) {
		
		case setOn:
			if(buttons == 0x02) {
				state = buttonHeldPN;
			} 
			else if(buttons == 0x08) {
				if(noteArrayIter < NOTE_RANGE - 1) {
					noteArrayIter++;
				} else {
					noteArrayIter = 0;
					octaveIter = (octaveIter < OCTAVES - 1) ? octaveIter+1 : 0;
					eeprom_write_byte((uint8_t*)octaveIter_ADDR, octaveIter);
				}
				eeprom_write_byte((uint8_t*)noteArrayIter_ADDR, noteArrayIter);
				updateDisplay = 1;
				state = buttonHeldForIncDec;
			} 
			else if(buttons == 0x04) {
				if(noteArrayIter > 0) {
					noteArrayIter--;
				} else {
					noteArrayIter = NOTE_RANGE - 1;
					octaveIter = (octaveIter > 0) ? octaveIter-1 : OCTAVES - 1;
					eeprom_write_byte((uint8_t*)octaveIter_ADDR, octaveIter);
				}
				eeprom_write_byte((uint8_t*)noteArrayIter_ADDR, noteArrayIter);
				updateDisplay = 1;
				state = buttonHeldForIncDec;
			}
			else if (buttons == 0x10){
				PWM_on();
				if(octaveIter == 0) {
					set_PWM(noteArrayFreq3[noteArrayIter]);
				} else if(octaveIter == 1) {
					set_PWM(noteArrayFreq4[noteArrayIter]);
				} else if(octaveIter == 2) {
					set_PWM(noteArrayFreq5[noteArrayIter]);
				}
				state = playHeld;
			} else {
				state = setOn;
			}
			break;
			
		case play:
			if(buttons == 0x02) {
				state = buttonHeldPN;
				PWM_off();
			} else if(buttons == 0x10) {
				state = buttonHeldForIncDec;
				PWM_off();
			}
			else {
				state = play;
			}
			break;
			
		case playHeld:
			if(buttons == 0x00) {
				state = play;
			} else {
				state = playHeld;
			}
			break;
			
		case buttonHeldForIncDec:
			if(buttons == 0x00) {
				state = setOn;
			} else {
				state = buttonHeldForIncDec;
			}
			break;
			
		
		case buttonHeldPN:
			if(buttons != 0x00) {
				state = buttonHeldPN;
			} 
			else {
				playNoteFlag = 0;
				defaultMenuFlag = 1;
				state = idle_playNote;
				updateDisplay = 1;
			}
			break;
		
		case idle_playNote:
			if(playNoteFlag != 0) {
				state = setOn;
			} 
			else {
				state = idle_playNote;
			}
			break;
		
		default:
			state = idle_playNote;
			break;
	}
	
	return state;
}

/******************************************************/

//enum tunerStates{sampling, convert, idle_tuner};
enum tunerStates{doNothing, buttonHeldT, idle_tuner};
	
unsigned short micOutput = 0;
unsigned short prevMicOutput = 0;

unsigned char cnt = 0;

int tuner(int state) {
	
	char unsigned button = ~PINA & 0x02;
	
	switch(state) {
		case doNothing:
			if(button == 0x02) {
				state = buttonHeldT;
			} else {
				micOutput = ADC;
				cnt++;
				if(cnt > 50) {
					cnt = 0;
					updateDisplay = 1;
				}
				state = doNothing;
			}
			break;
			
		case buttonHeldT:
			if(button != 0x00) {
				state = buttonHeldT;
			} else {
				tunerFlag = 0;
				defaultMenuFlag = 1;
				state = idle_tuner;
				updateDisplay = 1;
			}
			break;
		
		case idle_tuner:
			if(tunerFlag != 0) {
				state = doNothing;
				updateDisplay = 1;
			} else {
				state = idle_tuner;
			}
			break;
	}
	
	return state;
	
}

/******************************************************/

// playTempoFlag = 0;
// setTempoFlag = 0;
// playNoteFlag = 0;
// tunerFlag = 0;

enum displayLCDState {firstDisplay, display};
int displayLCD(int state) {
	
	//char hunDigit = TEMPO / 100;
	//char tenDigit = (TEMPO - (hunDigit * 100)) / 10;
	//char oneDigit = TEMPO - (hunDigit * 100) - (tenDigit * 10);
	
	switch(state) {
		
		case firstDisplay:
			nokia_lcd_clear();
			nokia_lcd_write_string("1.Play Tempo ", 1);
			
			if(menuFlagIter == 0) {
				nokia_lcd_write_char('<', 1);
			} else {
				nokia_lcd_write_char(' ', 1);
			}
			
			nokia_lcd_write_string("2.Set Tempo  ", 1);
			
			if(menuFlagIter == 1) {
				nokia_lcd_write_char('<', 1);
				} else {
				nokia_lcd_write_char(' ', 1);
			}
			
			nokia_lcd_write_string("3.Pick Note  ", 1);
			
			if(menuFlagIter == 2) {
				nokia_lcd_write_char('<', 1);
				} else {
				nokia_lcd_write_char(' ', 1);
			}
			
			nokia_lcd_write_string("4.Mic        ", 1);
			
			if(menuFlagIter == 3) {
				nokia_lcd_write_char('<', 1);
				} else {
				nokia_lcd_write_char(' ', 1);
			}
			
			nokia_lcd_render();
			state = display;
			break;
		
		case display:
			if(updateDisplay == 1) {
				if(defaultMenuFlag == 1) {
					nokia_lcd_clear();
					nokia_lcd_write_string("1.Play Tempo ", 1);
					
					if(menuFlagIter == 0) {
						nokia_lcd_write_char('<', 1);
					} else {
						nokia_lcd_write_char(' ', 1);
					}
					
					nokia_lcd_write_string("2.Set Tempo  ", 1);
					
					if(menuFlagIter == 1) {
						nokia_lcd_write_char('<', 1);
					} else {
						nokia_lcd_write_char(' ', 1);
					}
					
					nokia_lcd_write_string("3.Pick Note  ", 1);
					
					if(menuFlagIter == 2) {
						nokia_lcd_write_char('<', 1);
					} else {
						nokia_lcd_write_char(' ', 1);
					}
					
					nokia_lcd_write_string("4.Mic        ", 1);
					
					if(menuFlagIter == 3) {
						nokia_lcd_write_char('<', 1);
					} else {
						nokia_lcd_write_char(' ', 1);
					}
					
					nokia_lcd_render();
				} 
				else if(playTempoFlag == 1) {
					nokia_lcd_clear();
					nokia_lcd_write_string("Playing Tempo:", 1);
					nokia_lcd_write_char('0' + TEMPO / 100, 4);
					nokia_lcd_write_char('0' + (TEMPO % 100) / 10, 4);
					nokia_lcd_write_char('0' + (TEMPO % 10), 4);
					nokia_lcd_render();
				} 
				else if(setTempoFlag == 1) {
					nokia_lcd_clear();
					nokia_lcd_write_string("Set Tempo:    ", 1);
					nokia_lcd_write_char('0' + TEMPO / 100, 1);
					nokia_lcd_write_char('0' + (TEMPO % 100) / 10, 1);
					nokia_lcd_write_char('0' + (TEMPO % 10), 1);
					
					if(tempoVarArrIter == 0) { nokia_lcd_write_char('<', 1); }
					else { nokia_lcd_write_char(' ', 1); }
					
					nokia_lcd_write_string("          ", 1);
					
					nokia_lcd_write_string("Set Beat:     ", 1);
					
					nokia_lcd_write_char('0' + BEAT, 1);
					
					if(tempoVarArrIter == 1) { nokia_lcd_write_char('<', 1); }
					else { nokia_lcd_write_char(' ', 1); }
					
					
					nokia_lcd_render();
				} 
				else if(playNoteFlag == 1) {
					nokia_lcd_clear();
					
					nokia_lcd_write_string("Play/Set Note:", 1);
					nokia_lcd_write_char(noteArray[noteArrayIter], 1);
					
					nokia_lcd_write_string("             ", 1);
					
					nokia_lcd_write_string("Octave:       ", 1);
					nokia_lcd_write_char('3' + octaveIter, 1);
					
					nokia_lcd_render();
				} 
				else if(tunerFlag == 1) {
					nokia_lcd_clear();
					nokia_lcd_write_string("Mic Output:   ", 1);
					
					nokia_lcd_write_char('0' + (micOutput / 10000), 1); //ten thousands
					nokia_lcd_write_char('0' + (micOutput % 10000) / 1000, 1); //thousands
					nokia_lcd_write_char('0' + (micOutput % 1000) / 100, 1); //hun
					nokia_lcd_write_char('0' + (micOutput % 100 / 10), 1); //ten
					nokia_lcd_write_char('0' + micOutput % 10, 1); //one
					
					nokia_lcd_render();
				}
				updateDisplay = 0;
			}
			state = display;
			break;
			
		default:
			state = display;
			break;
	}
	
	return state;	
}

/******************************************************/

#define LEFT_TO_RIGHT_SIZE	16

//pattern_one
char leftToRight_rows[LEFT_TO_RIGHT_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
char leftToRight_columns[LEFT_TO_RIGHT_SIZE] = {0x7F, 0x3F, 0x9F, 0xCF, 0xE7, 0xF3, 0xF9, 0xFC, 0xFE, 0xFC, 0xF9, 0xF3, 0xE7, 0xCF, 0x9F, 0x3F};

//pattern_two
#define NUMBER_MATRIX_MAX 8

char zero_rows[NUMBER_MATRIX_MAX] = {0x7E, 0xFF, 0xC3, 0x81, 0x81, 0xC3, 0xFF, 0x7E}; //char zero_rows[NUMBER_MATRIX_MAX] = {0x7E, 0xFF, 0xC3, 0x8D, 0xB1, 0xC3, 0xFF, 0x7E};
char zero_cols[NUMBER_MATRIX_MAX] = {0x7F, 0xBF, 0xCF, 0xEF, 0xF7, 0xFB, 0xFC, 0xFE}; //char zero_cols[NUMBER_MATRIX_MAX] = {0x7F, 0xBF, 0xCF, 0xEF, 0xF7, 0xFB, 0xFC, 0xFE};

char one_rows[NUMBER_MATRIX_MAX] = {0x41, 0x41, 0x41, 0xFF, 0xFF, 0xFF, 0x01, 0x01}; //optimized
char one_cols[NUMBER_MATRIX_MAX] = {0x9F, 0x9F, 0x9F, 0xE7, 0xE7, 0xE7, 0xF9, 0xF9}; //optimized

char two_rows[NUMBER_MATRIX_MAX] = {0x63, 0x67, 0xCF, 0xCF, 0xDB, 0xDB, 0x73, 0x73};
char two_cols[NUMBER_MATRIX_MAX] = {0x7F, 0xBF, 0xCF, 0xEF, 0xF7, 0xFB, 0xFC, 0xFE};

char three_rows[NUMBER_MATRIX_MAX] = {0x42, 0xC3, 0xDB, 0xDB, 0xDB, 0xFF, 0x66, 0x00};
char three_cols[NUMBER_MATRIX_MAX] = {0x7F, 0xBF, 0xCF, 0xEF, 0xF7, 0xFB, 0xFC, 0xFE};

char four_rows[NUMBER_MATRIX_MAX] = {0xF0, 0xF8, 0x18, 0x18, 0x18, 0x18, 0xFF, 0xFF};
char four_cols[NUMBER_MATRIX_MAX] = {0x7F, 0xBF, 0xCF, 0xEF, 0xF7, 0xFB, 0xFC, 0xFE};

char five_rows[NUMBER_MATRIX_MAX] = {0xF3, 0xF3, 0xB3, 0xB3, 0xB3, 0xB3, 0xBF, 0x1E};
char five_cols[NUMBER_MATRIX_MAX] = {0x7F, 0xBF, 0xCF, 0xEF, 0xF7, 0xFB, 0xFC, 0xFE};

char six_rows[NUMBER_MATRIX_MAX] = {0x7E, 0xFF, 0xDB, 0x9B, 0x9B, 0x9B, 0x9F, 0x0E};
char six_cols[NUMBER_MATRIX_MAX] = {0x7F, 0xBF, 0xCF, 0xEF, 0xF7, 0xFB, 0xFC, 0xFE};

//pattern_three
char flash_rows[3] = {0xFF, 0xFF, 0x18};
char flash_cols[3] = {0xFF, 0x00, 0xE7};

//playNote
#define NOTES_MATRIX_MAX 8
char c_rows[NOTES_MATRIX_MAX] = {0x7E, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3, 0xF7, 0x76};
char c_cols[NOTES_MATRIX_MAX] = {0x7F, 0xBF, 0xCF, 0xEF, 0xF7, 0xFB, 0xFC, 0xFE};
	
char d_rows[NOTES_MATRIX_MAX] = {0xFF, 0xFF, 0xC3, 0xC3, 0xC3, 0xE7, 0x7E, 0x3C};
char d_cols[NOTES_MATRIX_MAX] = {0x7F, 0xBF, 0xCF, 0xEF, 0xF7, 0xFB, 0xFC, 0xFE};
	
char e_rows[NOTES_MATRIX_MAX] = {0xFF, 0xFF, 0xDB, 0xDb, 0xC3, 0xC3, 0xC3, 0x00};
char e_cols[NOTES_MATRIX_MAX] = {0x7F, 0xBF, 0xCF, 0xEF, 0xF7, 0xFB, 0xFC, 0xFE};
	
char f_rows[NOTES_MATRIX_MAX] = {0xFF, 0xFF, 0xD8, 0xD8, 0xD0, 0xC0, 0xC0, 0x00};
char f_cols[NOTES_MATRIX_MAX] = {0x7F, 0xBF, 0xCF, 0xEF, 0xF7, 0xFB, 0xFC, 0xFE};
	
char g_rows[NOTES_MATRIX_MAX] = {0x7E, 0xFF, 0xC3, 0xC3, 0xDB, 0xDB, 0xDF, 0xDE};
char g_cols[NOTES_MATRIX_MAX] = {0x7F, 0xBF, 0xCF, 0xEF, 0xF7, 0xFB, 0xFC, 0xFE};

char a_rows[NOTES_MATRIX_MAX] = {0x7F, 0xFF, 0xCC, 0xCC, 0xCC, 0xCC, 0xFF, 0x7F};
char a_cols[NOTES_MATRIX_MAX] = {0x7F, 0xBF, 0xCF, 0xEF, 0xF7, 0xFB, 0xFC, 0xFE};

char b_rows[NOTES_MATRIX_MAX] = {0xFF, 0xFF, 0xDB, 0xDB, 0xDB, 0xFF, 0x66, 0x00};
char b_cols[NOTES_MATRIX_MAX] = {0x7F, 0xBF, 0xCF, 0xEF, 0xF7, 0xFB, 0xFC, 0xFE};

unsigned char graphicIter = 0;

enum matrixDisplay{displayMatrix, idle_displayMatrix};
int matrixDisplay(int state) {
	
	//unsigned short TEMPO_DIV_8 = (tempoToMs(eeprom_read_byte(TEMPO_ADDR)) / 4);
	
	switch(state) {
		
		case displayMatrix:
			if (playTempoFlag == 1) {
				if(pattern_one) {
					state = displayMatrix;
					transmit_data_lower_nibble(leftToRight_rows[matrixIter]);
					transmit_data_upper_nibble(leftToRight_columns[matrixIter]);
					pattern_one = 0;
				} else if(pattern_two) {
					state = displayMatrix;
						if(matrixCnt >= 3) {
							if(BEAT == 0) {
								transmit_data_lower_nibble(zero_rows[graphicIter]);
								transmit_data_upper_nibble(zero_cols[graphicIter]);
								graphicIter = (graphicIter < NUMBER_MATRIX_MAX - 1) ? graphicIter+1 : 0;
							}
							else if(currentBeat == 1) {
								transmit_data_upper_nibble(one_cols[graphicIter]);
								transmit_data_lower_nibble(one_rows[graphicIter]);
								graphicIter = (graphicIter < NUMBER_MATRIX_MAX - 1) ? graphicIter+1 : 0;
							}
							else if(currentBeat == 2) {
								transmit_data_lower_nibble(two_rows[graphicIter]);
								transmit_data_upper_nibble(two_cols[graphicIter]);
								graphicIter = (graphicIter < NUMBER_MATRIX_MAX - 1) ? graphicIter+1 : 0;
							}
							else if(currentBeat == 3) {
								transmit_data_lower_nibble(three_rows[graphicIter]);
								transmit_data_upper_nibble(three_cols[graphicIter]);
								graphicIter = (graphicIter < NUMBER_MATRIX_MAX - 1) ? graphicIter+1 : 0;
							}
							else if(currentBeat == 4) {
								transmit_data_lower_nibble(four_rows[graphicIter]);
								transmit_data_upper_nibble(four_cols[graphicIter]);
								graphicIter = (graphicIter < NUMBER_MATRIX_MAX - 1) ? graphicIter+1 : 0;
							}
							else if(currentBeat == 5) {
								transmit_data_lower_nibble(five_rows[graphicIter]);
								transmit_data_upper_nibble(five_cols[graphicIter]);
								graphicIter = (graphicIter < NUMBER_MATRIX_MAX - 1) ? graphicIter+1 : 0;
							}
							else if(currentBeat == 6) {
								transmit_data_lower_nibble(six_rows[graphicIter]);
								transmit_data_upper_nibble(six_cols[graphicIter]);
								graphicIter = (graphicIter < NUMBER_MATRIX_MAX - 1) ? graphicIter+1 : 0;
							}
							matrixCnt = 0;
						}
					matrixCnt++;
				} else if(pattern_three) {
					transmit_data_lower_nibble(flash_rows[matrixIter]);
					transmit_data_upper_nibble(flash_cols[matrixIter]);
					pattern_three = 0;
				}
			}
			else if(playNoteFlag == 1) {
				if(matrixCnt >= 3) {
						if(noteArrayIter == 0) {
							//display c
							transmit_data_lower_nibble(c_rows[graphicIter]);
							transmit_data_upper_nibble(c_cols[graphicIter]);
							graphicIter = (graphicIter < NOTES_MATRIX_MAX - 1) ? graphicIter+1 : 0;
						} 
						else if(noteArrayIter == 1) {
							//display d
							transmit_data_lower_nibble(d_rows[graphicIter]);
							transmit_data_upper_nibble(d_cols[graphicIter]);
							graphicIter = (graphicIter < NOTES_MATRIX_MAX - 1) ? graphicIter+1 : 0;
						} 
						else if(noteArrayIter == 2) {
							//display e
							transmit_data_lower_nibble(e_rows[graphicIter]);
							transmit_data_upper_nibble(e_cols[graphicIter]);
							graphicIter = (graphicIter < NOTES_MATRIX_MAX - 1) ? graphicIter+1 : 0;
						} 
						else if(noteArrayIter == 3) {
							//display f
							transmit_data_lower_nibble(f_rows[graphicIter]);
							transmit_data_upper_nibble(f_cols[graphicIter]);
							graphicIter = (graphicIter < NOTES_MATRIX_MAX - 1) ? graphicIter+1 : 0;
						} 
						else if(noteArrayIter == 4) {
							//display g
							transmit_data_lower_nibble(g_rows[graphicIter]);
							transmit_data_upper_nibble(g_cols[graphicIter]);
							graphicIter = (graphicIter < NOTES_MATRIX_MAX - 1) ? graphicIter+1 : 0;
						} 
						else if(noteArrayIter == 5) {
							//display a
							transmit_data_lower_nibble(a_rows[graphicIter]);
							transmit_data_upper_nibble(a_cols[graphicIter]);
							graphicIter = (graphicIter < NOTES_MATRIX_MAX - 1) ? graphicIter+1 : 0;
						} 
						else if(noteArrayIter == 6) {
							//display b
							transmit_data_lower_nibble(b_rows[graphicIter]);
							transmit_data_upper_nibble(b_cols[graphicIter]);
							graphicIter = (graphicIter < NOTES_MATRIX_MAX - 1) ? graphicIter+1 : 0;
						}
						matrixCnt = 0;
					}
				matrixCnt++;
				} 
			else {
				state = idle_displayMatrix;
				matrixIter = 0;
				matrixCnt = 0;
				transmit_data_lower_nibble(0x00);
				transmit_data_upper_nibble(0xFF);
			}	
			break;
		
		case idle_displayMatrix:
			if(playTempoFlag != 0 || playNoteFlag != 0) {
				state = displayMatrix;
			} else {
				state = idle_displayMatrix;
			}
			break;
			
		default:
			state = idle_displayMatrix;
			break;
		}
		
	switch(state) {
		case displayMatrix:
			break;
		
		case idle_displayMatrix:
			matrixIter = 0;
			matrixCnt = 0;
			break;
			
		default:
			break;
	}
		
	return state;
	
}


/******************************************************/

int main(void) {
	
	TEMPO = eeprom_read_byte((uint8_t*)TEMPO_ADDR);
	BEAT = eeprom_read_byte((uint8_t*)BEAT_ADDR);
	noteArrayIter = eeprom_read_byte((uint8_t*)noteArrayIter_ADDR);
	octaveIter = eeprom_read_byte((uint8_t*)octaveIter_ADDR);
	
	const unsigned long timerPeriod = 1;

	//HIGH = input, LOW = output
	DDRA = 0x00; PORTA = 0xFF; // A[0] = ADC; A[1:3] = buttons; A[6:7] = LCD E/RW;
	DDRB = 0xFF; PORTB = 0x00; // B[0:5] =  Nokia5110, B[6] = Piezo Buzzer
	DDRC = 0xFF; PORTC = 0x00; // Nothing no mo
	DDRD = 0xFF; PORTD = 0x00; // LED Matrix
	
	const unsigned short tasksNum = 8; //Make sure to change later on
	unsigned char iter = 0;
	task tasks[tasksNum];
	
	//menu
	tasks[iter].state = idle_menu;
	tasks[iter].period = 1;
	tasks[iter].elapsedTime = 0;
	tasks[iter].TickFct = &menu;
	iter++;
	
	//playTempoHelper 
	tasks[iter].state = flagIsDown;
	tasks[iter].period = 1;
	tasks[iter].elapsedTime = 0;
	tasks[iter].TickFct = &playTempoHelper;
	iter++;
	
	//playTempo
	tasks[iter].state = idle_playTempo;
	tasks[iter].period = 1;
	tasks[iter].elapsedTime = 0;
	tasks[iter].TickFct = &playTempo;
	iter++;
	
	//setTempoState
	tasks[iter].state = idle_setTempo;
	tasks[iter].period = 1;
	tasks[iter].elapsedTime = 0;
	tasks[iter].TickFct = &setTempo;
	iter++;
	
	//playNote
	tasks[iter].state = idle_playNote;
	tasks[iter].period = 1;
	tasks[iter].elapsedTime = 0;
	tasks[iter].TickFct = &playNote;
	iter++;
	
	//tuner
	tasks[iter].state = idle_tuner;
	tasks[iter].period = 1;
	tasks[iter].elapsedTime = 0;
	tasks[iter].TickFct = &tuner;
	iter++;
	
	//displayLCD
	tasks[iter].state = firstDisplay;
	tasks[iter].period = 1;
	tasks[iter].elapsedTime = 0;
	tasks[iter].TickFct = &displayLCD;
	iter++;
	
	//displayMatrix
	tasks[iter].state = idle_displayMatrix;
	tasks[iter].period = 1;
	tasks[iter].elapsedTime = 0;
	tasks[iter].TickFct = &matrixDisplay;
	iter++;

	ADC_init();

	nokia_lcd_init();

	TimerSet(timerPeriod);
	TimerOn();
	
	while(1) {
		//task array
		for(unsigned i = 0; i < tasksNum; i++) {
			if(tasks[i].elapsedTime >= tasks[i].period) {
				tasks[i].state = tasks[i].TickFct(tasks[i].state);
				tasks[i].elapsedTime = 0;
			}
			tasks[i].elapsedTime += timerPeriod;
		}
		
		while(!TimerFlag);
		TimerFlag = 0;
	}

	return 0;
}