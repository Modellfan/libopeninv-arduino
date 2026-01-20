/*
 * Arduino port of libopeninv param_save
 * Uses EEPROM for parameter persistence.
 */

#include <Arduino.h>
#include <EEPROM.h>
#include "params.h"
#include "param_save.h"
#include "my_string.h"

namespace
{
constexpr size_t kParamBlockSize = 2048;
constexpr int kEepromBase = 0;

typedef struct __attribute__((packed))
{
   uint16_t key;
   uint8_t dummy;
   uint8_t flags;
   uint32_t value;
} PARAM_ENTRY;

constexpr size_t kNumParams = (kParamBlockSize - 8) / sizeof(PARAM_ENTRY);
constexpr size_t kParamWords = kParamBlockSize / 4;

typedef struct __attribute__((packed))
{
   PARAM_ENTRY data[kNumParams];
   uint32_t crc;
   uint32_t padding;
} PARAM_PAGE;
}

// Simple CRC32 calculation
static uint32_t calculate_crc32(uint32_t *data, uint32_t length)
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

/**
* Save parameters to flash
*
* @return CRC of parameter flash page
*/
uint32_t parm_save()
{
   PARAM_PAGE parmPage;
   uint32_t idx;

   memset32((int*)&parmPage, 0xFFFFFFFF, kParamWords);

   // Copy parameter values and keys to block structure
   for (idx = 0; idx < kNumParams && idx < Param::PARAM_LAST; idx++)
   {
      if (Param::GetType((Param::PARAM_NUM)idx) == Param::TYPE_PARAM)
      {
         const Param::Attributes *pAtr = Param::GetAttrib((Param::PARAM_NUM)idx);
         parmPage.data[idx].flags = (uint8_t)Param::GetFlag((Param::PARAM_NUM)idx);
         parmPage.data[idx].key = pAtr->id;
         parmPage.data[idx].value = Param::Get((Param::PARAM_NUM)idx);
      }
   }

   parmPage.crc = calculate_crc32((uint32_t*)&parmPage, 2 * kNumParams);
   EEPROM.put(kEepromBase, parmPage);

   return parmPage.crc;
}

/**
* Load parameters from flash
*
* @retval 0 Parameters loaded successfully
* @retval -1 CRC error, parameters not loaded
*/
int parm_load()
{
   PARAM_PAGE parmPage;
   EEPROM.get(kEepromBase, parmPage);

   uint32_t crc = calculate_crc32((uint32_t*)&parmPage, 2 * kNumParams);

   if (crc == parmPage.crc)
   {
      int loaded = 0;
      for (unsigned int idxPage = 0; idxPage < kNumParams; idxPage++)
      {
         Param::PARAM_NUM idx = Param::NumFromId(parmPage.data[idxPage].key);
         if (idx != Param::PARAM_INVALID && Param::GetType((Param::PARAM_NUM)idx) == Param::TYPE_PARAM)
         {
            Param::SetFixed(idx, parmPage.data[idxPage].value);
            Param::SetFlagsRaw(idx, parmPage.data[idxPage].flags);
            loaded++;
         }
      }
      return 0;
   }

   // Check if EEPROM is erased (all 0xFF)
   if (parmPage.crc == 0xFFFFFFFF)
      return -1;

   return -1;
}
