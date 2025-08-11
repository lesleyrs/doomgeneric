#include "doomkeys.h"

#include "doomgeneric.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <js/glue.h>
#include <js/dom_pk_codes.h>

#include "str.h"

// TODO sound, uppercase wad names
void __unordtf2(void) {}

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

char *strdup(const char *s)
{
	size_t l = strlen(s);
	char *d = malloc(l+1);
	if (!d) return NULL;
	return memcpy(d, s, l+1);
}

int strncasecmp(const char *_l, const char *_r, size_t n)
{
	const unsigned char *l=(void *)_l, *r=(void *)_r;
	if (!n--) return 0;
	for (; *l && *r && n && (*l == *r || tolower(*l) == tolower(*r)); l++, r++, n--);
	return tolower(*l) - tolower(*r);
}

int strcasecmp(const char *_l, const char *_r)
{
	const unsigned char *l=(void *)_l, *r=(void *)_r;
	for (; *l && *r && (*l == *r || tolower(*l) == tolower(*r)); l++, r++);
	return tolower(*l) - tolower(*r);
}

static unsigned char convertToDoomKey(int key, int code) {
  switch (code)
    {
    case DOM_PK_ENTER:
      key = KEY_ENTER;
      break;
    case DOM_PK_ESCAPE:
      key = KEY_ESCAPE;
      break;
    case DOM_PK_ARROW_LEFT:
      key = KEY_LEFTARROW;
      break;
    case DOM_PK_ARROW_RIGHT:
      key = KEY_RIGHTARROW;
      break;
    case DOM_PK_ARROW_UP:
      key = KEY_UPARROW;
      break;
    case DOM_PK_ARROW_DOWN:
      key = KEY_DOWNARROW;
      break;
    case DOM_PK_CONTROL_LEFT:
    case DOM_PK_CONTROL_RIGHT:
      key = KEY_FIRE;
      break;
    case DOM_PK_SPACE:
      key = KEY_USE;
      break;
    case DOM_PK_SHIFT_LEFT:
    case DOM_PK_SHIFT_RIGHT:
      key = KEY_RSHIFT;
      break;
    case DOM_PK_ALT_LEFT:
    case DOM_PK_ALT_RIGHT:
      key = KEY_LALT;
      break;
    case DOM_PK_F2:
      key = KEY_F2;
      break;
    case DOM_PK_F3:
      key = KEY_F3;
      break;
    case DOM_PK_F4:
      key = KEY_F4;
      break;
    case DOM_PK_F5:
      key = KEY_F5;
      break;
    case DOM_PK_F6:
      key = KEY_F6;
      break;
    case DOM_PK_F7:
      key = KEY_F7;
      break;
    case DOM_PK_F8:
      key = KEY_F8;
      break;
    case DOM_PK_F9:
      key = KEY_F9;
      break;
    case DOM_PK_F10:
      key = KEY_F10;
      break;
    case DOM_PK_F11:
      key = KEY_F11;
      break;
    case DOM_PK_EQUAL:
    // case DOM_PK_PLUS:
      key = KEY_EQUALS;
      break;
    case DOM_PK_MINUS:
      key = KEY_MINUS;
      break;
    default:
      key = tolower(key);
      break;
    }

  return key;
}

static void addKeyToQueue(int pressed, int key, int code)
{
  unsigned char dKey = convertToDoomKey(key, code);

  unsigned short keyData = (pressed << 8) | dKey;

  s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
  s_KeyQueueWriteIndex++;
  s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

uint32_t canvas[DOOMGENERIC_RESX * DOOMGENERIC_RESY];

bool onkey(void* userData, bool pressed, int key, int code, int modifiers) {
    (void)userData,(void)modifiers;
    addKeyToQueue(pressed, key, code);
    if (code == DOM_PK_F12) {
        return 0;
    }
    return 1;
}

void DG_Init()
{
    JS_createCanvas(640, 400, "2d");
    JS_addKeyEventListener(NULL, onkey);
}

void DG_DrawFrame()
{
    for (size_t i = 0; i < DOOMGENERIC_RESX * DOOMGENERIC_RESY; i++) {
        uint32_t pixel = DG_ScreenBuffer[i];
	    canvas[i] = ((pixel & 0xff0000) >> 16) | (pixel & 0x00ff00) | ((pixel & 0x0000ff) << 16) | 0xff000000;
    }
    JS_setPixelsAlpha(canvas);
}

void DG_SleepMs(uint32_t ms)
{
    JS_setTimeout(ms);
}

uint32_t DG_GetTicksMs()
{
    return JS_performanceNow();
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
  if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex)
  {
    //key queue is empty
    return 0;
  }
  else
  {
    unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
    s_KeyQueueReadIndex++;
    s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

    *pressed = keyData >> 8;
    *doomKey = keyData & 0xFF;

    return 1;
  }

  return 0;
}

void DG_SetWindowTitle(const char * title)
{
    JS_setTitle(title);
}

int main(int argc, char **argv)
{
    doomgeneric_Create(argc, argv);

    while(1)
    {
      doomgeneric_Tick(); 
    }

    return 0;
}

