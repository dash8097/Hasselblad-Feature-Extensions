#pragma once
#include "key_filter.hpp"
namespace x2dui {
struct KeyMapProfile {void*const*instance;const void*meta;const void*(*cast)(const void*,const void*);bool(*is_fn)(const void*,int,unsigned,int);bool(*unblockable)(const void*,int,unsigned);};
// Fixed 4.2.0 meta-enumeration, independently asserted by profile generation.
enum class KeyFunction:int {Menu=1,Escape=5,Abort=6,Select=8,HalfPress=11,PopAccept=28,JstkEscape=31};
inline const void*key_instance(const KeyMapProfile&p){void*object=p.instance?*p.instance:nullptr;return object&&p.cast?p.cast(p.meta,object):nullptr;}
inline bool key_matches(const KeyMapProfile&p,const KeyInput&k,KeyFunction f){auto*object=key_instance(p);return object&&p.is_fn(object,k.key,static_cast<unsigned>(k.modifiers),static_cast<int>(f));}
inline bool key_unblockable(const KeyMapProfile&p,const KeyInput&k){auto*object=key_instance(p);return object&&p.unblockable(object,k.key,static_cast<unsigned>(k.modifiers));}
}
