OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(start)
SECTIONS
{
	. = 0x100000;

	.text ALIGN(4096) :
	{
		__text_start = .;
		*(.text*)
		*(.gnu.linkonce.t*)
	}
	__text_end = .;

	.rodata ALIGN(4096) :
	{
		__rodata_start = .;
		start_ctors = .;
		*(.ctor*)
		end_ctors = .;

		start_dtors = .;
		*(.dtor*)
		end_dtors = .;

		*(.rodata*)
		*(.gnu.linkonce.r*)
	}
	__rodata_end = .;

	.data ALIGN(4096) :
	{
		__data_start = .;
		*(.data*)
		*(.gnu.linkonce.d*)
	}
	__data_end = .;

	.bss ALIGN(4096) :
	{
		__bss_start = .;
		*(.COMMON*)
		*(.bss*)
		*(.gnu.linkonce.b*)
	}
	__bss_end = .;

	 /DISCARD/ :
	 {
		*(.comment)
		*(.eh_frame)
	 }
	 __kernel_end = .;
	 end = .; _end = .; __end = .;
}
