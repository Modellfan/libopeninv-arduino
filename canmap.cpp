/*
 * Arduino port of libopeninv canmap
 * Uses EEPROM for persistence.
 */
#include <Arduino.h>
#include <EEPROM.h>
#include "canmap.h"
#include "my_math.h"

static const int kCanMapEepromBase = 2048;

#define ITEM_UNSET            0xff
#define forEachCanMap(c,m) for (CANIDMAP *c = m; (c - m) < MAX_MESSAGES && c->first != MAX_ITEMS; c++)
#define forEachPosMap(c,m) for (CANPOS *c = &canPosMap[m->first]; c->next != ITEM_UNSET; c = &canPosMap[c->next])
#define IS_EXT_FORCE(id)      ((SHIFT_FORCE_FLAG(1) & id) != 0)
#define MASK_EXT_FORCE(id)    (id & ~SHIFT_FORCE_FLAG(1))

#ifdef CAN_EXT
#define IDMAPSIZE 8
#define SHIFT_FORCE_FLAG(f) (f << 29)
#else
#define IDMAPSIZE 4
#define SHIFT_FORCE_FLAG(f) (f << 11)
#endif // CAN_EXT

volatile bool CanMap::isSaving = false;

// Simple CRC32 for flash verification
static uint32_t calculate_crc32_block(uint32_t *data, uint32_t length)
{
   uint32_t crc = 0xFFFFFFFF;
   for (uint32_t i = 0; i < length; i++)
   {
      crc ^= data[i];
      for (int j = 0; j < 32; j++)
      {
         if (crc & 1)
            crc = (crc >> 1) ^ 0xEDB88320;
         else
            crc = crc >> 1;
      }
   }
   return ~crc;
}

CanMap::CanMap(CanHardware* hw, bool loadFromFlash)
 : canHardware(hw)
{
   ClearMap(canSendMap);
   ClearMap(canRecvMap);
   if (loadFromFlash) LoadFromFlash();
   HandleClear();
}

void CanMap::HandleClear()
{
   forEachCanMap(curMap, canRecvMap)
   {
      bool forceExtended = IS_EXT_FORCE(curMap->canId);
      canHardware->RegisterUserMessage((curMap->canId & ~SHIFT_FORCE_FLAG(1)) + (forceExtended * CAN_FORCE_EXTENDED));
   }
}

void CanMap::HandleRx(uint32_t canId, uint32_t data[2], uint8_t)
{
   if (isSaving) return;

   CANIDMAP *recvMap = FindById(canRecvMap, canId);

   if (0 != recvMap)
   {
      forEachPosMap(curPos, recvMap)
      {
         uint32_t word;
         uint8_t pos = curPos->offsetBits;
         uint8_t numBits = ABS(curPos->numBits);

         if (curPos->numBits < 0) // big endian
         {
            if (curPos->offsetBits < 32)
            {
               word = data[0];
            }
            else if ((curPos->offsetBits + curPos->numBits) > 31)
            {
               word = data[1];
               pos -= 32;
            }
            else
            {
               pos = pos - numBits + 1;
               word = data[0] >> pos;
               word |= data[1] << (32 - pos);
               pos = numBits - 1;
            }

            const uint8_t* bptr = (uint8_t*)&word;
            word = (bptr[0] << 24) | (bptr[1] << 16) | (bptr[2] << 8) | bptr[3];
            pos = 31 - pos;
         }
         else // little endian
         {
            if (curPos->offsetBits > 31)
            {
               word = data[1];
               pos -= 32;
            }
            else if ((curPos->offsetBits + curPos->numBits) <= 32)
            {
               word = data[0];
            }
            else
            {
               word = data[0] >> pos;
               word |= data[1] << (32 - pos);
               pos = 0;
            }
         }

         uint32_t mask = (1L << numBits) - 1;
         word = (word >> pos) & mask;

         #if CAN_SIGNED
            int32_t ival;
            if (numBits > 1)
            {
               uint32_t sign_bit = 1L << (numBits - 1);
               ival = static_cast<int32_t>(((word + sign_bit) & mask)) - sign_bit;
            }
            else
            {
               ival = word;
            }
            float val = ival;
         #else
            float val = word;
         #endif

         val += curPos->offset;
         val *= curPos->gain;

         if (Param::GetType((Param::PARAM_NUM)curPos->mapParam) == Param::TYPE_PARAM || Param::GetType((Param::PARAM_NUM)curPos->mapParam) == Param::TYPE_TESTPARAM)
            Param::Set((Param::PARAM_NUM)curPos->mapParam, FP_FROMFLT(val));
         else
            Param::SetFloat((Param::PARAM_NUM)curPos->mapParam, val);
      }
   }
}

void CanMap::Clear()
{
   ClearMap(canSendMap);
   ClearMap(canRecvMap);
   canHardware->ClearUserMessages();
}

void CanMap::SendAll()
{
   forEachCanMap(curMap, canSendMap)
   {
      uint32_t data[2] = { 0 };

      forEachPosMap(curPos, curMap)
      {
         if (isSaving) return;

         float val = Param::GetFloat((Param::PARAM_NUM)curPos->mapParam);

         val *= curPos->gain;
         val += curPos->offset;
         uint32_t ival = (int32_t)val;
         uint8_t numBits = ABS(curPos->numBits);
         ival &= (1UL << numBits) - 1;

         if (curPos->numBits < 0) // big-endian
         {
            const uint8_t* bptr = (uint8_t*)&ival;
            ival = (bptr[0] << 24) | (bptr[1] << 16) | (bptr[2] << 8) | bptr[3];

            if (curPos->offsetBits < 32)
            {
               data[0] |= ival >> (31 - curPos->offsetBits);
            }
            else if ((curPos->offsetBits + curPos->numBits) >= 31)
            {
               data[1] |= ival >> (63 - curPos->offsetBits);
            }
            else
            {
               data[0] |= ival << (curPos->offsetBits - 31);
               data[1] |= ival >> (63 - curPos->offsetBits);
            }
         }
         else // little-endian
         {
            if (curPos->offsetBits > 31)
            {
               data[1] |= ival << (curPos->offsetBits - 32);
            }
            else if ((curPos->offsetBits + curPos->numBits) <= 32)
            {
               data[0] |= ival << curPos->offsetBits;
            }
            else
            {
               data[0] |= ival << curPos->offsetBits;
               data[1] |= ival >> (32 - curPos->offsetBits);
            }
         }
      }

      canHardware->Send(curMap->canId, data);
   }
}

int CanMap::AddSend(Param::PARAM_NUM param, uint32_t canId, uint8_t offsetBits, int8_t length, float gain, int8_t offset)
{
   if (canId > MAX_COB_ID) return CAN_ERR_INVALID_ID;
   return Add(canSendMap, param, canId, offsetBits, length, gain, offset);
}

int CanMap::AddSend(Param::PARAM_NUM param, uint32_t canId, uint8_t offsetBits, int8_t length, float gain)
{
   return AddSend(param, canId, offsetBits, length, gain, 0);
}

int CanMap::AddRecv(Param::PARAM_NUM param, uint32_t canId, uint8_t offsetBits, int8_t length, float gain, int8_t offset)
{
   bool forceExtended = (canId & CAN_FORCE_EXTENDED) != 0;
   uint32_t moddedId = canId & ~CAN_FORCE_EXTENDED;
   if (moddedId > MAX_COB_ID) return CAN_ERR_INVALID_ID;
   moddedId |= SHIFT_FORCE_FLAG(forceExtended);

   int res = Add(canRecvMap, param, moddedId, offsetBits, length, gain, offset);
   canHardware->RegisterUserMessage(canId);
   return res;
}

int CanMap::AddRecv(Param::PARAM_NUM param, uint32_t canId, uint8_t offsetBits, int8_t length, float gain)
{
   return AddRecv(param, canId, offsetBits, length, gain, 0);
}

int CanMap::Remove(Param::PARAM_NUM param)
{
   bool rx = false;
   bool done = false;
   uint8_t messageIdx = 0, itemIdx = 0;

   for (CANIDMAP *map = canSendMap; !done; map = canRecvMap)
   {
      messageIdx = 0;
      forEachCanMap(curMap, map)
      {
         itemIdx = 0;
         forEachPosMap(curPos, curMap)
         {
            if (curPos->mapParam == param)
               goto itemfound;

            itemIdx++;
         }
         messageIdx++;
      }
      done = rx;
      rx = true;
   }

itemfound:

   if (!done)
      return Remove(rx, messageIdx, itemIdx);

   return 0;
}

int CanMap::Remove(bool rx, uint8_t messageIdx, uint8_t itemidx)
{
   CANPOS *lastPosMap = 0;
   CANIDMAP *map = rx ? &canRecvMap[messageIdx] : &canSendMap[messageIdx];

   if (messageIdx > MAX_MESSAGES || map->first == MAX_ITEMS) return 0;

   forEachPosMap(curPos, map)
   {
      if (itemidx == 0)
      {
         if (lastPosMap != 0)
         {
            lastPosMap->next = curPos->next;
         }
         else if (curPos->next != MAX_ITEMS)
         {
            map->first = curPos->next;
         }
         else
         {
            uint8_t lastIdx = 0;
            for (; (lastIdx + messageIdx) < MAX_MESSAGES && map[lastIdx].first != MAX_ITEMS; lastIdx++);
            lastIdx--;

            map->first = map[lastIdx].first;
            map->canId = map[lastIdx].canId;
            map[lastIdx].first = MAX_ITEMS;
         }
         curPos->next = ITEM_UNSET;
         return 1;
      }
      itemidx--;
      lastPosMap = curPos;
   }
   return 0;
}

bool CanMap::FindMap(Param::PARAM_NUM param, uint32_t& canId, uint8_t& start, int8_t& length, float& gain, int8_t& offset, bool& rx)
{
   rx = false;
   bool done = false;

   for (CANIDMAP *map = canSendMap; !done; map = canRecvMap)
   {
      forEachCanMap(curMap, map)
      {
         forEachPosMap(curPos, curMap)
         {
            if (curPos->mapParam == param)
            {
               bool forceExt = IS_EXT_FORCE(curMap->canId);
               canId = curMap->canId;
               canId = MASK_EXT_FORCE(canId);
               canId |= forceExt * CAN_FORCE_EXTENDED;
               start = curPos->offsetBits;
               length = curPos->numBits;
               gain = curPos->gain;
               offset = curPos->offset;
               return true;
            }
         }
      }
      done = rx;
      rx = true;
   }
   return false;
}

const CanMap::CANPOS* CanMap::GetMap(bool rx, uint8_t ididx, uint8_t itemidx, uint32_t& canId)
{
   CANIDMAP *map = rx ? &canRecvMap[ididx] : &canSendMap[ididx];

   if (ididx > MAX_MESSAGES || map->first == MAX_ITEMS) return 0;

   forEachPosMap(curPos, map)
   {
      if (itemidx == 0)
      {
         bool forceExt = IS_EXT_FORCE(map->canId);
         canId = map->canId;
         canId = MASK_EXT_FORCE(canId);
         canId |= forceExt * CAN_FORCE_EXTENDED;
         return curPos;
      }
      itemidx--;
   }
   return 0;
}

void CanMap::IterateCanMap(void (*callback)(Param::PARAM_NUM, uint32_t, uint8_t, int8_t, float, int8_t, bool))
{
   bool done = false, rx = false;

   for (CANIDMAP *map = canSendMap; !done; map = canRecvMap)
   {
      forEachCanMap(curMap, map)
      {
         forEachPosMap(curPos, curMap)
         {
            bool forceExt = IS_EXT_FORCE(curMap->canId);
            uint32_t canId = curMap->canId;
            canId = MASK_EXT_FORCE(canId);
            canId |= forceExt * CAN_FORCE_EXTENDED;
            callback((Param::PARAM_NUM)curPos->mapParam, canId, curPos->offsetBits, curPos->numBits, curPos->gain, curPos->offset, rx);
         }
      }
      done = rx;
      rx = true;
   }
}

/****************** Private methods ********************/

void CanMap::ClearMap(CANIDMAP *canMap)
{
   for (int i = 0; i < MAX_MESSAGES; i++)
   {
      canMap[i].first = MAX_ITEMS;
   }

   for (int i = 0; i < (MAX_ITEMS + 1); i++)
   {
      canPosMap[i].next = ITEM_UNSET;
   }
}

int CanMap::Add(CANIDMAP *canMap, Param::PARAM_NUM param, uint32_t canId, uint8_t offsetBits, int8_t length, float gain, int8_t offset)
{
   if (length == 0 || ABS(length) > 32) return CAN_ERR_INVALID_LEN;
   if (length > 0)
   {
      if (offsetBits + length - 1 > 63) return CAN_ERR_INVALID_OFS;
   }
   else
   {
      if (offsetBits > 63) return CAN_ERR_INVALID_OFS;
      if (static_cast<int8_t>(offsetBits) + length + 1 < 0) return CAN_ERR_INVALID_OFS;
   }

   CANIDMAP *existingMap = FindById(canMap, canId);

   if (0 == existingMap)
   {
      for (int i = 0; i < MAX_MESSAGES; i++)
      {
         if (canMap[i].first == MAX_ITEMS)
         {
            existingMap = &canMap[i];
            break;
         }
      }

      if (0 == existingMap)
         return CAN_ERR_MAXMESSAGES;

      existingMap->canId = canId;
   }

   int freeIndex;

   for (freeIndex = 0; freeIndex < MAX_ITEMS && canPosMap[freeIndex].next != ITEM_UNSET; freeIndex++);

   if (freeIndex == MAX_ITEMS)
      return CAN_ERR_MAXITEMS;

   CANPOS* precedingItem = 0;

   for (int precedingIndex = existingMap->first; precedingIndex != MAX_ITEMS; precedingIndex = canPosMap[precedingIndex].next)
      precedingItem = &canPosMap[precedingIndex];

   CANPOS* freeItem = &canPosMap[freeIndex];
   freeItem->mapParam = param;
   freeItem->gain = gain;
   freeItem->offset = offset;
   freeItem->offsetBits = offsetBits;
   freeItem->numBits = length;
   freeItem->next = MAX_ITEMS;

   if (precedingItem == 0)
   {
      existingMap->first = freeIndex;
   }
   else
   {
      precedingItem->next = freeIndex;
   }

   int count = 0;

   forEachCanMap(curMap, canMap)
      count++;

   return count;
}

CanMap::CANIDMAP* CanMap::FindById(CANIDMAP *canMap, uint32_t canId)
{
   forEachCanMap(curMap, canMap)
   {
      if ((curMap->canId & ~SHIFT_FORCE_FLAG(1)) == (canId & ~SHIFT_FORCE_FLAG(1)))
         return curMap;
   }
   return 0;
}

void CanMap::ReplaceParamEnumByUid(CANIDMAP *canMap)
{
   forEachCanMap(curMap, canMap)
   {
      forEachPosMap(curPos, curMap)
      {
         const Param::Attributes* attr = Param::GetAttrib((Param::PARAM_NUM)curPos->mapParam);
         curPos->mapParam = (uint16_t)attr->id;
      }
   }
}

void CanMap::ReplaceParamUidByEnum(CANIDMAP *canMap)
{
   forEachCanMap(curMap, canMap)
   {
      forEachPosMap(curPos, curMap)
      {
         Param::PARAM_NUM param = Param::NumFromId(curPos->mapParam);
         curPos->mapParam = param;
      }
   }
}

void CanMap::Save()
{
   struct MapStorage
   {
      CANIDMAP sendMap[MAX_MESSAGES];
      CANIDMAP recvMap[MAX_MESSAGES];
      CANPOS posMap[MAX_ITEMS + 1];
      uint32_t crc;
   };

   isSaving = true;

   ReplaceParamEnumByUid(canSendMap);
   ReplaceParamEnumByUid(canRecvMap);

   MapStorage storage;
   memcpy(&storage.sendMap, canSendMap, sizeof(canSendMap));
   memcpy(&storage.recvMap, canRecvMap, sizeof(canRecvMap));
   memcpy(&storage.posMap, canPosMap, sizeof(canPosMap));

   const uint32_t words = (sizeof(storage) - sizeof(uint32_t)) / sizeof(uint32_t);
   storage.crc = calculate_crc32_block((uint32_t*)&storage, words);
   EEPROM.put(kCanMapEepromBase, storage);

   ReplaceParamUidByEnum(canSendMap);
   ReplaceParamUidByEnum(canRecvMap);

   isSaving = false;
}

int CanMap::LoadFromFlash()
{
   struct MapStorage
   {
      CANIDMAP sendMap[MAX_MESSAGES];
      CANIDMAP recvMap[MAX_MESSAGES];
      CANPOS posMap[MAX_ITEMS + 1];
      uint32_t crc;
   };

   MapStorage storage;
   EEPROM.get(kCanMapEepromBase, storage);

   const uint32_t words = (sizeof(storage) - sizeof(uint32_t)) / sizeof(uint32_t);
   const uint32_t crc = calculate_crc32_block((uint32_t*)&storage, words);

   if (storage.crc == crc)
   {
      memcpy(canSendMap, storage.sendMap, sizeof(canSendMap));
      memcpy(canRecvMap, storage.recvMap, sizeof(canRecvMap));
      memcpy(canPosMap, storage.posMap, sizeof(canPosMap));
      ReplaceParamUidByEnum(canSendMap);
      ReplaceParamUidByEnum(canRecvMap);
      return 1;
   }

   return LegacyLoadFromFlash();
}

int CanMap::LegacyLoadFromFlash()
{
   // Simplified - just return failure for now
   return 0;
}
