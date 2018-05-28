OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(main)
SECTIONS
{
	. = 0x01000000;
	.text ALIGN(4096) :
	{
		*(.text*)
		*(.gnu.linkonce.t*)
	}

	.rodata ALIGN(4096) :
	{
		start_ctors = .;
		*(.ctor*)
		end_ctors = .;

		start_dtors = .;
		*(.dtor*)
		end_dtors = .;

		*(.rodata*)
		*(.gnu.linkonce.r*)
	}

	.data ALIGN(4096) :
	{
		*(.data*)
		*(.gnu.linkonce.d*)
	}

	.bss ALIGN(4096) :
	{
		_bss = .;
		*(.COMMON*)
		*(.bss*)
		*(.gnu.linkonce.b*)
		_end = .;
	}

	 /DISCARD/ :
	 {
		*(.comment)
		*(.eh_frame)
	 }
	 end = .; _end = .; __end = .;
}
