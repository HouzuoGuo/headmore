# headmore
A fully functional VNC client for your geeky character terminals (Linux VT, xterm, and more).

## Screenshots
![screenshot 1](https://raw.githubusercontent.com/HouzuoGuo/headmore/master/demo1.jpg)
![screenshot 2](https://raw.githubusercontent.com/HouzuoGuo/headmore/master/demo2.jpg)

## Compile and Run
`headmore` requires the following components to compile and run:
- `libcaca` and its development files
- `libvncserver` (or `libvncclient`) and its development files

The two libraries themself depend on:
- `libgcrypt` and its development files
- `libjpeg8` and its development files
- `libopenssl` and its development files
- `libpng16-compat` and its development files

After having installed the dependencies, simply run `make`, then start your favourite VNC server (`vncsever` for example), and `./headmore host_or_ip:port`!

## Distribution Package
I will be very happy to assist you (as a packager) to make headmore available in your favourite Linux/BSD/Solaris distribution. A sample RPM package is available [here](https://build.opensuse.org/package/show/home:guohouzuo/headmore).

## Keyboard controls
`headmore` has two input modes that determine where your keyboard input goes, the mode switch is carried out by backtick (`) key.

In general, the left hand side keys pan and zoom viewer, the right hand side keys clicks mouse and move cursor. The key mapping is comprehensively explained in the program's help menu (type `h`), the manual page (`man 1 headmore`), and the illustration below:

![screenshot 1](https://raw.githubusercontent.com/HouzuoGuo/headmore/master/key-map.png)

## Feedback
You are most welcomed to contribute code and file bug report, feature request, and questions in the project's [issues page](https://github.com/HouzuoGuo/headmore/issues), you may also direct any inquiry to the author in [Email](mailto:guohouzuo@gmail.com).

## License
Copyright (c) 2017 Howard Guo <guohouzuo@gmail.com>

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

See `LICENSE` file for the full license text.
