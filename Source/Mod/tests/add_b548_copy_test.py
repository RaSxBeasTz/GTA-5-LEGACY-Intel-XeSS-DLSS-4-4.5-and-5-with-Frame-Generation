from pathlib import Path
p=Path('tests/MotionGpuRegression.cpp');s=p.read_text();needle=' float p[16]{},prevView[16]'
insert=''' // B548 native SDR transport: preserve two distinct full frames exactly,
 // including alpha and alternating edges, across the shared texture path.
 {
  D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=8;d.MipLevels=d.ArraySize=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;d.MiscFlags=D3D11_RESOURCE_MISC_SHARED;
  ComPtr<ID3D11Texture2D> source,shared,readback;hr(dev->CreateTexture2D(&d,nullptr,&shared),"shared SDR transport");d.MiscFlags=0;hr(dev->CreateTexture2D(&d,nullptr,&source),"native SDR source");d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;hr(dev->CreateTexture2D(&d,nullptr,&readback),"SDR readback");
  for(unsigned frame=0;frame<2;++frame){unsigned pixels[64];for(unsigned i=0;i<64;++i)pixels[i]=(frame?0x1f0055aa:0xe0ffaa55)^(i*0x01010101u);ctx->UpdateSubresource(source.Get(),0,nullptr,pixels,32,0);ctx->CopyResource(shared.Get(),source.Get());ctx->CopyResource(readback.Get(),shared.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(ctx->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped),"map SDR copy");bool exact=true;for(unsigned y=0;y<8;++y)exact&=std::memcmp((char*)mapped.pData+y*mapped.RowPitch,pixels+y*8,32)==0;ctx->Unmap(readback.Get(),0);check(exact,"native SDR colour must be bit exact without temporal trails");}
  std::cout<<"PASS: B548 shared SDR transport preserves both frames bit exactly\\n";
 }
'''
assert needle in s;s=s.replace(needle,insert+needle);p.write_text(s)
