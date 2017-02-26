//
//  COMP1927 Assignment 1 - Vlad: the memory allocator
//  allocator.c ... implementation
//
//  Created by Liam O'Connor on 18/07/12.
//  Modified by John Shepherd in August 2014, August 2015
//  Copyright (c) 2012-2015 UNSW. All rights reserved.
//

#include "allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define HEADER_SIZE    sizeof(struct free_list_header)  
#define MAGIC_FREE     0xDEADBEEF
#define MAGIC_ALLOC    0xBEEFDEAD

typedef unsigned char byte;
typedef u_int32_t vlink_t;
typedef u_int32_t vsize_t;
typedef u_int32_t vaddr_t;

void vlad_merge(vaddr_t array);

typedef struct free_list_header {
   u_int32_t magic;  // ought to contain MAGIC_FREE
   vsize_t size;     // # bytes in this block (including header)
   vlink_t next;     // memory[] index of next free block
   vlink_t prev;     // memory[] index of previous free block
} free_header_t;

// Global data

static byte *memory = NULL;   // pointer to start of allocator memory
static vaddr_t free_list_ptr; // index in memory[] of first block in free list
static vsize_t memory_size;   // number of bytes malloc'd in memory[]


// Input: size - number of bytes to make available to the allocator
// Output: none              
// Precondition: Size is a power of two.
// Postcondition: `size` bytes are now available to the allocator
// 
// (If the allocator is already initialised, this function does nothing,
//  even if it was initialised with different size)

void vlad_init(u_int32_t size)
{
   if (memory!=NULL) return;   //check if already called 
   int n=1;                  
   for (n=1; size>=n*2; n=n*2);  //size change
   if (size <= 512) n=512;      //min size 512 bytes
   memory = malloc(size);  
   if (memory==NULL){         //check if malloc succeeded
      fprintf(stderr, "vlad_init: insufficient memory");
      abort();
   }
   free_header_t *header=(free_header_t *)&memory[0]; //set to first
   free_list_ptr = 0;
   memory_size=(vsize_t)n;
   header->magic=MAGIC_FREE;        //init stuff
   header->size=(vsize_t)n;
   header->next=free_list_ptr;
   header->prev=free_list_ptr;
}


// Input: n - number of bytes requested
// Output: p - a pointer, or NULL
// Precondition: n is < size of memory available to the allocator
// Postcondition: If a region of size n or greater cannot be found, p = NULL 
//                Else, p points to a location immediately after a header block
//                      for a newly-allocated region of some size >= 
//                      n + header size.

void *vlad_malloc(u_int32_t n)
{  
   if (memory_size<n+HEADER_SIZE) return NULL;    //check if enough total mem
   free_header_t *temp=(free_header_t *)(&memory[free_list_ptr]);     //set to first
   

   u_int32_t search=n+HEADER_SIZE;   //compiler is unhappy if put in for loop
   for(search=n+HEADER_SIZE; search > temp->size && temp->next!=free_list_ptr; temp=(free_header_t *)&memory[temp->next]){  
      if (temp==(free_header_t *)&memory[temp->next]){      //check for suitable block
         break;
      }
      if (temp->magic!=MAGIC_FREE){
            fprintf(stderr, "Memory corruption");  //check magic number
            abort();
      } 
   }
   if (temp->next==free_list_ptr && search > temp->size){   //if only one block and can't be split return null
         return NULL;
   }
   //vsize_t size=memory_size;
   int array=(int)(temp->size);     //set array number to the block size
  
   while (temp->size/2>HEADER_SIZE+n){   //if size can be split to allow n, then split    
      array=array/2;             //split block
      if(temp==(free_header_t *)&memory[temp->next]){     //only one node
         temp->size = array;                    
         free_header_t *newnode = temp + array/HEADER_SIZE;    
         //printf("newnode %p and memory %p\n", newnode, memory);
         newnode->magic=MAGIC_FREE;       
         newnode->size=(vsize_t)array;
         newnode->prev=(void *)temp-(void *)memory;
         temp->next = (void *)newnode-(void *)memory;         //setting node links
         newnode->next=(void *)temp-(void *)memory;
         free_header_t *tempnext=(free_header_t *)&memory[newnode->next];
         tempnext->prev=(void *)newnode-(void *)memory;
       
         free_list_ptr=newnode->next;   //set free list to next free block
         if (newnode->magic!=MAGIC_FREE){
            fprintf(stderr, "Memory corruption");
            abort();
         }
      }else{
         temp->size = array;
         free_header_t *newnode = temp + array/HEADER_SIZE;
         newnode->magic=MAGIC_FREE;       
         newnode->size=(vsize_t)array;
         newnode->prev=(void *)temp-(void *)memory;              //similar to above
         newnode->next=temp->next;
         temp->next = (void *)newnode-(void *)memory;
         free_header_t *tempnext=(free_header_t *)&memory[newnode->next];
         tempnext->prev=(void *)newnode-(void *)memory;
                
         free_list_ptr=newnode->next;
         if (newnode->magic!=MAGIC_FREE){
            fprintf(stderr, "Memory corruption");
            abort();
         }
      }  
   }
   if (temp==(free_header_t *)&memory[temp->next]){      //if one block return null
      return NULL;
   } 
   temp->magic = MAGIC_ALLOC;                   //setting alloc tag
   free_header_t *tempnext=(free_header_t *)&memory[temp->next];
   free_header_t *tempprev=(free_header_t *)&memory[temp->prev];
   tempnext->prev = temp->prev;
   tempprev->next = temp->next;     //node links
   free_list_ptr = temp->next;
   //vlad_stats();
   return ((void *)(temp+HEADER_SIZE/HEADER_SIZE));   //return free region
}

// Input: object, a pointer.
// Output: none
// Precondition: object points to a location immediately after a header block
//               within the allocator's memory.
// Postcondition: The region pointed to by object can be re-allocated by 
//                vlad_malloc

void vlad_free(void *object)
{
   free_header_t *deleteNode = (free_header_t *)(object-HEADER_SIZE);    //change to the header
   if (deleteNode->magic != MAGIC_ALLOC){                            //alloc check
      fprintf(stderr, "Attempt to free non-allocated memory");
      abort();
   }
   
   free_header_t *temp = (free_header_t *)&memory[free_list_ptr];  
   free_header_t *tempprev; 
   free_header_t *last = (free_header_t *)&memory[temp->prev];       
   //temp stays one in front of deleteNode
   if ((void *)deleteNode<=(void *)temp){            //if smallest; using void * rather than ->size as mem indexes keep in order
      free_list_ptr = (void *)deleteNode-(void *)memory;     //set the free ptr to the first 
   } else if ((void *)deleteNode > (void *)last){         //if largest leave temp as first
   } else {
      for(temp=(free_header_t *)&memory[free_list_ptr];(void *)deleteNode>=(void *)temp;temp=(free_header_t *)&memory[temp->next]);   
      //find suitable position
   }
   
   tempprev=(free_header_t *)&memory[temp->prev];     
   deleteNode->magic=MAGIC_FREE;       //set free tag
   deleteNode->prev = temp->prev;                     //linking up
   deleteNode->next = (void *)temp-(void *)memory;
   tempprev->next = (void *)deleteNode-(void *)memory;
   temp->prev = (void *)deleteNode-(void *)memory;
     
   vaddr_t array = ((void *)deleteNode - (void *)memory);   
   vlad_merge(array);     
}

//Merge blocks together if they are same size and consequtive

void vlad_merge(vaddr_t array)
{
   free_header_t *temp = (free_header_t *)&memory[array];    //initializing
   free_header_t *tempnext = (free_header_t *)&memory[temp->next];
   free_header_t *tempprev = (free_header_t *)&memory[temp->prev];     
   
   
   if (((void *)temp - (void *)memory)%(2*temp->size)==0){           //if temp index is a multiple of double size, merge
      if (temp->size == tempnext->size){      //if adjacent blocks are equal
         free_header_t * next = (free_header_t *)&memory[temp->next];      
         free_header_t * nnext = (free_header_t *)&memory[next->next];
         if (temp->next - array == temp->size){     //if temp next equal to temp size
            //free_list_ptr = (void *)temp - (void *)memory;                    
            temp->next = (void *)nnext - (void *)memory;  
            next = (free_header_t *)&memory[temp->next]; 
            nnext->prev = array; 
            temp->size = 2*temp->size;    //merge
            vlad_merge(array);         //make sure properly merged by going through the headers again
         }
      }         
   }else{               
      if (tempprev->size == temp->size) {    //if prev block equal to temp
         if (array-temp->prev ==  temp->size) {       //if index equal to size
          tempnext->prev = temp->prev;       //link up
          tempprev->next = temp->next;       
          tempprev->size = 2*tempprev->size;    //merge
          array = temp->prev;       
          vlad_merge(array);
         }
        
      //temp=(free_header_t *)&memory[temp->next];     
      }
   } 
          
      
}


// Stop the allocator, so that it can be init'ed again:
// Precondition: allocator memory was once allocated by vlad_init()
// Postcondition: allocator is unusable until vlad_int() executed again

void vlad_end(void)
{
   if (memory!=NULL){      //if memory has been allocated free it
      free(memory);
      memory=NULL;
   }  
   memory_size=0;
   
  
}


// Precondition: allocator has been vlad_init()'d
// Postcondition: allocator stats displayed on stdout

void vlad_stats(void)
{
   free_header_t *temp = (free_header_t *)&memory[free_list_ptr];
   free_header_t *first = (free_header_t *)&memory[0];
   
   printf("================================STATS=================================\n");
   printf("FIRST IN MEMORY:\n");
   printf("%d, 0, %d, %d, %d\n", first->magic, first->size, first->prev, first->next);
   printf("======================================================================\n");
   printf("MAGIC     |INDEX SIZE PREV NEXT\n");
   printf("%d| %d, %d, %d, %d\n", temp->magic, (void *)temp-(void *)memory, temp->size, temp->prev, temp->next);
   temp=(free_header_t *)&memory[temp->next];
   while (temp!=(free_header_t *)&memory[free_list_ptr]){      
      printf("%d| %d, %d, %d, %d\n", temp->magic, (void *)temp-(void *)memory, temp->size, temp->prev, temp->next);
      temp=(free_header_t *)&memory[temp->next];
   }
   printf("=============================END OF LIST==============================\n");
   
}


//
// All of the code below here was written by Alen Bou-Haidar, COMP1927 14s2
//

//
// Fancy allocator stats
// 2D diagram for your allocator.c ... implementation
// 
// Copyright (C) 2014 Alen Bou-Haidar <alencool@gmail.com>
// 
// FancyStat is free software: you can redistribute it and/or modify 
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or 
// (at your option) any later version.
// 
// FancyStat is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>


#include <string.h>

#define STAT_WIDTH  32
#define STAT_HEIGHT 16
#define BG_FREE      "\x1b[48;5;35m" 
#define BG_ALLOC     "\x1b[48;5;39m"
#define FG_FREE      "\x1b[38;5;35m" 
#define FG_ALLOC     "\x1b[38;5;39m"
#define CL_RESET     "\x1b[0m"


typedef struct point {int x, y;} point;

static point offset_to_point(int offset,  int size, int is_end);
static void fill_block(char graph[STAT_HEIGHT][STAT_WIDTH][20], 
                        int offset, char * label);



// Print fancy 2D view of memory
// Note, This is limited to memory_sizes of under 16MB
void vlad_reveal(void *alpha[26])
{
    int i, j;
    vlink_t offset;
    char graph[STAT_HEIGHT][STAT_WIDTH][20];
    char free_sizes[26][32];
    char alloc_sizes[26][32];
    char label[3]; // letters for used memory, numbers for free memory
    int free_count, alloc_count, max_count;
    free_header_t * block;


    // initilise size lists
    for (i=0; i<26; i++) {
        free_sizes[i][0]= '\0';
        alloc_sizes[i][0]= '\0';
    }

    // Fill graph with free memory
    offset = 0;
    i = 1;
    free_count = 0;
    while (offset < memory_size){
        block = (free_header_t *)(memory + offset);
        if (block->magic == MAGIC_FREE) {
            snprintf(free_sizes[free_count++], 32, 
                "%d) %d bytes", i, block->size);
            snprintf(label, 3, "%d", i++);
            fill_block(graph, offset,label);
        }
        offset += block->size;
    }

    // Fill graph with allocated memory
    alloc_count = 0;
    for (i=0; i<26; i++) {
        if (alpha[i] != NULL) {
            offset = ((byte *) alpha[i] - (byte *) memory) - HEADER_SIZE;
            block = (free_header_t *)(memory + offset);
            snprintf(alloc_sizes[alloc_count++], 32, 
                "%c) %d bytes", 'a' + i, block->size);
            snprintf(label, 3, "%c", 'a' + i);
            fill_block(graph, offset,label);
        }
    }

    // Print all the memory!
    for (i=0; i<STAT_HEIGHT; i++) {
        for (j=0; j<STAT_WIDTH; j++) {
            printf("%s", graph[i][j]);
        }
        printf("\n");
    }

    //Print table of sizes
    max_count = (free_count > alloc_count)? free_count: alloc_count;
    printf(FG_FREE"%-32s"CL_RESET, "Free");
    if (alloc_count > 0){
        printf(FG_ALLOC"%s\n"CL_RESET, "Allocated");
    } else {
        printf("\n");
    }
    for (i=0; i<max_count;i++) {
        printf("%-32s%s\n", free_sizes[i], alloc_sizes[i]);
    }

}

// Fill block area
static void fill_block(char graph[STAT_HEIGHT][STAT_WIDTH][20], 
                        int offset, char * label)
{
    point start, end;
    free_header_t * block;
    char * color;
    char text[3];
    block = (free_header_t *)(memory + offset);
    start = offset_to_point(offset, memory_size, 0);
    end = offset_to_point(offset + block->size, memory_size, 1);
    color = (block->magic == MAGIC_FREE) ? BG_FREE: BG_ALLOC;

    int x, y;
    for (y=start.y; y < end.y; y++) {
        for (x=start.x; x < end.x; x++) {
            if (x == start.x && y == start.y) {
                // draw top left corner
                snprintf(text, 3, "|%s", label);
            } else if (x == start.x && y == end.y - 1) {
                // draw bottom left corner
                snprintf(text, 3, "|_");
            } else if (y == end.y - 1) {
                // draw bottom border
                snprintf(text, 3, "__");
            } else if (x == start.x) {
                // draw left border
                snprintf(text, 3, "| ");
            } else {
                snprintf(text, 3, "  ");
            }
            sprintf(graph[y][x], "%s%s"CL_RESET, color, text);            
        }
    }
}

// Converts offset to coordinate
static point offset_to_point(int offset,  int size, int is_end)
{
    int pot[2] = {STAT_WIDTH, STAT_HEIGHT}; // potential XY
    int crd[2] = {0};                       // coordinates
    int sign = 1;                           // Adding/Subtracting
    int inY = 0;                            // which axis context
    int curr = size >> 1;                   // first bit to check
    point c;                                // final coordinate
    if (is_end) {
        offset = size - offset;
        crd[0] = STAT_WIDTH;
        crd[1] = STAT_HEIGHT;
        sign = -1;
    }
    while (curr) {
        pot[inY] >>= 1;
        if (curr & offset) {
            crd[inY] += pot[inY]*sign; 
        }
        inY = !inY; // flip which axis to look at
        curr >>= 1; // shift to the right to advance
    }
    c.x = crd[0];
    c.y = crd[1];
    return c;
}
