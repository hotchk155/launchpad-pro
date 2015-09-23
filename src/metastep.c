
//______________________________________________________________________________
//
// Headers
//______________________________________________________________________________

#include "app.h"


#define SHIFT_YELLOW	39
#define SHIFT_GREEN		29
#define SHIFT_RED		19


#define LED_RED 	0xFF0000U
#define LED_GREEN 	0x00FF00U
#define LED_BLUE 	0x0000FFU
#define LED_YELLOW 	0xFFFF00U

#define SEQ_GREEN	0x01
#define SEQ_RED		0x02

#define NULL (void*)0

typedef unsigned char byte;

/*******************************************************************************
 *
 * API FOR SEQUENCER SKETCHES TO CALL
 *
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

typedef unsigned int COLOUR;

/*******************************************************************************
 *
 * DEFINE TYPES FOR SEQUENECR SKETCHES
 *
 */

typedef void(*SqInitFunc)(void *this);
typedef void(*SqStepFunc)(void *this);
typedef void(*SqRedrawFunc)(void *this);
typedef void(*SqGridPressFunc)(void *this, byte row, byte col, byte press);
typedef void(*SqMenuButtonFunc)(void *this, MNU_BUTTON which, byte press);

typedef struct _SQ_FUNCTIONS {
	SqInitFunc				init;
	SqStepFunc				step;
	SqRedrawFunc			redraw;
	SqGridPressFunc			gridButton;
	SqMenuButtonFunc		menuButton;
} SQ_FUNCTIONS;

typedef SQ_FUNCTIONS* PSEQUENCER;

PSEQUENCER activeSequencer = NULL;



/*******************************************************************************
 *
 * METASTEP SEQUENCER DEFINITION
 *
 */

extern void metastepInit(void *ptr);
extern void metastepStep(void *ptr);
extern void metastepRedraw(void *ptr);
extern void metastepGridButton(void *ptr, byte row, byte col, byte press);
extern void metastepMenuButton(void *ptr, MNU_BUTTON which, byte press);

typedef struct _METASTEP_SEQ {
	SQ_FUNCTIONS handlers;
	int stepNumber;
	byte grid[8][8];
} METASTEP_SEQ;


METASTEP_SEQ metastepInstance = {
		{
				// The handler functions
				metastepInit,
				metastepStep,
				metastepRedraw,
				metastepGridButton,
				metastepMenuButton
		}
};



/******************************************************************************
 Button indexing is as follows - numbers in brackets do not correspond to real
buttons, but can be harmessly sent in hal_set_led.

 (90)91 92 93 94 95 96 97 98 (99)
 80 .......
 70 .......
 60 .......
 50 .......
 40 .......
 30 .......
 20  21 22 23 24 25 26 27 28  29
 10  11 12 13 14 15 16 17 18  19
 (0)  1  2  3  4  5  6  7  8  (9)

 *****************************************************************************/



void gridLed(int row, int col, COLOUR colour) {
	if(row >= 0 && row < 8 && col >= 0 && col < 8) {
		int index = col + 81 - 10*row;
		hal_plot_led(TYPEPAD, index, (colour>>16)&0xFF, (colour>>8)&0xFF, colour&0xFF);
	}
}
void menuLed(MNU_BUTTON button, COLOUR colour) {
	switch(button) {
	case MNU_ARROW0:
	case MNU_ARROW1:
	case MNU_ARROW2:
	case MNU_ARROW3:
	case MNU_ARROW4:
	case MNU_ARROW5:
	case MNU_ARROW6:
	case MNU_ARROW7:
		hal_plot_led(TYPEPAD, 89 + 10*(button - MNU_ARROW0), (colour>>16)&0xFF, (colour>>8)&0xFF, colour&0xFF);
		break;

	case MNU_UP:
	case MNU_DOWN:
	case MNU_LEFT:
	case MNU_RIGHT:
	case MNU_SESSION:
	case MNU_NOTE:
	case MNU_DEVICE:
	case MNU_USER:
		hal_plot_led(TYPEPAD, 91 + (button - MNU_UP), (colour>>16)&0xFF, (colour>>8)&0xFF, colour&0xFF);
		break;

	case MNU_SHIFT:
	case MNU_CLICK:
	case MNU_UNDO:
	case MNU_DELETE:
	case MNU_QUANTISE:
	case MNU_DUPLICATE:
	case MNU_DOUBLE:
	case MNU_RECORD:
		hal_plot_led(TYPEPAD, 80 + 10*(button - MNU_SHIFT), (colour>>16)&0xFF, (colour>>8)&0xFF, colour&0xFF);
		break;

	case MNU_RECORDARM:
	case MNU_TRACKSELECT:
	case MNU_MUTE:
	case MNU_SOLO:
	case MNU_VOLUME:
	case MNU_PAN:
	case MNU_SENDS:
	case MNU_STOPCLIP:
		hal_plot_led(TYPEPAD, 1 + (button - MNU_RECORDARM), (colour>>16)&0xFF, (colour>>8)&0xFF, colour&0xFF);
		break;

	case MNU_SETUP:
		hal_plot_led(TYPESETUP, 0, (colour>>16)&0xFF, (colour>>8)&0xFF, colour&0xFF);
		break;
	}
}



void metastepInit(void *ptr) {
	METASTEP_SEQ *this = (METASTEP_SEQ *)ptr;
	this->stepNumber = -1;
	for(int i=0; i<sizeof(this->grid); ++i) {
		((byte*)this->grid)[i] = 0;
	}
	/*
	setLedByIndex(SHIFT_YELLOW, LED_YELLOW);
	setLedByIndex(SHIFT_RED, LED_RED);
	setLedByIndex(SHIFT_GREEN, LED_GREEN);
	*/
}

void metastepStep(void *ptr) {
}

void metastepRedraw(void *ptr) {

}

void metastepMenuButton(void *ptr, MNU_BUTTON which, byte press) {
	if(press) {
		menuLed(which, 0xFFFFFF);
	}
	else {
		menuLed(which, 0);
	}
}

void metastepGridButton(void *ptr, byte row, byte col, byte press) {
	METASTEP_SEQ *this = (METASTEP_SEQ *)ptr;
	if(press) {
		/*
		if(row >= 6) {
			this->grid[row][col]^=SEQ_GREEN;
			unsigned long colour = 0;
			if(this->grid[row][col]&SEQ_GREEN) {
				colour|=LED_GREEN;
			}
			setLed(row,col,colour);
		}*/

		gridLed(row,col,0xFFFFFF);
	}
	else {
		gridLed(row,col,0);
	}
}


//______________________________________________________________________________
//
// This is where the fun is!  Add your code to the callbacks below to define how
// your app behaves.

//______________________________________________________________________________


/******************************************************************************
 Button indexing is as follows - numbers in brackets do not correspond to real
buttons, but can be harmessly sent in hal_set_led.

 (90)91 92 93 94 95 96 97 98 (99)
 80 .......
 70 .......
 60 .......
 50 .......
 40 .......
 30 .......
 20  21 22 23 24 25 26 27 28  29
 10  11 12 13 14 15 16 17 18  19
 (0)  1  2  3  4  5  6  7  8  (9)

 *****************************************************************************/

void app_surface_event(u8 type, u8 index, u8 value)
{
	switch (type)
	{
		case  TYPEPAD:
			if(index < 10) {
				activeSequencer->menuButton(activeSequencer, MNU_RECORDARM + (index-1), !!value);
			}
			else if(index < 89) {
				switch(index%10) {
				case 0:
					activeSequencer->menuButton(activeSequencer, MNU_SHIFT + (8-index/10), !!value);
					break;
				case 9:
					activeSequencer->menuButton(activeSequencer, MNU_ARROW0 + (8-index/10), !!value);
					break;
				default:
					activeSequencer->gridButton(activeSequencer, 8-index/10, index%10-1, !!value);
					break;
				}
			}
			else if(index < 99) {
				activeSequencer->menuButton(activeSequencer, MNU_UP+ (index-91), !!value);
			}
			break;
		case  TYPESETUP:
			activeSequencer->menuButton(activeSequencer, MNU_SETUP, !!value);
			break;

	}
}

//______________________________________________________________________________

void app_midi_event(u8 port, u8 status, u8 d1, u8 d2)
{
}

//______________________________________________________________________________

void app_sysex_event(u8 port, u8 * data, u16 count)
{

}

//______________________________________________________________________________

void app_aftertouch_event(u8 index, u8 value)
{
}

//______________________________________________________________________________

void app_cable_event(u8 type, u8 value)
{
}

//______________________________________________________________________________


void app_timer_event()
{
}

//______________________________________________________________________________

void app_init()
{
	activeSequencer = &metastepInstance.handlers;
	activeSequencer->init(activeSequencer);
}

