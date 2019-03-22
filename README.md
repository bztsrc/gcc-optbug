gcc optimizer problem
---------------------

This is an example of misbehaving `gcc -O2`. With `-O0` or `-fno-partial-inlining` or with Clang the problem does not appear.

The gcc is an unpatched cross-gcc (see the .o.txt files for dumps):
```
$ x86_64-elf-gcc --version
x86_64-elf-gcc (GCC) 8.2.0
Copyright (C) 2018 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

$ sha256sum x86_64-elf-gcc
42332f73f1cb1dc89b975efeb38063d7bbea6b4c653bcd1f3a3e536262391dfb  x86_64-elf-gcc
```
But the problem also produced by the original gcc shipped with my distro (see the .o.gcc.txt files for dumps):
```
$ gcc --version
gcc (GCC) 8.2.1 20181127
Copyright (C) 2018 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

$ sha256sum gcc
cea3b14bfff153c1834c57b1578c45f71532a7a1820ff860f8af42ce334158c2  gcc
```

Difference between sched1.c and sched2.c:
```
$ diff sched1.c sched2.c
395d394
<                 continue;
```

In the original build environment the optimizer generates a code in which sched_pick jumps to the middle of sched_awake without
setting %rdi, causing page faults. This does not happen in this minimal example, but instead it creates a new `sched_pick.part.1`
function after sched_awake.

Non-the-less this repo demonstrates that a "continue" which should be irrelevant influences the optimizer in a way it shouldn't.
Also note that the generated code __is incorrect__ as it does not call sched_awake(tcba) from sched_pick() at all. With -O0 there
should be a "callq sched_awake" somewhere in the sched_pick, but there's none.

Cheers,
bzt
