#pragma once
#include <DirectXPackedVector.h>
#include <vector>
#include <algorithm>
// Opt-in, single-frame GPU comparison. No readback allocation or GPU wait in normal use.
namespace nrcomparison {
rtx::ComPtr<ID3D12Resource> before,after,gameInput;
rtx::ComPtr<ID3D12Fence> fence;
D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
UINT64 bytes{},inputBytes{};unsigned serial{},seen{},draws{},captureFrame{};bool recorded{},signalled{},hotkeyUsed{},wasDown{};
D3D12_PLACED_SUBRESOURCE_FOOTPRINT inputFootprint{};
void record(ID3D12GraphicsCommandList* list,ID3D12Resource* source,ID3D12Resource* result,ID3D12Resource* original,D3D12_RESOURCE_STATES originalState){
 static const bool legacy=GetPrivateProfileIntW(L"Diagnostics",L"NRComparison",0,(moduleDirectory()+L"\\GTAXeSS.ini").c_str())!=0;
 static const bool hotkey=GetPrivateProfileIntW(L"Diagnostics",L"BlurCaptureOnF11",0,(moduleDirectory()+L"\\GTAXeSS.ini").c_str())!=0;
 if(!legacy&&!hotkey)return;
 const SHORT key=hotkey?GetAsyncKeyState(VK_F11):0;const bool down=(key&0x8000)!=0;const bool trigger=hotkey&&!hotkeyUsed&&((down&&!wasDown)||(key&1));wasDown=down;
 if(recorded||(!trigger&&(!legacy||++draws%120!=0)))return;
 const auto ini=moduleDirectory()+L"\\GTAXeSS.ini";
 const unsigned requested=GetPrivateProfileIntW(L"Diagnostics",L"NRComparison",0,ini.c_str());
 if(!trigger&&(!requested||requested==seen))return;
 auto desc=source->GetDesc();if(desc.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT||result->GetDesc().Format!=desc.Format)return;
 nativeDevice->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&bytes);
 D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;
 D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=bytes;buffer.Height=1;buffer.DepthOrArraySize=1;buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 try{
 rtx::check(nativeDevice->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&before)),"NR comparison before");
 rtx::check(nativeDevice->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&after)),"NR comparison after");
 rtx::check(nativeDevice->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"NR comparison fence");
 auto originalDesc=original->GetDesc();nativeDevice->GetCopyableFootprints(&originalDesc,0,1,0,&inputFootprint,nullptr,nullptr,&inputBytes);buffer.Width=inputBytes;
 rtx::check(nativeDevice->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&gameInput)),"blur capture game input");
 }catch(const std::exception&e){before.Reset();after.Reset();gameInput.Reset();fence.Reset();hotkeyUsed=true;log(std::string("R636: BLUR_CAPTURE_DISABLED reason=")+e.what());return;}
 ID3D12Resource* sources[]{source,result};ID3D12Resource* destinations[]{before.Get(),after.Get()};
 for(unsigned i=0;i<2;++i){transition(list,sources[i],i?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE);D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=sources[i];src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=destinations[i];dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprint;list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);transition(list,sources[i],D3D12_RESOURCE_STATE_COPY_SOURCE,i?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);}
 transition(list,original,originalState,D3D12_RESOURCE_STATE_COPY_SOURCE);D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=original;src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.pResource=gameInput.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=inputFootprint;list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);transition(list,original,D3D12_RESOURCE_STATE_COPY_SOURCE,originalState);
 seen=requested;serial=trigger?unsigned(GetTickCount64()):requested;captureFrame=frameId;recorded=true;signalled=false;if(trigger)hotkeyUsed=true;log("R636: BLUR_CAPTURE_QUEUED frame="+std::to_string(captureFrame)+", same_frame=1");
}
void bmp(const std::wstring& path,const unsigned char* data,const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint){
 const unsigned w=footprint.Footprint.Width,h=footprint.Footprint.Height,stride=(w*3+3)&~3u;
 BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+stride*h;
 BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=w;info.biHeight=-(LONG)h;info.biPlanes=1;info.biBitCount=24;info.biSizeImage=stride*h;
 std::ofstream out(path,std::ios::binary);out.write((char*)&file,sizeof(file));out.write((char*)&info,sizeof(info));std::vector<unsigned char> row(stride);
 for(unsigned y=0;y<h;++y){auto raw=data+y*footprint.Footprint.RowPitch;auto p=reinterpret_cast<const uint16_t*>(raw);for(unsigned x=0;x<w;++x)for(unsigned c=0;c<3;++c){float f=footprint.Footprint.Format==DXGI_FORMAT_R16G16B16A16_FLOAT?DirectX::PackedVector::XMConvertHalfToFloat(p[x*4+c]):raw[x*4+(footprint.Footprint.Format==DXGI_FORMAT_B8G8R8A8_UNORM?2-c:c)]/255.f;row[x*3+2-c]=(unsigned char)(std::clamp(std::isfinite(f)?f:0.f,0.f,1.f)*255.f+.5f);}out.write((char*)row.data(),row.size());}
}
void poll(){
 if(!recorded)return;
 if(!signalled){rtx::check(proxyQueue->Signal(fence.Get(),1),"NR comparison signal");signalled=true;return;}
 if(fence->GetCompletedValue()==UINT64_MAX){before.Reset();after.Reset();gameInput.Reset();fence.Reset();recorded=signalled=false;log("R636: BLUR_CAPTURE_CANCELLED device_removed=1");return;}if(fence->GetCompletedValue()<1)return;
 unsigned char *a{},*b{};D3D12_RANGE range{0,(SIZE_T)bytes};rtx::check(before->Map(0,&range,(void**)&a),"NR before map");rtx::check(after->Map(0,&range,(void**)&b),"NR after map");
 const auto prefix=moduleDirectory()+L"\\GTAXeSS-NR-"+std::to_wstring(serial);if(seen){bmp(prefix+L"-before.bmp",a,footprint);bmp(prefix+L"-after.bmp",b,footprint);}
 const auto packet=moduleDirectory()+L"\\GTAXeSS-BlurCapture-"+std::to_wstring(serial);std::filesystem::create_directories(packet);bmp(packet+L"\\02-after-upscaling.bmp",a,footprint);bmp(packet+L"\\03-after-neural-rendering-before-FG.bmp",b,footprint);
 unsigned char* input{};D3D12_RANGE inputRange{0,(SIZE_T)inputBytes};rtx::check(gameInput->Map(0,&inputRange,(void**)&input),"blur game input map");bmp(packet+L"\\01-game-input.bmp",input,inputFootprint);D3D12_RANGE inputWritten{0,0};gameInput->Unmap(0,&inputWritten);
 std::ofstream(packet+L"\\frame.txt")<<"build=R639\nframe_id="<<captureFrame<<"\nsame_processed_frame=1\nAll BMPs are before frame generation. Generated frames are NOT captured.\nImages clamp floating-point colour to SDR for viewing.\ninput="<<inputFootprint.Footprint.Width<<"x"<<inputFootprint.Footprint.Height<<" format="<<unsigned(inputFootprint.Footprint.Format)<<"\noutput="<<footprint.Footprint.Width<<"x"<<footprint.Footprint.Height<<"\n";
 double sum=0;float max=0;uint64_t changed=0,invalid=0;const uint64_t pixels=uint64_t(footprint.Footprint.Width)*footprint.Footprint.Height;
 for(unsigned y=0;y<footprint.Footprint.Height;++y){auto x=reinterpret_cast<uint16_t*>(a+y*footprint.Footprint.RowPitch),z=reinterpret_cast<uint16_t*>(b+y*footprint.Footprint.RowPitch);for(unsigned i=0;i<footprint.Footprint.Width;++i){float delta=0;for(unsigned c=0;c<3;++c){float av=DirectX::PackedVector::XMConvertHalfToFloat(x[i*4+c]),bv=DirectX::PackedVector::XMConvertHalfToFloat(z[i*4+c]);if(!std::isfinite(av)||!std::isfinite(bv)){++invalid;continue;}float d=std::fabs(av-bv);sum+=d;max=(std::max)(max,d);delta=(std::max)(delta,d);}if(delta>1.f/255)++changed;}}
 std::ofstream(prefix+L".txt")<<"same_frame=1 width="<<footprint.Footprint.Width<<" height="<<footprint.Footprint.Height<<" mean_abs_rgb="<<sum/(pixels*3)<<" max_abs_rgb="<<max<<" changed_over_1_255_percent="<<100.*changed/pixels<<" invalid_channels="<<invalid<<"\n";
 D3D12_RANGE written{0,0};before->Unmap(0,&written);after->Unmap(0,&written);before.Reset();after.Reset();gameInput.Reset();fence.Reset();recorded=signalled=false;log("R636: BLUR_CAPTURE_SAVED frame="+std::to_string(captureFrame)+", folder="+std::filesystem::path(packet).string());
}
}
