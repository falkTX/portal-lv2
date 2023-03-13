# portal-lv2

The Portal audio plugins allow you to magically send audio from one side of the processing chain into another.

They can be used as a way to parallelize the audio processing by forcely splitting a single chain into 2.

## Explanation

Imagine the scenario of 4 plugins running in series (one after the other), where each plugin is given a letter:

  A -> B -> C -> D

Plugin D waits for C, which waits for B which waits for A, all a single line.
In audio processing each plugin runs consecutively one after the other, typically on a single CPU core.
From A to D, all plugins must be able to process audio within a certain time limit, otherwise xruns occur.
If one plugin in the chain is heavy, it will limit the amount of time and processing available to the others.

Now imagine the scenario of 4 plugins running in parallel, using 2 series for simplicity:

  A -> B
  C -> D

There are only 2 dependency chains now, B waits for A and D waits for C.
This means the 2 series can run in parallel, each on its own CPU core.
In the audio processing side the time spent running all plugins is now up to half of the first example.
We can run as many series as there are cores in the system while keeping good performance.

By splitting the processing chain in 2, the Portal audio plugins allow running the 2nd part of a chain in another CPU core.
Simply load both sink and source portal plugins and connect each to the appropriate plugins.
Note that only load 1 sink and 1 source can be loaded at a time.

The side effect is 1 audio cycle of latency coming out of the source/exit/transmitter portal.
For some effects like delay and reverb this could be a worthy compromise.

## Requirements

There are a few requirements for the host side in order to make this plugin setup work:

- Host does per-audio-chain/mixer multi-threaded plugin processing
- Host supports fixed-size audio buffers
- Host does not do plugin bridging or any kind of out-of-process sync

Expect undefined behaviour when these requirements are not met.

## Additional Credits

Based on an original idea by Florian Schmidt.

Portal graphics based on https://codepen.io/propjockey/pen/vYxraBz

> Copyright (c) 2023 by Jane Ori
> 
> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
> 
> The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
> 
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
