#include "hash.h"
#include "ll_htable.h"


int getAmt(volatile node* ptr){
  return ((unsigned long)ptr)&0xf;
}
volatile node* getPtr(volatile node* ptr){
  unsigned long mask=0xf;
  return (volatile node*)((((unsigned long)ptr)&(~mask)));
}
void setPtr(volatile node** ptr, int amt){

  *ptr=(void*)(((unsigned long)getPtr(*ptr))|amt);
}
void incPtr(volatile node** ptr){

  int amt=getAmt(*ptr)+1;
  *ptr=(void*)(((unsigned long)getPtr(*ptr))|amt);
}
void freeTable(table* toadd){
  for(int i =0;i<toadd->size;i++){
    free(toadd->q[i]);
  }
  free(toadd->q);
  free(toadd);

 
}

table* createTable(int n_size){
  table* t=(table*)malloc(sizeof(table));
  t->size=n_size;
  t->q=(que**)malloc(sizeof(que*)*(t->size));
  //  ht->next=NULL;
  for(int i =0;i<n_size;i++){
  node* new_node = (node*)malloc(sizeof(node));
    new_node->val=-1;
  t->q[i]=(que*)malloc(sizeof(que));
  new_node->next.ptr=NULL;
  t->q[i]->head.ptr=new_node;
  t->q[i]->tail.ptr=new_node;
  }

  return t;
}

int searchq(que** q, unsigned long val){
  //  printf("start search\n");
  unsigned int bucket= murmur3_32(&val, 8, seeds)%initSize;
  volatile pointer tail=q[bucket]->head;
  while(getPtr(tail.ptr)!=NULL){
    if(getPtr(tail.ptr)->val==val){
      //  printf("end search\n");
      return 1;
    }
    tail=getPtr(tail.ptr)->next;
  }
  //  printf("end search\n");
  return 0;
}


int addDrop(node* ele ,table* toadd, int tt_size){
  table* expected=NULL;
  int res = __atomic_compare_exchange(&global->t[tt_size] ,&expected, &toadd, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  if(res){
    int newSize=tt_size+1;
    __atomic_compare_exchange(&global->cur,
			      &tt_size,
			      &newSize,
			      1,__ATOMIC_RELAXED, __ATOMIC_RELAXED);
    enq(ele);
  }
  else{
    freeTable(toadd);
    int newSize=tt_size+1;
    __atomic_compare_exchange(&global->cur,
			      &tt_size,
			      &newSize,
			      1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    enq(ele);
  }
  return 0;
}


volatile pointer makeFrom(volatile node* new_node, int amt){
  volatile pointer p = {.ptr=new_node};
  setPtr(&(p.ptr),amt);
  return p;
}



void enq(node* new_node){
  int startCur=global->cur;
  table* t;
  for(int i =0;i<global->cur;i++){
    t=global->t[i];
  unsigned int bucket= murmur3_32(&new_node->val, 8, seeds)%t->size;

  volatile pointer tail;
  volatile pointer next;
  tail=t->q[bucket]->head;

  int should_hit=0;
  while(1){
    
    while(getPtr(tail.ptr)!=getPtr(t->q[bucket]->tail.ptr)){
    if(getPtr(tail.ptr)->val==getPtr(new_node)->val){
      return;
    }
    tail=getPtr(tail.ptr)->next;
  }
    next=getPtr(tail.ptr)->next;
 if(getPtr(tail.ptr)->val==getPtr(new_node)->val){
   return;
 }


   if(getPtr(tail.ptr)==getPtr(t->q[bucket]->tail.ptr)){
     if(getPtr(next.ptr)==NULL){
       
       int pval=getAmt(tail.ptr)+1;
       // printf("pval=%d\n",pval);
       if(pval>=max_ele){
	 break;
       }
       volatile pointer p=makeFrom(new_node, pval);
       if(__atomic_compare_exchange((pointer*)&(getPtr(tail.ptr)->next) ,(pointer*)&next,(pointer*) &p, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
	  should_hit=1;
	  break;
	}
	else{
	  if(next.ptr->val==new_node->val){
	    return;
	  }
	  int pval=getAmt(next.ptr);
	  //	         printf("pval=%d\n",pval);
	  /*	  if(pval>=max_ele){
	    	    if(searchq(t->q, getPtr(new_node)->val)){
	    	      return;
	    	    }
	    break;
	    }*/
	  volatile pointer p2=makeFrom(next.ptr, pval);
	  if(__atomic_compare_exchange((pointer*)&t->q[bucket]->tail ,(pointer*)&tail, (pointer*)&p2, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
	  }
	}
      }
    }
  }
  if(should_hit){
    int pval=getAmt(t->q[bucket]->tail.ptr)+1;
    //    printf("pval=%d\n",pval);
    volatile pointer p3=makeFrom(new_node, pval);
  if(__atomic_compare_exchange((pointer*)&t->q[bucket]->tail ,(pointer*)&tail, (pointer*)&p3, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
  }
  return;
  }
	 
  startCur=global->cur;
  }
  table* new_table=createTable(global->t[startCur-1]->size<<1);
  addDrop(new_node, new_table, startCur);
}

double freeAll(){

  int amt=0;
  table* t;
  int extra=0;
  int r_amt=0;
  for(int j =0;j<global->cur;j++){
    t=global->t[j];
            printf("Table %d Size %d\n", j, t->size);
    extra+=t->size;
 for(int i =0;i<t->size;i++){
   volatile node* temp=getPtr(t->q[i]->head.ptr);
  if(todo)
    printf("%d: ", i);
  int pval=getAmt(temp->next.ptr);
  temp=getPtr(temp->next.ptr);

  while(temp!=NULL){

    amt++;
    r_amt++;

      if(todo)
	printf("%p - (%d) %lu, ", getPtr(temp),pval,temp->val);
      if(pval!=r_amt||pval>=max_ele){
		printf("fuck up\n");
      }
      
      pval=getAmt(temp->next.ptr);
      temp=getPtr(temp->next.ptr);

  }
  r_amt=0;
  if(todo)
    printf("\n");
 }
   if(todo)
     printf("\n\n\n");
  }
 


}
