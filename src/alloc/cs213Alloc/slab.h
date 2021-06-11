//
//  slab.h
//
//
//  Created by Dian Chen on 5/29/21.
//

#ifndef slab_h
#define slab_h

#include <stdint.h>

#define CFLGS_OFF_SLAB 2

struct obj_s{
    
    struct{
        
        struct obj_s *active; //link to next active object
    }header;
    
    char *data;  //data stored per object
};

struct slab_s{
    
    void *s_mem; //this gives the starting address of the first object within the slab
    unsigned int inuse; //this gives the number of active objects in the slab
    struct obj_s *list; //this is an array of objects in the slab
    struct obj_s *active; //this is the object which is ready to be used
    
    //slabs are allocated as the continuous page which we implements by using linked list
    //each node has the link to its previous slab, and its next slab
    struct slab_s *prev;
    struct slab_s *next;
    
};

struct kmem_cache_s{
    
    char *name;
    
    //three lists where each type of slabs are stored as descriped in the document
    struct slab_s *slabs_full;
    struct slab_s *slabs_partial;
    struct slab_s *slabs_free;
    
    unsigned int objsize_s; //the fixed size of each object packed into the slab
    unsigned int objsize; //real object packed into the slab
    unsigned int flags; //the flags determine how parts of the allocator will behave when dealing with the    cache
    unsigned int num; //the number of objects contained in each slab
    unsigned int total_slabs; //total number of slabs
    unsigned int num_pages_per_slab; //the pages requried to create a slab
    
    unsigned int num_active; //the current number of active objects in the cache is stored
    unsigned int num_allocation; //running total number of objects that have been allocated on this cache
    
    void (*ctor)(void *); //a complex object has the option of providing a constructor function to be called to initialize each new object( a pointer to that function)
    
    struct kmem_cache_s *descriptor; //a cache descriptor to allow for fast accesses to segmented memory
    
};

struct kmem_cache_s * kmem_cache_create(struct kmem_cache_s *the_cache, char *name, size_t size, unsigned int num, unsigned int flags, void (*ctor)(void *));
struct slab_s *kmem_cache_grow(struct kmem_cache_s *the_cache, size_t size, unsigned int num, void* state);
void *kmem_cache_alloc(struct kmem_cache_s *the_cache);
void kmem_cache_free(struct kmem_cache_s *the_cache, void *objp);
void kmem_slab_destory(struct slab_s * the_slab);
void kmem_cache_destory(struct kmem_cache_s * the_cache);





#endif /* slab_h */
