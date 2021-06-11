//
//  slab.c
//
//
//  Created by Dian Chen on 5/29/21.
//  Reference: https://www.kernel.org/doc/gorman/html/understand/understand011.html
//  Some code is modified from Linux slab allocator
//
#include <nautilus/nautilus.h>
#include <nautilus/spinlock.h>
#include <nautilus/paging.h>
#include <nautilus/thread.h>
#include <nautilus/shell.h>

#include <nautilus/alloc.h>

#include <nautilus/mm.h>
#include <nautilus/naut_string.h>


#include "memlib.h"
#include<nautilus/naut_types.h>

#include "slab.h"

#ifndef NAUT_CONFIG_DEBUG_ALLOC_CS213
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define ALLOC_ERROR(fmt, args...) ERROR_PRINT("alloc-custom-slab: " fmt"\n", ##args)
#define ALLOC_DEBUG(fmt, args...) DEBUG_PRINT("alloc-custom-slab: " fmt"\n", ##args)
#define ALLOC_INFO(fmt, args...)   INFO_PRINT("alloc-custom-slab: " fmt"\n", ##args)

//Macro def for allocator
#define MAX(x, y) ((x) > (y)? (x) : (y))
//reference: https://stackoverflow.com/questions/1010922/question-about-round-up-macro 
#define ROUNDDOWN(N, S) ({ (N/S)*S;})

#define NUM_PAGES_PER_SLAB 1    //the pages requried to create one slab, typically 1
static struct kmem_cache_s descriptor;  //a cache descriptor

/*
 create a cache that hold a set of slabs
 @param
 the_cache: a struct kmem_cache_s from user space
 name: name of the cache
 size: size of the objects
 num: number of pages in the slabs
 flags: behave flag
 ctor: a complex object
 return the cache
 */
struct kmem_cache_s * kmem_cache_create(struct kmem_cache_s *the_cache, char *name, size_t size, unsigned int num, unsigned int flags, void (*ctor)(void *)){
    
    
    the_cache->name = name;
    
    if (the_cache == NULL || num == 0){
        return NULL;
    }
    
    the_cache->slabs_full = NULL;
    the_cache->slabs_partial = NULL;
    the_cache->slabs_free = NULL;
    
    the_cache->objsize_s = size;
    the_cache->num_pages_per_slab = num;
    
    if (size > sizeof(void*)){
        the_cache->objsize = size;
    }else{
        the_cache->objsize = sizeof(void*);
    }
    
    the_cache->flags = flags;
    the_cache->total_slabs = 0;
    the_cache->num_active = 0;
    the_cache->num_allocation = 0;
    
    if (flags & CFLGS_OFF_SLAB){
        the_cache->num = ((mem_pagesize() - (sizeof(void*) * 2 - sizeof (struct kmem_cache_s))) / the_cache->objsize) * the_cache->num_pages_per_slab;
    }else{
        the_cache->num = ((mem_pagesize() - (sizeof(void*) * 2)) / the_cache->objsize) * the_cache->num_pages_per_slab;
                          
        the_cache->descriptor = &descriptor;
    }
                          
    the_cache->ctor = ctor;
    
    return the_cache;
    
         
}
/*
 create a slab that holds objects
 @param
 the_cache: the cache descriptor
 size: size of objects
 num: num of pages in slab
 state: current state of the allocator
 return address of the slab
 */


struct slab_s *kmem_cache_grow(struct kmem_cache_s *the_cache, size_t size, unsigned int num, void* state){
    
    
    struct nk_alloc_slab *as = (struct nk_alloc_slab *) state;
    
    //allocte memory from the sizes cache
    void *page_slab = kmem_sys_malloc_specific(num * mem_pagesize(), 0, 0&NK_ALLOC_ZERO);
    
    //initialize a new slab which will be added to cache later
    struct slab_s *the_slab = NULL;
    
    if (the_cache == NULL){
        
        //the size of a slab is just the size of a page
        the_slab = (struct slab_s*)((size_t)page_slab + sizeof(struct slab_s *));
        
    }else{
        
        //off slab
        the_slab = kmem_cache_alloc(the_cache);
        
    }
    
    if(the_slab == NULL){
        
        return NULL;
        
    }else{
    
    struct slab_s **pages = page_slab;
    
    //set slab into the page
    for(unsigned int i = 0;i<num;i++){
        
        *pages = the_slab;
        pages = (struct slab_s **)((size_t)pages + mem_pagesize());
        
    }
    
    the_slab->s_mem = page_slab;
    
        if (the_cache == NULL){
            
            //set the first free object for new slab
            the_slab->active = (struct obj_s*)((size_t)page_slab + sizeof(struct slab_s *) + sizeof(struct slab_s));
            
        }else{
            the_slab->active = (struct obj_s*)((size_t)page_slab + sizeof(struct slab_s *));
        }
        

        //create a freelist for the slab
        the_slab->list = the_slab->active;
        struct obj_s *the_obj = the_slab->active;
        struct obj_s *next = NULL;
        
        void * temp = page_slab;
        
        for(unsigned int i = 0;i<num;i++){
            
            struct obj_s *prev, *cur;

            prev = cur = the_obj;
             
            //find an active object
            while(((size_t)cur - (size_t)temp) <= (mem_pagesize() - size)){
                
                cur = (struct obj_s *)((size_t)prev + size);
                prev->header.active = cur;
                
                if(((size_t)cur - (size_t)temp) <= (mem_pagesize() - size)){
                    prev = cur;
                }
            }
            
            prev->header.active = NULL;
            
            if (i < (num - 1)){
                temp = (void *)((size_t)temp + mem_pagesize());
                next->header.active = (struct obj_s *)((size_t)temp + sizeof(struct slab_s*));
            }
        }
        
        return the_slab;
    
    
    }
    
}


/*
Allocate a new object to the slab
@param
the_cache: current state of the allocator
return
*/
void *kmem_cache_alloc(struct kmem_cache_s *the_cache){
    
    
    struct obj_s *new_obj = NULL;
    
    if(the_cache != NULL){

   
    if(the_cache->slabs_partial == NULL){

        
        //allocate object from free slab
        if(the_cache->slabs_free == NULL){

            
            //initial array of free slabs
            if(the_cache->flags & CFLGS_OFF_SLAB)
            {
                the_cache->slabs_free = kmem_cache_grow(NULL, the_cache->objsize_s, the_cache->num_pages_per_slab, NULL);
                
            }else{
                the_cache->slabs_free = kmem_cache_grow(the_cache->descriptor, the_cache->objsize_s, the_cache->num_pages_per_slab, NULL);
            }
            
        }

        
        //allocate object from slab
        struct slab_s * the_slab = the_cache->slabs_free;

         new_obj = the_slab->active;
        
        the_slab->active = new_obj->header.active;
        new_obj->header.active = NULL;
        
        if (new_obj == NULL){
            ALLOC_DEBUG("No available object for current slab!\n");
            return NULL;
        }
        
    }else{

        
        //allocate object from existing slab
        struct slab_s * the_slab = the_cache->slabs_partial;

        new_obj = the_slab->active;
        
        the_slab->active = new_obj->header.active;
        new_obj->header.active = NULL;
        
        
    }
    
    }

    
    if (the_cache->ctor == NULL){

        
        return new_obj;

    }else{

      
        the_cache->ctor(new_obj);
        
    }
    
    

    return new_obj;
}

/*
free the given object from the cache
@param
the_cache: current state of the allocator 
objp: the object to be freed
return
*/
void kmem_cache_free(struct kmem_cache_s *the_cache, void *objp){
    
    
    
    if(the_cache == NULL || objp == NULL){
        ALLOC_DEBUG("Fail to free the cache!\n");
        
        return;
    }else{
        
        //free the given object
        //just unlink the slab by removing the pointer of the active object to next
        struct slab_s *the_slab = *((struct slab_s **)ROUNDDOWN((size_t)objp, mem_pagesize()));
        
        struct obj_s *obj = objp;
        obj->header.active = the_slab->active;
        the_slab->active = objp;
        
    }

    
    return;
}

/*
destory the given slab
@param
the_slab: the slab to be destoryed
return
*/
void kmem_slab_destory(struct slab_s * the_slab){

  
    
    if(the_slab == NULL){
        
        ALLOC_DEBUG("Fail to destory slab! No slab exists\n");
        
        return;
    }else{
        
        struct slab_s * current = the_slab;
        struct slab_s * next;
        while(current != NULL){
            
            next = current->next;
            //unlinked the slab
            //free memory from the system
            kmem_sys_free(current);
            current = next;
        }
        
        return;
    }
}

/*
destory the given cache
@param
the_cache: the cache to be destoryed
return
*/
void kmem_cache_destory(struct kmem_cache_s * the_cache){
    
    
    if(the_cache == NULL){
        ALLOC_DEBUG("Fail to destory the cache! No cache exists \n");
        
        return;
    }else{
        
        kmem_slab_destory(the_cache->slabs_free);
        kmem_slab_destory(the_cache->slabs_partial);
        kmem_slab_destory(the_cache->slabs_full);

        
        return;
    }
}


//Nautilus allocator interface implement
// this is an implicit free list allocator
struct nk_alloc_slab {
    
  nk_alloc_t *alloc;

  struct kmem_cache_s the_cache;

};

/*
 * An indirection/wrapper to impl_alloc designed for
 * compiler instrumentation purposes
 */
static void * impl_alloc(void *state, size_t size, size_t align, int cpu, nk_alloc_flags_t flags)
{
    
    
    
    struct nk_alloc_slab *as = (struct nk_alloc_slab *)state;

  return kmem_cache_alloc(&as->the_cache);
}

/*
 * An indirection/wrapper to impl_free designed for
 * compiler instrumentation purposes
 */
static void impl_free(void *state, void *ptr)
{
    struct nk_alloc_slab *as = (struct nk_alloc_slab *)state;
  /* $end mmfree */
    return kmem_cache_free(&as->the_cache,ptr);
}

static  int destroy(void *state)
{
  struct nk_alloc_slab *as = (struct nk_alloc_slab *)state;
  ALLOC_DEBUG("%s: destroy - note all memory leaked...\n",as->alloc->name);

  kmem_cache_destory(&as->the_cache);
  kmem_cache_destory(&descriptor);

  return 0;
}

static int print(void *state, int detailed)
{

  struct nk_alloc_slab *as = (struct nk_alloc_slab *)state;
  return 0;
}


static nk_alloc_interface_t slab_interface = {
  .destroy = destroy,
  .allocp = impl_alloc,
  .reallocp = NULL,
  .freep = impl_free,
  .print = print
};

static struct nk_alloc * create(char *name)
{
  ALLOC_DEBUG("create allocator %s\n",name);

  struct nk_alloc_slab *as = kmem_sys_malloc_specific(sizeof(*as),my_cpu_id(),1);

  if (!as) {
    ALLOC_ERROR("unable to allocate allocator state for %s\n",name);
    return 0;
  }

    kmem_cache_create(&descriptor,name,sizeof(struct kmem_cache_s),NUM_PAGES_PER_SLAB,CFLGS_OFF_SLAB,NULL);

  as->the_cache = descriptor;
  as->alloc = nk_alloc_register(name,0,&slab_interface, as);
  if (!as->alloc) {
    ALLOC_ERROR("Unable to register allocator %s\n",name);
    kmem_sys_free(as);
    return 0;
  }

  ALLOC_DEBUG("allocator %s configured and initialized\n", as->alloc->name);



  return as->alloc;
}


static nk_alloc_impl_t slab = {
  .impl_name = "slab",
  .create = create,
};



nk_alloc_register_impl(slab);



