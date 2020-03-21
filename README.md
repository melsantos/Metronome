# Metronome
A metronome built on a breadboard. Can handle tempos between 40 bpm to 180 bpm.

# Hardware
 + ATmega1284 microcontroller: The unit responsible for running the whole system
 + 8x8 LED Matrix: A moving display that simulates pendulum motion of physical metronomes
 + Electret Microphone: Gets the user's audio
 + Buttons: For navigating the menu
 + Nokia 5110: Displays the menu
 + Piezo Buzzer: Plays a selected tuning note

# Overview
The system has four modes, 
 1. Metronome: Plays the tempo that has been set. User can cycle through the LED matrix displays. 
 2. Plays a note selected by the user.
 3. Sets the tempo for the metronome.
 4. Tuner: Processes the given audio to detect pitch accuracy.

