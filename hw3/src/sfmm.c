/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"

#define WORDSIZE 8
#define M 32
#define PUT(p,val) (*((size_t *)(p)) = (val)) //write 8 bytes 
#define GET(p) (*((size_t *)(p)))
#define PACK(i,curr_prev_alloc) ((i) | (curr_prev_alloc))
#define NOTALIGN(p) (((size_t)(p) & (0xf)))
#define ALIGN(p) (((size_t)(p) + 16) & ~(0xf))
#define GETSIZE(p) (GET(p) & BLOCK_SIZE_MASK)
#define GETSIZEFOOTER(p) ((GET(p)^sf_magic()) & BLOCK_SIZE_MASK)
#define GETPREVALOC(p) (GET(p) & PREV_BLOCK_ALLOCATED)
#define GETCURALOC(p) (GET(p) & THIS_BLOCK_ALLOCATED)

#define HDBP(bp) ((char*)(bp) - WORDSIZE)
#define FTBP(bp) ((char*)(bp) + GETSIZE(HDBP(bp)) - 2*WORDSIZE)
#define GET_HEAD_FROM_FOOT(p) ((char *)(p) - GETSIZEFOOTER(p) + WORDSIZE)
sf_prologue * prologue;
sf_epilogue * epilogue;
int initialized=0;

void insert_node_helper(sf_block * p, char * heap_ptr, size_t size){
    sf_block * first_node = p->body.links.next;
    sf_block * insert_this = (sf_block *)(heap_ptr - WORDSIZE);

    //printf("INSERT HERE %p %p %p \n",temp->body.links.next,p->body.links.prev, (void *)heap_ptr);

    insert_this->body.links.next = first_node;
    insert_this->body.links.prev = p;
    first_node->body.links.prev = insert_this;
    p->body.links.next = insert_this;
}

void insert_node(char * heap_ptr){

    size_t size = GETSIZE(heap_ptr);
    debug("-------------------SIZE(in bytes)------------- %lu ",size);
    if(size==M){
        insert_node_helper(&sf_free_list_heads[0],heap_ptr,size);
    }else if(size>M && size<=2*M){
        insert_node_helper(&sf_free_list_heads[1],heap_ptr,size);
    }else if(size>2*M && size<=4*M){
        insert_node_helper(&sf_free_list_heads[2],heap_ptr,size);
    }else if(size>4*M && size<=8*M){
        insert_node_helper(&sf_free_list_heads[3],heap_ptr,size);
    }else if(size>8*M && size<=16*M){
        insert_node_helper(&sf_free_list_heads[4],heap_ptr,size);
    }else if(size>16*M && size<=32*M){
        insert_node_helper(&sf_free_list_heads[5],heap_ptr,size);
    }else if(size>32*M && size<=64*M){
        insert_node_helper(&sf_free_list_heads[6],heap_ptr,size);
    }else if(size>64*M && size<=128*M){
        insert_node_helper(&sf_free_list_heads[7],heap_ptr,size);
    }else if(size>128*M){
        insert_node_helper(&sf_free_list_heads[8],heap_ptr,size);
    }
}
char * remove_node_helper(sf_block * p, size_t needed_size){
    sf_block * first_node = p->body.links.next;
    sf_block * temp = first_node;

    debug("REMOVE_NODE_HELPER size %lu",needed_size);
    while(temp->body.links.next != first_node){
        char * head = (char *) temp + WORDSIZE;
        size_t block_size = GETSIZE(head);
        debug("Node Adress %p",head);
        debug("block_size (in bytes)%lu",GETSIZE(head));
        if(block_size >= needed_size){

            sf_block * prev = temp->body.links.prev;
            sf_block * next = temp->body.links.next;
            prev->body.links.next = next;
            next->body.links.prev = prev; // node successfully deleted
            size_t splinter_size = block_size - needed_size;
            debug("splinter_size (in bytes)%lu",splinter_size);
            if(splinter_size>=32){
                char  * splinter_head = head + needed_size;
                debug("splinter head address %p",splinter_head);
                PUT(splinter_head,PACK(splinter_size,0x1));
                PUT(splinter_head + splinter_size - WORDSIZE, GET(splinter_head) ^ sf_magic()); //put footer
                insert_node(splinter_head);
            }
            size_t is_prev_alloc = GETPREVALOC(head);
            PUT(head,PACK(needed_size,2 + is_prev_alloc));
            PUT(head + needed_size - WORDSIZE, GET(head) ^ sf_magic()); //put footer
            char * next_blk = head + needed_size;
            size_t next_blk_size = GETSIZE(next_blk);
            size_t is_next_alloc = GETCURALOC(next_blk);
            PUT(next_blk,PACK(next_blk_size,is_next_alloc+1));
            if(next_blk_size!=0){//not epilogue
                 PUT(next_blk + next_blk_size - WORDSIZE,GET(next_blk)^ sf_magic());
            }

            return (head + WORDSIZE);
        }
        temp=temp->body.links.next;
    }
    return NULL;
}
void remove_by_address(sf_block * p, char * remove_this){

    sf_block * first_node = p->body.links.next;
    sf_block * temp = first_node;

    debug("REMOVE_NODE_BY_Address size %p",remove_this);
    while(temp->body.links.next != first_node){
        char * head = (char *) temp + WORDSIZE;
        if(head == remove_this){
            sf_block * prev = temp->body.links.prev;
            sf_block * next = temp->body.links.next;
            prev->body.links.next = next;
            next->body.links.prev = prev;
            temp->body.links.prev = NULL;
            temp->body.links.prev = NULL;
            break;
        }
    }
}
void * remove_node(size_t size, char * address){
    debug("-------------Remove Node-------------");
    int j=0;
    char * ret = NULL;
    for(int i = 1; j<9; i=i*2){
        if(i*M >= size || j==8){
            if(address==NULL)
                ret = remove_node_helper(&sf_free_list_heads[j],size);
            else{
                remove_by_address(&sf_free_list_heads[j],address);
                break;
            }
            if(ret!=NULL){
                debug("Block found!!! for %d ",j);
                break;
            }
            debug("block not found for %d ",j);
        }
        j++;
    }

    return (void *)ret;
}

void * init(){

    debug("------------------init----------------");
    // printf("%p,%p \n",sf_mem_start(),sf_mem_end());

    char * heap_ptr = sf_mem_grow(); //4096 bytes 
    if(heap_ptr == NULL){
        return NULL;
    }

    PUT(heap_ptr,0x0); //8 byte alignment 
    PUT(heap_ptr + WORDSIZE, PACK(32,0x3));
    PUT(heap_ptr + 2*WORDSIZE, 0x0);
    PUT(heap_ptr + 3*WORDSIZE, 0x0);
    PUT(heap_ptr + 4*WORDSIZE, GET(heap_ptr + WORDSIZE) ^ (size_t)sf_magic());
   
    char * e_loc = heap_ptr + PAGE_SZ - WORDSIZE;
    PUT(e_loc, PACK(0,0x2)); //putting epilogue
    heap_ptr += 5*WORDSIZE;

    for(int i=0; i<NUM_FREE_LISTS; i++){
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }
    size_t size = e_loc - heap_ptr;
    PUT(heap_ptr,PACK(size,0x1));
    PUT(heap_ptr + size - WORDSIZE, GET(heap_ptr) ^ (size_t)sf_magic()); //putting block footer

    debug(" check %lu",*((size_t *)(heap_ptr)));

    insert_node(heap_ptr);

    initialized = 1;
    return heap_ptr;

}
char * coalesce(char * middle_blk_head){
     debug("-------------coalesce------------");
     size_t size = GETSIZE(middle_blk_head);
     char* prev_head = GET_HEAD_FROM_FOOT(middle_blk_head - WORDSIZE);
     char* next_head = middle_blk_head + size;
     debug("Prev n next head %p %p %p",prev_head,middle_blk_head,next_head);
     size_t prev_is_alloc = GETCURALOC(prev_head);
     size_t next_is_alloc = GETCURALOC(next_head);
     if(prev_is_alloc && next_is_alloc){
        debug("prev next not free");
        PUT(middle_blk_head,PACK(size,0x00000001));
        PUT(middle_blk_head + size - WORDSIZE, GET(middle_blk_head)^sf_magic());
     }
     else if(prev_is_alloc && !next_is_alloc){
        debug("next free");
        size +=GETSIZE(next_head);
        remove_node(GETSIZE(next_head),next_head);

        PUT(middle_blk_head,PACK(size,0x00000001));
        PUT(middle_blk_head + size - WORDSIZE, GET(middle_blk_head)^sf_magic());
        //sf_show_heap();
        //debug("Footer %lu %p",GET(middle_blk_head),middle_blk_head + size - WORDSIZE);


     }
     else if(!prev_is_alloc && next_is_alloc){
        debug("prev free");
        size +=GETSIZE(prev_head);
        remove_node(GETSIZE(prev_head),prev_head);
        PUT(prev_head,PACK(size,0x1));
        PUT(prev_head + size - WORDSIZE, GET(prev_head)^sf_magic());
        middle_blk_head = prev_head;
     }
     else{
        debug("prev next both free");
        size = size + GETSIZE(prev_head) + GETSIZE(next_head);
        remove_node(GETSIZE(next_head),next_head);
        remove_node(GETSIZE(prev_head),prev_head);
        PUT(prev_head,PACK(size,0x00000001));
        debug("Size %lu %p",size,prev_head);
        PUT(prev_head + size - WORDSIZE, GET(prev_head)^sf_magic());
        middle_blk_head = prev_head;
     }
     return middle_blk_head;
}
void * extend(){
    debug("-----------EXTEND---------------");
    char * epilogue_head = sf_mem_end() - WORDSIZE;
    size_t is_prev_aloc = GETPREVALOC(epilogue_head);
    char * new_blk = sf_mem_grow();
    if(new_blk == NULL){
        debug("Cant grow mem any further");
        return NULL;
    }
    PUT(epilogue_head,PACK(PAGE_SZ,is_prev_aloc));
    PUT(epilogue_head + PAGE_SZ - WORDSIZE,GET(epilogue_head) ^ sf_magic()); //add new block
    PUT(new_blk + PAGE_SZ - WORDSIZE,PACK(0,0x2)); //Add new epilouge

    char * new_big_mem = coalesce(epilogue_head);
    insert_node(new_big_mem);
    sf_show_heap();
    return new_big_mem;
}
void * sf_malloc(size_t size) {
    debug("----------MALLOC size ----------%lu",size);
    if(size==0)
        return NULL;
    void * ret=NULL;
    if(!initialized){
        ret = init();
        if(ret==NULL)
            return NULL;
    }

    if(size<16)size=16;  //bring to min size
    size+=16;
    if(size%16!=0){
        size = size +16 - size%16;
    }

    sf_show_heap();
    ret = remove_node(size,NULL);
    while(ret==NULL){
        if(extend()==NULL)
            return NULL; //extend till required size is achieved
        ret = remove_node(size,NULL);
    }

    sf_show_heap();
    return ret;
}

void sf_free(void *pp) {
    debug("-----------------FREE------------------- %p",pp);
    if(pp==NULL){
        debug("Pointer NULL");
        abort();
    }
    if((char*)pp < ((char*)sf_mem_start() + 5*WORDSIZE) || (char *)pp>((char *)sf_mem_end() - WORDSIZE)){
        debug("Pointer out of heap");
        abort();
    }
    char* blk_head = HDBP(pp);
    size_t blk_size = GETSIZE(blk_head);
    if(blk_size<32 || blk_head + blk_size > (char *)sf_mem_end()){
        debug("Block size below 32 or beyond memory");
        abort();
    }

    char* blk_footer = FTBP(pp);
    if((GET(blk_head))!= (GET(blk_footer)^sf_magic())){
        debug("Header footer mismatch");
        abort();
    }

    int is_alloc = GETCURALOC(blk_head);
    int is_prev_alloc1 = GETPREVALOC(blk_head);
    char* prev_head = GET_HEAD_FROM_FOOT(blk_head - WORDSIZE);
    int is_prev_alloc2 = GETCURALOC(prev_head);
    
    
    if(is_alloc==1){
        debug("Block is not free");
        abort();
    }
    else if(is_prev_alloc1==0 && is_prev_alloc2!=0){
        debug("Prev block free data mismatch");
        abort();
    }
    blk_head = coalesce(blk_head);
    //new size
    // blk_size = GETSIZE(blk_head);

    // //update curr alloc bit to 0 of current block
    // PUT(blk_head,PACK(blk_size,0x1));
    // PUT(blk_head + blk_size - WORDSIZE,GET(blk_head)^sf_magic());

    // //update prev alloc bit of next block
    // char * next_blk = blk_head + blk_size;
    // size_t next_blk_size = GETSIZE(next_blk);
    // size_t is_next_alloc = GETCURALOC(next_blk);
    // debug("%p , %lu , %lu",next_blk, next_blk_size, is_next_alloc);
    // PUT(next_blk, PACK(next_blk_size,is_next_alloc));
    // PUT(next_blk + next_blk_size - WORDSIZE,GET(next_blk)^sf_magic());

    //insert to free seg list
    insert_node(blk_head);


    sf_show_heap();
    return;
}

void *sf_realloc(void *pp, size_t rsize) {
    debug("-------------------------Realloc------------");
    if(pp==NULL){
        debug("Pointer NULL");
        abort();
    }
    if((char*)pp < ((char*)sf_mem_start() + 5*WORDSIZE) || (char *)pp>((char *)sf_mem_end() - WORDSIZE)){
        debug("Pointer out of heap");
        abort();
    }
    char* blk_head = HDBP(pp);
    size_t blk_size = GETSIZE(blk_head);
    if(blk_size<32 || blk_head + blk_size > (char *)sf_mem_end()){
        debug("Block size below 32 or beyond memory");
        abort();
    }

    char* blk_footer = FTBP(pp);
    if((GET(blk_head))!= (GET(blk_footer)^sf_magic())){
        debug("Header footer mismatch");
        abort();
    }

    int is_prev_alloc1 = GETPREVALOC(blk_head);
    char* prev_head = GET_HEAD_FROM_FOOT(blk_head - WORDSIZE);
    int is_prev_alloc2 = GETCURALOC(prev_head);
    
    
    if(is_prev_alloc1==0 && is_prev_alloc2!=0){
        debug("Prev block free data mismatch");
        abort();
    }

    if(rsize == 0){
        debug("rsize is 0");
        sf_free(pp);
        return NULL;
    }
    size_t blk_payload = blk_size - 16;
    //CASE Larger
    if(rsize>blk_payload){
        debug("case Larger payload req %lu",rsize);
        void * dest = sf_malloc(rsize);
        if(dest == NULL){
            return NULL;
        }
        memcpy(dest,pp,blk_size);
        sf_free(pp);
        sf_show_heap();
        return dest;
    }
    else if(rsize<blk_payload){
        debug("case smaller payload req %lu available %lu",rsize,blk_payload);
        if(rsize<16)rsize=16;  //next pointer prev pointer size
        rsize+=16;
        if(rsize%16!=0){
            rsize = rsize +16 - rsize%16;
        }
        size_t splinter = blk_size - rsize;
        debug("splinter size %lu",splinter);
        if(splinter>=32){
            PUT(blk_head,PACK(rsize,2 + is_prev_alloc1));
            PUT(blk_head + rsize - WORDSIZE, GET(blk_head)^sf_magic());
            char * splinter_head = blk_head + rsize;
            PUT(splinter_head, PACK(splinter,0x1));
            PUT(splinter_head + splinter - WORDSIZE, GET(splinter_head)^sf_magic());
            splinter_head = coalesce(splinter_head);
            insert_node(splinter_head);
            sf_show_heap();
            return pp;
        }
        else{
            return pp;
        }
    }
    else{
        return pp;
    }
    return NULL;
}
