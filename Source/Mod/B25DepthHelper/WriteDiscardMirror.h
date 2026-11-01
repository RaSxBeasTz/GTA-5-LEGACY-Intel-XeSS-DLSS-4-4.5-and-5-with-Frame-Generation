#pragma once
#include <cstring>
// Only the locked 256-byte dynamic constant buffer uses this mirror. The game
// writes normal CPU memory; Unmap uploads it without reading write-combined RAM.
struct WriteDiscardMirror {
 alignas(64) unsigned char bytes[256]{};
 void* destination{};const void* resource{};const void* context{};
 void* begin(void* mapped,const void* r,const void* c){
  if(destination||!mapped||!r||!c)return nullptr;
  destination=mapped;resource=r;context=c;std::memset(bytes,0,sizeof(bytes));return bytes;
 }
 bool commit(const void* r,const void* c,unsigned sub){
  if(!destination||r!=resource||c!=context||sub!=0)return false;
  std::memcpy(destination,bytes,sizeof(bytes));destination=nullptr;resource=context=nullptr;return true;
 }
};
