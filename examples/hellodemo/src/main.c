//#define SYNC_PLAYER

// hellodemo.pro, based on beam riders

#include "../shared/rocket/lib/sync.h"

#ifndef AMIGA
#include "../shared/libs/bass24/c/bass.h"
#endif

#include "wos/wos.h"
#include "profile/profile.h"
#include "tools/cachesim.h"
#include "sound/adpcm.h"

#include "tools/mem32.h"
#include "tools/malloc.h"

#include "stars/starseffect.h"

#include <stdio.h>
#include <math.h>

#ifdef AMIGA
#include <intuition/intuition.h>
struct IntuitionBase *IntuitionBase

struct EasyStruct volumeES = {
	    sizeof (struct EasyStruct),
	    0,
	    "Beam Riders Launch Control",
	    "Demo loaded and waiting to start.",
	    "Start|Exit",
	};
#endif

unsigned int* g_currentPal;

int xres,yres;
// cli parameters
int ocs= 0;
int profile = 0;
int wait = 0;

unsigned char* screenBuffer= 0;
unsigned char* saveScreenBuffer; 

unsigned char* AdpcmFileLeft= 0;
unsigned char* AdpcmFileRight= 0;

int timerC2P;
int timerEffect;

int part= 0;
int prevPart=-1;
int timeoffset=0; //128 for diver death

// amiga build tests
//extern static unsigned int titlepal[256];


struct sync_device *rocket;

const struct sync_track *rt_part;
const struct sync_track *rt_brightness;

// stars
const struct sync_track *rt_star_time;
const struct sync_track *rt_star_pers;

static const double rocket_bpm = 69.9f; /* beats per minute amiga and pc */
static const int rocket_rpb = 8; /* rows per beat */
static double rocket_row_rate;  //c problem? = (bpm / 60) * rpb;

#ifndef AMIGA
HSTREAM stream;

static double bass_get_row(HSTREAM h)
{
	QWORD pos = BASS_ChannelGetPosition(h, BASS_POS_BYTE);
	double time = BASS_ChannelBytes2Seconds(h, pos);
	return (time) * rocket_row_rate;
}

static void bass_pause(void *d, int flag)
{
	HSTREAM h = *((HSTREAM *)d);
	if (flag)
		BASS_ChannelPause(h);
	else
		BASS_ChannelPlay(h, FALSE);
}

static void bass_set_row(void *d, int row)
{
	HSTREAM h = *((HSTREAM *)d);
	QWORD pos = BASS_ChannelSeconds2Bytes(h, row / rocket_row_rate);
	BASS_ChannelSetPosition(h, pos, BASS_POS_BYTE);
}

static int bass_is_playing(void *d)
{
	HSTREAM h = *((HSTREAM *)d);
	return BASS_ChannelIsActive(h) == BASS_ACTIVE_PLAYING;
}

#ifndef SYNC_PLAYER
static struct sync_cb bass_cb = {
	bass_pause,
	bass_set_row,
	bass_is_playing
};
#endif

#endif /* !defined(AMIGA/SYNC_PLAYER) */

static void die(const char *fmt, ...)
{
	fprintf(stderr, "***ERROR: %s\n", fmt);
	//exit();
}

// returns 0 on success, otherwise 1
int initDemo()
{
//   int elapsed;
   int rate;
   int size;

   //printf("init...\n");

   size= adpcmLoad("data/music_stereo.wav", &AdpcmFileLeft, &AdpcmFileRight, &rate);
   if (size < 0)
      return 1;
   rocket_row_rate = (rocket_bpm / 60.0f) * rocket_rpb;  

   xres= 320;
   yres= 180;
   //screenBuffer= (unsigned char*)malloc(xres*yres*sizeof(int));
   saveScreenBuffer= (unsigned char*)malloc(xres*yres*3+16);
   memset32(saveScreenBuffer, 0x0, xres*yres*3+16);
   screenBuffer= saveScreenBuffer+xres*yres;
   // align screen buffer to 16 byte
   screenBuffer= (unsigned char*)( ((unsigned int)screenBuffer+15) & ~15 );

   // effect init
   starsEffectInit();
	return 0;
}


void reloadDemo()
{
   starsEffectInit();
}

void deinitDemo()
{    
#ifdef AMIGA
   removeCVBI();
#endif
   starsEffectRelease();

   free(AdpcmFileLeft);
   free(AdpcmFileRight);
   free(saveScreenBuffer);
}

void updateDemo(int time)
{
   int ri_brightness, ri_part;
      
   // either get time from stream for rocket editor
#ifndef AMIGA
   // PC
#ifndef   SYNC_PLAYER
   double row = bass_get_row(stream); 
   if (sync_update(rocket, (int)floor(row), &bass_cb, (void *)&stream))
      sync_tcp_connect(rocket, "localhost", SYNC_DEFAULT_PORT);
   BASS_Update(0); /* decrease the chance of missing vsync */
#else
   // or calculate it since start of demo
   double row = (timeoffset*50 + time)/50.0*rocket_row_rate;
#endif
   
#else
   // AMIGA
   // or calculate it since start of demo
   double row = (timeoffset*50+time)/50.0*rocket_row_rate;
#endif
   
   ri_brightness = (int) sync_get_val(rt_brightness, row);
   ri_part = (int) sync_get_val(rt_part, row);   
   
   if ( (g_currentPal!=0))
   {
      wosSetCols(g_currentPal, ri_brightness);
   }
 
}

void dummyOn(int dummy)
{
    // dummy for effects that need no second init before render
    
}


void drawDemo(int time)
{
   // general
   int curPart;
   int ri_brightness;
  
   // stars
   int ri_star_time;
   float rf_star_pers;

   unsigned int playpos;
   double row;

   // either get time from stream for rocket editor
#ifndef AMIGA
   // PC
#ifndef   SYNC_PLAYER
   row = bass_get_row(stream); 
   if (sync_update(rocket, (int)floor(row), &bass_cb, (void *)&stream))
      sync_tcp_connect(rocket, "localhost", SYNC_DEFAULT_PORT);
   BASS_Update(0); /* decrease the chance of missing vsync */
#else
   // or calculate it since start of demo
   row = (timeoffset*50 + time)/50.0*rocket_row_rate;
#endif
   
#else
   // AMIGA
   // or calculate it since start of demo
   // row = time/50.0*rocket_row_rate;   // as in Prototype1, LTTD, Beam Riders compo version
   
   playpos = wosGetPlayPos();
   // removed the 70.0 special case rocket_bpm for AMIGA and based the row calculation on playpos
   row = (double) playpos*rocket_row_rate/28603.99f; // PAL maximum playrate is 28636.54f
   // lower the dividing number to make the visuals come earlier
   
#endif
   
   // stars
   ri_star_time = (int) sync_get_val(rt_star_time, row);
   rf_star_pers = (float) sync_get_val(rt_star_pers, row);

   // general
   ri_brightness = (int) sync_get_val(rt_brightness, row);
   curPart = (int) sync_get_val(rt_part, row);
   
   // init part
   if (prevPart != curPart)
   {
      switch (curPart)
      {
      case 1:
          starsEffectOn(time); break;
         
      default: break;
      };   
   }


   // draw part
   switch (curPart)
   {
   case 1: 
      starsEffectRender(ri_star_time, rf_star_pers);
      break;
      
   default: break;
   };   

   prevPart = curPart;
   part= curPart;

   
   wosDisplay(2);
   
   if (g_currentPal!=0)
   {
      wosSetCols(g_currentPal, ri_brightness);
   }

/*
   {
      int i=0;
      for (i=0; i<xres*yres; i++)
         screenBuffer[i] >>= 1;
      tgaSave8("test.tga", screenBuffer, xres, yres, g_currentPal);
   }
*/
}


// der win32 springt hier einmal rein, macht dann die loop aber selbst (die callt dann einfach drawDemo)
void mainDemo()
{
   int bytes;
   
   /* init BASS */

   /* let's roll! */
#ifndef AMIGA
   if (!BASS_Init(-1, 44100, 0, 0, 0))
       die("failed to init bass");
  stream = BASS_StreamCreateFile(FALSE, "data/music_stereo_44100.mp3", 0, 0, BASS_STREAM_PRESCAN);
   if (!stream)
       die("failed to open tune");
  bytes = (int)BASS_ChannelSeconds2Bytes(stream, timeoffset);
  BASS_ChannelSetPosition(stream,bytes,BASS_POS_BYTE);
   BASS_Start();
   BASS_ChannelPlay(stream, FALSE);
#endif
   
   //  8: 320x180
   //  9: 320x90
   // 10: 160x90
   // 11: 160x90x6 (18 bit)
   // 12: 640x180
   // 13: 640x360
   // 14: 320x180 (6 bit, on the fly saturation)
   // 15: 320x180 (upper 5 bit, with bitplane lines)
   // 16: 320x180 (with copper colours)
   
   
   // 17: 220x180x8 (15 bit, no saturation, 32 bit layout)
   // 18: 220x180x6 (15 bit, on the fly saturation)
   // 19: 220x90x6 (15 bit, on the fly saturation)
   // 21: 220x180
   // 22: 220x90
   // 23: 220x180x12
   // 24: 320x180x5 (OCS)
    
#if !defined (WIN32) && !defined(linux)
   g_vbitimer+=0 ;//66*50;
   while (wosCheckExit()==0)
   {
      //if (g_renderedFrames==200) break;
      drawDemo(g_vbitimer);
      // soundtrack duration:    4:38, 278s * 50 = 13900 (to avoid click at end)
      if (g_vbitimer>=13800-1000+(7*50)+25 ) wosSetExit();  // Beam Riders final
      //if (g_vbitimer>=500 ) wosSetExit();  // debug
   }
#endif
}

void getSyncTracks()
{
   // rocket get tracks
   
   // general
   rt_part = sync_get_track(rocket, "part");
   rt_brightness = sync_get_track(rocket, "brightness");
  
   // stars
   rt_star_time = sync_get_track(rocket, "stars(1):time");
   rt_star_pers = sync_get_track(rocket, "stars(1):perspective");
}   


int main(int argc, char* argv[])
{
   unsigned int playpos1, playpos2;
   int i;
  
  
#ifdef AMIGA
   IntuitionBase = (struct IntuitionBase *) OpenLibrary( "intuition.library", 39 );
   if( IntuitionBase == NULL ) 
   {
      printf("Kickstart 3.0 or newer required!\n");
      return 0;
   }
   
   if (wosCheckCPUFPU()!=0)
   {
      printf("68020+ and FPU required!\n");
      CloseLibrary( IntuitionBase );
      return 0;
   }
   if (wosCheckAGA()!=1)
   {
      printf("No AGA found. Running in OCS mode!\n");
      ocs = 1;
   }
   playpos1 = wosGetPlayPos();
#endif
   
    /* rocket init */
//   printf("rocket...\n");
   rocket = sync_create_device("sync");
   if (!rocket)
      return 0;

#ifndef AMIGA
   // PC
#ifndef   SYNC_PLAYER
   if (!sync_tcp_connect(rocket, "localhost", SYNC_DEFAULT_PORT))
   {
      getSyncTracks();
   } else {
      printf("Could not connect to rocket editor!\n");
      return 0;
   }
#else
   getSyncTracks();
#endif
   
#else
   // AMIGA
   getSyncTracks();
#endif
   
  
   
	if (!initDemo())
   {
      wosInit();
   }
   else
   {
      printf("***ERROR: initDemo() aborted\n");
#ifdef AMIGA
      CloseLibrary( IntuitionBase );
#endif
      return 0;
   }

   // calls mainDemo

   deinitDemo();

   wosClearExit();
  
#ifndef AMIGA
   BASS_StreamFree(stream);
	BASS_Free();
#endif
#ifndef SYNC_PLAYER
	sync_save_tracks(rocket);
#endif
	sync_destroy_device(rocket);
   
#ifdef AMIGA
   CloseLibrary( IntuitionBase );
#endif

   profileDeinit();

//   malloc_debug();
   
   return 0;
}
