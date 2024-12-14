//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>


int count_free_rg(struct pcb_t *caller, int vmaid) {
    if (caller == NULL)
        return 0;
    int count = 0;
    struct vm_area_struct * vma = get_vma_by_num(caller->mm, vmaid);
    struct vm_rg_struct * head = vma->vm_freerg_list;
    printf("Free region list in vmaid %d: ", vmaid);
    if (!head)
        printf("NULL");
    else {
        while (head != NULL) {
            count++;
            if (!head->rg_next)
                printf("[%ld, %ld]->NULL", head->rg_start, head->rg_end);
            else
                printf("[%ld, %ld]->", head->rg_start, head->rg_end);
            head = head->rg_next;
        }
    }
    printf("\n");
    return count;
}
/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct rg_elmt)
{
  // struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  // if (rg_elmt.rg_start >= rg_elmt.rg_end)
  //   return -1;

  // if (rg_node != NULL)
  //   rg_elmt.rg_next = rg_node;

  // /* Enlist the new region */
  // mm->mmap->vm_freerg_list = &rg_elmt;
    struct vm_rg_struct *rg_node;
    if (rg_elmt.vmaid == 0)
        rg_node = mm->mmap->vm_freerg_list;
    else
        rg_node = mm->mmap->vm_next->vm_freerg_list;
    if ((rg_elmt.rg_start >= rg_elmt.rg_end && rg_elmt.vmaid == 0) 
        || (rg_elmt.rg_end >= rg_elmt.rg_start && rg_elmt.vmaid == 1))
        return -1;

    struct vm_rg_struct *new_rg = malloc(sizeof(struct vm_rg_struct));

    if (new_rg == NULL) {
        return -1;
    }

    new_rg->vmaid = rg_elmt.vmaid;
    new_rg->rg_start = rg_elmt.rg_start;
    new_rg->rg_end = rg_elmt.rg_end;
    new_rg->rg_next = NULL;  /* New region will be the last element */

    if (rg_node == NULL) {
        if (rg_elmt.vmaid == 0)
            mm->mmap->vm_freerg_list = new_rg;
        else
            mm->mmap->vm_next->vm_freerg_list = new_rg;
    } else {
        /* Traverse to the last element of the list */
        struct vm_rg_struct *last_rg = rg_node;
        while (last_rg->rg_next != NULL) {
            last_rg = last_rg->rg_next;
        }
        last_rg->rg_next = new_rg;
    }
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
    if (caller->mm->symrgtbl[rgid].rg_start != caller->mm->symrgtbl[rgid].rg_end)
        return -2;
    if (caller->mm->symrgtbl[rgid].rg_start == -1)
        return -2;

    /* Try to find a free region in the free region list */
    if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0) {

        /* Update the symbol table */
        caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
        caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
        caller->mm->symrgtbl[rgid].vmaid = rgnode.vmaid;

        /* Commit the allocation address */
        *alloc_addr = rgnode.rg_start;
#ifdef PAGETBL_DUMP
        print_pgtbl(caller, 0, -1); /* print max TBL */
#endif /* PAGETBL_DUMP */
        return 0;
    }
    /* TODO: get_free_vmrg_area FAILED handle the region management (Fig.6) */
    /* TODO retrive current vma if needed, current comment out due to compiler redundant warning*/
    /* Attempt to increate limit to get space */

    int inc_limit_ret;

    /* TODO INCREASE THE LIMIT
   * inc_vma_limit(caller, vmaid, inc_sz)
   */
    if (inc_vma_limit(caller, vmaid, size, &inc_limit_ret) == -1) {
        return -1;
    }
  /* TODO: commit the limit increment */
  // printf("free region list after alloc: %d\n", count_free_rg(caller, vmaid));
    get_free_vmrg_area(caller, vmaid, size, &rgnode);

    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
    caller->mm->symrgtbl[rgid].vmaid = rgnode.vmaid;
    printf("Get region in alloc rgid %d vmaid: %d, rg start: %ld, rg end: %ld\n", rgid, caller->mm->symrgtbl[rgid].vmaid, caller->mm->symrgtbl[rgid].rg_start, caller->mm->symrgtbl[rgid].rg_end);
#ifdef PAGETBL_DUMP
    print_pgtbl(caller, 0, -1); // print max TBL
#endif /* PAGETBL_DUMP */
    /* TODO: commit the allocation address */
    *alloc_addr = rgnode.rg_start; 
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
    if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
        return -1;

    if (get_symrg_byid(caller->mm, rgid) == NULL)
        return -1;

    rgnode = *get_symrg_byid(caller->mm, rgid);

    if (rgnode.rg_start == rgnode.rg_end)
        return -1;

    /* enlist the obsoleted memory region */
    printf("Put free rg calling from __free() vmaid %d: rg start: %ld, rg end: %ld\n", rgnode.vmaid, rgnode.rg_start, rgnode.rg_end);
    enlist_vm_freerg_list(caller->mm, rgnode);
    caller->mm->symrgtbl[rgid].rg_start = -1;
    caller->mm->symrgtbl[rgid].rg_end = -1;
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
        int vicpgn, swpfpn, vicfpn;
        uint32_t vicpte;

        // Target frame number in swap space
        int tgtfpn = PAGING_PTE_SWP(pte);

        /* Find a victim page to evict from RAM */
        find_victim_page(caller->mm, &vicpgn);
        if (vicpgn == -1)
            return -1;

        vicpte = mm->pgd[vicpgn];
        vicfpn = PAGING_PTE_PGN(vicpte);

        /* Get free frame in MEMSWP */
        MEMPHY_get_freefp(caller->active_mswp, &swpfpn);

        if (swpfpn == -1)
            return -1;

        /* Do swap frame from MEMRAM to MEMSWP and vice versa */
        /* Copy victim frame to swap */
        __swap_cp_page(caller->mram, vicfpn, *(caller->mswp), swpfpn);

        /* Copy target frame from swap to mem */
        __swap_cp_page(*(caller->mswp), tgtfpn, caller->mram, vicfpn);

        /* Update page table */
        pte_set_swap(&mm->pgd[vicpgn], 1, PAGING_OFFST(vicpgn));

        /* Update its online status of the target page */
        pte_set_fpn(&mm->pgd[pgn], tgtfpn);
        enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
    }
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

    MEMPHY_read(caller->mram, phyaddr, data);

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

    MEMPHY_write(caller->mram, phyaddr, value);

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

    if(currg == NULL || cur_vma == NULL || currg->rg_start == currg->rg_end) /* Invalid memory identify */
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
    
    if(currg == NULL || cur_vma == NULL ||  currg->rg_start == currg->rg_end) /* Invalid memory identify */
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

    newrg->vmaid = vmaid;

    if (vmaid) {
        newrg->rg_start = cur_vma->sbrk;
        newrg->rg_end = cur_vma->sbrk - size;
        cur_vma->sbrk -= size;
    }
    else {
        newrg->rg_start = cur_vma->sbrk;
        newrg->rg_end = cur_vma->sbrk + size;
        cur_vma->sbrk += size;
    }
    newrg->rg_next = NULL;
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

    // // Traverse the VMA list
    // while (vma != NULL) {
    //     if (vma->vm_id == vmaid) {
    //         // Check if the planned range overlaps with this VMA's existing regions
    //         struct vm_rg_struct *rg = vma->vm_freerg_list;
    //         while (rg != NULL) {
    //             if (!(vmaend <= rg->rg_start || vmastart >= rg->rg_end)) {
    //                 // Overlap detected
    //                 return -1;
    //             }
    //             rg = rg->rg_next;
    //         }

    //         // Check if the planned range overlaps with the VMA's main range
    //         if (!(vmaend <= vma->vm_start || vmastart >= vma->vm_end)) {
    //             // Overlap detected with the VMA's allocated area
    //             return -1;
    //         }
    //     }
    //     // Move to the next VMA in the list
    //     vma = vma->vm_next;
    // }

    // // No overlap found
    while (vma) {
        if(OVERLAP(vmastart, vmaend, vma->vm_start, vma->vm_end))
            return -1;
        vma = vma->vm_next;
    }
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
    int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
    int incnumpage = inc_amt / PAGING_PAGESZ;
    struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    int old_end = cur_vma->vm_end;
    /*Validate overlap of obtained region */
    int new_start_vma = area->rg_start;

    if (vmaid == 1) {
        if (area->rg_start > old_end)
            new_start_vma = old_end;
    }
    else if (vmaid == 0) {
        if (area->rg_start < old_end)
            new_start_vma = old_end;
    }

    if (validate_overlap_vm_area(caller, vmaid, new_start_vma, area->rg_end) < 0) {
        return -1; /*Overlap and failed allocation */
    }
    /* TODO: Obtain the new vm area based on vmaid */
    inc_amt = PAGING_PAGE_ALIGNSZ(abs(inc_sz - abs(old_end - area->rg_start)));
    incnumpage = inc_amt / PAGING_PAGESZ;

#ifdef MM_PAGING_HEAP_GODOWN
    if (vmaid == 1) {
        if (area->rg_end < cur_vma->vm_end) // Tang kich thuoc vma neu viec cap phat reg hop le
            cur_vma->vm_end -= inc_amt;
    }
    else if (vmaid == 0) {
        if (area->rg_end > cur_vma->vm_end)
            cur_vma->vm_end += inc_amt;
    }
#else /* not MM_PAGING_HEAP_GODOWN */
    if (vmaid == 1)
    {
        if (area->rg_end > cur_vma->vm_end)
            cur_vma->vm_end += inc_amt;
    }
    else if (vmaid == 0) {
        if (area->rg_end > cur_vma->vm_end)
            cur_vma->vm_end += inc_amt;
    }
#endif /* MM_PAGING_HEAP_GODOWN */
    *inc_limit_ret = cur_vma->vm_end;

    if (vm_map_ram(caller, area->rg_start, area->rg_end, old_end, incnumpage, newrg, vmaid) < 0) {
        free(area);
        return -1; /* Map the memory to MEMRAM */
    }

    enlist_vm_freerg_list(caller->mm, *area);
    free(area);
    free(newrg);

    return 0;
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
    struct pgn_t **pg = &(mm->fifo_pgn), *deletePage;
    if (*pg == NULL)
        return -1;

    /* Vì fifo_pgn là danh sách các trang đang được sử dụng, 
    * khi thêm 1 trang sử dụng mới sẽ được thêm vào đầu. 
    * Do đó cần chọn trang đầu tiên được add vào ở cuối hàng.
    */

    while ((*pg)->pg_next != NULL)
        pg = &((*pg)->pg_next);
    *retpgn = (*pg)->pgn;
    deletePage = (*pg);
    *pg = NULL;
    free(deletePage);
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

  /* Probe uninitialized newrg */
  newrg->rg_start = newrg->rg_end = -1;

  if (rgit != NULL)
    /* Traverse on list of free vm region to find a fit space */
    while (rgit != NULL && rgit->vmaid == vmaid)
    {
      if (rgit->vmaid == 0)
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
          { /* Use up all space, remove current node */
            /* Clone next rg node */
            struct vm_rg_struct *nextrg = rgit->rg_next;

            /* Cloning */
            if (nextrg != NULL)
            {
              rgit->rg_start = nextrg->rg_start;
              rgit->rg_end = nextrg->rg_end;
              rgit->rg_next = nextrg->rg_next;

              free(nextrg);
            }
            else
            { /* End of free list */
              rgit->rg_start = rgit->rg_end;
              rgit->rg_next = NULL;
            }
          }
          break;
        }
        else
        {
          rgit = rgit->rg_next; /* Traverse next rg */
        }
      }
      else if (rgit->vmaid == 1)
      {
        if (rgit->rg_start - size >= rgit->rg_end)
        { /* Current region has enough space */
          newrg->rg_start = rgit->rg_start;
          newrg->rg_end = rgit->rg_start - size;

          /* Update left space in chosen region */
          if (rgit->rg_start - size > rgit->rg_end)
          {
            rgit->rg_start = rgit->rg_start - size;
          }
          else
          { /* Use up all space, remove current node */
            /* Clone next rg node */
            struct vm_rg_struct *nextrg = rgit->rg_next;

            /* Cloning */
            if (nextrg != NULL)
            {
              rgit->rg_start = nextrg->rg_start;
              rgit->rg_end = nextrg->rg_end;
              rgit->rg_next = nextrg->rg_next;

              free(nextrg);
            }
            else
            { /* End of free list */
              rgit->rg_start = rgit->rg_end;
              rgit->rg_next = NULL;
            }
          }
          break;
        }
        else
        {
          rgit = rgit->rg_next; /* Traverse next rg */
        }
      }
    }

  if (newrg->rg_start == -1) /* new region not found */
    return -1;

  /* Delete regions with the same start and end */
  rgit = cur_vma->vm_freerg_list;
  struct vm_rg_struct *prev = NULL;

  while (rgit != NULL)
  {
    if (rgit->rg_start == rgit->rg_end)
    {
      struct vm_rg_struct *nextrg = rgit->rg_next;

      if (prev != NULL)
        prev->rg_next = nextrg;
      else
        cur_vma->vm_freerg_list = nextrg;

      free(rgit);
      rgit = nextrg;
    }
    else
    {
      prev = rgit;
      rgit = rgit->rg_next;
    }
  }

  return 0;
}

int helper(struct pcb_t *caller, int rgid) {
  struct vm_rg_struct *rgnode = get_symrg_byid(caller->mm, rgid);
  printf("Region start: %ld, Region end: %ld, Region vmaid: %d\n", rgnode->rg_start, rgnode->rg_end, rgnode->vmaid);

  printf("Number of free region in data: %d\n", count_free_rg(caller, 0));
  printf("Number of free region in heap: %d\n", count_free_rg(caller, 1));
  return 1;
}

int pgaddr(struct pcb_t *proc, uint32_t reg_index) {
  return helper(proc, reg_index);
}

//#endif
