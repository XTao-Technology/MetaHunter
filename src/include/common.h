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
#ifndef __COMMON_H__
#define __COMMON_H__

enum _xt_boolean
{
	_xt_false = 0,
	_xt_true = 1
};

typedef enum _xt_boolean xt_boolean_t;

#ifdef __GNUC__
#define atomic_add_and_fetch __sync_add_and_fetch
#define atomic_sub_and_fetch __sync_sub_and_fetch
#define atomic_fetch_and_add __sync_fetch_and_add
#define atomic_fetch_and_sub __sync_fetch_and_sub
#define atomic_inc_and_fetch(var) atomic_add_and_fetch(var, 1)
#define atomic_dec_and_fetch(var) atomic_sub_and_fetch(var, 1)
#define atomic_fetch_and_inc(var) atomic_fetch_and_add(var, 1)
#define atomic_fetch_and_dec(var) atomic_fetch_and_sub(var, 1)
#define atomic_add(var,v)       (void)atomic_add_and_fetch(var,v)
#define atomic_sub(var,v)       (void)atomic_sub_and_fetch(var,v)
#define atomic_inc(var)         (void)atomic_inc_and_fetch(var)
#define atomic_dec(var)         (void)atomic_dec_and_fetch(var)
#else
#error "Your compiler is not supported!"
#endif


#endif
