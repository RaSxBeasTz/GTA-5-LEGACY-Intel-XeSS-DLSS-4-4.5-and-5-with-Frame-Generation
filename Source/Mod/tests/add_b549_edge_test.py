from pathlib import Path
p=Path('tests/MotionGpuRegression.cpp');s=p.read_text().replace('float expectedX=.05f){','float expectedX=.05f,const float* expectedPixels=nullptr){').replace('std::fabs(mx-expectedX)<.003f','std::fabs(mx-(expectedPixels?expectedPixels[y*8+x]:expectedX))<.003f')
needle=' std::cout<<"PASS: GTA reversed depth,'
insert=''' // Depth discontinuity: foreground occupies one pixel. XeFG requires its
 // velocity in the surrounding 3x3, including clamped boundaries, while the
 // original depth resource must remain undilated.
 for(unsigned foreground:{0u,27u,63u}){
  const float nearZ=p[10]+p[14]/5.f;const unsigned near24=(unsigned)std::round(nearZ*16777215.f);
  for(unsigned i=0;i<64;++i)packed[i]=reversed24|0xff000000u;packed[foreground]=near24|0xff000000u;
  float expected[64];for(unsigned i=0;i<64;++i){int dx=int(i%8)-int(foreground%8),dy=int(i/8)-int(foreground/8);expected[i]=(std::abs(dx)<=1&&std::abs(dy)<=1)?.1f:.05f;}
  check(run(GhostingMotionShader1(),true,.05f,expected)==64,"XeFG foreground 3x3 dilation including image boundaries");
  check(run(GhostingMotionShader2(),true,.05f,expected)<64,"undilated shader negative control at depth edge");
  run(GhostingMotionShader1(),true,.05f,expected);
  D3D11_TEXTURE2D_DESC dd{};depthOut->GetDesc(&dd);dd.Usage=D3D11_USAGE_STAGING;dd.BindFlags=0;dd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> originalDepth;hr(dev->CreateTexture2D(&dd,nullptr,&originalDepth),"depth check staging");ctx->CopyResource(originalDepth.Get(),depthOut.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(ctx->Map(originalDepth.Get(),0,D3D11_MAP_READ,0,&mapped),"depth check read");bool exact=true;for(unsigned y=0;y<8;++y){auto row=(float*)((char*)mapped.pData+y*mapped.RowPitch);for(unsigned x=0;x<8;++x)exact&=std::fabs(row[x]-float(packed[y*8+x]&0xffffff)/16777215.f)<1e-7f;}ctx->Unmap(originalDepth.Get(),0);check(exact,"motion dilation must preserve original depth");
 }
 // Dilation must never invent motion with a stationary camera at depth edges.
 std::memcpy(constants.previousViewProjection,curVp,64);check(GhostingBuildMotionTransform(constants,curVp),"stationary edge transform");ctx->UpdateSubresource(buffer.Get(),0,nullptr,&constants,0,0);check(run(GhostingMotionShader1(),true,0)==64,"stationary depth edges remain zero motion");
 std::cout<<"PASS: B549 nearest-foreground motion, border clamping, undilated depth, stationary edge, old-path negative control\\n";
'''
assert needle in s;s=s.replace(needle,insert+needle);p.write_text(s)

