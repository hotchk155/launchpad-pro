/*
 * LAUNCHPAD PRO SEQUENCER FRAMEWORK
 * hotchk155/2015
 *
 * This file contains the general definitions for sequencer
 * sketches that run within the LaunchPad Pro API
 *
 */

#ifndef SEQUENCER_H_
#define SEQUENCER_H_

/*
 * Some basic definitions
 */
#define NULL ((void*)0L)
#define TRUE 1
#define FALSE 0
typedef unsigned char byte;

/*
 * Define some basic colours
 */
typedef unsigned int COLOUR;
#define COLOUR_NONE		0x000000U
#define COLOUR_RED 		0xFF0000U
#define COLOUR_GREEN 	0x00FF00U
#define COLOUR_BLUE 	0x0000FFU
#define COLOUR_YELLOW 	(COLOUR_RED|COLOUR_GREEN)
#define COLOUR_MAGENTA 	(COLOUR_RED|COLOUR_BLUE)
#define COLOUR_CYAN 	(COLOUR_GREEN|COLOUR_BLUE)
#define COLOUR_WHITE	(COLOUR_RED|COLOUR_GREEN|COLOUR_BLUE)

/*
 * Enumeration of the menu buttons around the outside of
 * the launchpad grid
 */
typedef enum {
	MNU_ARROW0 	= 0,
	MNU_ARROW1,
	MNU_ARROW2,
	MNU_ARROW3,
	MNU_ARROW4,
	MNU_ARROW5,
	MNU_ARROW6,
	MNU_ARROW7,

	MNU_UP,
	MNU_DOWN,
	MNU_LEFT,
	MNU_RIGHT,
	MNU_SESSION,
	MNU_NOTE,
	MNU_DEVICE,
	MNU_USER,

	MNU_SHIFT,
	MNU_CLICK,
	MNU_UNDO,
	MNU_DELETE,
	MNU_QUANTISE,
	MNU_DUPLICATE,
	MNU_DOUBLE,
	MNU_RECORD,

	MNU_RECORDARM,
	MNU_TRACKSELECT,
	MNU_MUTE,
	MNU_SOLO,
	MNU_VOLUME,
	MNU_PAN,
	MNU_SENDS,
	MNU_STOPCLIP,

	MNU_SETUP

} MNU_BUTTON;

/*
 * Logical channels which get mapped to real MIDI channels
 */
typedef enum {
	LCHAN_A,
	LCHAN_B
} LCHAN;


typedef enum {
	EVENT_NONE = 0,
	EVENT_TICK,
	EVENT_STEP,
	EVENT_START,
	EVENT_STOP,
	EVENT_RESET,
	EVENT_REPAINT,
} SQ_EVENT;

/*
 * types for callback functions that each sequencer sketch
 * needs to implement
 */
typedef void(*SqInitFunc)(void *this);
typedef void(*SqDoneFunc)(void *this);
typedef void(*SqEventFunc)(void *this, SQ_EVENT event);
typedef void(*SqGridPressFunc)(void *this, byte row, byte col, byte press);
typedef void(*SqMenuButtonFunc)(void *this, MNU_BUTTON which, byte press);

/*
 * each sequencer needs to implement the full interface defined
 * by this structure
 */
typedef struct _SQ_HANDLERS {
	SqInitFunc				init;
	SqDoneFunc				done;
	SqEventFunc				event;
	SqGridPressFunc			gridButton;
	SqMenuButtonFunc		menuButton;
} SQ_HANDLERS;
typedef SQ_HANDLERS* PSEQUENCER;

/*
 * Additional API wrapper functions that can be called by sequencers
 */
extern void gridLed(int row, int col, COLOUR colour);
extern void menuLed(MNU_BUTTON button, COLOUR colour);
extern void playNote(LCHAN lchan, byte note, byte velocity, unsigned duration);
extern void stopNote(LCHAN lchan, byte note);
extern void stopNotes();


#endif /* SEQUENCER_H_ */
