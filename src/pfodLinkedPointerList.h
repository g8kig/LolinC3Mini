#ifndef PFOD_LINKEDPOINTERLIST_H_
#define PFOD_LINKEDPOINTERLIST_H_
// pfodLinkedPointerList.h
/*
   Modified by Matthew Ford to remove index and only use pointers and add getFirst(), getNext() iterator
   Now acts as LOFO list
   NULL data pointers not added to list
   Not longer limited to 255 elements. Only limited by available memory. Still no caching
   !! NOTE CAREFULLY !! The destructor and clear() now calls delete() on the data pointers so only pointers to data allocated via new allowed
   e.g. int* int_ptr = new int;
   or  MyClass* myClass_ptr = new MyClass;
   Using pointers returned from malloc etc will most likely crash when clear() is called.

  (c)2022 Forward Computing and Control Pty. Ltd.
  This code is not warranted to be fit for any purpose. You may only use it at your own risk.
  These modifications may be freely used for both private and commercial use subject to the included LICENSE file
  Provide this copyright is maintained.
*/

/*

  Light-weight implementation of LinkedList, appropriate for use in Arduino and other memory-critical environments.

  - Handling pointers, rather than copies of actual contained objects.
  - Up to 255 entries
  - No last element pointer
  - No sequential get-cache
  - No support for sorting

  This object consumes just 5 bytes per instance + 4 per node, making itself more appropriate for use in memory-critical
  environments. Since the class is targeted for use with small lists with up to a few dozens of entries, the optimization
  cuts are not significantly affecting the performance.

  Based on LinkedList library by ivanseidel (https://github.com/ivanseidel/LinkedList).

   Created on: 31. sij 2017.
       Author: JonnieZG
*/

#include <stddef.h>

template<class T>
struct pfodPointerListNode {
  T *data;
  pfodPointerListNode<T> *next;
};

template<typename T>
class pfodLinkedPointerList {

  protected:
    pfodPointerListNode<T> *root;
    pfodPointerListNode<T> *current; // for list tranversals
    size_t count; // the number of items in the list

  public:
    pfodLinkedPointerList();
    virtual ~pfodLinkedPointerList(); // calls clear to release all data as well as the list containers
    /*
        The size of the list
        @ret - the current number of elements in the list
    */
    virtual size_t size();
    /*
        Adds this data pointer at the front of the list
        NULL data pointers not added to list
        current iterator not changed, call getFirst() to include this new element
        @ret - true if added, else false if data pointer NULL or out of memory
    */
    virtual bool add(T*);

    /*
       Removes this data pointer
       current iterator is updated if necessary
      @ret - NULL if data pointer not found on the list, else the removed data element pointer
    */
    virtual T* remove(T*);

    /*
       Removes first element
       Use this repeatedly to clear list.
       You need to free() the data pointer returned
       current iterator is updated if necessary
      @ret - NULL if list empty, else first data element pointer
    */
    virtual T* remove();

    /*
      get the root of the list
      Sets current interator to root
      @ret - NULL at end of list, else first data element pointer
    */
    virtual T* getFirst();

    /* get next element in list
      using current interator
      @ret - NULL at end of list, else next element
    */
    virtual T* getNext();

    /*
      This is also called by the destructor when the list goes out of scope!!
      !! NOTE CAREFULLY !! The destructor and clear() now calls delete() on the data pointers so only pointers to data allocated via new allowed
      e.g. int* int_ptr = new int;
      or  MyClass* myClass_ptr = new MyClass;
      Using pointers returned from malloc etc will most likely crash when clear() is called.
    */
    virtual void clear();

};

// pfodLinkedPointerList.cpp
/*
   Modified by Matthew Ford to remove index and only use pointers and add getFirst(), getNext() iterator
   Now acts as LOFO list
   NULL data pointers not added to list
   Not longer limited to 255 elements. Only limited by available memory. Still no caching
   !! NOTE CAREFULLY !! The destructor and clear() now calls delete() on the data pointers so only pointers to data allocated via new allowed

  (c)2022 Forward Computing and Control Pty. Ltd.
  This code is not warranted to be fit for any purpose. You may only use it at your own risk.
  These modifications may be freely used for both private and commercial use subject to the included LICENSE file
  Provide this copyright is maintained.
*/

/*

  Light-weight implementation of LinkedList, appropriate for use in Arduino and other memory-critical environments.

  - Handling pointers, rather than copies of actual contained objects.
  - Up to 255 entries
  - No last element pointer
  - No sequential get-cache
  - No support for sorting

  This object consumes just 5 bytes per instance + 4 per node, making itself more appropriate for use in memory-critical
  environments. Since the class is targeted for use with small lists with up to a few dozens of entries, the optimization
  cuts are not significantly affecting the performance.

  Based on LinkedList library by ivanseidel (https://github.com/ivanseidel/LinkedList).

   Created on: 31. sij 2017.
       Author: JonnieZG
*/

// ------------ Template Implementation ------------

#include "pfodLinkedPointerList.h"

template<typename T>
pfodLinkedPointerList<T>::pfodLinkedPointerList() {
  root = NULL;
}

/*
  calls clear to release all data as well as the list containers
*/
template<typename T>
pfodLinkedPointerList<T>::~pfodLinkedPointerList() {
  clear();
}

/*
    The size of the list
    @ret - the current number of elements in the list
*/
template<typename T>
size_t pfodLinkedPointerList<T>::size() {
  return count;
}

/*
    Adds this data pointer at the front of the list
    NULL data pointers not added to list
    current iterator not changed, call getFirst() to include this new element
    @ret - true if added, else false if data pointer NULL or out of memory
*/
template<typename T>
bool pfodLinkedPointerList<T>::add(T* _t) {
  if (_t == NULL) {
    return false;
  }
  pfodPointerListNode<T> *tmp = new pfodPointerListNode<T>();
  if (tmp == NULL) {
    return false;
  }
  tmp->data = _t;
  tmp->next = root;
  root = tmp;
  count++;
  return true;
}

/*
   Removes this data pointer
   current iterator is updated if necessary so getNext() gets next element after list updated by this removal
  @ret - NULL if data pointer not found on the list, else the removed data element pointer
*/
template<typename T>
T* pfodLinkedPointerList<T>::remove(T* _t) {
  if (root == NULL) {
    return NULL;
  }
  T* rtnData = NULL;
  pfodPointerListNode<T> *toDelete = NULL;
  if (root->data == _t) {
    toDelete = root;
    root = toDelete->next;
    rtnData = toDelete->data;
    delete(toDelete);
    if (count >= 1) {
      count--;
    }
    return rtnData;
  }
  // else not root
  pfodPointerListNode<T>* lastListPtr = root;
  pfodPointerListNode<T>* listPtr = root->next;
  while (listPtr) {
    if (listPtr->data == _t) {
      // found it
      toDelete = listPtr;
      lastListPtr->next = toDelete->next;
      rtnData = toDelete->data;
      if (current == toDelete) {
        current = lastListPtr; // so getNext() returns next element in list
      }
      delete(toDelete);
      if (count >= 1) {
        count--;
      }
      return rtnData;
    }
    // else get next
    lastListPtr = listPtr;
    listPtr = listPtr->next;
  }
  return rtnData; // NULL; // null not found
}

/*
   Removes first element
   Use this repeatedly to clear list.
   You need to free() the data pointer returned
   current iterator is updated if necessary
  @ret - NULL if list empty, else first data element pointer
*/
template<typename T>
T* pfodLinkedPointerList<T>::remove() {
  if (root == NULL) {
    return NULL;
  }
  T* rtnData = NULL;
  pfodPointerListNode<T> *toDelete = NULL;
  toDelete = root;
  root = toDelete->next;
  rtnData = toDelete->data;
  delete(toDelete);
  if (count >= 1) {
    count--;
  }
  return rtnData;
}

/*
    get the root of the list
    Sets current interator to root
    @ret - NULL at end of list, else first data element pointer
*/
template<typename T>
T* pfodLinkedPointerList<T>::getFirst() {
  current = root;
  if (current) {
    return (current->data);
  } // else
  return NULL;
}

/* get next element in list
  using current interator
  @ret - NULL at end of list, else next element
*/
template<typename T>
T* pfodLinkedPointerList<T>::getNext() {
  if (current == NULL) {
    return NULL;
  } // else
  current = current->next;
  if (current) {
    return (current->data);
  } // else
  return NULL;
}

/*
  This is also called by the destructor when the list goes out of scope!!
  !! NOTE CAREFULLY !! The destructor and clear() now calls delete() on the data pointers so only pointers to data allocated via new allowed
  e.g. int* int_ptr = new int;
  or  MyClass* myClass_ptr = new MyClass;
  Using pointers returned from malloc etc will most likely crash when clear() is called.
*/
template<typename T>
void pfodLinkedPointerList<T>::clear() {
  pfodPointerListNode<T>* tmp;
  while (root != NULL) {
    tmp = root;
    root = root->next;
    delete tmp->data; // must be class created with new ...
    delete tmp;
  }
  count = 0;
}

#endif /* PFOD_LINKEDPOINTERLIST_H_ */
