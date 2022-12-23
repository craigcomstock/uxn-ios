#include <time.h>
#include "emu_platform.h"
#include "app_uxn.h"
#include "stdlib.h" // calloc
#include "stdio.h" // fprintf, stderr
#include "uxn.h"

#include "devices/system.c"
//#include "devices/screen.h"
//#include "devices/audio.c"
//#include "devices/screen.c"
#if DEBUG
#include "uxn.c"
#else
#include "uxn-fast.c"
#endif

static Uxn _uxn;
static Device *devsystem;
//*devscreen, *devmouse, *devaudio0;
static Uint8 reqdraw = 0;


static void
redraw(Uxn *u) {
    // system 0xe set means debug, for now skip it
    // in the case of the screen this means add data/lines/things
    //if(devsystem->dat[0xe]) {
    //    inspect(&uxn_screen, u->wst.dat, u->wst.ptr, u->rst.ptr, u->ram.dat);
    //}
    /*
    PlatformBitmap bg = {
        .width = uxn_screen.width,
        .height = uxn_screen.height,
        .pixels = uxn_screen.bg.pixels,
    };
    PlatformBitmap fg = {
        .width = uxn_screen.width,
        .height = uxn_screen.height,
        .pixels = uxn_screen.fg.pixels,
    };
    PlatformDrawBackground(&bg);
    PlatformDrawForeground(&fg);
    reqdraw = 0;
     */
}


static void
nil_talk(Device *d, Uint8 b0, Uint8 w) {
}

/* removed system_talk(), use system_dei and system_deo from uxn/src/devices, all of those are port independent
 */

static void
console_deo(Device *d, Uint8 b0, Uint8 w) {
    //fprintf(stderr,"console_deo() called, d=%p, b0=%d, w=%d\n, c=%c\n", d, b0, w, w);
    fprintf(stdout,"%c", w);
// TODO how to write to stdout(port==0x8) or stderr(port=0x9) in an ios app?
//    if(w && b0 > 0x7)
//        write(b0 - 0x7, (char *)&d->dat[b0], 1);
}

/*
static void
screen_talk(Device *d, Uint8 b0, Uint8 w) {
    if(w && b0 == 0xe) {
        Uint16 x,y,mem_addr;
        DEVPEEK16(x, 0x8);
        DEVPEEK16(y, 0xa);
        DEVPEEK16(mem_addr, 0xc);
        Uint8 *addr = &d->mem[mem_addr];
        Layer *layer = d->dat[0xe] >> 4 & 0x1 ? &uxn_screen.fg : &uxn_screen.bg;
        Uint8 mode = d->dat[0xe] >> 5;
        if(!mode)
            screen_write(&uxn_screen, layer, x, y, d->dat[0xe] & 0x3);
        else if(mode-- & 0x1)
            puticn(&uxn_screen, layer, x, y, addr, d->dat[0xe] & 0xf, mode & 0x2, mode & 0x4);
        else
            putchr(&uxn_screen, layer, x, y, addr, d->dat[0xe] & 0xf, mode & 0x2, mode & 0x4);
        reqdraw = 1;
    }
}


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


static void
datetime_talk(Device *d, Uint8 b0, Uint8 w) {
    time_t seconds = time(NULL);
    struct tm *t = localtime(&seconds);
    t->tm_year += 1900;
    mempoke16(d->dat, 0x0, t->tm_year);
    d->dat[0x2] = t->tm_mon;
    d->dat[0x3] = t->tm_mday;
    d->dat[0x4] = t->tm_hour;
    d->dat[0x5] = t->tm_min;
    d->dat[0x6] = t->tm_sec;
    d->dat[0x7] = t->tm_wday;
    mempoke16(d->dat, 0x08, t->tm_yday);
    d->dat[0xa] = t->tm_isdst;
}
*/
// TODO refactor system_deo_special() into uxn/src/devices/screen.c?
// copied from uxn/src/uxnemu.c (SDL emulator)
// screen_palette is in uxn/src/devices/screen.c so port independent
void system_deo_special(Device *d, Uint8 port)
{
    /*
    if(port > 0x7 && port < 0xe)
        screen_palette(&uxn_screen, &d->dat[0x8]);
*/
     }

// TODO refactor nil_dei() and nil_deo() from uxn/src/uxnmenu.c to uxn/src/devices/something.c?
// copied from uxn/src/uxnemu.c (SDL emulator)
static Uint8
nil_dei(Device *d, Uint8 port)
{
    return d->dat[port];
}
static void
nil_deo(Device *d, Uint8 port)
{
    (void)d;
    (void)port;
}

#define RAMSIZE 0x10000

void
uxnapp_init(void) {
    fprintf(stderr, "uxnapp_init()\n");
    Uxn* u = &_uxn;

    fprintf(stderr, "before uxn_boot()\n");
    Uint8 dat[0x10000];
    uxn_boot(u, dat);
    //uxn_boot(u, calloc(RAMSIZE, 1));

    fprintf(stderr, "u->ram is %p\n", u->ram);
    // loaduxn
    PlatformCopyRom(u->ram + PAGE_PROGRAM, RAMSIZE - PAGE_PROGRAM);
    u16 w, h;
    PlatformGetScreenSize(&w, &h);
    w /= 8;
    h /= 8;
//    if (!initppu(&uxn_screen, w, h)) {
//        return;
//    }

    uxn_port(u, 0x0, system_dei, system_deo);
    uxn_port(u, 0x1, nil_dei, console_deo);
    //devscreen = portuxn(u, 0x2, "screen", screen_talk);
    //devaudio0 = portuxn(u, 0x3, "audio0", audio_talk);
    //portuxn(u, 0x4, "audio1", audio_talk);
    //portuxn(u, 0x5, "audio2", audio_talk);
    //portuxn(u, 0x6, "audio3", audio_talk);
    //portuxn(u, 0x7, "---", nil_talk);
    //portuxn(u, 0x8, "controller", nil_talk);
    //devmouse = portuxn(u, 0x9, "mouse", nil_talk);
    //portuxn(u, 0xa, "file", file_talk);
    //portuxn(u, 0xb, "datetime", datetime_talk);
    //portuxn(u, 0xc, "---", nil_talk);
    //portuxn(u, 0xd, "---", nil_talk);
    //portuxn(u, 0xe, "---", nil_talk);
    //portuxn(u, 0xf, "---", nil_talk);

//    mempoke16(devscreen->dat, 2, uxn_screen.hor * 8);
//    mempoke16(devscreen->dat, 4, uxn_screen.ver * 8);

    uxn_eval(u, PAGE_PROGRAM);
    //redraw(u);
}


void
uxnapp_deinit(void) {
    PlatformAudioCloseOutput();
    //PlatformFree(uxn_screen.bg.pixels);
    //PlatformFree(uxn_screen.fg.pixels);
}


void
uxnapp_runloop(void) {
    //Uxn* u = &_uxn;
    //uxn_eval(u, mempeek16(devscreen->dat, 0));
    //if(reqdraw || devsystem->dat[0xe])
    //    redraw(u);
}


void
uxnapp_setdebug(u8 debug) {
    Uxn* u = &_uxn;
    devsystem->dat[0xe] = debug ? 1 : 0;
    redraw(u);
}

/*
static int
clamp(int val, int min, int max) {
    return (val >= min) ? (val <= max) ? val : max : min;
}


void
uxnapp_movemouse(i16 mx, i16 my) {
    Uxn* u = &_uxn;

    Uint16 x = clamp(mx, 0, uxn_screen.hor * 8 - 1);
    Uint16 y = clamp(my, 0, uxn_screen.ver * 8 - 1);
    mempoke16(devmouse->dat, 0x2, x);
    mempoke16(devmouse->dat, 0x4, y);

    evaluxn(u, mempeek16(devmouse->dat, 0));
}


void
uxnapp_setmousebutton(u8 button, u8 state) {
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
}


void
uxnapp_audio_callback(Uint8 *stream, Uint32 len) {
    int running = 0;
    Sint16 *samples = (Sint16 *)stream;
    PlatformMemset(stream, 0, len);
    for(int i = 0; i < POLYPHONY; ++i) {
        running += apu_render(&_apu[i], samples, samples + len / 2);
    }
    if (!running) {
        PlatformAudioPauseOutput();
    }
}
*/

// TODO, not sure what this needs to do, in SDL emu it calls return !SDL_QuitRequested();
// in uxncli it simply returns 1
int uxn_interrupt(void)
{
    return 1;
}
