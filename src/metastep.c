
//______________________________________________________________________________
//
// Headers
//______________________________________________________________________________

#include "app.h"




#define COLOUR_RED 		0xFF0000U
#define COLOUR_GREEN 	0x00FF00U
#define COLOUR_BLUE 	0x0000FFU
#define COLOUR_YELLOW 	(COLOUR_RED|COLOUR_GREEN)
#define COLOUR_WHITE	(COLOUR_RED|COLOUR_GREEN|COLOUR_BLUE)

#define SEQ_GREEN	0x01
#define SEQ_RED		0x02
#define SEQ_YELLOW	(SEQ_GREEN|SEQ_RED)
#define SEQ_NOTE	0x40
#define SEQ_SHARP	0x80

#define NULL (void*)0
#define TRUE 1
#define FALSE 0

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
typedef void(*SqTickFunc)(void *this);
typedef void(*SqStepFunc)(void *this);
typedef void(*SqRedrawFunc)(void *this);
typedef void(*SqGridPressFunc)(void *this, byte row, byte col, byte press);
typedef void(*SqMenuButtonFunc)(void *this, MNU_BUTTON which, byte press);

typedef struct _SQ_FUNCTIONS {
	SqInitFunc				init;
	SqTickFunc				tick;
	SqStepFunc				step;
	SqRedrawFunc			redraw;
	SqGridPressFunc			gridButton;
	SqMenuButtonFunc		menuButton;
} SQ_FUNCTIONS;

typedef SQ_FUNCTIONS* PSEQUENCER;

PSEQUENCER activeSequencer = NULL;
unsigned long milliseconds;
double stepRate;
double nextStepTime;



/*******************************************************************************
 *
 * METASTEP SEQUENCER DEFINITION
 *
 */

extern void metastepInit(void *ptr);
extern void metastepTick(void *ptr);
extern void metastepStep(void *ptr);
extern void metastepRedraw(void *ptr);
extern void metastepGridButton(void *ptr, byte row, byte col, byte press);
extern void metastepMenuButton(void *ptr, MNU_BUTTON which, byte press);

typedef struct _METASTEP_CELL {
	byte note;
	byte flags;
	byte ping;
} METASTEP_CELL;
typedef struct _METASTEP_SEQ {
	SQ_FUNCTIONS handlers;
	int stepNumber;
	byte shiftKey;
	byte redNote;
	byte greenNote;
	byte counter;
	METASTEP_CELL grid[8][8];
} METASTEP_SEQ;

enum {
	SHIFT_YELLOW = MNU_ARROW5,
	SHIFT_GREEN = MNU_ARROW6,
	SHIFT_RED = MNU_ARROW7
};


METASTEP_SEQ metastepInstance = {
		{
				// The handler functions
				metastepInit,
				metastepTick,
				metastepStep,
				metastepRedraw,
				metastepGridButton,
				metastepMenuButton
		},
		0
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
		hal_plot_led(TYPEPAD, 89 - 10*(button - MNU_ARROW0), (colour>>16)&0xFF, (colour>>8)&0xFF, colour&0xFF);
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
		hal_plot_led(TYPEPAD, 80 - 10*(button - MNU_SHIFT), (colour>>16)&0xFF, (colour>>8)&0xFF, colour&0xFF);
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

/*
 * Seek the next note to play of a given colour
 */
void metastepNextNote(METASTEP_SEQ *this, byte isRed) {

	METASTEP_CELL *nextNote = NULL;
	METASTEP_CELL *firstNote = NULL;

	// check the last note played for this colour
	byte lastNote = isRed ? this->redNote : this->greenNote;
	byte mask = isRed ? SEQ_RED : SEQ_GREEN;

	// scan over the keyboard part of the display from lower
	// notes up to higher ones
	if(lastNote) {
		hal_send_midi(DINMIDI, 0x90, lastNote, 0x00);
	}

	for(int y=5; y>0 && !nextNote; y-=2  ) {
		for(int x=0; x<16; ++x) {
			int col = x/2;
			int row = y-x%2;
			if(this->grid[row][col].flags & mask) {
				// track the lowest note of the given colour
				if(!firstNote) {
					firstNote = &this->grid[row][col];
				}
				// look for the next highest note of the given colour
				if(this->grid[row][col].note > lastNote) {
					nextNote = &this->grid[row][col];
					break;
				}
			}
		}
	}
	if(!nextNote) {
		// no next note, use the first note
		nextNote = firstNote;
	}
	if(nextNote) {
		nextNote->ping = 0xFF;
		if(isRed) {
			this->redNote = nextNote->note;
		}
		else {
			this->greenNote = nextNote->note;
		}
		hal_send_midi(DINMIDI, 0x90, nextNote->note, 0x7F);
	}
}


void metastepUpdateGridLed(METASTEP_SEQ *this, byte row, byte col) {
	COLOUR colour = 0;
	if(this->grid[row][col].ping) {
		colour |= this->grid[row][col].ping;
		colour <<= 8;
		colour |= this->grid[row][col].ping;
		colour <<= 8;
		colour |= this->grid[row][col].ping;
	}
	else
	{
		int ii = (row - 6) * 8 + col;
		byte flags = this->grid[row][col].flags;
		if(row > 5 && ii == this->stepNumber) {
			colour = COLOUR_WHITE;
		}
		else if(flags & (SEQ_RED|SEQ_GREEN)){
			if(flags&SEQ_RED) {
				colour |= COLOUR_RED;
			}
			if(flags&SEQ_GREEN) {
				colour |= COLOUR_GREEN;
			}
		}
		else if(flags & SEQ_NOTE) {
			colour = 0x000011;
		}
	}
	gridLed(row, col, colour);
}

void metastepInitNotes(METASTEP_SEQ *this, byte row, byte note) {
	for(int i=0; i<7; ++i) {
		this->grid[row+1][i].flags = SEQ_NOTE;
		this->grid[row+1][i].note = note++;
		switch(i) {
		case 0: case 1: case 3: case 4: case 5:
			this->grid[row][i].flags = SEQ_NOTE|SEQ_SHARP;
			this->grid[row][i].note = note++;
		}
		metastepUpdateGridLed(this, row, i);
		metastepUpdateGridLed(this, row+1, i);
	}
}

void metastepInit(void *ptr) {
	METASTEP_SEQ *this = (METASTEP_SEQ *)ptr;
	this->stepNumber = -1;
	menuLed(SHIFT_YELLOW, COLOUR_YELLOW);
	menuLed(SHIFT_RED, COLOUR_RED);
	menuLed(SHIFT_GREEN, COLOUR_GREEN);
	int baseNote = 24;
	metastepInitNotes(this, 4, baseNote);
	metastepInitNotes(this, 2, baseNote + 12);
	metastepInitNotes(this, 0, baseNote + 24);

}

void metastepTick(void *ptr) {
	METASTEP_SEQ *this = (METASTEP_SEQ *)ptr;
	++this->counter;
	if(!(this->counter & 0x3F)) {
		for(int row=0; row < 8; ++row) {
			for(int col=0; col< 8; ++col) {
				if(this->grid[row][col].ping) {
					this->grid[row][col].ping/=2;
					metastepUpdateGridLed(this, row, col);
				}
			}
		}
	}
}

void metastepStep(void *ptr) {
	METASTEP_SEQ *this = (METASTEP_SEQ *)ptr;

	// move the step marker
	int s = this->stepNumber;
	if(++this->stepNumber >= 16) {
		this->stepNumber = 0;
	}
	metastepUpdateGridLed(this, 6 + s/8, s%8);

	byte row = 6 + this->stepNumber/8;
	byte col = this->stepNumber%8;
	metastepUpdateGridLed(this, row, col);


	if(this->grid[row][col].flags & SEQ_RED) {
		metastepNextNote(this, TRUE);
	}
	if(this->grid[row][col].flags & SEQ_GREEN) {
		metastepNextNote(this, FALSE);
	}


}

void metastepRedraw(void *ptr) {

}

void metastepMenuButton(void *ptr, MNU_BUTTON which, byte press) {
	METASTEP_SEQ *this = (METASTEP_SEQ *)ptr;
	switch(which) {
		case SHIFT_YELLOW:
		case SHIFT_GREEN:
		case SHIFT_RED:
			this->shiftKey = press? which : 0;
			break;
		default:
			break;
	}
}


void metastepGridButton(void *ptr, byte row, byte col, byte press) {
	METASTEP_SEQ *this = (METASTEP_SEQ *)ptr;
	byte *pflags= &this->grid[row][col].flags;
	if(press) {
		if(row>=6 || (*pflags&SEQ_NOTE))
		switch(this->shiftKey) {
		case SHIFT_YELLOW:
			if((*pflags & SEQ_YELLOW)==SEQ_YELLOW) {
				*pflags&=~SEQ_YELLOW;
			}
			else {
				*pflags|=SEQ_YELLOW;
			}
			break;
		case SHIFT_GREEN:
			*pflags^=SEQ_GREEN;
			break;
		case SHIFT_RED:
			*pflags^=SEQ_RED;
			break;
		default:
			*pflags&=~(SEQ_GREEN|SEQ_RED);
			break;
		}
		metastepUpdateGridLed(this, row, col);
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
	if(++milliseconds >= nextStepTime) {
		nextStepTime += stepRate;
		activeSequencer->step(activeSequencer);
	}
	activeSequencer->tick(activeSequencer);
}

//______________________________________________________________________________

void app_init()
{
	activeSequencer = &metastepInstance.handlers;
	milliseconds = 0;
	stepRate = 300.0;
	nextStepTime = stepRate;
	activeSequencer->init(activeSequencer);
}

