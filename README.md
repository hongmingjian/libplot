# A small plot library for drawing curves in the Cartesian or polar system 

## What's the difference?
The libplot does not draw curves itself, instead it outputs instructions for drawing. 

The user of libplot can then interpret instructions to do the actual drawing.

Currently, it has 8 instructions as defined in the [libplot.h](https://github.com/hongmingjian/libplot/blob/master/libplot.h).

## How to use?
- The [plot.hpp](https://github.com/hongmingjian/libplot/blob/master/plot.hpp) and [plot.cpp](https://github.com/hongmingjian/libplot/blob/master/plot.cpp) can be used directly if you are using C++17 or later, as demonstrated in [demo.cpp](https://github.com/hongmingjian/libplot/blob/master/demo.cpp) for Windows.
- Run *nmake* for libplot.dll or *make* for libplot.so or libplot.dylib. The examples can be found in [libplot_qt.py](https://github.com/hongmingjian/libplot/blob/master/libplot_qt.py) for Python3 with Qt5, in [libplot-capi.lisp](https://github.com/hongmingjian/libplot/blob/master/libplot-capi.lisp) for LispWorks or [libplot-gtk.lisp](https://github.com/hongmingjian/libplot/blob/master/libplot-gtk.lisp) for SBCL+GTK3.
- For details, please look at [libplot.h](https://github.com/hongmingjian/libplot/blob/master/libplot.h).

## Notes
- When calling **Plot::render**(or its exported **plt_render**), the number of outputed **plt_render_cmd** is not known.
  It depends on number of curves and points within them and whether they have markers or not, as well as the number of grids and ticks of the coordinate.
- The **arg3** in the **plt_render_cmd** is a string of hard-coded 64 chars.
