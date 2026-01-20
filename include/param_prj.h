#ifndef PARAM_PRJ_H
#define PARAM_PRJ_H

/*
 * This file is part of the libopeninv-arduino ISA example.
 *
 * Copyright (C) 2011-2019 Johannes Huebner <dev@johanneshuebner.com>
 * Copyright (C) 2019-2022 Damien Maguire <info@evbmw.com>
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

#define VER 0.1

/* Entries must be ordered as follows:
   1. Saveable parameters (id != 0)
   2. Temporary parameters (id = 0)
   3. Display values
 */
// Next param id (increase when adding new parameter!): 2
/*              category    name            unit  min   max   default id */
#define PARAM_LIST \
    PARAM_ENTRY(CAT_SETUP,   canNodeId,      "",   1,    127,  22,     1) \
    PARAM_ENTRY(CAT_SHUNT,   isaInit,        ONOFF,0,    1,    0,      2) \
    VALUE_ENTRY(isaCurrent,  "A",                  1100) \
    VALUE_ENTRY(isaVoltage1, "V",                  1101) \
    VALUE_ENTRY(isaVoltage2, "V",                  1102) \
    VALUE_ENTRY(isaVoltage3, "V",                  1103) \
    VALUE_ENTRY(isaTemperature, "C",               1104) \
    VALUE_ENTRY(isaAh,       "Ah",                 1105) \
    VALUE_ENTRY(isaKW,       "kW",                 1106) \
    VALUE_ENTRY(isaKWh,      "kWh",                1107) \
    VALUE_ENTRY(BMS_Vmin,    "V",                  2084) \
    VALUE_ENTRY(BMS_Vmax,    "V",                  2085) \
    VALUE_ENTRY(BMS_Tmin,    "C",                  2086) \
    VALUE_ENTRY(BMS_Tmax,    "C",                  2087)

#define VERSTR STRINGIFY(4=VER)

#define ONOFF     "0=Off, 1=On, 2=na"
#define CAT_SETUP "General Setup"
#define CAT_SHUNT "ISA Shunt Control"

#endif // PARAM_PRJ_H
