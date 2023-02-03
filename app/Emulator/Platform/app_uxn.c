#include <time.h>
#include <stdio.h>
#include "emu_platform.h"
#include "app_uxn.h"

#import "uxn.h"
#include "devices/screen.h"
//#include "devices/system.c"
//#include "devices/screen.c"
//#include "devices/audio.c"
//#include "devices/file.h"
//#include "devices/controller.c"
//#include "devices/mouse.c"
//#include "devices/datetime.c"

static Uxn _uxn;
/* no need, already present in uxn/src/devices/screen.c
UxnScreen uxn_screen;
 */
static Uint8 reqdraw = 0;

// from src/uxnemu.c
//static int
void
error(char *msg, const char *err)
{
    fprintf(stderr, "%s: %s\n", msg, err);
    fflush(stderr);
//    return 0;
}

static int
console_input(Uxn *u, char c)
{
    Uint8 *d = &u->dev[0x10];
    d[0x02] = c;
    return uxn_eval(u, GETVEC(d));
}

static void
console_deo(Uint8 *d, Uint8 port)
{
    FILE *fd = port == 0x8 ? stdout : port == 0x9 ? stderr
                                                  : 0;
    if(fd) {
        fputc(d[port], fd);
        fflush(fd);
    }
}

// from src/uxnemu.c
static void
redraw(void) {
    // TODO if UI width/height is different than uxn_screen width/height, then resize UI element
    screen_redraw(&uxn_screen, uxn_screen.pixels);

    // UxnScreen, Layer.pixels is Uint8 aka 4-bit uxn colors, not 32bit RGBA as we need it to be, translate from palette.
    PlatformBitmap bmp = {
        .width = uxn_screen.width,
        .height = uxn_screen.height,
        .pixels = uxn_screen.pixels,
    };
    PlatformDrawBitmap(&bmp);
    reqdraw = 0; // TODO need this?
}

/*
static void
file_talk(Device *d, Uint8 b0, Uint8 w) {
    Uint8 read = b0 == 0xd;
    if(w && (read || b0 == 0xf)) {
        char *name = (char *)&d->mem[mempeek16(d->dat, 0x8)];
        Uint16 result = 0, length = mempeek16(d->dat, 0xa);
        Uint16 offset = mempeek16(d->dat, 0x4);
        Uint16 addr = mempeek16(d->dat, b0 - 1);
        PlatformFile f = PlatformOpenFile(name, read ? "r" : (offset ? "a" : "w"));
        if(f) {
            fprintf(stderr, "%s %s %s #%04x, ", read ? "Loading" : "Saving", name, read ? "to" : "from", addr);
            if(PlatformSeekFile(f, offset, SEEK_SET) != -1)
                result = read ? PlatformReadFile(f, &d->mem[addr], length) : PlatformWriteFile(f, &d->mem[addr], length);
            fprintf(stderr, "%04x bytes\n", result);
            PlatformCloseFile(f);
        }
        mempoke16(d->dat, 0x2, result);
    }
}


static void
audio_talk(Device *d, Uint8 b0, Uint8 w) {
    Apu *c = &_apu[d - devaudio0];
    if(!w) {
        if(b0 == 0x2)
            mempoke16(d->dat, 0x2, c->i);
        else if(b0 == 0x4)
            d->dat[0x4] = apu_get_vu(c);
    } else if(b0 == 0xf) {
//        SDL_LockAudioDevice(audio_id);
        c->len = mempeek16(d->dat, 0xa);
        c->addr = &d->mem[mempeek16(d->dat, 0xc)];
        c->volume[0] = d->dat[0xe] >> 4;
        c->volume[1] = d->dat[0xe] & 0xf;
        c->repeat = !(d->dat[0xf] & 0x80);
        apu_start(c, mempeek16(d->dat, 0x8), d->dat[0xf] & 0x7f);
//        SDL_UnlockAudioDevice(audio_id);
        PlatformAudioOpenOutput();
    }
}
 */


// from uxn/src/uxnemu.c
static Uint8
emu_dei(Uxn *u, Uint8 addr)
{
    
    Uint8 p = addr & 0x0f, d = addr & 0xf0;
    switch(d) {
    case 0x20: return screen_dei(&u->dev[d], p);
    //case 0x30: return audio_dei(0, &u->dev[d], p);
    //case 0x40: return audio_dei(1, &u->dev[d], p);
    //case 0x50: return audio_dei(2, &u->dev[d], p);
    //case 0x60: return audio_dei(3, &u->dev[d], p);
    //case 0xa0: return file_dei(0, &u->dev[d], p);
    //case 0xb0: return file_dei(1, &u->dev[d], p);
    //case 0xc0: return datetime_dei(&u->dev[d], p);
    }
    return u->dev[addr];
    // wacky, todo, why two returns? Twas this way in upstream unx right?
    return 0;
}

static void
emu_deo(Uxn *u, Uint8 addr, Uint8 v)
{
    Uint8 p = addr & 0x0f, d = addr & 0xf0;
    u->dev[addr] = v;
    switch(d) {
    case 0x00:
        //todo: system_deo(u, &u->dev[d], p);
        if(p > 0x7 && p < 0xe)
            screen_palette(&uxn_screen, &u->dev[0x8]);
        break;
    case 0x10: console_deo(&u->dev[d], p); break;
    case 0x20: screen_deo(u->ram, &u->dev[d], p); break;
    //case 0x30: audio_deo(0, &u->dev[d], p, u); break;
    //case 0x40: audio_deo(1, &u->dev[d], p, u); break;
    //case 0x50: audio_deo(2, &u->dev[d], p, u); break;
    //case 0x60: audio_deo(3, &u->dev[d], p, u); break;
    //case 0xa0: file_deo(0, u->ram, &u->dev[d], p); break;
    //case 0xb0: file_deo(1, u->ram, &u->dev[d], p); break;
    }
}


void
uxnapp_init(void) {
    Uxn* u = &_uxn;
    // from uxn/src/uxnemu.c start()
    PlatformFree(u->ram);
    if(!uxn_boot(u, (Uint8 *)PlatformAlloc(0x10300), emu_dei, emu_deo))
        return error("Boot", "Failed to start uxn.");
    u16 w, h;
    PlatformGetScreenSize(&w, &h);
    screen_resize(&uxn_screen, w, h);
    PlatformCopyRom(u->ram + PAGE_PROGRAM, (u32)(sizeof(u->ram) - PAGE_PROGRAM)); // conversion problem here? sizeof() returns what? #define PAGE_PROGRAM is ?
//    exec_deadline = SDL_GetPerformanceCounter() + deadline_interval;
    if(!uxn_eval(u, PAGE_PROGRAM))
        return error("Boot", "Failed to eval rom.");

    redraw();
}


void
uxnapp_deinit(void) {
    PlatformAudioCloseOutput();
    /*
    PlatformFree(uxn_screen.bg.pixels);
    PlatformFree(_ppu.fg.pixels);
     */
}



void
uxnapp_runloop(void) {
    Uxn* u = &_uxn;
    uxn_eval(u, GETVEC(&u->dev[0x20]));
    if(uxn_screen.fg.changed || uxn_screen.bg.changed)
        redraw();
}


void
uxnapp_setdebug(u8 debug) {
    /*
    Uxn* u = &_uxn;
    devsystem->dat[0xe] = debug ? 1 : 0;
    redraw(u);
     */
}

/* remove this, already present in uxn/src/devices/screen.c */
/*int
clamp(int val, int min, int max) {
    return (val >= min) ? (val <= max) ? val : max : min;
}
 */


void
uxnapp_movemouse(i16 mx, i16 my) {
    /*
    Uxn* u = &_uxn;
    Uint16 x = clamp(mx, 0, _ppu.hor * 8 - 1);
    Uint16 y = clamp(my, 0, _ppu.ver * 8 - 1);
    mempoke16(devmouse->dat, 0x2, x);
    mempoke16(devmouse->dat, 0x4, y);

    evaluxn(u, mempeek16(devmouse->dat, 0));
*/
}


void
uxnapp_setmousebutton(u8 button, u8 state) {
    /*
    Uxn* u = &_uxn;

    Uint8 flag = 0x00;
    switch (button) {
        case 0: flag = 0x01; break;
        case 1: flag = 0x10; break;
    }

    if (state) {
        devmouse->dat[6] |= flag;
    }
    else {
        devmouse->dat[6] &= (~flag);
    }

    evaluxn(u, mempeek16(devmouse->dat, 0));
 */
}


void
uxnapp_audio_callback(Uint8 *stream, Uint32 len) {
    /*
    int running = 0;
    Sint16 *samples = (Sint16 *)stream;
    PlatformMemset(stream, 0, len);
    for(int i = 0; i < POLYPHONY; ++i) {
        running += apu_render(&_apu[i], samples, samples + len / 2);
    }
    if (!running) {
        PlatformAudioPauseOutput();
    }
     */
}
