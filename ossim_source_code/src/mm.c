//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* 
 * init_pte - Initialize PTE entry
 */
int init_pte(uint32_t *pte,
             int pre,    // present
             int fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             int swpoff) //swap offset
{
  if (pre != 0) {
    if (swp == 0) { // Non swap ~ page online
      if (fpn == 0) 
        return -1; // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT); 
    } else { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT); 
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;   
}

/* 
 * pte_set_present - Mark the page as present in the page table entry
 * @pte: Pointer to the page table entry (PTE) to modify
 *
 * This function marks the page as present in memory by setting the present bit
 * and clearing the swapped bit in the PTE. It also resets swap-related information.
 */
int pte_set_present(uint32_t *pte) 
{
    // Set the PRESENT bit (bit 31) in the PTE to indicate the page is in memory
    PAGING_PTE_SET_PRESENT(*pte);

    // Clear the SWAPPED bit (bit 30) since the page is now present in memory
    CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

    // Reset the swap type (bits 0-4) and swap offset (bits 5-25) because the page is no longer swapped
    SETVAL(*pte, 0, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
    SETVAL(*pte, 0, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

    // Optionally reset other bits like the Dirty bit if required
    // This would depend on our specific use case and how dirty pages are managed

    return 0;
}


/* 
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(uint32_t *pte, int swptyp, int swpoff)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

/* 
 * pte_set_fpn - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(uint32_t *pte, int fpn)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT); 

  return 0;
}


/* 
 * vmap_page_range - map a range of page at aligned address
 */
int vmap_page_range(struct pcb_t *caller, // process call
                                int addr, // start address which is aligned to pagesz
                               int pgnum, // num of mapping page
           struct framephy_struct *frames,// list of the mapped frames
              struct vm_rg_struct *ret_rg)// return mapped region, the real mapped fp
{                                         // no guarantee all given pages are mapped
    struct framephy_struct *fpit = frames; // Iterator for frames
    int pgit = 0;                          // Page index iterator
    int pgn = PAGING_PGN(addr);            // Extract page number from address

    // Step 1: Update the start and end of the returned region (ret_rg)
    ret_rg->rg_start = addr;        // Start address of the mapped region
    ret_rg->rg_end = addr + (pgnum * PAGING_PAGESZ); // End address of the mapped region
    ret_rg->rg_next = NULL;

    // ret_rg->vmaid = caller->mm->mmap->vm_freerg_list->vmaid;// Associate with active virtual memory area ID

    // Step 2: Map the range of frames to the address space in the page table
    while (fpit != NULL && pgit < pgnum) {
        uint32_t pte = 0; // Page Table Entry

        // Set the Frame page Number (FPN) in the page table
        SETVAL(pte, fpit->fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

        // Mark the page as present
        SETBIT(pte, PAGING_PTE_PRESENT_MASK);

        // Update the page table at the appropriate index
        caller->mm->pgd[pgn + pgit] = pte;

        // Move to the next frame and increment page index
        fpit = fpit->fp_next;
        pgit++;
    }

    // Step 3: Track the mapped pages for later page replacement activities
    for (int i = 0; i < pgit; i++) {
        enlist_pgn_node(&caller->mm->fifo_pgn, pgn + i);
    }

    // Return success (0) or failure (-1 if not all pages were mapped)
    return (pgit == pgnum) ? 0 : -3000;
}

/* 
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

int alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{
    int pgit;  // Iterates over the requested number of pages
    int fpn;   // Frame Page Number for the allocated frame

    struct framephy_struct *newfp = NULL;  // New frame to be allocated
    struct framephy_struct *tail = NULL;  // Tail of the allocated frame list

    *frm_lst = NULL;  // Initialize the list as empty

    for (pgit = 0; pgit < req_pgnum; pgit++) {
        // Attempt to get a free frame page number from RAM
        if (MEMPHY_get_freefp(caller->mram, &fpn) == 0) {
            // Successfully obtained a free frame, allocate a new frame structure
            newfp = (struct framephy_struct *)malloc(sizeof(struct framephy_struct));
            if (!newfp) {
                // Memory allocation failed; clean up and return error
                printf("Error: Memory allocation for framephy_struct failed.\n");
                return -3000;
            }

            // Initialize the frame structure
            newfp->fpn = fpn;
            newfp->fp_next = NULL;  // Ensure it's the end of the list
            newfp->owner = caller->mm;  // Assign ownership to the caller process

            // Append the new frame to the frame list
            if (*frm_lst == NULL) {
                // First frame in the list
                *frm_lst = newfp;
                tail = newfp;
            } else {
                // Append to the end of the existing list
                tail->fp_next = newfp;
                tail = newfp;
            }
        } else {
            // Failed to obtain enough frames, clean up allocated frames and return error
            printf("Error: Not enough free frames in RAM for allocation.\n");
            struct framephy_struct *current = *frm_lst;
            while (current != NULL) {
                struct framephy_struct *next = current->fp_next;
                free(current);
                current = next;
            }
            *frm_lst = NULL;  // Reset the list to empty
            return -3000;
        }
    }

    // Successfully allocated all requested frames
    return 0;
}



/* 
 * vm_map_ram - do the mapping all vm area to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
int vm_map_ram(struct pcb_t *caller, int astart, int aend, int mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  struct framephy_struct *frm_lst = NULL;
  int ret_alloc;

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide 
   *duplicate control mechanism, keep it simple
   */
  ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);

  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;

  /* Out of memory */
  if (ret_alloc == -3000) 
  {
#ifdef MMDBG
     printf("OOM: vm_map_ram out of memory \n");
#endif
     return -1;
  }

  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */
  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

  return 0;
}

/* Swap copy content page from source frame to destination frame 
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, int srcfpn,
                struct memphy_struct *mpdst, int dstfpn) 
{
  int cellidx;
  int addrsrc,addrdst;
  for(cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

/*
 * Initialize an empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller) {
    // Allocate and initialize the common vm_area_struct
    struct vm_area_struct *vma = malloc(sizeof(struct vm_area_struct));
    vma->vm_id = 0;                 // Unified VMA ID
    vma->vm_start = 0;              // Start of the entire VMA
    vma->vm_end = caller->vmemsz;   // End of the entire VMA
    vma->sbrk = 0;                  // Initially points to the start of the data segment
    vma->vm_mm = mm;                // Link to mm struct
    vma->vm_next = NULL;

    // Initialize the free region list (data and heap regions)
    struct vm_rg_struct *data_rg = malloc(sizeof(struct vm_rg_struct));
    struct vm_rg_struct *heap_rg = malloc(sizeof(struct vm_rg_struct));

    // Set up the data region (grows upward)
    data_rg->vmaid = 0;                     // Data region ID
    data_rg->rg_start = 0;                  // Starts at 0
    data_rg->rg_end = 0;                    // Initially empty
    data_rg->rg_next = NULL;                // No next region
    enlist_vm_rg_node(&vma->vm_freerg_list, data_rg); // Add to free region list

    // Set up the heap region (grows downward)
    heap_rg->vmaid = 1;                     // Heap region ID
    heap_rg->rg_start = caller->vmemsz;     // Starts at the maximum memory address
    heap_rg->rg_end = caller->vmemsz;       // Initially empty
    heap_rg->rg_next = NULL;                // No next region
    enlist_vm_rg_node(&vma->vm_freerg_list, heap_rg); // Add to free region list

    // Link the VMA to the mm_struct
    mm->mmap = vma;

    // Initialize the page directory and symbol region table
    mm->pgd = malloc(PAGING_MAX_PGN * sizeof(uint32_t));
    memset(mm->pgd, 0, PAGING_MAX_PGN * sizeof(uint32_t));
    memset(mm->symrgtbl, 0, sizeof(mm->symrgtbl));

    return 0;
}



struct vm_rg_struct* init_vm_rg(int rg_start, int rg_end, int vmaid)
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->vmaid = vmaid;
  rgnode->rg_next = NULL;

  return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct* rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, int pgn)
{
  struct pgn_t* pnode = malloc(sizeof(struct pgn_t));

  pnode->pgn = pgn;
  pnode->pg_next = *plist;
  *plist = pnode;

  return 0;
}

int print_list_fp(struct framephy_struct *ifp)
{
   struct framephy_struct *fp = ifp;
 
   printf("print_list_fp: ");
   if (fp == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (fp != NULL )
   {
       printf("fp[%d]\n",fp->fpn);
       fp = fp->fp_next;
   }
   printf("\n");
   return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
   struct vm_rg_struct *rg = irg;
 
   printf("print_list_rg: ");
   if (rg == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (rg != NULL)
   {
       printf("rg[%ld->%ld<at>vma=%d]\n",rg->rg_start, rg->rg_end, rg->vmaid);
       rg = rg->rg_next;
   }
   printf("\n");
   return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
   struct vm_area_struct *vma = ivma;
 
   printf("print_list_vma: ");
   if (vma == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (vma != NULL )
   {
       printf("va[%ld->%ld]\n",vma->vm_start, vma->vm_end);
       vma = vma->vm_next;
   }
   printf("\n");
   return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
   printf("print_list_pgn: ");
   if (ip == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (ip != NULL )
   {
       printf("va[%d]-\n",ip->pgn);
       ip = ip->pg_next;
   }
   printf("n");
   return 0;
}

int print_pgtbl(struct pcb_t *caller, uint32_t start, uint32_t end)
{
  int pgn_start,pgn_end;
  int pgit;

  if(end == -1){
    pgn_start = 0;
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, 0);
    end = cur_vma->vm_end;
  }
  pgn_start = PAGING_PGN(start);
  pgn_end = PAGING_PGN(end);

  printf("print_pgtbl: %d - %d", start, end);
  if (caller == NULL) {printf("NULL caller\n"); return -1;}
    printf("\n");


  for(pgit = pgn_start; pgit < pgn_end; pgit++)
  {
     printf("%08lld: %08x\n", pgit * sizeof(uint32_t), caller->mm->pgd[pgit]);
  }

  return 0;
}

//#endif
