//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt.rg_start >= rg_elmt.rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt.rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = &rg_elmt;

  return 0;
}

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *not related with vmaid in vm_rg_struct
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma= mm->mmap;

  if(mm->mmap == NULL)
    return NULL;

  int vmait = 0;
  
  while (vmait < vmaid)
  {
    if(pvma == NULL)
	  return NULL;

    vmait++;
    pvma = pvma->vm_next;
  }

  return pvma;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/* 
 * __alloc - allocate a region memory
 * @caller: caller
 * @vmaid: ID vm area to alloc memory region
 * @rgid: memory region ID (used to identify variable in symbol table)
 * @size: allocated size 
 * @alloc_addr: address of allocated memory region
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
    /* Allocate at the toproof */
    struct vm_rg_struct rgnode;

    /* Commit the vmaid */
    rgnode.vmaid = vmaid;

    /* Try to find a free region in the free region list */
    if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0) {
        /* Update the symbol table with the allocated region */
        caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
        caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
        caller->mm->symrgtbl[rgid].vmaid = rgnode.vmaid;

        /* Return the starting address of the allocated region */
        *alloc_addr = rgnode.rg_start;

        return 0; // Allocation successful
    }

    /* Handle case where get_free_vmrg_area fails (allocate by extending VMA limit) */

    /* Retrieve the target VMA */
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    if (!cur_vma) {
        printf("Error: VMA with vmaid=%d not found.\n", vmaid);
        return -1; // Failure
    }

    /* Calculate the size to increment (aligned to page size) */
    int inc_sz = PAGING_PAGE_ALIGNSZ(size);
    int inc_limit_ret;

    /* Attempt to increase the VMA limit to allocate the required memory */
    if (inc_vma_limit(caller, vmaid, inc_sz, &inc_limit_ret) != 0) {
        printf("Error: Failed to increase VMA limit for vmaid=%d.\n", vmaid);
        return -1; // Failure
    }

    /* After increasing the limit, allocate the new region */
    rgnode.rg_start = cur_vma->sbrk;
    rgnode.rg_end = rgnode.rg_start + size;
    rgnode.vmaid = vmaid;

    /* Update the symbol table */
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
    caller->mm->symrgtbl[rgid].vmaid = rgnode.vmaid;

    /* Commit the allocation address */
    *alloc_addr = rgnode.rg_start;

    /* Update the VMA's sbrk pointer */
    cur_vma->sbrk = rgnode.rg_end;

    return 0; // Allocation successful
}


/*
 * __free - remove a region memory
 * @caller: caller
 * @vmaid: ID vm area to alloc memory region
 * @rgid: memory region ID (used to identify variable in symbol table)
 */
int __free(struct pcb_t *caller, int rgid)
{
    struct vm_rg_struct rgnode;

    // Check if rgid is within valid bounds
    if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
        return -1;

    // Retrieve the region from the symbol table
    struct vm_rg_struct *symrg = &caller->mm->symrgtbl[rgid];

    // Ensure the region exists
    if (symrg->rg_start == 0 && symrg->rg_end == 0)
        return -1;

    // Populate rgnode with the details of the region to be freed
    rgnode.rg_start = symrg->rg_start;
    rgnode.rg_end = symrg->rg_end;

    // Reset the symbol table entry for this region
    symrg->rg_start = 0;
    symrg->rg_end = 0;
    symrg->vmaid = -1;

    // Enlist the obsoleted memory region to the free region list
    if (enlist_vm_freerg_list(caller->mm, rgnode) < 0)
        return -1;

    return 0;
}


/*pgalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  int addr;

  /* By default using vmaid = 0 */
  return __alloc(proc, 0, reg_index, size, &addr);
}

/*pgmalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify vaiable in symbole table)
 */
int pgmalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  int addr;

  /* By default using vmaid = 1 */
  return __alloc(proc, 1, reg_index, size, &addr);
}

/*pgfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int pgfree_data(struct pcb_t *proc, uint32_t reg_index)
{
   return __free(proc, reg_index);
}

/*
 * pg_getpage - Get the page in RAM
 * @mm: memory region
 * @pgn: Page Number (PGN)
 * @fpn: Frame Page Number (FPN) to be returned
 * @caller: Caller process control block
 *
 * Return: 0 on success, -1 on failure
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
    uint32_t pte = mm->pgd[pgn]; // Get the page table entry for the given page

    // Check if the page is present in RAM
    if (!PAGING_PTE_PAGE_PRESENT(pte))
    {
        /* Page is not in RAM, swapping is required */
        int vicpgn, swpfpn;
        uint32_t vicpte;

        // Target frame number in swap space
        int tgtfpn = PAGING_PTE_SWP(pte);

        /* Find a victim page to evict from RAM */
        if (find_victim_page(mm, &vicpgn) < 0)
            return -1; // No victim page found, swapping fails

        // Get the victim page's PTE
        vicpte = mm->pgd[vicpgn];

        /* Get a free frame in the swap space */
        if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) < 0)
            return -1; // No free swap space available

        /* Swap out the victim page to swap space */
        if (__swap_cp_page(caller->mram, vicpgn, caller->active_mswp, swpfpn) < 0)
            return -1;

        /* Update the victim page's PTE to reflect its new location in swap */
        pte_set_swap(&vicpte, 0 /* swap type */, swpfpn);
        mm->pgd[vicpgn] = vicpte; // Update the page table

        /* Swap in the target page from swap space to RAM */
        if (__swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, vicpgn) < 0)
            return -1;

        /* Update the page table entry for the target page */
        pte_set_fpn(&pte, vicpgn);      // Set the frame page number
        pte_set_present(&pte);         // Mark the page as present
        mm->pgd[pgn] = pte;            // Update the page table

        /* Add the target page to the FIFO queue for page management */
        enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
    }

    /* Extract the frame number (FPN) from the updated PTE */
    *fpn = PAGING_PTE_FPN(pte);

    return 0;
}


/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess 
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if(pg_getpage(mm, pgn, &fpn, caller) != 0) 
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  MEMPHY_read(caller->mram,phyaddr, data);

  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess 
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if(pg_getpage(mm, pgn, &fpn, caller) != 0) 
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  MEMPHY_write(caller->mram,phyaddr, value);

   return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region 
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
int __read(struct pcb_t *caller, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  int vmaid = currg->vmaid;

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if(currg == NULL || cur_vma == NULL) /* Invalid memory identify */
	  return -1;

  pg_getval(caller->mm, currg->rg_start + offset, data, caller);

  return 0;
}


/*pgwrite - PAGING-based read a region memory */
int pgread(
		struct pcb_t * proc, // Process executing the instruction
		uint32_t source, // Index of source register
		uint32_t offset, // Source address = [source] + [offset]
		uint32_t destination) 
{
  BYTE data;
  int val = __read(proc, source, offset, &data);

  destination = (uint32_t) data;
#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region 
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
int __write(struct pcb_t *caller, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  int vmaid = currg->vmaid;

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  
  if(currg == NULL || cur_vma == NULL) /* Invalid memory identify */
	  return -1;

  pg_setval(caller->mm, currg->rg_start + offset, value, caller);

  return 0;
}

/*pgwrite - PAGING-based write a region memory */
int pgwrite(
		struct pcb_t * proc, // Process executing the instruction
		BYTE data, // Data to be wrttien into memory
		uint32_t destination, // Index of destination register
		uint32_t offset)
{
#ifdef IODUMP
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return __write(proc, destination, offset, data);
}


/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;


  for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte= caller->mm->pgd[pagenum];

    if (!PAGING_PTE_PAGE_PRESENT(pte))
    {
      fpn = PAGING_PTE_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    } else {
      fpn = PAGING_PTE_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);    
    }
  }

  return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: requested size in bytes
 *@alignedsz: aligned size for memory allocation
 *
 */
struct vm_rg_struct* get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));

    if (!newrg || !cur_vma) {
        return NULL; // Handle allocation failure or invalid VMA
    }

    /* Determine the starting and ending address for the new region */
    newrg->rg_start = cur_vma->sbrk;
    newrg->rg_end = newrg->rg_start + alignedsz - 1;

    /* Update the `sbrk` of the current VMA to reflect the allocation */
    cur_vma->sbrk = newrg->rg_end + 1;

    /* Set the VMA ID for this region */
    newrg->vmaid = vmaid;

    return newrg;
}


/*
 * validate_overlap_vm_area
 * @caller: caller
 * @vmaid: ID vm area to alloc memory region
 * @vmastart: start address of the planned memory area
 * @vmaend: end address of the planned memory area
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
    // Get the VMA list for the process
    struct vm_area_struct *vma = caller->mm->mmap;

    // Traverse the VMA list
    while (vma != NULL) {
        if (vma->vm_id == vmaid) {
            // Check if the planned range overlaps with this VMA's existing regions
            struct vm_rg_struct *rg = vma->vm_freerg_list;
            while (rg != NULL) {
                if (!(vmaend <= rg->rg_start || vmastart >= rg->rg_end)) {
                    // Overlap detected
                    return -1;
                }
                rg = rg->rg_next;
            }

            // Check if the planned range overlaps with the VMA's main range
            if (!(vmaend <= vma->vm_start || vmastart >= vma->vm_end)) {
                // Overlap detected with the VMA's allocated area
                return -1;
            }
        }
        // Move to the next VMA in the list
        vma = vma->vm_next;
    }

    // No overlap found
    return 0;
}


/*
 * inc_vma_limit - increase vm area limits to reserve space for new variable
 * @caller: caller
 * @vmaid: ID vm area to alloc memory region
 * @inc_sz: increment size
 * @inc_limit_ret: increment limit return
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz, int* inc_limit_ret)
{
    struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
    int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);  // Align increment size to page size
    int incnumpage = inc_amt / PAGING_PAGESZ;  // Number of pages needed
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    if (!cur_vma) {
        printf("Error: VMA with vmaid=%d not found.\n", vmaid);
        free(newrg);
        return -1;  // Failure
    }

    int old_end = cur_vma->vm_end;  // Store the old end limit

    // Calculate new region range
    newrg->rg_start = cur_vma->vm_end;
    newrg->rg_end = cur_vma->vm_end + inc_amt;

    // Validate overlap of the obtained region
    if (validate_overlap_vm_area(caller, vmaid, newrg->rg_start, newrg->rg_end) < 0) {
        printf("Error: Overlap detected while extending VMA limit for vmaid=%d.\n", vmaid);
        free(newrg);
        return -1;  // Overlap and allocation failed
    }

    // Update VMA's limit to reflect the new region
    cur_vma->vm_end = newrg->rg_end;
    *inc_limit_ret = inc_amt;

    // Map the new memory region to MEMRAM
    if (vm_map_ram(caller, newrg->rg_start, newrg->rg_end, old_end, incnumpage, newrg) < 0) {
        printf("Error: Failed to map memory region to MEMRAM for vmaid=%d.\n", vmaid);
        free(newrg);
        return -1;  // Memory mapping failed
    }

    // Add the newly created region to the VMA's free region list
    enlist_vm_rg_node(&cur_vma->vm_freerg_list, newrg);

    return 0;  // Success
}


/*
 * find_victim_page - find a victim page to evict
 * @mm: memory management structure of the process
 * @retpgn: pointer to store the page number of the victim page
 *
 * Return: 0 on success, -1 if no victim page is found
 */
int find_victim_page(struct mm_struct *mm, int *retpgn) 
{
    if (mm->fifo_pgn == NULL) {
        // No pages in the FIFO queue to evict
        return -1;
    }

    // The first page in the FIFO queue is the victim page
    struct pgn_t *victim = mm->fifo_pgn;

    *retpgn = victim->pgn;  // Retrieve the victim page number

    // Remove the victim page from the FIFO queue
    mm->fifo_pgn = victim->pg_next;

    // Free the victim page node
    free(victim);

    return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size 
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

  if (rgit == NULL)
    return -1;

  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;

  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL && rgit->vmaid == vmaid)
  {
    if (rgit->rg_start + size <= rgit->rg_end)
    { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      /* Update left space in chosen region */
      if (rgit->rg_start + size < rgit->rg_end)
      {
        rgit->rg_start = rgit->rg_start + size;
      }
      else
      { /*Use up all space, remove current node */
        /*Clone next rg node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        /*Cloning */
        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;

          rgit->rg_next = nextrg->rg_next;

          free(nextrg);
        }
        else
        { /*End of free list */
          rgit->rg_start = rgit->rg_end;	//dummy, size 0 region
          rgit->rg_next = NULL;
        }
      }
    }
    else
    {
      rgit = rgit->rg_next;	// Traverse next rg
    }
  }

 if(newrg->rg_start == -1) // new region not found
   return -1;

 return 0;
}

//#endif
