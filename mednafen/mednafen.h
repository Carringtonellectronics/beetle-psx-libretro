#ifndef _MEDNAFEN_H
#define _MEDNAFEN_H

#include "mednafen-types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _(String) (String)

#include "math_ops.h"
#include "git.h"

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

#define GET_FDATA_PTR(fp) (fp->data)
#define GET_FSIZE_PTR(fp) (fp->size)
#define GET_FEXTS_PTR(fp) (fp->ext)

#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(ar) (sizeof(ar)/sizeof((ar)[0]))
#endif
#define MEMORY_ALIGNED16(x) __attribute__((aligned(16))) x

#define _assert_(_a_) assert(_a_)
#define _assert_msg_(_type_, _cond_, ...) if(!(_cond_)){log_cb(RETRO_LOG_ERROR, __VA_ARGS__); exit(-1);}
#define PanicAlert(...) _assert_msg_(PANIC, 0, __VA_ARGS__) 

#ifdef DEBUG
#define _dbg_assert_(_type_, _cond_) assert(_cond_)
#define _dbg_assert_msg_(_type_, _cond_, ...) if(!(_cond_)) {log_cb(RETRO_LOG_ERROR, __VA_ARGS__); exit(-1);}
#else
#define _dbg_assert_(_type_, _cond_)
#define _dbg_assert_msg_(_type_, _cond_, ...) 
#endif

#define INFO_LOG(_type_, ...) log_cb(RETRO_LOG_INFO, __VA_ARGS__)
#define WARN_LOG(_type_, ...) log_cb(RETRO_LOG_WARN, __VA_ARGS__)
#define ERROR_LOG(_type_, ...) log_cb(RETRO_LOG_ERROR, __VA_ARGS__)
#define DEBUG_LOG(_type_, ...) log_cb(RETRO_LOG_INFO, __VA_ARGS__)

#define WARN_LOG_REPORT(_type_, ...) WARN_LOG(_type_, __VA_ARGS__)
#define ERROR_LOG_REPORT(_type_, ...) ERROR_LOG(_type_, __VA_ARGS__)

extern MDFNGI *MDFNGameInfo;

#include "settings.h"

void MDFN_DispMessage(const char *format, ...);

void MDFN_LoadGameCheats(void *override);
void MDFN_FlushGameCheats(int nosave);

#include "mednafen-driver.h"

// An inheritable class to disallow the copy constructor and operator= functions
class NonCopyable
{
protected:
	NonCopyable() {}
private:
	NonCopyable(const NonCopyable&);
	void operator=(const NonCopyable&);
};
//Maybe should be in some memory class?
//Anyways, just byte order swap stuff
#undef swap16
#undef swap32
#undef swap64

#ifdef _WIN32
inline uint16_t swap16(uint16_t _data) {return _byteswap_ushort(_data);}
inline uint32_t swap32(uint32_t _data) {return _byteswap_ulong (_data);}
inline uint64_t swap64(uint64_t _data) {return _byteswap_uint64(_data);}
#elif defined(__GNUC__)
inline uint16_t swap16(uint16_t _data) {return __builtin_bswap16(_data);}
inline uint32_t swap32(uint32_t _data) {return __builtin_bswap32(_data);}
inline uint64_t swap64(uint64_t _data) {return __builtin_bswap64(_data);}
#else
// Slow generic implementation. Hopefully this never hits
inline uint16_t swap16(uint16_t data) {return (data >> 8) | (data << 8);}
inline uint32_t swap32(uint32_t data) {return (swap16(data) << 16) | swap16(data >> 16);}
inline uint64_t swap64(uint64_t data) {return ((uint64_t)swap32(data) << 32) | swap32(data >> 32);}
#endif

inline uint16_t swap16(const uint8_t* _pData) {return swap16(*(const uint16_t*)_pData);}
inline uint32_t swap32(const uint8_t* _pData) {return swap32(*(const uint32_t*)_pData);}
inline uint64_t swap64(const uint8_t* _pData) {return swap64(*(const uint64_t*)_pData);}

#endif