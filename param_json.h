#ifndef PARAM_JSON_H
#define PARAM_JSON_H

#include <stdint.h>
#include "params.h"

#ifdef ARDUINO
#include <Arduino.h>

namespace ParamJson
{
   void Build();
   const String& Get();
   uint32_t GetSize();
   int GetByte(uint32_t offset);
   void BeginStream();
   size_t Read(uint8_t* out, size_t maxLen);
}
#else
namespace ParamJson
{
   inline void Build() {}
   inline uint32_t GetSize() { return 0; }
   inline int GetByte(uint32_t) { return -1; }
   inline void BeginStream() {}
   inline size_t Read(uint8_t*, size_t) { return 0; }
}
#endif

#endif // PARAM_JSON_H
