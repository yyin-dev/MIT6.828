# Chapter 5 Memory Management

## 5.1 Segment Translation

![img](https://pdos.csail.mit.edu/6.828/2018/readings/i386/fig5-1.gif)

Seems that "PAGING ENABLED" and "PAGING DISABLED" should be reversed.

```
Logical address [selector:offset] -- segmentation --> linear address 
Linear address  -- paging --> physical address

If paging disabled:
linear address = physical address

Logical address === Virtual address
```

In x86, segmenting is always enabled. Virtual address consists a 15-bit selector from segment register and a 32-bit offset: `[selector:offset]`.

Data structures used in segmentation:

- Descriptor (and descriptor table)
- Selector
- Segment Registers

#### Descriptors

![img](https://pdos.csail.mit.edu/6.828/2018/readings/i386/fig5-2.gif)

![img](https://pdos.csail.mit.edu/6.828/2018/readings/i386/fig5-3.gif)

#### Descriptor table

Segment descriptors are stored in either Global Descriptor Table (GDT) or Local Descriptor Table (LDT). A descriptor table is simply an array of 8-byte entries that contain descriptors.

LDT stores memory segments specific to a specific program, while GDT stores global segments.

The processor locates the GDT and the current LDT by means of the GDTR and LDTR registers. These registers store the base addresses of the tables in the **linear** address space and store the segment limits. Use instructions of`LGDT`, `SGDT`, `LLDT`, and `SLDT`.

#### Selector

The selector portion of a logical address identifies a descriptor by specifying a descriptor table and indexing a descriptor within that table.

![img](https://pdos.csail.mit.edu/6.828/2018/readings/i386/fig5-6.gif)

Index: Selects one of the descriptors in a descriptor table. `SegmentSelectorAddr = 8 * Index + DescriptorTableBaseAddr`.

Table Indicator: 0 for GDT, 1 for LDT.

As entry 0 of GDT is not used, a selector with index = table indicator = 0 can be used as a null selector. An exception is raised when a segment register is used to access memory. 

#### Segment Registers

CS, SS, DS, ES, FS, GS.



## 5.2 Page Translation

Page translation is enabled when the PG bit of %cr0 is set: linear address --> physical address.

#### Page frame

A 4k-byte unit of contiguous addresses of physical memory. 

#### Linear Address

![img](https://pdos.csail.mit.edu/6.828/2018/readings/i386/fig5-8.gif)

![img](https://pdos.csail.mit.edu/6.828/2018/readings/i386/fig5-9.gif)

#### Page Tables

A page table is an array of 32-bit page specifiers. A page table is no larger than a page.

The **physical address** of the current page directory is stored in %cr3. 



#### Page Table Entries

PTE in each level has the same format.

![img](https://pdos.csail.mit.edu/6.828/2018/readings/i386/fig5-10.gif)

The TLB should be flushed whenever the page tables are changed. This can be done by reloading %cr3 with a `MOV` instruction: `mov %eax, %cr3`.



## 5.3 Combining Segment and Page Translation

![img](https://pdos.csail.mit.edu/6.828/2018/readings/i386/fig5-12.gif)

#### "Flat" Architecture

The x86 alwasy enable segmentation, but the effect can be achieved by loading the segment registers with selectors for descriptors that encompass the entire 32-bit linear address space. The 32-bit offset is the same as the linear address.