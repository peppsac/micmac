#include"GpGpu/GpGpu_Interface_Census.h"


dataCorrelMS::dataCorrelMS()
{
    for (int t = 0; t < NBEPIIMAGE; ++t)
    {
        _texImage[t]    = pTexture_ImageEpi(t);
        _texMaskErod[t] = ptexture_Masq_Erod(t);
        GpGpuTools::SetParamterTexture(*_texImage[t]);

        _texMaskErod[t]->addressMode[0]	= cudaAddressModeBorder;
        _texMaskErod[t]->addressMode[1]	= cudaAddressModeBorder;
        _texMaskErod[t]->filterMode     = cudaFilterModePoint; //cudaFilterModePoint cudaFilterModeLinear
        _texMaskErod[t]->normalized     = false;
    }


}

void dataCorrelMS::transfertImage(uint2 sizeImage, float ***dataImage, int id)
{
    _HostImage[id].ReallocIfDim(sizeImage,3);
    for (int tScale = 0; tScale < 3; tScale++)
    {
        float ** source   = dataImage[tScale];
        float *  dest     = _HostImage[id].pLData(tScale);
        memcpy( dest , source[0],  size(sizeImage) * sizeof(float));
    }
}

void dataCorrelMS::transfertMask(uint2 dimMask0,uint2 dimMask1, pixel **mImMasqErod_0, pixel **mImMasqErod_1)
{
    uint2 dimMaskByte0 = make_uint2((dimMask0.x+7)/8,dimMask0.y);
    _HostMaskErod[0].ReallocIfDim(dimMaskByte0,1);

    uint2 dimMaskByte1 = make_uint2((dimMask1.x+7)/8,dimMask1.y);
    _HostMaskErod[1].ReallocIfDim(dimMaskByte1,1);

    memcpy( _HostMaskErod[0].pData() , mImMasqErod_0[0],  size(dimMaskByte0) * sizeof(pixel));
    memcpy( _HostMaskErod[1].pData() , mImMasqErod_1[0],  size(dimMaskByte1) * sizeof(pixel));

//    for (uint y = 0; y < dimMask.y; ++y)
//    {
//        //pixel* yP = mImMasqErod_0[y];

//        for (uint x = 0; x < dimMask.x; ++x)
//        {
//            _HostMaskErod[make_uint3(x,y,0)] = mImMasqErod_0[y][x];
////            _HostMaskErod[make_uint3(x,y,0)] = ((yP[x/8] >> (7-x %8) ) & 1) ? 255 : 0;
//        }
//    }

//    _HostMaskErod.saveImage("Mask_",0);

}

void dataCorrelMS::transfertNappe(int mX0Ter, int mX1Ter, int mY0Ter, int mY1Ter, short **mTabZMin, short **mTabZMax)
{

    uint2 dimNappe = make_uint2(mX1Ter-mX0Ter,mY1Ter-mY0Ter);

    _HostInterval_Z.ReallocIfDim(dimNappe,1);

    for (int anX = mX0Ter ; anX <  mX1Ter ; anX++)
    {
        int X = anX - mX0Ter;
        for (int anY = mY0Ter ; anY < mY1Ter ; anY++)
            _HostInterval_Z[make_uint2(X,anY - mY0Ter)] = make_short2(mTabZMin[anY][anX],mTabZMax[anY][anX]);
    }
}

void dataCorrelMS::syncDeviceData()
{
    for (int t = 0; t < NBEPIIMAGE; ++t)
    {
        _dt_Image[t].syncDevice(_HostImage[t],*_texImage[t]);
        _dt_MaskErod[t].syncDevice(_HostMaskErod[t],*_texMaskErod[t]);
    }

    _DeviceInterval_Z.ReallocIf(_HostInterval_Z.GetDimension());
    _DeviceInterval_Z.CopyHostToDevice(_HostInterval_Z.pData());
}

void dataCorrelMS::dealloc()
{
    for (int t = 0; t < NBEPIIMAGE; ++t)
    {
        _HostImage[t].Dealloc();
        _HostMaskErod[t].Dealloc();
        _dt_MaskErod[t].Dealloc();
        _dt_Image[t].Dealloc();
    }

    _HostInterval_Z.Dealloc();
    _DeviceInterval_Z.Dealloc();

}

void constantParameterCensus::transfertConstantCensus(
        const std::vector<std::vector<Pt2di> > &VV,
        const std::vector<double> &VPds,
        int2 offset0,
        int2 offset1,
        ushort NbByPix,
        ushort nbscale)
{
    aNbScale    = nbscale;
    mNbByPix    = NbByPix;

    for (int s = 0; s < (int)VV.size(); ++s)
    {
        short2 *lw = aVV[s];

        const std::vector<Pt2di> &vv = VV[s];
        size_aVV[s] = vv.size();
        aVPds[s] = VPds[s];
        anOff0 = offset0;
        anOff1 = offset1;

        for (int p = 0; p < (int)vv.size(); ++p)
        {
            Pt2di pt = vv[p];
            lw[p] = make_short2(pt.x,pt.y);
        }
    }
}

void constantParameterCensus::transfertTerrain(Rect zoneTerrain)
{
    _zoneTerrain    = zoneTerrain;
    _dimTerrain     = _zoneTerrain.dimension();
}

void constantParameterCensus::dealloc()
{
    // TODO A Faire avec la liberation de symbole GPU
}

GpGpuInterfaceCensus::GpGpuInterfaceCensus():
    CSimpleJobCpuGpu(true)
{
    freezeCompute();
}

GpGpuInterfaceCensus::~GpGpuInterfaceCensus()
{
    _dataCMS.dealloc();
    _cDataCMS.dealloc();
}

void GpGpuInterfaceCensus::jobMask()
{
    paramCencus2Device(_cDataCMS);   
    _dataCMS.syncDeviceData();
    LaunchKernelCorrelationCensusPreview(_dataCMS,_cDataCMS);
}

void GpGpuInterfaceCensus::transfertImageAndMask(uint2 sI0, uint2 sI1, float ***dataImg0, float ***dataImg1, pixel **mask0, pixel **mask1)
{
    _dataCMS.transfertImage(sI0,dataImg0,0);
    _dataCMS.transfertImage(sI1,dataImg1,1);
    _dataCMS.transfertMask(sI0,sI1,mask0,mask1);
}

void GpGpuInterfaceCensus::transfertParamCensus(
        Rect                                    terrain,
        const std::vector<std::vector<Pt2di> > &aVV,
        const std::vector<double>              &aVPds,
        int2                                    offset0,
        int2                                    offset1,
        short                                 **mTabZMin,
        short                                 **mTabZMax,
        ushort                                  NbByPix,
        ushort                                  nbscale)
{
    _cDataCMS.transfertConstantCensus(aVV,aVPds,offset0,offset1,NbByPix);
    _dataCMS.transfertNappe(terrain.pt0.x, terrain.pt1.x, terrain.pt0.y, terrain.pt1.y, mTabZMin, mTabZMax);
    _cDataCMS.transfertTerrain(terrain);
    //_cDataCMS.transfertTerrain(Rect(mX0Ter,mY0Ter,mY1Ter,mX1Ter));
}

