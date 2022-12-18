This file describes the goals of adding the not yet fully designed/specified language called `natural` and it's addition to this iOS uxn emulator.

natural will be a different type of rom and will replace the uxn machine with it's own.

the varvara computer will be used in natural.

Steps towards this goal are:
- enable the native on screen keyboard linked to the controller varvarva device with a toggle similar to already existing debug, back-arrow and scale in the UI
- enable a console for typing into
- maybe create a simple rom that uses the controller device
- add natural as a submodule
- allow loading of either uxn roms or natural roms
- possibly implement gesture in the native app to send keys to the varvara controller device as an alternate to the native keyboard

End goal is an app on iOS, which can run both uxn and natural roms, and which boots into what I am conceiving of as a normal natural system: like boot-to-basic or boot into forth, a "live" coding environment filled with useful words.
