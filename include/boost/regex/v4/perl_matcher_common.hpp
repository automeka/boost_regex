/*
 *
 * Copyright (c) 2002
 * Dr John Maddock
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Dr John Maddock makes no representations
 * about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 *
 */

 /*
  *   LOCATION:    see http://www.boost.org for most recent version.
  *   FILE         perl_matcher_common.cpp
  *   VERSION      see <boost/version.hpp>
  *   DESCRIPTION: Definitions of perl_matcher member functions that are 
  *                common to both the recursive and non-recursive versions.
  */

#ifndef BOOST_REGEX_V4_PERL_MATCHER_COMMON_HPP
#define BOOST_REGEX_V4_PERL_MATCHER_COMMON_HPP

#ifdef __BORLANDC__
#  pragma option push -a8 -b -Vx -Ve -pc -w-8027
#endif

namespace boost{
namespace re_detail{

template <class BidiIterator, class Allocator, class traits, class Allocator2>
perl_matcher<BidiIterator, Allocator, traits, Allocator2>::perl_matcher(BidiIterator first, BidiIterator end, 
   match_results<BidiIterator, Allocator>& what, 
   const reg_expression<char_type, traits, Allocator2>& e,
   match_flag_type f)
   :  m_result(what), base(first), last(end), 
      position(first), re(e), traits_inst(e.get_traits()), 
      next_count(&rep_obj), rep_obj(&next_count)
{ 
   typedef typename regex_iterator_traits<BidiIterator>::iterator_category category;

   pstate = 0;
   m_match_flags = f;
   icase = re.flags() & regbase::icase;
   estimate_max_state_count(static_cast<category*>(0));
   if(!(m_match_flags & (match_perl|match_posix)))
   {
      if(re.flags() & regbase::perlex)
         m_match_flags |= match_perl;
      else
         m_match_flags |= match_posix;
   }
   if(m_match_flags & match_posix)
   {
      m_temp_match.reset(new match_results<BidiIterator, Allocator>());
      m_presult = m_temp_match.get();
   }
   else
      m_presult = &m_result;
#ifdef BOOST_REGEX_NON_RECURSIVE
   m_stack_base = 0;
   m_backup_state = 0;
#endif
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
void perl_matcher<BidiIterator, Allocator, traits, Allocator2>::estimate_max_state_count(std::random_access_iterator_tag*)
{
   difference_type dist = boost::re_detail::distance(base, last);
   traits_size_type states = static_cast<traits_size_type>(re.size());
   states *= states;
   difference_type lim = std::numeric_limits<difference_type>::max() - 1000 - states;
   if(dist > (difference_type)(lim / states))
      max_state_count = lim;
   else
      max_state_count = 1000 + states * dist;
}
template <class BidiIterator, class Allocator, class traits, class Allocator2>
void perl_matcher<BidiIterator, Allocator, traits, Allocator2>::estimate_max_state_count(void*)
{
   // we don't know how long the sequence is:
   max_state_count = BOOST_REGEX_MAX_STATE_COUNT;
}



template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match()
{
#ifdef BOOST_REGEX_HAS_MS_STACK_GUARD
   __try{
#endif
   // initialise our stack if we are non-recursive:
#ifdef BOOST_REGEX_NON_RECURSIVE
   save_state_init init(&m_stack_base, &m_backup_state);
   used_block_count = BOOST_REGEX_MAX_BLOCKS;
#endif

   // reset our state machine:
   position = base;
   search_base = base;
   state_count = 0;
   m_presult->set_size(re.mark_count(), base, last);
   m_presult->set_base(base);
   if(m_match_flags & match_posix)
      m_result = *m_presult;
   if(0 == match_prefix())
      return false;
   return m_result[0].second == last;

#ifdef BOOST_REGEX_HAS_MS_STACK_GUARD
   }__except(EXCEPTION_STACK_OVERFLOW == GetExceptionCode())
   {
      reset_stack_guard_page();
   }
   // we only get here after a stack overflow:
   raise_error<traits>(traits_inst, REG_E_MEMORY);
   // and we never really get here at all:
   return false;
#endif
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::find()
{
#ifdef BOOST_REGEX_HAS_MS_STACK_GUARD
  __try{
#endif

   // initialise our stack if we are non-recursive:
#ifdef BOOST_REGEX_NON_RECURSIVE
   save_state_init init(&m_stack_base, &m_backup_state);
   used_block_count = BOOST_REGEX_MAX_BLOCKS;
#endif

   state_count = 0;
   if((m_match_flags & match_init) == 0)
   {
      // reset our state machine:
      position = base;
      search_base = base;
      pstate = access::first(re);
      m_presult->set_size(re.mark_count(), base, last);
      m_presult->set_base(base);
      m_match_flags |= match_init;
   }
   else
   {
      // start again:
      search_base = position = (*m_presult)[0].second;
      // If last match was null and match_not_null was not set then increment
      // our start position, otherwise we go into an infinite loop:
      if(((m_match_flags & match_not_null) == 0) && (m_presult->length() == 0))
      {
         if(position == last)
            return false;
         else 
            ++position;
      }
      // reset $` start:
      m_presult->set_size(re.mark_count(), search_base, last);
      if(base != search_base)
         m_match_flags |= match_prev_avail;
   }
   if(m_match_flags & match_posix)
   {
      m_result.set_size(re.mark_count(), base, last);
      m_result.set_base(base);
   }

   // find out what kind of expression we have:
   unsigned type = (m_match_flags & match_continuous) ? 
      static_cast<unsigned int>(regbase::restart_continue) 
         : static_cast<unsigned int>(access::restart_type(re));

   // call the appropriate search routine:
   matcher_proc_type proc = s_find_vtable[type];
   return (this->*proc)();

#ifdef BOOST_REGEX_HAS_MS_STACK_GUARD
   }__except(EXCEPTION_STACK_OVERFLOW == GetExceptionCode())
   {
      reset_stack_guard_page();
   }
   // we only get here after a stack overflow:
   raise_error<traits>(traits_inst, REG_E_MEMORY);
   // and we never really get here at all:
   return false;
#endif
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_prefix()
{
   m_has_partial_match = false;
   m_has_found_match = false;
   pstate = access::first(re);
   m_presult->set_first(position);
   restart = position;
   match_all_states();
   if(!m_has_found_match && m_has_partial_match && (m_match_flags & match_partial))
   {
      m_has_found_match = true;
      m_presult->set_second(last, 0, false);
      position = last;
   }
   if(!m_has_found_match)
      position = restart; // reset search postion
   return m_has_found_match;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_endmark()
{
   int index = static_cast<const re_brace*>(pstate)->index;
   if(index > 0)
   {
      m_presult->set_second(position, index);
   }
   else if(index < 0)
   {
      // matched forward lookahead:
      pstate = 0;
      return true;
   }
   pstate = pstate->next.p;
   return true;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_literal()
{
   unsigned int len = static_cast<const re_literal*>(pstate)->length;
   const char_type* what = reinterpret_cast<const char_type*>(static_cast<const re_literal*>(pstate) + 1);
   //
   // compare string with what we stored in
   // our records:
   for(unsigned int i = 0; i < len; ++i, ++position)
   {
      if((position == last) || (traits_inst.translate(*position, icase) != what[i]))
         return false;
   }
   pstate = pstate->next.p;
   return true;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_start_line()
{
   if((position == base) && ((m_match_flags & match_prev_avail) == 0))
   {
      if((m_match_flags & match_not_bol) == 0)
      {
         pstate = pstate->next.p;
         return true;
      }
      return false;
   }

   // check the previous value character:
   BidiIterator t(position);
   --t;
   if(position != last)
   {
      if(traits_inst.is_separator(*t) && !((*t == '\r') && (*position == '\n')) )
      {
         pstate = pstate->next.p;
         return true;
      }
   }
   return false;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_end_line()
{
   if(position != last)
   {
      // we're not yet at the end so *first is always valid:
      if(traits_inst.is_separator(*position))
      {
         if((position != base) || (m_match_flags & match_prev_avail))
         {
            // check that we're not in the middle of \r\n sequence
            BidiIterator t(position);
            --t;
            if((*t == '\r') && (*position == '\n'))
            {
               return false;
            }
         }
         pstate = pstate->next.p;
         return true;
      }
   }
   else if((m_match_flags & match_not_eol) == 0)
   {
      pstate = pstate->next.p;
      return true;
   }
   return false;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_wild()
{
   if(position == last) 
      return false;
   if(traits_inst.is_separator(*position) && (m_match_flags & match_not_dot_newline))
      return false;
   if((*position == char_type(0)) && (m_match_flags & match_not_dot_null))
      return false;
   pstate = pstate->next.p;
   ++position;
   return true;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_match()
{
   if((m_match_flags & match_not_null) && (position == (*m_presult)[0].first))
      return false;
   if((m_match_flags & match_all) && (position != last))
      return false;
   m_presult->set_second(position);
   pstate = 0;
   m_has_found_match = true;
   if((m_match_flags & (match_posix|match_any)) == match_posix)
   {
      m_result.maybe_assign(*m_presult);
      return false;
   }
   return true;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_word_boundary()
{
   bool b; // indcates whether next character is a word character
   if(position != last)
   {
      // prev and this character must be opposites:
   #if defined(BOOST_REGEX_USE_C_LOCALE) && defined(__GNUC__) && (__GNUC__ == 2) && (__GNUC_MINOR__ < 95)
      b = traits::is_class(*position, traits::char_class_word);
   #else
      b = traits_inst.is_class(*position, traits::char_class_word);
   #endif
   }
   else
   {
      b = (m_match_flags & match_not_eow) ? true : false;
   }
   if((position == base)  && ((m_match_flags & match_prev_avail) == 0))
   {
      if(m_match_flags & match_not_bow)
         b ^= true;
      else
         b ^= false;
   }
   else
   {
      --position;
      b ^= traits_inst.is_class(*position, traits::char_class_word);
      ++position;
   }
   if(b)
   {
      pstate = pstate->next.p;
      return true;
   }
   return false; // no match if we get to here...
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_within_word()
{
   if(position == last)
      return false;
   // both prev and this character must be traits::char_class_word:
   if(traits_inst.is_class(*position, traits::char_class_word))
   {
      bool b;
      if((position == base) && ((m_match_flags & match_prev_avail) == 0))
         return false;
      else
      {
         --position;
         b = traits_inst.is_class(*position, traits::char_class_word);
         ++position;
      }
      if(b)
      {
         pstate = pstate->next.p;
         return true;
      }
   }
   return false;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_word_start()
{
   if(position == last)
      return false; // can't be starting a word if we're already at the end of input
   if(!traits_inst.is_class(*position, traits::char_class_word))
      return false; // next character isn't a word character
   if((position == base) && ((m_match_flags & match_prev_avail) == 0))
   {
      if(m_match_flags & match_not_bow)
         return false; // no previous input
   }
   else
   {
      // otherwise inside buffer:
      BidiIterator t(position);
      --t;
      if(traits_inst.is_class(*t, traits::char_class_word))
         return false; // previous character not non-word
   }
   // OK we have a match:
   pstate = pstate->next.p;
   return true;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_word_end()
{
   if((position == base) && ((m_match_flags & match_prev_avail) == 0))
      return false;  // start of buffer can't be end of word
   BidiIterator t(position);
   --t;
   if(traits_inst.is_class(*t, traits::char_class_word) == false)
      return false;  // previous character wasn't a word character

   if(position == last)
   {
      if(m_match_flags & match_not_eow)
         return false; // end of buffer but not end of word
   }
   else
   {
      // otherwise inside buffer:
      if(traits_inst.is_class(*position, traits::char_class_word))
         return false; // next character is a word character
   }
   pstate = pstate->next.p;
   return true;      // if we fall through to here then we've succeeded
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_buffer_start()
{
   if((position != base) || (m_match_flags & match_not_bob))
      return false;
   // OK match:
   pstate = pstate->next.p;
   return true;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_buffer_end()
{
   if((position != last) || (m_match_flags & match_not_eob))
      return false;
   // OK match:
   pstate = pstate->next.p;
   return true;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_backref()
{
   // compare with what we previously matched:
   BidiIterator i = (*m_presult)[static_cast<const re_brace*>(pstate)->index].first;
   BidiIterator j = (*m_presult)[static_cast<const re_brace*>(pstate)->index].second;
   while(i != j)
   {
      if((position == last) || (traits_inst.translate(*position, icase) != traits_inst.translate(*i, icase)))
         return false;
      ++i;
      ++position;
   }
   pstate = pstate->next.p;
   return true;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_long_set()
{
   // let the traits class do the work:
   if(position == last)
      return false;
   BidiIterator t = re_is_set_member(position, last, static_cast<const re_set_long*>(pstate), re);
   if(t != position)
   {
      pstate = pstate->next.p;
      position = t;
      return true;
   }
   return false;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_set()
{
   if(position == last)
      return false;
   if(static_cast<const re_set*>(pstate)->_map[(traits_uchar_type)traits_inst.translate(*position, icase)])
   {
      pstate = pstate->next.p;
      ++position;
      return true;
   }
   return false;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_jump()
{
   pstate = static_cast<const re_jump*>(pstate)->alt.p;
   return true;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_combining()
{
   if(position == last)
      return false;
   if(traits_inst.is_combining(traits_inst.translate(*position, icase)))
      return false;
   ++position;
   while((position != last) && traits_inst.is_combining(traits_inst.translate(*position, icase)))
      ++position;
   pstate = pstate->next.p;
   return true;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_soft_buffer_end()
{
   if(m_match_flags & match_not_eob)
      return false;
   BidiIterator p(position);
   while((p != last) && traits_inst.is_separator(traits_inst.translate(*p, icase)))++p;
   if(p != last)
      return false;
   pstate = pstate->next.p;
   return true;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_restart_continue()
{
   if(position == search_base)
   {
      pstate = pstate->next.p;
      return true;
   }
   return false;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::find_restart_any()
{
#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable:4127)
#endif
   const unsigned char* _map = access::get_map(re);
   while(true)
   {
      // skip everything we can't match:
      while((position != last) && !access::can_start(*position, _map, (unsigned char)mask_any) )
         ++position;
      if(position == last)
      {
         // run out of characters, try a null match if possible:
         if(access::first(re)->can_be_null)
            return match_prefix();
         return false;
      }
      // now try and obtain a match:
      if(match_prefix())
         return true;
      if(position == last)
         return false;
      ++position;
   }
   return false;
#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::find_restart_word()
{
#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable:4127)
#endif
   // do search optimised for word starts:
   const unsigned char* _map = access::get_map(re);
   if((m_match_flags & match_prev_avail) || (position != base))
      --position;
   else if(match_prefix())
      return true;
   do
   {
      while((position != last) && traits_inst.is_class(*position, traits::char_class_word))
         ++position;
      while((position != last) && !traits_inst.is_class(*position, traits::char_class_word))
         ++position;
      if(position == last)
         return false;

      if(access::can_start(*position, _map, (unsigned char)mask_any) )
      {
         if(match_prefix())
            return true;
      }
      if(position == last)
         return false;
   } while(true);
   return false;
#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::find_restart_line()
{
   // do search optimised for line starts:
   const unsigned char* _map = access::get_map(re);
   if(match_prefix())
      return true;
   while(position != last)
   {
      while((position != last) && (*position != '\n'))
         ++position;
      if(position == last)
         return false;
      ++position;
      if(position == last)
         return false;

      if( access::can_start(*position, _map, (unsigned char)mask_any) )
      {
         if(match_prefix())
            return true;
      }
      if(position == last)
         return false;
      ++position;
   }
   return false;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::find_restart_buf()
{
   if((position == base) && ((m_match_flags & match_not_bob) == 0))
      return match_prefix();
   return false;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
bool perl_matcher<BidiIterator, Allocator, traits, Allocator2>::find_restart_lit()
{
   if(position == last)
      return false; // can't possibly match if we're at the end already

   unsigned type = (m_match_flags & match_continuous) ? 
      static_cast<unsigned int>(regbase::restart_continue) 
         : static_cast<unsigned int>(access::restart_type(re));

   const kmp_info<char_type>* info = access::get_kmp(re);
   int len = info->len;
   const char_type* x = info->pstr;
   int j = 0; 
   bool icase = re.flags() & regbase::icase;
   while (position != last) 
   {
      while((j > -1) && (x[j] != traits_inst.translate(*position, icase))) 
         j = info->kmp_next[j];
      ++position;
      ++j;
      if(j >= len) 
      {
         if(type == regbase::restart_fixed_lit)
         {
            std::advance(position, -j);
            restart = position;
            std::advance(restart, len);
            m_result.set_first(position);
            m_result.set_second(restart);
            position = restart;
            return true;
         }
         else
         {
            restart = position;
            std::advance(position, -j);
            if(match_prefix())
               return true;
            else
            {
               for(int k = 0; (restart != position) && (k < j); ++k, --restart)
                     {} // dwa 10/20/2000 - warning suppression for MWCW
               if(restart != last)
                  ++restart;
               position = restart;
               j = 0;  //we could do better than this...
            }
         }
      }
   }
   if((m_match_flags & match_partial) && (position == last) && j)
   {
      // we need to check for a partial match:
      restart = position;
      std::advance(position, -j);
      return match_prefix();
   }
   return false;
}

template <class BidiIterator, class Allocator, class traits, class Allocator2>
typename perl_matcher<BidiIterator, Allocator, traits, Allocator2>::matcher_proc_type const
perl_matcher<BidiIterator, Allocator, traits, Allocator2>::s_match_vtable[] = 
{
   (&perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_startmark),
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_endmark,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_literal,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_start_line,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_end_line,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_wild,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_match,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_word_boundary,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_within_word,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_word_start,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_word_end,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_buffer_start,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_buffer_end,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_backref,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_long_set,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_set,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_jump,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_alt,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_rep,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_combining,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_soft_buffer_end,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_restart_continue,
#if defined(BOOST_MSVC) && (BOOST_MSVC <= 1300)
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_dot_repeat_fast,
#else
   (::boost::is_random_access_iterator<BidiIterator>::value ? &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_dot_repeat_fast : &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_dot_repeat_slow),
#endif
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_char_repeat,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_set_repeat,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_long_set_repeat,

};

template <class BidiIterator, class Allocator, class traits, class Allocator2>
typename perl_matcher<BidiIterator, Allocator, traits, Allocator2>::matcher_proc_type const
perl_matcher<BidiIterator, Allocator, traits, Allocator2>::s_find_vtable[] = 
{
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::find_restart_any,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::find_restart_word,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::find_restart_line,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::find_restart_buf,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::match_prefix,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::find_restart_lit,
   &perl_matcher<BidiIterator, Allocator, traits, Allocator2>::find_restart_lit,
};

} // namespace re_detail

} // namespace boost

#ifdef __BORLANDC__
#  pragma option pop
#endif

#endif