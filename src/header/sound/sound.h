#include <stdint.h>
#include "header/cpu/portio.h"  // Assuming this contains out() and in()

#define NOTE_C4  261
#define NOTE_D4  294
#define NOTE_E4  329
#define NOTE_F4  349
#define NOTE_G4  391
#define NOTE_A4  440
#define NOTE_B4  493
#define NOTE_C5  523
#define REST     0

void play_sound(uint32_t freq);

void nosound() ;

void delay(int ms) ;

void play_melody(uint32_t* m, int length);