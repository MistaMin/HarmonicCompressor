#include "PluginProcessor.h"
#include <iostream>
int main(){
 for(double sr:{44100.0,48000.0,96000.0}) {
  OptoLevelerAlgorithm opto;RMSVCADynamicsAlgorithm rms;
  for(CompressorAlgorithm* alg:{static_cast<CompressorAlgorithm*>(&opto),static_cast<CompressorAlgorithm*>(&rms)}) {
   alg->prepare({sr,256,2});CompressorParams p;p.thresholdDb=-18;p.ratio=4;alg->updateParameters(p);
   float minimum=1,last=1;
   for(int i=0;i<int(sr*3);++i){float x=i<int(sr)?0.6f*std::sin(float(i)*0.13f):0;
    last=alg->processSidechain(x);if(!std::isfinite(last)||last<=0||last>2)return 2;minimum=std::min(minimum,last);}
   if(minimum>=0.9f||last<minimum)return 3;
   alg->reset();float reset=alg->processSidechain(0);if(!std::isfinite(reset)||reset<0.99f)return 4;
   std::cout<<sr<<" Hz: minimum gain "<<minimum<<", recovered "<<last<<"; reset finite\n";
  }
 }
 std::cout<<"PASS: both new engines, three sample rates, burst/recovery/reset\n";
}
