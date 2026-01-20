/*  This library supports ISA Scale IVT Modular current/voltage sensor device.  These devices measure current, up to three voltages, and provide temperature compensation.

    This library was written by Jack Rickard of EVtv - http://www.evtv.me
    copyright 2014
    You are licensed to use this library for any purpose, commercial or private,
    without restriction.

*/


#include "isa_shunt.h"
#include "my_fp.h"
#include "my_math.h"
#include "params.h"

uint16_t  framecount=0;
bool firstframe=true;




#define FLASH_DELAY 8000000
static void delay(void) //delay used for isa setup fumction. probably much better ways but its used only once.......
{
   int i;
   for (i = 0; i < FLASH_DELAY; i++)       /* Wait a bit. */
      __asm__("nop");
}

void ISA::DecodeCAN(int id, uint32_t data[2])
{
   switch (id)
   {
   case 0x521:
      ISA::handle521(data);//ISA CAN MESSAGE
      break;
   case 0x522:
      ISA::handle522(data);//ISA CAN MESSAGE
      break;
   case 0x523:
      ISA::handle523(data);//ISA CAN MESSAGE
      break;
   case 0x524:
      ISA::handle524(data);//ISA CAN MESSAGE
      break;
   case 0x525:
      ISA::handle525(data);//ISA CAN MESSAGE
      break;
   case 0x526:
      ISA::handle526(data);//ISA CAN MESSAGE
      break;
   case 0x527:
      ISA::handle527(data);//ISA CAN MESSAGE
      break;
   case 0x528:
      ISA::handle528(data);//ISA CAN MESSAGE
      break;
   }
}

void ISA::RegisterCanMessages(CanHardware* can)
{
   can->RegisterUserMessage(0x521);//ISA MSG
   can->RegisterUserMessage(0x522);//ISA MSG
   can->RegisterUserMessage(0x523);//ISA MSG
   can->RegisterUserMessage(0x524);//ISA MSG
   can->RegisterUserMessage(0x525);//ISA MSG
   can->RegisterUserMessage(0x526);//ISA MSG
   can->RegisterUserMessage(0x527);//ISA MSG
   can->RegisterUserMessage(0x528);//ISA MSG
}

void ISA::initialize(CanHardware* can)
{
   uint8_t bytes[8];

   firstframe=false;
   STOP(can);
   delay();
   for(int i=0; i<9; i++)
   {
      bytes[0]=(0x20+i);
      bytes[1]=0x42;
      bytes[2]=0x00;
      bytes[3]=0x64;
      bytes[4]=0x00;
      bytes[5]=0x00;
      bytes[6]=0x00;
      bytes[7]=0x00;

      can->Send(0x411, (uint32_t*)bytes, 8);
      delay();

      sendSTORE(can);
      delay();
   }
   START(can);
   delay();

}

void ISA::STOP(CanHardware* can)
{
   uint8_t bytes[8];
//SEND STOP///////


   bytes[0]=0x34;
   bytes[1]=0x00;
   bytes[2]=0x01;
   bytes[3]=0x00;
   bytes[4]=0x00;
   bytes[5]=0x00;
   bytes[6]=0x00;
   bytes[7]=0x00;

   can->Send(0x411, (uint32_t*)bytes,8);

}
void ISA::sendSTORE(CanHardware* can)
{
   uint8_t bytes[8];
//SEND STORE///////

   bytes[0]=0x32;
   bytes[1]=0x00;
   bytes[2]=0x00;
   bytes[3]=0x00;
   bytes[4]=0x00;
   bytes[5]=0x00;
   bytes[6]=0x00;
   bytes[7]=0x00;

   can->Send(0x411, (uint32_t*)bytes,8);
}

void ISA::START(CanHardware* can)
{
   uint8_t bytes[8];
//SEND START///////

   bytes[0]=0x34;
   bytes[1]=0x01;
   bytes[2]=0x01;
   bytes[3]=0x00;
   bytes[4]=0x00;
   bytes[5]=0x00;
   bytes[6]=0x00;
   bytes[7]=0x00;
   can->Send(0x411, (uint32_t*)bytes,8);


}

void ISA::RESTART(CanHardware* can)
{
   //Has the effect of zeroing AH and KWH
   uint8_t bytes[8];

   bytes[0]=0x3F;
   bytes[1]=0x00;
   bytes[2]=0x00;
   bytes[3]=0x00;
   bytes[4]=0x00;
   bytes[5]=0x00;
   bytes[6]=0x00;
   bytes[7]=0x00;
   //Can::GetInterface(Param::GetInt(Param::shunt_can))->;
   can->Send(0x411, (uint32_t*)bytes,8);

}


void ISA::deFAULT(CanHardware* can)
{
   //Returns module to original defaults
   uint8_t bytes[8];

   bytes[0]=0x3D;
   bytes[1]=0x00;
   bytes[2]=0x00;
   bytes[3]=0x00;
   bytes[4]=0x00;
   bytes[5]=0x00;
   bytes[6]=0x00;
   bytes[7]=0x00;
   can->Send(0x411, (uint32_t*)bytes,8);


}


void ISA::initCurrent(CanHardware* can)
{
   uint8_t bytes[8];
   STOP(can);
   delay();
   bytes[0]=0x21;
   bytes[1]=0x42;
   bytes[2]=0x01;
   bytes[3]=0x61;
   bytes[4]=0x00;
   bytes[5]=0x00;
   bytes[6]=0x00;
   bytes[7]=0x00;

   can->Send(0x411, (uint32_t*)bytes,8);

   delay();
   sendSTORE(can);
   START(can);
   delay();

}

/********* Private functions *******/

void ISA::handle521(uint32_t data[2])  //Amperes

{
   uint8_t* bytes = (uint8_t*)data;// arrgghhh this converts the two 32bit array into bytes. See comments are useful:)
   int32_t amperes = ((bytes[5] << 24) | (bytes[4] << 16) | (bytes[3] << 8) | (bytes[2]));
   Param::SetFloat(Param::isaCurrent, static_cast<float>(amperes));
}

void ISA::handle522(uint32_t data[2])  //Voltage

{
   uint8_t* bytes = (uint8_t*)data;// arrgghhh this converts the two 32bit array into bytes. See comments are useful:)
   int32_t voltage = ((bytes[5] << 24) | (bytes[4] << 16) | (bytes[3] << 8) | (bytes[2]));
   Param::SetFloat(Param::isaVoltage1, static_cast<float>(voltage));
}

void ISA::handle523(uint32_t data[2]) //Voltage2

{
   uint8_t* bytes = (uint8_t*)data;// arrgghhh this converts the two 32bit array into bytes. See comments are useful:)
   int32_t voltage2 = (uint32_t)((bytes[5] << 24) | (bytes[4] << 16) | (bytes[3] << 8) | (bytes[2]));
   Param::SetFloat(Param::isaVoltage2, static_cast<float>(voltage2));


}

void ISA::handle524(uint32_t data[2])  //Voltage3

{
   uint8_t* bytes = (uint8_t*)data;// arrgghhh this converts the two 32bit array into bytes. See comments are useful:)
   int32_t voltage3 = (uint32_t)((bytes[5] << 24) | (bytes[4] << 16) | (bytes[3] << 8) | (bytes[2]));
   Param::SetFloat(Param::isaVoltage3, static_cast<float>(voltage3));

}

void ISA::handle525(uint32_t data[2])  //Temperature
{
   uint8_t* bytes = (uint8_t*)data;// arrgghhh this converts the two 32bit array into bytes. See comments are useful:)
   framecount++;
   int32_t temp=0;
   temp = (int32_t)((bytes[5] << 24) | (bytes[4] << 16) | (bytes[3] << 8) | (bytes[2]));

   temp = temp / 10;
   Param::SetFloat(Param::isaTemperature, static_cast<float>(temp));

}

void ISA::handle526(uint32_t data[2]) //Kilowatts
{
   uint8_t* bytes = (uint8_t*)data;// arrgghhh this converts the two 32bit array into bytes. See comments are useful:)
   int32_t kw = (int32_t)((bytes[5] << 24) | (bytes[4] << 16) | (bytes[3] << 8) | (bytes[2]));
   Param::SetFloat(Param::isaKW, static_cast<float>(kw));
}


void ISA::handle527(uint32_t data[2]) //Ampere-Hours

{
   uint8_t* bytes = (uint8_t*)data;// arrgghhh this converts the two 32bit array into bytes. See comments are useful:)
   int32_t ah = (bytes[5] << 24) | (bytes[4] << 16) | (bytes[3] << 8) | (bytes[2]);
   Param::SetFloat(Param::isaAh, static_cast<float>(ah));
}

void ISA::handle528(uint32_t data[2])  //kiloWatt-hours

{
   uint8_t* bytes = (uint8_t*)data;// arrgghhh this converts the two 32bit array into bytes. See comments are useful:)
   int32_t kwh = ((bytes[5] << 24) | (bytes[4] << 16) | (bytes[3] << 8) | (bytes[2]));
   Param::SetFloat(Param::isaKWh, static_cast<float>(kwh));

}
