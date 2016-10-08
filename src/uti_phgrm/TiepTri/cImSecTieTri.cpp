/*Header-MicMac-eLiSe-25/06/2007

    MicMac : Multi Image Correspondances par Methodes Automatiques de Correlation
    eLiSe  : ELements of an Image Software Environnement

    www.micmac.ign.fr

   
    Copyright : Institut Geographique National
    Author : Marc Pierrot Deseilligny
    Contributors : Gregoire Maillet, Didier Boldo.

[1] M. Pierrot-Deseilligny, N. Paparoditis.
    "A multiresolution and optimization-based image matching approach:
    An application to surface reconstruction from SPOT5-HRS stereo imagery."
    In IAPRS vol XXXVI-1/W41 in ISPRS Workshop On Topographic Mapping From Space
    (With Special Emphasis on Small Satellites), Ankara, Turquie, 02-2006.

[2] M. Pierrot-Deseilligny, "MicMac, un lociel de mise en correspondance
    d'images, adapte au contexte geograhique" to appears in 
    Bulletin d'information de l'Institut Geographique National, 2007.

Francais :

   MicMac est un logiciel de mise en correspondance d'image adapte 
   au contexte de recherche en information geographique. Il s'appuie sur
   la bibliotheque de manipulation d'image eLiSe. Il est distibue sous la
   licences Cecill-B.  Voir en bas de fichier et  http://www.cecill.info.


English :

    MicMac is an open source software specialized in image matching
    for research in geographic information. MicMac is built on the
    eLiSe image library. MicMac is governed by the  "Cecill-B licence".
    See below and http://www.cecill.info.

Header-MicMac-eLiSe-25/06/2007*/


#include "TiepTri.h"


/************************************************/
/*                                              */
/*          cImSecTieTri                        */
/*                                              */
/************************************************/

cImSecTieTri::cImSecTieTri(cAppliTieTri & anAppli ,const std::string& aNameIm) :
   cImTieTri   (anAppli,aNameIm),
   mImReech    (1,1),
   mTImReech   (mImReech),
   mImLabelPC  (1,1),
   mTImLabelPC (mImLabelPC),
   mAffMas2Sec (ElAffin2D::Id()),
   mAffSec2Mas (ElAffin2D::Id()),
   mMaster     (anAppli.Master())
{
}


void cImSecTieTri::LoadTri(const cXml_Triangle3DForTieP &  aTri)
{
   cImTieTri::LoadTri(aTri);


   // Reechantillonage des images

   mAffMas2Sec = ElAffin2D::FromTri2Tri
                 (
                      mMaster->mP1Loc,mMaster->mP2Loc,mMaster->mP3Loc,
                      mP1Loc,mP2Loc,mP3Loc
                 );

   mAffSec2Mas = mAffMas2Sec.inv();

   mSzReech = mMaster->mSzIm;

   mImReech.Resize(mSzReech);
   mTImReech =  TIm2D<tElTiepTri,tElTiepTri>(mImReech);

   mImLabelPC.Resize(mSzReech);
   mImLabelPC.raz();
   mTImLabelPC = TIm2D<U_INT1,INT>(mImLabelPC);


   Pt2di aPSec;
   for (aPSec.x=0 ; aPSec.x<mSzReech.x ; aPSec.x++)
   {
       for (aPSec.y=0 ; aPSec.y<mSzReech.y ; aPSec.y++)
       {
           Pt2dr aPMast = mAffMas2Sec(Pt2dr(aPSec));
           double aVal = mTImInit.getr(aPMast,-1);
           mTImReech.oset(aPSec,aVal);
       }
   }

   if (mW)
   {
/*
      ELISE_COPY
      (
          mImReech.all_pts(),
          Max(0,Min(255,Virgule(mImReech.in(),mMaster->mImInit.in(0),mMaster->mImInit.in(0)))),
          mW->orgb()
      );
*/
      mW->clear();
      ELISE_COPY
      (
          mImReech.all_pts(),
          Max(0,Min(255,255-mImReech.in())),
          mW->ogray()
      );
      ELISE_COPY(select(mImReech.all_pts(),mMaster->mMasqTri.in()),Min(255,Max(0,mImReech.in())),mW->ogray());

      // mW->clik_in();
   }

   MakeInterestPoint(0,&mTImLabelPC,mMaster->mTMasqTri,mTImReech);
   //MakeInterestPointFAST(0,&mTImLabelPC,mMaster->mTMasqTri,mTImReech);

}

/*
 glob  -0.0861334 [-9,7] [-2,2]
--loc-- -0.259069 [-8,5] [-1,0]
  glob  0.346995 [4,-8] [2,-1]
--loc-- 0.267576 [3,-8] [1,-1]
  glob  0.840767 [4,7] [2,-2]
--loc-- 0.781191 [3,8] [1,-1]
==================== [333,157] 2

*/

void  cImSecTieTri::DecomposeVecHom(const Pt2dr & aPSH1Ori,const Pt2dr & aPSH2Ori,Pt2dr & aDir,Pt2dr & aDecompos)
{
       Pt2dr aPSH1 = aPSH1Ori  + Pt2dr(mDecal);
       Pt2dr aPSH2 = aPSH2Ori  + Pt2dr(mDecal);

       bool Ok;
       Pt3dr aPTer =  mAppli.CurPlan().Inter(mCam->Capteur2RayTer(aPSH1) , &Ok);


// std::cout << "Veriiff , reproj: " << euclid(mCam->Ter2Capteur(aPTer)-aPSH1) 
// << " Ter "<<  mAppli.CurPlan().Proj(aPTer) - aPTer  << "\n";

       double aProf = mMaster->mCam->ProfondeurDeChamps(aPTer);
       Pt2dr aPM1 = mMaster->mCam->Ter2Capteur(aPTer);
       Pt3dr aPTerMod =  mMaster->mCam->ImEtProf2Terrain(aPM1,aProf*(1+1e-3));
       Pt2dr aPSH1Mod =  mCam->Ter2Capteur(aPTerMod);

       aDir = vunit(aPSH1Mod -aPSH1);
       aDecompos = (aPSH2-aPSH1) / aDir;

       

       std::cout << "GGGGGg  " << aDecompos << "\n";
       // Pt2dr aDirProf =  vunit(aPSH1Mod- aPSH1);
}





void cImSecTieTri::RechHomPtsInteret(const cIntTieTriInterest & aPI,int aNivInter)
{

if (0)
{
   Pt2dr aP1(763.4705,1260.2509);
   Pt2dr aP2(836.3564,1265.2243);

   Pt2dr aDir,aNewDec;
   DecomposeVecHom(aP1-Pt2dr(mDecal),aP2-Pt2dr(mDecal),aDir,aNewDec);
    
   std::cout << "Ggggggggggggggggg\n";
   getchar();
}


    double aD= mAppli.DistRechHom();
    Pt2di aP0 = aPI.mPt;
    eTypeTieTri aLab = aPI.mType;

    const std::vector<Pt2di> &   aVH = mAppli.VoisHom();
                   
    cResulRechCorrel<int> aCRCMax;
    for (int aKH=0 ; aKH<int(aVH.size()) ; aKH++)
    {
        if (mTImLabelPC.get(aP0+aVH[aKH],-1)==aLab)
        {
           Pt2di aPV = aP0+aVH[aKH];
           if (aNivInter>=2)
           {
               mW->draw_circle_loc(Pt2dr(aPV),2.0,ColOfType(aLab));

           }
               // cResulRechCorrel<int> aCRC = TT_RechMaxCorrelBasique(mMaster->mTImInit,aP0,mTImReech,aPV,3,2,aSzRech);

           int aSzRech = 6;
           cResulRechCorrel<int> aCRCLoc = TT_RechMaxCorrelLocale(mMaster->mTImInit,aP0,mTImReech,aPV,3,2,aSzRech);
           if (aCRCLoc.mCorrel > TT_SEUIL_CORREL_1PIXSUR2)
           {
               // aPV = aPV+ aCRCLoc.mPt;
               aCRCLoc = TT_RechMaxCorrelLocale(mMaster->mTImInit,aP0,mTImReech,aCRCLoc.mPt,6,1,aSzRech);
                   
               // aCRCLoc.mPt = aPV+ aCRCLoc.mPt;  // Contient la coordonnee directe dans Im2

               aCRCMax.Merge(aCRCLoc);
           }
        }
    }

    if (aNivInter>=2)
    {
        mW->draw_circle_loc(Pt2dr(aP0),1.0,mW->pdisc()(P8COL::green));
        mW->draw_circle_loc(Pt2dr(aP0),aD,mW->pdisc()(P8COL::yellow));
    }

    if (! aCRCMax.IsInit())
    {
        if (aNivInter>=2)
            std::cout  << "- NO POINT for Correl Int\n";
       return;
    }

   
    if (aNivInter>=2)
    {
          std::cout  << "-- CORREL INT -- " << aCRCMax.mCorrel << " " << aCRCMax.mPt- aP0 << "\n";

          if (1)
          {
              double aCorrelMax= -2;
              Pt2dr  aPMax(0,0);
              double aStep = 0.125;
              for (double aDx= -1.5 ; aDx <=1.5 ; aDx+=aStep)
              {
                 for (double aDy= -1.5 ; aDy <=1.5 ; aDy+=aStep)
                 {
                      Pt2dr aPIm2 = Pt2dr(aCRCMax.mPt) + Pt2dr(aDx,aDy);
                      double aC =  TT_CorrelBilin
                                   (
                                        mMaster->mTImInit,
                                        aP0,
                                        mTImReech,
                                        aPIm2,
                                        6
                                    );
                      if (aC>aCorrelMax)
                      {
                         aCorrelMax = aC;
                         aPMax = aPIm2;
                      }

                      // std::cout << "Correl = " << aPIm2 << " " << aC << "\n";
                   }
              }
              std::cout << "CorrelMaxGrid  = " << aPMax -Pt2dr(aP0) << " " << aCorrelMax << "\n";
          }
    }
    
    cResulRechCorrel<double> aRes =TT_RechMaxCorrelMultiScaleBilin (mMaster->mTImInit,aP0,mTImReech,Pt2dr(aCRCMax.mPt),6);

    Pt2dr aDir,aNewDec;
    DecomposeVecHom(mAffMas2Sec(Pt2dr(aP0)),mAffMas2Sec(aRes.mPt),aDir,aNewDec);

    if (aNivInter >=1)
    {
        Pt2dr aDepl = aRes.mPt - Pt2dr(aP0);

        // mW->draw_seg(Pt2dr(aP0),Pt2dr(aP0) + aDepl * 3  ,mW->pdisc()(P8COL::red));

        mW->draw_seg(Pt2dr(aP0),Pt2dr(aP0) + Pt2dr(aNewDec.y*5,0)  ,mW->pdisc()(P8COL::red));

        mW->draw_circle_loc(Pt2dr(aP0),0.5,mW->pdisc()(P8COL::yellow));
    }



    // std::cout << "MulScale  = " << aRes.mPt -Pt2dr(aP0)  << " " << aRes.mCorrel << "\n\n";
        // std::cout << "==================== " << aP0 << " "  << (int) aLab << "\n";
}

bool  cImSecTieTri::IsMaster() const 
{
    return false;
}




/*Footer-MicMac-eLiSe-25/06/2007

Ce logiciel est un programme informatique servant à la mise en
correspondances d'images pour la reconstruction du relief.

Ce logiciel est régi par la licence CeCILL-B soumise au droit français et
respectant les principes de diffusion des logiciels libres. Vous pouvez
utiliser, modifier et/ou redistribuer ce programme sous les conditions
de la licence CeCILL-B telle que diffusée par le CEA, le CNRS et l'INRIA 
sur le site "http://www.cecill.info".

En contrepartie de l'accessibilité au code source et des droits de copie,
de modification et de redistribution accordés par cette licence, il n'est
offert aux utilisateurs qu'une garantie limitée.  Pour les mêmes raisons,
seule une responsabilité restreinte pèse sur l'auteur du programme,  le
titulaire des droits patrimoniaux et les concédants successifs.

A cet égard  l'attention de l'utilisateur est attirée sur les risques
associés au chargement,  à l'utilisation,  à la modification et/ou au
développement et à la reproduction du logiciel par l'utilisateur étant 
donné sa spécificité de logiciel libre, qui peut le rendre complexe à 
manipuler et qui le réserve donc à des développeurs et des professionnels
avertis possédant  des  connaissances  informatiques approfondies.  Les
utilisateurs sont donc invités à charger  et  tester  l'adéquation  du
logiciel à leurs besoins dans des conditions permettant d'assurer la
sécurité de leurs systèmes et ou de leurs données et, plus généralement, 
à l'utiliser et l'exploiter dans les mêmes conditions de sécurité. 

Le fait que vous puissiez accéder à cet en-tête signifie que vous avez 
pris connaissance de la licence CeCILL-B, et que vous en avez accepté les
termes.
aooter-MicMac-eLiSe-25/06/2007*/
