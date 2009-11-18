#ifndef INC_tw_eventq_h
#define INC_tw_eventq_h

#define ROSS_DEBUG 0

#include <ross.h>

INLINE(void)
tw_eventq_debug(tw_eventq * q)
{
#if ROSS_DEBUG
  tw_event	*next;
  tw_event	*last;

  int		 cnt;

  cnt = 0;
  next = q->head;
  last = NULL;

  while(next)
    {
      cnt++;

      if(next->prev != last)
	tw_error(TW_LOC, "Prev pointer not correct!");

      last = next;
      next = next->next;
    }

  if(q->tail != last)
    tw_error(TW_LOC, "Tail pointer not correct!");

  if(cnt != q->size)
    tw_error(TW_LOC, "Size not correct!");	
#endif
}

INLINE(void)
tw_eventq_push_list(tw_eventq * q, tw_event * h, tw_event * t, int cnt)
{
  tw_event	*e;
  tw_event	*cev;
  tw_event	*next;

  tw_eventq_debug(q);

  t->next = q->head;

  if(q->head)
    q->head->prev = t;

  q->head = h;
  q->head->prev = NULL;

  if(!q->tail)
    q->tail = t;

  // iterate through list to collect sent events
  t = t->next;
  for(e = h; e != t; e = e->next)
    {
      if(e->caused_by_me)
	{
	  cev = next = e->caused_by_me;

	  while(cev)
	    {
	      next = cev->cause_next;
	      cev->cause_next = NULL;

	      if(cev->state.owner == TW_pe_sevent_q)
		tw_event_free(cev->src_lp->pe, cev);

	      cev = next;
	    }

	  e->caused_by_me = NULL;
	  e->state.owner = TW_pe_free_q;
	}
    }
  q->size += cnt;

  tw_eventq_debug(q);
}

INLINE(void)
tw_eventq_fossil_collect(tw_eventq *q, tw_pe *pe)
{
  tw_stime gvt = pe->GVT;

  tw_event *h = q->head;
  tw_event *t = q->tail;

  int	 cnt;

  /* Nothing to collect from this event list? */
  if (!t || t->recv_ts >= gvt)
    return;

  if (h->recv_ts < gvt)
    {
      /* Everything in the queue can be collected */
      tw_eventq_push_list(&pe->free_q, h, t, q->size);
      q->head = q->tail = NULL;
      q->size = 0;
    } else {
    /* Only some of the list can be collected.  We'll wind up
     * with at least one event being collected and at least
     * another event staying behind in the eventq structure so
     * we can really optimize this list splicing operation for
     * these conditions.
     */
    tw_event *n;

    /* Search the leading part of the list... */
    for (h = t->prev, cnt = 1; h && h->recv_ts < gvt; cnt++)
      h = h->prev;

    /* t isn't eligible for collection; its the new head */
    n = h;

    /* Back up one cell, we overshot where to cut the list */
    h = h->next;

    /* Cut h..t out of the event queue */
    q->tail = n;
    n->next = NULL;
    q->size -= cnt;

    /* Free h..t (inclusive) */
    tw_eventq_push_list(&pe->free_q, h, t, cnt);
  }
}

INLINE(void)
tw_eventq_alloc(tw_eventq * q, unsigned int cnt)
{
  tw_event *event;
  size_t event_len;
  size_t align;

  /* Construct a linked list of free events.  We allocate
   * the events such that they look like this in memory:
   *
   *  ------------------
   *  | tw_event       |
   *  | user_data      |
   *  ------------------
   *  | tw_event       |
   *  | user_data      |
   *  ------------------
   *  ......
   *  ------------------
   */

  align = max(sizeof(double), sizeof(void*));
  event_len = sizeof(tw_event) + g_tw_msg_sz;
  if (event_len & (align - 1))
    {
      event_len += align - (event_len & (align - 1));
      //tw_error(TW_LOC, "REALIGNING EVENT MEMORY!\n");
    }
  g_tw_event_msg_sz = event_len;

  // compute number of events needed for the network.
  g_tw_gvt_threshold = (int) ceil(g_tw_net_device_size / g_tw_event_msg_sz);
  g_tw_gvt_threshold = g_tw_net_device_size;
  g_tw_events_per_pe += g_tw_gvt_threshold;
  cnt += g_tw_gvt_threshold;

  q->head = event = tw_calloc(TW_LOC, "events", event_len, cnt);
  q->size = cnt;

  while (--cnt) {
    event->state.owner = TW_pe_free_q;
    event->prev = (tw_event *) (((char *)event) - event_len);
    event->next = (tw_event *) (((char *)event) + event_len);
    event = event->next;
  }

  event->state.owner = TW_pe_free_q;
  event->prev = (tw_event *) (((char *)event) - event_len);
  q->head->prev = event->next = NULL;
  q->tail = event;
}

INLINE(void)
tw_eventq_push(tw_eventq *q, tw_event *e)
{
  tw_event *t = q->tail;

  tw_eventq_debug(q);

  e->next = NULL;
  e->prev = t;
  if (t)
    t->next = e;
  else
    q->head = e;

  q->tail = e;
  q->size++;

  tw_eventq_debug(q);
}

INLINE(tw_event *)
tw_eventq_peek(tw_eventq *q)
{
  return q->tail;
}

INLINE(tw_event *)
tw_eventq_pop(tw_eventq * q)
{
  tw_event *t = q->tail;
  tw_event *p;

  tw_eventq_debug(q);

  if(!t)
    return NULL;

  p = t->prev;

  if (p)
    p->next = NULL;
  else
    q->head = NULL;

  q->tail = p;
  q->size--;

  t->next = t->prev = NULL;

  tw_eventq_debug(q);

  return t;
}

INLINE(void)
tw_eventq_unshift(tw_eventq *q, tw_event *e)
{
  tw_event *h = q->head;

  tw_eventq_debug(q);

  e->prev = NULL;
  e->next = h;

  if (h)
    h->prev = e;
  else
    q->tail = e;

  q->head = e;
  q->size++;

  tw_eventq_debug(q);
}

INLINE(tw_event *)
tw_eventq_peek_head(tw_eventq *q)
{
  return q->head;
}

INLINE(tw_event *)
tw_eventq_shift(tw_eventq *q)
{
  tw_event *h = q->head;
  tw_event *n;

  tw_eventq_debug(q);

  if(!h)
    return NULL;

  n = h->next;

  if (n)
    n->prev = NULL;
  else
    q->tail = NULL;

  q->head = n;
  q->size--;

  h->next = h->prev = NULL;

  tw_eventq_debug(q);

  return h;
}

INLINE(void)
tw_eventq_delete_any(tw_eventq *q, tw_event *e)
{
  tw_event *p = e->prev;
  tw_event *n = e->next;

  tw_eventq_debug(q);

  if (p)
    p->next = n;
  else
    q->head = n;

  if (n)
    n->prev = p;
  else
    q->tail = p;

  e->next = e->prev = NULL;
  q->size--;

  tw_eventq_debug(q);
}

INLINE(tw_event *)
tw_eventq_pop_list(tw_eventq * q)
{
  tw_event	*h = q->head;

  q->size = 0;
  q->head = q->tail = NULL;

  return h;
}

/*
 * The purpose of this function is to be able to remove some
 * part of a list.. could be all of list, from head to some inner
 * buffer, or from some inner buffer to tail.  I only care about the
 * last case.. 
 */
INLINE(void)
tw_eventq_splice(tw_eventq * q, tw_event * h, tw_event * t, int cnt)
{
  tw_eventq_debug(q);

  if(h == q->head && t == q->tail)
    {
      q->size = 0;
      q->head = q->tail = NULL;
      return;
    }

  if(h == q->head)
    q->head = t->next;
  else
    h->prev->next = t->next;

  if(t == q->tail)
    q->tail = h->prev;
  else
    t->next->prev = h->prev;

  q->size -= cnt;

  tw_eventq_debug(q);
}

#endif
