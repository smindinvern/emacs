/* Execution of byte code produced by bytecomp.el.
   Copyright (C) 1985-1988, 1993, 2000-2016 Free Software Foundation,
   Inc.

This file is part of GNU Emacs.

GNU Emacs is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at
your option) any later version.

GNU Emacs is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Emacs.  If not, see <http://www.gnu.org/licenses/>.  */

/*
hacked on by jwz@lucid.com 17-jun-91
  o  added a compile-time switch to turn on simple sanity checking;
  o  put back the obsolete byte-codes for error-detection;
  o  added a new instruction, unbind_all, which I will use for
     tail-recursion elimination;
  o  made temp_output_buffer_show be called with the right number
     of args;
  o  made the new bytecodes be called with args in the right order;
  o  added metering support.

by Hallvard:
  o  added relative jump instructions;
  o  all conditionals now only do QUIT if they jump.
 */

#include <config.h>

#include <jit.h>

#include "lisp.h"
#include "blockinput.h"
#include "character.h"
#include "buffer.h"
#include "keyboard.h"
#include "syntax.h"
#include "window.h"

#ifdef CHECK_FRAME_FONT
#include "frame.h"
#include "xterm.h"
#endif

/*
 * define BYTE_CODE_SAFE to enable some minor sanity checking (useful for
 * debugging the byte compiler...)
 *
 * define BYTE_CODE_METER to enable generation of a byte-op usage histogram.
 */
/* #define BYTE_CODE_SAFE */
/* #define BYTE_CODE_METER */

/* If BYTE_CODE_THREADED is defined, then the interpreter will be
   indirect threaded, using GCC's computed goto extension.  This code,
   as currently implemented, is incompatible with BYTE_CODE_SAFE and
   BYTE_CODE_METER.  */
#if (defined __GNUC__ && !defined __STRICT_ANSI__ \
     && !defined BYTE_CODE_SAFE && !defined BYTE_CODE_METER)
#define BYTE_CODE_THREADED
#endif


#ifdef BYTE_CODE_METER

#define METER_2(code1, code2) AREF (AREF (Vbyte_code_meter, code1), code2)
#define METER_1(code) METER_2 (0, code)

#define METER_CODE(last_code, this_code)				\
{									\
  if (byte_metering_on)							\
    {									\
      if (XFASTINT (METER_1 (this_code)) < MOST_POSITIVE_FIXNUM)	\
        XSETFASTINT (METER_1 (this_code),				\
		     XFASTINT (METER_1 (this_code)) + 1);		\
      if (last_code							\
	  && (XFASTINT (METER_2 (last_code, this_code))			\
	      < MOST_POSITIVE_FIXNUM))					\
        XSETFASTINT (METER_2 (last_code, this_code),			\
		     XFASTINT (METER_2 (last_code, this_code)) + 1);	\
    }									\
}

#endif /* BYTE_CODE_METER */


/*  Byte codes: */

#define BYTE_CODES							\
DEFINE (Bstack_ref, 0) /* Actually, Bstack_ref+0 is not implemented: use dup.  */ \
DEFINE (Bstack_ref1, 1)							\
DEFINE (Bstack_ref2, 2)							\
DEFINE (Bstack_ref3, 3)							\
DEFINE (Bstack_ref4, 4)							\
DEFINE (Bstack_ref5, 5)							\
DEFINE (Bstack_ref6, 6)							\
DEFINE (Bstack_ref7, 7)							\
DEFINE (Bvarref, 010)							\
DEFINE (Bvarref1, 011)							\
DEFINE (Bvarref2, 012)							\
DEFINE (Bvarref3, 013)							\
DEFINE (Bvarref4, 014)							\
DEFINE (Bvarref5, 015)							\
DEFINE (Bvarref6, 016)							\
DEFINE (Bvarref7, 017)							\
DEFINE (Bvarset, 020)							\
DEFINE (Bvarset1, 021)							\
DEFINE (Bvarset2, 022)							\
DEFINE (Bvarset3, 023)							\
DEFINE (Bvarset4, 024)							\
DEFINE (Bvarset5, 025)							\
DEFINE (Bvarset6, 026)							\
DEFINE (Bvarset7, 027)							\
DEFINE (Bvarbind, 030)							\
DEFINE (Bvarbind1, 031)							\
DEFINE (Bvarbind2, 032)							\
DEFINE (Bvarbind3, 033)							\
DEFINE (Bvarbind4, 034)							\
DEFINE (Bvarbind5, 035)							\
DEFINE (Bvarbind6, 036)							\
DEFINE (Bvarbind7, 037)							\
DEFINE (Bcall, 040)							\
DEFINE (Bcall1, 041)							\
DEFINE (Bcall2, 042)							\
DEFINE (Bcall3, 043)							\
DEFINE (Bcall4, 044)							\
DEFINE (Bcall5, 045)							\
DEFINE (Bcall6, 046)							\
DEFINE (Bcall7, 047)							\
DEFINE (Bunbind, 050)							\
DEFINE (Bunbind1, 051)							\
DEFINE (Bunbind2, 052)							\
DEFINE (Bunbind3, 053)							\
DEFINE (Bunbind4, 054)							\
DEFINE (Bunbind5, 055)							\
DEFINE (Bunbind6, 056)							\
DEFINE (Bunbind7, 057)							\
									\
DEFINE (Bpophandler, 060)						\
DEFINE (Bpushconditioncase, 061)					\
DEFINE (Bpushcatch, 062)						\
									\
DEFINE (Bnth, 070)							\
DEFINE (Bsymbolp, 071)							\
DEFINE (Bconsp, 072)							\
DEFINE (Bstringp, 073)							\
DEFINE (Blistp, 074)							\
DEFINE (Beq, 075)							\
DEFINE (Bmemq, 076)							\
DEFINE (Bnot, 077)							\
DEFINE (Bcar, 0100)							\
DEFINE (Bcdr, 0101)							\
DEFINE (Bcons, 0102)							\
DEFINE (Blist1, 0103)							\
DEFINE (Blist2, 0104)							\
DEFINE (Blist3, 0105)							\
DEFINE (Blist4, 0106)							\
DEFINE (Blength, 0107)							\
DEFINE (Baref, 0110)							\
DEFINE (Baset, 0111)							\
DEFINE (Bsymbol_value, 0112)						\
DEFINE (Bsymbol_function, 0113)						\
DEFINE (Bset, 0114)							\
DEFINE (Bfset, 0115)							\
DEFINE (Bget, 0116)							\
DEFINE (Bsubstring, 0117)						\
DEFINE (Bconcat2, 0120)							\
DEFINE (Bconcat3, 0121)							\
DEFINE (Bconcat4, 0122)							\
DEFINE (Bsub1, 0123)							\
DEFINE (Badd1, 0124)							\
DEFINE (Beqlsign, 0125)							\
DEFINE (Bgtr, 0126)							\
DEFINE (Blss, 0127)							\
DEFINE (Bleq, 0130)							\
DEFINE (Bgeq, 0131)							\
DEFINE (Bdiff, 0132)							\
DEFINE (Bnegate, 0133)							\
DEFINE (Bplus, 0134)							\
DEFINE (Bmax, 0135)							\
DEFINE (Bmin, 0136)							\
DEFINE (Bmult, 0137)							\
									\
DEFINE (Bpoint, 0140)							\
/* Was Bmark in v17.  */						\
DEFINE (Bsave_current_buffer, 0141) /* Obsolete.  */			\
DEFINE (Bgoto_char, 0142)						\
DEFINE (Binsert, 0143)							\
DEFINE (Bpoint_max, 0144)						\
DEFINE (Bpoint_min, 0145)						\
DEFINE (Bchar_after, 0146)						\
DEFINE (Bfollowing_char, 0147)						\
DEFINE (Bpreceding_char, 0150)						\
DEFINE (Bcurrent_column, 0151)						\
DEFINE (Bindent_to, 0152)						\
DEFINE (Beolp, 0154)							\
DEFINE (Beobp, 0155)							\
DEFINE (Bbolp, 0156)							\
DEFINE (Bbobp, 0157)							\
DEFINE (Bcurrent_buffer, 0160)						\
DEFINE (Bset_buffer, 0161)						\
DEFINE (Bsave_current_buffer_1, 0162) /* Replacing Bsave_current_buffer.  */ \
DEFINE (Binteractive_p, 0164) /* Obsolete since Emacs-24.1.  */		\
									\
DEFINE (Bforward_char, 0165)						\
DEFINE (Bforward_word, 0166)						\
DEFINE (Bskip_chars_forward, 0167)					\
DEFINE (Bskip_chars_backward, 0170)					\
DEFINE (Bforward_line, 0171)						\
DEFINE (Bchar_syntax, 0172)						\
DEFINE (Bbuffer_substring, 0173)					\
DEFINE (Bdelete_region, 0174)						\
DEFINE (Bnarrow_to_region, 0175)					\
DEFINE (Bwiden, 0176)							\
DEFINE (Bend_of_line, 0177)						\
									\
DEFINE (Bconstant2, 0201)						\
DEFINE (Bgoto, 0202)							\
DEFINE (Bgotoifnil, 0203)						\
DEFINE (Bgotoifnonnil, 0204)						\
DEFINE (Bgotoifnilelsepop, 0205)					\
DEFINE (Bgotoifnonnilelsepop, 0206)					\
DEFINE (Breturn, 0207)							\
DEFINE (Bdiscard, 0210)							\
DEFINE (Bdup, 0211)							\
									\
DEFINE (Bsave_excursion, 0212)						\
DEFINE (Bsave_window_excursion, 0213) /* Obsolete since Emacs-24.1.  */	\
DEFINE (Bsave_restriction, 0214)					\
DEFINE (Bcatch, 0215)							\
									\
DEFINE (Bunwind_protect, 0216)						\
DEFINE (Bcondition_case, 0217)						\
DEFINE (Btemp_output_buffer_setup, 0220) /* Obsolete since Emacs-24.1.  */ \
DEFINE (Btemp_output_buffer_show, 0221)  /* Obsolete since Emacs-24.1.  */ \
									\
DEFINE (Bunbind_all, 0222)	/* Obsolete.  Never used.  */		\
									\
DEFINE (Bset_marker, 0223)						\
DEFINE (Bmatch_beginning, 0224)						\
DEFINE (Bmatch_end, 0225)						\
DEFINE (Bupcase, 0226)							\
DEFINE (Bdowncase, 0227)						\
									\
DEFINE (Bstringeqlsign, 0230)						\
DEFINE (Bstringlss, 0231)						\
DEFINE (Bequal, 0232)							\
DEFINE (Bnthcdr, 0233)							\
DEFINE (Belt, 0234)							\
DEFINE (Bmember, 0235)							\
DEFINE (Bassq, 0236)							\
DEFINE (Bnreverse, 0237)						\
DEFINE (Bsetcar, 0240)							\
DEFINE (Bsetcdr, 0241)							\
DEFINE (Bcar_safe, 0242)						\
DEFINE (Bcdr_safe, 0243)						\
DEFINE (Bnconc, 0244)							\
DEFINE (Bquo, 0245)							\
DEFINE (Brem, 0246)							\
DEFINE (Bnumberp, 0247)							\
DEFINE (Bintegerp, 0250)						\
									\
DEFINE (BRgoto, 0252)							\
DEFINE (BRgotoifnil, 0253)						\
DEFINE (BRgotoifnonnil, 0254)						\
DEFINE (BRgotoifnilelsepop, 0255)					\
DEFINE (BRgotoifnonnilelsepop, 0256)					\
									\
DEFINE (BlistN, 0257)							\
DEFINE (BconcatN, 0260)							\
DEFINE (BinsertN, 0261)							\
									\
/* Bstack_ref is code 0.  */						\
DEFINE (Bstack_set,  0262)						\
DEFINE (Bstack_set2, 0263)						\
DEFINE (BdiscardN,   0266)						\
									\
DEFINE (Bconstant, 0300)

enum byte_code_op
{
#define DEFINE(name, value) name = value,
    BYTE_CODES
#undef DEFINE

#ifdef BYTE_CODE_SAFE
    Bscan_buffer = 0153, /* No longer generated as of v18.  */
    Bset_mark = 0163, /* this loser is no longer generated as of v18 */
#endif
};

/* Whether to maintain a `top' and `bottom' field in the stack frame.  */
#define BYTE_MAINTAIN_TOP BYTE_CODE_SAFE

/* Structure describing a value stack used during byte-code execution
   in Fbyte_code.  */

struct byte_stack
{
  /* Program counter.  This points into the byte_string below
     and is relocated when that string is relocated.  */
  const unsigned char *pc;

  /* Top and bottom of stack.  The bottom points to an area of memory
     allocated with alloca in Fbyte_code.  */
#if BYTE_MAINTAIN_TOP
  Lisp_Object *top, *bottom;
#endif

  /* The string containing the byte-code, and its current address.
     Storing this here protects it from GC because mark_byte_stack
     marks it.  */
  Lisp_Object byte_string;
  const unsigned char *byte_string_start;

  /* Next entry in byte_stack_list.  */
  struct byte_stack *next;
};

/* A list of currently active byte-code execution value stacks.
   Fbyte_code adds an entry to the head of this list before it starts
   processing byte-code, and it removes the entry again when it is
   done.  Signaling an error truncates the list.  */

struct byte_stack *byte_stack_list;


/* Relocate program counters in the stacks on byte_stack_list.  Called
   when GC has completed.  */

void
relocate_byte_stack (void)
{
  struct byte_stack *stack;

  for (stack = byte_stack_list; stack; stack = stack->next)
    {
      if (stack->byte_string_start != SDATA (stack->byte_string))
	{
	  ptrdiff_t offset = stack->pc - stack->byte_string_start;
	  stack->byte_string_start = SDATA (stack->byte_string);
	  stack->pc = stack->byte_string_start + offset;
	}
    }
}


/* Fetch the next byte from the bytecode stream.  */

#ifdef BYTE_CODE_SAFE
#define FETCH (eassert (stack.byte_string_start == SDATA (stack.byte_string)), *stack.pc++)
#else
#define FETCH *stack.pc++
#endif

/* Fetch two bytes from the bytecode stream and make a 16-bit number
   out of them.  */

#define FETCH2 (op = FETCH, op + (FETCH << 8))

/* Push x onto the execution stack.  This used to be #define PUSH(x)
   (*++stackp = (x)) This oddity is necessary because Alliant can't be
   bothered to compile the preincrement operator properly, as of 4/91.
   -JimB */

#define PUSH(x) (top++, *top = (x))

/* Pop a value off the execution stack.  */

#define POP (*top--)

/* Discard n values from the execution stack.  */

#define DISCARD(n) (top -= (n))

/* Get the value which is at the top of the execution stack, but don't
   pop it.  */

#define TOP (*top)

/* Actions that must be performed before and after calling a function
   that might GC.  */

#if !BYTE_MAINTAIN_TOP
#define BEFORE_POTENTIAL_GC()	((void)0)
#define AFTER_POTENTIAL_GC()	((void)0)
#else
#define BEFORE_POTENTIAL_GC()	stack.top = top
#define AFTER_POTENTIAL_GC()	stack.top = NULL
#endif

/* Garbage collect if we have consed enough since the last time.
   We do this at every branch, to avoid loops that never GC.  */

#define MAYBE_GC()		\
  do {				\
   BEFORE_POTENTIAL_GC ();	\
   maybe_gc ();			\
   AFTER_POTENTIAL_GC ();	\
 } while (0)

/* Check for jumping out of range.  */

#ifdef BYTE_CODE_SAFE

#define CHECK_RANGE(ARG) \
  if (ARG >= bytestr_length) emacs_abort ()

#else /* not BYTE_CODE_SAFE */

#define CHECK_RANGE(ARG)

#endif /* not BYTE_CODE_SAFE */

/* A version of the QUIT macro which makes sure that the stack top is
   set before signaling `quit'.  */

#define BYTE_CODE_QUIT					\
  do {							\
    if (!NILP (Vquit_flag) && NILP (Vinhibit_quit))	\
      {							\
        Lisp_Object flag = Vquit_flag;			\
	Vquit_flag = Qnil;				\
        BEFORE_POTENTIAL_GC ();				\
	if (EQ (Vthrow_on_input, flag))			\
	  Fthrow (Vthrow_on_input, Qt);			\
	Fsignal (Qquit, Qnil);				\
	AFTER_POTENTIAL_GC ();				\
      }							\
    else if (pending_signals)				\
      process_pending_signals ();			\
  } while (0)


DEFUN ("byte-code", Fbyte_code, Sbyte_code, 3, 3, 0,
       doc: /* Function used internally in byte-compiled code.
The first argument, BYTESTR, is a string of byte code;
the second, VECTOR, a vector of constants;
the third, MAXDEPTH, the maximum stack depth used in this function.
If the third argument is incorrect, Emacs may crash.  */)
  (Lisp_Object bytestr, Lisp_Object vector, Lisp_Object maxdepth)
{
  return exec_byte_code (bytestr, vector, maxdepth, Qnil, 0, NULL);
}

static void
bcall0 (Lisp_Object f)
{
  Ffuncall (1, &f);
}

// Global jit context
jit_context_t jit_context;

#define jit_type_Lisp_Object jit_type_nuint

jit_type_t native_varref_sig;
Lisp_Object
native_varref (Lisp_Object v1)
{
  Lisp_Object v2;

  if (SYMBOLP (v1))
    {
      if (XSYMBOL (v1)->redirect != SYMBOL_PLAINVAL
	  || (v2 = SYMBOL_VAL (XSYMBOL (v1)),
	      EQ (v2, Qunbound)))
	{
	  v2 = Fsymbol_value (v1);
	}
    }
  else
    {
      v2 = Fsymbol_value (v1);
    }
  return v2;
}

jit_type_t native_ifnil_sig;
bool
native_ifnil (Lisp_Object v1)
{
  MAYBE_GC ();
  if (NILP (v1))
    {
      BYTE_CODE_QUIT;
      return true;
    }
  else
    return false;
}

jit_type_t native_ifnonnil_sig;
bool
native_ifnonnil (Lisp_Object v1)
{
  MAYBE_GC ();
  if (!NILP (v1))
    {
      BYTE_CODE_QUIT;
      return true;
    }
  else
    return false;

}

jit_type_t native_car_sig;
Lisp_Object
native_car (Lisp_Object v1)
{
  if (CONSP (v1))
    return XCAR (v1);
  else if (NILP (v1))
    return Qnil;
  else
    {
      wrong_type_argument (Qlistp, v1);
    }
}

jit_type_t native_eq_sig;
Lisp_Object
native_eq (Lisp_Object v1, Lisp_Object v2)
{
  return EQ (v1, v2) ? Qt : Qnil;
}

jit_type_t native_memq_sig;
Lisp_Object
native_memq (Lisp_Object v1, Lisp_Object v2)
{
  v1 = Fmemq (v1, v2);
  return v1;
}

jit_type_t native_cdr_sig;
Lisp_Object
native_cdr (Lisp_Object v1)
{
  if (CONSP (v1))
    return XCDR (v1);
  else if (NILP (v1))
    return Qnil;
  else
    {
      wrong_type_argument (Qlistp, v1);
    }
}

jit_type_t native_varset_sig;
void
native_varset (Lisp_Object sym, Lisp_Object val)
{
  /* Inline the most common case.  */
  if (SYMBOLP (sym)
      && !EQ (val, Qunbound)
      && !XSYMBOL (sym)->redirect
      && !SYMBOL_CONSTANT_P (sym))
    SET_SYMBOL_VAL (XSYMBOL (sym), val);
  else
    {
      set_internal (sym, val, Qnil, 0);
    }
}

jit_type_t specbind_sig;
jit_type_t Ffuncall_sig;

jit_type_t native_unbind_to_sig;
Lisp_Object
native_unbind_to (ptrdiff_t x, Lisp_Object q)
{
  return unbind_to (SPECPDL_INDEX () - x, q);
}

jit_type_t unbind_to_sig;

jit_type_t byte_code_quit_sig;
void
byte_code_quit (void)
{
  MAYBE_GC ();
  BYTE_CODE_QUIT;
}

jit_type_t native_save_excursion_sig;
void
native_save_excursion (void)
{
  record_unwind_protect (save_excursion_restore,
			 save_excursion_save ());
}

jit_type_t native_save_restriction_sig;
void
native_save_restriction (void)
{
  record_unwind_protect (save_restriction_restore,
			 save_restriction_save ());
}


jit_type_t native_save_window_excursion_sig;
Lisp_Object
native_save_window_excursion (Lisp_Object v1)
{
  ptrdiff_t count1 = SPECPDL_INDEX ();
  record_unwind_protect (restore_window_configuration,
			 Fcurrent_window_configuration (Qnil));
  v1 = Fprogn (v1);
  unbind_to (count1, v1);
  return v1;
}

jit_type_t native_catch_sig;
Lisp_Object
native_catch (Lisp_Object v2, Lisp_Object v1)
{
  return internal_catch (v2, eval_sub, v1);
}

jit_type_t native_pophandler_sig;
void
native_pophandler (void)
{
  handlerlist = handlerlist->next;
}

jit_type_t native_pushhandler_sig;
bool
native_pushhandler (Lisp_Object **stack, Lisp_Object tag,
		    int type)
{
  struct handler *c = push_handler (tag, type);

  if (sys_setjmp (c->jmp))
    {
      struct handler *c = handlerlist;
      native_pophandler ();
      *stack++;
      **stack = c->val;
      return true;
    }
  else
    return false;
}

jit_type_t native_unwind_protect_sig;
void
native_unwind_protect (Lisp_Object handler)
{
  record_unwind_protect (NILP (Ffunctionp (handler))
			 ? unwind_body : bcall0,
			 handler);
}

jit_type_t native_temp_output_buffer_setup_sig;
void
native_temp_output_buffer_setup (Lisp_Object x)
{
  CHECK_STRING (x);
  temp_output_buffer_setup (SSDATA (x));
}

jit_type_t native_nth_sig;
Lisp_Object
native_nth (Lisp_Object v1, Lisp_Object v2)
{
  EMACS_INT n;
  CHECK_NUMBER (v2);
  n = XINT (v2);
  immediate_quit = 1;
  while (--n >= 0 && CONSP (v1))
    v1 = XCDR (v1);
  immediate_quit = 0;
  return CAR (v1);
}

jit_type_t native_symbolp_sig;
jit_type_t native_consp_sig;
jit_type_t native_stringp_sig;
jit_type_t native_listp_sig;
jit_type_t native_not_sig;
Lisp_Object
native_symbolp (Lisp_Object v1)
{
  return SYMBOLP (v1) ? Qt : Qnil;
}
Lisp_Object
native_consp (Lisp_Object v1)
{
  return CONSP (v1) ? Qt : Qnil;
}
Lisp_Object
native_stringp (Lisp_Object v1)
{
  return STRINGP (v1) ? Qt : Qnil;
}
Lisp_Object
native_listp (Lisp_Object v1)
{
  return CONSP (v1) || NILP (v1) ? Qt : Qnil;
}
Lisp_Object
native_not (Lisp_Object v1)
{
  return NILP (v1) ? Qt : Qnil;
}

jit_type_t native_add1_sig;
Lisp_Object
native_add1 (Lisp_Object v1, bool add)
{
  if (INTEGERP (v1))
    {
      XSETINT (v1, XINT (v1) + (add ? 1 : -1));
      return v1;
    }
  else if (add)
    return Fadd1 (v1);
  else
    return Fsub1 (v1);
}

jit_type_t native_eqlsign_sig;
Lisp_Object
native_eqlsign (Lisp_Object v1, Lisp_Object v2)
{
  CHECK_NUMBER_OR_FLOAT_COERCE_MARKER (v1);
  CHECK_NUMBER_OR_FLOAT_COERCE_MARKER (v2);
  if (FLOATP (v1) || FLOATP (v2))
    {
      double f1, f2;

      f1 = (FLOATP (v1) ? XFLOAT_DATA (v1) : XINT (v1));
      f2 = (FLOATP (v2) ? XFLOAT_DATA (v2) : XINT (v2));
      return (f1 == f2 ? Qt : Qnil);
    }
  else
    return (XINT (v1) == XINT (v2) ? Qt : Qnil);
}

jit_type_t arithcompare_sig;
jit_type_t native_negate_sig;
Lisp_Object native_negate (Lisp_Object v)
{
  if (INTEGERP (v))
    {
      XSETINT (v, - XINT (v));
      return v;
    }
  else
    return Fminus (1, &v);
}

jit_type_t native_point_sig;
Lisp_Object native_point (void)
{
  Lisp_Object v1;
  XSETFASTINT (v1, PT);
  return v1;
}

jit_type_t native_point_max_sig;
Lisp_Object native_point_max (void)
{
  Lisp_Object v1;
  XSETFASTINT (v1, ZV);
  return v1;
}

jit_type_t native_point_min_sig;
Lisp_Object native_point_min (void)
{
  Lisp_Object v1;
  XSETFASTINT (v1, BEGV);
  return v1;
}

jit_type_t native_current_column_sig;
Lisp_Object native_current_column (void)
{
  Lisp_Object v1;
  XSETFASTINT (v1, current_column ());
  return v1;
}

jit_type_t native_interactive_p_sig;
Lisp_Object native_interactive_p (void)
{
  return call0 (intern ("interactive-p"));
}

jit_type_t native_char_syntax_sig;
Lisp_Object native_char_syntax (Lisp_Object v)
{
  int c;

  CHECK_CHARACTER (v);
  c = XFASTINT (v);
  if (NILP (BVAR (current_buffer, enable_multibyte_characters)))
    MAKE_CHAR_MULTIBYTE (c);
  XSETFASTINT (v, syntax_code_spec[SYNTAX (c)]);
  return v;
}

jit_type_t native_elt_sig;
Lisp_Object native_elt (Lisp_Object v1, Lisp_Object v2)
{
  if (CONSP (v2))
    {
      /* Exchange args and then do nth.  */
      EMACS_INT n;
      CHECK_NUMBER (v2);
      n = XINT (v2);
      immediate_quit = 1;
      while (--n >= 0 && CONSP (v1))
	v1 = XCDR (v1);
      immediate_quit = 0;
      return CAR (v1);
    }
  else
    return Felt (v1, v2);
}

jit_type_t native_car_safe_sig;
Lisp_Object native_car_safe (Lisp_Object v)
{
  return CAR_SAFE (v);
}
jit_type_t native_cdr_safe_sig;
Lisp_Object native_cdr_safe (Lisp_Object v)
{
  return CDR_SAFE (v);
}

jit_type_t native_number_p_sig;
Lisp_Object native_number_p (Lisp_Object v)
{
  return NUMBERP (v) ? Qt : Qnil;
}
jit_type_t native_integer_p_sig;
Lisp_Object native_integer_p (Lisp_Object v)
{
  return INTEGERP (v) ? Qt : Qnil;
}

void
emacs_jit_init ()
{
#define JIT_SIG_(f, ret, params)		\
  do {						\
    f##_sig =					\
      jit_type_create_signature (		\
        jit_abi_cdecl,				\
        ret,					\
        params,					\
        sizeof (params) / sizeof (params[0]),	\
        1);					\
  } while (0)
#define JIT_SIG(f, ret, ...)			\
  do {						\
    jit_type_t params[] =			\
      {						\
        __VA_ARGS__				\
      };					\
    JIT_SIG_ (f, ret, params);			\
  } while (0)

  jit_context = jit_context_create();

  JIT_SIG (native_varref, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_ifnil, jit_type_sys_bool, jit_type_Lisp_Object);
  JIT_SIG (native_car, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_eq, jit_type_Lisp_Object, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_memq, jit_type_Lisp_Object, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_cdr, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_varset, jit_type_void, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (specbind, jit_type_void, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (Ffuncall, jit_type_Lisp_Object, jit_type_nuint, jit_type_void_ptr);
  JIT_SIG (native_unbind_to, jit_type_Lisp_Object, jit_type_nuint, jit_type_Lisp_Object);
  JIT_SIG (unbind_to, jit_type_Lisp_Object, jit_type_nuint, jit_type_Lisp_Object);
  JIT_SIG (byte_code_quit, jit_type_void);  /* should this have param type of jit_type_void? */
  JIT_SIG (native_save_excursion, jit_type_void);
  JIT_SIG (native_save_window_excursion, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_save_restriction, jit_type_void);
  JIT_SIG (native_catch, jit_type_Lisp_Object, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_pophandler, jit_type_void);
  JIT_SIG (native_pushhandler, jit_type_sys_bool, jit_type_create_pointer (jit_type_void_ptr, 1), jit_type_Lisp_Object, jit_type_nint);
  JIT_SIG (native_unwind_protect, jit_type_void, jit_type_Lisp_Object);
  JIT_SIG (native_temp_output_buffer_setup, jit_type_void, jit_type_Lisp_Object);
  JIT_SIG (native_nth, jit_type_Lisp_Object, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_symbolp, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_consp, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_stringp, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_listp, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_not, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_add1, jit_type_Lisp_Object, jit_type_Lisp_Object, jit_type_sys_bool);
  JIT_SIG (native_eqlsign, jit_type_Lisp_Object, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (arithcompare, jit_type_Lisp_Object, jit_type_Lisp_Object, jit_type_Lisp_Object, jit_type_nuint);
  JIT_SIG (native_negate, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_point, jit_type_Lisp_Object);
  JIT_SIG (native_point_max, jit_type_Lisp_Object);
  JIT_SIG (native_point_min, jit_type_Lisp_Object);
  JIT_SIG (native_current_column, jit_type_Lisp_Object);
  JIT_SIG (native_interactive_p, jit_type_Lisp_Object);
  JIT_SIG (native_char_syntax, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_elt, jit_type_Lisp_Object, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_car_safe, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_cdr_safe, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_number_p, jit_type_Lisp_Object, jit_type_Lisp_Object);
  JIT_SIG (native_integer_p, jit_type_Lisp_Object, jit_type_Lisp_Object);
}

Lisp_Object
jit_exec (Lisp_Object byte_code, Lisp_Object args_template, ptrdiff_t nargs, Lisp_Object *args)
{
  Lisp_Object *top;
  Lisp_Object maxdepth = XVECTOR (byte_code)->contents[COMPILED_STACK_DEPTH];

  CHECK_NATNUM (maxdepth);
  if (MAX_ALLOCA / word_size <= XFASTINT (maxdepth))
    memory_full (SIZE_MAX);
  top = alloca ((XFASTINT (maxdepth) + 1) * sizeof *top);

  if (INTEGERP (args_template))
    {
      ptrdiff_t at = XINT (args_template);
      bool rest = (at & 128) != 0;
      int mandatory = at & 127;
      ptrdiff_t nonrest = at >> 8;
      eassert (mandatory <= nonrest);
      if (nargs <= nonrest)
	{
	  ptrdiff_t i;
	  for (i = 0 ; i < nargs; i++, args++)
	    PUSH (*args);
	  if (nargs < mandatory)
	    /* Too few arguments.  */
	    Fsignal (Qwrong_number_of_arguments,
		     list2 (Fcons (make_number (mandatory),
				   rest ? Qand_rest : make_number (nonrest)),
			    make_number (nargs)));
	  else
	    {
	      for (; i < nonrest; i++)
		PUSH (Qnil);
	      if (rest)
		PUSH (Qnil);
	    }
	}
      else if (rest)
	{
	  ptrdiff_t i;
	  for (i = 0 ; i < nonrest; i++, args++)
	    PUSH (*args);
	  PUSH (Flist (nargs - nonrest, args));
	}
      else
	/* Too many arguments.  */
	Fsignal (Qwrong_number_of_arguments,
		 list2 (Fcons (make_number (mandatory), make_number (nonrest)),
			make_number (nargs)));
    }
  else if (! NILP (args_template))
    /* We should push some arguments on the stack.  */
    {
      error ("Unknown args template!");
    }

  {
    void *arg = &top;
    jit_function_t func =
      XSAVE_POINTER (
        XVECTOR (byte_code)->contents[COMPILED_JIT_ID],
	0);
    Lisp_Object ret;
    if (!jit_function_apply (func, &arg, (void *)&ret))
      emacs_abort ();
    return ret;
  }
}

Lisp_Object
jit_byte_code (Lisp_Object byte_code, Lisp_Object args_template, ptrdiff_t nargs, Lisp_Object *args)
{
  ptrdiff_t count = SPECPDL_INDEX ();
#ifdef BYTE_CODE_METER
  int volatile this_op = 0;
  int prev_op;
#endif
  int op;
  /* Lisp_Object v1, v2; */
  Lisp_Object *vectorp;
#ifdef BYTE_CODE_SAFE
  ptrdiff_t const_length;
  Lisp_Object *stacke;
#endif
  ptrdiff_t bytestr_length;
  struct byte_stack stack;
  Lisp_Object bytestr;
  Lisp_Object vector;
  Lisp_Object maxdepth;
  Lisp_Object *top;
  enum handlertype type;

  // jit-specific variables
  jit_function_t this_func;
  jit_type_t params[1];
  jit_type_t signature;
  jit_label_t *labels;
  jit_value_t stackv;

  if (!byte_code_jit_on)
    {
      return exec_byte_code (AREF (byte_code, COMPILED_BYTECODE),
			     AREF (byte_code, COMPILED_CONSTANTS),
			     AREF (byte_code, COMPILED_STACK_DEPTH),
			     args_template,
			     nargs,
			     args);
    }

  CHECK_COMPILED (byte_code);
  bytestr = XVECTOR (byte_code)->contents[COMPILED_BYTECODE];
  vector = XVECTOR (byte_code)->contents[COMPILED_CONSTANTS];
  maxdepth = XVECTOR (byte_code)->contents[COMPILED_STACK_DEPTH];
  CHECK_STRING (bytestr);
  CHECK_VECTOR (vector);
  CHECK_NATNUM (maxdepth);

#ifdef BYTE_CODE_SAFE
  const_length = ASIZE (vector);
#endif

  if (STRING_MULTIBYTE (bytestr))
    /* BYTESTR must have been produced by Emacs 20.2 or the earlier
       because they produced a raw 8-bit string for byte-code and now
       such a byte-code string is loaded as multibyte while raw 8-bit
       characters converted to multibyte form.  Thus, now we must
       convert them back to the originally intended unibyte form.  */
    bytestr = Fstring_as_unibyte (bytestr);

  bytestr_length = SBYTES (bytestr);
  vectorp = XVECTOR (vector)->contents;

  stack.byte_string = bytestr;
  stack.pc = stack.byte_string_start = SDATA (bytestr);
  if (MAX_ALLOCA / word_size <= XFASTINT (maxdepth))
    memory_full (SIZE_MAX);
#if BYTE_MAINTAIN_TOP
  stack.top = NULL;
#endif
  stack.next = byte_stack_list;
  byte_stack_list = &stack;

  // check if function has already been compiled
  if (XVECTOR (byte_code)->contents[COMPILED_JIT_ID])
    {
      jit_function_t f =
	XSAVE_POINTER (
	  XVECTOR (byte_code)->contents[COMPILED_JIT_ID], 0);
      return jit_exec (byte_code, args_template, nargs, args);
    }

  // prepare for jit
  jit_context_build_start (jit_context);
  params[0] = jit_type_void_ptr;
  signature = jit_type_create_signature (jit_abi_cdecl, jit_type_nuint, params, 1, 1);
  this_func = jit_function_create (jit_context, signature);
  stackv = jit_value_get_param (this_func, 0);
  labels = alloca (sizeof (*labels) * SBYTES (bytestr));
  {
    // give each instruction a label.  the labels won't be initialized
    // until we attach code to them, but they work as a placeholder.
    int i;
    for (i = 0; i < SBYTES (bytestr); i++)
      labels[i] = jit_label_undefined;
  }
  // start by jumping to the first label
  jit_insn_branch (this_func, &labels[0]);

  while (stack.pc < stack.byte_string_start + bytestr_length)
    {
#ifdef BYTE_CODE_METER
      prev_op = this_op;
      this_op = op = FETCH;
      METER_CODE (prev_op, op);
#else
#ifndef BYTE_CODE_THREADED
      op = FETCH;
#endif
#endif

      /* The interpreter can be compiled one of two ways: as an
	 ordinary switch-based interpreter, or as a threaded
	 interpreter.  The threaded interpreter relies on GCC's
	 computed goto extension, so it is not available everywhere.
	 Threading provides a performance boost.  These macros are how
	 we allow the code to be compiled both ways.  */
#ifdef BYTE_CODE_THREADED
      /* The CASE macro introduces an instruction's body.  It is
	 either a label or a case label.  */
#define CASE(OP) insn_ ## OP
      /* NEXT is invoked at the end of an instruction to go to the
	 next instruction.  It is either a computed goto, or a
	 plain break.  */
#define NEXT								\
      do {								\
	if (stack.pc >= stack.byte_string_start + bytestr_length)	\
	  goto exit;							\
	else								\
	  goto *(targets[op = FETCH]);					\
      } while (0)
      /* FIRST is like NEXT, but is only used at the start of the
	 interpreter body.  In the switch-based interpreter it is the
	 switch, so the threaded definition must include a semicolon.  */
#define FIRST NEXT;
      /* Most cases are labeled with the CASE macro, above.
	 CASE_DEFAULT is one exception; it is used if the interpreter
	 being built requires a default case.  The threaded
	 interpreter does not, because the dispatch table is
	 completely filled.  */
#define CASE_DEFAULT
      /* This introduces an instruction that is known to call abort.  */
#define CASE_ABORT CASE (Bstack_ref): CASE (default)
#else
      /* See above for the meaning of the various defines.  */
#define CASE(OP) case OP
#define NEXT break
#define FIRST switch (op)
#define CASE_DEFAULT case 255: default:
#define CASE_ABORT case 0
#endif

#ifdef BYTE_CODE_THREADED

      /* A convenience define that saves us a lot of typing and makes
	 the table clearer.  */
#define LABEL(OP) [OP] = &&insn_ ## OP

#if 4 < __GNUC__ + (6 <= __GNUC_MINOR__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Woverride-init"
#elif defined __clang__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Winitializer-overrides"
#endif

      /* This is the dispatch table for the threaded interpreter.  */
      static const void *const targets[256] =
	{
	  [0 ... (Bconstant - 1)] = &&insn_default,
	  [Bconstant ... 255] = &&insn_Bconstant,

#define DEFINE(name, value) LABEL (name) ,
	  BYTE_CODES
#undef DEFINE
	};

#if 4 < __GNUC__ + (6 <= __GNUC_MINOR__) || defined __clang__
# pragma GCC diagnostic pop
#endif

#endif

#define JIT_PC (stack.pc - stack.byte_string_start)
#define JIT_NEED_STACK jit_value_ref (this_func, stackv)
#define JIT_NEXT				\
      do {					\
        if (!jit_insn_branch (			\
              this_func,		        \
	      &labels[JIT_PC+1]))		\
	  emacs_abort ();			\
      } while (0)

#define JIT_INC(v, n)				\
  do {						\
    jit_value_t i =				\
      jit_insn_add_relative (			\
        this_func,		                \
        v,					\
        (jit_nint )n);				\
    if (!i)					\
      emacs_abort ();				\
    else if (!jit_insn_store (			\
                this_func,			\
		v,				\
		i))				\
      emacs_abort ();				\
  } while (0)

#define JIT_PUSH(v)				\
      do {					\
	JIT_INC (stackv, sizeof (Lisp_Object));	\
	if (!jit_insn_store_relative (		\
	      this_func,			\
	      stackv,				\
	      (jit_nint )0, 			\
	      v))				\
	  emacs_abort ();			\
      } while (0)

#define JIT_TOP(v)				\
      do {					\
	v = jit_insn_load_relative (		\
	      this_func,			\
	      stackv,				\
	      (jit_nint )0,			\
	      jit_type_Lisp_Object);		\
	if (!v)					\
	  emacs_abort ();			\
      } while (0)

#define JIT_POP(v)					\
      do {						\
	JIT_TOP (v);					\
	JIT_INC (stackv, -sizeof (Lisp_Object));	\
      } while (0)

#define JIT_CALL(f, args, n)				\
      jit_insn_call_native (				\
	this_func,					\
	#f,						\
	(void*)&f,					\
	f##_sig,					\
	args,						\
	n,						\
	JIT_CALL_NOTHROW)

#define JIT_CALL_ARGS(r, f, ...)			\
      do {						\
	jit_value_t params[] =				\
	  {						\
	    __VA_ARGS__					\
	  };						\
	r = JIT_CALL (					\
	      f,				        \
	      params,					\
	      sizeof (params) / sizeof (params[0]));	\
      } while (0)

#define JIT_CONSTANT(t, v)			\
      jit_value_create_nint_constant (		\
	this_func,				\
	t,					\
	v)

#define JIT_CALL_WITH_STACK_N(f, n)			\
      do {						\
	jit_type_t params[n];				\
	jit_value_t args[n];				\
	jit_value_t ret;				\
	jit_type_t f##_sig;				\
	int i;						\
	for (i = 0; i < n; i++)				\
	  params[i] = jit_type_Lisp_Object;		\
	JIT_SIG_ (f, jit_type_Lisp_Object, params);	\
	JIT_NEED_STACK;					\
	for (i = 1; i <= n; i++)			\
	  JIT_POP (args[n-i]);				\
	ret = JIT_CALL (f, args, n);			\
	JIT_PUSH (ret);					\
      } while (0)

#define JIT_CALL_WITH_STACK_MANY(f, n)				\
      do {							\
	jit_value_t ret;					\
	jit_type_t f##_sig;					\
	jit_type_t ptr;						\
	ptr = jit_type_create_pointer (				\
	        jit_type_void_ptr,		                \
	        1);						\
	if (!ptr)						\
	  emacs_abort ();					\
	JIT_SIG (						\
	  f,						        \
	  jit_type_Lisp_Object,					\
	  jit_type_nuint,					\
	  ptr);							\
	JIT_NEED_STACK;						\
	JIT_INC (stackv, -(n - 1) * sizeof (Lisp_Object));	\
	JIT_CALL_ARGS (						\
	  ret,					                \
	  f,							\
	  JIT_CONSTANT (jit_type_nuint, n),			\
	  stackv);						\
	if (!jit_insn_store_relative (				\
	      this_func,				        \
	      stackv,						\
	      (jit_nint )0,					\
	      ret))						\
	  emacs_abort ();					\
      } while (0)

      // create a new block and attach a label to it
      jit_insn_label (this_func, &labels[JIT_PC]);

      FIRST
	{
	CASE (Bvarref7):
	  op = FETCH2;
	  goto varref;

	CASE (Bvarref):
	CASE (Bvarref1):
	CASE (Bvarref2):
	CASE (Bvarref3):
	CASE (Bvarref4):
	CASE (Bvarref5):
	  op = op - Bvarref;
	  goto varref;

	/* This seems to be the most frequently executed byte-code
	   among the Bvarref's, so avoid a goto here.  */
	CASE (Bvarref6):
	  op = FETCH;
	varref:
	  {
	    jit_value_t v1, v2;
	    JIT_NEED_STACK;
	    v1 = JIT_CONSTANT (jit_type_nuint, vectorp[op]);
	    v2 = JIT_CALL (native_varref, &v1, 1);
	    JIT_PUSH (v2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bcar):
	  {
	    JIT_CALL_WITH_STACK_N (native_car, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Beq):
	  {
	    JIT_CALL_WITH_STACK_N (native_eq, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bmemq):
	  {
	    JIT_CALL_WITH_STACK_N (native_memq, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bcdr):
	  {
	    JIT_CALL_WITH_STACK_N (native_cdr, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bvarset):
	CASE (Bvarset1):
	CASE (Bvarset2):
	CASE (Bvarset3):
	CASE (Bvarset4):
	CASE (Bvarset5):
	  op -= Bvarset;
	  goto varset;

	CASE (Bvarset7):
	  op = FETCH2;
	  goto varset;

	CASE (Bvarset6):
	  op = FETCH;
	varset:
	  {
	    jit_value_t sym, val, x;
	    JIT_NEED_STACK;
	    sym = JIT_CONSTANT (jit_type_Lisp_Object, vectorp[op]);
	    JIT_POP (val);
	    JIT_CALL_ARGS (x, native_varset, sym, val);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bdup):
	  {
	    jit_value_t x;
	    JIT_NEED_STACK;
	    JIT_TOP (x);
	    JIT_PUSH (x);
	    JIT_NEXT;
	    NEXT;
	  }

	/* ------------------ */

	CASE (Bvarbind6):
	  op = FETCH;
	  goto varbind;

	CASE (Bvarbind7):
	  op = FETCH2;
	  goto varbind;

	CASE (Bvarbind):
	CASE (Bvarbind1):
	CASE (Bvarbind2):
	CASE (Bvarbind3):
	CASE (Bvarbind4):
	CASE (Bvarbind5):
	  op -= Bvarbind;
	varbind:
	  {
	    jit_value_t v1, v2, x;
	    JIT_NEED_STACK;
	    v1 = JIT_CONSTANT (jit_type_Lisp_Object, vectorp[op]);
	    JIT_POP (v2);
	    JIT_CALL_ARGS (x, specbind, v1, v2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bcall6):
	  op = FETCH;
	  goto docall;

	CASE (Bcall7):
	  op = FETCH2;
	  goto docall;

	CASE (Bcall):
	CASE (Bcall1):
	CASE (Bcall2):
	CASE (Bcall3):
	CASE (Bcall4):
	CASE (Bcall5):
	  op -= Bcall;
	docall:
	  {
	    JIT_NEED_STACK;
	    JIT_CALL_WITH_STACK_MANY (Ffuncall, op + 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bunbind6):
	  op = FETCH;
	  goto dounbind;

	CASE (Bunbind7):
	  op = FETCH2;
	  goto dounbind;

	CASE (Bunbind):
	CASE (Bunbind1):
	CASE (Bunbind2):
	CASE (Bunbind3):
	CASE (Bunbind4):
	CASE (Bunbind5):
	  op -= Bunbind;
	dounbind:
	  {
	    jit_value_t args[] =
	      {
		JIT_CONSTANT (jit_type_nuint, op),
		JIT_CONSTANT (jit_type_Lisp_Object, Qnil)
	      };
	    JIT_CALL (native_unbind_to, args, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bunbind_all):	/* Obsolete.  Never used.  */
	  /* To unbind back to the beginning of this frame.  Not used yet,
	     but will be needed for tail-recursion elimination.  */
	  {
	    jit_value_t args[] =
	      {
		JIT_CONSTANT (jit_type_nuint, count),
		JIT_CONSTANT (jit_type_Lisp_Object, Qnil)
	      };
	    JIT_CALL (unbind_to, args, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bgoto):
	  {
	    jit_value_t v;
	    op = FETCH2;
	    CHECK_RANGE (op);
	    JIT_CALL (byte_code_quit, NULL, 0);
	    jit_insn_branch (
	      this_func,
	      &labels[op]);
	    NEXT;
	  }

	CASE (Bgotoifnil):
	CASE (BRgotoifnil):
	CASE (Bgotoifnonnil):
	CASE (BRgotoifnonnil):
	CASE (Bgotoifnilelsepop):
	CASE (BRgotoifnilelsepop):
	CASE (Bgotoifnonnilelsepop):
	CASE (BRgotoifnonnilelsepop):
	  {
	    jit_value_t v1, v2, v3;
	    int insn = op;
	    if (insn >= Bgotoifnil && insn <= Bgotoifnonnilelsepop)
	      op = FETCH2;
	    else
	      {
		op = FETCH;
		op = (stack.pc + op - 127) - stack.byte_string_start;
	      }
	    CHECK_RANGE (op);
	    JIT_NEED_STACK;
	    v1 = JIT_CONSTANT (jit_type_Lisp_Object, vectorp[op]);
	    JIT_POP (v2);
	    if (insn == Bgotoifnil || insn == Bgotoifnilelsepop)
	      v3 = JIT_CALL (native_ifnil, &v2, 1);
	    else
	      v3 = JIT_CALL (native_ifnonnil, &v2, 1);
	    jit_insn_branch_if (
	      this_func,
	      v3,
	      &labels[op]);
	    if (insn == Bgotoifnilelsepop || insn == Bgotoifnonnilelsepop
		|| insn == BRgotoifnilelsepop || insn == BRgotoifnonnilelsepop)
	      JIT_INC (stackv, -sizeof (Lisp_Object));
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (BRgoto):
	  {
	    const unsigned char *dest = stack.pc + ((int) *stack.pc - 127);
	    JIT_CALL (byte_code_quit, NULL, 0);
	    jit_insn_branch (
	      this_func,
	      &labels[dest - stack.byte_string_start]);
	    NEXT;
	  }

	CASE (Breturn):
	  {
	    jit_value_t v;
	    JIT_NEED_STACK;
	    JIT_POP (v);
	    jit_insn_return (this_func, v);
	    NEXT;
	  }

	CASE (Bdiscard):
	  {
	    JIT_NEED_STACK;
	    JIT_INC (stackv, -sizeof (Lisp_Object));
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bconstant2):
	  {
	    jit_value_t v = JIT_CONSTANT (jit_type_Lisp_Object, vectorp[FETCH2]);
	    JIT_NEED_STACK;
	    JIT_PUSH (v);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bsave_excursion):
	  {
	    JIT_CALL (native_save_excursion, NULL, 0);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bsave_current_buffer): /* Obsolete since ??.  */
	CASE (Bsave_current_buffer_1):
	  {
	    jit_type_t record_unwind_current_buffer_sig;
	    JIT_SIG (record_unwind_current_buffer, jit_type_void);
	    JIT_CALL (record_unwind_current_buffer, NULL, 0);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bsave_window_excursion): /* Obsolete since 24.1.  */
	  {
	    JIT_CALL_WITH_STACK_N (native_save_window_excursion, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bsave_restriction):
	  JIT_CALL (native_save_restriction, NULL, 0);
	  JIT_NEXT;
	  NEXT;

	CASE (Bcatch):		/* Obsolete since 24.4.  */
	  {
	    JIT_CALL_WITH_STACK_N (native_catch, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bpushcatch):	/* New in 24.4.  */
	  type = CATCHER;
	  goto pushhandler;
	CASE (Bpushconditioncase): /* New in 24.4.  */
	  type = CONDITION_CASE;
	pushhandler:
	  {
	    jit_value_t tag, stackp, result, typev;
	    int dest=  FETCH2;
	    JIT_NEED_STACK;
	    JIT_POP (tag);
	    stackp = jit_insn_address_of (this_func, stackv);
	    typev = JIT_CONSTANT (jit_type_nint, type);
	    JIT_CALL_ARGS (result, native_pushhandler, stackp, tag, typev);
	    jit_insn_branch_if (this_func, result, &labels[dest]);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bpophandler):	/* New in 24.4.  */
	  {
	    JIT_CALL (native_pophandler, NULL, 0);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bunwind_protect):	/* FIXME: avoid closure for lexbind.  */
	  {
	    jit_value_t handler;
	    JIT_NEED_STACK;
	    JIT_POP (handler);
	    JIT_CALL (native_unwind_protect, &handler, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bcondition_case):		/* Obsolete since 24.4.  */
	  {
	    JIT_CALL_WITH_STACK_N (internal_lisp_condition_case, 3);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Btemp_output_buffer_setup): /* Obsolete since 24.1.  */
	  {
	    jit_value_t x, y;
	    JIT_NEED_STACK;
	    JIT_POP (x);
	    JIT_CALL (native_temp_output_buffer_setup, &x, 1);
	    y = JIT_CONSTANT (jit_type_Lisp_Object, Vstandard_output);
	    JIT_PUSH (y);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Btemp_output_buffer_show): /* Obsolete since 24.1.  */
	  {
	    jit_type_t temp_output_buffer_show_sig;
	    jit_value_t v1, v2, c, q, x;
	    JIT_NEED_STACK;
	    JIT_SIG (temp_output_buffer_show,
		     jit_type_void,
		     jit_type_Lisp_Object);
	    JIT_POP (v1);
	    JIT_POP (v2);
	    JIT_CALL (temp_output_buffer_show, &v2, 1);
	    JIT_PUSH (v1);
	    c = JIT_CONSTANT (jit_type_nuint, 1);
	    q = JIT_CONSTANT (jit_type_Lisp_Object, Qnil);
	    JIT_CALL_ARGS (x, native_unbind_to, c, q);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bnth):
	  {
	    JIT_CALL_WITH_STACK_N (native_nth, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bsymbolp):
	CASE (Bconsp):
	CASE (Bstringp):
	CASE (Blistp):
	CASE (Bnot):
	  {
	    jit_value_t v1, v2;
	    JIT_NEED_STACK;
	    JIT_POP (v1);
	    switch (op)
	      {
	      case Bsymbolp:
		JIT_CALL_ARGS (v2, native_symbolp, v1);
		break;
	      case Bconsp:
		JIT_CALL_ARGS (v2, native_consp, v1);
		break;
	      case Bstringp:
		JIT_CALL_ARGS (v2, native_stringp, v1);
		break;
	      case Blistp:
		JIT_CALL_ARGS (v2, native_listp, v1);
		break;
	      case Bnot:
		JIT_CALL_ARGS (v2, native_not, v1);
		break;
	      }
	    JIT_PUSH (v2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bcons):
	  {
	    JIT_CALL_WITH_STACK_N (Fcons, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Blist1):
	  {
	    JIT_CALL_WITH_STACK_N (list1, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Blist2):
	  {
	    JIT_CALL_WITH_STACK_N (list2, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Blist3):
	CASE (Blist4):
	CASE (BlistN):
	  {
	    size_t temp;
	    if (op == BlistN)
	      {
		temp = FETCH;
	      }
	    else
	      {
		if (op == Blist3)
		  temp = 3;
		else
		  temp = 4;
	      }
	    JIT_CALL_WITH_STACK_MANY (Flist, temp);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Blength):
	  {
	    JIT_CALL_WITH_STACK_N (Flength, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Baref):
	  {
	    JIT_CALL_WITH_STACK_N (Faref, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Baset):
	  {
	    JIT_CALL_WITH_STACK_N (Faset, 3);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bsymbol_value):
	  {
	    JIT_CALL_WITH_STACK_N (Fsymbol_value, 1);
	    JIT_NEXT;
	    NEXT;
	  }
	CASE (Bsymbol_function):
	  {
	    JIT_CALL_WITH_STACK_N (Fsymbol_function, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bset):
	  {
	    JIT_CALL_WITH_STACK_N (Fset, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bfset):
	  {
	    JIT_CALL_WITH_STACK_N (Ffset, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bget):
	  {
	    JIT_CALL_WITH_STACK_N (Fget, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bsubstring):
	  {
	    JIT_CALL_WITH_STACK_N (Fsubstring, 3);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bconcat2):
	CASE (Bconcat3):
	CASE (Bconcat4):
	CASE (BconcatN):
	  {
	    size_t n;
	    if (op == BconcatN)
	      n = FETCH;
	    else
	      n = op - Bconcat2 + 1;
	    JIT_CALL_WITH_STACK_MANY (Fconcat, n);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bsub1):
	  {
	    jit_value_t v1, v2;
	    JIT_NEED_STACK;
	    JIT_POP (v1);
	    JIT_CALL_ARGS (v2, native_add1, v1, JIT_CONSTANT (jit_type_sys_bool, 1));
	    JIT_PUSH (v2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Badd1):
	  {
	    jit_value_t v1, v2;
	    JIT_NEED_STACK;
	    JIT_POP (v1);
	    JIT_CALL_ARGS (v2, native_add1, v1, JIT_CONSTANT (jit_type_sys_bool, 0));
	    JIT_PUSH (v2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Beqlsign):
	  {
	    JIT_CALL_WITH_STACK_N (native_eqlsign, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bgtr):
	CASE (Blss):
	CASE (Bleq):
	CASE (Bgeq):
	  {
	    jit_value_t v1, v2, v3, c;
	    enum Arith_Comparison v[] =
	      {
		ARITH_GRTR,
		ARITH_LESS,
		ARITH_LESS_OR_EQUAL,
		ARITH_GRTR_OR_EQUAL
	      };
	    JIT_NEED_STACK;
	    c = JIT_CONSTANT (jit_type_nuint, v[op-Bgtr]);
	    JIT_POP (v2);
	    JIT_POP (v1);
	    JIT_CALL_ARGS (v3, arithcompare, v1, v2, c);
	    JIT_PUSH (v3);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bdiff):
	  {
	    JIT_CALL_WITH_STACK_MANY (Fminus, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bnegate):
	  {
	    JIT_CALL_WITH_STACK_N (native_negate, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bplus):
	  {
	    JIT_CALL_WITH_STACK_MANY (Fplus, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bmax):
	  {
	    JIT_CALL_WITH_STACK_MANY (Fmax, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bmin):
	  {
	    JIT_CALL_WITH_STACK_MANY (Fmin, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bmult):
	  {
	    JIT_CALL_WITH_STACK_MANY (Ftimes, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bquo):
	  {
	    JIT_CALL_WITH_STACK_MANY (Fquo, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Brem):
	  {
	    JIT_CALL_WITH_STACK_N (Frem, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bpoint):
	  {
	    JIT_CALL_WITH_STACK_N (native_point, 0);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bgoto_char):
	  {
	    JIT_CALL_WITH_STACK_N (Fgoto_char, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Binsert):
	  {
	    JIT_CALL_WITH_STACK_MANY (Finsert, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (BinsertN):
	  {
	    JIT_CALL_WITH_STACK_MANY (Finsert, FETCH);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bpoint_max):
	  {
	    JIT_CALL_WITH_STACK_N (native_point_max, 0);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bpoint_min):
	  {
	    JIT_CALL_WITH_STACK_N (native_point_min, 0);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bchar_after):
	  {
	    JIT_CALL_WITH_STACK_N (Fchar_after, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bfollowing_char):
	  {
	    JIT_CALL_WITH_STACK_N (Ffollowing_char, 0);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bpreceding_char):
	  {
	    JIT_CALL_WITH_STACK_N (Fprevious_char, 0);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bcurrent_column):
	  {
	    JIT_CALL_WITH_STACK_N (native_current_column, 0);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bindent_to):
	  {
	    JIT_PUSH (JIT_CONSTANT (jit_type_Lisp_Object, Qnil));
	    JIT_CALL_WITH_STACK_N (Findent_to, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Beolp):
	  {
	    JIT_CALL_WITH_STACK_N (Feolp, 0);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Beobp):
	  {
	    JIT_CALL_WITH_STACK_N (Feobp, 0);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bbolp):
	  {
	    JIT_CALL_WITH_STACK_N (Fbolp, 0);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bbobp):
	  {
	    JIT_CALL_WITH_STACK_N (Fbobp, 0);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bcurrent_buffer):
	  {
	    JIT_CALL_WITH_STACK_N (Fcurrent_buffer, 0);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bset_buffer):
	  {
	    JIT_CALL_WITH_STACK_N (Fset_buffer, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Binteractive_p):	/* Obsolete since 24.1.  */
	  {
	    JIT_CALL_WITH_STACK_N (native_interactive_p, 0);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bforward_char):
	  {
	    JIT_CALL_WITH_STACK_N (Fforward_char, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bforward_word):
	  {
	    JIT_CALL_WITH_STACK_N (Fforward_word, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bskip_chars_forward):
	  {
	    JIT_CALL_WITH_STACK_N (Fskip_chars_forward, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bskip_chars_backward):
	  {
	    JIT_CALL_WITH_STACK_N (Fskip_chars_backward, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bforward_line):
	  {
	    JIT_CALL_WITH_STACK_N (Fforward_line, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bchar_syntax):
	  {
	    JIT_CALL_WITH_STACK_N (native_char_syntax, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bbuffer_substring):
	  {
	    JIT_CALL_WITH_STACK_N (Fbuffer_substring, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bdelete_region):
	  {
	    JIT_CALL_WITH_STACK_N (Fdelete_region, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bnarrow_to_region):
	  {
	    JIT_CALL_WITH_STACK_N (Fnarrow_to_region, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bwiden):
	  {
	    JIT_CALL_WITH_STACK_N (Fwiden, 0);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bend_of_line):
	  {
	    JIT_CALL_WITH_STACK_N (Fend_of_line, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bset_marker):
	  {
	    JIT_CALL_WITH_STACK_N (Fset_marker, 3);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bmatch_beginning):
	  {
	    JIT_CALL_WITH_STACK_N (Fmatch_beginning, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bmatch_end):
	  {
	    JIT_CALL_WITH_STACK_N (Fmatch_end, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bupcase):
	  {
	    JIT_CALL_WITH_STACK_N (Fupcase, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bdowncase):
	  {
	    JIT_CALL_WITH_STACK_N (Fdowncase, 1);
	    JIT_NEXT;
	    NEXT;
	  }

      CASE (Bstringeqlsign):
	  {
	    JIT_CALL_WITH_STACK_N (Fstring_equal, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bstringlss):
	  {
	    JIT_CALL_WITH_STACK_N (Fstring_lessp, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bequal):
	  {
	    JIT_CALL_WITH_STACK_N (Fequal, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bnthcdr):
	  {
	    JIT_CALL_WITH_STACK_N (Fnthcdr, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Belt):
	  {
	    JIT_CALL_WITH_STACK_N (native_elt, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bmember):
	  {
	    JIT_CALL_WITH_STACK_N (Fmember, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bassq):
	  {
	    JIT_CALL_WITH_STACK_N (Fassq, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bnreverse):
	  {
	    JIT_CALL_WITH_STACK_N (Fnreverse, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bsetcar):
	  {
	    JIT_CALL_WITH_STACK_N (Fsetcar, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bsetcdr):
	  {
	    JIT_CALL_WITH_STACK_N (Fsetcdr, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bcar_safe):
	  {
	    JIT_CALL_WITH_STACK_N (native_car_safe, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bcdr_safe):
	  {
	    JIT_CALL_WITH_STACK_N (native_cdr_safe, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bnconc):
	  {
	    JIT_CALL_WITH_STACK_MANY (Fnconc, 2);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bnumberp):
	  {
	    JIT_CALL_WITH_STACK_N (native_number_p, 1);
	    JIT_NEXT;
	    NEXT;
	  }

	CASE (Bintegerp):
	  {
	    JIT_CALL_WITH_STACK_N (native_integer_p, 1);
	    JIT_NEXT;
	    NEXT;
	  }

#ifdef BYTE_CODE_SAFE
	  /* These are intentionally written using 'case' syntax,
	     because they are incompatible with the threaded
	     interpreter.  */

	case Bset_mark:
	  error ("set-mark is an obsolete bytecode");
	  break;
	case Bscan_buffer:
	  error ("scan-buffer is an obsolete bytecode");
	  break;
#endif

	CASE_ABORT:
	  /* Actually this is Bstack_ref with offset 0, but we use Bdup
	     for that instead.  */
	  /* CASE (Bstack_ref): */
	  call3 (Qerror,
		 build_string ("Invalid byte opcode: op=%s, ptr=%d"),
		 make_number (op),
		 make_number ((stack.pc - 1) - stack.byte_string_start));

	  /* Handy byte-codes for lexical binding.  */
	CASE (Bstack_ref1):
	CASE (Bstack_ref2):
	CASE (Bstack_ref3):
	CASE (Bstack_ref4):
	CASE (Bstack_ref5):
	CASE (Bstack_ref6):
	CASE (Bstack_ref7):
	  {
	    jit_value_t v1;
	    int offs = op - Bstack_ref;
	    if (offs == 6)
	      offs = FETCH;
	    else if (offs == 7)
	      offs = FETCH2;

	    JIT_NEED_STACK;
	    JIT_INC (stackv, -offs * sizeof (Lisp_Object));
	    JIT_TOP (v1);
	    JIT_INC (stackv, offs * sizeof (Lisp_Object));
	    JIT_PUSH (v1);
	    JIT_NEXT;
	    NEXT;
	  }
	CASE (Bstack_set):
	CASE (Bstack_set2):
	  /* stack-set-0 = discard; stack-set-1 = discard-1-preserve-tos.  */
	  {
	    jit_value_t v1;
	    int offs = (op == Bstack_set) ? FETCH : FETCH2;
	    JIT_NEED_STACK;
	    JIT_TOP (v1);
	    if (offs != 0)
	      JIT_INC (stackv, -offs * sizeof (Lisp_Object));
	    jit_insn_store_relative (
	      this_func,
	      stackv,
	      (jit_nint )0,
	      v1);
	    JIT_INC (stackv, (offs - 1) * sizeof (Lisp_Object));
	    JIT_NEXT;
	    NEXT;
	  }
	CASE (BdiscardN):
	  {
	    op = FETCH;
	    if (op & 0x80)
	      {
		jit_value_t v1;
		op &= 0x7F;
		JIT_NEED_STACK;
		JIT_TOP (v1);
		JIT_INC (stackv, -op * sizeof (Lisp_Object));
		jit_insn_store_relative (
		  this_func,
		  stackv,
		  (jit_nint )0,
		  v1);
	      }
	    else
	      JIT_INC (stackv, -op * sizeof (Lisp_Object));
	    JIT_NEXT;
	    NEXT;
	  }

	CASE_DEFAULT
	CASE (Bconstant):
	  {
	    jit_value_t c;
#ifdef BYTE_CODE_SAFE
	    if (op < Bconstant)
	      {
		emacs_abort ();
	      }
	    if ((op -= Bconstant) >= const_length)
	      {
		emacs_abort ();
	      }
	    c = JIT_CONSTANT (jit_type_Lisp_Object, vectorp[op]);
#else
	    c = JIT_CONSTANT (jit_type_Lisp_Object, vectorp[op - Bconstant]);
#endif
	    JIT_PUSH (c);
	    JIT_NEXT;
	    NEXT;
	  }
	}
    }

 exit:
  byte_stack_list = byte_stack_list->next;

  {
    Lisp_Object v;
    int err = !jit_function_compile (this_func);
    jit_context_build_end (jit_context);
    free (labels);
    if (err)
      emacs_abort ();
    v = make_save_ptr (this_func);
    XVECTOR (byte_code)->contents[COMPILED_JIT_ID] = v;
  }

  return jit_exec (byte_code, args_template, nargs, args);

#undef JIT_NEXT
#undef NEXT
#undef CASE
#undef FIRST
#undef CASE_DEFAULT
#undef CASE_ABORT
#undef LABEL
}


/* Execute the byte-code in BYTESTR.  VECTOR is the constant vector, and
   MAXDEPTH is the maximum stack depth used (if MAXDEPTH is incorrect,
   emacs may crash!).  If ARGS_TEMPLATE is non-nil, it should be a lisp
   argument list (including &rest, &optional, etc.), and ARGS, of size
   NARGS, should be a vector of the actual arguments.  The arguments in
   ARGS are pushed on the stack according to ARGS_TEMPLATE before
   executing BYTESTR.  */

Lisp_Object
exec_byte_code (Lisp_Object bytestr, Lisp_Object vector, Lisp_Object maxdepth,
		Lisp_Object args_template, ptrdiff_t nargs, Lisp_Object *args)
{
  ptrdiff_t count = SPECPDL_INDEX ();
#ifdef BYTE_CODE_METER
  int volatile this_op = 0;
  int prev_op;
#endif
  int op;
  /* Lisp_Object v1, v2; */
  Lisp_Object *vectorp;
#ifdef BYTE_CODE_SAFE
  ptrdiff_t const_length;
  Lisp_Object *stacke;
  ptrdiff_t bytestr_length;
#endif
  struct byte_stack stack;
  Lisp_Object *top;
  Lisp_Object result;
  enum handlertype type;

#if 0 /* CHECK_FRAME_FONT */
 {
   struct frame *f = SELECTED_FRAME ();
   if (FRAME_X_P (f)
       && FRAME_FONT (f)->direction != 0
       && FRAME_FONT (f)->direction != 1)
     emacs_abort ();
 }
#endif

  CHECK_STRING (bytestr);
  CHECK_VECTOR (vector);
  CHECK_NATNUM (maxdepth);

#ifdef BYTE_CODE_SAFE
  const_length = ASIZE (vector);
#endif

  if (STRING_MULTIBYTE (bytestr))
    /* BYTESTR must have been produced by Emacs 20.2 or the earlier
       because they produced a raw 8-bit string for byte-code and now
       such a byte-code string is loaded as multibyte while raw 8-bit
       characters converted to multibyte form.  Thus, now we must
       convert them back to the originally intended unibyte form.  */
    bytestr = Fstring_as_unibyte (bytestr);

#ifdef BYTE_CODE_SAFE
  bytestr_length = SBYTES (bytestr);
#endif
  vectorp = XVECTOR (vector)->contents;

  stack.byte_string = bytestr;
  stack.pc = stack.byte_string_start = SDATA (bytestr);
  if (MAX_ALLOCA / word_size <= XFASTINT (maxdepth))
    memory_full (SIZE_MAX);
  top = alloca ((XFASTINT (maxdepth) + 1) * sizeof *top);
#if BYTE_MAINTAIN_TOP
  stack.bottom = top + 1;
  stack.top = NULL;
#endif
  stack.next = byte_stack_list;
  byte_stack_list = &stack;

#ifdef BYTE_CODE_SAFE
  stacke = stack.bottom - 1 + XFASTINT (maxdepth);
#endif

  if (INTEGERP (args_template))
    {
      ptrdiff_t at = XINT (args_template);
      bool rest = (at & 128) != 0;
      int mandatory = at & 127;
      ptrdiff_t nonrest = at >> 8;
      eassert (mandatory <= nonrest);
      if (nargs <= nonrest)
	{
	  ptrdiff_t i;
	  for (i = 0 ; i < nargs; i++, args++)
	    PUSH (*args);
	  if (nargs < mandatory)
	    /* Too few arguments.  */
	    Fsignal (Qwrong_number_of_arguments,
		     list2 (Fcons (make_number (mandatory),
				   rest ? Qand_rest : make_number (nonrest)),
			    make_number (nargs)));
	  else
	    {
	      for (; i < nonrest; i++)
		PUSH (Qnil);
	      if (rest)
		PUSH (Qnil);
	    }
	}
      else if (rest)
	{
	  ptrdiff_t i;
	  for (i = 0 ; i < nonrest; i++, args++)
	    PUSH (*args);
	  PUSH (Flist (nargs - nonrest, args));
	}
      else
	/* Too many arguments.  */
	Fsignal (Qwrong_number_of_arguments,
		 list2 (Fcons (make_number (mandatory), make_number (nonrest)),
			make_number (nargs)));
    }
  else if (! NILP (args_template))
    /* We should push some arguments on the stack.  */
    {
      error ("Unknown args template!");
    }

  while (1)
    {
#ifdef BYTE_CODE_SAFE
      if (top > stacke)
	emacs_abort ();
      else if (top < stack.bottom - 1)
	emacs_abort ();
#endif

#ifdef BYTE_CODE_METER
      prev_op = this_op;
      this_op = op = FETCH;
      METER_CODE (prev_op, op);
#else
#ifndef BYTE_CODE_THREADED
      op = FETCH;
#endif
#endif

      /* The interpreter can be compiled one of two ways: as an
	 ordinary switch-based interpreter, or as a threaded
	 interpreter.  The threaded interpreter relies on GCC's
	 computed goto extension, so it is not available everywhere.
	 Threading provides a performance boost.  These macros are how
	 we allow the code to be compiled both ways.  */
#ifdef BYTE_CODE_THREADED
      /* The CASE macro introduces an instruction's body.  It is
	 either a label or a case label.  */
#define CASE(OP) insn_ ## OP
      /* NEXT is invoked at the end of an instruction to go to the
	 next instruction.  It is either a computed goto, or a
	 plain break.  */
#define NEXT goto *(targets[op = FETCH])
      /* FIRST is like NEXT, but is only used at the start of the
	 interpreter body.  In the switch-based interpreter it is the
	 switch, so the threaded definition must include a semicolon.  */
#define FIRST NEXT;
      /* Most cases are labeled with the CASE macro, above.
	 CASE_DEFAULT is one exception; it is used if the interpreter
	 being built requires a default case.  The threaded
	 interpreter does not, because the dispatch table is
	 completely filled.  */
#define CASE_DEFAULT
      /* This introduces an instruction that is known to call abort.  */
#define CASE_ABORT CASE (Bstack_ref): CASE (default)
#else
      /* See above for the meaning of the various defines.  */
#define CASE(OP) case OP
#define NEXT break
#define FIRST switch (op)
#define CASE_DEFAULT case 255: default:
#define CASE_ABORT case 0
#endif

#ifdef BYTE_CODE_THREADED

      /* A convenience define that saves us a lot of typing and makes
	 the table clearer.  */
#define LABEL(OP) [OP] = &&insn_ ## OP

#if 4 < __GNUC__ + (6 <= __GNUC_MINOR__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Woverride-init"
#elif defined __clang__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Winitializer-overrides"
#endif

      /* This is the dispatch table for the threaded interpreter.  */
      static const void *const targets[256] =
	{
	  [0 ... (Bconstant - 1)] = &&insn_default,
	  [Bconstant ... 255] = &&insn_Bconstant,

#define DEFINE(name, value) LABEL (name) ,
	  BYTE_CODES
#undef DEFINE
	};

#if 4 < __GNUC__ + (6 <= __GNUC_MINOR__) || defined __clang__
# pragma GCC diagnostic pop
#endif

#endif


      FIRST
	{
	CASE (Bvarref7):
	  op = FETCH2;
	  goto varref;

	CASE (Bvarref):
	CASE (Bvarref1):
	CASE (Bvarref2):
	CASE (Bvarref3):
	CASE (Bvarref4):
	CASE (Bvarref5):
	  op = op - Bvarref;
	  goto varref;

	/* This seems to be the most frequently executed byte-code
	   among the Bvarref's, so avoid a goto here.  */
	CASE (Bvarref6):
	  op = FETCH;
	varref:
	  {
	    Lisp_Object v1, v2;

	    v1 = vectorp[op];
	    if (SYMBOLP (v1))
	      {
		if (XSYMBOL (v1)->redirect != SYMBOL_PLAINVAL
		    || (v2 = SYMBOL_VAL (XSYMBOL (v1)),
			EQ (v2, Qunbound)))
		  {
		    BEFORE_POTENTIAL_GC ();
		    v2 = Fsymbol_value (v1);
		    AFTER_POTENTIAL_GC ();
		  }
	      }
	    else
	      {
		BEFORE_POTENTIAL_GC ();
		v2 = Fsymbol_value (v1);
		AFTER_POTENTIAL_GC ();
	      }
	    PUSH (v2);
	    NEXT;
	  }

	CASE (Bgotoifnil):
	  {
	    Lisp_Object v1;
	    MAYBE_GC ();
	    op = FETCH2;
	    v1 = POP;
	    if (NILP (v1))
	      {
		BYTE_CODE_QUIT;
		CHECK_RANGE (op);
		stack.pc = stack.byte_string_start + op;
	      }
	    NEXT;
	  }

	CASE (Bcar):
	  {
	    Lisp_Object v1;
	    v1 = TOP;
	    if (CONSP (v1))
	      TOP = XCAR (v1);
	    else if (NILP (v1))
	      TOP = Qnil;
	    else
	      {
		BEFORE_POTENTIAL_GC ();
		wrong_type_argument (Qlistp, v1);
	      }
	    NEXT;
	  }

	CASE (Beq):
	  {
	    Lisp_Object v1;
	    v1 = POP;
	    TOP = EQ (v1, TOP) ? Qt : Qnil;
	    NEXT;
	  }

	CASE (Bmemq):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Fmemq (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bcdr):
	  {
	    Lisp_Object v1;
	    v1 = TOP;
	    if (CONSP (v1))
	      TOP = XCDR (v1);
	    else if (NILP (v1))
	      TOP = Qnil;
	    else
	      {
		BEFORE_POTENTIAL_GC ();
		wrong_type_argument (Qlistp, v1);
	      }
	    NEXT;
	  }

	CASE (Bvarset):
	CASE (Bvarset1):
	CASE (Bvarset2):
	CASE (Bvarset3):
	CASE (Bvarset4):
	CASE (Bvarset5):
	  op -= Bvarset;
	  goto varset;

	CASE (Bvarset7):
	  op = FETCH2;
	  goto varset;

	CASE (Bvarset6):
	  op = FETCH;
	varset:
	  {
	    Lisp_Object sym, val;

	    sym = vectorp[op];
	    val = TOP;

	    /* Inline the most common case.  */
	    if (SYMBOLP (sym)
		&& !EQ (val, Qunbound)
		&& !XSYMBOL (sym)->redirect
		&& !SYMBOL_CONSTANT_P (sym))
	      SET_SYMBOL_VAL (XSYMBOL (sym), val);
	    else
	      {
		BEFORE_POTENTIAL_GC ();
		set_internal (sym, val, Qnil, 0);
		AFTER_POTENTIAL_GC ();
	      }
	  }
	  (void) POP;
	  NEXT;

	CASE (Bdup):
	  {
	    Lisp_Object v1;
	    v1 = TOP;
	    PUSH (v1);
	    NEXT;
	  }

	/* ------------------ */

	CASE (Bvarbind6):
	  op = FETCH;
	  goto varbind;

	CASE (Bvarbind7):
	  op = FETCH2;
	  goto varbind;

	CASE (Bvarbind):
	CASE (Bvarbind1):
	CASE (Bvarbind2):
	CASE (Bvarbind3):
	CASE (Bvarbind4):
	CASE (Bvarbind5):
	  op -= Bvarbind;
	varbind:
	  /* Specbind can signal and thus GC.  */
	  BEFORE_POTENTIAL_GC ();
	  specbind (vectorp[op], POP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bcall6):
	  op = FETCH;
	  goto docall;

	CASE (Bcall7):
	  op = FETCH2;
	  goto docall;

	CASE (Bcall):
	CASE (Bcall1):
	CASE (Bcall2):
	CASE (Bcall3):
	CASE (Bcall4):
	CASE (Bcall5):
	  op -= Bcall;
	docall:
	  {
	    BEFORE_POTENTIAL_GC ();
	    DISCARD (op);
#ifdef BYTE_CODE_METER
	    if (byte_metering_on && SYMBOLP (TOP))
	      {
		Lisp_Object v1, v2;

		v1 = TOP;
		v2 = Fget (v1, Qbyte_code_meter);
		if (INTEGERP (v2)
		    && XINT (v2) < MOST_POSITIVE_FIXNUM)
		  {
		    XSETINT (v2, XINT (v2) + 1);
		    Fput (v1, Qbyte_code_meter, v2);
		  }
	      }
#endif
	    TOP = Ffuncall (op + 1, &TOP);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bunbind6):
	  op = FETCH;
	  goto dounbind;

	CASE (Bunbind7):
	  op = FETCH2;
	  goto dounbind;

	CASE (Bunbind):
	CASE (Bunbind1):
	CASE (Bunbind2):
	CASE (Bunbind3):
	CASE (Bunbind4):
	CASE (Bunbind5):
	  op -= Bunbind;
	dounbind:
	  BEFORE_POTENTIAL_GC ();
	  unbind_to (SPECPDL_INDEX () - op, Qnil);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bunbind_all):	/* Obsolete.  Never used.  */
	  /* To unbind back to the beginning of this frame.  Not used yet,
	     but will be needed for tail-recursion elimination.  */
	  BEFORE_POTENTIAL_GC ();
	  unbind_to (count, Qnil);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bgoto):
	  MAYBE_GC ();
	  BYTE_CODE_QUIT;
	  op = FETCH2;    /* pc = FETCH2 loses since FETCH2 contains pc++ */
	  CHECK_RANGE (op);
	  stack.pc = stack.byte_string_start + op;
	  NEXT;

	CASE (Bgotoifnonnil):
	  {
	    Lisp_Object v1;
	    MAYBE_GC ();
	    op = FETCH2;
	    v1 = POP;
	    if (!NILP (v1))
	      {
		BYTE_CODE_QUIT;
		CHECK_RANGE (op);
		stack.pc = stack.byte_string_start + op;
	      }
	    NEXT;
	  }

	CASE (Bgotoifnilelsepop):
	  MAYBE_GC ();
	  op = FETCH2;
	  if (NILP (TOP))
	    {
	      BYTE_CODE_QUIT;
	      CHECK_RANGE (op);
	      stack.pc = stack.byte_string_start + op;
	    }
	  else DISCARD (1);
	  NEXT;

	CASE (Bgotoifnonnilelsepop):
	  MAYBE_GC ();
	  op = FETCH2;
	  if (!NILP (TOP))
	    {
	      BYTE_CODE_QUIT;
	      CHECK_RANGE (op);
	      stack.pc = stack.byte_string_start + op;
	    }
	  else DISCARD (1);
	  NEXT;

	CASE (BRgoto):
	  MAYBE_GC ();
	  BYTE_CODE_QUIT;
	  stack.pc += (int) *stack.pc - 127;
	  NEXT;

	CASE (BRgotoifnil):
	  {
	    Lisp_Object v1;
	    MAYBE_GC ();
	    v1 = POP;
	    if (NILP (v1))
	      {
		BYTE_CODE_QUIT;
		stack.pc += (int) *stack.pc - 128;
	      }
	    stack.pc++;
	    NEXT;
	  }

	CASE (BRgotoifnonnil):
	  {
	    Lisp_Object v1;
	    MAYBE_GC ();
	    v1 = POP;
	    if (!NILP (v1))
	      {
		BYTE_CODE_QUIT;
		stack.pc += (int) *stack.pc - 128;
	      }
	    stack.pc++;
	    NEXT;
	  }

	CASE (BRgotoifnilelsepop):
	  MAYBE_GC ();
	  op = *stack.pc++;
	  if (NILP (TOP))
	    {
	      BYTE_CODE_QUIT;
	      stack.pc += op - 128;
	    }
	  else DISCARD (1);
	  NEXT;

	CASE (BRgotoifnonnilelsepop):
	  MAYBE_GC ();
	  op = *stack.pc++;
	  if (!NILP (TOP))
	    {
	      BYTE_CODE_QUIT;
	      stack.pc += op - 128;
	    }
	  else DISCARD (1);
	  NEXT;

	CASE (Breturn):
	  result = POP;
	  goto exit;

	CASE (Bdiscard):
	  DISCARD (1);
	  NEXT;

	CASE (Bconstant2):
	  PUSH (vectorp[FETCH2]);
	  NEXT;

	CASE (Bsave_excursion):
	  record_unwind_protect (save_excursion_restore,
				 save_excursion_save ());
	  NEXT;

	CASE (Bsave_current_buffer): /* Obsolete since ??.  */
	CASE (Bsave_current_buffer_1):
	  record_unwind_current_buffer ();
	  NEXT;

	CASE (Bsave_window_excursion): /* Obsolete since 24.1.  */
	  {
	    ptrdiff_t count1 = SPECPDL_INDEX ();
	    record_unwind_protect (restore_window_configuration,
				   Fcurrent_window_configuration (Qnil));
	    BEFORE_POTENTIAL_GC ();
	    TOP = Fprogn (TOP);
	    unbind_to (count1, TOP);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bsave_restriction):
	  record_unwind_protect (save_restriction_restore,
				 save_restriction_save ());
	  NEXT;

	CASE (Bcatch):		/* Obsolete since 24.4.  */
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = internal_catch (TOP, eval_sub, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bpushcatch):	/* New in 24.4.  */
	  type = CATCHER;
	  goto pushhandler;
	CASE (Bpushconditioncase): /* New in 24.4.  */
	  type = CONDITION_CASE;
	pushhandler:
	  {
	    Lisp_Object tag = POP;
	    int dest = FETCH2;

	    struct handler *c = push_handler (tag, type);
	    c->bytecode_dest = dest;
	    c->bytecode_top = top;

	    if (sys_setjmp (c->jmp))
	      {
		struct handler *c = handlerlist;
		int dest;
		top = c->bytecode_top;
		dest = c->bytecode_dest;
		handlerlist = c->next;
		PUSH (c->val);
		CHECK_RANGE (dest);
		/* Might have been re-set by longjmp!  */
		stack.byte_string_start = SDATA (stack.byte_string);
		stack.pc = stack.byte_string_start + dest;
	      }

	    NEXT;
	  }

	CASE (Bpophandler):	/* New in 24.4.  */
	  {
	    handlerlist = handlerlist->next;
	    NEXT;
	  }

	CASE (Bunwind_protect):	/* FIXME: avoid closure for lexbind.  */
	  {
	    Lisp_Object handler = POP;
	    /* Support for a function here is new in 24.4.  */
	    record_unwind_protect (NILP (Ffunctionp (handler))
				   ? unwind_body : bcall0,
				   handler);
	    NEXT;
	  }

	CASE (Bcondition_case):		/* Obsolete since 24.4.  */
	  {
	    Lisp_Object handlers, body;
	    handlers = POP;
	    body = POP;
	    BEFORE_POTENTIAL_GC ();
	    TOP = internal_lisp_condition_case (TOP, body, handlers);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Btemp_output_buffer_setup): /* Obsolete since 24.1.  */
	  BEFORE_POTENTIAL_GC ();
	  CHECK_STRING (TOP);
	  temp_output_buffer_setup (SSDATA (TOP));
	  AFTER_POTENTIAL_GC ();
	  TOP = Vstandard_output;
	  NEXT;

	CASE (Btemp_output_buffer_show): /* Obsolete since 24.1.  */
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    temp_output_buffer_show (TOP);
	    TOP = v1;
	    /* pop binding of standard-output */
	    unbind_to (SPECPDL_INDEX () - 1, Qnil);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bnth):
	  {
	    Lisp_Object v1, v2;
	    EMACS_INT n;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    v2 = TOP;
	    CHECK_NUMBER (v2);
	    n = XINT (v2);
	    immediate_quit = 1;
	    while (--n >= 0 && CONSP (v1))
	      v1 = XCDR (v1);
	    immediate_quit = 0;
	    TOP = CAR (v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bsymbolp):
	  TOP = SYMBOLP (TOP) ? Qt : Qnil;
	  NEXT;

	CASE (Bconsp):
	  TOP = CONSP (TOP) ? Qt : Qnil;
	  NEXT;

	CASE (Bstringp):
	  TOP = STRINGP (TOP) ? Qt : Qnil;
	  NEXT;

	CASE (Blistp):
	  TOP = CONSP (TOP) || NILP (TOP) ? Qt : Qnil;
	  NEXT;

	CASE (Bnot):
	  TOP = NILP (TOP) ? Qt : Qnil;
	  NEXT;

	CASE (Bcons):
	  {
	    Lisp_Object v1;
	    v1 = POP;
	    TOP = Fcons (TOP, v1);
	    NEXT;
	  }

	CASE (Blist1):
	  TOP = list1 (TOP);
	  NEXT;

	CASE (Blist2):
	  {
	    Lisp_Object v1;
	    v1 = POP;
	    TOP = list2 (TOP, v1);
	    NEXT;
	  }

	CASE (Blist3):
	  DISCARD (2);
	  TOP = Flist (3, &TOP);
	  NEXT;

	CASE (Blist4):
	  DISCARD (3);
	  TOP = Flist (4, &TOP);
	  NEXT;

	CASE (BlistN):
	  op = FETCH;
	  DISCARD (op - 1);
	  TOP = Flist (op, &TOP);
	  NEXT;

	CASE (Blength):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Flength (TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Baref):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Faref (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Baset):
	  {
	    Lisp_Object v1, v2;
	    BEFORE_POTENTIAL_GC ();
	    v2 = POP; v1 = POP;
	    TOP = Faset (TOP, v1, v2);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bsymbol_value):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Fsymbol_value (TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bsymbol_function):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Fsymbol_function (TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bset):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Fset (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bfset):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Ffset (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bget):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Fget (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bsubstring):
	  {
	    Lisp_Object v1, v2;
	    BEFORE_POTENTIAL_GC ();
	    v2 = POP; v1 = POP;
	    TOP = Fsubstring (TOP, v1, v2);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bconcat2):
	  BEFORE_POTENTIAL_GC ();
	  DISCARD (1);
	  TOP = Fconcat (2, &TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bconcat3):
	  BEFORE_POTENTIAL_GC ();
	  DISCARD (2);
	  TOP = Fconcat (3, &TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bconcat4):
	  BEFORE_POTENTIAL_GC ();
	  DISCARD (3);
	  TOP = Fconcat (4, &TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (BconcatN):
	  op = FETCH;
	  BEFORE_POTENTIAL_GC ();
	  DISCARD (op - 1);
	  TOP = Fconcat (op, &TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bsub1):
	  {
	    Lisp_Object v1;
	    v1 = TOP;
	    if (INTEGERP (v1))
	      {
		XSETINT (v1, XINT (v1) - 1);
		TOP = v1;
	      }
	    else
	      {
		BEFORE_POTENTIAL_GC ();
		TOP = Fsub1 (v1);
		AFTER_POTENTIAL_GC ();
	      }
	    NEXT;
	  }

	CASE (Badd1):
	  {
	    Lisp_Object v1;
	    v1 = TOP;
	    if (INTEGERP (v1))
	      {
		XSETINT (v1, XINT (v1) + 1);
		TOP = v1;
	      }
	    else
	      {
		BEFORE_POTENTIAL_GC ();
		TOP = Fadd1 (v1);
		AFTER_POTENTIAL_GC ();
	      }
	    NEXT;
	  }

	CASE (Beqlsign):
	  {
	    Lisp_Object v1, v2;
	    BEFORE_POTENTIAL_GC ();
	    v2 = POP; v1 = TOP;
	    CHECK_NUMBER_OR_FLOAT_COERCE_MARKER (v1);
	    CHECK_NUMBER_OR_FLOAT_COERCE_MARKER (v2);
	    AFTER_POTENTIAL_GC ();
	    if (FLOATP (v1) || FLOATP (v2))
	      {
		double f1, f2;

		f1 = (FLOATP (v1) ? XFLOAT_DATA (v1) : XINT (v1));
		f2 = (FLOATP (v2) ? XFLOAT_DATA (v2) : XINT (v2));
		TOP = (f1 == f2 ? Qt : Qnil);
	      }
	    else
	      TOP = (XINT (v1) == XINT (v2) ? Qt : Qnil);
	    NEXT;
	  }

	CASE (Bgtr):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = arithcompare (TOP, v1, ARITH_GRTR);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Blss):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = arithcompare (TOP, v1, ARITH_LESS);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bleq):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = arithcompare (TOP, v1, ARITH_LESS_OR_EQUAL);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bgeq):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = arithcompare (TOP, v1, ARITH_GRTR_OR_EQUAL);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bdiff):
	  BEFORE_POTENTIAL_GC ();
	  DISCARD (1);
	  TOP = Fminus (2, &TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bnegate):
	  {
	    Lisp_Object v1;
	    v1 = TOP;
	    if (INTEGERP (v1))
	      {
		XSETINT (v1, - XINT (v1));
		TOP = v1;
	      }
	    else
	      {
		BEFORE_POTENTIAL_GC ();
		TOP = Fminus (1, &TOP);
		AFTER_POTENTIAL_GC ();
	      }
	    NEXT;
	  }

	CASE (Bplus):
	  BEFORE_POTENTIAL_GC ();
	  DISCARD (1);
	  TOP = Fplus (2, &TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bmax):
	  BEFORE_POTENTIAL_GC ();
	  DISCARD (1);
	  TOP = Fmax (2, &TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bmin):
	  BEFORE_POTENTIAL_GC ();
	  DISCARD (1);
	  TOP = Fmin (2, &TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bmult):
	  BEFORE_POTENTIAL_GC ();
	  DISCARD (1);
	  TOP = Ftimes (2, &TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bquo):
	  BEFORE_POTENTIAL_GC ();
	  DISCARD (1);
	  TOP = Fquo (2, &TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Brem):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Frem (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bpoint):
	  {
	    Lisp_Object v1;
	    XSETFASTINT (v1, PT);
	    PUSH (v1);
	    NEXT;
	  }

	CASE (Bgoto_char):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Fgoto_char (TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Binsert):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Finsert (1, &TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (BinsertN):
	  op = FETCH;
	  BEFORE_POTENTIAL_GC ();
	  DISCARD (op - 1);
	  TOP = Finsert (op, &TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bpoint_max):
	  {
	    Lisp_Object v1;
	    XSETFASTINT (v1, ZV);
	    PUSH (v1);
	    NEXT;
	  }

	CASE (Bpoint_min):
	  {
	    Lisp_Object v1;
	    XSETFASTINT (v1, BEGV);
	    PUSH (v1);
	    NEXT;
	  }

	CASE (Bchar_after):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Fchar_after (TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bfollowing_char):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = Ffollowing_char ();
	    AFTER_POTENTIAL_GC ();
	    PUSH (v1);
	    NEXT;
	  }

	CASE (Bpreceding_char):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = Fprevious_char ();
	    AFTER_POTENTIAL_GC ();
	    PUSH (v1);
	    NEXT;
	  }

	CASE (Bcurrent_column):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    XSETFASTINT (v1, current_column ());
	    AFTER_POTENTIAL_GC ();
	    PUSH (v1);
	    NEXT;
	  }

	CASE (Bindent_to):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Findent_to (TOP, Qnil);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Beolp):
	  PUSH (Feolp ());
	  NEXT;

	CASE (Beobp):
	  PUSH (Feobp ());
	  NEXT;

	CASE (Bbolp):
	  PUSH (Fbolp ());
	  NEXT;

	CASE (Bbobp):
	  PUSH (Fbobp ());
	  NEXT;

	CASE (Bcurrent_buffer):
	  PUSH (Fcurrent_buffer ());
	  NEXT;

	CASE (Bset_buffer):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Fset_buffer (TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Binteractive_p):	/* Obsolete since 24.1.  */
	  BEFORE_POTENTIAL_GC ();
	  PUSH (call0 (intern ("interactive-p")));
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bforward_char):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Fforward_char (TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bforward_word):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Fforward_word (TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bskip_chars_forward):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Fskip_chars_forward (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bskip_chars_backward):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Fskip_chars_backward (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bforward_line):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Fforward_line (TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bchar_syntax):
	  {
	    int c;

	    BEFORE_POTENTIAL_GC ();
	    CHECK_CHARACTER (TOP);
	    AFTER_POTENTIAL_GC ();
	    c = XFASTINT (TOP);
	    if (NILP (BVAR (current_buffer, enable_multibyte_characters)))
	      MAKE_CHAR_MULTIBYTE (c);
	    XSETFASTINT (TOP, syntax_code_spec[SYNTAX (c)]);
	  }
	  NEXT;

	CASE (Bbuffer_substring):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Fbuffer_substring (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bdelete_region):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Fdelete_region (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bnarrow_to_region):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Fnarrow_to_region (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bwiden):
	  BEFORE_POTENTIAL_GC ();
	  PUSH (Fwiden ());
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bend_of_line):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Fend_of_line (TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bset_marker):
	  {
	    Lisp_Object v1, v2;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    v2 = POP;
	    TOP = Fset_marker (TOP, v2, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bmatch_beginning):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Fmatch_beginning (TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bmatch_end):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Fmatch_end (TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bupcase):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Fupcase (TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bdowncase):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Fdowncase (TOP);
	  AFTER_POTENTIAL_GC ();
	NEXT;

      CASE (Bstringeqlsign):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Fstring_equal (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bstringlss):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Fstring_lessp (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bequal):
	  {
	    Lisp_Object v1;
	    v1 = POP;
	    TOP = Fequal (TOP, v1);
	    NEXT;
	  }

	CASE (Bnthcdr):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Fnthcdr (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Belt):
	  {
	    Lisp_Object v1, v2;
	    if (CONSP (TOP))
	      {
		/* Exchange args and then do nth.  */
		EMACS_INT n;
		BEFORE_POTENTIAL_GC ();
		v2 = POP;
		v1 = TOP;
		CHECK_NUMBER (v2);
		AFTER_POTENTIAL_GC ();
		n = XINT (v2);
		immediate_quit = 1;
		while (--n >= 0 && CONSP (v1))
		  v1 = XCDR (v1);
		immediate_quit = 0;
		TOP = CAR (v1);
	      }
	    else
	      {
		BEFORE_POTENTIAL_GC ();
		v1 = POP;
		TOP = Felt (TOP, v1);
		AFTER_POTENTIAL_GC ();
	      }
	    NEXT;
	  }

	CASE (Bmember):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Fmember (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bassq):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Fassq (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bnreverse):
	  BEFORE_POTENTIAL_GC ();
	  TOP = Fnreverse (TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bsetcar):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Fsetcar (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bsetcdr):
	  {
	    Lisp_Object v1;
	    BEFORE_POTENTIAL_GC ();
	    v1 = POP;
	    TOP = Fsetcdr (TOP, v1);
	    AFTER_POTENTIAL_GC ();
	    NEXT;
	  }

	CASE (Bcar_safe):
	  {
	    Lisp_Object v1;
	    v1 = TOP;
	    TOP = CAR_SAFE (v1);
	    NEXT;
	  }

	CASE (Bcdr_safe):
	  {
	    Lisp_Object v1;
	    v1 = TOP;
	    TOP = CDR_SAFE (v1);
	    NEXT;
	  }

	CASE (Bnconc):
	  BEFORE_POTENTIAL_GC ();
	  DISCARD (1);
	  TOP = Fnconc (2, &TOP);
	  AFTER_POTENTIAL_GC ();
	  NEXT;

	CASE (Bnumberp):
	  TOP = (NUMBERP (TOP) ? Qt : Qnil);
	  NEXT;

	CASE (Bintegerp):
	  TOP = INTEGERP (TOP) ? Qt : Qnil;
	  NEXT;

#ifdef BYTE_CODE_SAFE
	  /* These are intentionally written using 'case' syntax,
	     because they are incompatible with the threaded
	     interpreter.  */

	case Bset_mark:
	  BEFORE_POTENTIAL_GC ();
	  error ("set-mark is an obsolete bytecode");
	  AFTER_POTENTIAL_GC ();
	  break;
	case Bscan_buffer:
	  BEFORE_POTENTIAL_GC ();
	  error ("scan-buffer is an obsolete bytecode");
	  AFTER_POTENTIAL_GC ();
	  break;
#endif

	CASE_ABORT:
	  /* Actually this is Bstack_ref with offset 0, but we use Bdup
	     for that instead.  */
	  /* CASE (Bstack_ref): */
	  call3 (Qerror,
		 build_string ("Invalid byte opcode: op=%s, ptr=%d"),
		 make_number (op),
		 make_number ((stack.pc - 1) - stack.byte_string_start));

	  /* Handy byte-codes for lexical binding.  */
	CASE (Bstack_ref1):
	CASE (Bstack_ref2):
	CASE (Bstack_ref3):
	CASE (Bstack_ref4):
	CASE (Bstack_ref5):
	  {
	    Lisp_Object *ptr = top - (op - Bstack_ref);
	    PUSH (*ptr);
	    NEXT;
	  }
	CASE (Bstack_ref6):
	  {
	    Lisp_Object *ptr = top - (FETCH);
	    PUSH (*ptr);
	    NEXT;
	  }
	CASE (Bstack_ref7):
	  {
	    Lisp_Object *ptr = top - (FETCH2);
	    PUSH (*ptr);
	    NEXT;
	  }
	CASE (Bstack_set):
	  /* stack-set-0 = discard; stack-set-1 = discard-1-preserve-tos.  */
	  {
	    Lisp_Object *ptr = top - (FETCH);
	    *ptr = POP;
	    NEXT;
	  }
	CASE (Bstack_set2):
	  {
	    Lisp_Object *ptr = top - (FETCH2);
	    *ptr = POP;
	    NEXT;
	  }
	CASE (BdiscardN):
	  op = FETCH;
	  if (op & 0x80)
	    {
	      op &= 0x7F;
	      top[-op] = TOP;
	    }
	  DISCARD (op);
	  NEXT;

	CASE_DEFAULT
	CASE (Bconstant):
#ifdef BYTE_CODE_SAFE
	  if (op < Bconstant)
	    {
	      emacs_abort ();
	    }
	  if ((op -= Bconstant) >= const_length)
	    {
	      emacs_abort ();
	    }
	  PUSH (vectorp[op]);
#else
	  PUSH (vectorp[op - Bconstant]);
#endif
	  NEXT;
	}
    }

 exit:

  byte_stack_list = byte_stack_list->next;

  /* Binds and unbinds are supposed to be compiled balanced.  */
  if (SPECPDL_INDEX () != count)
    {
      if (SPECPDL_INDEX () > count)
	unbind_to (count, Qnil);
      error ("binding stack not balanced (serious byte compiler bug)");
    }

  return result;
}

void
syms_of_bytecode (void)
{
  defsubr (&Sbyte_code);

#ifdef BYTE_CODE_METER

  DEFVAR_LISP ("byte-code-meter", Vbyte_code_meter,
	       doc: /* A vector of vectors which holds a histogram of byte-code usage.
\(aref (aref byte-code-meter 0) CODE) indicates how many times the byte
opcode CODE has been executed.
\(aref (aref byte-code-meter CODE1) CODE2), where CODE1 is not 0,
indicates how many times the byte opcodes CODE1 and CODE2 have been
executed in succession.  */);

  DEFVAR_BOOL ("byte-metering-on", byte_metering_on,
	       doc: /* If non-nil, keep profiling information on byte code usage.
The variable byte-code-meter indicates how often each byte opcode is used.
If a symbol has a property named `byte-code-meter' whose value is an
integer, it is incremented each time that symbol's function is called.  */);

  byte_metering_on = 0;
  Vbyte_code_meter = Fmake_vector (make_number (256), make_number (0));
  DEFSYM (Qbyte_code_meter, "byte-code-meter");
  {
    int i = 256;
    while (i--)
      ASET (Vbyte_code_meter, i,
           Fmake_vector (make_number (256), make_number (0)));
  }

  DEFVAR_BOOL ("byte-code-jit-on", byte_code_jit_on,
	       doc: /* If non-nil, compile byte-code to machine code before executing */);
  byte_code_jit_on = 0;
#endif
}
