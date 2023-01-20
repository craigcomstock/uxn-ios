#include <time.h>
#include "emu_platform.h"
#include "app_uxn.h"
#include "stdlib.h" // calloc
#include "stdio.h" // fprintf, stderr
//#include "uxn.h"

#include "devices/system.c"
#include "devices/screen.c"
#include "devices/datetime.c"
#include "devices/mouse.c"
//#include "devices/audio.c"
#if DEBUG
#include "uxn.c"
#else
#include "uxn-fast.c"
#endif

static Uxn _uxn;
//UxnScreen uxn_screen; // over in screen.c
//static Uint8 *devsystem, *devscreen, *devmouse;
// *devaudio0;
static Uint8 reqdraw = 0;
/*
void
ios_screen_resize(UxnScreen *p, Uint16 width, Uint16 height)
{
    Uint8
        *bg = realloc(p->bg.pixels, width * height),
        *fg = realloc(p->fg.pixels, width * height);
    Uint8
        *pixels = realloc(p->pixels, 4 * width * height * sizeof(Uint8));
    if(bg) p->bg.pixels = bg;
    if(fg) p->fg.pixels = fg;
    if(pixels) p->pixels = pixels;
    if(bg && fg && pixels) {
        p->width = width;
        p->height = height;
        screen_clear(p, &p->bg);
        screen_clear(p, &p->fg);
    }
}

void
ios_screen_clear(UxnScreen *p, Layer *layer)
{
    Uint32 i, size = p->width * p->height;
    for(i = 0; i < size; i++)
        layer->pixels[i] = 0x00;
    layer->changed = 1;
}
*/

static void
redraw(Uxn *u) {
    // system 0xe set means debug, for now skip it
    // in the case of the screen this means add data/lines/things
    //if(devsystem->dat[0xe]) {
    //    inspect(&uxn_screen, u->wst.dat, u->wst.ptr, u->rst.ptr, u->ram.dat);
    //}
    // TODO here, we might call screen_redraw() to translate fg/bg 2-bit pixels to palette-based 8-bit pixels
    // but probably makes more sense to over-ride screen_write and just write directly to each fg/bg layer and
    // skip allocating the higher-level uxn_screen.pixels altogether as
    // it is not needed due to ios supporting two layers.
    u16 width, height;
    PlatformGetScreenSize(&width, &height);
    if (width != uxn_screen.width || height != uxn_screen.height) {
        fprintf(stderr,"in redraw() needed to change screen from %d by %d to %d by %d\n",
                width, height, uxn_screen.width, uxn_screen.height);
        PlatformSetScreenSize(uxn_screen.width, uxn_screen.height);
    }
    screen_redraw(&uxn_screen, uxn_screen.pixels);
    //PlatformBitmap bg = {
    //    .width = uxn_screen.width,
    //    .height = uxn_screen.height,
    //    .pixels = uxn_screen.bg.pixels,
    //};
    PlatformBitmap fg = {
        .width = uxn_screen.width,
        .height = uxn_screen.height,
        .pixels = uxn_screen.pixels,
    };
    // screen_resize() call elsewhere initializes these arrays of pixels
    //PlatformDrawBackground(&bg);
    PlatformDrawForeground(&fg);
    fprintf(stderr, "in redraw() set reqdraw=0\n");
    reqdraw = 0;
}

// copied from uxn/src/uxnemu.c
static void
console_deo(Uint8 *d, Uint8 port) {
    //fprintf(stderr, "console_deo(device=%p, port=%d, data=%c(%0x)\n", d, port, d->dat[port], d->dat[port]);
    FILE *fd = port == 0x8 ? stdout : port == 0x9 ? stderr : 0;
    // can't be if (fd) because stdout/stderr can be 0 or 1 instead of NULL
    
    if(fd != NULL) {
        fputc(d[port], fd);
        fflush(fd);
    }
}

void
screen_fill(UxnScreen *p, Layer *layer, Uint8 value)
{
    Uint32 i, size = p->width * p->height * 4;
    /*
     pretty pattern:
     
    Uint32 x,y;
    for(x=0; x<p->width; x++) {
        for(y=0; y<p->height; y++) {
            Uint32 loc = (y*p->width + x) *4;
            layer->pixels[loc] = y & 0xff; // blue
            layer->pixels[loc+1] = y & 0xff; // green
            layer->pixels[loc+2] = x & 0xff; // red
            layer->pixels[loc+3] = 0x0; // alpha (transparency)
        }
    }*/
    
    for(i = 0; i < size; i++)
    {
        layer->pixels[i] = value; // bgra - blue green red alpha, so maybe empty and transparent?
    }
    layer->changed = 1;
}

void
xscreen_write(UxnScreen *p, Layer *layer, Uint16 x, Uint16 y, Uint8 color)
{
    if(x < p->width && y < p->height) {
        Uint32 i = (x + y * p->width) * 4;
        Uint32 color_value = p->palette[color];
        if(color_value != (layer->pixels[i]<<12) +
           (layer->pixels[i+1]<<8) +
           (layer->pixels[i+2]<<4) +
           (layer->pixels[i+3])) {
          layer->pixels[i] = color_value >> 12;
          layer->pixels[i+1] = color_value >> 8 & 0xff;
          layer->pixels[i+2] = color_value >> 4 & 0xff;
          layer->pixels[i+3] = color_value & 0xff;
          layer->changed = 1;
        } else {
            //fprintf(stderr, "screen_write() already written that color to coordinate\n");
        }
  }
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

void
set_palette(UxnScreen *p, Uint8 *d, Uint8 port)
{
    p->palette[port-0x8] = d[port] << 16 | d[port+1];
}

// TODO refactor system_deo_special() into uxn/src/devices/screen.c?
// copied from uxn/src/uxnemu.c (SDL emulator)
// screen_palette is in uxn/src/devices/screen.c so port independent
void system_deo_special(Uint8 *d, Uint8 port)
{
    if(port > 0x7 && port < 0xe)
            screen_palette(&uxn_screen, &d[0x8]);
}

// TODO refactor nil_dei() and nil_deo() from uxn/src/uxnmenu.c to uxn/src/devices/something.c?
// copied from uxn/src/uxnemu.c (SDL emulator)
/*
static Uint8
nil_dei(Uint8 *d, Uint8 port)
{
    return d[port];
}
static void
nil_deo(Uint8 *d, Uint8 port)
{
    (void)d;
    (void)port;
}
*/

#define RAMSIZE 0x10000


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
    case 0xc0: return datetime_dei(&u->dev[d], p);
    }
    return u->dev[addr];
    return 0;
}

static void
emu_deo(Uxn *u, Uint8 addr, Uint8 v)
{
    Uint8 p = addr & 0x0f, d = addr & 0xf0;
    u->dev[addr] = v;
    switch(d) {
    case 0x00:
        system_deo(u, &u->dev[d], p);
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
    fprintf(stderr, "uxnapp_init()\n");
    Uxn* u = &_uxn;

    fprintf(stderr, "before uxn_boot()\n");
    //Uint8 dat[0x10000];
    if(!uxn_boot(u, (Uint8 *)calloc(0x10300, sizeof(Uint8)), emu_dei, emu_deo))
    {
        fprintf(stderr, "failed to boot uxn\n");
        return;
    }
    // maybe PlatformAlloc()
    //uxn_boot(u, calloc(RAMSIZE, 1));

    fprintf(stderr, "u->ram is %p\n", u->ram);
    // loaduxn
    PlatformCopyRom(u->ram + PAGE_PROGRAM, RAMSIZE - PAGE_PROGRAM);
    u16 w, h;
    PlatformGetScreenSize(&w, &h);
    screen_resize(&uxn_screen, w, h); // this function allocates foreground, background and resulting pixels

    u->dev[0x2] = uxn_screen.width;
    u->dev[0x4] = uxn_screen.height;

    uxn_eval(u, PAGE_PROGRAM);
    redraw(u);
}


void
uxnapp_deinit(void) {
    PlatformAudioCloseOutput();
    if (uxn_screen.bg.pixels != NULL) PlatformFree(uxn_screen.bg.pixels);
    uxn_screen.bg.pixels = NULL;
    if (uxn_screen.fg.pixels != NULL) PlatformFree(uxn_screen.fg.pixels);
    uxn_screen.fg.pixels = NULL;
}


void
uxnapp_runloop(void) {
    Uxn* u = &_uxn;
    // TODO should this function ALWAYS eval uxn on the screen vector?
    // should call "frame" in mouseconsole.tal right?
    //fprintf(stderr,"devscreen vector is %d\n", GETVECTOR(devscreen));
    // ^^^ this is called often and the on-frame in mouseconsole.rom is called as well
    uxn_eval(u, GETVEC(&u->dev[0x20]));
    if(reqdraw || u->dev[0xe]) // request draw || debug mode ( system 0xe set )
        redraw(u);
}


void
uxnapp_setdebug(u8 debug) {
    Uxn* u = &_uxn;
    u->dev[0xe] = debug ? 1 : 0;
    redraw(u);
}

// TODO mouse_scroll(devmouse, x, y);
void
uxnapp_movemouse(i16 mx, i16 my) {
    Uxn* u = &_uxn;
    fprintf(stderr,"uxnapp_movemouse()\n");
    Uint16 x = clamp(mx, 0, uxn_screen.width * 8 - 1);
    Uint16 y = clamp(my, 0, uxn_screen.height * 8 - 1);
    mouse_pos(u, &u->dev[0x90], x, y);
}


void
uxnapp_setmousebutton(u8 button, u8 state) {
    Uxn* u = &_uxn;
    fprintf(stderr,"uxnapp_setmousebutton()\n");
    Uint8 flag = 0x00;
    switch (button) {
        case 0: flag = 0x01; break;
        case 1: flag = 0x10; break;
    }

    if (state) {
        mouse_down(u, &u->dev[0x90], flag);
    }
    else {
        mouse_up(u, &u->dev[0x90], flag);
    }
}

/*
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
