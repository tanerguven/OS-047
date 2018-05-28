/*
 * malloc-free doubly linked list
 * Copyright (C) 2012 Taner Guven <tanerguven@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _LIST2_H_
#define _LIST2_H_

#ifndef offsetof
#define offsetof(TYPE, MEMBER) __builtin_offsetof (TYPE, MEMBER)
#endif

#define __DEFINE_LIST(ln)										\
	typedef struct __li_##ln##_node {							\
		struct __li_##ln##_node *prev;							\
		struct __li_##ln##_node *next;							\
		struct __li_##ln *list;									\
	} li_##ln##_node;											\
	/* */														\
	typedef struct __li_##ln {									\
		unsigned int size;										\
		li_##ln##_node __;										\
	} li_##ln;													\
	/* */														\
	static inline void li_##ln##_init(li_##ln *l) {				\
		l->size = 0;											\
		l->__.prev = l->__.next = &l->__;						\
		l->__.list = l;											\
	}															\
	/* */														\
	static inline li_##ln##_node *li_##ln##_begin(li_##ln *l) {	\
		return l->__.next;										\
	}															\
	/* */														\
	static inline li_##ln##_node *li_##ln##_end(li_##ln *l) {	\
		return &l->__;											\
	}															\
	/* */


#define __DEFINE_LIST_INSERT(ln)								\
	static inline int											\
	li_##ln##_insert(li_##ln##_node *pos, li_##ln##_node *n) {	\
		if (n->list)											\
			return -1;											\
		n->list = pos->list;									\
		n->next = pos;											\
		n->prev = pos->prev;									\
		n->prev->next = n;										\
		pos->prev = n;											\
		n->list->size++;										\
		return 0;												\
	}															\
	/* */														\
	static inline int											\
	li_##ln##_push_front(li_##ln *l, li_##ln##_node *n) {		\
		return li_##ln##_insert(li_##ln##_begin(l), n);			\
	}															\
	/* */														\
	static inline int											\
	li_##ln##_push_back(li_##ln *l, li_##ln##_node *n) {		\
		return li_##ln##_insert(li_##ln##_end(l), n);			\
	}															\
	/* */


#define __DEFINE_LIST_ERASE(ln)										\
	static inline li_##ln##_node *									\
	li_##ln##_erase(li_##ln##_node *pos) {							\
		li_##ln##_node *pos_next = pos->next;						\
		li_##ln *pos_list = pos->list;								\
		if (pos == &pos_list->__)									\
			return NULL;											\
		pos->prev->next = pos_next;									\
		pos_next->prev = pos->prev;									\
		pos_list->size--;											\
		pos->list = NULL;											\
		pos->prev = pos->next = NULL;								\
		return pos_next;											\
	}																\
	/* */															\
	static inline int li_##ln##_pop_front(li_##ln *l) {				\
		return -(li_##ln##_erase(li_##ln##_begin(l))==NULL);		\
	}																\
	/* */															\
	static inline int li_##ln##_pop_back(li_##ln *l) {				\
		return -(li_##ln##_erase(li_##ln##_end(l)->prev)==NULL);	\
	}																\
	/* */


#define DEFINE_LIST_OFFSET(ln,type_t,nn)						\
	static inline type_t* li_##ln##_value(li_##ln##_node *n) {	\
		return (type_t*)((char*)n - offsetof(type_t, nn));		\
	}															\
	static inline type_t* li_##ln##_front(li_##ln *l) {			\
		return li_##ln##_value(l->__.next);						\
	}															\
	static inline type_t* li_##ln##_back(li_##ln *l) {			\
		return li_##ln##_value(l->__.prev);						\
	}															\
	/* */														\
	static inline type_t* li_##ln##_next(li_##ln##_node *n) {	\
		if (n->next == li_##ln##_end(n->list))					\
			return NULL;										\
		return li_##ln##_value(n->next);						\
	}															\
	static inline type_t* li_##ln##_prev(li_##ln##_node *n) {	\
		if (n->prev == li_##ln##_end(n->list))					\
			return NULL;										\
		return li_##ln##_value(n->prev);						\
	}															\
	/* */


#define DEFINE_LIST(ln)							\
	__DEFINE_LIST(ln)							\
	__DEFINE_LIST_INSERT(ln)					\
	__DEFINE_LIST_ERASE(ln)

#endif /* _LIST2_H_ */
