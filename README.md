# Brainfuck Interpreter and JITC
## Build
```
$ make -j$(nproc)
```
## Run
Here I use `BadApple.bf` from [github.com/OpenSauce04/BadAppleBF](https://github.com/OpenSauce04/BadAppleBF/releases/tag/v2) as an example.

In interpreter mode:
```
$ ./bin/abf interpret BadApple.bf
```
In JITC mode:
```
$ ./bin/abf jit BadApple.bf
```
