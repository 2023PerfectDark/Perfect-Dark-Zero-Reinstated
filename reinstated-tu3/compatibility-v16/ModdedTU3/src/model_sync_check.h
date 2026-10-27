#pragma once
#include <cstdint>

namespace pdz_model_sync {
template<class Read32, class Read16, class Read8, class Access>
bool MaterialReady(uint32_t definition,uint32_t source,uint32_t destination,
                   Read32 u32,Read16 u16,Read8 u8,Access access) {
  if(!access(definition,108,false)||!access(source,20,false)||
     !access(destination,20,false))return false;
  auto buffer=[&](uint32_t offset,uint32_t bytes){
    return !bytes||(access(u32(source+offset),bytes,false)&&
                    access(u32(destination+offset),bytes,true));
  };
  const uint32_t matrices=u8(definition+21),vectors=u16(definition+22);
  if(!buffer(0,matrices*64)||!buffer(4,matrices)||
     !buffer(8,vectors*20)||!buffer(12,u8(definition+20)))return false;
  // Native code copies the optional effect only when the source has one.
  if(u32(source+16)&&(!buffer(16,72)||!access(u32(definition+104),65,false)))return false;
  return true;
}
// 82714220 dereferences two levels of node tables BEFORE MaterialReady runs.
// Validate exactly the entries the native loop will visit (40-byte definition
// records, 48-byte instance records). Null optional materials are legitimate.
template<class Read, class Access>
bool NodeTablesReady(uint32_t node,uint32_t source,uint32_t destination,
                     Read read,Access access) {
  if(!access(node,52,false))return false;
  const auto arrays=read(node+48),offset=read(node+12);
  const uint64_t src=uint64_t(source)+offset,dst=uint64_t(destination)+offset;
  if(src>UINT32_MAX-8||dst>UINT32_MAX-8||
     !access(uint32_t(src),8,false)||!access(uint32_t(dst),8,true)||
     !access(arrays,12,false))return false;
  const int32_t count=int32_t(read(arrays));
  if(count<=0)return true;
  if(uint32_t(count)>UINT32_MAX/48)return false;
  for(unsigned side=0;side<2;++side) {
    const auto definitions=read(arrays+4+side*4);
    if(!definitions)continue;
    if(!access(definitions,uint32_t(count)*40,false))return false;
    for(unsigned i=0;i<unsigned(count);++i) {
      if(!read(definitions+i*40))continue;
      const auto sourceTable=read(uint32_t(src)+4);
      if(!access(sourceTable,8,false))return false;
      const auto sourceEntries=read(sourceTable+side*4);
      if(!access(sourceEntries,uint32_t(count)*48,false))return false;
      if(!read(sourceEntries+i*48))continue;
      const auto destinationTable=read(uint32_t(dst)+4);
      if(!access(destinationTable,8,false)||
         !access(read(destinationTable+side*4),uint32_t(count)*48,false))return false;
    }
  }
  return true;
}
// Validate the first native copy before it can partially update model state.
template<class Read, class Access>
bool Ready(uint32_t definition, uint32_t source, uint32_t destination,
           Read read, Access access) {
  if (!access(definition,36,false)||!access(source,8,false)||
      !access(destination,8,false)) return false;
  const uint64_t bytes=uint64_t(read(definition+32))*4;
  if (!bytes) return true;
  if (bytes>UINT32_MAX) return false;
  return access(read(source+4),uint32_t(bytes),false)&&
         access(read(destination+4),uint32_t(bytes),true);
}
}
