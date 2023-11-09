# Ncurses Bad Apple

This felt obligatory after making an ncurses OpenGL frontend for my beloved [3D Spinning ASCII Donut](https://github.com/wrenby/donut)

![](demo.mp4)

## To Run:

```sh
mkdir build
cd build
cmake ..
ninja # or make, or whatever your default cmake output is
cd ..
./build/badapple
```

## Dependencies:
- OpenGL 4.3
- ncurses, therefore either Unix or a MinGW environment on Windows
  - No idea if this compiles on Windows or Mac; I think some OpenGL headers are in different locations or laid out differently?
- GLEW
- GLM
- libVLC
