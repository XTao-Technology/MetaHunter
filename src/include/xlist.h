/*
 * Copyright (c) 2016 XTAO Technology <www.xtaotech.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _XLIST_H
#define _XLIST_H


struct xlist_head {
	struct xlist_head *next;
	struct xlist_head *prev;
};


#define INIT_XLIST_HEAD(head) do {			\
		(head)->next = (head)->prev = head;	\
	} while (0)


static inline void
xlist_add (struct xlist_head *new, struct xlist_head *head)
{
	new->prev = head;
	new->next = head->next;
	head->next->prev = new;
	head->next = new;

}


static inline void
xlist_add_tail (struct xlist_head *new, struct xlist_head *head)
{
	new->next = head;
	new->prev = head->prev;

	head->prev->next = new;
	head->prev = new;
}


static inline void
xlist_del (struct xlist_head *old)
{
	old->prev->next = old->next;
	old->next->prev = old->prev;

	old->next = (void *)0xbabebabe;
	old->prev = (void *)0xcafecafe;
}


static inline void
xlist_del_init (struct xlist_head *old)
{
	old->prev->next = old->next;
	old->next->prev = old->prev;

	old->next = old;
	old->prev = old;
}


static inline void
xlist_move (struct xlist_head *list, struct xlist_head *head)
{
	xlist_del (list);
	xlist_add (list, head);
}


static inline void
xlist_move_tail (struct xlist_head *list, struct xlist_head *head)
{
	xlist_del (list);
	xlist_add_tail (list, head);
}

/*
 * get tail will return the valid tail entry in a list that is not
 * empty, if empty, it will return null
 */
static inline struct xlist_head *
xlist_get_tail (struct xlist_head *list)
{
	if ((list->prev) == list)
		return 0;

	return list->prev;
}


static inline int
xlist_empty (struct xlist_head *head)
{
	return (head->next == head);
}


static inline void
__xlist_splice (struct xlist_head *list, struct xlist_head *head)
{
	(list->prev)->next = (head->next);
	(head->next)->prev = (list->prev);

	(head)->next = (list->next);
	(list->next)->prev = (head);
}


static inline void
xlist_splice (struct xlist_head *list, struct xlist_head *head)
{
	if (xlist_empty (list))
		return;

	__xlist_splice (list, head);
}


/* Splice moves @list to the head of the list at @head. */
static inline void
xlist_splice_init (struct xlist_head *list, struct xlist_head *head)
{
	if (xlist_empty (list))
		return;

	__xlist_splice (list, head);
	INIT_XLIST_HEAD (list);
}


static inline void
__xlist_append (struct xlist_head *list, struct xlist_head *head)
{
	(head->prev)->next = (list->next);
        (list->next)->prev = (head->prev);
        (head->prev) = (list->prev);
        (list->prev)->next = head;
}


static inline void
xlist_append (struct xlist_head *list, struct xlist_head *head)
{
	if (xlist_empty (list))
		return;

	__xlist_append (list, head);
}


/* Append moves @list to the end of @head */
static inline void
xlist_append_init (struct xlist_head *list, struct xlist_head *head)
{
	if (xlist_empty (list))
		return;

	__xlist_append (list, head);
	INIT_XLIST_HEAD (list);
}


#define xlist_entry(ptr, type, member)					\
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))


#define xlist_for_each(pos, head)                                        \
	for (pos = (head)->next; pos != (head); pos = pos->next)


#define xlist_for_each_entry(pos, head, member)				\
	for (pos = xlist_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = xlist_entry(pos->member.next, typeof(*pos), member))


#define xlist_for_each_entry_safe(pos, n, head, member)			\
	for (pos = xlist_entry((head)->next, typeof(*pos), member),	\
		n = xlist_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = xlist_entry(n->member.next, typeof(*n), member))


#define xlist_for_each_entry_reverse(pos, head, member)                  \
        for (pos = xlist_entry((head)->prev, typeof(*pos), member);      \
             &pos->member != (head);    \
             pos = xlist_entry(pos->member.prev, typeof(*pos), member))


#endif /* _XLIST_H */
