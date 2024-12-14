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
      if (fpn < 0) 
        return -1; // Invalid setting

      /* Valid setting with FPN */
      SETVAL(*pte, 0,0xFFFFFFFF,0);
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
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
              struct vm_rg_struct *ret_rg,// return mapped region, the real mapped fp
                                int vmaid,
                      unsigned long astart,
                      unsigned long aend)
{                                         // no guarantee all given pages are mapped
    /*
    cập nhật cho mỗi rg_end rg_start bằng addr bắt đầu,
    với mỗi page được thêm vào thì dời dời end lên
    */
    struct framephy_struct *fpit = malloc(sizeof(struct framephy_struct));
    int fpn;
    int incr_descr;
    int pgit = 0;
    int pgn = PAGING_PGN(addr); /* get the pos of next pte */
    uint32_t *pte = malloc(sizeof(uint32_t));
    if (!pte) {
        printf("Can't malloc pte");
    }
    /* TODO: update the rg_end and rg_start of ret_rg */
    ret_rg->rg_start = addr;
    ret_rg->vmaid = vmaid;
    fpit->fp_next = frames;

    /* TODO map range of frame to address space
    *      in page table pgd in caller->mm
    */
    if (astart >= aend) {
        incr_descr = -1;
    }
    else {
        incr_descr = 1;
    }
    ret_rg->rg_end = addr + incr_descr * PAGING_PAGESZ * pgnum;
    for (pgit = 0; pgit < pgnum; pgit++) {
        if (!fpit) {
            printf("NO frame in %d\n", pgit);
        }
        struct framephy_struct *temp = fpit;
        fpit = fpit->fp_next;
        fpn = fpit->fpn;
        /* use this because the when fpit is the last the next is null
          it can made some problem 
        */
        if (init_pte(pte, 1, fpn, 0, 0, 0, 0) != 0) {
            printf("init_pte failed\n");
        }
        caller->mm->pgd[pgn + incr_descr * pgit] = *pte;
        enlist_pgn_node(&caller->mm->fifo_pgn, pgn + pgit);
        if (temp) {
            free(temp); /* delete the frame */
        }
    }
    /* Tracking for later page replacement activities (if needed)
    * Enqueue new usage page */

    /* frame has been acllocated in the RAM */
    free(pte);
    return 0;
}

/* 
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

int alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{
    /* xin địa chỉ trong ram của cho các frm nhớ
     nếu địa chỉ trống không đủ thì swap ra ngoài
    */
    int pgit, fpn;
    struct framephy_struct *newfp_str;
    struct mm_struct *mm = caller->mm;
    if (!mm)
    {
        printf(" mm failed");
    }
    /* TODO: allocate the page */
    for (pgit = 0; pgit < req_pgnum; pgit++) {
        if (MEMPHY_get_freefp(caller->mram, &fpn) == 0) {
            newfp_str = malloc(sizeof(struct framephy_struct));
            newfp_str->fpn = fpn;
            newfp_str->owner = mm;
            newfp_str->fp_next = NULL;

            /* If frm_lst is empty, the new frame becomes the head */
            if (*frm_lst == NULL) {
                *frm_lst = newfp_str;
            }
            else {
                /* Otherwise, traverse to the last element and 
                  add the new frame to the tail 
                */
                struct framephy_struct *temp = *frm_lst;
                while (temp->fp_next != NULL) {
                    temp = temp->fp_next;
                }
                temp->fp_next = newfp_str;
            }

            /* add fram into used frame list if we need we can use it,
              maybe it is unused in this assignment 
            */
            struct framephy_struct *new_used_ls = malloc(sizeof(struct framephy_struct));
            new_used_ls->fpn = fpn;
            new_used_ls->owner = mm;
            new_used_ls->fp_next = caller->mram->used_fp_list;
            caller->mram->used_fp_list = new_used_ls;
        }
        else { /* ERROR CODE of obtaining somes but not enough frames */
            if (MEMPHY_get_freefp(caller->active_mswp, &fpn) == 0) {
                /* nếu không tìm được chỗ trống trong mram tìm page phải 
                * giải phóng một frame trong page và đổi chỗ với **mswp
                *  tìm chỗ trống trong active_swap nếu có thì hoán đổi ô nhớ trống.
                *  tìm pte của ô nhớ trong ram cần được thay
                * thay pte trỏ tới vùng swap
                * cập nhật frm_lst với fpn là fpn của ram
                * Detail change the data of the victime_page->fpn to the swap 
                * and add new data to this
                */
                int victim_page;
                int no_fpn_sw = fpn;

                if (find_victim_page(mm, &victim_page) < 0)
                {
                    printf("can't find victim page\n");
                }
                /* change the pte of victim_page to swap */
                /* find pte from victim page */
                uint32_t *pte = malloc(sizeof(uint32_t));
                *pte = mm->pgd[victim_page];
                int no_fpn_ram = PAGING_PTE_FPN(*pte);
                /* get fpn in ram to change the data with swap
                  after have the fpn in ram we  need to change the pte
                  in swaptype= 1 => swap in swap 1 | in acitve swap
                */
                if (init_pte(pte, 1, -1, 0, 1, 1, no_fpn_sw) != 0)
                {
                    printf("can't change the pte from ram mode to swap\n");
                }
                __swap_cp_page(caller->mram, no_fpn_ram, caller->active_mswp, no_fpn_sw);
                mm->pgd[victim_page] = *pte;

                /* swap the content of no_fpn_ram to no_fpn_swap */
                /* create the framestruct again with the fpn=no_fpn_ram */
                newfp_str = malloc(sizeof(struct framephy_struct));
                newfp_str->fpn = fpn;
                newfp_str->owner = mm;
                newfp_str->fp_next = NULL;

                /* If frm_lst is empty, the new frame becomes the head */
                if (*frm_lst == NULL) {
                    *frm_lst = newfp_str;
                }
                else {
                    /* Otherwise, traverse to the last element and 
                      add the new frame to the tail
                    */
                    struct framephy_struct *temp = *frm_lst;
                    while (temp->fp_next != NULL) {
                        temp = temp->fp_next;
                    }
                    temp->fp_next = newfp_str;
                }
                /* add a new list */
                struct framephy_struct *new_used_ls = malloc(sizeof(struct framephy_struct));
                new_used_ls->fpn = fpn;
                new_used_ls->owner = mm;
                new_used_ls->fp_next = caller->active_mswp->used_fp_list;
                caller->active_mswp->used_fp_list = new_used_ls;

                if (pte) {
                    free(pte);
                }
            }
            else {
                if (MEMPHY_get_freefp(caller->mram, &fpn) < 0 &&
                    MEMPHY_get_freefp(caller->active_mswp, &fpn) < 0)
                  return -3000;
                else
                  return -1;
                /* return because there is no empty space in */
                /* can get freefp in the **swmem but in this assignment 
                  we only use one swap so just use active_mswp
                */
            }
        }
    }
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
int vm_map_ram(struct pcb_t *caller, unsigned long astart, unsigned long aend, int mapstart, int incpgnum, struct vm_rg_struct *ret_rg, int vmaid)
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
    if (ret_alloc == -3000) {
#ifdef MMDBG
      printf("OOM: vm_map_ram out of memory \n");
#endif
      return -1;
    }

    /* it leaves the case of memory is enough but half in ram, half in swap
    * do the swaping all to swapper to get the all in ram */
    vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg, vmaid, astart, aend);

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
    struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));
    struct vm_area_struct *vma1 = malloc(sizeof(struct vm_area_struct));
    mm->pgd = malloc(PAGING_MAX_PGN * sizeof(uint32_t));
    /* By default the owner comes with at least one vma for DATA */

#ifdef MM_PAGING_HEAP_GODOWN
    mm->mmap = vma0;
    vma0->vm_id = 0;
    vma0->vm_start = 0;
    vma0->vm_end = vma0->vm_start;
    vma0->sbrk = vma0->vm_start;
    vma0->vm_freerg_list = NULL;

    /* TODO update VMA0 next */
    vma0->vm_next = vma1;

    /* TODO: update one vma for HEAP */
    vma1->vm_id = 1;
    vma1->vm_start = caller->vmemsz;
    vma1->vm_end = vma1->vm_start;
    vma1->sbrk = vma1->vm_start;
    vma1->vm_freerg_list = NULL;
    vma1->vm_next = NULL;

    /* Point vma owner backward */
    vma0->vm_mm = mm;
    vma1->vm_mm = mm;

    /* TODO: update mmap */
    mm->mmap = vma0;

#else /* not MM_PAGING_HEAP_GODOWN */
    mm->mmap = vma0;
    vma0->vm_id = 0;
    vma0->vm_start = 0;
    vma0->vm_end = vma0->vm_start;
    vma0->sbrk = vma0->vm_start;
    vma0->vm_freerg_list = NULL;

    /* TODO update VMA0 next */
    vma0->vm_next = vma1;

    /* TODO: update one vma for HEAP */
    vma1->vm_id = 1;
    vma1->vm_start = caller->vmemsz/2; 
    vma1->vm_end = vma1->vm_start;
    vma1->sbrk = vma1->vm_start;
    vma1->vm_freerg_list = NULL;
    vma1->vm_next = NULL;
    /* Point vma owner backward */
    vma0->vm_mm = mm;
    vma1->vm_mm = mm;
    /* TODO: update mmap */
    mm->mmap = vma0;
#endif /* MM_PAGING_HEAP_GODOWN */
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
    end = -1;
    if (end == -1)
    {
        start = caller->vmemsz;
        struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, 1);
        end = cur_vma->vm_end;
    }
    pgn_start = PAGING_PGN(start);
    pgn_end = PAGING_PGN(end);
    printf("print_pgtbl HEAP: %d - %d\n", start, end);
    if (caller == NULL)
    {
        return -1;
    }

    for (pgit = pgn_start; pgit > pgn_end; pgit--)
    {
        printf("%08lld: %08x\n", pgit * sizeof(uint32_t), caller->mm->pgd[pgit]);
    }
    return 0;
}

//#endif
