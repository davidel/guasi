/*    Copyright 2023 Davide Libenzi
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 * 
 */


#if !defined(_GUASI_DLISTS_H)
#define _GUASI_DLISTS_H


#define DLIST_ENTRY(p, t, m) ((t *)((char *)(p) - (unsigned long)(&((t *) 0)->m)))


struct dlist_head {
	struct dlist_head *next, *prev;
};



static inline void dlist_init_head(struct dlist_head *p) {

	p->prev = p->next = p;
}

static inline void dlist_del(struct dlist_head *p) {
	struct dlist_head *prev = p->prev, *next = p->next;

	next->prev = prev;
	prev->next = next;
}

static inline void dlist_add(struct dlist_head *itm,
			     struct dlist_head *prev,
			     struct dlist_head *next) {

	next->prev = itm;
	itm->next = next;
	itm->prev = prev;
	prev->next = itm;
}

static inline void dlist_addh(struct dlist_head *itm, struct dlist_head *head) {

	dlist_add(itm, head, head->next);
}

static inline void dlist_addt(struct dlist_head *itm, struct dlist_head *head) {

	dlist_add(itm, head->prev, head);
}

static inline void dlist_splice(struct dlist_head *list, struct dlist_head *head) {
	struct dlist_head *first = list->next;

	if (first != list) {
		struct dlist_head *last = list->prev;
		struct dlist_head *at = head->next;
		first->prev = head;
		head->next = first;
		last->next = at;
		at->prev = last;
	}
}

static inline struct dlist_head *dlist_first(struct dlist_head *head) {

	return head->next != head ? head->next: NULL;
}

static inline struct dlist_head *dlist_next(struct dlist_head *itm,
					    struct dlist_head *head) {

	return itm->next != head ? itm->next: NULL;
}

static inline struct dlist_head *dlist_last(struct dlist_head *head) {

	return head->prev != head ? head->prev: NULL;
}

static inline struct dlist_head *dlist_prev(struct dlist_head *itm,
					    struct dlist_head *head) {

	return itm->prev != head ? itm->prev: NULL;
}


#endif

