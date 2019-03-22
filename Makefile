all: sched1.o sched2.o

%.o: %.c
	x86_64-elf-gcc -Wno-builtin-declaration-mismatch  -O0 -fno-stack-protector -fvisibility=hidden -Wall -Wextra -Wpedantic -mtune=core2  -ansi -fpic -fno-builtin -ffreestanding -mcmodel=small -nostdlib -nostdinc -I. -mno-red-zone -fstack-limit-symbol=STACK_LIMIT -c $< -o $@
	x86_64-elf-objdump -d $@ > $@.O0.txt
	x86_64-elf-gcc -Wno-builtin-declaration-mismatch  -O2 -fno-partial-inlining -fno-delete-null-pointer-checks -fno-stack-protector -fvisibility=hidden -Wall -Wextra -Wpedantic -mtune=core2  -ansi -fpic -fno-builtin -ffreestanding -mcmodel=small -nostdlib -nostdinc -I. -mno-red-zone -fstack-limit-symbol=STACK_LIMIT -c $< -o $@
	x86_64-elf-objdump -d $@ > $@.O2np.txt
	x86_64-elf-gcc -Wno-builtin-declaration-mismatch  -O2 -fno-delete-null-pointer-checks -fno-stack-protector -fvisibility=hidden -Wall -Wextra -Wpedantic -mtune=core2  -ansi -fpic -fno-builtin -ffreestanding -mcmodel=small -nostdlib -nostdinc -I. -mno-red-zone -fstack-limit-symbol=STACK_LIMIT -c $< -o $@
	x86_64-elf-objdump -d $@ > $@.O2.txt

clean:
	@rm *.o *.txt
