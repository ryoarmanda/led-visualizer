# LED Visualizer

This repo contains my personal project code for visualizing music through the usage
of Fast Fourier Transform (FFT) and individually addressable LEDs.

## `LinearMusicStrip`

This visualizes the music as stationary LEDs, where each LED represents one FFT bin, and pulses in brightness according to the recorded magnitude of the bin.

## `LogMusicStrip`

Similar to `LinearMusicStrip`, but it makes sure all FFT bins are
represented within the length of the LED strip. Each LED may represent
more than one bin, and the bins are aggregated through a logarithmic formula.

## `MovingVisualizer`

This visualizes the music as moving LEDs, where the peak frequency of the
audio samples are represented as multiple LEDs. The whole strip will pulse
in brightness according to the peak frequency's magnitude.
