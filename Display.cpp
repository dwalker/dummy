#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

#include "UserCmdTables.h"
#include "CmdCommon.h"
#include "Display.h"
#include "splash_bmp.h"
#include "logo_01A_bmp.h"

// Declare our globals for the display
long blankTime = 0;     // Initial setting doesn't matter...
bool isBlanked = true;  // Make sure the screen is turned on
bool blankEnabled = false;  // By default we have no blanking

#define splash_bmp  logo_01A_bmp

struct animatedDisplayDef {
  unsigned long size;
  const char *p;
} animatedDisplay[] = {
    {sizeof(logo_01A_bmp), logo_01A_bmp},
    {sizeof(logo_02A_bmp), logo_02A_bmp},
    {sizeof(logo_03A_bmp), logo_03A_bmp},
    {sizeof(logo_04A_bmp), logo_04A_bmp},
    {0, 0}
};

int animationCount = 0;

typedef struct {
  int cmd;
  char *str;
  int len;
} CNTRL_STRINGS;

static CNTRL_STRINGS noritakeControlStrings[] = {
  {DISPLAY_INIT_STRING, "\x1b\x40", 2}, 
  {DISPLAY_SIZE_1, "\x1f\x28\x67\x1\x1", 5}, 
  {DISPLAY_SIZE_2, "\x1f\x28\x67\x1\x2", 5}, 
  {DISPLAY_SIZE_4, "\x1f\x28\x67\x1\x4", 5}, 
  {DISPLAY_BLANK_STRING, "\x1f\x58\0", 3},
  {DISPLAY_BRIGHTNESS_STRING,"\x1f\x58", 2},
  {DISPLAY_UNBLANK_STRING, "\x1f\x58\x4", 3}, 
  {DISPLAY_CLEAR_STRING, "\xc\xb", 2},
  {DISPLAY_RAM_BITMAP_DEFINITION_START_STRING, "\x1f\x28\x66\x01" , 4},
  {DISPLAY_RAM_BITMAP_START_STRING, "\x1f\x28\x66\x10\0", 5},
  {DISPLAY_BITMAP_REALTIME_STRING, "\x1f\x28\x66\x11", 4},
  {DISPLAY_BLINK_STRING, "\x1F\x28\x61\x11\x2\x10\x10\x1", 8},
  {-1, 0, 0}    // Must be the last entry!
};

static CNTRL_STRINGS ansiControlStrings[] = {
  {DISPLAY_CLEAR_STRING, "\x1b[2J", 4},
  {-1, 0, 0}    // Must be the last entry!
};
 
static CNTRL_STRINGS *displayControlStrings = NULL;

void sendDisplayCmd(DISP_CMDS cmd) 
{
  CNTRL_STRINGS *dp = displayControlStrings;
  while (dp && (dp->cmd >= 0)) {
    if (dp->cmd == cmd) {
      if (dp->len > 0)
        write(fdo, dp->str, dp->len);
      break;
    }
    dp++;
  }
}

void setDisplayType(int dispType)
{
  if (dispType)
    displayControlStrings = ansiControlStrings;
  else
    displayControlStrings = noritakeControlStrings;
}

void showDisplayRam(int x, int y)
{
  char ch;
  long startAddr = 0;

  sendDisplayCmd(DISPLAY_RAM_BITMAP_START_STRING);
  ch = startAddr & 0xFF;
  write(fdo, &ch, 1 );
  ch = (startAddr >> 8) & 0xFF;
  write(fdo, &ch, 1 );
  ch = (startAddr >> 16) & 0xFF;
  write(fdo, &ch, 1 );
  
  y /= 8;
  ch = y & 0xFF;
  write(fdo, &ch, 1);
  ch = (y >> 8) & 0xFF;
  write(fdo, &ch, 1);
  
  ch = x & 0xFF;
  write(fdo, &ch, 1 );
  ch = (x >> 8) & 0xFF;
  write(fdo, &ch, 1 );

  ch = y & 0xFF;
  write(fdo, &ch, 1 );
  ch = (y >> 8) & 0xFF;
  write(fdo, &ch, 1 );
  ch = 1;
  write(fdo, &ch, 1 );
}

void animateDisplay(bool restart)
{
  if (restart)
    animationCount = 0;
  showSplash(animatedDisplay[animationCount].p, animatedDisplay[animationCount].size, 127, 64);
  animationCount++;
  if (animatedDisplay[animationCount].p == 0)
    animationCount = 0;
}


void showSplash(const char *bmpStartAddr, long bmpLen, int x, int y)
{

}

void showSplash(int x, int y)
{
  char ch;
  long startAddr = 0;
#if 1 
  long bmpLen;
  
  sendDisplayCmd(DISPLAY_RAM_BITMAP_DEFINITION_START_STRING);
  // Write the starting address
  ch = startAddr & 0xFF;
  write(fdo, &ch, 1 );
  ch = (startAddr >> 8) & 0xFF;
  write(fdo, &ch, 1 );
  ch = (startAddr >> 16) & 0xFF;
  write(fdo, &ch, 1 );
  // Write the length
  bmpLen = sizeof(splash_bmp);
  ch = bmpLen & 0xFF;
  write(fdo, &ch, 1 );
  ch = (bmpLen >> 8) & 0xFF;
  write(fdo, &ch, 1 );
  ch = (bmpLen >> 16) & 0xFF;
  write(fdo, &ch, 1 );
  // Write out all of the information
  write(fdo, &splash_bmp, bmpLen);
  
  showDisplayRam(x, y);
#else
  sendDisplayCmd(DISPLAY_BITMAP_REALTIME_STRING);
  ch = x & 0xFF;
  write(fdo, &ch, 1 );
  ch = (x >> 8) & 0xFF;
  write(fdo, &ch, 1 );

  y /= 8;
  ch = y & 0xFF;
  write(fdo, &ch, 1 );
  ch = (y >> 8) & 0xFF;
  write(fdo, &ch, 1 );
  ch = 1;
  write(fdo, &ch, 1 );
  write(fdo, &splash_bmp, sizeof(splash_bmp));
#endif
  sleep(5);
}

void setDisplayBrightness(char brightness) 
{
  if (brightness < 0x10)
    brightness |= 0x10;
  if (brightness > 0x18)
    brightness = 0x18;
  sendDisplayCmd(DISPLAY_BRIGHTNESS_STRING);
  write(fdo, &brightness, 1);
}

bool displayBlank(int mode) 
{
  bool retValue = false;
  long currentUserSetting;

  currentUserSetting = getSettingValueLong(DISP_BLANK_CMD);

//  printf("In blank: %ld\r\n", currentUserSetting);
  // Read the current setting for the blanking timeout
  if (mode == 1)
    blankTime = currentUserSetting;
  else if (mode == 2) {
    // Set the brightness level.  If we are not blanked then
    //  we need to update the display immediately.  If we are
    //  blanked then we don't need to do anything - the display
    //  level will be updated when the screen is enabled
    if (!isBlanked) {
      isBlanked = true;
    }
  }
  else if (mode == 3) {
    // Set the blanking time-out period
    isBlanked = false;
    blankEnabled = true;      // Blanking is enabled - will be disabled if necessary later
    if (currentUserSetting) {
      blankTime = currentUserSetting;
    }
    else {
      // Blanking is disabled - need to make sure the display is turned on
      //  at the brightness level
      blankTime = 2;        // To blank...
      isBlanked = true;      // To force the display to the level
    }
  }

  // If blanking is not enabled then don't process the time-out
  if (blankEnabled) {
    if (blankTime <= 0) {
      blankTime = 0;
      if (!isBlanked) {
        sendDisplayCmd(DISPLAY_BLANK_STRING);
        isBlanked = true;
      }
    }
    else
      blankTime--;
  }

  if (blankTime && isBlanked) {
    setDisplayBrightness((char)getSettingValueLong(DISP_BRIGHT_CMD));
    isBlanked = false;
    retValue = true;          // We have turned the display back on
  }

  if (!currentUserSetting)
    blankEnabled = false;    // Blanking is disabled

  return retValue;
}

void blinkDisplay()
{
  sendDisplayCmd(DISPLAY_BLINK_STRING);
}
