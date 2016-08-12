# x64_tracer
x64dbg conditional branches logger [Plugin]

This is just a work in progress so don't expect too much.

 

Please test it and report if you find bugs.

 

I use it like this :

 

First you need 2 break points to trace between, Start and End.

 

1 - Throw your target in the debugger.

2 - The Start point should break.

3 - Start the plugin.

4 - Enter the name of the module you are interested in, the plugin will try to detect the name where RIP is now.

5 - Enter the target VA, i.e the point where logging should stop, It's your End point from above.

 

There will be single stepping into this module but if RIP goes out of this module then there will be stepping over

in those external modules unless there is a call back into the that target module then there will be a single step into the target module.

 

5 - stepping will continue until we hit the 2nd point.

6 - The plugin will show a message box telling we have ended tracing.

7 - now you can save the result to a log file which looks like this in the image below.

8 - you can use any diffing system to compare the results between 2 traces, here I used a plugin for Notepad++.
