/*
 * LAUNCHPAD PRO SEQUENCER FRAMEWORK
 * hotchk155/2015
 *
 */
#include "app.h"
#include "sequencer.h"

/*
 * Declare the create functions for the various sequencer
 * patches here
 */
extern PSEQUENCER metastepCreate();

/*
 * pointer to the active sequencer
 *
 */
static PSEQUENCER activeSequencer = NULL;

/*
 * variables used for timing
 *
 */
static unsigned long milliseconds;
static unsigned long midiTicks;
static double stepRate;
static double nextStepTime;
static byte midiChannelA;
static byte midiChannelB;
static byte isRunning;
static byte synchMode;

enum {
	SYNCH_INTERNAL,
	SYNCH_MIDI
};

typedef struct {
	byte chan;
	byte note;
	unsigned long stopTime;
} NOTE;

#define MAX_NOTES 16

NOTE playingNotes[MAX_NOTES] = {{0}};

void playNote(LCHAN lchan, byte note, byte velocity, unsigned duration) {
	byte chan = 0;
	switch(lchan) {
	case LCHAN_A:
		chan = midiChannelA;
		break;
	case LCHAN_B:
		chan = midiChannelB;
		break;
	}
	NOTE *pnote = NULL;
	for(int i=0; i<MAX_NOTES; ++i) {
		if(playingNotes[i].note == note && playingNotes[i].chan == chan) {
			pnote = &playingNotes[i];
			break;
		}
	}
	if(!pnote) {
		for(int i=0; i<MAX_NOTES; ++i) {
			if(!playingNotes[i].note) {
				pnote = &playingNotes[i];
				break;
			}
		}
	}
	if(pnote) {
		pnote->chan = chan;
		pnote->note = note;
		pnote->stopTime = duration? (milliseconds + duration) : 0;
		hal_send_midi(DINMIDI, 0x90 + chan, note, velocity);
	}
}

void stopNote(LCHAN lchan, byte note) {
	byte chan = 0;
	switch(lchan) {
	case LCHAN_A:
		chan = midiChannelA;
		break;
	case LCHAN_B:
		chan = midiChannelB;
		break;
	}
	NOTE *pnote = NULL;
	for(int i=0; i<MAX_NOTES; ++i) {
		if(playingNotes[i].note == note && playingNotes[i].chan == chan) {
			pnote = &playingNotes[i];
			break;
		}
	}
	if(pnote) {
		pnote->note = 0;
		pnote->stopTime = 0;
	}
	hal_send_midi(DINMIDI, 0x90 + chan, note, 0);
}

void stopNotes() {
	for(int i=0; i<MAX_NOTES; ++i) {
		if(playingNotes[i].note) {
			hal_send_midi(DINMIDI, 0x90 + playingNotes[i].chan, playingNotes[i].note, 0);
			playingNotes[i].note = 0;
			playingNotes[i].stopTime = 0;
		}
	}
}
static void manageNotes() {
	for(int i=0; i<MAX_NOTES; ++i) {
		if(playingNotes[i].stopTime) {
			if(milliseconds >= playingNotes[i].stopTime) {
				hal_send_midi(DINMIDI, 0x90 + playingNotes[i].chan, playingNotes[i].note, 0);
				playingNotes[i].note = 0;
				playingNotes[i].stopTime = 0;
			}
		}
	}
}


/*
 * address a grid LED by row and column position
 */
void gridLed(int row, int col, COLOUR colour) {
	if(row >= 0 && row < 8 && col >= 0 && col < 8) {
		int index = col + 81 - 10*row;
		hal_plot_led(TYPEPAD, index, (colour>>16)&0xFF, (colour>>8)&0xFF, colour&0xFF);
	}
}

/*
 * address a menu LED by identifier
 */
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
 * Handler for when a button is pressed on the Launchpad
 */
void app_surface_event(u8 type, u8 index, u8 value)
{
	switch (type)
	{
		case  TYPEPAD:
			if(index < 10) {
				// bottom row of menu buttons
				activeSequencer->menuButton(activeSequencer, MNU_RECORDARM + (index-1), value);
			}
			else if(index < 89) {
				switch(index%10) {
				case 0:
					// left side column of menu buttons
					activeSequencer->menuButton(activeSequencer, MNU_SHIFT + (8-index/10), value);
					break;
				case 9:
					// right side column of menu buttons
					activeSequencer->menuButton(activeSequencer, MNU_ARROW0 + (8-index/10), value);
					break;
				default:
					// grid
					activeSequencer->gridButton(activeSequencer, 8-index/10, index%10-1, value);
					break;
				}
			}
			else if(index < 99) {
				// top row of menu button
				activeSequencer->menuButton(activeSequencer, MNU_UP+ (index-91), value);
			}
			break;

		case  TYPESETUP:
			// setup button
			activeSequencer->menuButton(activeSequencer, MNU_SETUP, value);
			break;

	}
}

/*
 * Handler for incoming MIDI event
 */
void app_midi_event(u8 port, u8 status, u8 d1, u8 d2)
{
	// external synch?
	if(synchMode == SYNCH_MIDI) {
		switch(status) {
		case MIDITIMINGCLOCK:
			if(!((++midiTicks)%6)) {
				activeSequencer->event(activeSequencer, EVENT_STEP);
			}
			break;
		case MIDISTART:
			midiTicks = 0;
			isRunning = TRUE;
			activeSequencer->event(activeSequencer, EVENT_RESET);
			activeSequencer->event(activeSequencer, EVENT_START);
			break;
		case MIDICONTINUE:
			activeSequencer->event(activeSequencer, EVENT_START);
			break;
		case MIDISTOP:
			activeSequencer->event(activeSequencer, EVENT_STOP);
			break;
		}
	}
}

/*
 * Handler for incoming MIDI SYSEX event
 */
void app_sysex_event(u8 port, u8 * data, u16 count)
{
}

/*
 * Handler for incoming MIDI AFTERTOUCH event
  */
void app_aftertouch_event(u8 index, u8 value)
{
}

/*
 * Handler for MIDI cable being plugged/unplugged
 */
void app_cable_event(u8 type, u8 value)
{
}

/*
 * Handler for millisecond timer events
 */
void app_timer_event()
{
	activeSequencer->event(activeSequencer, EVENT_TICK);

	// increment count of milliseconds
	++milliseconds;
	if(!milliseconds) {
		// rollover... just in case
		nextStepTime = 0;
		stopNotes();
	}
	if(synchMode == SYNCH_INTERNAL) {
		if(milliseconds >= nextStepTime) {
			nextStepTime += stepRate;
			activeSequencer->event(activeSequencer, EVENT_STEP);
		}
	}
	manageNotes();
}

/*
 * Handler for initialisation
 */
void app_init()
{
	activeSequencer = metastepCreate();
	milliseconds = 0;
	stepRate = 300.0;
	nextStepTime = stepRate;
	midiChannelA = 0;
	midiChannelB = 0;
	isRunning = TRUE;
	midiTicks = 0;
	synchMode = SYNCH_INTERNAL;
	activeSequencer->init(activeSequencer);
}

