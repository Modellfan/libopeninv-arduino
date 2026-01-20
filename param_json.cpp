/*
 * This file is part of the libopeninv project.
 *
 * Copyright (C) 2024
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "param_json.h"

#ifdef ARDUINO
#include <ArduinoJson.h>
#include <string.h>

namespace
{
   String parameterJson;
   size_t streamOffset = 0;

   size_t EstimateJsonDocSize()
   {
      const size_t perParamOverhead = 160;
      const size_t baseOverhead = 512;
      return baseOverhead + (Param::PARAM_LAST * perParamOverhead);
   }
}

namespace ParamJson
{
   void Build()
   {
      DynamicJsonDocument doc(EstimateJsonDocSize());

      for (int i = 0; i < Param::PARAM_LAST; i++)
      {
         const Param::Attributes* attr = Param::GetAttrib((Param::PARAM_NUM)i);
         if (!attr) continue;

         JsonObject param = doc[attr->name].to<JsonObject>();
         param["unit"] = attr->unit;
         param["category"] = attr->category;
         param["minimum"] = attr->min;
         param["maximum"] = attr->max;
         param["default"] = attr->def;
         param["id"] = attr->id;
         param["isparam"] = (Param::GetType((Param::PARAM_NUM)i) == Param::TYPE_PARAM) ? 1 : 0;

         if (attr->name != nullptr && strcmp(attr->name, "version") == 0)
         {
            param["value"] = Param::GetFloat((Param::PARAM_NUM)i);
         }
         else if (Param::GetType((Param::PARAM_NUM)i) == Param::TYPE_SPOTVALUE)
         {
            param["value"] = Param::GetFloat((Param::PARAM_NUM)i);
         }
      }

      parameterJson = "";
      serializeJson(doc, parameterJson);
      streamOffset = 0;
   }

   const String& Get()
   {
      return parameterJson;
   }

   uint32_t GetSize()
   {
      return parameterJson.length();
   }

   int GetByte(uint32_t offset)
   {
      if (offset >= parameterJson.length())
      {
         return -1;
      }
      return (int)(uint8_t)parameterJson[offset];
   }

   void BeginStream()
   {
      Build();
   }

   size_t Read(uint8_t* out, size_t maxLen)
   {
      if (out == nullptr || maxLen == 0)
      {
         return 0;
      }

      const size_t totalLen = parameterJson.length();
      if (streamOffset >= totalLen)
      {
         return 0;
      }

      size_t remaining = totalLen - streamOffset;
      size_t toCopy = remaining < maxLen ? remaining : maxLen;

      for (size_t i = 0; i < toCopy; i++)
      {
         out[i] = (uint8_t)parameterJson[streamOffset + i];
      }

      streamOffset += toCopy;
      return toCopy;
   }
}
#endif
