#include <time.h>
#include "emu_platform.h"
#include "app_uxn.h"
#include "stdlib.h" // calloc
#include "stdio.h" // fprintf, stderr
#include "uxn.h"

#include "devices/system.c"
//#include "devices/screen.h"
#include "devices/screen.c"
//#include "devices/audio.c"
#if DEBUG
#include "uxn.c"
#else
#include "uxn-fast.c"
#endif

static Uxn _uxn;
UxnScreen uxn_screen;
static Device *devsystem, *devscreen;
//*devscreen, *devmouse, *devaudio0;
static Uint8 reqdraw = 0;

// copied from uxn/src/devices/screen.c
/*
static Uint8 blending[5][16] = {
    {0, 0, 0, 0, 1, 0, 1, 1, 2, 2, 0, 2, 3, 3, 3, 0},
    {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3},
    {1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1},
    {2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2},
    {1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0}};
*/

static void
redraw(Uxn *u) {
    // system 0xe set means debug, for now skip it
    // in the case of the screen this means add data/lines/things
    //if(devsystem->dat[0xe]) {
    //    inspect(&uxn_screen, u->wst.dat, u->wst.ptr, u->rst.ptr, u->ram.dat);
    //}
    // TODO here, we might call screen_redraw() to translate fg/bg 2-bit pixels to palette-based 8-bit pixels
    // but probably makes more sense to over-ride screen_write and just write directly to each fg/bg layer and'
    // skip allocating the higher-level uxn_screen.pixels altogether as
    // it is not needed due to ios supporting two layers.
    u16 width, height;
    PlatformGetScreenSize(&width, &height);
    if (width != uxn_screen.width || height != uxn_screen.height) {
        fprintf(stderr,"in redraw() needed to change screen from %d by %d to %d by %d\n",
                width, height, uxn_screen.width, uxn_screen.height);
        PlatformSetScreenSize(uxn_screen.width, uxn_screen.height);
    }
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
    // screen_resize() call elsewhere initializes these arrays of pixels
    PlatformDrawBackground(&bg);
    PlatformDrawForeground(&fg);
    fprintf(stderr, "in redraw() set reqdraw=0\n");
    reqdraw = 0;
}

static void
console_deo(Device *d, Uint8 port) {
    // copied from uxn/src/uxnemu.c
    //fprintf(stderr, "console_deo(device=%p, port=%d)\n", d, port);
    FILE *fd = port == 0x8 ? stdout : port == 0x9 ? stderr : 0;
    //fprintf(stderr, "fd=%d\n");
    // can't be if (fd) because stdout/stderr can be 0 or 1 instead of NULL
    
    if(fd != NULL) {
        fputc(d->dat[port], fd);
        fflush(fd);
    }
    //if (port == 0x8) {
    //    fputc(d->dat[port], stdout);
   // }
}

void
screen_rainbow(UxnScreen *p, Layer *layer)
{
    Uint32 x,y;
    for(x=0; x<p->width; x++) {
        for(y=0; y<p->height; y++) {
            Uint32 loc = (y*p->width + x) *4;
            layer->pixels[loc] = y & 0xff; // blue
            layer->pixels[loc+1] = x & 0xff; // green
            layer->pixels[loc+2] = x & 0xff; // red
            layer->pixels[loc+3] = 0x0; // alpha (transparency)
        }
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
xscreen_resize(UxnScreen *p, Uint16 width, Uint16 height)
{
    // I get an EXC_BAD_ACCESS in Platform.m - (void)setBackgroundPixels:(void *)pixels {
    PlatformSetScreenSize(width, height);
    //CGSize canvasSize = self.canvasSize;
    //NSUInteger count = 4 * canvasSize.width * canvasSize.height;
  //  self.bgPixels = [NSData dataWithBytes:pixels length:count];
//}
    // but why? is it because this realloc() isn't quite right? doesn't PERSIST somehow? Maybe need to call calloc/PlatformAlloc?
    if (p->bg.pixels != NULL) free(p->bg.pixels);
    if (p->fg.pixels != NULL) free(p->fg.pixels);
    /* if (p->pixels != NULL) free(p->pixels); */
    int size = 4 * width * height;
    Uint8
        *bg = malloc(size),
        *fg = malloc(size);
    //Uint32
    //    *pixels = malloc(4 * width * height * sizeof(Uint32));
    if(bg) p->bg.pixels = bg;
    if(fg) p->fg.pixels = fg;
    //if(pixels) p->pixels = pixels;
    fprintf(stderr,"screen_resize(), size=%d, p->bg.pixels=%p, p->fg.pixels=%p\n",
            size,
             p->bg.pixels, p->fg.pixels /*, p->pixels */);
    /* p->pixels=%p\n",*/
    if(bg && fg /* && pixels */) {
        p->width = width;
        p->height = height;
        screen_fill(p, &p->bg, 0xff); // white/opaque background
        screen_fill(p, &p->fg, 0x00); // clear/black foreground
    }
}

void
screen_write(UxnScreen *p, Layer *layer, Uint16 x, Uint16 y, Uint8 color)
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
            fprintf(stderr, "screen_write() already written that color to coordinate\n");
        }
  }
}

static void
xscreen_blit(UxnScreen *p, Layer *layer, Uint16 x, Uint16 y, Uint8 *sprite, Uint8 color, Uint8 flipx, Uint8 flipy, Uint8 twobpp)
{
    int v, h, opaque = blending[4][color];
    for(v = 0; v < 8; v++) {
        Uint16 c = sprite[v] | (twobpp ? sprite[v + 8] : 0) << 8;
        for(h = 7; h >= 0; --h, c >>= 1) {
            Uint8 ch = (c & 1) | ((c >> 7) & 2);
            if(opaque || ch)
                screen_write(p,
                    layer,
                    x + (flipx ? 7 - h : h),
                    y + (flipy ? 7 - v : v),
                    blending[ch][color]);
        }
    }
}


Uint8 xscreen_dei(Device *d, Uint8 port)
{
    switch(port) {
        case 0x2: return uxn_screen.width >> 8;
        case 0x3: return uxn_screen.width;
        case 0x4: return uxn_screen.height >> 8;
        case 0x5: return uxn_screen.height;
        default: return d->dat[port];
    }
}

void
xscreen_deo(Device *d, Uint8 port) {
    fprintf(stderr, "screen_deo(), port=%0x\n", port);
    switch(port) {
        case 0xe: {
            Uint16 x,y;
            Uint8 layerIndex = d->dat[0xe] & 0x40;
            DEVPEEK16(x, 0x8);
            DEVPEEK16(y, 0xa);
            Layer layer = *(layerIndex ? &uxn_screen.fg : &uxn_screen.bg);
            screen_write(&uxn_screen, &layer, x, y, d->dat[0xe] & 0x3);
            if(d->dat[0x6] & 0x01) DEVPOKE16(0x8, x + 1); /* auto x+1 */
            if(d->dat[0x6] & 0x02) DEVPOKE16(0xa, y + 1); /* auto y+1 */
            fprintf(stderr,"in screen_deo() setting reqdraw=1\n");
            reqdraw = 1;
            break;
        }
        case 0xf: {
            Uint16 x, y, dx, dy, addr;
            Uint8 i, n, twobpp = !!(d->dat[0xf] & 0x80);
            Layer *layer = (d->dat[0xf] & 0x40) ? &uxn_screen.fg : &uxn_screen.bg;
            DEVPEEK16(x, 0x8);
            DEVPEEK16(y, 0xa);
            DEVPEEK16(addr, 0xc);
            n = d->dat[0x6] >> 4;
            dx = (d->dat[0x6] & 0x01) << 3;
            dy = (d->dat[0x6] & 0x02) << 2;
            if(addr > 0x10000 - ((n + 1) << (3 + twobpp)))
                return;
            for(i = 0; i <= n; i++) {
                screen_blit(&uxn_screen, layer, x + dy * i, y + dx * i, &d->u->ram[addr], d->dat[0xf] & 0xf, d->dat[0xf] & 0x10, d->dat[0xf] & 0x20, twobpp);
                addr += (d->dat[0x6] & 0x04) << (1 + twobpp);
            }
            DEVPOKE16(0xc, addr);   /* auto addr+length */
            DEVPOKE16(0x8, x + dx); /* auto x+8 */
            DEVPOKE16(0xa, y + dy); /* auto y+8 */
            break;
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

// copied from uxn/src/devices/screen.c
/*void
screen_palette(UxnScreen *p, Uint8 *addr)
{
    int i, shift;
    for(i = 0, shift = 4; i < 4; ++i, shift ^= 4) {
        Uint8
            r = (addr[0 + i / 2] >> shift) & 0x0f,
            g = (addr[2 + i / 2] >> shift) & 0x0f,
            b = (addr[4 + i / 2] >> shift) & 0x0f;
        p->palette[i] = 0x0f000000 | r << 16 | g << 8 | b;
        p->palette[i] |= p->palette[i] << 4;
    }
    p->fg.changed = p->bg.changed = 1;
}*/
void
set_palette(UxnScreen *p, Device *d, Uint8 port)
{
    p->palette[port-0x8] = d->dat[port] << 16 | d->dat[port+1];
}

// copied from uxn/src/devices/screen.c
void
xscreen_palette(UxnScreen *p, Uint8 *addr)
{
    int i, shift;
    for(i = 0, shift = 4; i < 4; ++i, shift ^= 4) {
        Uint8
            r = (addr[0 + i / 2] >> shift) & 0x0f,
            g = (addr[2 + i / 2] >> shift) & 0x0f,
            b = (addr[4 + i / 2] >> shift) & 0x0f;
        p->palette[i] = 0x0f000000 | r << 16 | g << 8 | b;
        p->palette[i] |= p->palette[i] << 4;
    }
    p->fg.changed = p->bg.changed = 1;
}

// TODO refactor system_deo_special() into uxn/src/devices/screen.c?
// copied from uxn/src/uxnemu.c (SDL emulator)
// screen_palette is in uxn/src/devices/screen.c so port independent
void system_deo_special(Device *d, Uint8 port)
{
    if(port > 0x7 && port < 0xe)
            screen_palette(&uxn_screen, &d->dat[0x8]);
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
    // maybe PlatformAlloc()
    //uxn_boot(u, calloc(RAMSIZE, 1));

    fprintf(stderr, "u->ram is %p\n", u->ram);
    // loaduxn
    PlatformCopyRom(u->ram + PAGE_PROGRAM, RAMSIZE - PAGE_PROGRAM);
    u16 w, h;
    PlatformGetScreenSize(&w, &h);
    //w /= 8;
    //h /= 8;
//    if (!initppu(&uxn_screen, w, h)) {
//        return;
//    }
    screen_resize(&uxn_screen, w, h);
    // I think I need a screen_resize() like function which initializes both layers in uxn_screen.
    

    devsystem = uxn_port(u, 0x0, system_dei, system_deo);
    uxn_port(u, 0x1, nil_dei, console_deo);
    devscreen = uxn_port(u, 0x2, /*screen_dei*/ screen_dei, screen_deo);
    //devaudio0 = portuxn(u, 0x3, "audio0", audio_talk);
    uxn_port(u, 0x3, nil_dei, nil_deo);
    //portuxn(u, 0x4, "audio1", audio_talk);
    uxn_port(u, 0x4, nil_dei, nil_deo);
    //portuxn(u, 0x5, "audio2", audio_talk);
    uxn_port(u, 0x5, nil_dei, nil_deo);
    //portuxn(u, 0x6, "audio3", audio_talk);
    uxn_port(u, 0x6, nil_dei, nil_deo);
    //portuxn(u, 0x7, "---", nil_talk);
    uxn_port(u, 0x7, nil_dei, nil_deo);
    //portuxn(u, 0x8, "controller", nil_talk);
    uxn_port(u, 0x8, nil_dei, nil_deo);
    //devmouse = portuxn(u, 0x9, "mouse", nil_talk);
    uxn_port(u, 0x9, nil_dei, nil_deo);
    //portuxn(u, 0xa, "file", file_talk);
    uxn_port(u, 0xa, nil_dei, nil_deo);
    //portuxn(u, 0xb, "datetime", datetime_talk);
    uxn_port(u, 0xa, nil_dei, nil_deo);
    //portuxn(u, 0xc, "---", nil_talk);
    uxn_port(u, 0xb, nil_dei, nil_deo);
    //portuxn(u, 0xd, "---", nil_talk);
    uxn_port(u, 0xc, nil_dei, nil_deo);
    //portuxn(u, 0xe, "---", nil_talk);
    uxn_port(u, 0xd, nil_dei, nil_deo);
    //portuxn(u, 0xf, "---", nil_talk);
    uxn_port(u, 0xe, nil_dei, nil_deo);

    devsystem->dat[0x2] = uxn_screen.width * 8;
    devsystem->dat[0x4] = uxn_screen.height * 8;
//    mempoke16(devscreen->dat, 2, uxn_screen.hor * 8);
//    mempoke16(devscreen->dat, 4, uxn_screen.ver * 8);

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
    /*
    if (uxn_screen.pixels != NULL) PlatformFree(uxn_screen.pixels);
    uxn_screen.pixels = NULL;
*/
}


void
uxnapp_runloop(void) {
    Uxn* u = &_uxn;
    uxn_eval(u, devscreen->dat[0x0]); // callback that something has changed
    //uxn_eval(u, PAGE_PROGRAM);
    if(reqdraw || devsystem->dat[0xe]) // request draw || debug mode ( system 0xe set )
        redraw(u);
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
