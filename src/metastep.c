/*
 * LAUNCHPAD PRO SEQUENCER FRAMEWORK
 * hotchk155/2015
 *
 * This file defines a "meta-stepping" sequencer
 *
 */
#include "app.h"
#include "sequencer.h"

/*
 * Forward declarations of the handler functions that
 * need to be exposed to the sequencer core
 */
static void seqInit(void *ptr);
static void seqDone(void *ptr);
static void seqTick(void *ptr);
static void seqStep(void *ptr);
static void seqRedraw(void *ptr);
static void seqGridButton(void *ptr, byte row, byte col, byte press);
static void seqMenuButton(void *ptr, MNU_BUTTON which, byte press);

/*
 * A type to track the state of a single grid cell
 */
typedef struct _CELL_TYPE {
	byte note;	/* MIDI note */
	byte flags; /* Flag values */
	byte ping;  /* Used to indicate visual "ping" of a cell */
} CELL_TYPE;

/*
 * Flag bit values
 */
#define SEQ_GREEN	0x01
#define SEQ_RED		0x02
#define SEQ_YELLOW	(SEQ_GREEN|SEQ_RED)
#define SEQ_NOTE	0x40
#define SEQ_SHARP	0x80

/*
 * A type to define state of an instance of the sequencer
 */
typedef struct _SEQ_TYPE {
	SQ_HANDLERS handlers;
	int stepNumber;
	byte shiftKey;
	byte redNote;
	byte greenNote;
	byte counter;
	CELL_TYPE grid[8][8];
} SEQ_TYPE;

/*
 * Assign various menu buttons to sequencer functions
 */
enum {
	SHIFT_YELLOW = MNU_ARROW5,
	SHIFT_GREEN = MNU_ARROW6,
	SHIFT_RED = MNU_ARROW7
};

/*
 * Instantiate the sequencer - must ensure that the handlers structure
 * is populated correctly!
 *
 */
static SEQ_TYPE seqInstance = {
		{
				// The handler functions
				seqInit,
				seqDone,
				seqTick,
				seqStep,
				seqRedraw,
				seqGridButton,
				seqMenuButton
		},
		0
};

/*
 * The single "create" function that is exported from this module
 */
PSEQUENCER metastepCreate() {
	return &seqInstance.handlers;
}


/*
 * Seek the next note to play of a given colour
 */
static void findNextNote(SEQ_TYPE *this, byte isRed) {

	CELL_TYPE *nextNote = NULL;
	CELL_TYPE *firstNote = NULL;

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

/*
 * establish the state of a grid cell and update the LED to the
 * appropriate colour
 */
static void updateGridLed(SEQ_TYPE *this, byte row, byte col) {
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

/*
 * Helper function Initialise a row of notes on the grid
 */
static void initNotes(SEQ_TYPE *this, byte row, byte note) {
	for(int i=0; i<7; ++i) {
		this->grid[row+1][i].flags = SEQ_NOTE;
		this->grid[row+1][i].note = note++;
		switch(i) {
		case 0: case 1: case 3: case 4: case 5:
			this->grid[row][i].flags = SEQ_NOTE|SEQ_SHARP;
			this->grid[row][i].note = note++;
		}
		updateGridLed(this, row, i);
		updateGridLed(this, row+1, i);
	}
}

/*
 * Handler to initialise the sequencer
 */
static void seqInit(void *ptr) {
	SEQ_TYPE *this = (SEQ_TYPE *)ptr;
	this->stepNumber = -1;
	menuLed(SHIFT_YELLOW, COLOUR_YELLOW);
	menuLed(SHIFT_RED, COLOUR_RED);
	menuLed(SHIFT_GREEN, COLOUR_GREEN);
	int baseNote = 24;
	initNotes(this, 4, baseNote);
	initNotes(this, 2, baseNote + 12);
	initNotes(this, 0, baseNote + 24);

}

/*
 * Handler to deinitialise the sequencer
 */
static void seqDone(void *ptr) {
}


/*
 * Handler called every millisecond
 */
static void seqTick(void *ptr) {
	SEQ_TYPE *this = (SEQ_TYPE *)ptr;
	++this->counter;
	if(!(this->counter & 0x3F)) {
		for(int row=0; row < 8; ++row) {
			for(int col=0; col< 8; ++col) {
				if(this->grid[row][col].ping) {
					this->grid[row][col].ping/=2;
					updateGridLed(this, row, col);
				}
			}
		}
	}
}

/*
 * Handler called every "step" (beat)
 */
static void seqStep(void *ptr) {
	SEQ_TYPE *this = (SEQ_TYPE *)ptr;

	// move the step marker
	int s = this->stepNumber;
	if(++this->stepNumber >= 16) {
		this->stepNumber = 0;
	}
	updateGridLed(this, 6 + s/8, s%8);

	byte row = 6 + this->stepNumber/8;
	byte col = this->stepNumber%8;
	updateGridLed(this, row, col);


	if(this->grid[row][col].flags & SEQ_RED) {
		findNextNote(this, TRUE);
	}
	if(this->grid[row][col].flags & SEQ_GREEN) {
		findNextNote(this, FALSE);
	}


}

/*
 * Handler called to fully redraw the sequencer
 */
static void seqRedraw(void *ptr) {
	//SEQ_TYPE *this = (SEQ_TYPE *)ptr;
}

/*
 * Handler when a menu button is pressed
 */
static void seqMenuButton(void *ptr, MNU_BUTTON which, byte press) {
	SEQ_TYPE *this = (SEQ_TYPE *)ptr;
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

/*
 * Handler when a grid button is pressed
 */
static void seqGridButton(void *ptr, byte row, byte col, byte press) {
	SEQ_TYPE *this = (SEQ_TYPE *)ptr;
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
		updateGridLed(this, row, col);
	}
}
