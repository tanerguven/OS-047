#ifndef _X86_DESCRIPTORS_H_
#define _X86_DESCRIPTORS_H_

#ifndef ASM_FILE

/* http://pdos.csail.mit.edu/6.828/2011/readings/i386/s09_04.htm#fig9-2 */
struct PseudoDesc {
	uint16_t limit;
	uint32_t base;
} __attribute__((packed));

/* http://pdos.csail.mit.edu/6.828/2011/readings/i386/s09_05.htm */
struct GateDesc {
	uint32_t offset_15_0: 16;
	uint32_t selector : 16;

	uint32_t __37_33 : 5; /* not used */
	uint32_t __40_38 : 3; /* 0 */
	uint32_t type : 4;
	uint32_t s : 1; /* 0 */
	uint32_t dpl : 2;
	uint32_t p : 1; /* present */
	uint32_t offset_31_16 : 16;
} __attribute__((packed));

static inline void GateDesc_set(struct GateDesc *desc, uint32_t type, uint32_t selector,
						 uint32_t offset, uint32_t dpl) {
	desc->offset_15_0 = offset & 0xffff;
	desc->offset_31_16 = (offset >> 16) & 0xffff;
	desc->selector = selector;
	desc->type = type;
	desc->dpl = dpl;
	desc->__37_33 = 0;
	desc->__40_38 = 0;
	desc->s = 0;
	desc->p = 1;
}


/*
 * Intel 80386 Reference Programmer's Manual, 5.1, Figure 5-3
 * http://pdos.csail.mit.edu/6.828/2011/readings/i386/s05_01.htm#fig5-3
 */
struct SegmentDesc {
	uint32_t limit_15_0 : 16;
	uint32_t base_15_0 : 16;

	uint32_t base_23_16 : 8;
	uint32_t type : 4;
	uint32_t s : 1; /* 0 -> system, 1 -> application */
	uint32_t dpl : 2; /* Descriptor Privilege Level */
	uint32_t p : 1; /* present */

	uint32_t limit_19_16 : 4;
	uint32_t avl : 1;
	uint32_t __21 : 1; /* 0 */
	uint32_t x : 1; /* 0 -> 16 bit seg, 1 -> 32 bit seg */
	uint32_t g : 1; /* 1 -> limit>>12 */
	uint32_t base_31_24 : 8;
} __attribute__((packed));

static inline void SegmentDesc_set(struct SegmentDesc *s, uint32_t type,
								   uint32_t base, uint32_t lim, uint32_t dpl,
								   uint32_t g) {
	s->limit_15_0 = lim & 0xffff;
	s->base_15_0 = base & 0xffff;
	s->base_23_16 = (base>>16) & 0xff;
	s->type = type;
	s->s = 1;
	s->dpl = dpl;
	s->p = 1;
	s->limit_19_16 = (lim>>16) & 0xff;
	s->avl = 0;
	s->__21 = 0;
	s->x = 1;
	s->g = g;
	s->base_31_24 = base >> 24;
}

static inline void SegmentDesc_seg(struct SegmentDesc *s, uint32_t type,
								   uint32_t base, uint32_t lim, uint32_t dpl) {
	SegmentDesc_set(s, type, base, lim>>12, dpl, 1);
}

static inline void SegmentDesc_seg16(struct SegmentDesc *s, uint32_t type,
								   uint32_t base, uint32_t lim, uint32_t dpl) {
	SegmentDesc_set(s, type, base, lim, dpl, 0);
}


struct TSS {
	uint32_t back_link;
	uint32_t esp0;
	uint32_t ss0; /* 16 high bits zero */
	uint32_t esp1;
	uint32_t ss1; /* 16 high bits zero */
	uint32_t esp2;
	uint32_t ss2; /* 16 high bits zero */
	uint32_t cr3;
	uint32_t eip;
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint32_t es; /* 16 high bits zero */
	uint32_t cs; /* 16 high bits zero */
	uint32_t ss; /* 16 high bits zero */
	uint32_t ds; /* 16 high bits zero */
	uint32_t fs; /* 16 high bits zero */
	uint32_t gs; /* 16 high bits zero */
	uint32_t ldt; /* 16 high bits zero */
	uint16_t t; /* 15 high bits zero */
	uint16_t io_map_base;
};


#endif /* ASM_FILE */

/* GateDesc types */
/*** Available 32-bit TSS */
#define STS_T32A		0x9
/** 32-bit Call Gate */
#define STS_CG32		0xC
/** 32-bit Interrupt Gate */
#define STS_IG32		0xE
/** 32-bit Trap Gate */
#define STS_TG32		0xF

/* GateDesc selectors */
#define GD_KERNEL_TEXT	0x08
#define GD_KERNEL_DATA	0x10
#define GD_USER_TEXT	0x18
#define GD_USER_DATA	0x20
#define GD_TSS			0x28


/** Executable segment */
#define STA_X		0x8
/** Expand down (non-executable segments) */
#define STA_E		0x4
/** Conforming code segment (executable only) */
#define STA_C		0x4
/** Writeable (non-executable segments) */
#define STA_W		0x2
/** Readable (executable segments) */
#define STA_R		0x2
/** Accessed */
#define STA_A		0x1

#endif /* _X86_DESCRIPTORS_H_ */
