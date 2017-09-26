# *Robot Fun Police*

*Robot Fun Police* is *Jordan Tick*'s implementation of [*Robot Fun Police*](http://graphics.cs.cmu.edu/courses/15-466-f17/game2-designs/jmccann) for game2 in 15-466-f17.

![alt text](https://raw.githubusercontent.com/jrtick/15-466-f17-base2/master/screenshots/screenshot.png)

## Asset Pipeline

The assets used for this game are in models/robot.blend. They are processed via the blender python api into byte 'blob' files that list out the meshes' vertices, normals, and colors.

Hierarchy is set in main.cpp by parenting transforms to their parent transform.

## Architecture

There is a struct of rotations that describes the state of the robot arm. It can be manipulated through ('a','s'),('w','e'),('z','x'),and ('d', 'c').

There is a class for balloons. When all balloons instantiated are set to the state 'Gone', the game is over. Until then, the balloon positions are stepped forward by their velocities. Collision between robot needle and balloon was determined by instantiating a small cube at the tip of the needle such that checking the distance between this cube and the balloon centers determines collision.

## Reflection

It was a little difficult to add vertex colors into the game. At one point, a struct string I originally had as "v3n3c4" was packed to be 8 chars instead of 6. To combat this, I just left it as "v3n3" which correctly packed to 4.

The balloon and robot logic was simple! Editing the source code for the shaders was cool. If I was doing it again, I'd try to get more familiar with the blender API and set hierarchy into the blob file. 

My computer didn't seem to cooperate with the SDL keysyms for period and comma, which is why I switched to using letter keys instead. That was my only issue with the design doc!


# About Base2

This game is based on Base2, starter code for game2 in the 15-466-f17 course. It was developed by Jim McCann, and is released into the public domain.

## Requirements

 - modern C++ compiler
 - glm
 - libSDL2
 - libpng
 - blender (for mesh export script)

On Linux or OSX these requirements should be available from your package manager without too much hassle.

## Building

This code has been set up to be built with [FT jam](https://www.freetype.org/jam/).

### Getting Jam

For more information on Jam, see the [Jam Documentation](https://www.perforce.com/documentation/jam-documentation) page at Perforce, which includes both reference documentation and a getting started guide.

On unixish OSs, Jam is available from your package manager:
```
	brew install ftjam #on OSX
	apt get ftjam #on Debian-ish Linux
```

On Windows, you can get a binary [from sourceforge](https://sourceforge.net/projects/freetype/files/ftjam/2.5.2/ftjam-2.5.2-win32.zip/download),
and put it somewhere in your `%PATH%`.
(Possibly: also set the `JAM_TOOLSET` variable to `VISUALC`.)

### Bulding
Open a terminal (on windows, a Visual Studio Command Prompt), change to this directory, and type:
```
	jam
```

### Building (local libs)

Depending on your OSX, clone 
[kit-libs-linux](https://github.com/ixchow/kit-libs-linux),
[kit-libs-osx](https://github.com/ixchow/kit-libs-osx),
or [kit-libs-win](https://github.com/ixchow/kit-libs-win)
as a subdirectory of the current directory.

The Jamfile sets up library and header search paths such that local libraries will be preferred over system libraries.
