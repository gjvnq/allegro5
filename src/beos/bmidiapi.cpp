/*         ______   ___    ___ 
 *        /\  _  \ /\_ \  /\_ \ 
 *        \ \ \L\ \\//\ \ \//\ \      __     __   _ __   ___ 
 *         \ \  __ \ \ \ \  \ \ \   /'__`\ /'_ `\/\`'__\/ __`\
 *          \ \ \/\ \ \_\ \_ \_\ \_/\  __//\ \L\ \ \ \//\ \L\ \
 *           \ \_\ \_\/\____\/\____\ \____\ \____ \ \_\\ \____/
 *            \/_/\/_/\/____/\/____/\/____/\/___L\ \/_/ \/___/
 *                                           /\____/
 *                                           \_/__/
 *
 *      MIDI driver routines for BeOS.
 *
 *      By Angelo Mottola.
 *
 *      See readme.txt for copyright information.
 */

#include "bealleg.h"
#include "allegro/aintern.h"
#include "allegro/aintbeos.h"

#ifndef ALLEGRO_BEOS
#error something is wrong with the makefile
#endif                


BMidiSynth *be_midisynth = NULL;
static char be_midi_driver_desc[256] = EMPTY_STRING;
static int cur_patch[17];
static int cur_note[17];
static int cur_vol[17];



/* be_midi_detect:
 *  BeOS MIDI detection.
 */
extern "C" int be_midi_detect(int input)
{
   if (input) {
      ustrncpy(allegro_error, get_config_text("Input is not supported"), ALLEGRO_ERROR_SIZE - ucwidth(0));
      return FALSE;
   }
   
   return TRUE;
}



/* be_midi_init:
 *  Initializes the BeOS MIDI driver.
 */
extern "C" int be_midi_init(int input, int voices)
{
   char tmp[128], tmp2[128] = EMPTY_STRING;
   char *sound = uconvert_ascii("sound", tmp);
   int mode, freq, quality, reverb;
   synth_mode sm = B_BIG_SYNTH;
   interpolation_mode im = B_2_POINT_INTERPOLATION;
   char *reverb_name[] =
      { "no", "closet", "garage", "ballroom", "cavern", "dungeon" };
   
   if (input) {
      ustrncpy(allegro_error, get_config_text("Input is not supported"), ALLEGRO_ERROR_SIZE - ucwidth(0));
      return -1;
   }
         
   be_midisynth = new BMidiSynth();
   if (!be_midisynth) {
      ustrncpy(allegro_error, get_config_text("Not enough memory"), ALLEGRO_ERROR_SIZE - ucwidth(0));
      return -1;
   }
   
   /* Checks if instruments are available */
   mode = MID(0, get_config_int(sound, uconvert_ascii("be_midi_quality", tmp), 1), 1);
   if (mode)
      sm = B_BIG_SYNTH;
   else
      sm = B_LITTLE_SYNTH;
   if ((be_synth->LoadSynthData(sm) != B_OK) ||
       (!be_synth->IsLoaded())) {
      delete be_midisynth;
      be_midisynth = NULL;
      ustrncpy(allegro_error, get_config_text("Can not load MIDI instruments data file"), ALLEGRO_ERROR_SIZE - ucwidth(0));
      return -1;
   }
   
   /* Sets up synthetizer and loads instruments */
   be_midisynth->EnableInput(true, true);
   
   /* Prevents other apps from changing instruments on the fly */
   be_midisynth->FlushInstrumentCache(true);
   
   /* Reverberation is cool */
   reverb = MID(0, get_config_int(sound, uconvert_ascii("be_midi_reverb", tmp), 0), 5);
   if (reverb) {
      be_synth->SetReverb((reverb_mode)reverb);
      be_synth->EnableReverb(true);
   }
   else
      be_synth->EnableReverb(false);
         
   /* Sets sampling rate and sample interpolation method */
   freq = get_config_int(sound, uconvert_ascii("be_midi_freq", tmp), 22050);
   quality = MID(0, get_config_int(sound, uconvert_ascii("be_midi_interpolation", tmp), 1), 2);
   be_synth->SetSamplingRate(freq);
   switch (quality) {
      case 0:
         im = B_DROP_SAMPLE;
         break;
      case 1:
         im = B_2_POINT_INTERPOLATION;
         ustrncpy(tmp2, uconvert_ascii("fast", tmp), sizeof(tmp2) - ucwidth(0));
         break;
      case 2:
         im = B_LINEAR_INTERPOLATION;
         ustrncpy(tmp2, uconvert_ascii("linear", tmp), sizeof(tmp2) - ucwidth(0));
         break;
   }
   be_synth->SetInterpolation(im);
   
   /* Sets up driver description */
   usnprintf(be_midi_driver_desc, sizeof(be_midi_driver_desc),
             uconvert_ascii("BeOS %s quality synth, %s %d kHz, %s reverberation", tmp),
             (mode ? "high" : "low"), tmp2, (be_synth->SamplingRate() / 1000), reverb_name[reverb]);
   midi_beos.desc = be_midi_driver_desc;

   return 0;
}



/* be_midi_exit:
 *  Shuts down MIDI subsystem.
 */
extern "C" void be_midi_exit(int input)
{
   if (be_midisynth) {
      be_midisynth->AllNotesOff(false);
      delete be_midisynth;
      be_midisynth = NULL;
   }
}



/* be_midi_mixer_volume:
 *  Sets MIDI mixer output volume.
 */
extern "C" int be_midi_mixer_volume(int volume)
{
   be_midisynth->SetVolume((double)volume / 255.0);
   return 0;
}



/* be_midi_key_on:
 *  Triggers a specified voice.
 */
extern "C" void be_midi_key_on(int inst, int note, int bend, int vol, int pan)
{
   int voice;
   
   if (inst > 127) {
      /* percussion */
      
      /* hack to use channel 10 only */
      midi_beos.xmin = midi_beos.xmax = -1;
      voice = _midi_allocate_voice(10, 10);
      midi_beos.xmin = midi_beos.xmax = 10;
      
      if (voice < 0)
         return;
      cur_note[10] = inst - 128;
      cur_vol[10] = vol;
      be_midi_set_pan(voice, pan);
      be_midisynth->NoteOn(10, inst - 128, vol, B_NOW);
   }
   else {
      /* normal instrument */
      voice = _midi_allocate_voice(1, 16);
      if (voice < 0)
         return;
      if (inst != cur_patch[voice]) {
         be_midisynth->ProgramChange(voice, inst);
         cur_patch[voice] = inst;
      }
 
      cur_note[voice] = note;
      cur_vol[voice] = vol;
      be_midi_set_pitch(voice, note, bend);
      be_midi_set_pan(voice, pan);
      be_midisynth->NoteOn(voice, note, vol, B_NOW);
   }
}



/* be_midi_key_off:
 *  Turns off specified voice.
 */
extern "C" void be_midi_key_off(int voice)
{
   be_midisynth->NoteOff(voice, cur_note[voice], cur_vol[voice], B_NOW);
}



/* be_midi_set_volume:
 *  Sets volume for a specified voice.
 */
extern "C" void be_midi_set_volume(int voice, int vol)
{
   /* This seems to work */
   be_midisynth->ChannelPressure(voice, vol, B_NOW);
}



/* be_midi_set_pitch:
 *  Sets pitch of specified voice.
 */
extern "C" void be_midi_set_pitch(int voice, int note, int bend)
{
   /* ?? Is this correct? */
   be_midisynth->PitchBend(voice, bend & 0x7F, bend >> 7, B_NOW);
}



/* be_midi_set_pan:
 *  Sets pan value on specified voice.
 */
extern "C" void be_midi_set_pan(int voice, int pan)
{
   be_midisynth->ControlChange(voice, B_PAN, pan);
}
