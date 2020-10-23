# LED Visualizer

This repo contains my personal project code for visualizing music through the usage
of Fast Fourier Transform (FFT) and individually addressable LEDs.

## `led_music_strip_linear`

This visualizes the music as stationary LEDs, where each LED represents one FFT bin, and pulses in brightness according to the recorded magnitude of the bin.

## `led_music_strip_logarithmic`

Similar to `led_music_strip_linear`, but it makes sure all FFT bins are
represented within the length of the LED strip. Each LED may represent
more than one bin, and the bins are aggregated through a logarithmic formula.

## `led_visualizer`

This visualizes the music as moving LEDs, where the peak frequency of the
audio samples are represented as multiple LEDs. The whole strip will pulse
in brightness according to the peak frequency's magnitude.
