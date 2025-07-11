#include "doomkeys.h"

#include "doomgeneric.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <js/glue.h>
#include <js/key_codes.h>

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

static unsigned char convertToDoomKey(unsigned int key)
{
  switch (key)
    {
    case DOM_VK_RETURN:
      key = KEY_ENTER;
      break;
    case DOM_VK_ESCAPE:
      key = KEY_ESCAPE;
      break;
    case DOM_VK_LEFT:
      key = KEY_LEFTARROW;
      break;
    case DOM_VK_RIGHT:
      key = KEY_RIGHTARROW;
      break;
    case DOM_VK_UP:
      key = KEY_UPARROW;
      break;
    case DOM_VK_DOWN:
      key = KEY_DOWNARROW;
      break;
    case DOM_VK_CONTROL:
      key = KEY_FIRE;
      break;
    case DOM_VK_SPACE:
      key = KEY_USE;
      break;
    case DOM_VK_SHIFT:
      key = KEY_RSHIFT;
      break;
    case DOM_VK_ALT:
      key = KEY_LALT;
      break;
    case DOM_VK_F2:
      key = KEY_F2;
      break;
    case DOM_VK_F3:
      key = KEY_F3;
      break;
    case DOM_VK_F4:
      key = KEY_F4;
      break;
    case DOM_VK_F5:
      key = KEY_F5;
      break;
    case DOM_VK_F6:
      key = KEY_F6;
      break;
    case DOM_VK_F7:
      key = KEY_F7;
      break;
    case DOM_VK_F8:
      key = KEY_F8;
      break;
    case DOM_VK_F9:
      key = KEY_F9;
      break;
    case DOM_VK_F10:
      key = KEY_F10;
      break;
    case DOM_VK_F11:
      key = KEY_F11;
      break;
    case DOM_VK_EQUALS:
    case DOM_VK_PLUS:
      key = KEY_EQUALS;
      break;
    case DOM_VK_HYPHEN_MINUS:
      key = KEY_MINUS;
      break;
    default:
      key = tolower(key);
      break;
    }

  return key;
}

static void addKeyToQueue(int pressed, unsigned int keyCode)
{
  unsigned char key = convertToDoomKey(keyCode);

  unsigned short keyData = (pressed << 8) | key;

  s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
  s_KeyQueueWriteIndex++;
  s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

uint32_t canvas[DOOMGENERIC_RESX * DOOMGENERIC_RESY];

bool onkeydown(void* userData, int keyCode, int modifiers) {
    (void)userData,(void)modifiers;
    addKeyToQueue(1, keyCode);
    if (keyCode == DOM_VK_F12) {
        return 0;
    }
    return 1;
}
bool onkeyup(void* userData, int keyCode, int modifiers) {
    (void)userData,(void)modifiers;
    addKeyToQueue(0, keyCode);
    if (keyCode == DOM_VK_F12) {
        return 0;
    }
    return 1;
}

void DG_Init()
{
    JS_createCanvas(640, 400);
    JS_addKeyDownEventListener(NULL, onkeydown);
    JS_addKeyUpEventListener(NULL, onkeyup);
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

