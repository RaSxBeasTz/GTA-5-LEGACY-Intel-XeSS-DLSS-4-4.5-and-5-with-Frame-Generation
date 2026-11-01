from pathlib import Path
p=Path('B25DepthHelper/B140aDirectPresentSharedOpen.cpp');s=p.read_text();old=''' D3D11_TEXTURE2D_DESC captureColorTransport{};g_b173iColorSlots[slotIndex].shared->GetDesc(&captureColorTransport);
 const bool needsCaptureMotion=NeedsCaptureMotion(g_b173iColorSlots[slotIndex].sourceDesc,captureColorTransport);''';new=''' bool needsCaptureMotion=true;
 if(g_b173iColorPoolReady.load(std::memory_order_acquire)&&g_b173iColorSlots[slotIndex].shared){
  D3D11_TEXTURE2D_DESC captureColorTransport{};g_b173iColorSlots[slotIndex].shared->GetDesc(&captureColorTransport);
  needsCaptureMotion=NeedsCaptureMotion(g_b173iColorSlots[slotIndex].sourceDesc,captureColorTransport);
 }''';assert old in s;s=s.replace(old,new);p.write_text(s)
p=Path('GTAXeSS.ini');s=p.read_text();import re;s=re.sub(r'(?m)^SourceFPS=\d+', 'SourceFPS=0',s);s=s.replace('; Source cap while enabled and focused. 30 targets 120 FPS at x4; 0 disables.','; Uncapped source rendering for B557 performance test; 0 disables the source limiter.');p.write_text(s)
