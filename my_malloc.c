#include "my_malloc.h"
#include <limits.h>
#include <pthread.h>

typedef struct free_list_node_t {
  void *addressForUser;
  size_t size; // Excluding the node itself;
  struct free_list_node_t *next;
  struct free_list_node_t *pre;
} free_node;

static free_node *head = NULL;
unsigned long data_segment_size = 0;
unsigned long occupied_space_size = 0;

unsigned long get_data_segment_size() {
  return data_segment_size;
}

unsigned long get_data_segment_free_space_size() {
  return data_segment_size - occupied_space_size;
}

void *splitButSmallSpace(free_node *cur, size_t size) {
  //  printf("Can not split! size in call is %lu and cur->size is %lu\n", (unsigned long) size,
  //   (unsigned long) cur->size);
  if (cur == head) {
    if (cur->next != NULL) {
      cur->next->pre = NULL;
    }
    head = cur->next;
  } else {
    if (cur->pre != NULL) {
      cur->pre->next = cur->next;
    }
    if (cur->next != NULL) {
      cur->next->pre = cur->pre;
    }
  }
  cur->next = NULL;
  cur->pre = NULL;
  occupied_space_size += cur->size;
  return cur->addressForUser;
}

void *splitEnoughSpace(free_node *cur, size_t size) {
  // printf("Can split! size in call is %lu and cur->size is %lu\n", (unsigned long)
  // size, (unsigned long) cur->size);
  free_node *newOccupiedNode = (free_node *) (cur->addressForUser + cur->size
					      - size - sizeof(free_node));
  newOccupiedNode->pre = NULL;
  newOccupiedNode->next = NULL;
  newOccupiedNode->size = size;
  newOccupiedNode->addressForUser = (void *) newOccupiedNode + sizeof(free_node);
  cur->size -= size + sizeof(free_node);
  occupied_space_size += size;
  return newOccupiedNode->addressForUser;
}

void *splitAndMalloc(free_node *cur, size_t size) {
  if (cur->size <= size + sizeof(free_node)) { // Discard the so small part
    return splitButSmallSpace(cur, size);
  } else {
    return splitEnoughSpace(cur, size);
  }
}

void *incSizeAndMalloc(size_t size) {
  // printf("incSize for %lu", (unsigned long) size);
  // printf("program break now is %p\n", sbrk(0));
  free_node *newOccupiedNode = (free_node *) (sbrk(size + sizeof(free_node)));
  newOccupiedNode->next = NULL;
  newOccupiedNode->pre = NULL;
  newOccupiedNode->size = size;
  newOccupiedNode->addressForUser = (void *) newOccupiedNode + sizeof(free_node);
  data_segment_size += sizeof(free_node) + size;
  occupied_space_size += size;
  return newOccupiedNode->addressForUser;
}


void traverseToFindtoFree(free_node *toFree) {
  free_node *cur = head;
  while (cur != NULL) {
    // printf("Loooooooop");
    if (cur->next != NULL) {
      if ((unsigned long)cur->next > (unsigned long)toFree) {
	break;
      } else {
	cur = cur->next;
      }
    } else {
      break;
    }
  }
  if (cur->next == NULL) {
    cur->next = toFree;
    toFree->pre = cur;
    toFree->next = NULL;
  } else {
    toFree->next = cur->next;
    toFree->pre = cur;
    cur->next->pre = toFree;
    cur->next = toFree;
  }
}

void generalFree(free_node *toFree) {
  occupied_space_size -= toFree->size;
  if (head == NULL) {
    head = toFree;
    head->next = NULL;
    head->pre = NULL;
    return;
  }
  if (head > toFree) {
    toFree->next = head;
    toFree->pre = NULL;
    head->pre = toFree;
    head = toFree;
    return;
  }
  // Now toFree is behind head
  traverseToFindtoFree(toFree);
}

void merge(free_node *toMerge) {
  // Here head could not be NULL
  /*Here I choose to merge toMerge's pre node and the possible toMerge's pre node's
   next node*/
  if (toMerge->pre != NULL) { // toMerge is not the head
    if (toMerge->pre->addressForUser + toMerge->pre->size == (void *) toMerge) {
      toMerge->pre->size += toMerge->size + sizeof(free_node);
      toMerge->pre->next = toMerge->next;
      if (toMerge->next != NULL) {
	toMerge->next->pre = toMerge->pre;
      }
      toMerge = toMerge->pre;
    }
  }
  // merge operation above may
  if (toMerge->next != NULL) {
    if (toMerge->addressForUser + toMerge->size == (void *) toMerge->next) {
      toMerge->size += toMerge->next->size + sizeof(free_node);
      toMerge->next = toMerge->next->next;
      if (toMerge->next != NULL) {
	toMerge->next->pre = toMerge;
      }
    }
  }
}

pthread_mutex_t mutex;

void *ts_malloc_lock(size_t size) {
  free_node *cur = head;
  void *res = NULL;
  free_node *minAvaiNode = NULL;
  unsigned long min = INT_MAX;
  pthread_mutex_init(&mutex, NULL);
  pthread_mutex_lock(&mutex);
  while (cur != NULL) {
    // printf("XXXXXXXXXXX");
    if (cur->size > size) {
      if (cur->size < min) {
	min = cur->size;
	minAvaiNode = cur;
      }
    } else if (cur->size == size) {
      minAvaiNode = cur;
      break;
    }
    cur = cur->next;
  }
  if (minAvaiNode == NULL) {
    res = incSizeAndMalloc(size);
  } else {
    res = splitAndMalloc(minAvaiNode, size);
  }
  pthread_mutex_unlock(&mutex);
  return res;
}

void ts_free_lock(void *ptr) {
  free_node *toFree = (free_node *) ((char *) ptr - sizeof(free_node));
  generalFree(toFree);
  merge(toFree);
}


void *ts_malloc_nolock(size_t size) {
  free_node *cur = head;
  void *res = NULL;
  free_node *minAvaiNode = NULL;
  unsigned long min = INT_MAX;
  while (cur != NULL) {
    // printf("XXXXXXXXXXX");
    if (cur->size > size) {
      if (cur->size < min) {
	min = cur->size;
	minAvaiNode = cur;
      }
    } else if (cur->size == size) {
      minAvaiNode = cur;
      break;
    }
    cur = cur->next;
  }
  if (minAvaiNode == NULL) {
    res = incSizeAndMalloc(size);
  } else {
    res = splitAndMalloc(minAvaiNode, size);
  }
  return res;
}


void ts_free_nolock(void *ptr) {
  free_node *toFree = (free_node *) ((char *) ptr - sizeof(free_node));
  generalFree(toFree);
  merge(toFree);
}
